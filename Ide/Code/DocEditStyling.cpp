#include "Lgi.h"
#include "LgiIde.h"
#include "DocEdit.h"

#define COMP_STYLE	1

#define ColourComment		GColour(0, 140, 0)
#define ColourHashDef		GColour(0, 0, 222)
#define ColourLiteral		GColour(192, 0, 0)
#define ColourKeyword		GColour::Black
#define ColourType			GColour(0, 0, 222)
#define ColourPhp			GColour(140, 140, 180)
#define ColourHtml			GColour(80, 80, 255)
#define ColourPre			GColour(150, 110, 110)
#define ColourStyle			GColour(110, 110, 150)

#define IsSymbolChar(ch)	(IsAlpha(ch) || (ch) == '_')

#define DetectKeyword()			\
	Node *n = &Root;			\
	char16 *e = s;				\
	do							\
	{							\
		int idx = n->Map(*e);	\
		if (idx < 0)			\
			break;				\
		n = n->Next[idx];		\
		e++;					\
	}							\
	while (n);
					

struct LanguageParams
{
	const char **Keywords;
	const char **Types;
	const char **Edges;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CPP
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
							"GAutoPtr", "LHashTbl",
							NULL};
const char *CppEdges[] = {	"/*", "*/", "\"", NULL };

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Python
const char *PythonKeywords[] = {"def", "try", "except", "import", "if", "for", "elif", "else", NULL};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// XML

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HTML
const char *HtmlEdges[] = {	"<?php", "?>", "/*", "*/", "<style", "</style>", "<pre", "</pre>", "\"", "\'", NULL };

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
LanguageParams LangParam[] =
{
	// Unknown
	{NULL, NULL, NULL},
	// Plain text
	{DefaultKeywords, NULL, NULL},
	// C/C++
	{CppKeywords, CppTypes, CppEdges},
	// Python
	{PythonKeywords, NULL, NULL},
	// Xml
	{NULL, NULL, NULL},
	// Html/Php
	{NULL, NULL, HtmlEdges}
};

DocEditStyling::DocEditStyling(DocEdit *view) : 
	View(view),
	ParentState(KWaiting), WorkerState(KWaiting),
	LThread("DocEditStyling.Thread"),
	LMutex("DocEditStyling.Lock"),
	Event("DocEditStyling.Event"),
	Params(view)
{
}

void DocEdit::OnApplyStyles()
{
	GProfile p("OnApplyStyles");

	Style.Empty();

	p.Add("Lock");

	GTextView3::GStyle Vis(STYLE_NONE);
	GetVisible(Vis);

	if (DocEditStyling::Lock(_FL))
	{
		p.Add("Insert");
		Style.Swap(Params.Styles);

		p.Add("Inval");

		if (Params.Dirty.Start >= 0)
		{
			// LgiTrace("Visible rgn: %i + %i = %i\n", Vis.Start, Vis.Len, Vis.End());
			// LgiTrace("Dirty rgn: %i + %i = %i\n", Dirty.Start, Dirty.Len, Dirty.End());

			ssize_t CurLine = -1, DirtyStartLine = -1, DirtyEndLine = -1;
			GetTextLine(Cursor, &CurLine);
			GTextLine *Start = GetTextLine(Params.Dirty.Start, &DirtyStartLine);
			GTextLine *End = GetTextLine(MIN(Size, Params.Dirty.End()), &DirtyEndLine);
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
							Params.Dirty.End() >= Vis.End() ? c.y2 : DocToScreen(End->r).y2);
						
					// LgiTrace("Cli: %s, CursorLine: %s, Start rgn: %s, End rgn: %s, Update: %s\n", c.GetStr(), CursorLine->r.GetStr(), Start->r.GetStr(), End->r.GetStr(), r.GetStr());
					Invalidate(&r);
				}						
			}
			else
			{
				// LgiTrace("No Change: %i, %i, %i\n", CurLine, DirtyStartLine, DirtyEndLine);
			}

		}
		else
		{
			Invalidate();
		}

		p.Add("Unlock");
		DocEditStyling::Unlock();
	}
}

int DocEditStyling::Main()
{
	LThreadEvent::WaitStatus s;
	while (ParentState != KExiting && (s = Event.Wait()) == LThreadEvent::WaitSignaled)
	{
		StylingParams p(View);
		if (Lock(_FL))
		{
			Params.Dirty.Empty();
			Params.Styles.Empty();
			p = Params;
			Unlock();
		}

		WorkerState = KStyling;
		LgiTrace("DocEdit.Worker starting style...\n");
		switch (FileType)
		{
			case SrcCpp:
				StyleCpp(p);
				break;
			case SrcPython:
				StylePython(p);
				break;
			case SrcXml:
				StyleXml(p);
				break;
			case SrcHtml:
				StyleHtml(p);
				break;
			default:
				StyleDefault(p);
				break;
		}
		if (ParentState != KCancel)
		{
			LgiTrace("DocEdit.Worker finished style...\n");
			View->PostEvent(M_STYLING_DONE);
		}
		else
		{
			LgiTrace("DocEdit.Worker canceled style...\n");
		}

		if (LMutex::Lock(_FL))
		{
			Params.Dirty = p.Dirty;
			Params.Styles.Swap(p.Styles);
			LMutex::Unlock();
		}

		WorkerState = KWaiting;
	}

	return 0;
}

void DocEditStyling::StyleCpp(StylingParams &p)
{
	GProfile Prof("DocEdit::StyleCpp");

	if (!p.Text.Length())
		return;

	char16 *Text = p.Text.AddressOf();
	char16 *s = Text;
	char16 *e = s + p.Text.Length();
		
	Prof.Add("Scan");

	LUnrolledList<GTextView3::GStyle> Out;
	for (; ParentState != KCancel && s < e; s++)
	{
		// uint64 Start = LgiMicroTime();
		char16 ch;
		if (IsDigit(*s))
			ch = '0';
		else if (IsAlpha(*s))
			ch = 'a';
		else
			ch = *s;

		switch (*s)
		{
			case '\"':
			case '\'':
				p.StyleString(s, e, ColourLiteral);
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
					auto &st = Out.New().Construct(View, STYLE_IDE);
					st.Start = s - Text;
					st.Font = View->GetFont();
							
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
							
					st.Len = (s - Text) - st.Start;
					st.Fore = ColourHashDef;
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
					auto &st = Out.New().Construct(View, STYLE_IDE);

					st.Start = s - Text;
					st.Font = View->GetFont();

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
							
					st.Len = (s - Text) - st.Start;
					st.Fore = ColourLiteral;
					s--;
				}
				while (s < e - 1 && IsDigit(s[1]))
					s++;
				break;
			}
			case '/':
			{
				if (s[1] == '/')
				{
					auto &st = Out.New().Construct(View, STYLE_IDE);
					st.Start = s - Text;
					st.Font = View->GetFont();
					while (s < e && *s != '\n')
						s++;
					st.Len = (s - Text) - st.Start;
					st.Fore = ColourComment;
					s--;
				}
				else if (s[1] == '*')
				{
					auto &st = Out.New().Construct(View, STYLE_IDE);
					st.Start = s - Text;
					st.Font = View->GetFont();
					s += 2;
					while (s < e && !(s[-2] == '*' && s[-1] == '/'))
						s++;
					st.Len = (s - Text) - st.Start;
					st.Fore = ColourComment;
					s--;
				}
				break;
			}
			default:
			{
				wchar_t Ch = ToLower(*s);
				if (Ch >= 'a' && Ch <= 'z')
				{
					DetectKeyword();

					if (n && n->Type)
					{
						auto &st = Out.New().Construct(View, STYLE_IDE);
						st.Start = s - Text;
						st.Font = n->Type == KType ? View->GetFont() : View->GetBold();
						st.Len = e - s;
						st.Fore = n->Type == KType ? ColourType : ColourKeyword;
					}

					s = e - 1;
				}
			}
		}

		// uint64 End = LgiMicroTime();
		// Times[ch&0xff] += End - Start;
	}

	/*
	for (int i=0; i<CountOf(Times); i++)
	{
		if (Times[i])
			LgiTrace("\t'%c' = %.1f\n", (char)i, (double)Times[i]/1000.0);
	}
	*/

	#if COMP_STYLE
	Prof.Add("Compare");

	GTextView3::GStyle Vis(STYLE_NONE);
	if (View->GetVisible(Vis) && ParentState != KCancel)
	{
		GArray<GTextView3::GStyle*> Old, Cur;
		for (auto s : PrevStyle)
		{
			if (s.Overlap(Vis))
				Old.Add(&s);
			else if (Old.Length())
				break;
		}
		for (auto s : Out)
		{
			if (s.Overlap(Vis))
				Cur.Add(&s);
			else if (Cur.Length())
				break;
		}

		for (int o=0; o<Old.Length(); o++)
		{
			bool Match = false;
			GTextView3::GStyle *OldStyle = Old[o];
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
				p.Dirty.Union(*OldStyle);
		}
		for (int n=0; n<Cur.Length(); n++)
		{
			p.Dirty.Union(*Cur[n]);
		}

		PrevStyle = Out;
	}	
	#endif

	p.Styles.Swap(Out);
}

void DocEditStyling::StylePython(StylingParams &p)
{
	char16 *Text = p.Text.AddressOf();
	char16 *e = Text + p.Text.Length();
		
	auto &Style = p.Styles;
	for (char16 *s = Text; s < e; s++)
	{
		switch (*s)
		{
			case '\"':
			case '\'':
				p.StyleString(s, e, ColourLiteral);
				break;
			case '#':
			{
				// Single line comment
				auto &st = Style.New().Construct(View, STYLE_IDE);
				st.Start = s - Text;
				st.Font = View->GetFont();
				while (s < e && *s != '\n')
					s++;
				st.Len = (s - Text) - st.Start;
				st.Fore = ColourComment;
				s--;
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
					auto &st = Style.New().Construct(View, STYLE_IDE);
					st.Start = s - Text;
					st.Font = View->GetFont();

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
							
					st.Len = (s - Text) - st.Start;
					st.Fore = ColourLiteral;
					s--;
				}
				while (s < e - 1 && IsDigit(s[1]))
					s++;
				break;
			}
			default:
			{
				if (*s >= 'a' && *s <= 'z')
				{
					/*
					if (HasKeyword[*s - 'a'])
					{
						static char16 buf[64], *o = buf;
						char16 *e = s;
						while (IsSymbolChar(*e))
							*o++ = *e++;
						*o = 0;
						KeyworkType type = Keywords.Find(buf);
						
						if (type != KNone)
						{
							GAutoPtr<GStyle> st(new GTextView3::GStyle(STYLE_IDE));
							if (st)
							{
								st->View = this;
								st->Start = s - Text;
								st->Font = Bold;
								st->Len = e - s;
								st->Fore = ColourKeyword;
								InsertStyle(st);
								s = e - 1;
							}
						}
					}
					*/
				}
				break;
			}
		}
	}
}

void DocEditStyling::StyleDefault(StylingParams &p)
{
	char16 *Text = p.Text.AddressOf();
	char16 *e = Text + p.Text.Length();
		
	auto &Style = p.Styles;
	for (char16 *s = Text; s < e; s++)
	{
		switch (*s)
		{
			case '\"':
			case '\'':
			case '`':
			{
				auto &st = Style.New().Construct(View, STYLE_IDE);
				bool Quoted = s > Text && s[-1] == '\\';

				st.Start = s - Text - Quoted;
				st.Font = View->GetFont();

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
				st.Len = (s - Text) - st.Start + 1;
				st.Fore = ColourLiteral;
				break;
			}
			case '#':
			{
				// Single line comment
				auto &st = Style.New().Construct(View, STYLE_IDE);
				st.Start = s - Text;
				st.Font = View->GetFont();
				while (s < e && *s != '\n')
					s++;
				st.Len = (s - Text) - st.Start;
				st.Fore = ColourComment;
				s--;
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
					auto &st = Style.New().Construct(View, STYLE_IDE);
					st.Start = s - Text - ((s > Text && strchr("-+", s[-1])) ? 1 : 0);
					st.Font = View->GetFont();

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
							
					st.Len = (s - Text) - st.Start;
					st.Fore = ColourLiteral;
					s--;
				}
				while (s < e - 1 && IsDigit(s[1]))
					s++;
				break;
			}
			default:
			{
				if (*s >= 'a' && *s <= 'z')
				{
					/*
					if (HasKeyword[*s - 'a'])
					{
						static char16 buf[64], *o = buf;
						char16 *e = s;
						while (IsSymbolChar(*e))
							*o++ = *e++;
						*o = 0;
						KeyworkType type = Keywords.Find(buf);
						
						if (type != KNone)
						{
							GAutoPtr<GStyle> st(new GTextView3::GStyle(STYLE_IDE));
							if (st)
							{
								st->View = this;
								st->Start = s - Text;
								st->Font = Bold;
								st->Len = e - s;
								st->Fore = ColourKeyword;
								InsertStyle(st);
								s = e - 1;
							}
						}
					}
					*/
				}
				break;
			}
		}
	}
}

void DocEditStyling::StyleXml(StylingParams &p)
{
	char16 *Text = p.Text.AddressOf();
	char16 *e = Text + p.Text.Length();
		
	auto &Style = p.Styles;
	for (char16 *s = Text; s < e; s++)
	{
		switch (*s)
		{
			case '\"':
			case '\'':
				p.StyleString(s, e, ColourLiteral);
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
					auto &st = Style.New().Construct(View, STYLE_IDE);
					st.Start = s - Text;
					st.Font = View->GetFont();

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
							
					st.Len = (s - Text) - st.Start;
					st.Fore = ColourLiteral;
					s--;
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
					auto &st = Style.New().Construct(View, STYLE_IDE);
					st.Start = s - Text;
					st.Font = View->GetFont();
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
					st.Len = (s - Text) - st.Start;
					st.Fore = ColourComment;
					s--;
				}
				break;
			}
			default:
			{
				if (*s >= 'a' && *s <= 'z')
				{
					/*
					if (HasKeyword[*s - 'a'])
					{
						static char16 buf[64], *o = buf;
						char16 *e = s;
						while (IsSymbolChar(*e))
							*o++ = *e++;
						*o = 0;
						KeyworkType type = Keywords.Find(buf);
						
						if (type != KNone)
						{
							GAutoPtr<GStyle> st(new GTextView3::GStyle(STYLE_IDE));
							if (st)
							{
								st->View = this;
								st->Start = s - Text;
								st->Font = Bold;
								st->Len = e - s;
								st->Fore = ColourKeyword;
								InsertStyle(st);
								s = e - 1;
							}
						}
					}
					*/
				}
				break;
			}
		}
	}
}

GColour DocEditStyling::ColourFromType(DocType t)
{
	switch (t)
	{
		default:
			return GColour::Black;
		case CodePhp:
			return ColourPhp;
		case CodeCss:
			return ColourStyle;
		case CodePre:
			return ColourPre;
		case CodeComment:
			return ColourComment;
	}
}

void DocEditStyling::StyleHtml(StylingParams &p)
{
	char16 *Text = p.Text.AddressOf();
	char16 *e = Text + p.Text.Length();

	auto &Style = p.Styles;

	char *Ext = LgiGetExtension(p.FileName);
	DocType Type = Ext && !stricmp(Ext, "js") ? CodePhp : CodeHtml;
	GTextView3::GStyle *Cur;

	#define START_CODE() \
		if (Type != CodeHtml) \
		{ \
			Cur = &Style.New().Construct(View, STYLE_IDE); \
			Cur->Start = s - Text; \
			Cur->Font = View->GetFont(); \
			Cur->Fore = ColourFromType(Type); \
		}
	#define END_CODE() \
		if (Cur) \
		{ \
			Cur->Len = (s - Text) - Cur->Start; \
			if (Cur->Len <= 0) \
			{ \
				Style.Delete(Style.rbegin()); \
				Cur = NULL; \
			} \
		}

	char16 *s;
	for (s = Text; s < e; s++)
	{
		switch (*s)
		{
			case '\'':
			{
				if (s > Text &&
					IsAlpha(s[-1]))
					break;
				// else fall through
			}
			case '\"':
			{
				if (Type != CodeComment)
				{
					END_CODE();
					p.StyleString(s, e, ColourLiteral);
					s++;
					START_CODE();
					s--;
				}
				break;
			}
			case '/':
			{
				if (Type != CodeHtml &&
					Type != CodePre)
				{
					if (s[1] == '/')
					{
						END_CODE();

						char16 *nl = Strchr(s, '\n');
						if (!nl) nl = s + Strlen(s);
						
						auto &st = Style.New().Construct(View, STYLE_IDE);
						st.Start = s - Text;
						st.Font = View->GetFont();
						st.Fore = ColourComment;
						st.Len = nl - s;

						s = nl;
						START_CODE();
					}
					else if (s[1] == '*')
					{
						END_CODE();

						char16 *end_comment = Stristr(s, L"*/");
						if (!end_comment) end_comment = s + Strlen(s);
						else end_comment += 2;
						
						auto &st = Style.New().Construct(View, STYLE_IDE);
						st.Start = s - Text;
						st.Font = View->GetFont();
						st.Fore = ColourComment;
						st.Len = end_comment - s;

						s = end_comment;
						START_CODE();
					}
				}
				break;
			}
			case '<':
			{
				#define IS_TAG(name) \
					((tmp = strlen(#name)) && \
						len == tmp && \
						!Strnicmp(tag, L ## #name, tmp))
				#define SCAN_TAG() \
					char16 *tag = s + 1; \
					char16 *c = tag; \
					while (c < e && strchr(WhiteSpace, *c)) c++; \
					while (c < e && (IsAlpha(*c) || strchr("!_/0123456789", *c))) c++; \
					size_t len = c - tag, tmp;

				if (Type == CodeHtml)
				{
					if (s[1] == '?' &&
						s[2] == 'p' &&
						s[3] == 'h' &&
						s[4] == 'p')
					{
						// Start PHP block
						Type = CodePhp;
						START_CODE();
						s += 4;
					}
					else if (	s[1] == '!' &&
								s[2] == '-' &&
								s[3] == '-')
					{
						// Start comment
						Type = CodeComment;
						START_CODE();
						s += 3;
					}
					else
					{
						// Html element
						SCAN_TAG();
						bool start = false;
						if (IS_TAG(pre))
						{
							Type = CodePre;
							while (*c && *c != '>') c++;
							if (*c) c++;
							start = true;
						}
						else if (IS_TAG(style))
						{
							Type = CodeCss;
							while (*c && *c != '>') c++;
							if (*c) c++;
							start = true;
						}

						auto &st = Style.New().Construct(View, STYLE_IDE);
						st.Start = s - Text;
						st.Font = View->GetFont();
						st.Fore = ColourHtml;
						st.Len = c - s;
						s = c - 1;

						if (start)
						{
							s++;
							START_CODE();
						}
					}
				}
				else if (Type == CodePre)
				{
					SCAN_TAG();
					if (IS_TAG(/pre))
					{
						END_CODE();
						Type = CodeHtml;
						s--;
					}
				}
				else if (Type == CodeCss)
				{
					SCAN_TAG();
					if (IS_TAG(/style))
					{
						END_CODE();
						Type = CodeHtml;
						s--;
					}
				}
				break;
			}
			case '>':
			{
				if (Type == CodeHtml)
				{
					auto &st = Style.New().Construct(View, STYLE_IDE);
					st.Start = s - Text;
					st.Font = View->GetFont();
					st.Fore = ColourHtml;
					st.Len = 1;
				}
				else if (Type == CodeComment)
				{
					if (s - 2 >= Text &&
						s[-1] == '-' &&
						s[-2] == '-')
					{
						s++;
						END_CODE();
						Type = CodeHtml;
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

	END_CODE();
}
	
void DocEditStyling::AddKeywords(const char **keys, bool IsType)
{
	for (const char **k = keys; *k; k++)
	{
		Node *n = &Root;
		for (const char *c = *k; *c && n; c++)
		{
			int idx = n->Map(*c);
			LgiAssert(idx >= 0);
			if (!n->Next[idx])
				n->Next[idx] = new Node;
			n = n->Next[idx];
		}
		if (n)
			n->Type = IsType ? KType : KLang;
	}
}

void DocEdit::PourStyle(size_t Start, ssize_t EditSize)
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
				!stricmp(Ext, "php") ||
				!stricmp(Ext, "js"))
			FileType = SrcHtml;
		else
			FileType = SrcPlainText;

		if (LangParam[FileType].Keywords)
			AddKeywords(LangParam[FileType].Keywords, false);
		if (LangParam[FileType].Types)
			AddKeywords(LangParam[FileType].Types, true);
		if (LangParam[FileType].Edges)
		{
			RefreshEdges = LangParam[FileType].Edges;
			RefreshSize = 0;
			for (const char **e = RefreshEdges; *e; e++)
				RefreshSize = MAX(strlen(*e), RefreshSize);
		}
	}

	// Get into the waiting mode (so the worker is not using 'StyleIn' data)
	if (WorkerState == KStyling)
	{
		#ifdef _DEBUG
		uint64 Start = LgiMicroTime();
		#endif

		ParentState = KCancel;
		while (WorkerState != KWaiting)
			LgiSleep(1);

		#ifdef _DEBUG
		uint64 Tm = LgiMicroTime() - Start;
		LgiTrace("DocEdit: Styling->Waiting time: %.1g ms\n", (double) Tm / 1000.0);
		#endif
	}

	// Create the StyleIn array from the current document

	// Lock the object and give the text to the worker
	LgiTrace("DocEdit starting style...\n");
	ParentState = KStyling;
	if (LMutex::Lock(_FL))
	{
		Params.PourStart = Start;
		Params.PourSize = EditSize;
		Params.Text.Length(Size);
		Params.FileName = Doc->GetFileName();
		memcpy(Params.Text.AddressOf(), Text, sizeof(*Text) * Size);

		LMutex::Unlock();
		Event.Signal();
	}
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

