#include "Lgi.h"
#include "GList.h"
#include "GButton.h"
#include "GPrinter.h"

using namespace Gtk;

////////////////////////////////////////////////////////////////////
class GPrinterPrivate
{
public:
	GtkPrintOperation *Op;
	GtkPrintSettings *Settings;
	::GString JobName;
	::GString Printer;
	::GString Err;
	GPrintEvents *Events;
	GAutoPtr<GPrintDC> PrintDC;
	
	GPrinterPrivate()
	{
		Settings = NULL;
		Events = NULL;
		Op = gtk_print_operation_new();	
	}
	
	~GPrinterPrivate()
	{
		if (Op)
			g_object_unref(Op);
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
	::GList *Lst;

public:
	GPrinterDlg(GView *parent, int dests, GPrinterPrivate *priv)
	{
		Lst = 0;
		SetParent(parent);
		Dests = dests;
		Printer = priv;
		
		GRect r(0, 0, 250, 250);
		Name("Select Printer");
		Children.Insert(Lst = new ::GList(IDC_PRINTERS, 10, 10, 150, 220));
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
	if (d->Settings != NULL)
		Gtk::gtk_print_operation_set_print_settings(d->Op, d->Settings);
	
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

	
::GString GPrinter::GetErrorMsg()
{
	return d->Err;
}

static void
GtkPrintBegin(	GtkPrintOperation	*operation,
				GtkPrintContext		*context,
				GPrinterPrivate		*d)
{
	bool Status = false;
	cairo_t *ct = gtk_print_context_get_cairo_context(context);
	if (ct &&
		d->PrintDC.Reset(new GPrintDC(ct, d->JobName)))
	{
		int Pages = d->Events->OnBeginPrint(d->PrintDC);
		if (Pages > 0)
		{
			gtk_print_operation_set_n_pages(d->Op, Pages);
			Status = true;
		}
	}
	if (!Status)
		gtk_print_operation_cancel(d->Op);
}
	
static void
GtkPrintDrawPage(	GtkPrintOperation	*operation,
					GtkPrintContext		*context,
					gint				page_number,
					GPrinterPrivate		*d)
{
	cairo_t *ct = gtk_print_context_get_cairo_context(context);
	if (ct && d->PrintDC)
		d->PrintDC->Handle(ct);
	
	bool r = d->Events->OnPrintPage(d->PrintDC, page_number);
}

bool GPrinter::Print(GPrintEvents *Events, const char *PrintJobName, int Pages /* = -1 */, GView *Parent /* = 0 */)
{
	if (!d->Op || !Events)
		return false;
		
	GError *Error = NULL;
	GtkPrintOperationResult Result;
	GtkWindow *Wnd = NULL;
	
	if (Parent)
	{
		GWindow *w = Parent->GetWindow();
		if (w)
			Wnd = GTK_WINDOW(w->Handle());
	}
	
	d->Events = Events;
	d->JobName = PrintJobName;

	g_signal_connect(d->Op, "begin-print", G_CALLBACK(GtkPrintBegin), d);
	g_signal_connect(d->Op, "draw-page", G_CALLBACK(GtkPrintDrawPage), d);
	
	gtk_print_operation_set_job_name(d->Op, PrintJobName);
	Result = gtk_print_operation_run(d->Op,
									GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
									Wnd,
									&Error);    
    
    return Error == NULL;
}
