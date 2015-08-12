#include "Lgi.h"
#include "Base64.h"

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
	)
	{
		if (!Browse(Parent))
		{
			return 0;
		}
	}

	return new GPrintDC(0, 0);
}

bool GPrinter::Browse(GView *Parent)
{
	return false;
}

bool GPrinter::GetPageRange(GArray<int> &p)
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

