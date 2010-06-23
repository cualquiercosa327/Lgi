#include "Lgi.h"
#include "GList.h"
#include "GButton.h"

////////////////////////////////////////////////////////////////////
class GPrinterPrivate
{
public:
	char *Printer;
	
	GPrinterPrivate()
	{
		Printer = 0;
	}
	
	~GPrinterPrivate()
	{
		DeleteArray(Printer);
	}
};

////////////////////////////////////////////////////////////////////
#define IDC_PRINTERS				100
#define IDC_PROPERTIES				101

class GPrinterDlg : public GDialog
{
	GView *Parent;
	int Dests;
	GPrinterPrivate *Printer;
	GList *Lst;

public:
	GPrinterDlg(GView *parent, int dests, GPrinterPrivate *priv)
	{
		Lst = 0;
		SetParent(parent);
		Dests = dests;
		Printer = priv;
		
		GRect r(0, 0, 250, 250);
		Name("Select Printer");
		Children.Insert(Lst = new GList(IDC_PRINTERS, 10, 10, 150, 220));
		Children.Insert(new GButton(IDOK, 170, 10, 70, 20, "Ok"));
		Children.Insert(new GButton(IDCANCEL, 170, 35, 70, 20, "Cancel"));
		Children.Insert(new GButton(IDC_PROPERTIES, 170, 60, 70, 20, "Properties"));
		SetPos(r);
		MoveToCenter();
		
		SetCtrlEnabled(IDOK, Dests > 0);
	}	
	
	int OnNotify(GViewI *c, int f)
	{
		switch (c->GetId())
		{
			case IDOK:
			{
				if (Lst)
				{
					DeleteArray(Printer->Printer);
					
					GListItem *l = Lst->GetSelected();
					if (l)
					{
						Printer->Printer = NewStr(l->GetText(0));
					}
				}
			}
			case IDCANCEL:
			{
				EndModal(c->GetId() == IDOK);
				break;
			}				
		}
		return 0;
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
	/*
	if (Dests AND Dest)
	{
		GPrinterDlg Dlg(Parent, Dests, Dest, d);
		if (Dlg.DoModal())
		{
			return true;
		}
	}
	else
	{
		LgiMsg(Parent, "No printers detected.", "GPrinter");
	}
	*/

	return false;
}

#define MAGIC_PRINTDLG					0xAAFF0100
#define MAGIC_DEVMODE					0xAAFF0101
#define MAGIC_DEVNAMES					0xAAFF0102

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

GPrintDC *GPrinter::StartDC(char *PrintJobName, GView *Parent)
{
	return new GPrintDC(d->Printer, PrintJobName);
}
