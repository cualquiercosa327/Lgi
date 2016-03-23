#include "Lgi.h"
#include "GdiLeak.h"

class GPrintDCPrivate
{
public:
	int x, y;
	GdcPt2 Dpi;
	bool PageOpen;
	bool DocOpen;
	CGContextRef Ctx;
	GString PrintJobName;
	GString PrinterName;

	GPrintDCPrivate()
	{
		PageOpen = false;
		DocOpen = false;
		x = y = 0;
		Ctx = NULL;
	}
};

GPrintDC::GPrintDC(void *Handle, const char *PrintJobName, const char *PrinterName) :
	GScreenDC((GPrintDcParams*)Handle)
{
	d = new GPrintDCPrivate;
	d->PrintJobName = PrintJobName;
	d->PrinterName = PrinterName;
	
	GPrintDcParams *Params = (GPrintDcParams*)Handle;
	if (Params)
	{
		d->Dpi.x = Params->Dpi.hRes;
		d->Dpi.y = Params->Dpi.vRes;
		
		d->x = (Params->Page.right - Params->Page.left) * d->Dpi.x;
		d->y = (Params->Page.bottom - Params->Page.top) * d->Dpi.y;

		d->Ctx = Params->Ctx;
	}
}

GPrintDC::~GPrintDC()
{
	DeleteObj(d);
}

int GPrintDC::X()
{
	return d->x;
}

int GPrintDC::Y()
{
	return d->y;
}

int GPrintDC::GetBits()
{
	return 24;
}

int GPrintDC::DpiX()
{
	return d->Dpi.x;
}

int GPrintDC::DpiY()
{
	return d->Dpi.y;
}
