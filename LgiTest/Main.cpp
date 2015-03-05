#include "Lgi.h"
#include "GEdit.h"

#include "GStringClass.h"

void GStringTest()
{
	GString a("This<TD>is<TD>a<TD>test");
	int idx = a.RFind("<TD>");
	GString f = a(8,10);
	GString end = a(-5, -1);
	
	GString sep(", ");
	GString::Array parts = GString("This is a test").Split(" ");
	GString joined = sep.Join(parts);
	
	GString src("  asdwer   ");
	GString left = src.LStrip();
	GString right = src.RStrip();
	GString both = src.Strip();
	
}

class App : public GWindow
{
	GEdit *e;
	GEdit *e2;

public:
	App()
	{
		GRect r(0, 0, 1000, 800);
		SetPos(r);
		MoveToCenter();
		SetQuitOnClose(true);
		
		if (Attach(0))
		{
			AddView(e = new GEdit(100, 10, 10, 200, 22));
			AddView(e2 = new GEdit(101, 10, 50, 200, 22));
			// e->Focus(true);
			e->Password(true);
			e->SetEmptyText("(this is a test)");
			
			AttachChildren();
			Visible(true);
		}
	}
};

int LgiMain(OsAppArguments &AppArgs)
{
	GApp a(AppArgs, "Lgi Test");
	if (a.IsOk())
	{
		GStringTest();
		a.AppWnd = new App;
		a.Run();
	}
	return 0;
}
