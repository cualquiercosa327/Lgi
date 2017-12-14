#include "Lgi.h"
#include "GdiLeak.h"

class GPrintDCPrivate
{
public:
	bool PageOpen;
	bool DocOpen;
	GString OutputFileName;

	GPrintDCPrivate()
	{
		PageOpen = false;
		DocOpen = false;
	}
};

BOOL CALLBACK LgiAbortProc(HDC hdc, int iError)
{
	LgiYield();
	return true;
}

GPrintDC::GPrintDC(void *Handle, const char *PrintJobName, const char *PrinterName)
{
	d = new GPrintDCPrivate;
	hDC = (HDC) Handle;

	SetAbortProc(hDC, LgiAbortProc);
	
	// Start document
	if (hDC)
	{
		DOCINFO Info;
		GAutoWString OutName;
		GAutoWString DocName(Utf8ToWide(PrintJobName ? PrintJobName : "Lgi Print Job"));

		ZeroObj(Info);
		Info.cbSize = sizeof(DOCINFO); 
		Info.lpszDocName = DocName;
		
		if (PrinterName &&
			stristr(PrinterName, "XPS"))
		{
			GFile::Path p(LSP_USER_DOCUMENTS);
			GString FileName;
			FileName.Printf("%s.xps", PrintJobName);
			p += FileName;
			if (FileExists(p.GetFull()))
			{
				for (unsigned i=1; i<1000; i++)
				{
					p.Parent();
					FileName.Printf("%s%i.xps", PrintJobName, i);
					p += FileName;
					if (!FileExists(p.GetFull()))
						break;
				}
			}
			
			d->OutputFileName = p.GetFull();
			OutName.Reset(Utf8ToWide(d->OutputFileName));
			Info.lpszOutput = OutName;
		}

		d->DocOpen = StartDoc(hDC, &Info) > 0;
		if (d->DocOpen)
		{
			SetSize(X(), Y());
		}
		else
		{
			DeleteDC(hDC);
			hDC = 0;
		}
	}
}

GPrintDC::~GPrintDC()
{
	// EndPage();

    if (hDC)
    {
		if (d->DocOpen)
		{
			EndDoc(hDC);
		}

	    DeleteDC(hDC);
		hDC = 0;
    }
}

const char *GPrintDC::GetOutputFileName()
{
	return d->OutputFileName;
}

int GPrintDC::X()
{
	return GetDeviceCaps(hDC, HORZRES);
}

int GPrintDC::Y()
{
	return GetDeviceCaps(hDC, VERTRES);
}

int GPrintDC::GetBits()
{
	return GetDeviceCaps(hDC, BITSPIXEL);
}

int GPrintDC::DpiX()
{
	return GetDeviceCaps(hDC, LOGPIXELSX); 
}

int GPrintDC::DpiY()
{
	return GetDeviceCaps(hDC, LOGPIXELSY);
}

/*
bool GPrintDC::StartPage()
{
	bool Status = false;
	if (hDC && d->DocOpen)
	{
		EndPage();
		d->PageOpen = Status = ::StartPage(hDC) > 0;
	}

	return Status;
}

void GPrintDC::EndPage()
{
	if (hDC && d->PageOpen)
	{
		::EndPage(hDC);
		d->PageOpen = false;
	}
}
*/

