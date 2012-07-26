#include "Lgi.h"
#include "Base64.h"

////////////////////////////////////////////////////////////////////
class GPrinterPrivate
{
public:
	PRINTDLG Info;
	int Pages;

	GPrinterPrivate()
	{
		Pages = 3;
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

void GPrinter::SetPages(int p)
{
	d->Pages = p;
}

int GPrinter::GetPages()
{
	return d->Pages;
}

GPrintDC *GPrinter::StartDC(const char *PrintJobName, GView *Parent)
{
	if
	(
		Parent
		AND
		(
			!d->Info.hDevMode
			||
			!d->Info.hDevNames
		)
	)
	{
		if (!Browse(Parent))
		{
			return 0;
		}
	}

	d->Info.hwndOwner = (Parent) ? Parent->Handle() : 0;
	d->Info.Flags = PD_RETURNDC;
	d->Info.hDC = 0;
	if (d->Pages > 1)
	{
		d->Info.nMinPage = 1;
		d->Info.nMaxPage = d->Pages;
	}

	if ( // (d->Info.hDevMode AND d->Info.hDevNames) ||
		PrintDlg(&d->Info))
	{
		/*
		if (NOT d->Info.hDC)
		{
			GMem DevMode(d->Info.hDevMode);
			GMem DevNames(d->Info.hDevNames);
			DEVNAMES *Names = (DEVNAMES*) DevNames.Lock();
			if (Names)
			{
				d->Info.hDC = ::CreateDC(	NULL,
											((char*) Names) + Names->wDeviceOffset,
											NULL,
											(DEVMODE*) DevMode.Lock());
			}

			DevMode.Detach();
			DevNames.Detach();
		}
		*/
	}
	else return 0;

	if (!d->Info.hDC)
	{
		LgiMsg(	Parent,
				"Couldn't initialize printer output.\n"
				"No printer defined?",
				"Print");
		return 0;
	}

	return new GPrintDC(d->Info.hDC, PrintJobName);
}

bool GPrinter::Browse(GView *Parent)
{
	d->Info.hwndOwner = (Parent) ? Parent->Handle() : 0;
	d->Info.Flags = PD_PRINTSETUP | PD_PAGENUMS;
	if (d->Pages > 1)
	{
		d->Info.nMinPage = 1;
		d->Info.nMaxPage = d->Pages;
	}

	return PrintDlg(&d->Info);
}

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
		GBytePipe Temp;
		int32 m;

		// Dlg
		m = MAGIC_PRINTDLG;
		Temp.Push(m);
		m = sizeof(d->Info);
		Temp.Push(m);
		Temp.Write((uchar*)&d->Info, m);

		// Mode
		if (Mode)
		{
			m = MAGIC_DEVMODE;
			Temp.Push(m);
			m = DevMode.GetSize();
			Temp.Push(m);
			Temp.Write((uchar*) Mode, m);
		}

		// Names
		if (Names)
		{
			m = MAGIC_DEVNAMES;
			Temp.Push(m);
			m = DevNames.GetSize();
			Temp.Push(m);
			Temp.Write((uchar*) Names, m);
		}

		DevMode.Detach();
		DevNames.Detach();

		// Convert to base64
		int BinLen = Temp.GetSize();
		uchar *Bin = new uchar[BinLen];
		if (Bin AND Temp.Read(Bin, BinLen))
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
			GBytePipe Temp;

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

					if (Temp.Pop(Type) AND
						Temp.Pop(Size))
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

