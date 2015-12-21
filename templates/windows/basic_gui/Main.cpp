#include "Lgi.h"
#include "resdefs.h"

//////////////////////////////////////////////////////////////////
const char *AppName = "%name%";

class App : public GWindow
{
public:
    App()
    {
        Name(AppName);
        GRect r(0, 0, 1000, 800);
        SetPos(r);
        MoveToCenter();
        SetQuitOnClose(true);

        if (Attach(0))
        {
            Visible(true);
        }
    }
};

//////////////////////////////////////////////////////////////////
int LgiMain(OsAppArguments &AppArgs)
{
	GApp a(AppArgs, AppName);
	if (a.IsOk())
	{
		a.AppWnd = new App;
		a.Run();
	}

	return 0;
}

