#include "Lgi.h"
#include "GPrinter.h"

class GPrinterPrivate
{
public:
	GString Err;
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

#define MAGIC_PRINTDLG					0xAAFF0100
#define MAGIC_DEVMODE					0xAAFF0101
#define MAGIC_DEVNAMES					0xAAFF0102

bool GPrinter::Serialize(char *&Str, bool Write)
{
	if (Write)
	{
	}
	else
	{
	}

	return false;
}

GString GPrinter::GetErrorMsg()
{
	return d->Err;
}

bool GPrinter::Print(GPrintEvents *Events, const char *PrintJobName, int Pages, GView *Parent)
{
	return false;
}