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
		#if defined COCOA
		#else
		int x = Params->Page.right - Params->Page.left;
		int y = Params->Page.bottom - Params->Page.top;
		
		d->Dpi.x = Params->Dpi.hRes;
		d->Dpi.y = Params->Dpi.vRes;

		d->Ctx = Params->Ctx;

		#if 0
		d->x = x * d->Dpi.x;
		d->y = y * d->Dpi.y;
		#else
		d->x = x;
		d->y = y;
		#endif

		// Invert the co-ordinate space so 0,0 is the top left corner.
		CGContextTranslateCTM(d->Ctx, 0, y);
		CGContextScaleCTM(d->Ctx, 1.0, -1.0);
		#endif
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
