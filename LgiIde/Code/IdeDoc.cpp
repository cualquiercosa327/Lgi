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

const char *Untitled = "[untitled]";
// static const char *White = " \r\t\n";

#define USE_OLD_FIND_DEFN	1
#define POPUP_WIDTH			700 // px
#define POPUP_HEIGHT		350 // px

#define EDIT_TRAY_HEIGHT	(SysFont->GetHeight() + 10)
#define EDIT_LEFT_MARGIN	16 // gutter for debug break points
#define EDIT_CTRL_WIDTH		200

#define IsSymbolChar(ch)	(IsAlpha(ch) || (ch) == '_')

#define ColourComment		GColour(0, 140, 0)
#define ColourHashDef		GColour(0, 0, 222)
#define ColourLiteral		GColour(192, 0, 0)
#define ColourKeyword		GColour::Black
#define ColourType			GColour(0, 0, 222)
#define ColourPhp			GColour(140, 140, 180)
#define ColourHtml			GColour(80, 80, 255)

enum SourceType
{
	SrcUnknown,
	SrcPlainText,
	SrcCpp,
	SrcPython,
	SrcXml,
	SrcHtml,
};

struct LanguageParams
{
	const char **Keywords;
	const char **Types;
};

const char *DefaultKeywords[] = {"if", "elseif", "endif", "else", "ifeq", "ifdef", "ifndef", "ifneq", "include", NULL};
const char *CppKeywords[] = {"extern", "class", "struct", "static", "default", "case", "break",
							"switch", "new", "delete", "sizeof", "return", "enum", "else",
							"if", "for", "while", "do", "continue", "public", "virtual", 
							"protected", "friend", "union", "template", "typedef", "dynamic_cast",
							NULL};
const char *CppTypes[] = {	"int", "char", "unsigned", "double", "float", "bool", "const", "void",
							"int8", "int16", "int32", "int64",
							"uint8", "uint16", "uint32", "uint64",
							"GArray", "GHashTbl", "List", "GString", "GAutoString", "GAutoWString",
							"GAutoPtr",
							NULL};
const char *PythonKeywords[] = {"def", "try", "except", "import", "if", "for", "elif", "else", NULL};
const char *XmlTypes[] = {	NULL};

LanguageParams LangParam[] =
{
	// Unknown
	{NULL, NULL},
	// Plain text
	{DefaultKeywords, NULL},
	// C/C++
	{CppKeywords, CppTypes},
	// Python
	{PythonKeywords, NULL},
	// Xml
	{NULL, XmlTypes},
	// Html/Php
	{NULL, NULL}
};

GAutoPtr<GDocFindReplaceParams> GlobalFindReplace;

int FileNameSorter(char **a, char **b)
{
	char *A = strrchr(*a, DIR_CHAR);
	char *B = strrchr(*b, DIR_CHAR);
	return stricmp(A?A:*a, B?B:*b);
}

class EditTray : public GLayout
{
	GRect FileBtn;
	GEdit *FileSearch;

	GRect FuncBtn;
	GEdit *FuncSearch;

	GRect SymBtn;
	GEdit *SymSearch;

	GCheckBox *AllPlatforms;

	GRect TextMsg;

	GTextView3 *Ctrl;
	IdeDoc *Doc;

public:
	int Line, Col;

	EditTray(GTextView3 *ctrl, IdeDoc *doc)
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
	
	~EditTray()
	{
	}
	
	void GotoSearch(int CtrlId, char *InitialText = NULL)
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
	
	void OnCreate()
	{
		AttachChildren();
	}
	
	void OnPosChange()
	{
		GLayoutRect c(this, 2);

		c.Left(FileBtn, 20);
		if (FileSearch)
			c.Left(FileSearch, EDIT_CTRL_WIDTH);
		c.x1 += 8;

		c.Left(FuncBtn, 20);
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
	
	void OnPaint(GSurface *pDC)
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
			GDisplayString ds(SysFont, "{ }");
			ds.Draw(pDC, f.x1 + 3, f.y1);
		}

		f = SymBtn;
		LgiThinBorder(pDC, f, DefaultRaisedEdge);
		{
			GDisplayString ds(SysFont, "s");
			ds.Draw(pDC, f.x1 + 6, f.y1);
		}
	}

	bool Pour(GRegion &r)
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
	
	void OnMouseClick(GMouse &m);

	void OnHeaderList(GMouse &m);
	void OnFunctionList(GMouse &m);
	void OnSymbolList(GMouse &m);
};

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
				GHashTbl<char*, int> Map;
				int DisplayLines = GdcD->Y() / SysFont->GetHeight();
				if (Headers.Length() > (0.7 * DisplayLines))
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
		int Buckets = UseSubMenus ? (int)(ScreenLines * 0.75) : 1;
		int BucketSize = MAX(5, Funcs.Length() / Buckets);
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

class IdeDocPrivate : public NodeView
{
	GString FileName;
	GString Buffer;

public:
	IdeDoc *Doc;
	AppWnd *App;
	IdeProject *Project;
	bool IsDirty;
	LDateTime ModTs;
	class DocEdit *Edit;
	EditTray *Tray;
	GHashTbl<int, bool> BreakPoints;
	class ProjFilePopup *FilePopup;
	class ProjMethodPopup *MethodPopup;
	class ProjSymPopup *SymPopup;
	
	IdeDocPrivate(IdeDoc *d, AppWnd *a, NodeSource *src, const char *file);
	void OnDelete();
	void UpdateName();
	GString GetDisplayName();
	bool IsFile(const char *File);
	char *GetLocalFile();
	void SetFileName(const char *f);
	bool Load();
	bool Save();
	void OnSaveComplete(bool Status);

	LDateTime GetModTime()
	{
		LDateTime Ts;

		GString Full = nSrc ? nSrc->GetFullPath() : FileName;
		if (Full)
		{
			GDirectory Dir;
			if (Dir.First(Full, NULL))
				Ts.Set(Dir.GetLastWriteTime());
		}

		return Ts;
	}

	void CheckModTime();
};

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

class DocEdit : public GTextView3, public GDocumentEnv
{
	IdeDoc *Doc;
	int CurLine;
	GdcPt2 MsClick;
	SourceType FileType;

	struct Keyword
	{
		char16 *Word;
		int Len;
		bool IsType;
		Keyword *Next;

		Keyword(const char *w, bool istype = false)
		{
			Word = Utf8ToWide(w);
			Len = Strlen(Word);
			IsType = istype;
			Next = NULL;
		}

		~Keyword()
		{
			delete Next;
			delete [] Word;
		}
	};

	Keyword *HasKeyword[26];

public:
	static int LeftMarginPx;

	DocEdit(IdeDoc *d, GFontType *f) : GTextView3(IDC_EDIT, 0, 0, 100, 100, f)
	{
		FileType = SrcUnknown;
		ZeroObj(HasKeyword);
		Doc = d;
		CurLine = -1;
		if (!GlobalFindReplace)
		{
			GlobalFindReplace.Reset(CreateFindReplaceParams());
		}
		SetFindReplaceParams(GlobalFindReplace);
		
		CanScrollX = true;
		GetCss(true)->PaddingLeft(GCss::Len(GCss::LenPx, (float)(LeftMarginPx + 2)));
		
		if (!f)
		{
			GFontType Type;
			if (Type.GetSystemFont("Fixed"))
			{
				GFont *f = Type.Create();
				if (f)
				{
					#if defined LINUX
					f->PointSize(9);
					#elif defined WIN32
					f->PointSize(8);
					#endif
					SetFont(f);
				}
			}
		}
		
		SetWrapType(TEXTED_WRAP_NONE);
		SetEnv(this);
	}
	
	#define IDM_FILE_COMMENT			100
	#define IDM_FUNC_COMMENT			101
	bool AppendItems(GSubMenu *Menu, int Base) override
	{
		GSubMenu *Insert = Menu->AppendSub("Insert...");
		if (Insert)
		{
			Insert->AppendItem("File Comment", IDM_FILE_COMMENT, Doc->GetProject() != 0);
			Insert->AppendItem("Function Comment", IDM_FUNC_COMMENT, Doc->GetProject() != 0);
		}

		return true;
	}
	
	~DocEdit()
	{
		SetEnv(0);
		for (int i=0; i<CountOf(HasKeyword); i++)
		{
			DeleteObj(HasKeyword[i]);
		}
	}

	bool DoGoto() override
	{
		GInput Dlg(this, "", LgiLoadString(L_TEXTCTRL_GOTO_LINE, "Goto [file:]line:"), "Text");
		if (Dlg.DoModal() != IDOK || !ValidStr(Dlg.Str))
			return false;

		GString s = Dlg.Str.Get();
		GString::Array p = s.SplitDelimit(":,");
		if (p.Length() == 2)
		{
			GString file = p[0];
			int line = (int)p[1].Int();
			Doc->GetApp()->GotoReference(file, line, false, true);
		}
		else if (p.Length() == 1)
		{
			int line = (int)p[0].Int();
			if (line > 0)
				SetLine(line);
			else
				// Probably a filename with no line number..
				Doc->GetApp()->GotoReference(p[0], 1, false, true);				
		}
		
		return true;
	}

	int GetTopPaddingPx()
	{
		return GetCss(true)->PaddingTop().ToPx(GetClient().Y(), GetFont());
	}

	void InvalidateLine(int Idx)
	{
		GTextLine *Ln = GTextView3::Line[Idx];
		if (Ln)
		{
			int PadPx = GetTopPaddingPx();
			GRect r = Ln->r;
			r.Offset(0, -ScrollYPixel() + PadPx);
			// LgiTrace("%s:%i - r=%s\n", _FL, r.GetStr());
			Invalidate(&r);
		}
	}
	
	void OnPaintLeftMargin(GSurface *pDC, GRect &r, GColour &colour) override
	{
		GColour GutterColour(0xfa, 0xfa, 0xfa);
		GTextView3::OnPaintLeftMargin(pDC, r, GutterColour);
		int Y = ScrollYLine();
		
		int TopPaddingPx = GetTopPaddingPx();

		pDC->Colour(GColour(200, 0, 0));
		List<GTextLine>::I it = GTextView3::Line.Start(Y);
		int DocOffset = (*it)->r.y1;
		for (GTextLine *l = *it; l; l = *++it, Y++)
		{
			if (Doc->d->BreakPoints.Find(Y+1))
			{
				int r = l->r.Y() >> 1;
				pDC->FilledCircle(8, l->r.y1 + r + TopPaddingPx - DocOffset, r - 1);
			}
		}

		bool DocMatch = Doc->IsCurrentIp();
		{
			// We have the current IP location
			it = GTextView3::Line.Start();
			int Idx = 1;
			for (GTextLine *ln = *it; ln; ln = *++it, Idx++)
			{
				if (DocMatch && Idx == IdeDoc::CurIpLine)
				{
					ln->Back.Set(LC_DEBUG_CURRENT_LINE, 24);
				}
				else
				{
					ln->Back.Empty();
				}
			}
		}
	}

	void OnMouseClick(GMouse &m) override
	{
		if (m.Down())
		{
			if (HasSelection())
			{
				MsClick.x = -100;
				MsClick.y = -100;
			}
			else
			{
				MsClick.x = m.x;
				MsClick.y = m.y;
			}
		}
		else if
		(
			m.x < LeftMarginPx &&
			abs(m.x - MsClick.x) < 5 &&
			abs(m.y - MsClick.y) < 5
		)
		{
			// Margin click... work out the line
			int Y = (VScroll) ? (int)VScroll->Value() : 0;
			GFont *f = GetFont();
			if (!f) return;
			GCss::Len PaddingTop = GetCss(true)->PaddingTop();
			int TopPx = PaddingTop.ToPx(GetClient().Y(), f);
			int Idx = ((m.y - TopPx) / f->GetHeight()) + Y + 1;
			if (Idx > 0 && Idx <= GTextView3::Line.Length())
			{
				Doc->OnMarginClick(Idx);
			}
		}

		GTextView3::OnMouseClick(m);
	}

	void SetCaret(size_t i, bool Select, bool ForceFullUpdate = false) override
	{
		GTextView3::SetCaret(i, Select, ForceFullUpdate);
		
		if (IsAttached())
		{
			int Line = GetLine();
			if (Line != CurLine)
			{
				Doc->OnLineChange(CurLine = Line);
			}
		}
	}
	
	bool OnMenu(GDocView *View, int Id, void *Context) override;
	bool OnKey(GKey &k) override;
	
	char *TemplateMerge(const char *Template, char *Name, List<char> *Params)
	{
		// Parse template and insert into doc
		GStringPipe T;
		for (const char *t = Template; *t; )
		{
			char *e = strstr((char*) t, "<%");
			if (e)
			{
				// Push text before tag
				T.Push(t, e-t);
				char *TagStart = e;
				e += 2;
				skipws(e);
				char *Start = e;
				while (*e && isalpha(*e)) e++;
				
				// Get tag
				char *Tag = NewStr(Start, e-Start);
				if (Tag)
				{
					// Process tag
					if (Name && stricmp(Tag, "name") == 0)
					{
						T.Push(Name);
					}
					else if (Params && stricmp(Tag, "params") == 0)
					{
						char *Line = TagStart;
						while (Line > Template && Line[-1] != '\n') Line--;
						
						int i = 0;
						for (char *p=Params->First(); p; p=Params->Next(), i++)
						{
							if (i) T.Push(Line, TagStart-Line);
							T.Push(p);
							if (i < Params->Length()-1) T.Push("\n");
						}
					}
					
					DeleteArray(Tag);
				}
				
				e = strstr(e, "%>");
				if (e)
				{
					t = e + 2;
				}
				else break;
			}
			else
			{
				T.Push(t);
				break;
			}
		}
		T.Push("\n");
		return T.NewStr();
	}

	#define COMP_STYLE	1

	bool GetVisible(GStyle &s)
	{
		GRect c = GetClient();
		int a = HitText(c.x1, c.y1, false);
		int b = HitText(c.x2, c.y2, false);
		s.Start = a;
		s.Len = b - a + 1;
		return true;
	}

	void StyleString(char16 *&s, char16 *e)
	{
		GAutoPtr<GStyle> st(new GTextView3::GStyle(STYLE_IDE));
		if (st)
		{
			st->View = this;
			st->Start = s - Text;
			st->Font = GetFont();

			char16 Delim = *s++;
			while (s < e && *s != Delim)
			{
				if (*s == '\\')
					s++;
				s++;
			}
			st->Len = (s - Text) - st->Start + 1;
			st->Fore = ColourLiteral;
			InsertStyle(st);
		}
	}

	void StyleCpp(ssize_t Start, ssize_t EditSize)
	{
		if (!Text)
			return;

		char16 *e = Text + Size;
		
		// uint64 StartTs = LgiMicroTime();
		#if COMP_STYLE
		List<GStyle> OldStyle;
		OldStyle = Style;
		Style.Empty();
		#else
		Style.DeleteObjects();
		#endif
		// uint64 SetupTs = LgiMicroTime();
		
		for (char16 *s = Text; s < e; s++)
		{
			switch (*s)
			{
				case '\"':
				case '\'':
					StyleString(s, e);
					break;
				case '#':
				{
					// Check that the '#' is the first non-whitespace on the line
					bool IsWhite = true;
					for (char16 *w = s - 1; w >= Text && *w != '\n'; w--)
					{
						if (!IsWhiteSpace(*w))
						{
							IsWhite = false;
							break;
						}
					}

					if (IsWhite)
					{
						GAutoPtr<GStyle> st(new GTextView3::GStyle(STYLE_IDE));
						if (st)
						{
							st->View = this;
							st->Start = s - Text;
							st->Font = GetFont();
							
							char LastNonWhite = 0;
							while (s < e)
							{
								if
								(
									// Break at end of line
									(*s == '\n' && LastNonWhite != '\\')
									||
									// Or the start of a comment
									(*s == '/' && s[1] == '/')
									||
									(*s == '/' && s[1] == '*')
								)
									break;
								if (!IsWhiteSpace(*s))
									LastNonWhite = *s;
								s++;
							}
							
							st->Len = (s - Text) - st->Start;
							st->Fore = ColourHashDef;
							InsertStyle(st);
							s--;
						}
					}
					break;
				}
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				{
					if (s == Text || !IsSymbolChar(s[-1]))
					{
						GAutoPtr<GStyle> st(new GTextView3::GStyle(STYLE_IDE));
						if (st)
						{
							st->View = this;
							st->Start = s - Text;
							st->Font = GetFont();

							bool IsHex = false;
							if (s[0] == '0' &&
								ToLower(s[1]) == 'x')
							{
								s += 2;
								IsHex = true;
							}
							
							while (s < e)
							{
								if
								(
									IsDigit(*s)
									||
									*s == '.'
									||
									(
										IsHex
										&&
										(
											(*s >= 'a' && *s <= 'f')
											||
											(*s >= 'A' && *s <= 'F')
										)
									)
								)
									s++;
								else
									break;
							}
							
							st->Len = (s - Text) - st->Start;
							st->Fore = ColourLiteral;
							InsertStyle(st);
							s--;
						}
					}
					while (s < e - 1 && IsDigit(s[1]))
						s++;
					break;
				}
				case '/':
				{
					if (s[1] == '/')
					{
						GAutoPtr<GStyle> st(new GTextView3::GStyle(STYLE_IDE));
						if (st)
						{
							st->View = this;
							st->Start = s - Text;
							st->Font = GetFont();
							while (s < e && *s != '\n')
								s++;
							st->Len = (s - Text) - st->Start;
							st->Fore = ColourComment;
							InsertStyle(st);
							s--;
						}
					}
					else if (s[1] == '*')
					{
						GAutoPtr<GStyle> st(new GTextView3::GStyle(STYLE_IDE));
						if (st)
						{
							st->View = this;
							st->Start = s - Text;
							st->Font = GetFont();
							s += 2;
							while (s < e && !(s[-2] == '*' && s[-1] == '/'))
								s++;
							st->Len = (s - Text) - st->Start;
							st->Fore = ColourComment;
							InsertStyle(st);
							s--;
						}
					}
					break;
				}
				case '-':
				{
					const wchar_t *t = L"-DVERSION=\\\"3.0_ColdFire_FlexCAN\\\"";
					if (Strnicmp(s, t, Strlen(t)) == 0)
					{
						int asd=0;
					}
				}
				default:
				{
					wchar_t Ch = ToLower(*s);
					if (Ch >= 'a' && Ch <= 'z')
					{
						Keyword *k;
						if ((k = HasKeyword[Ch - 'a']))
						{
							do
							{
								if (!StrnicmpW(k->Word, s, k->Len))
									break;
							}
							while ((k = k->Next));

							if
							(
								k
								&&
								(s == Text || !IsSymbolChar(s[-1]))
								&&
								!IsSymbolChar(s[k->Len])
							)
							{
								GAutoPtr<GStyle> st(new GTextView3::GStyle(STYLE_IDE));
								if (st)
								{
									st->View = this;
									st->Start = s - Text;
									st->Font = k->IsType ? Font : Bold;
									st->Len = k->Len;
									st->Fore = k->IsType ? ColourType : ColourKeyword;
									InsertStyle(st);
									s += k->Len - 1;
								}
							}
						}
					}
				}
			}
		}

		// uint64 PourTs = LgiMicroTime();

		#if COMP_STYLE
		GStyle Vis(STYLE_NONE);
		if (GetVisible(Vis))
		{
			GArray<GStyle*> Old, Cur;
			for (GStyle *s = OldStyle.First(); s; s = OldStyle.Next())
			{
				if (s->Overlap(&Vis))
					Old.Add(s);
				else if (Old.Length())
					break;
			}
			for (GStyle *s = Style.First(); s; s = Style.Next())
			{
				if (s->Overlap(&Vis))
					Cur.Add(s);
				else if (Cur.Length())
					break;
			}

			GStyle Dirty(STYLE_NONE);
			for (int o=0; o<Old.Length(); o++)
			{
				bool Match = false;
				GStyle *OldStyle = Old[o];
				for (int n=0; n<Cur.Length(); n++)
				{
					if (*OldStyle == *Cur[n])
					{
						Old.DeleteAt(o--);
						Cur.DeleteAt(n--);
						Match = true;
						break;
					}
				}
				if (!Match)
					Dirty.Union(*OldStyle);
			}
			for (int n=0; n<Cur.Length(); n++)
			{
				Dirty.Union(*Cur[n]);
			}

			if (Dirty.Start >= 0)
			{
				// LgiTrace("Visible rgn: %i + %i = %i\n", Vis.Start, Vis.Len, Vis.End());
				// LgiTrace("Dirty rgn: %i + %i = %i\n", Dirty.Start, Dirty.Len, Dirty.End());

				int CurLine = -1, DirtyStartLine = -1, DirtyEndLine = -1;
				GetTextLine(Cursor, &CurLine);
				GTextLine *Start = GetTextLine(Dirty.Start, &DirtyStartLine);
				GTextLine *End = GetTextLine(MIN(Size, Dirty.End()), &DirtyEndLine);
				if (CurLine >= 0 &&
					DirtyStartLine >= 0 &&
					DirtyEndLine >= 0)
				{
					// LgiTrace("Dirty lines %i, %i, %i\n", CurLine, DirtyStartLine, DirtyEndLine);
					
					if (DirtyStartLine != CurLine ||
						DirtyEndLine != CurLine)
					{
						GRect c = GetClient();
						GRect r(c.x1,
								Start->r.Valid() ? DocToScreen(Start->r).y1 : c.y1,
								c.x2,
								Dirty.End() >= Vis.End() ? c.y2 : DocToScreen(End->r).y2);
						
						// LgiTrace("Cli: %s, CursorLine: %s, Start rgn: %s, End rgn: %s, Update: %s\n", c.GetStr(), CursorLine->r.GetStr(), Start->r.GetStr(), End->r.GetStr(), r.GetStr());

						Invalidate(&r);
					}						
				}
				else
				{
					// LgiTrace("No Change: %i, %i, %i\n", CurLine, DirtyStartLine, DirtyEndLine);
				}
			}
		}
		OldStyle.DeleteObjects();
		#endif

		// uint64 DirtyTs = LgiMicroTime();		
		// LgiTrace("PourCpp = %g, %g\n", (double)(PourTs - SetupTs) / 1000.0, (double)(DirtyTs - PourTs) / 1000.0);
	}

	void StylePython(ssize_t Start, ssize_t EditSize)
	{
		char16 *e = Text + Size;
		
		Style.DeleteObjects();
		for (char16 *s = Text; s < e; s++)
		{
			switch (*s)
			{
				case '\"':
				case '\'':
					StyleString(s, e);
					break;
				case '#':
				{
					// Single line comment
					GAutoPtr<GStyle> st(new GTextView3::GStyle(STYLE_IDE));
					if (st)
					{
						st->View = this;
						st->Start = s - Text;
						st->Font = GetFont();
						while (s < e && *s != '\n')
							s++;
						st->Len = (s - Text) - st->Start;
						st->Fore = ColourComment;
						InsertStyle(st);
						s--;
					}
					break;
				}
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				{
					if (s == Text || !IsSymbolChar(s[-1]))
					{
						GAutoPtr<GStyle> st(new GTextView3::GStyle(STYLE_IDE));
						if (st)
						{
							st->View = this;
							st->Start = s - Text;
							st->Font = GetFont();

							bool IsHex = false;
							if (s[0] == '0' &&
								ToLower(s[1]) == 'x')
							{
								s += 2;
								IsHex = true;
							}
							
							while (s < e)
							{
								if
								(
									IsDigit(*s)
									||
									*s == '.'
									||
									(
										IsHex
										&&
										(
											(*s >= 'a' && *s <= 'f')
											||
											(*s >= 'A' && *s <= 'F')
										)
									)
								)
									s++;
								else
									break;
							}
							
							st->Len = (s - Text) - st->Start;
							st->Fore = ColourLiteral;
							InsertStyle(st);
							s--;
						}
					}
					while (s < e - 1 && IsDigit(s[1]))
						s++;
					break;
				}
				default:
				{
					if (*s >= 'a' && *s <= 'z')
					{
						Keyword *k;
						if ((k = HasKeyword[*s - 'a']))
						{
							do
							{
								if (!Strncmp(k->Word, s, k->Len))
									break;
							}
							while ((k = k->Next));

							if
							(
								k
								&&
								(s == Text || !IsSymbolChar(s[-1]))
								&&
								!IsSymbolChar(s[k->Len])
							)
							{
								GAutoPtr<GStyle> st(new GTextView3::GStyle(STYLE_IDE));
								if (st)
								{
									st->View = this;
									st->Start = s - Text;
									st->Font = Bold;
									st->Len = k->Len;
									st->Fore = ColourKeyword;
									InsertStyle(st);
									s += k->Len - 1;
								}
							}
						}
					}
					break;
				}
			}
		}
	}

	void StyleDefault(ssize_t Start, ssize_t EditSize)
	{
		char16 *e = Text + Size;
		
		Style.DeleteObjects();
		for (char16 *s = Text; s < e; s++)
		{
			switch (*s)
			{
				case '\"':
				case '\'':
				case '`':
				{
					GAutoPtr<GStyle> st(new GTextView3::GStyle(STYLE_IDE));
					if (st)
					{
						bool Quoted = s > Text && s[-1] == '\\';

						st->View = this;
						st->Start = s - Text - Quoted;
						st->Font = GetFont();

						char16 Delim = *s++;
						while
						(
							s < e
							&&
							*s != Delim
							&&
							!(Delim == '`' && *s == '\'')
						)
						{
							if (*s == '\\')
							{
								if (!Quoted || s[1] != Delim)
									s++;
							}
							else if (*s == '\n')
								break;
							s++;
						}
						st->Len = (s - Text) - st->Start + 1;
						st->Fore = ColourLiteral;
						InsertStyle(st);
					}
					break;
				}
				case '#':
				{
					// Single line comment
					GAutoPtr<GStyle> st(new GTextView3::GStyle(STYLE_IDE));
					if (st)
					{
						st->View = this;
						st->Start = s - Text;
						st->Font = GetFont();
						while (s < e && *s != '\n')
							s++;
						st->Len = (s - Text) - st->Start;
						st->Fore = ColourComment;
						InsertStyle(st);
						s--;
					}
					break;
				}
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				{
					if (s == Text || !IsSymbolChar(s[-1]))
					{
						GAutoPtr<GStyle> st(new GTextView3::GStyle(STYLE_IDE));
						if (st)
						{
							st->View = this;
							st->Start = s - Text - ((s > Text && strchr("-+", s[-1])) ? 1 : 0);
							st->Font = GetFont();

							bool IsHex = false;
							if (s[0] == '0' &&
								ToLower(s[1]) == 'x')
							{
								s += 2;
								IsHex = true;
							}
							
							while (s < e)
							{
								if
								(
									IsDigit(*s)
									||
									*s == '.'
									||
									(
										IsHex
										&&
										(
											(*s >= 'a' && *s <= 'f')
											||
											(*s >= 'A' && *s <= 'F')
										)
									)
								)
									s++;
								else
									break;
							}
							if (*s == '%')
								s++;
							
							st->Len = (s - Text) - st->Start;
							st->Fore = ColourLiteral;
							InsertStyle(st);
							s--;
						}
					}
					while (s < e - 1 && IsDigit(s[1]))
						s++;
					break;
				}
				default:
				{
					if (*s >= 'a' && *s <= 'z')
					{
						Keyword *k;
						if ((k = HasKeyword[*s - 'a']))
						{
							do
							{
								if (!Strncmp(k->Word, s, k->Len) &&
									!IsSymbolChar(s[k->Len]))
									break;
							}
							while ((k = k->Next));

							if
							(
								k
								&&
								(s == Text || !IsSymbolChar(s[-1]))
								&&
								!IsSymbolChar(s[k->Len])
							)
							{
								GAutoPtr<GStyle> st(new GTextView3::GStyle(STYLE_IDE));
								if (st)
								{
									st->View = this;
									st->Start = s - Text;
									st->Font = Bold;
									st->Len = k->Len;
									st->Fore = ColourKeyword;
									InsertStyle(st);
									s += k->Len - 1;
								}
							}
						}
					}
					break;
				}
			}
		}
	}

	void StyleXml(ssize_t Start, ssize_t EditSize)
	{
		char16 *e = Text + Size;
		
		Style.DeleteObjects();
		for (char16 *s = Text; s < e; s++)
		{
			switch (*s)
			{
				case '\"':
				case '\'':
					StyleString(s, e);
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				{
					if (s == Text || !IsSymbolChar(s[-1]))
					{
						GAutoPtr<GStyle> st(new GTextView3::GStyle(STYLE_IDE));
						if (st)
						{
							st->View = this;
							st->Start = s - Text;
							st->Font = GetFont();

							bool IsHex = false;
							if (s[0] == '0' &&
								ToLower(s[1]) == 'x')
							{
								s += 2;
								IsHex = true;
							}
							
							while (s < e)
							{
								if
								(
									IsDigit(*s)
									||
									*s == '.'
									||
									(
										IsHex
										&&
										(
											(*s >= 'a' && *s <= 'f')
											||
											(*s >= 'A' && *s <= 'F')
										)
									)
								)
									s++;
								else
									break;
							}
							
							st->Len = (s - Text) - st->Start;
							st->Fore = ColourLiteral;
							InsertStyle(st);
							s--;
						}
					}
					while (s < e - 1 && IsDigit(s[1]))
						s++;
					break;
				}
				case '<':
				{
					if (s[1] == '!' &&
						s[2] == '-' &&
						s[3] == '-')
					{
						GAutoPtr<GStyle> st(new GTextView3::GStyle(STYLE_IDE));
						if (st)
						{
							st->View = this;
							st->Start = s - Text;
							st->Font = GetFont();
							s += 2;
							while
							(
								s < e
								&&
								!
								(
									s[-3] == '-'
									&&
									s[-2] == '-'
									&&
									s[-1] == '>'
								)
							)
								s++;
							st->Len = (s - Text) - st->Start;
							st->Fore = ColourComment;
							InsertStyle(st);
							s--;
						}
					}
					break;
				}
				default:
				{
					if (*s >= 'a' && *s <= 'z')
					{
						Keyword *k;
						if ((k = HasKeyword[*s - 'a']))
						{
							do
							{
								if (!Strncmp(k->Word, s, k->Len))
									break;
							}
							while ((k = k->Next));

							if
							(
								k
								&&
								(s == Text || !IsSymbolChar(s[-1]))
								&&
								!IsSymbolChar(s[k->Len])
							)
							{
								GAutoPtr<GStyle> st(new GTextView3::GStyle(STYLE_IDE));
								if (st)
								{
									st->View = this;
									st->Start = s - Text;
									st->Font = Bold;
									st->Len = k->Len;
									st->Fore = ColourKeyword;
									InsertStyle(st);
									s += k->Len - 1;
								}
							}
						}
					}
					break;
				}
			}
		}
	}

	enum HtmlType
	{
		CodeHtml,
		CodePhp,
		CodeCss,
	};

	void StyleHtml(ssize_t Start, ssize_t EditSize)
	{
		char16 *e = Text + Size;

		Style.DeleteObjects();

		HtmlType Type = CodeHtml;
		GAutoPtr<GStyle> Php;

		#define START_CODE() \
			if (Type == CodePhp) \
			{ \
				if (Php.Reset(new GTextView3::GStyle(STYLE_IDE))) \
				{ \
					Php->View = this; \
					Php->Start = s - Text; \
					Php->Font = GetFont(); \
					Php->Fore = ColourPhp; \
				} \
			}
		#define END_CODE() \
			if (Php) \
			{ \
				Php->Len = (s - Text) - Php->Start; \
				InsertStyle(Php); \
			}

		for (char16 *s = Text; s < e; s++)
		{
			switch (*s)
			{
				case '\"':
				case '\'':
				{
					END_CODE();
					StyleString(s, e);
					s++;
					START_CODE();
					s--;
					break;
				}
				case '/':
				{
					if (Type != CodeHtml &&
						s[1] == '/')
					{
						END_CODE();

						char16 *nl = Strchr(s, '\n');
						if (!nl) nl = s + Strlen(s);
						
						GAutoPtr<GStyle> st;
						if (st.Reset(new GTextView3::GStyle(STYLE_IDE)))
						{
							st->View = this;
							st->Start = s - Text;
							st->Font = GetFont();
							st->Fore = ColourComment;
							st->Len = nl - s;
							InsertStyle(st);
						}

						s = nl;
						START_CODE();
					}
					break;
				}
				case '<':
				{
					if (Type == CodeHtml)
					{
						if (s[1] == '?' &&
							s[2] == 'p' &&
							s[3] == 'h' &&
							s[4] == 'p')
						{
							Type = CodePhp;
							START_CODE();
							s += 4;
						}
						else
						{
							char16 *tag = s + 1;
							while (tag < e && strchr(WhiteSpace, *tag)) tag++;
							while (tag < e && (IsAlpha(*tag) || strchr("!_/0123456789", *tag))) tag++;

							GAutoPtr<GStyle> st;
							if (st.Reset(new GTextView3::GStyle(STYLE_IDE)))
							{
								st->View = this;
								st->Start = s - Text;
								st->Font = GetFont();
								st->Fore = ColourHtml;
								st->Len = tag - s;
								InsertStyle(st);
								s = tag - 1;
							}
						}
					}
					break;
				}
				case '>':
				{
					if (Type == CodeHtml)
					{
						GAutoPtr<GStyle> st;
						if (st.Reset(new GTextView3::GStyle(STYLE_IDE)))
						{
							st->View = this;
							st->Start = s - Text;
							st->Font = GetFont();
							st->Fore = ColourHtml;
							st->Len = 1;
							InsertStyle(st);
						}
					}
					break;
				}
				case '?':
				{
					if (Type == CodePhp &&
						s[1] == '>')
					{
						Type = CodeHtml;
						s += 2;
						END_CODE();
						s--;
					}
					break;
				}
			}
		}
	}
	
	void AddKeywords(const char **keys, bool IsType)
	{
		for (const char **k = keys; *k; k++)
		{
			const char *Word = *k;
			int idx = ToLower(*Word)-'a';
			LgiAssert(idx >= 0 && idx < CountOf(HasKeyword));
			
			Keyword **Ptr = &HasKeyword[idx];
			while (*Ptr != NULL)
				Ptr = &(*Ptr)->Next;
			*Ptr = new Keyword(Word, IsType);
		}
	}

	void PourStyle(size_t Start, ssize_t EditSize) override
	{
		if (FileType == SrcUnknown)
		{
			char *Ext = LgiGetExtension(Doc->GetFileName());
			if (!Ext)
				FileType = SrcPlainText;
			else if (!stricmp(Ext, "c") ||
					!stricmp(Ext, "cpp") ||
					!stricmp(Ext, "cc") ||
					!stricmp(Ext, "h") ||
					!stricmp(Ext, "hpp") )
				FileType = SrcCpp;
			else if (!stricmp(Ext, "py"))
				FileType = SrcPython;
			else if (!stricmp(Ext, "xml"))
				FileType = SrcXml;
			else if (!stricmp(Ext, "html") ||
					!stricmp(Ext, "htm") ||
					!stricmp(Ext, "php"))
				FileType = SrcHtml;
			else
				FileType = SrcPlainText;

			ZeroObj(HasKeyword);
			if (LangParam[FileType].Keywords)
				AddKeywords(LangParam[FileType].Keywords, false);
			if (LangParam[FileType].Types)
				AddKeywords(LangParam[FileType].Types, true);
		}

		switch (FileType)
		{
			case SrcCpp:
				StyleCpp(Start, EditSize);
				break;
			case SrcPython:
				StylePython(Start, EditSize);
				break;
			case SrcXml:
				StyleXml(Start, EditSize);
				break;
			case SrcHtml:
				StyleHtml(Start, EditSize);
				break;
			default:
				StyleDefault(Start, EditSize);
				break;
		}		
	}

	bool Pour(GRegion &r) override
	{
		GRect c = r.Bound();

		c.y2 -= EDIT_TRAY_HEIGHT;
		SetPos(c);
		
		return true;
	}
};

int DocEdit::LeftMarginPx = EDIT_LEFT_MARGIN;

bool DocEdit::OnKey(GKey &k)
{
	if (k.Alt())
	{
		if (ToLower(k.vkey) == 'm')
		{
			if (k.Down())
				Doc->GotoSearch(IDC_METHOD_SEARCH);
			return true;
		}
		else if (ToLower(k.vkey) == 'o' &&
				k.Shift())
		{
			if (k.Down())
				Doc->GotoSearch(IDC_FILE_SEARCH);
			return true;
		}
	}

	return GTextView3::OnKey(k); 
}

bool DocEdit::OnMenu(GDocView *View, int Id, void *Context)
{
	if (View)
	{
		switch (Id)
		{
			case IDM_FILE_COMMENT:
			{
				const char *Template = Doc->GetProject()->GetFileComment();
				if (Template)
				{
					char *File = strrchr(Doc->GetFileName(), DIR_CHAR);
					if (File)
					{
						char *Comment = TemplateMerge(Template, File + 1, 0);
						if (Comment)
						{
							char16 *C16 = Utf8ToWide(Comment);
							DeleteArray(Comment);
							if (C16)
							{
								Insert(Cursor, C16, StrlenW(C16));
								DeleteArray(C16);
								Invalidate();
							}
						}
					}
				}
				break;
			}
			case IDM_FUNC_COMMENT:
			{
				const char *Template = Doc->GetProject()->GetFunctionComment();
				if (ValidStr(Template))
				{
					char16 *n = NameW();
					if (n)
					{
						List<char16> Tokens;
						char16 *s;
						char16 *p = n + GetCaret();
						char16 OpenBrac[] = { '(', 0 };
						char16 CloseBrac[] = { ')', 0 };
						int OpenBracketIndex = -1;							
						
						// Parse from cursor to the end of the function defn
						while ((s = LexCpp(p, LexStrdup)))
						{
							if (StricmpW(s, OpenBrac) == 0)
							{
								OpenBracketIndex = Tokens.Length();
							}

							Tokens.Insert(s);

							if (StricmpW(s, CloseBrac) == 0)
							{
								break;
							}
						}
						
						if (OpenBracketIndex > 0)
						{
							char *FuncName = WideToUtf8(Tokens[OpenBracketIndex-1]);
							if (FuncName)
							{
								// Get a list of parameter names
								List<char> Params;
								for (int i = OpenBracketIndex+1; (p = Tokens[i]); i++)
								{
									char16 Comma[] = { ',', 0 };
									if (StricmpW(p, Comma) == 0 ||
										StricmpW(p, CloseBrac) == 0)
									{
										char16 *Param = Tokens[i-1];
										if (Param)
										{
											Params.Insert(WideToUtf8(Param));
										}
									}
								}
								
								// Do insertion
								char *Comment = TemplateMerge(Template, FuncName, &Params);
								if (Comment)
								{
									char16 *C16 = Utf8ToWide(Comment);
									DeleteArray(Comment);
									if (C16)
									{
										Insert(Cursor, C16, StrlenW(C16));
										DeleteArray(C16);
										Invalidate();
									}
								}
								
								// Clean up
								DeleteArray(FuncName);
								Params.DeleteArrays();
							}
							else
							{
								LgiTrace("%s:%i - No function name.\n", _FL);
							}
						}
						else
						{
							LgiTrace("%s:%i - OpenBracketIndex not found.\n", _FL);
						}
						
						Tokens.DeleteArrays();
					}
					else
					{
						LgiTrace("%s:%i - No input text.\n", _FL);
					}
				}
				else
				{
					LgiTrace("%s:%i - No template.\n", _FL);
				}
				break;
			}
		}
	}
	
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
IdeDocPrivate::IdeDocPrivate(IdeDoc *d, AppWnd *a, NodeSource *src, const char *file) : NodeView(src)
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

enum
{
	IDM_COPY_FILE = 1100,
	IDM_COPY_PATH,
	IDM_BROWSE
};

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
				char Args[MAX_PATH];
				if (LgiGetAppForMimeType("inode/directory", Args, sizeof(Args)))
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
	if (Write)
	{
		d->SetFileName(f);
		d->Edit->Save(d->GetLocalFile());
	}
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
						if (Resp->Results.Length() == 1)
						{
							FindSymResult *r = Resp->Results[0];
							d->SymPopup->Visible(false);

							d->App->GotoReference(r->File, r->Line, false);
						}
						else
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
