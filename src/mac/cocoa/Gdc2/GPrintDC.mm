#include "Lgi.h"
#include "GdiLeak.h"

class GPrintDCPrivate
{
public:
	int x, y;
	GdcPt2 Dpi;
	bool PageOpen;
	bool DocOpen;
	#if 0
	CGContextRef Ctx;
	#endif
	GString PrintJobName;
	GString PrinterName;
	
	GPrintDCPrivate()
	{
		PageOpen = false;
		DocOpen = false;
		x = y = 0;
		#if 0
		Ctx = NULL;
		#endif
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
		#if 0
		int x = Params->Page.right - Params->Page.left;
		int y = Params->Page.bottom - Params->Page.top;
		
		d->Dpi.x = Params->Dpi.hRes;
		d->Dpi.y = Params->Dpi.vRes;
		
		d->x = x;
		d->y = y;
		
		d->Ctx = Params->Ctx;
		
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
