#include <stdio.h>
#include <ctype.h>

#include "Lgi.h"
#include "LgiIde.h"
#include "FindSymbol.h"
#include "GList.h"
#include "GProcess.h"
#include "GToken.h"
#include "GEventTargetThread.h"
#include "GTextFile.h"
#include "SimpleCppParser.h"

#if 1
#include "GParseCpp.h"
#endif

#include "resdefs.h"

#define DEBUG_FIND_SYMBOL		0
#define DEBUG_NO_THREAD			1
#define DEBUG_FILE				"silabs_srate_program.h"

class FindSymbolDlg : public GDialog
{
	AppWnd *App;
	GList *Lst;
	FindSymbolSystem *Sys;

public:
	FindSymResult Result;

	FindSymbolDlg(GViewI *parent, FindSymbolSystem *sys);
	~FindSymbolDlg();
	
	int OnNotify(GViewI *v, int f);
	void OnCreate();
	bool OnViewKey(GView *v, GKey &k);
	GMessage::Result OnEvent(GMessage *m);
};


struct FindSymbolSystemPriv : public GEventTargetThread
{
	struct FileSyms
	{
		GString Path;
		GString::Array *Inc;
		GAutoWString Source;
		GArray<DefnInfo> Defs;
		
		bool Parse()
		{
			Defs.Length(0);
			
			bool Status = false;
			GString Src = Source.Get();
			char *Ext = LgiGetExtension(Path);
			if
			(
				Ext
				&&
				(
					!_stricmp(Ext, "c")
					||
					!_stricmp(Ext, "cpp")
					||
					!_stricmp(Ext, "h")
				)
			)
			{
				Status = BuildDefnList(Path, Source, Defs, DefnNone);
			}
			
			return Status;
		}
	};

	GEventSinkI *App;
	GArray<GString::Array*> IncPaths;
	GArray<FileSyms*> Files;
	
	FindSymbolSystemPriv(GEventSinkI *app) : GEventTargetThread("FindSymbolSystemPriv")
	{
		App = app;
	}
	
	int GetFileIndex(GString &Path)
	{
		for (unsigned i=0; i<Files.Length(); i++)
		{
			if (Path == Files[i]->Path)
				return i;
		}
		return -1;
	}
	
	bool AddFile(GString Path)
	{
		// Already added?
		int Idx = GetFileIndex(Path);

		bool Debug = false;
		#ifdef DEBUG_FILE
		if ((Debug = Path.Find(DEBUG_FILE) >= 0))
			printf("%s:%i - AddFile(%s) = %i\n", _FL, Path.Get(), Idx);
		#endif

		if (Idx >= 0)
			return true;
			
		LgiAssert(!LgiIsRelativePath(Path));

		// Setup the file sym data...
		FileSyms *f = new FileSyms;
		if (!f) return false;	
		f->Path = Path;
		f->Inc = IncPaths.Last();
		Files.Add(f);
		
		// Parse for headers...
		GTextFile Tf;
		if (!Tf.Open(Path, O_READ)  ||
			Tf.GetSize() < 4)
		{
			LgiTrace("%s:%i - Error: GTextFile.Open(%s) failed.\n", _FL, Path.Get());
			return false;
		}

		GAutoString Source = Tf.Read();
		GArray<char*> Headers;
		if (BuildHeaderList(Source, Headers, *f->Inc, false))
		{
			for (unsigned i=0; i<Headers.Length(); i++)
			{
				AddFile(Headers[i]);
			}
		}
		Headers.DeleteArrays();
		
		// Parse for symbols...
		f->Source.Reset(LgiNewUtf8To16(Source));

		#ifdef DEBUG_FILE
		if (Debug)
			printf("%s:%i - About to parse '%s' containing %i chars.\n", _FL, f->Path.Get(), StrlenW(f->Source));
		#endif

		return f->Parse();
	}
	
	bool ReparseFile(GString Path)
	{
		int Idx = GetFileIndex(Path);
		if (Idx < 0) return false;
		Files[Idx]->Parse();
		return true;
	}
	
	bool RemoveFile(GString Path)
	{
		int Idx = GetFileIndex(Path);
		if (Idx < 0) return false;
		delete Files[Idx];
		Files.DeleteAt(Idx);
		return true;
	}

	GMessage::Result OnEvent(GMessage *Msg)
	{
		switch (Msg->Msg())
		{
			case M_FIND_SYM_REQUEST:
			{
				GAutoPtr<FindSymRequest> Req((FindSymRequest*)Msg->A());
				if (Req && Req->Sink)
				{
					GString::Array p = Req->Str.SplitDelimit(" \t");
					if (p.Length() == 0)
						break;
					
					// For each file...
					for (unsigned f=0; f<Files.Length(); f++)
					{
						FileSyms *fs = Files[f];
						if (fs)
						{
							bool Debug = false;
							#ifdef DEBUG_FILE
							Debug = fs->Path.Find(DEBUG_FILE) >= 0;
							if (Debug)
								printf("%s:%i - Searching '%s' with %i syms...\n", _FL, fs->Path.Get(), fs->Defs.Length());
							#endif
							
							// For each symbol...
							for (unsigned i=0; i<fs->Defs.Length(); i++)
							{
								DefnInfo &Def = fs->Defs[i];
								
								// For each search term...
								bool Match = true;
								for (unsigned n=0; n<p.Length(); n++)
								{
									if (!stristr(Def.Name, p[n]))
									{
										Match = false;
										break;
									}
								}

								#ifdef DEBUG_FILE
								if (Debug)
									printf("%s:%i - '%s' = %i\n", _FL, Def.Name.Get(), Match);
								#endif
								
								if (Match)
								{
									// Create a result for this match...
									FindSymResult *r = new FindSymResult();
									if (r)
									{
										r->File = Def.File.Get();
										r->Symbol = Def.Name.Get();
										r->Line = Def.Line;
										Req->Results.Add(r);
									}
								}
							}
						}
					}
					
					Req->Sink->PostEvent(M_FIND_SYM_REQUEST, (GMessage::Param) Req.Release());
				}
				break;
			}
			case M_FIND_SYM_FILE:
			{
				GAutoPtr<GString> File((GString*)Msg->A());
				if (File)
				{
					FindSymbolSystem::SymAction Action = (FindSymbolSystem::SymAction)Msg->B();

					if (Action == FindSymbolSystem::FileAdd)
						AddFile(*File);
					else if (Action == FindSymbolSystem::FileRemove)
						RemoveFile(*File);
					else if (Action == FindSymbolSystem::FileReparse)
						ReparseFile(*File);
				}
				break;
			}
			case M_FIND_SYM_INC_PATHS:
			{
				GAutoPtr<GString::Array> Paths((GString::Array*)Msg->A());
				if (Paths)
					IncPaths.Add(Paths.Release());
				break;
			}
			default:
			{
				LgiAssert(!"Implement handler for message.");
				break;
			}
		}
		
		return 0;
	}	
};

int AlphaCmp(GListItem *a, GListItem *b, NativeInt d)
{
	return stricmp(a->GetText(0), b->GetText(0));
}

FindSymbolDlg::FindSymbolDlg(GViewI *parent, FindSymbolSystem *sys)
{
	Lst = NULL;
	Sys = sys;
	SetParent(parent);
	if (LoadFromResource(IDD_FIND_SYMBOL))
	{
		MoveToCenter();

		GViewI *f;
		if (GetViewById(IDC_STR, f))
			f->Focus(true);
		GetViewById(IDC_RESULTS, Lst);
	}
}

FindSymbolDlg::~FindSymbolDlg()
{
}

void FindSymbolDlg::OnCreate()
{
	RegisterHook(this, GKeyEvents);
}

bool FindSymbolDlg::OnViewKey(GView *v, GKey &k)
{
	switch (k.vkey)
	{
		case VK_UP:
		case VK_DOWN:
		case VK_PAGEDOWN:
		case VK_PAGEUP:
		{
			return Lst->OnKey(k);
			break;
		}
	}
	
	return false;
}

GMessage::Result FindSymbolDlg::OnEvent(GMessage *m)
{
	switch (m->Msg())
	{
		case M_FIND_SYM_REQUEST:
		{
			GAutoPtr<FindSymRequest> Req((FindSymRequest*)m->A());
			if (Req)
			{
				GString Str = GetCtrlName(IDC_STR);
				if (Str == Req->Str)
				{
					Lst->Empty();
					List<GListItem> Ls;

					GString s;
					int CommonPathLen = 0;					
					for (unsigned i=0; i<Req->Results.Length(); i++)
					{
						FindSymResult *r = Req->Results[i];

						if (i)
						{
							char *a = s.Get();
							char *a_end = strrchr(a, DIR_CHAR);

							char *b = r->File.Get();
							char *b_end = strrchr(b, DIR_CHAR);

							int Common = 0;
							while (	*a && a <= a_end
									&& 
									*b && b <= b_end
									&&
									ToLower(*a) == ToLower(*b))
							{
								Common++;
								a++;
								b++;
							}
							if (i == 1)
								CommonPathLen = Common;
							else
								CommonPathLen = min(CommonPathLen, Common);
						}
						else s = r->File;
					}

					for (unsigned i=0; i<Req->Results.Length(); i++)
					{
						FindSymResult *r = Req->Results[i];
						GListItem *it = new GListItem;
						if (it)
						{
							GString Ln;
							Ln.Printf("%i", r->Line);
							
							it->SetText(r->File.Get() + CommonPathLen, 0);
							it->SetText(Ln, 1);
							it->SetText(r->Symbol, 2);
							Ls.Insert(it);
						}
					}
					
					Lst->Insert(Ls);
					Lst->ResizeColumnsToContent();
				}
			}
			break;
		}
	}

	return GDialog::OnEvent(m);
}

int FindSymbolDlg::OnNotify(GViewI *v, int f)
{
	switch (v->GetId())
	{
		case IDC_STR:
		{
			if (f != VK_RETURN)
			{
				char *Str = v->Name();
				if (Str && strlen(Str) > 2)
				{
					// Create a search
					Sys->Search(this, Str);
				}
			}
			break;
		}
		case IDC_RESULTS:
		{
			if (f == GNotifyItem_DoubleClick)
			{
				// Fall throu
			}
			else break;
		}
		case IDOK:
		{
			if (Lst)
			{
				GListItem *i = Lst->GetSelected();
				if (i)
				{
					Result.File = i->GetText(0);
					Result.Line = atoi(i->GetText(1));
				}
			}
			
			EndModal(true);
			break;
		}
		case IDCANCEL:
		{
			EndModal(false);
			break;
		}
	}
	
	return GDialog::OnNotify(v, f);
}

///////////////////////////////////////////////////////////////////////////
FindSymbolSystem::FindSymbolSystem(GEventSinkI *app)
{
	d = new FindSymbolSystemPriv(app);
	d->App = app;
}

FindSymbolSystem::~FindSymbolSystem()
{
	delete d;
}

FindSymResult FindSymbolSystem::OpenSearchDlg(GViewI *Parent)
{
	FindSymbolDlg Dlg(Parent, this);
	Dlg.DoModal();
	return Dlg.Result;	
}

bool FindSymbolSystem::SetIncludePaths(GString::Array &Paths)
{
	GString::Array *a = new GString::Array;
	if (!a)
		return false;
	*a = Paths;
	return d->PostEvent(M_FIND_SYM_INC_PATHS, (GMessage::Param)a);
}

bool FindSymbolSystem::OnFile(const char *Path, SymAction Action)
{
	GString *s = new GString(Path);
	
	return d->PostEvent(M_FIND_SYM_FILE,
						(GMessage::Param)s,
						(GMessage::Param)Action);
}

void FindSymbolSystem::Search(GEventSinkI *Results, const char *SearchStr)
{
	FindSymRequest *Req = new FindSymRequest(Results);
	if (Req)
	{
		Req->Str = SearchStr;
		d->PostEvent(M_FIND_SYM_REQUEST, (GMessage::Param)Req);
	}
}
