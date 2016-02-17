#include "Lgi.h"
#include "GPrinter.h"

////////////////////////////////////////////////////////////////////
GPrinter::GPrinter()
{
}

GPrinter::~GPrinter()
{
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


