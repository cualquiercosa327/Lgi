#include "Lgi.h"
#include "Base64.h"
#include "GPrinter.h"

////////////////////////////////////////////////////////////////////
class GPrinterPrivate
{
public:
	int Pages;

	GPrinterPrivate()
	{
		Pages = 3;
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

bool GPrinter::Print(GPrintEvents *Events, const char *PrintJobName, int Pages, GView *Parent)
{
	return false;
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
	return false;
}

