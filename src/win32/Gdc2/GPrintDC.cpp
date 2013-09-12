#include "Lgi.h"
#include "GdiLeak.h"

class GPrintDCPrivate
{
public:
	bool PageOpen;
	bool DocOpen;

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

GPrintDC::GPrintDC(void *Handle, const char *PrintJobName)
{
	d = new GPrintDCPrivate;
	hDC = (HDC) Handle;

	SetAbortProc(hDC, LgiAbortProc);
	
	// Start document
	if (hDC)
	{
		DOCINFO Info;

		ZeroObj(Info);
		Info.cbSize = sizeof(DOCINFO); 
		Info.lpszDocName = PrintJobName ? PrintJobName : "Lgi Print Job"; 

		d->DocOpen = StartDoc(hDC, &Info) > 0;
	}
}

GPrintDC::~GPrintDC()
{
	EndPage();

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

