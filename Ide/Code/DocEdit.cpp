#include "Lgi.h"
#include "LgiIde.h"
#include "LgiRes.h"
#include "DocEdit.h"
#include "IdeDocPrivate.h"
#include "GScrollBar.h"

#define EDIT_TRAY_HEIGHT	(SysFont->GetHeight() + 10)
#define EDIT_LEFT_MARGIN	16 // gutter for debug break points

int DocEdit::LeftMarginPx = EDIT_LEFT_MARGIN;

GAutoPtr<GDocFindReplaceParams> GlobalFindReplace;

DocEdit::DocEdit(IdeDoc *d, GFontType *f) : GTextView3(IDC_EDIT, 0, 0, 100, 100, f)
{
	RefreshSize = 0;
	RefreshEdges = NULL;
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
	
DocEdit::~DocEdit()
{
	SetEnv(0);
	for (int i=0; i<CountOf(HasKeyword); i++)
	{
		DeleteObj(HasKeyword[i]);
	}
}

bool DocEdit::AppendItems(GSubMenu *Menu, int Base)
{
	GSubMenu *Insert = Menu->AppendSub("Insert...");
	if (Insert)
	{
		Insert->AppendItem("File Comment", IDM_FILE_COMMENT, Doc->GetProject() != 0);
		Insert->AppendItem("Function Comment", IDM_FUNC_COMMENT, Doc->GetProject() != 0);
	}

	return true;
}
	
bool DocEdit::DoGoto()
{
	GInput Dlg(this, "", LgiLoadString(L_TEXTCTRL_GOTO_LINE, "Goto [file:]line:"), "Goto");
	if (Dlg.DoModal() != IDOK || !ValidStr(Dlg.GetStr()))
		return false;

	GString s = Dlg.GetStr();
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

bool DocEdit::SetPourEnabled(bool b)
{
	bool e = PourEnabled;
	PourEnabled = b;
	if (PourEnabled)
	{
		PourText(0, Size);
		PourStyle(0, Size);
	}

	return e;
}

int DocEdit::GetTopPaddingPx()
{
	return GetCss(true)->PaddingTop().ToPx(GetClient().Y(), GetFont());
}

void DocEdit::InvalidateLine(int Idx)
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
	
void DocEdit::OnPaintLeftMargin(GSurface *pDC, GRect &r, GColour &colour)
{
	GColour GutterColour(0xfa, 0xfa, 0xfa);
	GTextView3::OnPaintLeftMargin(pDC, r, GutterColour);
	int Y = ScrollYLine();
		
	int TopPaddingPx = GetTopPaddingPx();

	pDC->Colour(GColour(200, 0, 0));
	List<GTextLine>::I it = GTextView3::Line.begin(Y);
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
		it = GTextView3::Line.begin();
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

void DocEdit::OnMouseClick(GMouse &m)
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

void DocEdit::SetCaret(size_t i, bool Select, bool ForceFullUpdate)
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
	
char *DocEdit::TemplateMerge(const char *Template, char *Name, List<char> *Params)
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

bool DocEdit::GetVisible(GStyle &s)
{
	GRect c = GetClient();
	int a = HitText(c.x1, c.y1, false);
	int b = HitText(c.x2, c.y2, false);
	s.Start = a;
	s.Len = b - a + 1;
	return true;
}

bool DocEdit::Pour(GRegion &r)
{
	GRect c = r.Bound();

	c.y2 -= EDIT_TRAY_HEIGHT;
	SetPos(c);
		
	return true;
}

int DocEdit::CountRefreshEdges(size_t At, ssize_t Len)
{
	if (!RefreshEdges)
		return 0;

	size_t s = MAX(0, At - (RefreshSize - 1));
	bool t[256] = {0};
	for (const char **Edge = RefreshEdges; *Edge; Edge++)
	{
		const char *e = *Edge;
		t[e[0]] = true;
	}

	int Edges = 0;
	for (size_t i = s; i <= At; i++)
	{
		if (Text[i] < 256 && t[Text[i]])
		{
			for (const char **Edge = RefreshEdges; *Edge; Edge++)
			{
				int n = i;
				const char *e;
				for (e = *Edge; *e; e++)
					if (Text[n++] != *e)
						break;
				if (!*e)
					Edges++;
			}
		}
	}

	return Edges;
}

bool DocEdit::Insert(size_t At, char16 *Data, ssize_t Len)
{
	int Old = PourEnabled ? CountRefreshEdges(At, 0) : 0;
	bool Status = GTextView3::Insert(At, Data, Len);
	int New = PourEnabled ? CountRefreshEdges(At, Len) : 0;
	if (Old != New)
		Invalidate();

	return Status;
}

bool DocEdit::Delete(size_t At, ssize_t Len)
{
	int Old = CountRefreshEdges(At, Len);
	bool Status = GTextView3::Delete(At, Len);
	int New = CountRefreshEdges(At, 0);
	if (Old != New)
		Invalidate();

	return Status;
}

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