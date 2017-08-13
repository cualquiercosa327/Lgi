#include "Lgi.h"
#include "LList.h"
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
	::GString PrinterName;
	GPrintEvents *Events;
	
	GAutoPtr<GPrintDC> PrintDC;
	
	GPrinterPrivate()
	{
		Settings = NULL;
		Events = NULL;
		Op = NULL;
	}
	
	~GPrinterPrivate()
	{
		if (Op)
			g_object_unref(Op);
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

	GtkPrintSettings *settings = gtk_print_operation_get_print_settings(operation);
	if (settings)
	{
		d->PrinterName = gtk_print_settings_get_printer(settings);
	}

	if (d->PrintDC.Reset(new GPrintDC(context, d->JobName, d->PrinterName)))
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
		LgiAssert(d->PrintDC->Handle() == ct); // Just checking it's the same handle
	
	bool r = d->Events->OnPrintPage(d->PrintDC, page_number);
}

bool GPrinter::Print(GPrintEvents *Events, const char *PrintJobName, int Pages /* = -1 */, GView *Parent /* = 0 */)
{
	if (!Events)
	{
		LgiTrace("%s:%i - Error: missing param.\n", _FL);
		return false;
	}
	
	d->Op = gtk_print_operation_new();
	if (!d->Op)
	{
		LgiTrace("%s:%i - Error: gtk_print_operation_new failed.\n", _FL);
		return false;
	}
		
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

	g_signal_connect(G_OBJECT(d->Op), "begin-print", G_CALLBACK(GtkPrintBegin), d);
	g_signal_connect(G_OBJECT(d->Op), "draw-page", G_CALLBACK(GtkPrintDrawPage), d);
	
	gtk_print_operation_set_job_name(d->Op, PrintJobName);
	Result = gtk_print_operation_run(d->Op,
									GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
									Wnd,
									&Error);    
    
    bool Status = Result != GTK_PRINT_OPERATION_RESULT_ERROR;
    if (!Status)
    {
		if (Error)
		{
			LgiTrace("%s:%i - gtk_print_operation_run failed with: %i - %s\n",
					_FL,
					Error->code,
					Error->message);
		}
		else
		{
			LgiTrace("%s:%i - gtk_print_operation_run failed with: unknown error\n", _FL);
		}
    }

	if (d->Op)
	{
		g_object_unref(d->Op);
		d->Op = NULL;
	}
    
    return Status;
}
