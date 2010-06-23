#include "Lgi.h"

////////////////////////////////////////////////////////////////////
GPrinter::GPrinter()
{
}

GPrinter::~GPrinter()
{
}

GPrintDC *GPrinter::StartDC(char *PrintJobName, GView *Parent)
{
	return NULL;
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

bool GPrinter::GetPageRange(GArray<int> &p)
{
	return false;
}

void GPrinter::SetPages(int p)
{
}

int GPrinter::GetPages()
{
	return 0;
}


