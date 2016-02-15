#include "Lgi.h"
#include "GList.h"
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
		DeleteArray(d->Printer);
		d->Printer = NewStr(Str);
	}

	return true;
}

GString GPrinter::GetErrorMsg()
{
	return d->Err;
}

bool GPrinter::Print(GPrintEvents *Events, const char *PrintJobName, int Pages, GView *Parent)
{
	return false;
}
