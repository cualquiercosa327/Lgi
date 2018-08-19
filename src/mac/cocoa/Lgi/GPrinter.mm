#include "Lgi.h"
#include "LList.h"
#include "GButton.h"
#include "GPrinter.h"

////////////////////////////////////////////////////////////////////
class GPrinterPrivate
{
public:
	GString Printer;
	GString Err;
	
	GPrinterPrivate()
	{
	}
	
	~GPrinterPrivate()
	{
	}
};


////////////////////////////////////////////////////////////////////
GPrinter::GPrinter()
{
	d = new GPrinterPrivate;
}

GPrinter::~GPrinter()
{
	DeleteObj(d);
}

bool GPrinter::Browse(GView *Parent)
{
	return false;
}

bool GPrinter::Serialize(char *&Str, bool Write)
{
	if (Write)
	{
		Str = NewStr(d->Printer);
	}
	else
	{
		d->Printer = Str;
	}
	
	return true;
}

GString GPrinter::GetErrorMsg()
{
	return d->Err;
}

#define ErrCheck(fn) \
if (e != noErr) \
{ \
d->Err.Printf("%s:%i - %s failed with %i\n", _FL, fn, e); \
LgiTrace(d->Err); \
goto OnError; \
}


bool GPrinter::Print(GPrintEvents *Events, const char *PrintJobName, int MaxPages, GView *Parent)
{
	if (!Events)
	{
		LgiAssert(0);
		return false;
	}
	
	
	bool Status = false;
	#if 0
	PMPrintSession ps = NULL;
	PMPageFormat PageFmt = NULL;
	PMPrintSettings PrintSettings = NULL;
	GWindow *Wnd = Parent ? Parent->GetWindow() : NULL;
	Boolean Accepted = false;
	Boolean Changed = false;
	GAutoPtr<GPrintDC> dc;
	int Pages;
	GPrintDcParams Params;
	double paperWidth, paperHeight;
	PMPaper Paper = NULL;
	UInt32 ResCount;
	PMPrinter CurrentPrinter = NULL;
	
	OSStatus e = PMCreateSession(&ps);
	ErrCheck("PMCreateSession");
	
	e = PMCreatePageFormat(&PageFmt);
	ErrCheck("PMCreatePageFormat");
	
	e = PMSessionDefaultPageFormat(ps, PageFmt);
	ErrCheck("PMSessionDefaultPageFormat");
	
	e = PMCreatePrintSettings(&PrintSettings);
	ErrCheck("PMCreatePrintSettings");
	
#if 0
	e = PMSessionUseSheets(ps, Wnd ? Wnd->WindowHandle() : NULL, NULL /*PMSheetDoneUPP sheetDoneProc*/);
	ErrCheck("PMSessionUseSheets");
#endif
	
	e = PMSessionPrintDialog(ps, PrintSettings, PageFmt, &Accepted);
	ErrCheck("PMSessionPrintDialog");
	
	e = PMSessionValidatePrintSettings(ps, PrintSettings, &Changed);
	e = PMSessionValidatePageFormat(ps, PageFmt, &Changed);
	
	e = PMSessionBeginCGDocumentNoDialog(ps, PrintSettings, PageFmt);
	ErrCheck("PMSessionBeginCGDocumentNoDialog");
	
	e = PMSessionBeginPageNoDialog(ps, PageFmt, NULL);
	ErrCheck("PMSessionBeginPageNoDialog");
	
	e = PMGetAdjustedPaperRect(PageFmt, &Params.Page); //PMGetUnadjustedPageRect
	ErrCheck("PMGetAdjustedPaperRect");
	
	e = PMSessionGetCGGraphicsContext(ps, &Params.Ctx);
	ErrCheck("PMSessionGetCGGraphicsContext");
	
	e = PMSessionGetCurrentPrinter(ps, &CurrentPrinter);
	ErrCheck("PMSessionGetCurrentPrinter");
	
	e = PMGetPageFormatPaper(PageFmt, &Paper);
	e = PMPaperGetWidth(Paper, &paperWidth);
	e = PMPaperGetHeight(Paper, &paperHeight);
	
	e = PMPrinterGetPrinterResolutionCount(CurrentPrinter, &ResCount);
	ErrCheck("PMPrinterGetPrinterResolutionCount");
	
	for (unsigned i=0; i<ResCount; i++)
	{
		e = PMPrinterGetIndexedPrinterResolution(CurrentPrinter, i, &Params.Dpi);
	}
	
	e = PMPrinterSetOutputResolution(CurrentPrinter, PrintSettings, &Params.Dpi);
	ErrCheck("PMPrinterSetOutputResolution");
	
	dc.Reset(new GPrintDC(&Params, PrintJobName));
	Pages = Events->OnBeginPrint(dc);
	for (int Page = 0; Page < Pages; Page++)
	{
		if (Page > 0)
		{
			e = PMSessionBeginPage(ps, PageFmt, NULL);
			ErrCheck("PMSessionBeginPage");
			
			e = PMSessionGetCGGraphicsContext(ps, &Params.Ctx);
			ErrCheck("PMSessionGetCGGraphicsContext");
			
			dc.Reset(new GPrintDC(&Params, PrintJobName));
		}
		
		Status |= Events->OnPrintPage(dc, Page);
		PMSessionEndPage(ps);
	}
	
	e = PMSessionEndDocumentNoDialog(ps);
	ErrCheck("PMSessionEndDocumentNoDialog");
	
	return Status;
	
OnError:
	PMRelease(PrintSettings);
	PMRelease(ps);
	#endif
	
	return Status;
}
