#include "Lgi.h"
#include "GTextView3.h"
#include "GTree.h"
#include "GTabView.h"
#include "GBox.h"
#include "resource.h"
#include "LgiSpellCheck.h"

#if 1
#include "GRichTextEdit.h"
typedef GRichTextEdit EditCtrl;
#else
#include "GHtmlEdit.h"
typedef GHtmlEdit EditCtrl;
#endif

enum Ctrls
{
	IDC_EDITOR = 100,
	IDC_HTML,
	IDC_TABS,
	IDC_TREE,
	IDC_TO_HTML,
	IDC_TO_NODES,
	IDC_VIEW_IN_BROWSER,
	IDC_SAVE_FILE,
	IDC_SAVE,
	IDC_EXIT
};

#define LOAD_DOC 1
#define SrcFileName	"Reply4.html"

#if 0

char Src[] =
	"This is a test. A longer string for the purpose of inter-line cursor testing.<br>\n"
	"A longer string for the <b>purpose</b> of inter-line cursor testing.<br>\n"
	"--<br>"
	"<a href=\"http://web/~matthew\">Matthew Allen</a>";

#else

char Src[] =	
	"<html>\n"
	"<body style=\"font-size: 9pt;\">Hi Suzy,<br/>\n"
	"<br/>\n"
	"The aircon is a little warm in our area of the woods. There seems to be nothing coming out of the 2nd AC unit.<br/>\n"
	"<br/>\n"
	"Regards<br/>\n"
	"--<br/>\n"
	"<a href=\"http://web/~matthew\">Matthew Allen</a>\n"
	"</body>\n"
	"</html>";

#endif

class App : public GWindow
{
	GBox *Split;
	GTextView3 *Txt;
	GTabView *Tabs;
	GTree *Tree;
	uint64 LastChange;

	EditCtrl *Edit;

	GAutoPtr<GSpellCheck> Speller;

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
		#ifdef WIN32
		SetIcon((const char*)IDI_APP);
		#endif
		
		#if defined(WINDOWS)
		Speller = CreateWindowsSpellCheck();
		#elif defined(MAC)
		Speller = CreateAppleSpellCheck();
		#elif !defined(LINUX)
		Speller = CreateAspellObject();
		#endif
		
		if (Attach(0))
		{
			Menu = new GMenu;
			if (Menu)
			{
				Menu->Attach(this);

				GSubMenu *s = Menu->AppendSub("&File");
				s->AppendItem("To &HTML", IDC_TO_HTML, true, -1, "F5");
				s->AppendItem("To &Nodes", IDC_TO_NODES, true, -1, "F6");
				s->AppendItem("View In &Browser", IDC_VIEW_IN_BROWSER, true, -1, "F7");
				s->AppendItem("&Save", IDC_SAVE, true, -1, "Ctrl+S");
				s->AppendItem("Save &As", IDC_SAVE_FILE, true, -1, "Ctrl+Shift+S");
				s->AppendSeparator();
				s->AppendItem("E&xit", IDC_EXIT, true, -1, "Ctrl+W");
			}

			AddView(Split = new GBox);
			if (Split)
			{
				Split->AddView(Edit = new EditCtrl(IDC_EDITOR));
				if (Edit)
				{
					if (Speller)
						Edit->SetSpellCheck(Speller);
					Edit->Sunken(true);
					Edit->SetId(IDC_EDITOR);
					// Edit->Name("<span style='color:#800;'>The rich editor control is not functional in this build.</span><b>This is some bold</b>");

					#if LOAD_DOC
					#ifndef SrcFileName
					Edit->Name(Src);
					#else
					char p[MAX_PATH];
					LgiGetSystemPath(LSP_APP_INSTALL, p, sizeof(p));
					LgiMakePath(p, sizeof(p), p, "Test");
					LgiMakePath(p, sizeof(p), p, SrcFileName);
					GAutoString html(ReadTextFile(p));
					if (html)
						Edit->Name(html);
					#endif
					#endif
				}

				Split->AddView(Tabs = new GTabView(IDC_TABS));
				if (Tabs)
				{
					Tabs->Debug();
					
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
				}

				Split->Value(GetClient().X()/2);
			}

			AttachChildren();
			Pour();
			Visible(true);

			if (Edit)
				Edit->Focus(true);
		}
	}

	int OnCommand(int Cmd, int Event, OsView Wnd)
	{
		switch (Cmd)
		{
			case IDC_EXIT:
				LgiCloseApp();
				break;
			case IDC_TO_HTML:
				Tabs->Value(0);
				Txt->Name(Edit->Name());
				break;
			case IDC_TO_NODES:
				Tabs->Value(1);
				#ifdef _DEBUG
				Tree->Empty();
				Edit->DumpNodes(Tree);
				#endif
				break;
			case IDC_VIEW_IN_BROWSER:
			{
				GFile::Path p(LSP_TEMP);
				p += "export.html";
				if (Edit->Save(p))
				{
					LgiExecute(p);
				}
				break;
			}
			case IDC_SAVE_FILE:
			{
				GFileSelect s;
				s.Parent(this);
				s.Type("HTML", "*.html");
				if (s.Save())
					Edit->Save(s.Name());
				break;
			}
			case IDC_SAVE:
			{
				if (Edit)
				{
					GString Html;
					GArray<GDocView::ContentMedia> Media;
					if (Edit->GetFormattedContent("text/html", Html, &Media))
					{
						GFile::Path p(LSP_APP_INSTALL);
						p += "Output";
						if (!p.IsFolder())
							FileDev->CreateFolder(p);
						p += "output.html";
						GFile f;
						if (f.Open(p, O_WRITE))
						{
							f.SetSize(0);
							f.Write(Html);
							f.Close();
							
							GString FileName = p.GetFull();
							
							for (unsigned i=0; i<Media.Length(); i++)
							{
								GDocView::ContentMedia &Cm = Media[i];
								
								p.Parent();
								p += Cm.FileName;
								if (f.Open(p, O_WRITE))
								{
									f.SetSize(0);
									if (Cm.Data.IsBinary())
										f.Write(Cm.Data.CastVoidPtr(), Cm.Data.Value.Binary.Length);
									else if (Cm.Stream)
									{
										GCopyStreamer Cp;
										Cp.Copy(Cm.Stream, &f);
									}
									else
										LgiAssert(0);
									f.Close();
								}
							}
							
							LgiExecute(FileName);
						}
					}
				}
				break;
			}
		}

		return GWindow::OnCommand(Cmd, Event, Wnd);
	}

	int OnNotify(GViewI *c, int f)
	{
		if (c->GetId() == IDC_EDITOR &&
			#if 1
			(f == GNotifyDocChanged || f == GNotifyCursorChanged) &&
			#else
			(f == GNotifyDocChanged) &&
			#endif
			Edit)
		{
			LastChange = LgiCurrentTime();
			Tree->Empty();
		}

		return 0;
	}
	
	void OnReceiveFiles(GArray<char*> &Files)
	{
		if (Edit && Files.Length() > 0)
		{
			GAutoString t(ReadTextFile(Files[0]));
			if (t)
				Edit->Name(t);
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
