#include <stdio.h>
#include <ctype.h>

#include "Lgi.h"
#include "LgiIde.h"
#include "GToken.h"
#include "INet.h"
#include "GClipBoard.h"
#include "GDisplayString.h"
#include "GScrollBar.h"
#include "LgiRes.h"
#include "GEdit.h"
#include "LList.h"
#include "GPopupList.h"
#include "GTableLayout.h"
#include "ProjectNode.h"
#include "GEventTargetThread.h"
#include "GCheckBox.h"
#include "SpaceTabConv.h"
#include "DocEdit.h"
#include "IdeDocPrivate.h"
#include "IHttp.h"

const char *Untitled = "[untitled]";
// static const char *White = " \r\t\n";

#define USE_OLD_FIND_DEFN	1
#define POPUP_WIDTH			700 // px
#define POPUP_HEIGHT		350 // px

enum
{
	IDM_COPY_FILE = 1100,
	IDM_COPY_PATH,
	IDM_BROWSE
};

int FileNameSorter(char **a, char **b)
{
	char *A = strrchr(*a, DIR_CHAR);
	char *B = strrchr(*b, DIR_CHAR);
	return stricmp(A?A:*a, B?B:*b);
}

EditTray::EditTray(GTextView3 *ctrl, IdeDoc *doc)
{
	Ctrl = ctrl;
	Doc = doc;
	Line = Col = 0;
	FuncBtn.ZOff(-1, -1);
	SymBtn.ZOff(-1, -1);
		
	int Ht = SysFont->GetHeight() + 6;
	AddView(FileSearch = new GEdit(IDC_FILE_SEARCH, 0, 0, EDIT_CTRL_WIDTH, Ht));
	AddView(FuncSearch = new GEdit(IDC_METHOD_SEARCH, 0, 0, EDIT_CTRL_WIDTH, Ht));
	AddView(SymSearch = new GEdit(IDC_SYMBOL_SEARCH, 0, 0, EDIT_CTRL_WIDTH, Ht));
	AddView(AllPlatforms = new GCheckBox(IDC_ALL_PLATFORMS, 0, 0, 20, Ht, "All Platforms"));
}
	
EditTray::~EditTray()
{
}
	
void EditTray::GotoSearch(int CtrlId, char *InitialText)
{
	if (FileSearch && FileSearch->GetId() == CtrlId)
	{
		FileSearch->Name(InitialText);
		FileSearch->Focus(true);
		if (InitialText)
			FileSearch->SendNotify(GNotifyDocChanged);
	}
		
	if (FuncSearch && FuncSearch->GetId() == CtrlId)
	{
		FuncSearch->Name(InitialText);
		FuncSearch->Focus(true);
		if (InitialText)
			FuncSearch->SendNotify(GNotifyDocChanged);
	}

	if (SymSearch && SymSearch->GetId() == CtrlId)
	{
		SymSearch->Name(InitialText);
		SymSearch->Focus(true);
		if (InitialText)
			SymSearch->SendNotify(GNotifyDocChanged);
	}
}
	
void EditTray::OnCreate()
{
	AttachChildren();
}
	
void EditTray::OnPosChange()
{
	GLayoutRect c(this, 2);

	c.Left(FileBtn, 20);
	if (FileSearch)
		c.Left(FileSearch, EDIT_CTRL_WIDTH);
	c.x1 += 8;

	#ifdef LINUX
	c.Left(FuncBtn, 28);
	#else
	c.Left(FuncBtn, 20);
	#endif
	if (FuncSearch)
		c.Left(FuncSearch, EDIT_CTRL_WIDTH);
	c.x1 += 8;

	c.Left(SymBtn, 20);
	if (SymSearch)
		c.Left(SymSearch, EDIT_CTRL_WIDTH);
	c.x1 += 8;

	if (AllPlatforms)
	{
		GViewLayoutInfo Inf;
		AllPlatforms->OnLayout(Inf);
		c.Left(AllPlatforms, Inf.Width.Max);
	}
		
	c.Remaining(TextMsg);
}
	
void EditTray::OnPaint(GSurface *pDC)
{
	GRect c = GetClient();
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle();
	SysFont->Colour(LC_TEXT, LC_MED);
	SysFont->Transparent(true);
		
	GString s;
	s.Printf("Cursor: %i,%i", Col, Line + 1);
	{
		GDisplayString ds(SysFont, s);
		ds.Draw(pDC, TextMsg.x1, TextMsg.y1 + ((c.Y()-TextMsg.Y())/2), &TextMsg);
	}

	GRect f = FileBtn;
	LgiThinBorder(pDC, f, DefaultRaisedEdge);
	{
		GDisplayString ds(SysFont, "h");
		ds.Draw(pDC, f.x1 + 6, f.y1);
	}

	f = FuncBtn;
	LgiThinBorder(pDC, f, DefaultRaisedEdge);
	{
		GDisplayString ds(SysFont,
			#ifdef LINUX
			"{}"
			#else
			"{ }"
			#endif
			);
		ds.Draw(pDC, f.x1 + 3, f.y1);
	}

	f = SymBtn;
	LgiThinBorder(pDC, f, DefaultRaisedEdge);
	{
		GDisplayString ds(SysFont, "s");
		ds.Draw(pDC, f.x1 + 6, f.y1);
	}
}

bool EditTray::Pour(GRegion &r)
{
	GRect *c = FindLargest(r);
	if (c)
	{
		GRect n = *c;
		SetPos(n);		
		return true;
	}
	return false;
}

void EditTray::OnHeaderList(GMouse &m)
{
	// Header list button
	GArray<GString> Paths;
	if (Doc->BuildIncludePaths(Paths, PlatformCurrent, false))
	{
		GArray<char*> Headers;
		if (Doc->BuildHeaderList(Ctrl->NameW(), Headers, Paths))
		{
			// Sort them..
			Headers.Sort(FileNameSorter);
			
			GSubMenu *s = new GSubMenu;
			if (s)
			{
				// Construct the menu
				LHashTbl<StrKey<char>, int> Map;
				int DisplayLines = GdcD->Y() / SysFont->GetHeight();
				if (Headers.Length() > (0.9 * DisplayLines))
				{
					GArray<char*> Letters[26];
					GArray<char*> Other;
					
					for (int i=0; i<Headers.Length(); i++)
					{
						char *h = Headers[i];
						char *f = LgiGetLeaf(h);
						
						Map.Add(h, i + 1);
						
						if (IsAlpha(*f))
						{
							int Idx = tolower(*f) - 'a';
							Letters[Idx].Add(h);
						}
						else
						{
							Other.Add(h);
						}
					}
					
					for (int i=0; i<CountOf(Letters); i++)
					{
						if (Letters[i].Length() > 1)
						{
							char *First = LgiGetLeaf(Letters[i][0]);
							char *Last = LgiGetLeaf(Letters[i].Last());

							char Title[256];
							sprintf_s(Title, sizeof(Title), "%s - %s", First, Last);
							GSubMenu *sub = s->AppendSub(Title);
							if (sub)
							{
								for (int n=0; n<Letters[i].Length(); n++)
								{
									char *h = Letters[i][n];
									int Id = Map.Find(h);
									LgiAssert(Id > 0);
									sub->AppendItem(LgiGetLeaf(h), Id, true);
								}
							}
						}
						else if (Letters[i].Length() == 1)
						{
							char *h = Letters[i][0];
							int Id = Map.Find(h);
							LgiAssert(Id > 0);
							s->AppendItem(LgiGetLeaf(h), Id, true);
						}
					}

					if (Other.Length() > 0)
					{
						for (int n=0; n<Other.Length(); n++)
						{
							char *h = Other[n];
							int Id = Map.Find(h);
							LgiAssert(Id > 0);
							s->AppendItem(LgiGetLeaf(h), Id, true);
						}
					}
				}
				else
				{
					for (int i=0; i<Headers.Length(); i++)
					{
						char *h = Headers[i];
						// char *f = LgiGetLeaf(h);
						
						Map.Add(h, i + 1);
					}

					for (int i=0; i<Headers.Length(); i++)
					{
						char *h = Headers[i];
						int Id = Map.Find(h);
						if (Id > 0)
							s->AppendItem(LgiGetLeaf(h), Id, true);
						else
							LgiTrace("%s:%i - Failed to get id for '%s' (map.len=%i)\n", _FL, h, Map.Length());
					}

					if (!Headers.Length())
					{
						s->AppendItem("(none)", 0, false);
					}
				}
				
				// Show the menu
				GdcPt2 p(m.x, m.y);
				PointToScreen(p);
				int Goto = s->Float(this, p.x, p.y, true);
				if (Goto > 0)
				{
					char *File = Headers[Goto-1];
					if (File)
					{
						// Open the selected file
						Doc->GetProject()->GetApp()->OpenFile(File);
					}
				}
				
				DeleteObj(s);
			}
		}
		
		// Clean up memory
		Headers.DeleteArrays();
	}
	else
	{
		LgiTrace("%s:%i - No include paths set.\n", _FL);
	}
}

void EditTray::OnFunctionList(GMouse &m)
{
	GArray<DefnInfo> Funcs;

	if (BuildDefnList(Doc->GetFileName(), Ctrl->NameW(), Funcs, DefnNone /*DefnFunc | DefnClass*/))
	{
		GSubMenu s;
		GArray<DefnInfo*> a;					
		
		int ScreenHt = GdcD->Y();
		int ScreenLines = ScreenHt / SysFont->GetHeight();
		float Ratio = ScreenHt ? (float)(SysFont->GetHeight() * Funcs.Length()) / ScreenHt : 0.0f;
		bool UseSubMenus = Ratio > 0.9f;
		int Buckets = UseSubMenus ? (int)(ScreenLines * 0.9) : 1;
		int BucketSize = MAX(2, Funcs.Length() / Buckets);
		GSubMenu *Cur = NULL;
		
		for (unsigned n=0; n<Funcs.Length(); n++)
		{
			DefnInfo *i = &Funcs[n];
			char Buf[256], *o = Buf;
			
			if (i->Type != DefnEnumValue)
			{
				for (char *k = i->Name; *k && o < Buf+sizeof(Buf)-8; k++)
				{
					if (*k == '&')
					{
						*o++ = '&';
						*o++ = '&';
					}
					else if (*k == '\t')
					{
						*o++ = ' ';
					}
					else
					{
						*o++ = *k;
					}
				}
				*o++ = 0;
				
				a[n] = i;

				if (UseSubMenus)
				{
					if (!Cur || n % BucketSize == 0)
					{
						GString SubMsg;
						SubMsg.Printf("%s...", Buf);
						Cur = s.AppendSub(SubMsg);
					}
					if (Cur)
						Cur->AppendItem(Buf, n+1, true);
				}
				else
				{
					s.AppendItem(Buf, n+1, true);
				}
			}
		}
		
		GdcPt2 p(m.x, m.y);
		PointToScreen(p);
		int Goto = s.Float(this, p.x, p.y, true);
		if (Goto)
		{
			DefnInfo *Info = a[Goto-1];
			if (Info)
			{
				Ctrl->SetLine(Info->Line);
			}
		}
	}
	else
	{
		LgiTrace("%s:%i - No functions in input.\n", _FL);
	}
}

void EditTray::OnSymbolList(GMouse &m)
{
	GAutoString s(Ctrl->GetSelection());
	if (s)
	{
		GAutoWString sw(Utf8ToWide(s));
		if (sw)
		{
			#if USE_OLD_FIND_DEFN
			List<DefnInfo> Matches;
			Doc->FindDefn(sw, Ctrl->NameW(), Matches);
			#else
			GArray<FindSymResult> Matches;
			Doc->GetApp()->FindSymbol(s, Matches);
			#endif

			GSubMenu *s = new GSubMenu;
			if (s)
			{
				// Construct the menu
				int n=1;
				
				#if USE_OLD_FIND_DEFN
				for (DefnInfo *Def = Matches.First(); Def; Def = Matches.Next())
				{
					char m[512];
					char *d = strrchr(Def->File, DIR_CHAR);
					sprintf(m, "%s (%s:%i)", Def->Name.Get(), d ? d + 1 : Def->File.Get(), Def->Line);
					s->AppendItem(m, n++, true);
				}
				#else
				for (int i=0; i<Matches.Length(); i++)
				{
					FindSymResult &Res = Matches[i];
					char m[512];
					char *d = strrchr(Res.File, DIR_CHAR);
					sprintf(m, "%s (%s:%i)", Res.Symbol.Get(), d ? d + 1 : Res.File.Get(), Res.Line);
					s->AppendItem(m, n++, true);
				}
				#endif

				if (!Matches.Length())
				{
					s->AppendItem("(none)", 0, false);
				}
				
				// Show the menu
				GdcPt2 p(m.x, m.y);
				PointToScreen(p);
				int Goto = s->Float(this, p.x, p.y, true);
				if (Goto)
				{
					#if USE_OLD_FIND_DEFN
					DefnInfo *Def = Matches[Goto-1];
					#else
					FindSymResult *Def = &Matches[Goto-1];
					#endif
					{
						// Open the selected symbol
						if (Doc->GetProject() &&
							Doc->GetProject()->GetApp())
						{
							AppWnd *App = Doc->GetProject()->GetApp();
							IdeDoc *Doc = App->OpenFile(Def->File);

							if (Doc)
							{
								Doc->SetLine(Def->Line, false);
							}
							else
							{
								char *f = Def->File;
								LgiTrace("%s:%i - Couldn't open doc '%s'\n", _FL, f);
							}
						}
						else
						{
							LgiTrace("%s:%i - No project / app ptr.\n", _FL);
						}
					}
				}
				
				DeleteObj(s);
			}
		}
	}
	else
	{
		GSubMenu *s = new GSubMenu;
		if (s)
		{
			s->AppendItem("(No symbol currently selected)", 0, false);
			GdcPt2 p(m.x, m.y);
			PointToScreen(p);
			s->Float(this, p.x, p.y, true);
			DeleteObj(s);
		}
	}
}

void EditTray::OnMouseClick(GMouse &m)
{
	if (m.Left() && m.Down())
	{
		if (FileBtn.Overlap(m.x, m.y))
		{
			OnHeaderList(m);
		}
		else if (FuncBtn.Overlap(m.x, m.y))
		{
			OnFunctionList(m);
		}
		else if (SymBtn.Overlap(m.x, m.y))
		{
			OnSymbolList(m);
		}
	}
}

class ProjMethodPopup : public GPopupList<DefnInfo>
{
	AppWnd *App;

public:
	GArray<DefnInfo> All;

	ProjMethodPopup(AppWnd *app, GViewI *target) : GPopupList(target, PopupAbove, POPUP_WIDTH)
	{
		App = app;
	}

	GString ToString(DefnInfo *Obj)
	{
		return GString(Obj->Name);
	}
	
	void OnSelect(DefnInfo *Obj)
	{
		App->GotoReference(Obj->File, Obj->Line, false);
	}
	
	bool Name(const char *s)
	{
		GString InputStr = s;
		GString::Array p = InputStr.SplitDelimit(" \t");
		
		GArray<DefnInfo*> Matching;
		for (unsigned i=0; i<All.Length(); i++)
		{
			DefnInfo *Def = &All[i];
			
			bool Match = true;
			for (unsigned n=0; n<p.Length(); n++)
			{
				if (!stristr(Def->Name, p[n]))
				{
					Match = false;
					break;
				}
			}
			
			if (Match)
				Matching.Add(Def);
		}
		
		return SetItems(Matching);
	}
	
	int OnNotify(GViewI *Ctrl, int Flags)
	{
		if (Lst &&
			Ctrl == Edit &&
			(!Flags || Flags == GNotifyDocChanged))
		{
			Name(Edit->Name());
		}
		
		return GPopupList<DefnInfo>::OnNotify(Ctrl, Flags);
	}
};

class ProjSymPopup : public GPopupList<FindSymResult>
{
	AppWnd *App;
	IdeDoc *Doc;
	int CommonPathLen;

public:
	GArray<FindSymResult*> All;

	ProjSymPopup(AppWnd *app, IdeDoc *doc, GViewI *target) : GPopupList(target, PopupAbove, POPUP_WIDTH)
	{
		App = app;
		Doc = doc;
		CommonPathLen = 0;
	}
	
	void FindCommonPathLength()
	{
		GString s;
		
		for (unsigned i=0; i<All.Length(); i++)
		{
			if (i)
			{
				char *a = s.Get();
				char *a_end = strrchr(a, DIR_CHAR);

				char *b = All[i]->File.Get();
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
					CommonPathLen = MIN(CommonPathLen, Common);
			}
			else s = All[i]->File;
		}
	}

	GString ToString(FindSymResult *Obj)
	{
		GString s;
		s.Printf("%s:%i - %s",
				CommonPathLen < Obj->File.Length()
				?
				Obj->File.Get() + CommonPathLen
				:
				Obj->File.Get(),
				Obj->Line,
				Obj->Symbol.Get());
		return s;
	}
	
	void OnSelect(FindSymResult *Obj)
	{
		App->GotoReference(Obj->File, Obj->Line, false);
	}
	
	int OnNotify(GViewI *Ctrl, int Flags)
	{
		if (Lst &&
			Ctrl == Edit &&
			(!Flags || Flags == GNotifyDocChanged))
		{
			// Kick off search...
			GString s = Ctrl->Name();
			int64 AllPlatforms = Ctrl->GetParent()->GetCtrlValue(IDC_ALL_PLATFORMS);
			s = s.Strip();
			if (s.Length() > 2)
				App->FindSymbol(Doc->AddDispatch(), s, AllPlatforms != 0);
		}
		
		return GPopupList<FindSymResult>::OnNotify(Ctrl, Flags);
	}
};

void FilterFiles(GArray<ProjectNode*> &Perfect, GArray<ProjectNode*> &Nodes, GString InputStr)
{
	GString::Array p = InputStr.SplitDelimit(" \t");
		
	int InputLen = InputStr.RFind(".");
	if (InputLen < 0)
		InputLen = InputStr.Length();

	GArray<ProjectNode*> Partial;
	for (unsigned i=0; i<Nodes.Length(); i++)
	{
		ProjectNode *Pn = Nodes[i];
		char *Fn = Pn->GetFileName();
		if (Fn)
		{
			char *Dir = strchr(Fn, '/');
			if (!Dir) Dir = strchr(Fn, '\\');
			char *Leaf = Dir ? strrchr(Fn, *Dir) : Fn;
				
			bool Match = true;
			for (unsigned n=0; n<p.Length(); n++)
			{
				if (!stristr(Leaf, p[n]))
				{
					Match = false;
					break;
				}
			}
			if (Match)
			{
				bool PerfectMatch = false;
				char *Leaf = LgiGetLeaf(Fn);
				if (Leaf)
				{
					char *Dot = strrchr(Leaf, '.');
					if (Dot)
					{
						int Len = Dot - Leaf;
						PerfectMatch =	Len == InputLen &&
										strncmp(InputStr, Leaf, Len) == 0;
					}
					else
					{
						PerfectMatch = stricmp(InputStr, Leaf);
					}
				}

				if (PerfectMatch)
					Perfect.Add(Pn);
				else
					Partial.Add(Pn);
			}
		}
	}

	Perfect.Add(Partial);
}

class ProjFilePopup : public GPopupList<ProjectNode>
{
	AppWnd *App;

public:
	GArray<ProjectNode*> Nodes;

	ProjFilePopup(AppWnd *app, GViewI *target) : GPopupList(target, PopupAbove, POPUP_WIDTH)
	{
		App = app;
	}

	GString ToString(ProjectNode *Obj)
	{
		return GString(Obj->GetFileName());
	}
	
	void OnSelect(ProjectNode *Obj)
	{
		char *Fn = Obj->GetFileName();
		if (LgiIsRelativePath(Fn))
		{
			IdeProject *Proj = Obj->GetProject();
			GAutoString Base = Proj->GetBasePath();
			GFile::Path p(Base);
			p += Fn;
			App->GotoReference(p, 1, false);			
		}
		else
		{
			App->GotoReference(Fn, 1, false);
		}
	}
	
	void Update(GString InputStr)
	{
		GArray<ProjectNode*> Matches;
		FilterFiles(Matches, Nodes, InputStr);
		SetItems(Matches);
	}
	
	int OnNotify(GViewI *Ctrl, int Flags)
	{
		if (Lst &&
			Ctrl == Edit &&
			(!Flags || Flags == GNotifyDocChanged))
		{
			char *s = Ctrl->Name();
			if (ValidStr(s))
				Update(s);
		}
		
		return GPopupList<ProjectNode>::OnNotify(Ctrl, Flags);
	}
};

class GStyleThread : public GEventTargetThread
{
public:
	GStyleThread() : GEventTargetThread("StyleThread")
	{
	}
	
	GMessage::Result OnEvent(GMessage *Msg)
	{
		switch (Msg->Msg())
		{
		}
		
		return 0;
	}	
}	StyleThread;

////////////////////////////////////////////////////////////////////////////////////////////
IdeDocPrivate::IdeDocPrivate(IdeDoc *d, AppWnd *a, NodeSource *src, const char *file) : NodeView(src), LMutex("IdeDocPrivate.Lock")
{
	IsDirty = false;

	FilePopup = NULL;
	MethodPopup = NULL;
	SymPopup = NULL;

	App = a;
	Doc = d;
	Project = 0;
	FileName = file;
	
	GFontType Font, *Use = 0;
	if (Font.Serialize(App->GetOptions(), OPT_EditorFont, false))
	{
		Use = &Font;
	}		
	
	Doc->AddView(Edit = new DocEdit(Doc, Use));
	Doc->AddView(Tray = new EditTray(Edit, Doc));
}

void IdeDocPrivate::OnDelete()
{
	IdeDoc *Temp = Doc;
	DeleteObj(Temp);
}

void IdeDocPrivate::UpdateName()
{
	char n[MAX_PATH+30];
	
	GString Dsp = GetDisplayName();
	char *File = Dsp;
	#if MDI_TAB_STYLE
	char *Dir = File ? strrchr(File, DIR_CHAR) : NULL;
	if (Dir) File = Dir + 1;
	#endif
	
	strcpy_s(n, sizeof(n), File ? File : Untitled);
	if (Edit->IsDirty())
	{
		strcat(n, " (changed)");
	}	
	Doc->Name(n);
}

GString IdeDocPrivate::GetDisplayName()
{
	if (nSrc)
	{
		char *Fn = nSrc->GetFileName();
		if (Fn)
		{
			if (stristr(Fn, "://"))
			{
				GUri u(nSrc->GetFileName());
				if (u.Pass)
				{
					DeleteArray(u.Pass);
					u.Pass = NewStr("******");
				}
				
				return u.GetUri().Release();
			}
			else if (*Fn == '.')
			{
				return nSrc->GetFullPath();
			}
		}

		return GString(Fn);
	}

	return GString(FileName);
}

bool IdeDocPrivate::IsFile(const char *File)
{
	GString Mem;
	char *f = NULL;
	
	if (nSrc)
	{
		Mem = nSrc->GetFullPath();
		f = Mem;
	}
	else
	{
		f = FileName;
	}

	if (!f)
		return false;

	GToken doc(f, DIR_STR);
	GToken in(File, DIR_STR);
	int in_pos = in.Length() - 1;
	int doc_pos = doc.Length() - 1;
	while (in_pos >= 0 && doc_pos >= 0)
	{
		char *i = in[in_pos--];
		char *d = doc[doc_pos--];
		if (!i || !d)
		{
			return false;
		}
		
		if (!strcmp(i, ".") ||
			!strcmp(i, ".."))
		{
			continue;
		}
		
		if (stricmp(i, d))
		{
			return false;
		}
	}
	
	return true;
}

char *IdeDocPrivate::GetLocalFile()
{
	if (nSrc)
	{
		if (nSrc->IsWeb())
			return nSrc->GetLocalCache();
		
		GString fp = nSrc->GetFullPath();
		if (_stricmp(fp.Get()?fp.Get():"", Buffer.Get()?Buffer.Get():""))
			Buffer = fp;
		return Buffer;
	}
	
	return FileName;
}

void IdeDocPrivate::SetFileName(const char *f)
{
	nSrc = NULL;
	FileName = f;
	Edit->IsDirty(true);
}

bool IdeDocPrivate::Load()
{
	bool Status = false;
	
	if (nSrc)
	{
		Status = nSrc->Load(Edit, this);
	}
	else if (FileName)
	{
		if (FileExists(FileName))
		{
			Status = Edit->Open(FileName);
		}
		else LgiTrace("%s:%i - '%s' doesn't exist.\n", _FL, FileName.Get());
	}

	if (Status)
		ModTs = GetModTime();	

	return Status;
}

bool IdeDocPrivate::Save()
{
	bool Status = false;
	
	if (nSrc)
	{
		Status = nSrc->Save(Edit, this);
	}
	else if (FileName)
	{
		Status = Edit->Save(FileName);
		if (!Status)
		{
			const char *Err = Edit->GetLastError();
			LgiMsg(App, "%s", AppName, MB_OK, Err ? Err : "$unknown_error");
		}
		
		OnSaveComplete(Status);
	}

	if (Status)
		ModTs = GetModTime();
	
	return Status;
}

void IdeDocPrivate::OnSaveComplete(bool Status)
{
	if (Status)
		IsDirty = false;
	UpdateName();

	ProjectNode *Node = dynamic_cast<ProjectNode*>(nSrc);
	if (Node)
	{
		GString Full = nSrc->GetFullPath();
		App->OnNode(Full, Node, FindSymbolSystem::FileReparse);
	}
}

void IdeDocPrivate::CheckModTime()
{
	if (!ModTs.IsValid())
		return;

	LDateTime Ts = GetModTime();
	if (Ts.IsValid() && Ts > ModTs)
	{
		static bool InCheckModTime = false;
		if (!InCheckModTime)
		{
			InCheckModTime = true;
			if (!IsDirty ||
				LgiMsg(Doc, "Do you want to reload modified file from\ndisk and lose your changes?", AppName, MB_YESNO) == IDYES)
			{
				int Ln = Edit->GetLine();
				Load();
				IsDirty = false;
				UpdateName();
				Edit->SetLine(Ln);
			}
			InCheckModTime = false;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////
GString IdeDoc::CurIpDoc;
int IdeDoc::CurIpLine = -1;

IdeDoc::IdeDoc(AppWnd *a, NodeSource *src, const char *file)
{
	d = new IdeDocPrivate(this, a, src, file);
	d->UpdateName();

	if (src || file)
		d->Load();
}

IdeDoc::~IdeDoc()
{
	d->App->OnDocDestroy(this);
	DeleteObj(d);
}

class WebBuild : public LThread
{
	IdeDocPrivate *d;
	GString Uri;
	int64 SleepMs;
	GStream *Log;
	LCancel Cancel;

public:
	WebBuild(IdeDocPrivate *priv, GString uri, int64 sleepMs) :
		LThread("WebBuild"), d(priv), Uri(uri), SleepMs(sleepMs)
	{
		Log = d->App->GetBuildLog();
		Run();
	}

	~WebBuild()
	{
		Cancel.Cancel();
		while (!IsExited())
		{
			LgiSleep(1);
		}
	}

	int Main()
	{
		if (SleepMs > 0)
		{
			// Sleep for a number of milliseconds to allow the file to upload/save to the website
			uint64 Ts = LgiCurrentTime();
			while (!Cancel.IsCancelled() && (LgiCurrentTime()-Ts) < SleepMs)
				LgiSleep(1);
		}

		// Download the file...
		GStringPipe Out;
		GString Error;
		bool r = LgiGetUri(&Cancel, &Out, &Error, Uri, NULL/*InHdrs*/, NULL/*Proxy*/);
		if (r)
		{
			// Parse through it and extract any errors...
		}
		else
		{
			// Show the download error in the build log...
			Log->Print("%s:%i - Web build download failed: %s\n", _FL, Error.Get());
		}

		return 0;
	}
};

bool IdeDoc::Build()
{
	if (!d->Edit)
		return false;

	int64 SleepMs = -1;
	GString s = d->Edit->Name(), Uri;
	GString::Array Lines = s.Split("\n");
	for (auto Ln : Lines)
	{
		s = Ln.Strip();
		if (s.Find("//") == 0)
		{
			GString::Array p = s(2,-1).Strip().Split(":", 1);
			if (p.Length() == 2)
			{
				if (p[0].Equals("build-sleep"))
				{
					SleepMs = p[1].Strip().Int();
				}
				else if (p[0].Equals("build-uri"))
				{
					Uri = p[1].Strip();
					break;
				}
			}
		}
	}

	if (Uri)
	{
		if (d->Build &&
			!d->Build->IsExited())
		{
			// Already building...
			GStream *Log = d->App->GetBuildLog();
			if (Log)
				Log->Print("%s:%i - Already building...\n");
			return false;
		}
		
		return d->Build.Reset(new WebBuild(d, Uri, SleepMs));
	}

	return false;
}

void IdeDoc::OnLineChange(int Line)
{
	d->App->OnLocationChange(d->GetLocalFile(), Line);
}

void IdeDoc::OnMarginClick(int Line)
{
	GString Dn = d->GetDisplayName();
	d->App->ToggleBreakpoint(Dn, Line);
}

void IdeDoc::OnTitleClick(GMouse &m)
{
	GMdiChild::OnTitleClick(m);
	
	if (m.IsContextMenu())
	{
		char Full[MAX_PATH] = "", sFile[MAX_PATH] = "", sFull[MAX_PATH] = "", sBrowse[MAX_PATH] = "";
		char *Fn = GetFileName(), *Dir = NULL;
		IdeProject *p = GetProject();
		if (Fn)
		{
			strcpy_s(Full, sizeof(Full), Fn);
			if (LgiIsRelativePath(Fn) && p)
			{
				GAutoString Base = p->GetBasePath();
				if (Base)
					LgiMakePath(Full, sizeof(Full), Base, Fn);
			}
			
			Dir = strrchr(Full, DIR_CHAR);
			if (Dir)
				sprintf_s(sFile, sizeof(sFile), "Copy '%s'", Dir + 1);
			sprintf_s(sFull, sizeof(sFull), "Copy '%s'", Full);
			sprintf_s(sBrowse, sizeof(sBrowse), "Browse to '%s'", Dir ? Dir + 1 : Full);			
		}
		
		GSubMenu s;
		s.AppendItem("Save", IDM_SAVE, d->IsDirty);
		s.AppendItem("Close", IDM_CLOSE, true);
		if (Fn)
		{
			s.AppendSeparator();
			if (Dir)
				s.AppendItem(sFile, IDM_COPY_FILE, true);
			s.AppendItem(sFull, IDM_COPY_PATH, true);
			s.AppendItem(sBrowse, IDM_BROWSE, true);
			s.AppendItem("Show In Project", IDM_SHOW_IN_PROJECT, true);
		}
		if (p)
		{
			s.AppendSeparator();
			s.AppendItem("Properties", IDM_PROPERTIES, true);
		}
		
		m.ToScreen();
		int Cmd = s.Float(this, m.x, m.y, m.Left());
		switch (Cmd)
		{
			case IDM_SAVE:
			{
				SetClean();
				break;
			}
			case IDM_CLOSE:
			{
				if (OnRequestClose(false))
					Quit();
				break;
			}
			case IDM_COPY_FILE:
			{
				if (Dir)
				{
					GClipBoard c(this);
					c.Text(Dir + 1);
				}
				break;
			}
			case IDM_COPY_PATH:
			{
				GClipBoard c(this);
				c.Text(Full);
				break;
			}
			case IDM_BROWSE:
			{
				#if defined(WIN32)
				char Args[MAX_PATH];
				sprintf(Args, "/e,/select,\"%s\"", Full);
				LgiExecute("explorer", Args);
				#elif defined(LINUX)
				GString Args = LgiGetAppForMimeType("inode/directory");
				if (Args)
				{
					LgiExecute(Args, Full);
				}
				else LgiMsg(this, "Failed to find file browser.", AppName);
				#else
				LgiAssert(!"Impl me.");
				#endif
				break;
			}
			case IDM_SHOW_IN_PROJECT:
			{
				d->App->ShowInProject(Fn);
				break;
			}
			case IDM_PROPERTIES:
			{
				p->ShowFileProperties(Full);
				break;
			}
		}
	}
}

AppWnd *IdeDoc::GetApp()
{
	return d->App;
}

bool IdeDoc::IsFile(const char *File)
{
	return File ? d->IsFile(File) : false;
}

bool IdeDoc::AddBreakPoint(int Line, bool Add)
{
	if (Add)
		d->BreakPoints.Add(Line, true);
	else
		d->BreakPoints.Delete(Line);
	
	if (d->Edit)
		d->Edit->Invalidate();

	return true;
}


void IdeDoc::GotoSearch(int CtrlId, char *InitialText)
{
	GString File;

	if (CtrlId == IDC_SYMBOL_SEARCH)
	{
		// Check if the cursor is on a #include line... in which case we
		// should look up the header and go to that instead of looking for
		// a symbol in the code.
		if (d->Edit)
		{
			// Get current line
			GString Ln = (*d->Edit)[d->Edit->GetLine()];
			if (Ln.Find("#include") >= 0)
			{
				GString::Array a = Ln.SplitDelimit(" \t", 1);
				if (a.Length() == 2)
				{
					File = a[1].Strip("\'\"<>");
					InitialText = File;
					CtrlId = IDC_FILE_SEARCH;
				}
			}			
		}
	}

	if (d->Tray)
		d->Tray->GotoSearch(CtrlId, InitialText);
}

#define IsVariableChar(ch) \
	( \
		IsAlpha(ch) \
		|| \
		IsDigit(ch) \
		|| \
		strchr("-_", ch) != NULL \
	)

void IdeDoc::SearchSymbol()
{
	if (!d->Edit || !d->Tray)
	{
		LgiAssert(0);
		return;
	}
	
	int Cur = d->Edit->GetCaret();
	char16 *Txt = d->Edit->NameW();
	if (Cur >= 0 &&
		Txt != NULL)
	{
		int Start = Cur;
		while (	Start > 0 &&
				IsVariableChar(Txt[Start-1]))
			Start--;
		int End = Cur;
		while (	Txt[End] &&
				IsVariableChar(Txt[End]))
			End++;
		GString Word(Txt + Start, End - Start);
		GotoSearch(IDC_SYMBOL_SEARCH, Word);
	}
}

void IdeDoc::UpdateControl()
{
	if (d->Edit)
		d->Edit->Invalidate();
}

void IdeDoc::SearchFile()
{
	GotoSearch(IDC_FILE_SEARCH, NULL);
}

bool IdeDoc::IsCurrentIp()
{
	char *Fn = GetFileName();
	bool DocMatch = CurIpDoc &&
					Fn &&
					!_stricmp(Fn, CurIpDoc);
	return DocMatch;
}

void IdeDoc::ClearCurrentIp()
{
	CurIpDoc.Empty();
	CurIpLine = -1;
}

void IdeDoc::SetCrLf(bool CrLf)
{
	if (d->Edit)
		d->Edit->SetCrLf(CrLf);
}

bool IdeDoc::OpenFile(const char *File)
{
	if (d->Edit)
	{
		return d->Edit->Open(File);
	}

	return false;
}

void IdeDoc::SetEditorParams(int IndentSize, int TabSize, bool HardTabs, bool ShowWhiteSpace)
{
	if (d->Edit)
	{
		d->Edit->SetIndentSize(IndentSize > 0 ? IndentSize : 4);
		d->Edit->SetTabSize(TabSize > 0 ? TabSize : 4);
		d->Edit->SetHardTabs(HardTabs);
		d->Edit->SetShowWhiteSpace(ShowWhiteSpace);
		d->Edit->Invalidate();
	}
}

bool IdeDoc::HasFocus(int Set)
{
	if (!d->Edit)
		return false;

	if (Set)
		d->Edit->Focus(Set);

	return d->Edit->Focus();
}

void IdeDoc::ConvertWhiteSpace(bool ToTabs)
{
	if (!d->Edit)
		return;

	GAutoString Sp(
		ToTabs ?
		SpacesToTabs(d->Edit->Name(), d->Edit->GetTabSize()) :
		TabsToSpaces(d->Edit->Name(), d->Edit->GetTabSize())
		);
	if (Sp)
	{
		d->Edit->Name(Sp);
		SetDirty();
	}
}

int IdeDoc::GetLine()
{
	return d->Edit ? d->Edit->GetLine() : -1;
}

void IdeDoc::SetLine(int Line, bool CurIp)
{
	if (CurIp)
	{
		GString CurDoc = GetFileName();
		
		if (ValidStr(CurIpDoc) ^ ValidStr(CurDoc)
			||
			(CurIpDoc && CurDoc && strcmp(CurDoc, CurIpDoc) != 0)
			||
			Line != CurIpLine)
		{
			bool Cur = IsCurrentIp();
			if (d->Edit && Cur && CurIpLine >= 0)
			{
				// Invalidate the old IP location
				d->Edit->InvalidateLine(CurIpLine - 1);
			}
			
			CurIpLine = Line;
			CurIpDoc = CurDoc;
			
			// LgiTrace("%s:%i - CurIpLine=%i\n", _FL, CurIpLine);
			d->Edit->InvalidateLine(CurIpLine - 1);
		}
	}
	
	if (d->Edit)
	{
		d->Edit->SetLine(Line);
	}
}

IdeProject *IdeDoc::GetProject()
{
	return d->Project;
}

void IdeDoc::SetProject(IdeProject *p)
{
	d->Project = p;
	
	if (d->Project->GetApp() &&
		d->BreakPoints.Length() == 0)
		d->Project->GetApp()->LoadBreakPoints(this);
}

char *IdeDoc::GetFileName()
{
	return d->GetLocalFile();
}

void IdeDoc::SetFileName(const char *f, bool Write)
{
	d->SetFileName(f);
	if (Write)
		d->Edit->Save(d->GetLocalFile());
}

void IdeDoc::Focus(bool f)
{
	d->Edit->Focus(f);
}

GMessage::Result IdeDoc::OnEvent(GMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_FIND_SYM_REQUEST:
		{
			GAutoPtr<FindSymRequest> Resp((FindSymRequest*)Msg->A());
			if (Resp && d->SymPopup)
			{
				GViewI *SymEd;
				if (GetViewById(IDC_SYMBOL_SEARCH, SymEd))
				{
					GString Input = SymEd->Name();
					if (Input == Resp->Str) // Is the input string still the same?
					{
						/*
						The problem with this is that the user it still typing something
						and the cursor / focus jumps to the document now they are typing
						a search string into the document, which is not what they intended.
						
						if (Resp->Results.Length() == 1)
						{
							FindSymResult *r = Resp->Results[0];
							d->SymPopup->Visible(false);

							d->App->GotoReference(r->File, r->Line, false);
						}
						else
						*/
						{
							d->SymPopup->All = Resp->Results;
							Resp->Results.Length(0);
							d->SymPopup->FindCommonPathLength();
							d->SymPopup->SetItems(d->SymPopup->All);
							d->SymPopup->Visible(true);
						}
					}
				}
			}
			break;
		}
	}
	
	return GMdiChild::OnEvent(Msg);
}

void IdeDoc::OnPulse()
{
	d->CheckModTime();

	if (d->Lock(_FL))
	{
		if (d->WriteBuf.Length())
		{
			bool Pour = d->Edit->SetPourEnabled(false);
			for (GString *s = NULL; d->WriteBuf.Iterate(s); )
			{
				GAutoWString w(Utf8ToWide(*s, s->Length()));
				d->Edit->Insert(d->Edit->GetSize(), w, Strlen(w.Get()));
			}
			d->Edit->SetPourEnabled(Pour);

			d->WriteBuf.Empty();
			d->Edit->Invalidate();
		}
		d->Unlock();
	}
}

GString IdeDoc::Read()
{
	return d->Edit->Name();
}

ssize_t IdeDoc::Write(const void *Ptr, ssize_t Size, int Flags)
{
	if (d->Lock(_FL))
	{
		d->WriteBuf.New().Set((char*)Ptr, Size);
		d->Unlock();
	}
	return 0;
}

void IdeDoc::OnProjectChange()
{
	DeleteObj(d->FilePopup);
	DeleteObj(d->MethodPopup);
	DeleteObj(d->SymPopup);
}

int IdeDoc::OnNotify(GViewI *v, int f)
{
	// printf("IdeDoc::OnNotify(%i, %i)\n", v->GetId(), f);
	
	switch (v->GetId())
	{
		case IDC_EDIT:
		{
			switch (f)
			{
				case GNotifyDocChanged:
				{
					if (d->IsDirty ^ d->Edit->IsDirty())
					{
						d->IsDirty = d->Edit->IsDirty();
						d->UpdateName();
					}
					break;
				}
				case GNotifyCursorChanged:
				{
					if (d->Tray)
					{
						GdcPt2 Pt;
						if (d->Edit->GetLineColumnAtIndex(Pt, d->Edit->GetCaret()))
						{
							d->Tray->Col = Pt.x;
							d->Tray->Line = Pt.y;
							d->Tray->Invalidate();
						}
					}
					break;
				}
			}
			break;
		}
		case IDC_FILE_SEARCH:
		{
			if (f == GNotify_EscapeKey)
			{
				d->Edit->Focus(true);
				break;
			}
			
			char *SearchStr = v->Name();
			if (ValidStr(SearchStr))
			{
				if (!d->FilePopup)
				{
					if ((d->FilePopup = new ProjFilePopup(d->App, v)))
					{
						// Populate with files from the project...
						
						// Find the root project...
						IdeProject *p = d->Project;
						while (p && p->GetParentProject())
							p = p->GetParentProject();
						
						if (p)
						{
							// Get all the nodes
							List<IdeProject> All;
							p->GetChildProjects(All);
							All.Insert(p);							
							for (IdeProject *p=All.First(); p; p=All.Next())
							{
								p->GetAllNodes(d->FilePopup->Nodes);
							}
						}
					}
				}
				if (d->FilePopup)
				{
					// Update list elements...
					d->FilePopup->OnNotify(v, f);
				}
			}
			else if (d->FilePopup)
			{
				DeleteObj(d->FilePopup);
			}
			break;
		}
		case IDC_METHOD_SEARCH:
		{
			if (f == GNotify_EscapeKey)
			{
				printf("%s:%i Got GNotify_EscapeKey\n", _FL);
				d->Edit->Focus(true);
				break;
			}
			
			char *SearchStr = v->Name();
			if (ValidStr(SearchStr))
			{
				if (!d->MethodPopup)
					d->MethodPopup = new ProjMethodPopup(d->App, v);
				if (d->MethodPopup)
				{
					// Populate with symbols
					d->MethodPopup->All.Length(0);
					BuildDefnList(GetFileName(), d->Edit->NameW(), d->MethodPopup->All, DefnFunc);

					// Update list elements...
					d->MethodPopup->OnNotify(v, f);
				}
			}
			else if (d->MethodPopup)
			{
				DeleteObj(d->MethodPopup);
			}
			break;
		}
		case IDC_SYMBOL_SEARCH:
		{
			if (f == GNotify_EscapeKey)
			{
				printf("%s:%i Got GNotify_EscapeKey\n", _FL);
				d->Edit->Focus(true);
				break;
			}
			
			char *SearchStr = v->Name();
			if (ValidStr(SearchStr))
			{
				if (!d->SymPopup)
					d->SymPopup = new ProjSymPopup(d->App, this, v);
				if (d->SymPopup)
					d->SymPopup->OnNotify(v, f);
			}
			else if (d->SymPopup)
			{
				DeleteObj(d->SymPopup);
			}
			break;
		}
	}
	
	return 0;
}

void IdeDoc::SetDirty()
{
	d->IsDirty = true;
	d->Edit->IsDirty(true);
	d->UpdateName();
}

bool IdeDoc::SetClean()
{
	static bool Processing = false;
	bool Status = false;

	if (!Processing)
	{
		Status = Processing = true;

		GAutoString Base;
		if (GetProject())
			Base = GetProject()->GetBasePath();
		
		GString LocalPath;
		if (d->GetLocalFile() &&
			LgiIsRelativePath(LocalPath) &&
			Base)
		{
			char p[MAX_PATH];
			LgiMakePath(p, sizeof(p), Base, d->GetLocalFile());
			LocalPath = p;
		}
		else
		{
			LocalPath = d->GetLocalFile();
		}

		if (!FileExists(LocalPath))
		{
			// We need a valid filename to save to...			
			GFileSelect s;
			s.Parent(this);
			if (Base)
				s.InitialDir(Base);
			
			if (s.Save())
			{
				d->SetFileName(s.Name());
			}
		}
		
		if (d->Edit->IsDirty())
		{
			d->Save();
		}
		
		Processing = false;
	}
	
	return Status;
}

void IdeDoc::OnPaint(GSurface *pDC)
{
	GMdiChild::OnPaint(pDC);
	
	#if !MDI_TAB_STYLE
	GRect c = GetClient();
	LgiWideBorder(pDC, c, SUNKEN);
	#endif
}

void IdeDoc::OnPosChange()
{
	GMdiChild::OnPosChange();
}

bool IdeDoc::OnRequestClose(bool OsShuttingDown)
{
	if (d->Edit->IsDirty())
	{
		GString Dsp = d->GetDisplayName();
		int a = LgiMsg(this, "Save '%s'?", AppName, MB_YESNOCANCEL, Dsp ? Dsp.Get() : Untitled);
		switch (a)
		{
			case IDYES:
			{
				if (!SetClean())
				{				
					return false;
				}
				break;
			}
			case IDNO:
			{
				break;
			}
			default:
			case IDCANCEL:
			{
				return false;
			}
		}
	}
	
	return true;
}

/*
GTextView3 *IdeDoc::GetEdit()
{
	return d->Edit;
}
*/

bool IdeDoc::BuildIncludePaths(GArray<GString> &Paths, IdePlatform Platform, bool IncludeSysPaths)
{
	if (!GetProject())
	{
		LgiTrace("%s:%i - GetProject failed.\n", _FL);
		return false;
	}

	bool Status = GetProject()->BuildIncludePaths(Paths, true, IncludeSysPaths, Platform);
	if (Status)
	{
		if (IncludeSysPaths)
			GetApp()->GetSystemIncludePaths(Paths);
	}
	else
	{
		LgiTrace("%s:%i - GetProject()->BuildIncludePaths failed.\n", _FL);
	}
	
	return Status;
}

bool IdeDoc::BuildHeaderList(char16 *Cpp, GArray<char*> &Headers, GArray<GString> &IncPaths)
{
	GAutoString c8(WideToUtf8(Cpp));
	if (!c8)
		return false;

	return ::BuildHeaderList(c8, Headers, IncPaths, true);
}

bool MatchSymbol(DefnInfo *Def, char16 *Symbol)
{
	static char16 Dots[] = {':', ':', 0};

	GBase o;
	o.Name(Def->Name);
	char16 *Name = o.NameW();

	char16 *Sep = StristrW(Name, Dots);
	char16 *Start = Sep ? Sep : Name;
	// char16 *End = StrchrW(Start, '(');
	int Len = StrlenW(Symbol);
	char16 *Match = StristrW(Start, Symbol);

	if (Match) // && Match + Len <= End)
	{
		if (Match > Start && isword(Match[-1]))
		{
			return false;
		}
		
		char16 *e = Match + Len;
		if (*e && isword(*e))
		{
			return false;
		}
		
		return true;
	}
	
	return false;
}

bool IdeDoc::FindDefn(char16 *Symbol, char16 *Source, List<DefnInfo> &Matches)
{
	if (!Symbol || !Source)
	{
		LgiTrace("%s:%i - Arg error.\n", _FL);
		return false;
	}

	#if DEBUG_FIND_DEFN
	GStringPipe Dbg;
	LgiTrace("FindDefn(%S)\n", Symbol);
	#endif

	GString::Array Paths;
	GArray<char*> Headers;

	if (!BuildIncludePaths(Paths, PlatformCurrent, true))
	{
		LgiTrace("%s:%i - BuildIncludePaths failed.\n", _FL);
		// return false;
	}

	char Local[MAX_PATH];
	strcpy_s(Local, sizeof(Local), GetFileName());
	LgiTrimDir(Local);
	Paths.New() = Local;

	if (!BuildHeaderList(Source, Headers, Paths))
	{
		LgiTrace("%s:%i - BuildHeaderList failed.\n", _FL);
		// return false;
	}

	{
		GArray<DefnInfo> Defns;
		
		for (int i=0; i<Headers.Length(); i++)
		{
			char *h = Headers[i];
			char *c8 = ReadTextFile(h);
			if (c8)
			{
				char16 *c16 = Utf8ToWide(c8);
				DeleteArray(c8);
				if (c16)
				{
					GArray<DefnInfo> Defns;
					if (BuildDefnList(h, c16, Defns, DefnNone, false ))
					{
						bool Found = false;
						for (unsigned n=0; n<Defns.Length(); n++)
						{
							DefnInfo &Def = Defns[n];
							if (MatchSymbol(&Def, Symbol))
							{
								Matches.Insert(new DefnInfo(Def));
								Found = true;
							}
						}
						
						#if DEBUG_FIND_DEFN
						if (!Found)
							Dbg.Print("Not in '%s'\n", h);
						#endif
					}
					
					DeleteArray(c16);
				}
			}
		}

		char *FileName = GetFileName();
		if (BuildDefnList(FileName, Source, Defns, DefnNone))
		{
			#if DEBUG_FIND_DEFN
			bool Found = false;
			#endif
			
			for (unsigned i=0; i<Defns.Length(); i++)
			{
				DefnInfo &Def = Defns[i];
				if (MatchSymbol(&Def, Symbol))
					Matches.Insert(new DefnInfo(Def));
			}

			#if DEBUG_FIND_DEFN
			if (!Found)
				Dbg.Print("Not in current file.\n");
			#endif
		}
	}
	
	Headers.DeleteArrays();

	#if DEBUG_FIND_DEFN
	{
		GAutoString a(Dbg.NewStr());
		if (a) LgiTrace("%s", a.Get());
	}
	#endif
	
	return Matches.Length() > 0;
}
