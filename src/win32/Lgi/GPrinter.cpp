#include "Lgi.h"
#include "Base64.h"
#include "GPrinter.h"

////////////////////////////////////////////////////////////////////
class GPrinterPrivate
{
public:
	PRINTDLG Info;
	GString Err;
	bool NeedsDC;

	GPrinterPrivate()
	{
		NeedsDC = false;
		ZeroObj(Info);
		Info.lStructSize = sizeof(Info);
	}
};

//////////////////////////////////////////////////////////////////////
GPrinter::GPrinter()
{
	d = new GPrinterPrivate;
}

GPrinter::~GPrinter()
{
	DeleteObj(d);
}

GString GPrinter::GetErrorMsg()
{
	return d->Err;
}

bool GPrinter::Print(GPrintEvents *Events, const char *PrintJobName, int Pages, GView *Parent)
{
	if (!Events)
		return false;
		
	if
	(
		Parent
		&&
		(
			!d->Info.hDevMode
			||
			!d->Info.hDevNames
		)
	)
	{
		d->NeedsDC = true;
		bool r = Browse(Parent);
		d->NeedsDC = false;
		if (!r)
			return false;
	}
	else
	{
		d->Info.hwndOwner = (Parent) ? Parent->Handle() : 0;
		d->Info.Flags = PD_RETURNDC;
		d->Info.hDC = 0;
		/*
		if (Pages >= 0)
		{
			d->Info.nMinPage = 1;
			d->Info.nMaxPage = Pages;
		}
		*/
		if (!PrintDlg(&d->Info))
		{
			return false;
		}
	}

	if (!d->Info.hDC)
	{
		return false;
	}

	GPrintDC dc(d->Info.hDC, PrintJobName);
	if (!dc.Handle())
	{
		d->Err.Printf("%s:%i - StartDoc failed.\n", _FL);
		return false;
	}	
	
	int JobPages = Events->OnBeginPrint(&dc);
	bool Status = false;

	DOCINFO Info;

	ZeroObj(Info);
	Info.cbSize = sizeof(DOCINFO); 
	Info.lpszDocName = PrintJobName ? PrintJobName : "Lgi Print Job"; 

	if (Pages > 0)
		JobPages = min(JobPages, Pages);
		
	for (int i=0; i<JobPages; i++)
	{
		if (StartPage(dc.Handle()) > 0)
		{
			Status |= Events->OnPrintPage(&dc, i);
			EndPage(dc.Handle());
		}
		else
		{
			d->Err.Printf("%s:%i - StartPage failed.", _FL);
			Status = false;
			break;
		}
	}
	
	return Status;
}

bool GPrinter::Browse(GView *Parent)
{
	d->Info.hwndOwner = (Parent) ? Parent->Handle() : 0;
	d->Info.Flags = PD_PRINTSETUP | PD_PAGENUMS;
	if (d->NeedsDC)
		d->Info.Flags |= PD_RETURNDC;
	/*
	if (d->Pages > 1)
	{
		d->Info.nMinPage = 1;
		d->Info.nMaxPage = d->Pages;
	}
	*/

	return PrintDlg(&d->Info);
}

/*
bool GPrinter::GetPageRange(GArray<int> &p)
{
	if (d->Info.Flags & PD_PAGENUMS)
	{
		p[0] = d->Info.nFromPage;
		p[1] = d->Info.nToPage;

		return true;
	}

	return false;
}
*/

#define MAGIC_PRINTDLG					0xAAFF0100
#define MAGIC_DEVMODE					0xAAFF0101
#define MAGIC_DEVNAMES					0xAAFF0102

bool GPrinter::Serialize(char *&Str, bool Write)
{
	int Size = sizeof(d->Info);
	if (Write)
	{
		GMem DevMode(d->Info.hDevMode);
		GMem DevNames(d->Info.hDevNames);
		DEVMODE *Mode = (DEVMODE*) DevMode.Lock();
		DEVNAMES *Names = (DEVNAMES*) DevNames.Lock();
		GMemQueue Temp;
		int32 m;

		// Dlg
		m = MAGIC_PRINTDLG;
		Temp.Write(&m, sizeof(m));
		m = sizeof(d->Info);
		Temp.Write(&m, sizeof(m));
		Temp.Write((uchar*)&d->Info, m);

		// Mode
		if (Mode)
		{
			m = MAGIC_DEVMODE;
			Temp.Write(&m, sizeof(m));
			m = DevMode.GetSize();
			Temp.Write(&m, sizeof(m));
			Temp.Write((uchar*) Mode, m);
		}

		// Names
		if (Names)
		{
			m = MAGIC_DEVNAMES;
			Temp.Write(&m, sizeof(m));
			m = DevNames.GetSize();
			Temp.Write(&m, sizeof(m));
			Temp.Write((uchar*) Names, m);
		}

		DevMode.Detach();
		DevNames.Detach();

		// Convert to base64
		int BinLen = Temp.GetSize();
		uchar *Bin = new uchar[BinLen];
		if (Bin && Temp.Read(Bin, BinLen))
		{
			int Base64Len = BufferLen_BinTo64(BinLen);
			char *Base64 = new char[Base64Len+1];
			if (Base64)
			{
				memset(Base64, 0, Base64Len+1);
				ConvertBinaryToBase64(Base64, Base64Len, Bin, BinLen);
				
				DeleteArray(Str);
				Str = Base64;
			}
			return true;
		}
	}
	else
	{
		bool Status = false;

		ZeroObj(d->Info);
		d->Info.lStructSize = Size;
		if (Str)
		{
			// Convert to binary
			GMemQueue Temp;

			int Base64Len = strlen(Str);
			int BinaryLen = BufferLen_64ToBin(Base64Len);
			uchar *Binary = new uchar[BinaryLen];
			if (Binary)
			{
				int Len = ConvertBase64ToBinary(Binary, BinaryLen, Str, Base64Len);
				Temp.Write(Binary, Len);

				bool Done = false;
				do
				{
					int Type = 0;
					int Size = 0;

					if (Temp.Read(&Type, sizeof(Type)) &&
						Temp.Read(&Size, sizeof(Size)))
					{
						switch (Type)
						{
							case MAGIC_PRINTDLG:
							{
								if (Size == sizeof(d->Info))
								{
									Temp.Read((uchar*) &d->Info, Size);
									d->Info.hDevMode = 0;
									d->Info.hDevNames = 0;
									Status = true;
								}
								else
								{
									// Grrrr: Error
									Done = true;
								}
								break;
							}
							case MAGIC_DEVMODE:
							{
								GMem DevMode(Size);
								DEVMODE *Mode = (DEVMODE*) DevMode.Lock();
								if (Mode)
								{
									Temp.Read((uchar*)Mode, Size);
									d->Info.hDevMode = DevMode.Handle();
									DevMode.Detach();
								}
								break;
							}
							case MAGIC_DEVNAMES:
							{
								GMem DevNames(Size);
								DEVNAMES *Names = (DEVNAMES*) DevNames.Lock();
								if (Names)
								{
									Temp.Read((uchar*)Names, Size);
									d->Info.hDevNames = DevNames.Handle();
									DevNames.Detach();
								}
								break;
							}
							default:
							{
								// Eof
								Done = true;
								break;
							}
						}
					}
					else
					{
						Done = true;
					}
				}
				while (!Done);

				DeleteArray(Binary);
			}
		}

		return Status;
	}

	return false;
}