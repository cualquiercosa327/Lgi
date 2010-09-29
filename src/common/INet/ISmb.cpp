#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "Gdc2.h"
#include "ISmb.h"
#include "GToken.h"

///////////////////////////////////////////////////////////////////
class SmbUtils
{
protected:
	GSocketI *Conn;

public:
	// Read
	ushort ReadUChar()
	{
		LgiAssert(Conn);

		uchar i;
		Conn->Read((char*) &i, sizeof(i), 0);
		return i;
	}

	ushort ReadUShort()
	{
		LgiAssert(Conn);

		ushort i;
		Conn->Read((char*) &i, sizeof(i), 0);
		return i;
	}

	ushort ReadULong()
	{
		LgiAssert(Conn);

		ulong i;
		Conn->Read((char*) &i, sizeof(i), 0);
		return i;
	}

	// Write
	void WriteUChar(uchar i)
	{
		LgiAssert(Conn);

		Conn->Read((char*) &i, sizeof(i), 0);
	}

	void WriteUShort(ushort i)
	{
		LgiAssert(Conn);

		Conn->Read((char*) &i, sizeof(i), 0);
	}

	void WriteULong(ulong i)
	{
		LgiAssert(Conn);

		Conn->Read((char*) &i, sizeof(i), 0);
	}
};

// Smb header structure
class SmbHeader : public SmbUtils
{
public:
	uchar Protocol[4];                // Contains 0xFF,'SMB'
	uchar Command;                    // Command code

	union {
		struct {
			uchar ErrorClass;         // Error class
			uchar Reserved;           // Reserved for future use
			ushort Error;             // Error code
		} DosError;
		ulong Status;                 // 32-bit error code

	} Status;

	uchar Flags;                      // Flags
	ushort Flags2;                    // More flags

	union {
		ushort Pad[6];                // Ensure section is 12 bytes long
		struct {
			ushort Reserved;          // reserved for obsolescent requests
			uchar SecuritySignature[8];   // reserved for MIC
		} Extra;
	};

	ushort Tid;                       // Tree identifier
	ushort Pid;                       // Opaque for client use
	ushort Uid;                       // User id
	ushort Mid;                       // multiplex id
	uchar  WordCount;                 // Count of parameter words
	ushort *ParameterWords;           // The parameter words
	ushort ByteCount;                 // Count of bytes
	uchar  *Buffer;                   // The bytes

	SmbHeader(GSocketI *c)
	{
		ParameterWords = 0;
		Conn = c;

		Protocol[0] = 0xFF;
		Protocol[1] = 'S';
		Protocol[2] = 'M';
		Protocol[3] = 'B';
	}

	bool Read()
	{
		Conn->Read((char*) Protocol, sizeof(Protocol), 0);
		Command = ReadUShort();
		Status.Status = ReadULong();
		Flags = ReadUChar();
		Flags2 = ReadUShort();
		Conn->Read((char*) Pad, sizeof(Pad), 0);
		Tid = ReadUShort();
		Pid = ReadUShort();
		Uid = ReadUShort();
		Mid = ReadUShort();
		
		WordCount = ReadUChar();
		DeleteArray(ParameterWords);
		ParameterWords = new ushort[WordCount];
		if (ParameterWords)
		{
			Conn->Read((char*) ParameterWords, sizeof(ushort)*WordCount, 0);
		}

		ByteCount = ReadUChar();
		DeleteArray(Buffer);
		Buffer = new uchar[ByteCount];
		if (Buffer)
		{
			Conn->Read((char*) Buffer, sizeof(uchar)*ByteCount, 0);
		}

		return true;
	}

	bool Write()
	{
		Conn->Write((char*) Protocol, sizeof(Protocol), 0);
		WriteUShort(Command);
		WriteULong(Status.Status);
		WriteUChar(Flags);
		WriteUShort(Flags2);
		Conn->Write((char*) Pad, sizeof(Pad), 0);
		WriteUShort(Tid);
		WriteUShort(Pid);
		WriteUShort(Uid);
		WriteUShort(Mid);
		
		WriteUChar(WordCount);
		if (ParameterWords)
		{
			Conn->Write((char*) ParameterWords, sizeof(ushort)*WordCount, 0);
		}

		WriteUChar(ByteCount);
		if (Buffer)
		{
			Conn->Write((char*) Buffer, sizeof(uchar)*ByteCount, 0);
		}

		return true;
	}
};

///////////////////////////////////////////////////////////////////
class ILogProxy : public GSocketI
{
	GSocketI *Dest;

public:
	ILogProxy(GSocketI *dest)
	{
		Dest = dest;
	}

	void OnError(int ErrorCode, char *ErrorDescription)
	{
		if (Dest AND ErrorDescription)
		{
			char Str[256];
			sprintf(Str, "[Data] %s", ErrorDescription);
			Dest->OnInformation(Str);
		}
	}

	void OnInformation(char *s)
	{
		if (Dest AND s)
		{
			char Str[256];
			sprintf(Str, "[Data] %s", s);
			Dest->OnInformation(Str);
		}
	}

	void OnDisconnect()
	{
		if (Dest)
		{
			Dest->OnInformation("[Data] Disconnect");
		}
	}
};

///////////////////////////////////////////////////////////////////
ISmb::ISmb()
{
	Socket = 0;
	Buffer[0] = 0;
	ResumeFrom = 0;
}

ISmb::~ISmb()
{
	Close();
}

bool ISmb::Open(GSocketI *S, char *RemoteHost, int Port)
{
	Close();
	Socket = S;
	if (Socket AND
		Socket->Open(RemoteHost, (Port>0) ? Port : 80))
	{
		return true;
	}

	return false;
}

bool ISmb::Close()
{
	if (Socket)
	{
		Socket->Close();
		DeleteObj(Socket);
	}
	return 0;
}

bool ISmb::IsOpen()
{
	return Socket != 0;
}

bool ISmb::GetFile(char *File, GBytePipe &Out)
{
	if (File AND Socket)
	{
		char Str[256];
		char *ContentStr = "Content-Length:";
		char Line[4096] = "", *Cur = Line;
		int ContentLength = 0;
		int DataToGo = 1000000000;

		if (ResumeFrom > 0)
		{
			sprintf(Str, "GET %s HTTP/1.0\r\nRange:Bytes=%i-\r\n\r\n", File, ResumeFrom);
		}
		else
		{
			sprintf(Str, "GET %s HTTP/1.0\r\n\r\n", File);
		}
		Socket->Write(Str, strlen(Str), 0);
		
		// Turn logging off
		Socket->SetParameter(PARAMETER_LOG, false);

		// read headers
		int LinesRead = 0;
		int StatusCode = 0;
		int Length = Socket->Read(Buffer, sizeof(Buffer), 0);
		while (	Length > 0 AND
				DataToGo > 0 AND
				(NOT Meter OR NOT Meter->Cancel()))
		{
			if (NOT ContentLength)
			{
				// search through data for feilds and blank line
				int Used = (int)(Cur-Line); // buffer bytes used
				int Left = sizeof(Line)-Used-1; // bytes left in buffer minus a NUL
				for (int i=0; i<Length AND i<Left; i++)
				{
					*Cur++ = Buffer[i];
					if (Cur > (Line+1) AND
						Cur[-2] == '\r' AND Cur[-1] == '\n')
					{
						// new line baby... reset
						Cur[-2] = 0;
						Cur = Line;

						// check data
						if (LinesRead == 0)
						{
							GToken T(Line, " ");
							if (T.Tokens >= 2)
							{
								StatusCode = atoi(T.Get(1));

								if (ResumeFrom > 0 AND StatusCode != 206)
								{
									// Resume not supported
									ResumeFrom = 0;
									Socket->OnError(StatusCode, "Resume not supported on this server");
									return false;
								}
								
								if (ResumeFrom == 0 AND StatusCode != 200)
								{
									Socket->OnError(StatusCode, Line);
									return false;
								}
							}
						}
						else if (strnicmp(Line, ContentStr, strlen(ContentStr)) == 0)
						{
							// content length
							ContentLength = atoi(Line + strlen(ContentStr) + 1);
						}
						else if (strlen(Line) < 2)
						{
							// end of headers
							i++;
							int DataInThisPacket = Length - i;
							DataToGo = ContentLength - DataInThisPacket;
							Out.Push((uchar*)Buffer+i, DataInThisPacket);

							if (Meter)
							{
								Meter->SetLimits(0, ContentLength-1);
								Meter->Value(DataInThisPacket);
							}
							break;
						}

						LinesRead++;
					}
				}
			}
			else
			{
				// push data onto output
				Out.Push((uchar*)Buffer, Length);
			}
			
			// get MORE data!!! HAHAHA
			Length = Socket->Read(Buffer, sizeof(Buffer), 0);
			Length = min(Length, DataToGo);
			DataToGo -= Length;

			if (Meter)
			{
				Meter->Value(ContentLength - DataToGo);
			}
		}

		Out.Push((uchar*)Buffer, Length);
		if (Meter)
		{
			Meter->Value(0);
		}

		// Turn logging on
		Socket->SetParameter(PARAMETER_LOG, true);

		// Calculate status
		int Data = Out.Sizeof();
		bool Status = Data == ContentLength;
		ResumeFrom = 0;
		return Status;
	}

	return false;
}
