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
							"GAutoPtr",
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

void DocEdit::StyleString(char16 *&s, char16 *e)
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

void DocEdit::StyleCpp(ssize_t Start, ssize_t EditSize)
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

void DocEdit::StylePython(ssize_t Start, ssize_t EditSize)
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

void DocEdit::StyleDefault(ssize_t Start, ssize_t EditSize)
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

void DocEdit::StyleXml(ssize_t Start, ssize_t EditSize)
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

GColour DocEdit::ColourFromType(HtmlType t)
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

void DocEdit::StyleHtml(ssize_t Start, ssize_t EditSize)
{
	char16 *e = Text + Size;

	Style.DeleteObjects();

	char *Ext = LgiGetExtension(Doc->GetFileName());
	HtmlType Type = Ext && !stricmp(Ext, "js") ? CodePhp : CodeHtml;
	GAutoPtr<GStyle> Cur;

	#define START_CODE() \
		if (Type != CodeHtml) \
		{ \
			if (Cur.Reset(new GTextView3::GStyle(STYLE_IDE))) \
			{ \
				Cur->View = this; \
				Cur->Start = s - Text; \
				Cur->Font = GetFont(); \
				Cur->Fore = ColourFromType(Type); \
			} \
		}
	#define END_CODE() \
		if (Cur) \
		{ \
			Cur->Len = (s - Text) - Cur->Start; \
			if (Cur->Len > 0) \
				InsertStyle(Cur); \
			else \
				Cur.Reset(); \
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
					StyleString(s, e);
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
					else if (s[1] == '*')
					{
						END_CODE();

						char16 *end_comment = Stristr(s, L"*/");
						if (!end_comment) end_comment = s + Strlen(s);
						else end_comment += 2;
						
						GAutoPtr<GStyle> st;
						if (st.Reset(new GTextView3::GStyle(STYLE_IDE)))
						{
							st->View = this;
							st->Start = s - Text;
							st->Font = GetFont();
							st->Fore = ColourComment;
							st->Len = end_comment - s;
							InsertStyle(st);
						}

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

						GAutoPtr<GStyle> st;
						if (st.Reset(new GTextView3::GStyle(STYLE_IDE)))
						{
							st->View = this;
							st->Start = s - Text;
							st->Font = GetFont();
							st->Fore = ColourHtml;
							st->Len = c - s;
							InsertStyle(st);
							s = c - 1;
						}

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
	
void DocEdit::AddKeywords(const char **keys, bool IsType)
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

		ZeroObj(HasKeyword);
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
