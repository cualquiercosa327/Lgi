#include "Lgi.h"
#include "GHtml.h"
#include "GList.h"
#include "GDateTime.h"
#include "GClipBoard.h"
#include "GScripting.h"
#include "GBox.h"
#include "GTextView3.h"
#include "resdefs.h"
#include "GProgressDlg.h"
#include "GCombo.h"
#include "GList.h"
#include "INet.h"
#include "IHttp.h"
#include "GFilter.h"
#include "INetTools.h"
#include "ImageComparison.h"
#include "OpenSSLSocket.h"
#include "INet.h"

#define HAS_LOG_VIEW			0
#define HAS_IMAGE_LOADER		0

enum Controls
{
	IDC_HTML = 100,
	IDC_LOG,
	IDC_LIST
};

class FileInf
{
public:
	char *File;
	GDateTime Date;

	FileInf()
	{
		File = 0;
	}

	~FileInf()
	{
		DeleteArray(File);
	}
};

int InfCmp(FileInf *a, FileInf *b, NativeInt d)
{
	return -a->Date.Compare(&b->Date);
}

class HtmlItem : public GListItem
{
    char *Base;
    
public:
    HtmlItem(char *b, char *n)
    {
        Base = b;
        SetText(n);
    }
    
	void OnMouseClick(GMouse &m)
    {
        if (m.IsContextMenu())
        {
            GSubMenu s;
            s.AppendItem("Copy Path", 100);
            m.ToScreen();
            if (s.Float(GetList(), m.x, m.y, m.Left()) == 100)
            {
                GClipBoard c(GetList());
                char Path[MAX_PATH];
                LgiMakePath(Path, sizeof(Path), Base, GetText(0));
                c.Text(Path);
            }
        }
    }
};

class HtmlScriptContext :
	#ifdef _GHTML2_H
	public Html2::GHtml2,
	#else
	public Html1::GHtml,
	#endif
	public GScriptContext
{
	GScriptEngine *Eng;
	
public:
	static GHostFunc Methods[];

	HtmlScriptContext(int Id, GDocumentEnv *Env) :
		#ifdef _GHTML2_H
		Html2::GHtml2
		#else
		Html1::GHtml
		#endif
		(Id, 0, 0, 100, 100, Env)
	{
		Eng = NULL;
		
		GFont *f = new GFont;
		if (f)
		{
			f->Face("Times New Roman");
			f->PointSize(12); // Size '16' is about 12 pt. Maybe they mean 16px?
			SetFont(f, true);
		}
	}

	GHostFunc *GetCommands()
	{
		return Methods;
	}
	
	void SetEngine(GScriptEngine *Eng)
	{		
	}
	
	char *GetIncludeFile(char *FileName)
	{
		return NULL;
	}
	
	GAutoString GetDataFolder()
	{
		return GAutoString();
	}
};

GHostFunc HtmlScriptContext::Methods[] =
{
	GHostFunc(0, 0, 0),
};

class HtmlImageLoader : public GThread, public GMutex
{
	bool Loop;
	GArray<GDocumentEnv::LoadJob*> In;

public:
	HtmlImageLoader() : GThread("HtmlImageLoader")
	{
		Loop = true;
		Run();
	}
	
	~HtmlImageLoader()
	{
		Loop = false;
		while (!IsExited())
			LgiSleep(1);
	}
	
	void Add(GDocumentEnv::LoadJob *j)
	{
		if (Lock(_FL))
		{
			In.Add(j);
			Unlock();
		}
	}
	
	GAutoPtr<GSocketI> CreateSock(const char *Proto)
	{
		GAutoPtr<GSocketI> s;
		if (Proto && !_stricmp(Proto, "https"))
		{
			SslSocket *ss;
			s.Reset(ss = new SslSocket);
			ss->SetSslOnConnect(false);
		}
		else
			s.Reset(new GSocket);
		
		return s;
	}
	
	int Main()
	{
		while (Loop)
		{
			GAutoPtr<GThreadJob> j;
			if (Lock(_FL))
			{
				if (In.Length())
				{
					j.Reset(In[0]);
					In.DeleteAt(0, true);
				}
				Unlock();
			}
			
			GDocumentEnv::LoadJob *Job = dynamic_cast<GDocumentEnv::LoadJob*>(j.Get());
			if (Job)
			{
				GUri u(Job->Uri);
				if
				(
					u.Protocol &&
					(
						!stricmp(u.Protocol, "http") ||
						!stricmp(u.Protocol, "https")
					)
				)
				{				
					IHttp http;
					GProxyUri proxy;
					if (proxy.Host)
						http.SetProxy(proxy.Host, proxy.Port);
					
					GStringPipe p;
					int Status = 0;
					IHttp::ContentEncoding Encoding = IHttp::EncodeRaw;
					GAutoPtr<GSocketI> sock = CreateSock(u.Protocol);
					if (http.Open(sock, u.Host, u.Port))
					{
						int RedirCount = 0;
						GStringPipe Headers;
						bool Result = http.Get(Job->Uri, NULL, &Status, &p, &Encoding, &Headers);
						while (Result && ((Status / 100) == 3) && RedirCount < 4)
						{
							GAutoString Hdr(Headers.NewStr());
							GAutoString Loc(InetGetHeaderField(Hdr,	"Location"));
							u.Set(Loc);
							sock = CreateSock(u.Protocol);
							
							if (!_stricmp(Loc, Job->Uri))
							{
								// Same location as before??
								LgiTrace("%s\n", Hdr.Get());
								break;
							}
							
							Result = http.Get(Loc, NULL, &Status, &p, &Encoding, &Headers);
							RedirCount++;
						}
						
						if (Result && ((Status / 100) == 2))
						{
							uchar Hint[16];
							p.Peek(Hint, sizeof(Hint));
							GAutoPtr<GFilter> Filter(GFilterFactory::New(u.Path, FILTER_CAP_READ, Hint));
							if (Filter)
							{
								GAutoPtr<GSurface> Img(new GMemDC);
								GFilter::IoStatus Rd = Filter->ReadImage(Img, &p);
								if (Rd == GFilter::IoSuccess)
								{
									Job->pDC = Img;
									if (Job->Env)
									{
										// LgiTrace("Loaded '%s' as image %ix%i\n", u.Path, j->pDC->X(), j->pDC->Y());
										Job->Env->OnDone(j);
									}
									else
									{
										LgiTrace("%s:%i - No env for '%s'\n", _FL, u.Path);
										LgiAssert(0);
									}
								}
								else LgiTrace("%s:%i - Failed to read '%s'\n", _FL, u.Path);
							}
							else LgiTrace("%s:%i - Failed to find filter for '%s'\n", _FL, u.Path);
						}
						else LgiTrace("%s:%i - Failed to get '%s', status=%i\n", _FL, Job->Uri.Get(), Status);
					}
					else LgiTrace("%s:%i - Failed to open connection to '%s:%i'\n", _FL, u.Host, u.Port);
				}
				else
				{
					// Local document?
					if (Job->pDC.Reset(LoadDC(Job->Uri)))
					{
						GDocumentEnv *e = Job->Env;
						if (e)
						{
							// LgiTrace("Loaded '%s' as image %ix%i\n", u.Path, j->pDC->X(), j->pDC->Y());
							e->OnDone(j);
						}
					}
				}
			}
			else LgiSleep(1);
		}
	
		return 0;
	}
};

class AppWnd : public GWindow, public GDefaultDocumentEnv, public GNetwork
{
	GList *Lst;
    HtmlScriptContext *Html;
    GTextView3 *Text;
	char Base[256];
	GAutoPtr<GScriptEngine> Script;
	GAutoPtr<HtmlImageLoader> Worker;

	LoadType GetContent(LoadJob *&j)
	{
		GUri u(j->Uri);
		if (!u.Protocol)
		{
			char p[MAX_PATH];
			if (LgiMakePath(p, sizeof(p), Base, j->Uri) &&
				FileExists(p))
			{
				GString Ext = LgiGetExtension(p);
				if (Ext.Equals("css") ||
					Ext.Equals("html"))
				{
					GAutoPtr<GFile> f(new GFile);
					if (f && f->Open(p, O_READ))
					{
						j->Stream.Reset(f.Release());
						return LoadImmediate;
					}
				}
				else
				{
					j->pDC.Reset(LoadDC(p));
					if (j->pDC)
						return LoadImmediate;
				}

				return LoadError;
			}
		}		
	
		#if HAS_IMAGE_LOADER
		if (!Worker)
			Worker.Reset(new HtmlImageLoader);
		Worker->Add(j);
		j = NULL;
		return LoadDeferred;
		#else
		return LoadNotImpl; // GDefaultDocumentEnv::GetContent(j);
		#endif
	}

public:
	AppWnd()
	{
		Html = 0;
		Lst = 0;
		Text = NULL;
		Base[0] = 0;
		
		GRect r(0, 0, 1200, 800);
		SetPos(r);
		MoveToCenter();
		Name("Html Test Suite");
		SetQuitOnClose(true);
		if (Attach(0))
		{
			Menu = new GMenu();
			if (Menu)
			{
				Menu->Attach(this);
				Menu->Load(this, "IDM_MENU");
			}
			
			GBox *s = new GBox;
			if (s)
			{
				s->AddView(Lst = new GList(IDC_LIST, 0, 0, 100, 100));
				Lst->Sunken(false);
				Lst->AddColumn("File", 400);
				Lst->SetCssStyle("width: 200px;");

				#if HAS_LOG_VIEW
				GBox *vert = new GBox;
				vert->SetVertical(true);
				s->AddView(vert);

				vert->AddView(Html = new HtmlScriptContext(IDC_HTML, this));
				Html->SetCssStyle("height: 70%;");

				vert->AddView(Text = new GTextView3(IDC_LOG, 0, 0, 100, 100));
				
				GBox::Spacer *sp = s->GetSpacer(0);
				if (sp)
				{
					sp->Colour.Rgb(64, 64, 64);
					sp->SizePx = 2;
				}
				sp = vert->GetSpacer(0);
				if (sp)
				{
					sp->Colour.Rgb(64, 64, 64);
					sp->SizePx = 2;
				}
				#else
				s->AddView(Html = new HtmlScriptContext(IDC_HTML, this));
				#endif

				Script.Reset(new GScriptEngine(this, Html, NULL));

                #ifdef _GHTML_H
				if (Html)
					Html->SetEnv(this);
				#endif
				if (Html)
					Html->SetEnv(this);
				
				if (LgiGetExePath(Base, sizeof(Base)))
				{
					#if defined(WIN32)
					if (stristr(Base, "Release") || stristr(Base, "Debug"))
						LgiTrimDir(Base);
					#endif
					#if defined(MAC)
					LgiMakePath(Base, sizeof(Base), Base, "../../..");
					#endif

					List<FileInf> Files;
					GDirectory *d = new GDirectory;
					if (d)
					{
						LgiMakePath(Base, sizeof(Base), Base, "Files");
						for (bool b = d->First(Base); b; b = d->Next())
						{
							if (!d->IsDir() && MatchStr("*.html", d->GetName()))
							{
								char p[256];
								if (d->Path(p, sizeof(p)))
								{
									FileInf *f = new FileInf;
									if (f)
									{
										f->File = NewStr(p);
										f->Date.Set(d->GetLastWriteTime());
										Files.Insert(f);
									}
								}
							}
						}
						DeleteObj(d);
						Files.Sort(InfCmp, 0);

						for (FileInf *f = Files.First(); f; f = Files.Next())
						{
							char *d = strrchr(f->File, DIR_CHAR);
							if (d)
							{
								HtmlItem *i = new HtmlItem(Base, d + 1);
								if (i)
									Lst->Insert(i);
							}
						}

						Files.DeleteObjects();
					}
				}
				
				s->Attach(this);
			}
			
			Visible(true);
			OnNotify(FindControl(IDC_LIST), GNotifyItem_Select);
		}
		else LgiExitApp();
	}

	~AppWnd()
	{
	}
	
	int OnCommand(int Cmd, int Event, OsView Wnd)
	{
		switch (Cmd)
		{
			case IDM_SAVE_IMGS:
			{
				GdcPt2 PageSize(1000, 2000);
				
				GProgressDlg Prog(this, true);
				Prog.SetDescription("Scanning for HTML...");
				
				char p[MAX_PATH];
				LgiGetSystemPath(LSP_APP_INSTALL, p, sizeof(p));
				GArray<const char*> Ext;
				GArray<char*> Files;
				Ext.Add("*.html");
				Ext.Add("*.htm");				
				if (LgiRecursiveFileSearch(p, &Ext, &Files))
				{
					GDateTime Now;
					Now.SetNow();
					char OutPath[MAX_PATH], NowStr[32];
					sprintf_s(	NowStr, sizeof(NowStr),
								"%.4i-%.2i-%.2i_%.2i-%.2i-%.2i",
								Now.Year(), Now.Month(), Now.Day(),
								Now.Hours(), Now.Minutes(), Now.Seconds());
					LgiMakePath(OutPath, sizeof(OutPath), p, "Output");
					if (!DirExists(OutPath))
						FileDev->CreateFolder(OutPath);
					LgiMakePath(OutPath, sizeof(OutPath), OutPath, NowStr);
					if (!DirExists(OutPath))
						FileDev->CreateFolder(OutPath);
					
					Prog.SetDescription("Saving renders...");
					Prog.SetLimits(0, Files.Length());
					for (int i=0; i<Files.Length() && !Prog.Cancel(); i++)
					{
						char *File = Files[i];
						GAutoString Content(ReadTextFile(File));
						if (!Content)
						{
							LgiAssert(0);
							continue;
						}

						char *Leaf = strrchr(File, DIR_CHAR);
						if (!Leaf)
						{
							LgiAssert(0);
							continue;
						}
						Leaf++;
						char Png[256];
						strcpy_s(Png, sizeof(Png), Leaf);
						char *Ext = strrchr(Png, '.');
						if (Ext) *Ext = 0;
						strcat_s(Png, sizeof(Png), ".png");
						LgiMakePath(p, sizeof(p), OutPath, Png);

						Html1::GHtml Html(100, 0, 0, PageSize.x, PageSize.y, this);
						Html.Name(Content);
						GMemDC Screen(PageSize.x, PageSize.y, System24BitColourSpace);
						Screen.Colour(GColour(255, 255, 255));
						Screen.Rectangle();
						Html.OnPaint(&Screen);
						if (!GdcD->Save(p, &Screen))
							LgiAssert(0);
						
						Prog.Value(i);
						LgiYield();
					}
					Files.DeleteArrays();
				}
				break;
			}
			case IDM_COMPARE_IMAGES:
			{
				char p[MAX_PATH];
				LgiGetSystemPath(LSP_APP_INSTALL, p, sizeof(p));
				GArray<const char*> Ext;
				GArray<char*> Files;
				Ext.Add("*.png");
				// if (LgiRecursiveFileSearch(p, &Ext, &Files))

				char OutPath[MAX_PATH];
				LgiMakePath(OutPath, sizeof(OutPath), p, "Output");
				if (!DirExists(OutPath))
				{
					LgiMsg(this, "No output render folder.", "Html Test Suite");
					break;
				}
				
				new ImageCompareDlg(this, OutPath);
				break;
			}
		}
		
		return 0;
	}
	
	int OnNotify(GViewI *c, int f)
	{
		switch (c->GetId())
		{
			case IDC_LIST:
			{
				if (f == GNotifyItem_Select)
				{
					GListItem *s = Lst->GetSelected();
					if (s)
					{
						char p[256];

						LgiMakePath(p, sizeof(p), Base, s->GetText(0));
						if (FileExists(p))
						{
							char *h = ReadTextFile(p);
							if (h)
							{
								if (Html)
									Html->Name(h);
								DeleteArray(h);
							}
						}
					}
				}
				break;
			}
			case IDC_HTML:
			{
				switch (f)
				{
					case GNotifySelectionChanged:
					{
						if (Text)
						{
							GAutoString s(Html->GetSelection());
							if (s)
								Text->Name(s);
							else
								Text->Name("(null)");
						}
						break;
					}
				}
				break;
			}
		}
		
		return 0;
	}

	bool OnCompileScript(GDocView *Parent, char *Script, const char *Language, const char *MimeType)
	{
		// return Script->Compile(Code, true);
		return false;
	}

	bool OnExecuteScript(GDocView *Parent, char *Script)
	{
		return false; // Script->RunTemporary(Code);
	}
};

int LgiMain(OsAppArguments &AppArgs)
{
	GApp a(AppArgs, "HtmlTestSuite");
	if (a.IsOk())
	{
		a.AppWnd = new AppWnd;
		a.Run();
	}
	return 0;
}
