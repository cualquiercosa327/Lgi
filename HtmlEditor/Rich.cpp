#include "Lgi.h"
#include "GHtmlEdit.h"
#include "GTextView3.h"
#include "GTree.h"
#include "GTabView.h"
#include "GBox.h"

enum Ctrls
{
	IDC_EDITOR = 100,
	IDC_HTML,
	IDC_TABS,
	IDC_TREE,
};

#if 1

char Src[] =
	"This is a test. A longer string for the purpose of inter-line cursor testing.<br>\n"
	"A longer string for the <b>purpose</b> of inter-line cursor testing.<br>\n"
	"--<br>"
	"<a href=\"http://web/~matthew\">Matthew Allen</a>";

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
	GBox *Split;
	GTextView3 *Txt;
	GTabView *Tabs;
	GTree *Tree;
	uint64 LastChange;

public:
	App()
	{
		LastChange = 0;
		Edit = 0;
		Txt = 0;
		Tabs = NULL;
		Tree = NULL;
		Name("Rich Text Testbed");
		GRect r(0, 0, 1200, 800);
		SetPos(r);
		MoveToCenter();
		SetQuitOnClose(true);
		if (Attach(0))
		{
			AddView(Split = new GBox);
			if (Split)
			{
				Split->AddView(Edit = new GHtmlEdit);
				if (Edit)
				{
					Edit->SetId(IDC_EDITOR);

					#if 1
					Edit->Name(Src);
					#else
					char *s = ReadTextFile("C:\\Documents and Settings\\matthew\\Desktop\\paypal.html");
					Edit->Name(s);
					DeleteArray(s);
					#endif
				}

				Split->AddView(Tabs = new GTabView(IDC_TABS));
				if (Tabs)
				{
					GTabPage *p = Tabs->Append("Html Output");
					if (p)
					{
						p->AddView(Txt = new GTextView3(IDC_HTML, 0, 0, 100, 100));
						Txt->SetPourLargest(true);
					}
					
					p = Tabs->Append("Node View");
					if (p)
					{
						p->AddView(Tree = new GTree(IDC_TREE, 0, 0, 100, 100));
						Tree->SetPourLargest(true);
					}
					
					Tabs->Value(1);
				}

				Split->Value(GetClient().X()/2);
			}

			AttachChildren();
			Pour();
			Visible(true);
			SetPulse(200);
		}
	}

	int OnNotify(GViewI *c, int f)
	{
		if (c->GetId() == IDC_EDITOR &&
			#if 1
			(f == GTVN_DOC_CHANGED || f == GTVN_CURSOR_CHANGED) &&
			#else
			(f == GTVN_DOC_CHANGED) &&
			#endif
			Edit)
		{
			LastChange = LgiCurrentTime();
			Tree->Empty();
		}

		return 0;
	}
	
	void OnPulse()
	{
		uint64 Now = LgiCurrentTime();
		if (LastChange != 0 && Now - LastChange > 1500)
		{
			LastChange = 0;
			
			if (Txt)
				Txt->Name(Edit->Name());
			
			if (Edit && Tree)
				Edit->DumpNodes(Tree);
		}
	}
};

int LgiMain(OsAppArguments &AppArgs)
{
	GApp a(AppArgs, "RichEditTest");
	a.AppWnd = new App;
	a.Run();
	return 0;
}
