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

#if 1
#include "GParseCpp.h"
#endif

#include "resdefs.h"

#define DEBUG_FIND_SYMBOL		0
#define DEBUG_NO_THREAD			1

#ifdef _GPARSECPP_H_
#else
const char *CTagsExeName = "ctags"
	#ifdef WIN32
	".exe"
	#endif
	;
#define TEMP_FILE_NAME		"lgiide_ctags_filelist.txt"
#endif

class FindSymbolDlg : public GDialog
{
	AppWnd *App;
	GList *Lst;

public:
	FindSymResult Result;

	FindSymbolDlg(GViewI *parent);
	~FindSymbolDlg();
	
	int OnNotify(GViewI *v, int f);
	void OnCreate();
	bool OnViewKey(GView *v, GKey &k);
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
			bool Status = BuildDefnList(Path, Source, Defs, DefnNone);
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
		if (!Tf.Open(Path, O_READ))
			return false;

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

FindSymbolDlg::FindSymbolDlg(GViewI *parent)
{
	Lst = NULL;
	SetParent(parent);
	if (LoadFromResource(IDD_FIND_SYMBOL))
	{
		MoveToCenter();

		GViewI *f;
		if (GetViewById(IDC_STR, f))
			f->Focus(true);
		
		if (GetViewById(IDC_RESULTS, Lst))
		{
			Lst->MultiSelect(false);
			
			#ifdef _GPARSECPP_H_
			#else
			if (!d->CTagsExe)
			{
				GListItem *i = new GListItem;
				i->SetText("Ctags binary missing.");
				d->Lst->Insert(i);
			}
			#endif
		}
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

int FindSymbolDlg::OnNotify(GViewI *v, int f)
{
	switch (v->GetId())
	{
		case IDC_STR:
		{
			if (f != VK_RETURN)
			{
				char *Str = v->Name();
				if (Str && strlen(Str) > 1)
				{
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
					char *r = i->GetText(1);
					if (r)
					{
						char *colon = strrchr(r, ':');
						if (colon)
						{
							Result.File.Set(r, colon - r);
							Result.Line = atoi(++colon);
						}
					}
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
	FindSymbolDlg Dlg(Parent);
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
