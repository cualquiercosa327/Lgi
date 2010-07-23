#include "Lgi.h"
#include "GButton.h"
#include "GEdit.h"
#include "GCombo.h"
#include "LgiSkinGel.h"

#define IDM_EXIT 4

class Test : public GDialog
{
public:
    Test(GViewI *p)
    {
        SetParent(p);
        SetPos(GRect(0, 0, 300, 300));
        MoveToCenter();
        
		GCombo *c;
		AddView(c = new GCombo(100, 10, 40, 100, 20, ""));
		c->Insert("One");
		c->Insert("Two");
		c->Insert("Three");

		AddView(new GButton(50, 10, 10, -1, -1, "Open"));

        /*
		AddView(c = new GCombo(200, 10, 140, 100, 20, ""));
		c->Insert("One");
		c->Insert("Two");
		c->Insert("Three");
		*/

        DoModal();
    }
    
	int OnNotify(GViewI *c, int f)
	{
		switch (c->GetId())
		{
		    case 50:
		    {
		        Test Dlg(this);
		        break;
		    }
		}
		return 0;
	}
};

class App : public GWindow
{
    GImageList *i;
    
public:
    App()
    {
        i = LgiLoadImageList("cmds.png", 16, 16);
        Name("Gtk Test");
        SetQuitOnClose(true);
        GRect r(0, 0, 1000, 800);
        SetPos(r);
        MoveToCenter();
        if (Attach(0))
        {
            /*
            #if 1
            Menu = new GMenu();
            Menu->Attach(this);
            GSubMenu *s = Menu->AppendSub("File");
            s->AppendItem("Open", 1, true);
            s->AppendItem("Save", 2, true);
            s->AppendItem("Close", 3, true);
            s->AppendSeparator();
            s->AppendItem("Exit", IDM_EXIT, true);
            s = Menu->AppendSub("Edit");
            s->AppendItem("Copy", 11, true);
            s->AppendItem("Paste", 12, true);
            s = Menu->AppendSub("Tools");
            s->AppendItem("Options", 21, true);
            s = Menu->AppendSub("Help");
            s->AppendItem("Help", 31, true);
            s->AppendItem("About", 31, true);
            #endif

			GToolBar *t = new GToolBar;
            t->SetImageList(i, 16, 16);
            t->AppendButton("New", 1);
            t->AppendButton("Open", 2);
            t->AppendButton("Save", 3);
            AddView(t);

            #if 1
            GSplitter *split = new GSplitter();
            split->Raised(true);
            split->IsVertical(true);
            split->Border(true);
            split->Value(200);
            AddView(split);
            #endif
			*/
			
			AddView(new GButton(50, 10, 10, -1, -1, "Open"));

		    GCombo *c;
		    AddView(c = new GCombo(100, 10, 40, 100, 20, ""));
		    c->Insert("One");
		    c->Insert("Two");
		    c->Insert("Three");


            AttachChildren();
            // LgiMsg(this, "Test", "App");
            Visible(true);
        }
    }

	int OnCommand(int Cmd, int Event, OsView Wnd)
	{
		switch (Cmd)
		{
			case IDM_EXIT:
				LgiCloseApp();
				break;
		}
		return 0;
	}
	
	int OnNotify(GViewI *c, int f)
	{
		switch (c->GetId())
		{
		    case 50:
		    {
		        Test Dlg(this);
		        break;
		    }
		}
		return 0;
	}
};

int LgiMain(OsAppArguments &AppArgs)
{
    GApp a("application/x-gtk-test", AppArgs);
    if (a.IsOk())
    {
        a.SkinEngine = CreateSkinEngine(&a);

        a.AppWnd = new App;
        a.Run();
    }    
    return 0;
}
