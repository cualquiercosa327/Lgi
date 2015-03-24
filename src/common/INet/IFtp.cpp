#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#include "Gdc2.h"
#include "IFtp.h"
#include "GToken.h"
#include "GString.h"
#include "LgiCommon.h"

///////////////////////////////////////////////////////////////////
enum FtpTransferMode
{
	FT_Unknown,
	FT_Ascii,
	FT_Binary
};

class IFtpPrivate
{
public:
	char OutBuf[256];
	char InBuf[256];
	List<char> In;
	FtpTransferMode CurMode;
	GAutoPtr<GSocket> Listen;	// listen for data
	GAutoPtr<GSocketI> Data;	// get data
	GFile *F;
	char *Charset;
	GAutoString ErrBuf;
	
	GAutoString Host;
	int Port;

	IFtpPrivate()
	{
		Charset = NewStr(DefaultFtpCharset);
		InBuf[0] = 0;
		CurMode = FT_Unknown;
		F = 0;
		Port = 0;
	}

	~IFtpPrivate()
	{
		DeleteArray(Charset);
		DeleteObj(F);
	}
};

///////////////////////////////////////////////////////////////////
int LookupMonth(char *m)
{
	const char *Months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	for (int i=0; i<12; i++)
	{
		if (m && _stricmp(m, Months[i]) == 0)
		{
			return i + 1;
		}
	}
	return 1;
}

///////////////////////////////////////////////////////////////////
IFtpEntry::IFtpEntry()
{
	Attributes = 0;
	Size = 0;
}

IFtpEntry::IFtpEntry(IFtpEntry *Entry)
{
	Attributes = Entry->Attributes;
	Size = Entry->Size;
	Name.Reset(NewStr(Entry->Name));
	Path.Reset(NewStr(Entry->Path));
	Date = Entry->Date;
	User.Reset(NewStr(Entry->User));
	Group.Reset(NewStr(Entry->Group));
}

IFtpEntry::IFtpEntry(char *Entry, const char *Cs)
{
	Attributes = 0;
	Size = 0;
	if (Entry)
	{
		const char *Ws = " \t";
		GToken T(Entry, Ws);
		if (T.Length() > 0)
		{
			char *_Size = 0, *_Perm = 0;
			int _Name = 0;

			if (isdigit(*T[0]))
			{
				// M$ format
				_Size = T[2];
				if (_stricmp(_Size, "<dir>") == 0)
				{
					Attributes |= IFTP_DIR;
				}

				_Name = 3;
				Date.SetDate(T[0]);
				Date.SetTime(T[1]);
			}
			else if (T.Length() > 7)
			{
				// Unix format
				int SizeElement = (isdigit(*T[4])) ? 4 : 3;
				_Size = T[SizeElement];
				_Name = SizeElement + 4;
				_Perm = T[0];
				User.Reset(NewStr(T[SizeElement-2]));
				Group.Reset(NewStr(T[SizeElement-1]));

				char *MonthStr = T[SizeElement+1];
				char *YearOrTime = T[SizeElement+3];
				if (MonthStr && YearOrTime)
				{
					int Day = atoi(T[SizeElement+2]);
					int Month = LookupMonth(MonthStr);
					int Year = 0;

					if (strchr(YearOrTime, ':'))
					{
						// It's time
						Date.SetTime(YearOrTime);

						// Default the year
						time_t Time = time(NULL);
						tm *ptm = localtime(&Time);
						if (ptm)
						{
							Year = 1900 + ptm->tm_year;
						}
					}
					else 
					{
						// It's a year
						Year = atoi(YearOrTime);
					}

					char DateStr[64];
					sprintf_s(DateStr, sizeof(DateStr), "%i/%i/%i", Day, Month, Year);
					Date.SetDate(DateStr);
				}
			}

			if (_Name)
			{
				char *n = Entry;
				for (int i=0; i<_Name; i++)
				{
					for (; *n && !strchr(Ws, *n); n++);
					for (; *n && strchr(Ws, *n); n++);
				}
				Name.Reset((char*) LgiNewConvertCp("utf-8", n, Cs));
				if (Name)
				{
					if (Name[0] == '.')
					{
						Attributes |= IFTP_HIDDEN;
					}
				}
			}

			if (_Perm && strlen(_Perm) == 10)
			{
				if (_Perm[0] == 'd' ||
					_Perm[0] == 'l') Attributes |= IFTP_DIR;
				if (_Perm[1] != '-') Attributes |= IFTP_READ;
				if (_Perm[2] != '-') Attributes |= IFTP_WRITE;
				if (_Perm[3] != '-') Attributes |= IFTP_EXECUTE;

				if (_Perm[4] != '-') Attributes |= IFTP_GRP_READ;
				if (_Perm[5] != '-') Attributes |= IFTP_GRP_WRITE;
				if (_Perm[6] != '-') Attributes |= IFTP_GRP_EXECUTE;

				if (_Perm[7] != '-') Attributes |= IFTP_GLOB_READ;
				if (_Perm[8] != '-') Attributes |= IFTP_GLOB_WRITE;
				if (_Perm[9] != '-') Attributes |= IFTP_GLOB_EXECUTE;

				if (_Perm[0] == 'l')
				{
					// link...
					Attributes |= IFTP_SYM_LINK;
					
					char *Arrow = Name ? strstr(Name, "->") : 0;
					if (Arrow)
					{
						Arrow[-1] = 0;
					}
				}
			}

			if (_Size)
			{
				Size = atoi64(_Size);
			}
		}
	}
}

IFtpEntry::~IFtpEntry()
{
}

///////////////////////////////////////////////////////////////////

// #define VERIFY(i, ret) if (i != ret) { Close(); return 0; }
// #define VERIFY_RANGE(i, range) if (((i)/100) != range) { Close(); return 0; }

#define Verify(i, ret) { int Code = i; if (Code != (ret)) throw Code; }
#define VerifyRange(i, range) { int Code = i; if ((Code/100) != range) throw Code; }

IFtp::IFtp(/* FtpSocketFactory sockFactory, void *factoryParam */)
{
	d = new IFtpPrivate;
	// SockFactory = sockFactory;
	// FactoryParam = factoryParam;
	Authenticated = false;
	Meter = 0;

	Ip[0] = 0;
	Port = 0;
	PassiveMode = false;
	ForceActive = false;
	LongList = true;
	ShowHidden = false;

	RestorePos = 0;
	AbortTransfer = false;
}

IFtp::~IFtp()
{
	DeleteObj(d);
}

const char *IFtp::GetError()
{
	return d->ErrBuf;
}

char *IFtp::ToFtpCs(const char *s)
{
	return (char*) LgiNewConvertCp(GetCharset(), s, "utf-8");
}

char *IFtp::FromFtpCs(const char *s)
{
	return (char*) LgiNewConvertCp("utf-8", s, GetCharset());
}

const char *IFtp::GetCharset()
{
	return d->Charset;
}

void IFtp::SetCharset(const char *cs)
{
	DeleteArray(d->Charset);
	d->Charset = NewStr(cs ? cs : (char*)DefaultFtpCharset);
}

int IFtp::WriteLine(char *Msg)
{
	int Status = 0;

	if (IsOpen())
	{
		char *b = Msg ? Msg : d->OutBuf;
		int l = strlen(b);
		Status = Socket->Write(b, l, 0);
	}

	return Status;	
}

int IFtp::ReadLine(char *Msg, int MsgSize)
{
	int Status = 0;
	while (Socket)
	{
		// look through the input list for result codes
		char *s;
		while ((s = d->In.First()))
		{
			int i = 0;
			if (isdigit(*s))
			{
				char *e = s;
				while (isdigit(*e)) e++;

				if (*e != '-')
				{
					i = atoi(s);

					if (Msg)
					{
						char *Sp = strchr(s, ' ');
						if (Sp)
						{
							strcpy_s(Msg, MsgSize, Sp + 1);
						}
					}
				}
			}

			d->In.Delete(s);
			d->ErrBuf.Reset(s);
			
			if (i)
			{
				return i;
			}
		}

		// Ok, no result code in the input list so
		// Read data off socket
		int Len = strlen(d->InBuf);
		int Read = 0;

		if (IsOpen())
		{
			Read = Socket->Read(d->InBuf+Len, sizeof(d->InBuf)-Len-1, 0);
		}

		if (Read > 0)
		{
			// process input into lines
			char *Eol = 0;
			d->InBuf[Len+Read] = 0;
			do
			{
				Eol = strstr(d->InBuf, "\r\n");
				if (Eol)
				{
					// copy line into input list
					char *New = 0;
					d->In.Insert(New = NewStr(d->InBuf, Eol-d->InBuf));
					Eol += 2;

					// move data after line up to the start of the buffer
					int EolLen = strlen(Eol);
					if (EolLen > 0)
					{
						memmove(d->InBuf, Eol, EolLen+1);
					}
					else
					{
						d->InBuf[0] = 0;
						Eol = 0;
					}
				}
			}
			while (Eol);
		}
		else break; // the read failed....

	} // loop around to check results

	return Status;
}

bool IFtp::IsOpen()
{
	return Socket ? Socket->IsOpen() : false;
}

void IFtp::GetHost(GAutoString *Host, int *Port)
{
	Host->Reset(NewStr(d->Host));
	*Port = d->Port;
}

FtpOpenStatus IFtp::Open(GSocketI *S, char *RemoteHost, int Port, char *User, char *Password)
{
	FtpOpenStatus Status = FO_ConnectFailed;

	try
	{
		Socket.Reset(S);
		S->SetTimeout(15 * 1000);
		Authenticated = false;	

		if (!Port)
			Port = 21;

		d->Host.Reset(NewStr(RemoteHost));
		d->Port = Port;

		if (Socket && Socket->Open(RemoteHost, Port))
		{
			Verify(ReadLine(), 220);

			if (User)
			{
				// User/pass
				sprintf_s(d->OutBuf, sizeof(d->OutBuf), "USER %s\r\n", User);
				if (WriteLine())
				{
					Verify(ReadLine(), 331);

					sprintf_s(d->OutBuf, sizeof(d->OutBuf), "PASS %s\r\n", ValidStr(Password) ? Password : (char*)"");
					if (WriteLine())
					{
						Verify(ReadLine(), 230);
						Authenticated = true;
					}
				}
			}
			else
			{
				// Anonymous
				sprintf_s(d->OutBuf, sizeof(d->OutBuf), "USER anonymous\r\n");
				if (WriteLine())
				{
					int Code = ReadLine();
					if (Code / 100 > 3)
						throw Code;

					sprintf_s(d->OutBuf, sizeof(d->OutBuf), "PASS me@privacy.net\r\n"); // garrenteed to never
															// be allocated to an IP
					if (WriteLine())
					{
						Verify(ReadLine(), 230);
						Authenticated = true;
					}
				}
			}

			if (Authenticated)
			{
				Status = FO_Connected;
			}
			else
			{
				Status = FO_LoginFailed;
			}

			#if 0
			sprintf_s(d->OutBuf, sizeof(d->OutBuf), "FEAT\r\n");
			if (WriteLine())
			{
				int r = ReadLine();
				if (r / 100 == 2)
				{
					// Is utf8 there?
					// FIXME
				}
			}
			#endif
		}
	}
	catch (int Error)
	{
		LgiTrace("%s:%i - Error: %i\n", _FL, Error);
		Status = FO_Error;
	}

	return Status;
}

bool IFtp::Close()
{
	if (d->F)
		d->F->Close();
	
	if (d->Data)
		d->Data->Close();
	
	if (d->Listen)
		d->Listen->Close();
	
	if (Socket)
		Socket->Close();
	
	return true;
}

void IFtp::Noop()
{
	if (IsOpen())
	{
		sprintf_s(d->OutBuf, sizeof(d->OutBuf), "NOOP\r\n");
		if (WriteLine())
		{
			ReadLine();
		}
	}
}

bool IFtp::GetDir(GAutoString &Dir)
{
	bool Status = false;

	try
	{
		if (IsOpen())
		{
			char Temp[256];

			sprintf_s(d->OutBuf, sizeof(d->OutBuf), "PWD\r\n");
			WriteLine();
			Verify(ReadLine(Temp, sizeof(Temp)), 257);

			char *Start = strchr(Temp, '\"');
			char *End = (Start) ? strchr(Start+1, '\"') : 0;
			if (End)
			{
				*End = 0;	
				Status = Dir.Reset(FromFtpCs(Start+1));
			}			
		}
	}
	catch (int Error)
	{
		printf("%s:%i - error: %i\n", _FL, Error);
		if (IsOpen())
		{
			LgiAssert(0);
		}
	}

	return Status;
}

bool IFtp::SetDir(const char *Dir)
{
	bool Status = false;

	try
	{
		if (Dir && IsOpen())
		{
			char *f = ToFtpCs(Dir);
			if (f)
			{
				sprintf_s(d->OutBuf, sizeof(d->OutBuf), "CWD %s\r\n", f);
				WriteLine();
				Verify(ReadLine(), 250);

				Status = true;
				DeleteArray(f);
			}
		}
	}
	catch (int Error)
	{
		printf("%s:%i - error: %i\n", _FL, Error);
	}

	return Status;
}

bool IFtp::CreateDir(const char *Dir)
{
	bool Status = false;

	try
	{
		if (IsOpen() && Dir)
		{
			char *f = ToFtpCs(Dir);
			if (f)
			{
				sprintf_s(d->OutBuf, sizeof(d->OutBuf), "MKD %s\r\n", f);
				WriteLine();
				VerifyRange(ReadLine(), 2);

				Status = true;
				DeleteArray(f);
			}
		}
	}
	catch (int Error)
	{
		printf("%s:%i - error: %i\n", _FL, Error);
		if (IsOpen())
		{
			LgiAssert(0);
		}
	}

	return Status;
}

bool IFtp::DeleteDir(const char *Dir)
{
	bool Status = false;

	try
	{
		if (IsOpen() && Dir)
		{
			char *f = ToFtpCs(Dir);
			if (f)
			{
				sprintf_s(d->OutBuf, sizeof(d->OutBuf), "RMD %s\r\n", f);
				WriteLine();
				VerifyRange(ReadLine(), 2);

				Status = true;
				DeleteArray(f);
			}
		}
	}
	catch (int Error)
	{
		printf("%s:%i - error: %i\n", _FL, Error);
		if (IsOpen())
		{
			LgiAssert(0);
		}
	}

	return Status;
}

bool IFtp::UpDir()
{
	bool Status = false;

	try
	{
		if (IsOpen())
		{
			sprintf_s(d->OutBuf, sizeof(d->OutBuf), "CDUP\r\n");
			WriteLine();
			VerifyRange(ReadLine(0), 2);

			Status = true;
		}
	}
	catch (int Error)
	{
		printf("%s:%i - error: %i\n", _FL, Error);
		if (IsOpen())
		{
			LgiAssert(0);
		}
	}

	return Status;
}

// #include "LgiCommon.h"
// #include <malloc.h>
#ifdef _INC_MALLOC
#define IsHeapOk() (_heapchk() != _HEAPOK)
#else
#define IsHeapOk() (-1)
#endif

bool IFtp::ListDir(List<IFtpEntry> *Dir)
{
	bool Status = false;

	try
	{
		if (Dir &&
			IsOpen() &&
			SetupData(true))
		{
			GMemQueue Buf;

			// List command
			strcpy_s(d->OutBuf, sizeof(d->OutBuf), "LIST");
			if (LongList || ShowHidden) strcat(d->OutBuf, " -");
			if (LongList) strcat(d->OutBuf, "l");
			if (ShowHidden) strcat(d->OutBuf, "a");
			strcat(d->OutBuf, "\r\n");
			WriteLine();
				
			// Accept the data connection
			if (ConnectData())
			{
				VerifyRange(ReadLine(), 1);

				if (Meter)
				{
					Meter->SetLimits(0, 0);
					Meter->Value(0);
				}

				// Read the data
				uchar Buffer[1024];
				int Len;
				
				while ((Len = d->Data->Read((char*) Buffer, sizeof(Buffer), 0)) > 0)
				{
					if (Meter)
					{
						Meter->Value(Buf.GetSize());
					}

					Buf.Write(Buffer, Len);
				}

				if (Meter)
				{
					Meter->Value(0);
				}
				
				Status = true;
			}
			
			d->Data.Reset();
			d->Listen.Reset();
			
			// Parse the results
			int Len = (int)Buf.GetSize();
			if (Status && Len)
			{
				char *Text = new char[Len+1];
				if (Text && Buf.Read((uchar*)Text, Len))
				{
					Text[Len] = 0;

					#ifdef LOG_LISTINGS
					GFile F;
					if (F.Open("c:\\temp\\ftplog.txt", O_WRITE))
					{
						F.Write(Text, Len);
						F.Close();
					}
					#endif

					GToken Lines(Text, "\r\n");
					for (unsigned i=0; i<Lines.Length(); i++)
					{
						IFtpEntry *e = new IFtpEntry(Lines[i], GetCharset());
						if (e && e->Name)
						{
							if (strcmp(e->Name, ".") != 0 &&
								strcmp(e->Name, "..") != 0)
							{
								Dir->Insert(e);
								e = 0;
							}
						}
						
						DeleteObj(e);
					}
					
					DeleteArray(Text);
					VerifyRange(ReadLine(), 2);
					Status = true;
				}
			}
			else
			{
				VerifyRange(ReadLine(), 2);
				Status = true;
			}
		}
		else printf("%s:%i - SetupData failed.\n", _FL);
	}
	catch (int Error)
	{
		printf("%s:%i - error: %i\n", _FL, Error);
		d->Data.Reset();
		d->Listen.Reset();
	}

	return Status;
}

bool IFtp::RenameFile(const char *From, const char *To)
{
	bool Status = false;

	try
	{
		if (IsOpen() && From && To)
		{
			char *f = ToFtpCs(From);
			char *t = ToFtpCs(To);
			if (f && t)
			{
				// From
				sprintf_s(d->OutBuf, sizeof(d->OutBuf), "RNFR %s\r\n", f);
				WriteLine();
				VerifyRange(ReadLine(), 3);

				// To
				sprintf_s(d->OutBuf, sizeof(d->OutBuf), "RNTO %s\r\n", t);
				WriteLine();
				VerifyRange(ReadLine(), 2);

				Status = true;
			}
			DeleteArray(f);
			DeleteArray(t);
		}
	}
	catch (int Error)
	{
		printf("%s:%i - error: %i\n", _FL, Error);
		if (IsOpen())
		{
			LgiAssert(0);
		}
	}

	return Status;
}

bool IFtp::DeleteFile(const char *Remote)
{
	bool Status = false;

	try
	{
		if (IsOpen() && Remote)
		{
			char *f = ToFtpCs(Remote);
			if (f)
			{
				sprintf_s(d->OutBuf, sizeof(d->OutBuf), "DELE %s\r\n", f);
				WriteLine();
				VerifyRange(ReadLine(), 2);

				Status = true;
				DeleteArray(f);
			}
		}
	}
	catch (int Error)
	{
		printf("%s:%i - error: %i\n", _FL, Error);
		if (IsOpen())
		{
			LgiAssert(0);
		}
	}

	return Status;
}

bool IFtp::DownloadFile(const char *Local, IFtpEntry *Remote, bool Binary)
{
	if (Remote && Remote->Name)
	{
		return TransferFile(Local, Remote->Name, Remote->Size, false, Binary);
	}
	return false;
}

bool IFtp::UploadFile(const char *Local, const char *Remote, bool Binary)
{
	return TransferFile(Local, Remote, LgiFileSize(Local), true, Binary);
}

bool IFtp::ResumeAt(int64 Pos)
{
	sprintf_s(d->OutBuf, sizeof(d->OutBuf), "REST " LGI_PrintfInt64 "\r\n", Pos);

	WriteLine();
	int FtpStatus = ReadLine();
	bool Status = (FtpStatus/100) < 4;
	RestorePos = (Status) ? Pos : 0;
	return Status;
}

bool IFtp::TransferFile(const char *Local, const char *Remote, int64 Size, bool Upload, bool Binary)
{
	bool Status = false;
	bool Aborted = false;

	d->F = new GFile;
	
	try
	{
		if (d->F &&
			Socket &&
			Local &&
			Remote)
		{
			if (d->F->Open(Local, (Upload)?O_READ:O_WRITE))
			{
				if (SetupData(Binary))
				{
					// Data command
					char *f = ToFtpCs(Remote);
					sprintf_s(d->OutBuf, sizeof(d->OutBuf), "%s %s\r\n", (Upload)?"STOR":"RETR", f ? f : Remote);
					WriteLine();
					DeleteArray(f);

					// Build data connection
					if (ConnectData())
					{
						VerifyRange(ReadLine(), 1);

						// do the transfer
						int TempLen = 64 << 10;
						uchar *Temp = new uchar[TempLen];
						if (Temp)
						{
							int64 Processed = 0;
							int64 Len = 0;

							if (Meter)
							{
								Meter->SetLimits(0, Size);
							}

							if (!Upload)
							{
								Size -= RestorePos;
							}
							AbortTransfer = false;

							bool Error = false;
							while (	Socket &&
									d->Data &&
									(Size < 0 || Processed < Size) &&
									!Error &&
									!AbortTransfer &&
									(!Meter || !Meter->Cancel()))
							{
								if (Meter)
								{
									Meter->Value(0);
								}

								if (Upload)
								{
									// upload loop
									do
									{
										Len = d->F->Read(Temp, TempLen);
										if (Len > 0)
										{
											int WriteLen = 0;
											
											if (d->Data)
											{
												WriteLen = d->Data->Write((char*) Temp, Len, 0);
											}

											if (WriteLen == Len)
											{
												Processed += Len;

												if (Meter)
												{
													Meter->Value(Meter->Value() + Len);
												}
											}
											else
											{
												printf("%s:%i - Data->Write failed, %i of "LGI_PrintfInt64" bytes written.\n",
													_FL, WriteLen, Len);
												Error = true;
											}
										}

										if (Meter && Meter->Cancel())
										{
											printf("%s:%i - Upload canceled.\n", _FL);
											break;
										}
									}
									while (	Len > 0 && !AbortTransfer);
								}
								else
								{
									// restore support
									d->F->Seek(RestorePos, SEEK_SET);
									if (Meter && RestorePos > 0)
									{
										Meter->SetParameter(10, RestorePos);
									}
									RestorePos = 0;

									// download loop
									while (d->Data)
									{
										if (d->Data)
										{
											Len = d->Data->Read((char*) Temp, TempLen, 0);
										}

										if (Len <= 0 || AbortTransfer)
										{
											break;
										}

										d->F->Write(Temp, Len);
										Processed += Len;
										if (Meter)
										{
											Meter->Value(Meter->Value() + Len);

											if (Meter->Cancel())
												break;
										}
									}

									if (Len <= 0)
										Error = true;
								}
							}
							
							if (AbortTransfer || (Meter && Meter->Cancel()))
							{
								// send abort command
								sprintf_s(d->OutBuf, sizeof(d->OutBuf), "ABOR\r\n");
								WriteLine();
								if ((ReadLine()/100) == 4) // temp msg
								{
									// response... we don't care just read it
									ReadLine();
								}

								Aborted = true;
							}

							if (d->Data)
							{
								d->Data->Close();
							}

							if (Size < 0)
							{
								Status = true;
							}
							else
							{
								Status = Size == Processed;
							}

							if (Meter)
							{
								Meter->Value(0);
							}

							DeleteArray(Temp);
						}

						// read eof response
						if (!Aborted)
						{
							VerifyRange(ReadLine(), 2);
						}
					}

					d->Data.Reset();
					d->Listen.Reset();
				}
				else
				{
					// data setup failed
				}
			}
			else
			{
				// couldn't open file...
				char Str[256];
				sprintf_s(Str, sizeof(Str), "Error: Couldn't open '%s' for %s", Local, (Upload)?(char*)"reading":(char*)"writing");
				Socket->OnInformation(Str);
			}
		}
	}
	catch (int Error)
	{
		printf("%s:%i - error: %i\n", _FL, Error);
		if (IsOpen())
		{
			LgiAssert(0);
		}
	}

	DeleteObj(d->F);

	return Status;
}

bool IFtp::SetupData(bool Binary)
{
	bool Status = false;
	try
	{
		d->Data.Reset();

		if (IsOpen())
		{
			if (Socket)
			{
				GStreamI *nstream = Socket->Clone();
				GSocket *nsock = dynamic_cast<GSocket*>(nstream);
				LgiAssert(nsock != NULL);
				d->Data.Reset(nsock);
			}
			else
			{
				d->Data.Reset(new GSocket());
			}
		}

		if (!d->Data)
		{
			LgiAssert(!"No data socket");
			LgiTrace("%s:%i - No data socket, Socket=%p.\n", _FL, Socket.Get());
		}
		else
		{
			PassiveMode = false;

			if ( (Binary && d->CurMode != FT_Binary) ||
				(!Binary && d->CurMode != FT_Ascii))
			{
				// Type command
				sprintf_s(d->OutBuf, sizeof(d->OutBuf), "TYPE %c\r\n", Binary ? 'I' : 'A');
				if (WriteLine())
				{
					VerifyRange(ReadLine(), 2);
					d->CurMode = Binary ? FT_Binary : FT_Ascii;
				}
			}

			if (!ForceActive)
			{
				// Try the PASV command first... it's better for
				// getting out of a firewall to the server
				sprintf_s(d->OutBuf, sizeof(d->OutBuf), "PASV\r\n");
				WriteLine();

				char Temp[256];
				if (ReadLine(Temp, sizeof(Temp))/100 == 2)
				{
					// Ok we have PASV mode
					// grab the IP and Port
					char *Start = strchr(Temp, '(');
					if (Start)
					{
						Start++;
						char *End = strchr(Start, ')');
						if (End)
						{
							*End = 0;
							GToken T(Start, ",");
							if (T.Length() == 6)
							{
								sprintf_s(Ip, sizeof(Ip), "%s.%s.%s.%s", T[0], T[1], T[2], T[3]);
								Port = (atoi(T[4])<<8) | atoi(T[5]);
								PassiveMode = true;
								return true;
							}
						}
					}
				}
				else
				{
					printf("\tread failed.\n");
					return false;
				}
			}

			// Ok, no PASV so we use the PORT command

			// Port command
			// static int p = (4<<8) | 27;

			bool ListenStatus = false;
			d->Listen.Reset(new GSocket);			// listen
			if (d->Listen)
			{
				if (d->Listen->Listen())
				{
					Socket->GetLocalIp(Ip);				// get current IP
					Port = d->Listen->GetLocalPort();		// get current port

					ListenStatus = true;
				}
			}

			if (ListenStatus)
			{
				// convert IP to PORT's comma delimited format
				for (char *p = Ip; *p; p++)
				{
					if (*p == '.')
					{
						*p = ',';
					}
				}

				sprintf_s(d->OutBuf, sizeof(d->OutBuf), "PORT %s,%i,%i\r\n", Ip, Port>>8, Port&0xFF);
				WriteLine();
				VerifyRange(ReadLine(), 2);
				Status = true;
			}
		}
	}
	catch (int Error)
	{
		printf("%s:%i - error: %i\n", _FL, Error);
		if (IsOpen())
		{
			LgiAssert(0);
		}
	}

	return Status;
}

bool IFtp::ConnectData()
{
	if (PassiveMode)
	{
		return d->Data->Open(Ip, Port) != 0;
	}
	else
	{
		d->Data.Reset(Socket ? dynamic_cast<GSocket*>(Socket->Clone()) : new GSocket());
		if (d->Data)
			return d->Listen->Accept(d->Data);
	}

	return false;
}

bool IFtp::SetPerms(const char *File, int Perms)
{
	bool Status = false;

	try
	{
		if (IsOpen() && File)
		{
			char *f = ToFtpCs(File);
			if (f)
			{
				sprintf_s(d->OutBuf, sizeof(d->OutBuf), "SITE CHMOD %3.3X %s\r\n", Perms, File);
				WriteLine();
				VerifyRange(ReadLine(), 2);

				Status = true;
				DeleteArray(f);
			}
		}
	}
	catch (int Error)
	{
		printf("%s:%i - error: %i\n", _FL, Error);
		if (IsOpen())
		{
			LgiAssert(0);
		}
	}

	return Status;
}
