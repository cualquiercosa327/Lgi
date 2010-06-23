#include "Lgi.h"
#include "GHtmlEdit.h"
#include "GTextView3.h"

enum Ctrls
{
	IDC_EDITOR = 100,
	IDC_HTML,
};

#if 1

char Src[] =
	"<br>\n"
	"<br>\n"
	"<br>\n"
	"<br>\n"
	"<br>\n"
	"<br>\n"
	"<br>\n"
	"<br>\n"
	"<br>\n"
	"<br>\n"
	"--<br>\n"
	"Matthew Allen\n";

#else

char Src[] =
	"<html>\n"
	"<body style='font-size:18pt;'>\n"
	"	First test. This is <b>bold</b>. This is a test. This is a test. This is a test.\n"
	"	<ul>\n"
	"		<li>This is a list item.\n"
	"		<li>This is a list item.\n"
	"		<li>This is a list item.\n"
	"		<li>This is a list item.\n"
	"	</ul>\n"
	"	bbbbb bbbbbb bbbbbbb bbbbbbb bbbbbbb bbbbbb. This is a test. This is a test.\n"
	"	This is a test. This is a test. This is a test. This is a test. This is a test.\n"
	"</body>\n"
	"</html>";

#endif

class App : public GWindow
{
	GHtmlEdit *Edit;
	GSplitter *Split;
	GTextView3 *Txt;

public:
	App()
	{
		Edit = 0;
		Txt = 0;
		Name("Rich Text Testbed");
		GRect r(0, 0, 1200, 800);
		SetPos(r);
		MoveToCenter();
		SetQuitOnClose(true);
		if (Attach(0))
		{
			Split = new GSplitter;
			if (Split)
			{
				Split->Value(GetClient().X()/2);
				Split->Attach(this);

				Edit = new GHtmlEdit;
				if (Edit)
				{
					Edit->SetId(IDC_EDITOR);
					Split->SetViewA(Edit, false);

					#if 1
					Edit->Name(Src);
					#else
					char *s = ReadTextFile("C:\\Documents and Settings\\matthew\\Desktop\\paypal.html");
					Edit->Name(s);
					DeleteArray(s);
					#endif
				}

				Txt = new GTextView3(IDC_HTML, 0, 0, 100, 100);
				if (Txt)
				{
					Split->SetViewB(Txt, true);
				}	
			}

			Pour();
			Visible(true);
		}
	}

	int OnNotify(GViewI *c, int f)
	{
		if (c->GetId() == IDC_EDITOR AND f == GTVN_DOC_CHANGED)
		{
			if (Txt AND Edit)
			{
				Txt->Name(Edit->Name());
			}
		}

		return 0;
	}
};

int LgiMain(OsAppArguments &AppArgs)
{
	GApp a("app", AppArgs);
	a.AppWnd = new App;
	a.Run();
	return 0;
}
