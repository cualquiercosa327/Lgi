#include "Lgi.h"
#include "resdefs.h"
#include "GTextLog.h"

//////////////////////////////////////////////////////////////////
const char *AppName = "VsLaunch";

struct VsInfo
{
	int Year;
	int MscVer;
	double Ver;
}
Versions[] =
{
	{2015, _MSC_VER_VS2015, 14.0},
	{2013, _MSC_VER_VS2013,	12.0},
	{2012, _MSC_VER_VS2012, 11.0},
	{2010, _MSC_VER_VS2010, 10.0},
	{2008, _MSC_VER_VS2008, 9.0},
	{2005, _MSC_VER_VS2005, 8.0},
	{2003, _MSC_VER_VS2003, 7.1},
	{7, _MSC_VER_VC7, 7.0},
	{6, _MSC_VER_VC6, 6.0},
	{5, _MSC_VER_VC5, 5.0},
};

double YearToVer(int64 y)
{
	for (int i=0; i<CountOf(Versions); i++)
	{
		if (Versions[i].Year == y)
			return Versions[i].Ver;
	}
	
	return 0.0;
}

class App : public GWindow
{
	GTextLog *Log;

public:
    App()
    {
        Name(AppName);
        GRect r(0, 0, 500, 300);
        SetPos(r);
        MoveToCenter();
        SetQuitOnClose(true);

        if (Attach(0))
        {
			AddView(Log = new GTextLog(100));
			AttachChildren();
            Visible(true);
        }
    }
    
	void OnReceiveFiles(GArray<char*> &Files)
	{
		Log->Print("Got %i files...\n", Files.Length());
		for (unsigned i=0; i<Files.Length(); i++)
		{
			const char *File = Files[i];
			Log->Print("\t%s...\n", File);
	
			const char *Ext = LgiGetExtension(File);
			if (Ext && !_stricmp(Ext, "sln"))
			{
				GFile f;
				if (f.Open(File, O_READ))
				{
					GString::Array a = f.Read().Split("\n");
					f.Close();
					
					double FileVersion = 0.0;
					int64 VsYear = 0;
					double VsVersion = 0.0;
					for (unsigned i=0; i<a.Length(); i++)
					{
						if (a[i].Find("File, Format Version") >= 0)
						{
							GString::Array p = a[i].Split(" ");
							FileVersion = p.Last().Float();
						}
						else if (a[i](0) == '#')
						{
							GString::Array p = a[i].Split(" ");
							VsYear = p.Last().Int();
							VsVersion = YearToVer(VsYear);
						}
					}
					
					if (VsVersion > 0.0)
					{
						// C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE\devenv.exe
						GFile::Path p(LSP_USER_APPS, 32);
						GString v;
						v.Printf("Microsoft Visual Studio %.1f", VsVersion);
						p += v;
						p += "Common7\\IDE\\devenv.exe";
						if (p.IsFile())
						{
							LgiExecute(p, File);
							LgiCloseApp();
						}
						else Log->Print("\t\tError: The file '%s' doesn't exist.", p.GetFull().Get());
					}
					else Log->Print("\t\tError: No version in '%s'.", File);
				}
				else Log->Print("\t\tError: Failed to open '%s'\n", File);
			}
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

