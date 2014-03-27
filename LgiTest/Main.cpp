#include "Lgi.h"
#include "GEdit.h"

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
		a.AppWnd = new App;
		a.Run();
	}
	return 0;
}
