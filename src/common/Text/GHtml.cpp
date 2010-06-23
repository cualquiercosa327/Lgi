#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "Lgi.h"
#include "GHtml.h"
#include "GHtmlPriv.h"
#include "GMap.h"
#include "GToken.h"
#include "GScrollBar.h"
#include "GVariant.h"
#include "GFindReplaceDlg.h"

#define LUIS_DEBUG					0
#define CRASH_TRACE					0
#ifdef MAC
#define GHTML_USE_DOUBLE_BUFFER		0
#else
#define GHTML_USE_DOUBLE_BUFFER		1
#endif
#define GT_TRANSPARENT				0x00000000
#ifndef IDC_HAND
#define IDC_HAND					MAKEINTRESOURCE(32649)
#endif

#undef CellSpacing
#define DefaultCellSpacing			0
#define DefaultCellPadding			3
#define DefaultPointSize			11
#define DefaultBodyMargin			5
#define DefaultImgSize				17
#define DefaultMissingCellColour	GT_TRANSPARENT // Rgb32(0xf0,0xf0,0xf0)
#ifdef _DEBUG
#define DefaultTableBorder			Rgb32(0xf8, 0xf8, 0xf8)
#else
#define DefaultTableBorder			GT_TRANSPARENT
#endif
#define DefaultTextColour			Rgb32(0, 0, 0)
#define MinFontSize					8
#define ShowNbsp					0

static char WordDelim[]	=			".,<>/?[]{}()*&^%$#@!+|\'\"";
static char16 WhiteW[] =			{' ', '\t', '\r', '\n', 0};
#define SkipWhiteSpace(s)			while (*s && IsWhiteSpace(*s)) s++;
#define SubtractPtr(a, b)			( (((int)(a))-((int)(b))) / sizeof(*a) )

//////////////////////////////////////////////////////////////////////
GHtmlStatic *Static = 0;

class GHtmlPrivate
{
public:
	GHashTable Loading;
	GHtmlStaticInst Inst;
	bool CursorVis;
	GRect CursorPos;
	bool WordSelectMode;
	GdcPt2 Content;
	bool LinkDoubleClick;

	GHtmlPrivate()
	{
		LinkDoubleClick = true;
		WordSelectMode = false;
		Static = Inst.Static;
		CursorVis = false;
		CursorPos.ZOff(-1, -1);
	}

	~GHtmlPrivate()
	{
	}
};

//////////////////////////////////////////////////////////////////////
GInfo TagInfo[] =
{
	{TAG_B,				"b",			0,			TI_NONE},
	{TAG_I,				"i",			0,			TI_NONE},
	{TAG_U,				"u",			0,			TI_NONE},
	{TAG_P,				"p",			0,			TI_BLOCK},
	{TAG_BR,			"br",			0,			TI_NEVER_CLOSES},
	{TAG_HR,			"hr",			0,			TI_BLOCK | TI_NEVER_CLOSES},
	{TAG_OL,			"ol",			0,			TI_BLOCK},
	{TAG_UL,			"ul",			0,			TI_BLOCK},
	{TAG_LI,			"li",			"ul",		TI_BLOCK},
	{TAG_FONT,			"font",			0,			TI_NONE},
	{TAG_A,				"a",			0,			TI_NONE},
	{TAG_TABLE,			"table",		0,			TI_BLOCK | TI_NO_TEXT | TI_TABLE},
	{TAG_TR,			"tr",			"table",	TI_BLOCK | TI_NO_TEXT | TI_TABLE},
	{TAG_TD,			"td",			"tr",		TI_BLOCK | TI_TABLE},
	{TAG_HEAD,			"head",			"html",		TI_NONE},
	{TAG_BODY,			"body",			0,			TI_BLOCK | TI_NO_TEXT},
	{TAG_IMG,			"img",			0,			TI_NEVER_CLOSES},
	{TAG_HTML,			"html",			0,			TI_BLOCK | TI_NO_TEXT},
	{TAG_DIV,			"div",			0,			TI_BLOCK},
	{TAG_SPAN,			"span",			0,			TI_NONE},
	{TAG_CENTER,		"center",		0,			TI_NONE},
	{TAG_META,			"meta",			0,			TI_NONE},
	{TAG_TBODY,			"tbody",		0,			TI_NONE},
	{TAG_STYLE,			"style",		0,			TI_NONE},
	{TAG_SCRIPT,		"script",		0,			TI_NONE},
	{TAG_STRONG,		"strong",		0,			TI_NONE},
	{TAG_BLOCKQUOTE,	"blockquote",	0,			TI_BLOCK},
	{TAG_PRE,			"pre",			0,			TI_BLOCK},
	{TAG_H1,			"h1",			0,			TI_BLOCK},
	{TAG_H2,			"h2",			0,			TI_BLOCK},
	{TAG_H3,			"h3",			0,			TI_BLOCK},
	{TAG_H4,			"h4",			0,			TI_BLOCK},
	{TAG_H5,			"h5",			0,			TI_BLOCK},
	{TAG_H6,			"h6",			0,			TI_BLOCK},
	{TAG_UNKNOWN,		0,				0,			TI_NONE},
};

GInfo *GetTagInfo(char *Tag)
{
	GInfo *i;

	if (!Tag)
		return 0;

	if (stricmp(Tag, "th") == 0)
	{
		Tag = "td";
	}

	for (i=TagInfo; i->Tag; i++)
	{
		if (stricmp(i->Tag, Tag) == 0)
		{
			break;
		}
	}

	return i;
}

static bool Is8Bit(char *s)
{
	while (*s)
	{
		if (((uchar)*s) & 0x80)
			return true;
		s++;
	}
	return false;
}

char *ParseName(char *s, char **Name)
{
	SkipWhiteSpace(s);
	char *Start = s;
	while (*s && (IsLetter(*s) || strchr("-:", *s) || IsDigit(*s))) s++;
	if (Name)
	{
		int Len = SubtractPtr(s, Start);
		if (Len > 0)
		{
			*Name = NewStr(Start, SubtractPtr(s, Start));
		}
	}
	return s;
}

char *ParsePropValue(char *s, char *&Value)
{
	Value = 0;
	if (s)
	{
		if (strchr("\"\'", *s))
		{
			char Delim = *s++;
			char *Start = s;
			while (*s && *s != Delim) s++;
			Value = NewStr(Start, SubtractPtr(s, Start));
			s++;
		}
		else
		{
			char *Start = s;
			while (*s && !IsWhiteSpace(*s) && *s != '>') s++;
			Value = NewStr(Start, SubtractPtr(s, Start));
		}
	}

	return s;
}

bool ParseColour(char *s, COLOUR &c)
{
	if (s)
	{
		COLOUR m;

		if (*s == '#')
		{
			s++;

			ParseHexColour:
			int i = htoi(s);
			int l = strlen(s);
			if (l == 3)
			{
				int r = i >> 8;
				int g = (i >> 4) & 0xf;
				int b = i & 0xf;
				c = Rgb32(r | (r<<4), g | (g << 4), b | (b << 4));
			}
			else if (l == 4)
			{
				int r = (i >> 12) & 0xf;
				int g = (i >> 8) & 0xf;
				int b = (i >> 4) & 0xf;
				int a = i & 0xf;
				c = Rgba32
					(
						r | (r <<4 ),
						g | (g << 4),
						b | (b << 4),
						a | (a << 4));
			}
			else if (l == 6)
			{
				c = Rgb32(i >> 16, (i >> 8) & 0xff, i & 0xff);
			}
			else if (l == 8)
			{
				c = Rgba32(i >> 24, (i >> 16) & 0xff, (i >> 8) & 0xff, i & 0xff);
			}
			else
			{
				return false;
			}
			
			return true;
		}
		else if ((m = GHtmlStatic::Inst->ColourMap.Find(s)) >= 0)
		{
			c = Rgb24To32(m);
			return true;
		}
		else if (tolower(s[0]) == 'r' &&
				 tolower(s[1]) == 'g' &&
				 tolower(s[2]) == 'b')
		{
			s += 3;
			SkipWhiteSpace(s);
			if (*s == '(')
			{
				s++;
				GArray<uint8> Col;
				while (Col.Length() < 3)
				{
					SkipWhiteSpace(s);
					if (isdigit(*s))
					{
						Col.Add(atoi(s));
						while (*s && isdigit(*s)) s++;
						SkipWhiteSpace(s);
						if (*s == ',') s++;
					}
					else break;
				}

				SkipWhiteSpace(s);
				if (*s == ')' && Col.Length() == 3)
				{
					c = Rgb32(Col[0], Col[1], Col[2]);
					return true;
				}
			}
		}
		else if (isdigit(*s) ||
				 (tolower(*s) >= 'a' && tolower(*s) <= 'f'))
		{
			goto ParseHexColour;
		}
	}

	return false;
}

char *ParsePropList(char *s, ObjProperties *Obj, bool &Closed)
{
	while (s && *s && *s != '>')
	{
		while (*s && IsWhiteSpace(*s)) s++;
		if (*s == '>') break;

		// get name
		char *Name = 0;
		char *n = ParseName(s, &Name);		
		if (*n == '/')
		{
			Closed = true;
		}		
		if (n == s)
		{
			s = ++n;
		}
		else
		{
			s = n;
		}

		while (*s && IsWhiteSpace(*s)) s++;

		if (*s == '=')
		{
			// get value
			s++;
			while (*s && IsWhiteSpace(*s)) s++;

			char *Value = 0;
			s = ParsePropValue(s, Value);

			if (Value && Name)
			{
				Obj->Set(Name, Value);
			}

			DeleteArray(Value);
		}

		DeleteArray(Name);
	}

	if (*s == '>') s++;

	return s;
}

//////////////////////////////////////////////////////////////////////
class GFontCache
{
	List<GFont> Fonts;

public:
	GFontCache()
	{
	}

	~GFontCache()
	{
		Fonts.DeleteObjects();
	}

	GFont *FontAt(int i)
	{
		return Fonts.ItemAt(i);
	}

	GFont *AddFont(GFont *Font)
	{
		if (Font)
		{
			char *Face = Font->Face();
			bool HasFace = ValidStr(Face);
			if (!HasFace)
			{
				LgiAssert(0);
			}
			if (HasFace)
			{
				GFont *f;
				for (f=Fonts.First(); f; f=Fonts.Next())
				{
					if (stricmp(f->Face(), Font->Face()) == 0 &&
						f->PointSize() == Font->PointSize() &&
						f->Bold() == Font->Bold() &&
						f->Italic() == Font->Italic() &&
						f->Underline() == Font->Underline())
					{
						break;
					}
				}

				if (f)
				{
					// Cached
					DeleteObj(Font);
					Font = f;
				}
				else
				{
					if (Font->Create((char*)0, 0))
					{
						// Not already cached
						Fonts.Insert(Font);
					}
					else
					{
						// Broken font, don't use...
						printf("%s:%i - Attempt to add broken font to font cache.\n", __FILE__, __LINE__);
						DeleteObj(Font);
						Font = Fonts.First();
					}
				}
			}
		}

		return Font;
	}
};

//////////////////////////////////////////////////////////////////////
class GFlowRegion
{
	List<class GFlowRect> Line;	// These pointers aren't owned by the flow region
								// When the line is finish, all the tag regions
								// will need to be vertically aligned

public:
	GHtml *Html;
	int x1, x2;					// Left and right margins
	int y1;						// Current y position
	int y2;						// Maximum used y position
	int cx;						// Current insertion point
	int my;						// How much of the area above y2 was just margin

	GFlowRegion(GHtml *html)
	{
		Html = html;
		x1 = x2 = y1 = y2 = cx = my = 0;
	}

	GFlowRegion(GHtml *html, GRect r)
	{
		Html = html;
		cx = x1 = r.x1;
		y1 = y2 = r.y1;
		x2 = r.x2;
		my = 0;
	}

	GFlowRegion(GFlowRegion &r)
	{
		Html = r.Html;
		x1 = r.x1;
		x2 = r.x2;
		y1 = r.y1;
		y2 = r.y2;
		cx = r.cx;
		my = r.my;
	}

	int X()
	{
		return x2 - cx;
	}

	void X(int newx)
	{
		x2 = x1 + newx - 1;
	}

	GFlowRegion &operator +=(GRect r)
	{
		x1 += r.x1;
		cx += r.x1;
		x2 -= r.x2;
		y1 += r.y1;
		y2 += r.y1;
		
		return *this;
	}

	GFlowRegion &operator -=(GRect r)
	{
		x1 -= r.x1;
		cx -= r.x1;
		x2 += r.x2;
		y1 += r.y2;
		y2 += r.y2;
	
		return *this;
	}
	
	void FinishLine(bool Margin = false);
	void EndBlock();
	void Insert(GFlowRect *Tr);
	GRect *LineBounds();

	void Indent(GFont *Font, GLength &Left, GLength &Top, GLength &Right, GLength &Bottom, bool Margin = false)
	{
		GFlowRegion This(*this);

		int LeftAbs = Left.Get(&This, Font, true);
		int RightAbs = Right.Get(&This, Font, true);
		int TopAbs = Top.Get(&This, Font, true);

		x1 += LeftAbs;
		cx += LeftAbs;
		x2 -= RightAbs;
		y1 += TopAbs;
		y2 += TopAbs;
		if (Margin)
			my += TopAbs;
	}

	void Outdent(GFont *Font, GLength &Left, GLength &Top, GLength &Right, GLength &Bottom, bool Margin = false)
	{
		GFlowRegion This = *this;

		int LeftAbs = Left.GetPrevAbs();
		int RightAbs = Right.GetPrevAbs();
		int BottomAbs = Bottom.Get(&This, Font);

		x1 -= LeftAbs;
		cx -= LeftAbs;
		x2 += RightAbs;
		// y1 += BottomAbs;
		y2 += BottomAbs;
		if (Margin)
			my += BottomAbs;
	}
};

//////////////////////////////////////////////////////////////////////
bool ParseDistance(char *s, float &d, char *units = 0)
{
	if (!s)
		return false;

	while (*s && IsWhiteSpace(*s)) s++;

	if (!IsDigit(*s) && !strchr("-.", *s))
		return false;

	d = atof(s);

	while (*s && (IsDigit(*s) || strchr("-.", *s))) s++;
	while (*s && IsWhiteSpace(*s)) s++;
	
	char _units[128];
	char *o = units = units ? units : _units;
	while (*s && (isalpha(*s) || *s == '%'))
	{
		*o++ = *s++;
	}
	*o++ = 0;

	return true;
}

GLength::GLength()
{
	d = 0;
	PrevAbs = 0;
	u = LengthNull;
}

GLength::GLength(char *s)
{
	Set(s);
}

bool GLength::IsValid()
{
	return u != LengthNull;
}

bool GLength::IsDynamic()
{
	return u == LengthPercentage || d == 0.0;
}

GLength::operator float ()
{
	return d;
}

GLength &GLength::operator =(float val)
{
	d = val;
	u = LengthPixels;
	return *this;
}

GLengthUnit GLength::GetUnits()
{
	return u;
}

void GLength::Set(char *s)
{
	if (ValidStr(s))
	{
		char Units[256] = "";
		if (ParseDistance(s, d, Units))
		{
			if (Units[0])
			{
				if (strchr(Units, '%'))
				{
					u = LengthPercentage;
				}
				else if (stristr(Units, "pt"))
				{
					u = LengthPoint;
				}
				else if (stristr(Units, "em"))
				{
					u = LengthEm;
				}
				else if (stristr(Units, "ex"))
				{
					u = LengthEx;
				}
				else
				{
					u = LengthPixels;
				}
			}
			else
			{
				u = LengthPixels;
			}
		}
	}
}

float GLength::Get(GFlowRegion *Flow, GFont *Font, bool Lock)
{
	switch (u)
	{
		case LengthEm:
		{
			return PrevAbs = d * (Font ? Font->GetHeight() : 14);
			break;
		}
		case LengthEx:
		{
			return PrevAbs = (Font ? Font->GetHeight() * d : 14) / 2;
			break;
		}
		case LengthPercentage:
		{
			if (Lock || PrevAbs == 0.0)
			{
				return PrevAbs = (Flow->X() * d / 100);
			}
			else
			{
				return PrevAbs;
			}
			break;
		}
	}

	double FlowX = Flow ? Flow->X() : d;
	return PrevAbs = min(FlowX, d);
}

GLine::GLine()
{
	LineStyle = -1;
	Colour = GT_TRANSPARENT;
	LineReset = 0x80000000;
}

GLine::~GLine()
{
}

GLine &GLine::operator =(int i)
{
	d = i;
	return *this;
}

void GLine::Set(char *s)
{
	GToken t(s, " \t");
	LineReset = 0x80000000;
	LineStyle = -1;
	char *Style = 0;

	for (int i=0; i<t.Length(); i++)
	{
		char *c = t[i];
		if
		(
			*c == '#'
			||
			GHtmlStatic::Inst->ColourMap.Find(c)
		)
		{
			ParseColour(c, Colour);
		}
		else if (strnicmp(c, "rgb(", 4) == 0)
		{
			char Buf[256];
			strcpy(Buf, c);
			while (!strchr(c, ')') &&
					(c = t[++i]))
			{
				strcat(Buf, c);
			}
			ParseColour(Buf, Colour);
		}
		else if (isdigit(*c))
		{
			GLength::Set(c);
		}
		else if (stricmp(c, "none") == 0)
		{
			Style = 0;
		}
		else if (	stricmp(c, "dotted") == 0 ||
					stricmp(c, "dashed") == 0 ||
					stricmp(c, "solid") == 0 ||
					stricmp(c, "float") == 0 ||
					stricmp(c, "groove") == 0 ||
					stricmp(c, "ridge") == 0 ||
					stricmp(c, "inset") == 0 ||
					stricmp(c, "outse") == 0)
		{
			Style = c;
		}
		else
		{
			// ???
		}
	}

	if (Style && stricmp(Style, "dotted") == 0)
	{
		switch ((int)d)
		{
			case 2:
			{
				LineStyle = 0xcccccccc;
				break;
			}
			case 3:
			{
				LineStyle = 0xe38e38;
				LineReset = 0x800000;
				break;
			}
			case 4:
			{
				LineStyle = 0xf0f0f0f0;
				break;
			}
			case 5:
			{
				LineStyle = 0xf83e0;
				LineReset = 0x80000;
				break;
			}
			case 6:
			{
				LineStyle = 0xfc0fc0;
				LineReset = 0x800000;
				break;
			}
			case 7:
			{
				LineStyle = 0xfe03f80;
				LineReset = 0x8000000;
				break;
			}
			case 8:
			{
				LineStyle = 0xff00ff00;
				break;
			}
			case 9:
			{
				LineStyle = 0x3fe00;
				LineReset = 0x20000;
				break;
			}
			default:
			{
				LineStyle = 0xaaaaaaaa;
				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////
GRect GTag::GetRect(bool Client)
{
	GRect r(Pos.x, Pos.y, Pos.x + Size.x - 1, Pos.y + Size.y - 1);
	if (!Client)
	{
		for (GTag *p = Parent; p; p=p->Parent)
		{
			r.Offset(p->Pos.x, p->Pos.y);
		}
	}
	return r;
}

CssAlign GTag::GetAlign(bool x)
{
	for (GTag *t = this; t; t = t->Parent)
	{
		if (t->TagId == TAG_TABLE)
			break;

		CssAlign *Attr = x ? &t->AlignX : &t->AlignY;
		if (*Attr != AlignInherit)
		{
			if (x)
			{
				AlignX = *Attr;
			}
			else
			{
				AlignY = *Attr;
			}
			return *Attr;
		}
	}
	
	return AlignInherit;
}

//////////////////////////////////////////////////////////////////////
void GTag::Attach(GTag *Child, int Idx)
{
	if (Child)
	{
		Child->Detach();

		Child->Parent = this;
		if (!Tags.HasItem(Child))
		{
			Tags.Insert(Child, Idx);
		}
	}
}

void GTag::Detach()
{
	if (Parent)
	{
		Parent->Tags.Delete(this);
		Parent = 0;
	}
}

GCellStore::GCellStore(GTag *Table)
{
	if (Table)
	{
		int y = 0;
		GTag *FakeRow = 0;
		GTag *FakeCell = 0;

		GTag *r;
		for (r=Table->Tags.First(); r; r=Table->Tags.Next())
		{
			if (r->TagId == TAG_TR)
			{
				FakeRow = 0;
				FakeCell = 0;
			}
			else if (r->TagId == TAG_TBODY)
			{
				int Index = Table->Tags.IndexOf(r);
				for (GTag *t = r->Tags.First(); t; t = r->Tags.Next())
				{
					Table->Tags.Insert(t, ++Index);
					t->Parent = Table;
				}
				r->Tags.Empty();
			}
			else
			{
				if (!FakeRow)
				{
					if (FakeRow = new GTag(Table->Html, 0))
					{
						FakeRow->Tag = NewStr("tr");
						FakeRow->TagId = TAG_TR;

						int Idx = Table->Tags.IndexOf(r);
						Table->Attach(FakeRow, Idx);
					}
				}
				if (FakeRow)
				{
					if (r->TagId != TAG_TD && !FakeCell)
					{
						if (FakeCell = new GTag(Table->Html, FakeRow))
						{
							FakeCell->Tag = NewStr("td");
							FakeCell->TagId = TAG_TD;
							FakeCell->Span.x = 1;
							FakeCell->Span.y = 1;
						}
					}

					int Idx = Table->Tags.IndexOf(r);
					r->Detach();

					if (r->TagId == TAG_TD)
					{
						FakeRow->Attach(r);
					}
					else
					{
						LgiAssert(FakeCell);
						FakeCell->Attach(r);
					}
					Table->Tags[Idx-1];
				}
			}
		}

		for (r=Table->Tags.First(); r; r=Table->Tags.Next())
		{
			if (r->TagId == TAG_TR)
			{
				int x = 0;
				for (GTag *c=r->Tags.First(); c; c=r->Tags.Next())
				{
					if (c->TagId == TAG_TD)
					{
						while (Get(x, y))
						{
							x++;
						}

						c->Cell.x = x;
						c->Cell.y = y;
						Set(c);
						x += c->Span.x;
					}
				}

				y++;
			}
		}
	}
}

void GCellStore::Dump()
{
	int Sx, Sy;
	GetSize(Sx, Sy);

	LgiTrace("Table %i x %i cells.\n", Sx, Sy);
	for (int x=0; x<Sx; x++)
	{
		LgiTrace("--- %-2i--- ", x);
	}
	LgiTrace("\n");

	for (int y=0; y<Sy; y++)
	{
		int x;
		for (x=0; x<Sx; x++)
		{
			GTag *t = Get(x, y);
			LgiTrace("%p ", t);
		}
		LgiTrace("\n");

		for (x=0; x<Sx; x++)
		{
			GTag *t = Get(x, y);
			char s[256] = "";
			if (t)
			{
				sprintf(s, "%i,%i-%i,%i", t->Cell.x, t->Cell.y, t->Span.x, t->Span.y);
			}
			LgiTrace("%-10s", s);
		}
		LgiTrace("\n");
	}

	LgiTrace("\n");
}

void GCellStore::GetAll(List<GTag> &All)
{
	List<Cell>::I i = Cells.Start();
	for (Cell *c=*i; c; c=*++i)
	{
		All.Insert(c->Tag);
	}
}

void GCellStore::GetSize(int &x, int &y)
{
	x = 0;
	y = 0;

	List<Cell>::I i = Cells.Start();
	for (Cell *c=*i; c; c=*++i)
	{
		if (c->Tag->Cell.x == c->x &&
			c->Tag->Cell.y == c->y)
		{
			x = max(x, c->x + c->Tag->Span.x);
			y = max(y, c->y + c->Tag->Span.y);
		}
	}
}

GTag *GCellStore::Get(int x, int y)
{
	List<Cell>::I i = Cells.Start();
	for (Cell *c=*i; c; c=*++i)
	{
		if (c->x == x && c->y == y)
		{
			return c->Tag;
		}
	}

	return 0;
}

bool GCellStore::Set(GTag *t)
{
	if (t)
	{
		for (int y=0; y<t->Span.y; y++)
		{
			for (int x=0; x<t->Span.x; x++)
			{
				int Cx = t->Cell.x + x;
				int Cy = t->Cell.y + y;

				if (!Get(Cx, Cy))
				{
					Cell *c = new Cell;
					if (c)
					{
						c->x = Cx;
						c->y = Cy;
						c->Tag = t;
						Cells.Insert(c);
					}
					else return false;
				}
				else return false;
			}
		}

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////
void GFlowRegion::EndBlock()
{
	if (cx > x1)
	{
		FinishLine();
	}
}

void GFlowRegion::FinishLine(bool Margin)
{
	/*
	GRect *b = LineBounds();
	if (b)
	{
		for (GFlowRect *Tr=Line.First(); Tr; Tr=Line.Next())
		{
			GRect n = *Tr;
			// int Base = b->y1 - n.y1;
			int Oy = Tr->Tag->AbsY();
			n.Offset(0, Oy);
			Tr->Offset(0, b->y2 - n.y2); // - Base
			// y2 = max(y2, Tr->y2);
		}
	}
	*/

	if (y2 > y1)
	{
		my = Margin ? y2 - y1 : 0;
		y1 = y2;
	}
	else
	{
		int fy = Html->DefFont()->GetHeight();
		my = Margin ? fy : 0;
		y1 += fy;
	}
	cx = x1;
	y2 = y1;

	Line.Empty();
}

GRect *GFlowRegion::LineBounds()
{
	GFlowRect *Prev = Line.First();
	GFlowRect *r=Prev;
	if (r)
	{
		GRect b;
		
		b = *r;
		int Ox = r->Tag->AbsX();
		int Oy = r->Tag->AbsY();
		b.Offset(Ox, Oy);

		// int Ox = 0, Oy = 0;
		while (r = Line.Next())
		{
			GRect c = *r;
			Ox = r->Tag->AbsX();
			Oy = r->Tag->AbsY();
			c.Offset(Ox, Oy);

			/*
			Ox += r->Tag->Pos.x - Prev->Tag->Pos.x;
			Oy += r->Tag->Pos.y - Prev->Tag->Pos.y;
			c.Offset(Ox, Oy);
			*/

			b.Union(&c);
			Prev = r;
		}

		static GRect Rgn;
		Rgn = b;
		return &Rgn;
	}

	return 0;
}

void GFlowRegion::Insert(GFlowRect *Tr)
{
	if (Tr)
	{
		Line.Insert(Tr);
		// y2 = max(y2, Tr->y2);
	}
}

//////////////////////////////////////////////////////////////////////
bool GTag::Selected = false;

GTag::GTag(GHtml *h, GTag *p)
{
	TipId = 0;
	IsBlock = false;
	Html = h;
	Parent = p;
	if (Parent)
	{
		Parent->Tags.Insert(this);
	}
	
	CellSpacing = DefaultCellSpacing;
	CellPadding = DefaultCellPadding;

	Cursor = -1;
	Selection = -1;
	PreText = Text = 0;
	Font = 0;
	Tag = 0;
	HtmlId = 0;
	// TableBorder = 0;
	Cells = 0;
	Image = 0;
	WasClosed = false;
	TagId = CONTENT;
	Info = 0;
	Visible = true;
	Fore = GT_TRANSPARENT;
	Back = GT_TRANSPARENT;
	MinContent = 0;
	MaxContent = 0;
	AlignX = AlignInherit;
	AlignY = AlignInherit;
	BackgroundRepeat = BgRepeat;

	#ifdef _DEBUG
	Debug = false;
	#endif
}

GTag::~GTag()
{
	if (Html->Cursor == this)
	{
		Html->Cursor = 0;
	}
	if (Html->Selection == this)
	{
		Html->Selection = 0;
	}
	if (Html->PrevTip == this)
	{
		Html->PrevTip = 0;
	}

	Tags.DeleteObjects();

	DeleteObj(Image);
	DeleteArray(Tag);
	DeleteArray(HtmlId);
	DeleteArray(Text);
	DeleteArray(PreText);
	DeleteObj(Cells);
}

bool GTag::GetVariant(char *Name, GVariant &Value, char *Array)
{
	return false;
}

bool GTag::SetVariant(char *Name, GVariant &Value, char *Array)
{
	if (!Name)
		return false;

	Set(Name, Value.CastString());
	Html->ViewWidth = -1;
	SetStyle();

	return true;
}

int GTag::GetTextStart()
{
	if (PreText)
	{
		GFlowRect *t = TextPos[1];
		return t ? t->Text - Text : 0;
	}
	else
	{
		GFlowRect *t = TextPos[0];
		return t ? t->Text - Text : 0;
	}
}

void GTag::SetText(char16 *NewText)
{
	TextPos.DeleteObjects();
	DeleteArray(Text);
	Text = NewText;
}

bool GTag::CreateSource(GStringPipe &p, int Depth, bool LastWasBlock)
{
	char *t8 = LgiNewUtf16To8(Text);
	char *Tabs = new char[Depth+1];
	memset(Tabs, '\t', Depth);
	Tabs[Depth] = 0;

	if (Tag)
	{
		if (IsBlock)
		{
			p.Print("\n%s<%s", Tabs, Tag);
		}
		else
		{
			p.Print("<%s", Tag);
		}
	}

	if (Tags.Length())
	{
		if (Tag)
		{
			p.Print(">%s", t8?t8:(char*)"");
		}

		bool Last = IsBlock;
		for (GTag *c = Tags.First(); c; c = Tags.Next())
		{
			c->CreateSource(p, Parent ? Depth+1 : 0, Last);
			Last = c->IsBlock;
		}

		if (Tag)
		{
			if (IsBlock)
			{
				p.Print("\n%s</%s>\n", Tabs, Tag);
			}
			else
			{
				p.Print("</%s>", Tag);
			}
		}
	}
	else if (Tag)
	{
		if (t8)
		{
			p.Print(">%s</%s>", t8, Tag);
		}
		else
		{
			p.Print("/>\n");
		}
	}
	else if (t8)
	{
		p.Print("%s", t8);
	}

	DeleteArray(t8);
	DeleteArray(Tabs);

	return true;
}

void GTag::SetTag(char *NewTag)
{
	DeleteArray(Tag);
	Tag = NewStr(NewTag);

	if (Info = GetTagInfo(Tag))
	{
		TagId = Info->Id;
		IsBlock = Info->Flags & TI_BLOCK;
		SetStyle();
	}
}

void GTag::_TraceOpenTags()
{
	GStringPipe p;
	for (GTag *t=Html->OpenTags.First(); t; t=Html->OpenTags.Next())
	{
		p.Print(", %s", t->Tag);
		if (t->HtmlId)
		{
			p.Print("#%s", t->HtmlId);
		}
	}
	char *s = p.NewStr();
	if (s)
	{
		LgiTrace("Open tags = '%s'\n", s + 2);
		DeleteArray(s);
	}
}

COLOUR GTag::_Colour(bool f)
{
	for (GTag *t = this; t; t = t->Parent)
	{
		COLOUR *c = f ? &t->Fore : &t->Back;
		if (*c != GT_TRANSPARENT)
		{
			return *c;
		}

		if (!f && t->TagId == TAG_TABLE)
			break;
	}

	return GT_TRANSPARENT;
}

void GTag::CopyClipboard(GBytePipe &p)
{
	if (!Parent)
	{
		Selected = false;
	}

	int Min = -1;
	int Max = -1;

	if (Cursor >= 0 && Selection >= 0)
	{
		Min = min(Cursor, Selection);
		Max = max(Cursor, Selection);
	}
	else if (Selected)
	{
		Max = max(Cursor, Selection);
	}
	else
	{
		Min = max(Cursor, Selection);
	}

	int Off = -1;
	int Chars = 0;
	if (Min >= 0 && Max >= 0)
	{
		Off = Min;
		Chars = Max - Min + 1;
	}
	else if (Min >= 0)
	{
		Off = Min;
		Chars = StrlenW(Text) - Min;
		Selected = true;
	}
	else if (Max >= 0)
	{
		Off = 0;
		Chars = Max;
		Selected = false;
	}
	else if (Selected)
	{
		Off = 0;
		Chars = StrlenW(Text);
	}

	if (Off >= 0 && Chars > 0)
	{
		p.Write((uchar*) (Text + Off), Chars * sizeof(char16));
	}

	if (Selected)
	{
		switch (TagId)
		{
			case TAG_BR:
			{
				char16 NL[] = {'\n', 0};
				p.Write((uchar*) NL, sizeof(char16));
				break;
			}
			case TAG_P:
			{
				char16 NL[] = {'\n', '\n', 0};
				p.Write((uchar*) NL, sizeof(char16) * 2);
				break;
			}
		}
	}

	for (GTag *c = Tags.First(); c; c = Tags.Next())
	{
		c->CopyClipboard(p);
	}
}

void GTag::_Dump(GStringPipe &Buf, int Depth)
{
	char Tab[32];
	char s[1024];
	memset(Tab, '\t', Depth);
	Tab[Depth] = 0;

	char Trs[1024] = "";
	for (GFlowRect *Tr=TextPos.First(); Tr; Tr=TextPos.Next())
	{
		char *Utf8 = 0;
		Utf8 = LgiNewUtf16To8(Tr->Text, Tr->Len*sizeof(char16));
		if (Utf8)
		{
			int Len = strlen(Utf8);
			if (Len > 40)
			{
				Utf8[40] = 0;
			}
		}

		sprintf(Trs+strlen(Trs), "Tr(%i,%i,'%s') ", Tr->x1, Tr->y1, Utf8);
		DeleteArray(Utf8);
	}
	
	snprintf(s, sizeof(s)-1,
			"%sStartTag=%s%c%s (%i) Pos=%i,%i Size=%i,%i Color=%06.6X/%06.6X %s\r\n",
			Tab,
			TagId == CONTENT ? "Content" : Tag,
			HtmlId ? '#' : ' ',
			HtmlId ? HtmlId : (char*)"",
			WasClosed,
			Pos.x, Pos.y,
			Size.x, Size.y,
			Fore, Back,
			Trs);
	Buf.Push(s);

	for (GTag *t=Tags.First(); t; t=Tags.Next())
	{
		t->_Dump(Buf, Depth+1);
	}

	sprintf(s, "%sEnd '%s'\r\n", Tab, Tag);
	Buf.Push(s);
}

char *GTag::Dump()
{
	GStringPipe Buf;
	_Dump(Buf, 0);
	return (char*)Buf.New(1);
}

GFont *GTag::NewFont()
{
	GFont *f = new GFont;
	if (f)
	{
		GFont *Cur = GetFont();
		if (Cur) *f = *Cur;
		else *f = *Html->DefFont();
	}
	return f;
}

GFont *GTag::GetFont()
{
	GFont *f = 0;
	GTag *t = this;
	while (!t->Font && t->Parent)
	{
		t = t->Parent;
	}

	if (t->Font)
		f = t->Font;
	
	if (!f)
		f = Html->DefFont();

	if (!f || !ValidStr(f->Face()))
	{
		printf("%s:%i - Warning not a valid font face.\n", __FILE__, __LINE__);
	}
	
	return f;
}

GTag *GTag::PrevTag()
{
	if (Parent)
	{
		int i = Parent->Tags.IndexOf(this);
		if (i >= 0)
		{
			return Parent->Tags.ItemAt(i-1);
		}
	}

	return 0;
}

void GTag::Invalidate()
{
	GRect p = GetRect();
	for (GTag *t=Parent; t; t=t->Parent)
	{
		p.Offset(t->Pos.x, t->Pos.y);
	}
	Html->Invalidate(&p);
}

GTag *GTag::IsAnchor(char **Uri)
{
	GTag *a = 0;
	for (GTag *t = this; t; t = t->Parent)
	{
		if (t->TagId == TAG_A)
		{
			a = t;
			break;
		}
	}

	if (a && Uri)
	{
		char *u = 0;
		if (a->Get("href", u))
		{
			char16 *w = CleanText(u, strlen(u));
			if (w)
			{
				*Uri = LgiNewUtf16To8(w);
				DeleteArray(w);
			}
		}
	}

	return a;
}

bool GTag::OnMouseClick(GMouse &m)
{
	bool Processed = false;

	if (m.IsContextMenu())
	{
		char *Uri = 0;
		GTag *a = IsAnchor(&Uri);
		if (a)
		{
			if (ValidStr(Uri))
			{
				GSubMenu RClick;

				#define IDM_COPY_LINK	100
				if (Html->GetMouse(m, true))
				{
					int Id = 0;

					RClick.AppendItem(LgiLoadString(L_COPY_LINK_LOCATION, "&Copy Link Location"), IDM_COPY_LINK, Uri != 0);
					if (Html->GetEnv())
						Html->GetEnv()->AppendItems(&RClick);

					switch (Id = RClick.Float(Html, m.x, m.y))
					{
						case IDM_COPY_LINK:
						{
							GClipBoard Clip(Html);
							Clip.Text(Uri);
							break;
						}
						default:
						{
							if (Html->GetEnv())
							{
								Html->GetEnv()->OnMenu(Html, Id, a);
							}
							break;
						}
					}
				}

				Processed = true;
			}
			
			DeleteArray(Uri);
		}
	}
	else if (m.Down() && m.Left())
	{
		char *Uri = 0;
		if (Html &&
			(!Html->d->LinkDoubleClick || m.Double()) &&
			Html->Environment &&
			IsAnchor(&Uri))
		{
			Html->Environment->OnNavigate(Uri);
			DeleteArray(Uri);
			Processed = true;
		}
	}

	return Processed;
}

GTag *GTag::GetBlockParent(int *Idx)
{
	if (IsBlock)
	{
		if (Idx)
			*Idx = 0;

		return this;
	}

	for (GTag *t = this; t; t = t->Parent)
	{
		if (t->Parent->IsBlock)
		{
			if (Idx)
			{
				*Idx = t->Parent->Tags.IndexOf(t);
			}

			return t->Parent;
		}
	}

	return 0;
}

GTag *GTag::GetTagByName(char *Name)
{
	if (Name)
	{
		if (Tag && stricmp(Tag, Name) == 0)
		{
			return this;
		}

		for (GTag *t=Tags.First(); t; t=Tags.Next())
		{
			GTag *Result = t->GetTagByName(Name);
			if (Result) return Result;
		}
	}

	return 0;
}

int IsNearRect(GRect *r, int x, int y)
{
	if (r->Overlap(x, y))
	{
		return 0;
	}
	else if (x >= r->x1 && x <= r->x2)
	{
		if (y < r->y1)
			return r->y1 - y;
		else
			return y - r->y2;
	}
	else if (y >= r->y1 && y <= r->y2)
	{
		if (x < r->x1)
			return r->x1 - x;
		else
			return x - r->x2;
	}

	int dx = 0;
	int dy = 0;
	
	if (x < r->x1)
	{
		if (y < r->y1)
		{
			// top left
			dx = r->x1 - x;
			dy = r->y1 - y;
		}
		else
		{
			// bottom left
			dx = r->x1 - x;
			dy = y - r->y2;
		}
	}
	else
	{
		if (y < r->y1)
		{
			// top right
			dx = x - r->x2;
			dy = r->y1 - y;
		}
		else
		{
			// bottom right
			dx = x - r->x2;
			dy = y - r->y2;
		}
	}

	return sqrt( (dx * dx) + (dy * dy) );
}

int GTag::NearestChar(GFlowRect *Tr, int x, int y)
{
	GFont *f = GetFont();
	if (f)
	{
		GDisplayString ds(f, Tr->Text, Tr->Len);
		int c = ds.CharAt(x - Tr->x1);
		
		if (Tr->Text == PreText)
		{
			return 0;
		}
		else
		{
			char16 *t = Tr->Text + c;
			int Len = StrlenW(Text);
			if (t >= Text &&
				t <= Text + Len)
			{
				return SubtractPtr(t, Text) - GetTextStart();
			}
			else
			{
				printf("%s:%i - Error getting char at position.\n", _FL);
				// LgiAssert(0);
			}
		}
	}

	return -1;
}

bool GTag::GetTagByPos(int x, int y, GTagHit *Hit)
{
	bool Status = false;

	for (GTag *t=Tags.First(); t; t=Tags.Next())
	{
		if (t->GetTagByPos(x - t->Pos.x, y - t->Pos.y, Hit) &&
			Hit->Near == 0)
		{
			return true;
		}
	}

	if (TagId == TAG_IMG)
	{
		GRect r(0, 0, Size.x - 1, Size.y - 1);
		if (r.Overlap(x, y))
		{
			Hit->Hit = this;
			Hit->Block = 0;
			Hit->Near = 0;
			Hit->Index = -1;
			return true;
		}
	}
	else if (Text)
	{
		for (GFlowRect *Tr=TextPos.First(); Tr; Tr=TextPos.Next())
		{
			int n = IsNearRect(Tr, x, y);

			if (n == 0)
			{
				Hit->Hit = this;
				Hit->Block = Tr;
				Hit->Near = 0;
				Hit->Index = NearestChar(Tr, x, y);

				// LgiTrace("Direct Hit on '%s' @ pos %i\n", Tag, Hit->Index);
				return true;
			}
			else if (!Hit->Hit || (Hit->Hit && n < Hit->Near))
			{
				Hit->Hit = this;
				Hit->Block = Tr;
				Hit->Near = n;
				Hit->Index = NearestChar(Tr, x, y);

				// LgiTrace("Near Miss on '%s' @ pos %i and %i pixels\n", Tag, Hit->Index, Hit->Near);

				Status = true;
			}
		}
	}

	return Status;
}

void GTag::SetImage(char *Uri, GSurface *Img)
{
	if (Img)
	{
		Image = Img;
		
		for (GTag *t = this; t; t = t->Parent)
		{
			t->MinContent = 0;
			t->MaxContent = 0;
		}
	}
	else
	{
		Html->d->Loading.Add(Uri, this);
	}
}

void GTag::LoadImages()
{
	char *Uri = 0;
	if (Html->Environment &&
		!Image &&
		Get("src", Uri))
	{
		GSurface *s = 0;
		if (Html->Environment->GetImageUri(Uri, &s))
		{
			SetImage(Uri, s);
		}
	}

	for (GTag *t=Tags.First(); t; t=Tags.Next())
	{
		t->LoadImages();
	}
}

void GTag::ImageLoaded(char *uri, GSurface *Img, int &Used)
{
	char *Uri = 0;
	if (!Image &&
		Get("src", Uri))
	{
		if (strcmp(Uri, uri) == 0)
		{
			if (Used == 0)
			{
				SetImage(Uri, Img);
			}
			else
			{
				SetImage(Uri, new GMemDC(Img));
			}
			Used++;
		}
	}

	for (GTag *t=Tags.First(); t; t=Tags.Next())
	{
		t->ImageLoaded(uri, Img, Used);
	}
}

void GTag::SetStyle()
{
	const static float FntMul[] =
	{
		0.6,	// size=1
		0.89,	// size=2
		1.0,	// size=3
		1.2,	// size=4
		1.5,	// size=5
		2.0,	// size=6
		3.0		// size=7
	};

	GFont *Def = GetFont();

	char *Color = 0;
	if (Get("Color", Color))
	{
		ParseColour(Color, Fore);
	}

	char *Background = 0;
	if (Get("Background", Background) ||
		Get("bgcolor", Background))
	{
		ParseColour(Background, Back);
	}

	char *s;
	switch (TagId)
	{
		case TAG_BLOCKQUOTE:
		{
			MarginTop = 8;
			MarginBottom = 8;
			MarginLeft = 16;

			if (Get("Type", s))
			{
				if (stricmp(s, "cite") == 0)
				{
					BorderLeft.Set("1px solid blue");
					PaddingLeft.Set("0.5em");
					Fore = Rgb32(0x80, 0x80, 0x80);
				}
			}
			break;
		}
		case TAG_P:
		{
			MarginBottom.Set("1em");
			break;
		}
		case TAG_A:
		{
			Fore = Rgb32(0, 0, 255);
			GFont *f = new GFont(Font ? *Font : *Def);
			if (f)
			{
				f->Underline(true);
				Font = Html->FontCache->AddFont(f);
			}
			break;
		}
		case TAG_TABLE:
		{			
			MarginLeft = MarginTop = MarginRight = MarginBottom = CellSpacing;

			if (Get("border", s))
			{
				int b = atoi(s);
				BorderLeft = BorderRight = BorderTop = BorderBottom = b;
			}
			break;
		}
		case TAG_BODY:
		{
			PaddingLeft = DefaultBodyMargin;
			PaddingTop = DefaultBodyMargin;
			PaddingRight = DefaultBodyMargin;
			break;
		}
	}

	#ifdef _DEBUG
	if (Get("debug", s))
	{
		Debug = atoi(s);
	}
	#endif
	if (Get("width", s))
	{
		Width.Set(s);
	}
	if (Get("height", s))
	{
		Height.Set(s);
	}
	if (Get("align", s))
	{
		if (stricmp(s, "left") == 0) AlignX = AlignLeft;
		else if (stricmp(s, "right") == 0) AlignX = AlignRight;
		else if (stricmp(s, "center") == 0) AlignX = AlignCenter;
	}
	if (Get("valign", s))
	{
		if (stricmp(s, "top") == 0) AlignY = AlignTop;
		else if (stricmp(s, "middle") == 0) AlignY = AlignMiddle;
		else if (stricmp(s, "bottom") == 0) AlignY = AlignBottom;
	}

	char *Class = 0;
	if (Get("id", s))
	{
		DeleteArray(HtmlId);
		HtmlId = NewStr(s);
	}

	// Tag style
	char *Style = Html->CssMap[Tag];
	if (Style)
	{
		SetCssStyle(Style);
	}
	if (HtmlId)
	{
		char b[256];
		snprintf(b, sizeof(b)-1, "%s#%s", Tag, HtmlId);
		b[sizeof(b)-1] = 0;
		if (Style = Html->CssMap[b])
		{
			SetCssStyle(Style);
		}
	}
	if (Get("class", Class))
	{
		// Class stype
		char c[256];
		snprintf(c, sizeof(c)-1, ".%s", Class);
		c[sizeof(c)-1] = 0;
		Style = Html->CssMap[c];
		if (Style)
		{
			SetCssStyle(Style);
		}

		snprintf(c, sizeof(c)-1, "%s.%s", Tag, Class);
		Style = Html->CssMap[c];
		if (Style)
		{
			SetCssStyle(Style);
		}
	}
	if (Get("style", Style))
	{
		SetCssStyle(Style);
	}

	switch (TagId)
	{
		case TAG_META:
		{
			char *Cs = 0;

			char *s;
			if (Get("http-equiv", s) &&
				stricmp(s, "Content-Type") == 0)
			{
				char *ContentType;
				if (Get("content", ContentType))
				{
					char *CharSet = stristr(ContentType, "charset=");
					if (CharSet)
					{
						ParsePropValue(CharSet + 8, Cs);
					}
				}
			}

			if (Get("name", s) && stricmp(s, "charset") == 0)
			{
				if (Get("content", s))
				{
					Cs = NewStr(s);
				}
			}

			if (Cs)
			{
				if (Cs &&
					stricmp(Cs, "utf-16") != 0 &&
					stricmp(Cs, "utf-32") != 0 &&
					LgiGetCsInfo(Cs))
				{
					DeleteArray(Html->DocCharSet);
					Html->DocCharSet = NewStr(Cs);
				}

				DeleteArray(Cs);
			}
			break;
		}
		case TAG_BODY:
		{
			if (Back != GT_TRANSPARENT)
			{
				Html->SetBackColour(Back);
			}

			char *Back;
			if (Get("background", Back) && Html->Environment)
			{
				Html->Environment->GetImageUri(Back, &Image);
			}
			break;
		}
		case TAG_HEAD:
		{
			Visible = false;
			break;
		}
		case TAG_PRE:
		{
			GFontType Type;
			if (Type.GetSystemFont("Fixed"))
			{
				Font = Html->FontCache->AddFont(Type.Create());
			}
			break;
		}
		case TAG_OL:
		case TAG_UL:
		{
			MarginLeft = 16;
			break;
		}
		case TAG_TABLE:
		{
			char *s;
			if (Get("cellspacing", s))
			{
				CellSpacing = atoi(s);
			}
			if (Get("cellpadding", s))
			{
				CellPadding = atoi(s);
			}
			break;
		}
		case TAG_TD:
		case TAG_TR:
		{
			GTag *Table = GetTable();
			if (Table)
			{
				CellPadding = Table->CellPadding;
				CellSpacing = Table->CellSpacing;
			}

			if (TagId == TAG_TD)
			{
				char *s;
				if (Get("colspan", s)) Span.x = atoi(s);
				else Span.x = 1;
				if (Get("rowspan", s)) Span.y = atoi(s);
				else Span.y = 1;
			}
			break;
		}
		case TAG_IMG:
		{
			Size.x = DefaultImgSize;
			Size.y = DefaultImgSize;

			char *Uri;
			if (Html->Environment &&
				Get("src", Uri))
			{
				GSurface *s = 0;
				if (Html->Environment->GetImageUri(Uri, &s))
				{
					SetImage(Uri, s);
				}
			}
			break;
		}
		case TAG_H1:
		{
			Font = new GFont();
			if (Font)
			{
				*Font = *Def;
				Font->PointSize((float)Html->DefFont()->PointSize() * FntMul[5]);
				Font->Bold(true);
				Font->Create();
				Font = Html->FontCache->AddFont(Font);
			}
			break;
		}
		case TAG_H2:
		{
			Font = new GFont();
			if (Font)
			{
				*Font = *Def;
				Font->PointSize((float)Html->DefFont()->PointSize() * FntMul[4]);
				Font->Bold(true);
				Font->Create();
				Font = Html->FontCache->AddFont(Font);
			}
			break;
		}
		case TAG_H3:
		{
			Font = new GFont();
			if (Font)
			{
				*Font = *Def;
				Font->PointSize((float)Html->DefFont()->PointSize() * FntMul[3]);
				Font->Bold(true);
				Font->Create();
				Font = Html->FontCache->AddFont(Font);
			}
			break;
		}
		case TAG_H4:
		{
			Font = new GFont();
			if (Font)
			{
				*Font = *Def;
				Font->PointSize((float)Html->DefFont()->PointSize() * FntMul[2]);
				Font->Bold(true);
				Font->Create();
				Font = Html->FontCache->AddFont(Font);
			}
			break;
		}
		case TAG_H5:
		{
			Font = new GFont();
			if (Font)
			{
				*Font = *Def;
				Font->PointSize((float)Html->DefFont()->PointSize() * FntMul[1]);
				Font->Bold(true);
				Font->Create();
				Font = Html->FontCache->AddFont(Font);
			}
			break;
		}
		case TAG_H6:
		{
			Font = new GFont();
			if (Font)
			{
				*Font = *Def;
				int Pt = (float)Html->DefFont()->PointSize() * FntMul[0];
				Font->PointSize(max(Pt, 7));
				Font->Bold(true);
				Font->Create();
				Font = Html->FontCache->AddFont(Font);
			}
			break;
		}
		case TAG_FONT:
		{
			char *Face = 0;
			char *Size = 0;
			Get("Face", Face);
			Get("Size", Size);
			if (Face || Size)				
			{
				GFont *f = NewFont();
				if (f)
				{
					if (Face)
					{
						char16 *cw = CleanText(Face, strlen(Face), true);
						char *c8 = LgiNewUtf16To8(cw);
						DeleteArray(cw);
						GToken Faces(c8, ",");
						DeleteArray(c8);
						char *face = TrimStr(Faces[0]);
						if (ValidStr(face))
						{
							f->Face(face);
							DeleteArray(face);
						}
						else
						{
							LgiTrace("%s:%i - No face for font tag.\n", __FILE__, __LINE__);
						}
					}
					
					if (Size)
					{
						int Height = 0;
						
						if (strchr(Size, '+') || strchr(Size, '-'))
						{
							Height = f->PointSize() + atoi(Size);
						}
						else if (IsDigit(*Size))
						{
							if (strchr(Size, '%'))
							{
								int Percent = atoi(Size);
								if (Percent)
								{
									Height = f->PointSize() * Percent / 100;
								}
							}
							else if (stristr(Size, "pt"))
							{
								Height = atoi(Size);
							}
							else
							{
								int s = atoi(Size);
								
								if (s < 1) s = 1;
								if (s > 7) s = 7;

								int Sys = Html->DefFont()->PointSize();
								Height = ((float)Sys) * FntMul[s-1];
							}
						}
						else if (stricmp(Size, "smaller") == 0)
						{
							Height = f->PointSize() - 1;
						}
						else if (stricmp(Size, "larger") == 0)
						{
							Height = f->PointSize() + 1;
						}

						if (Height)
						{
							f->PointSize(max(Height, MinFontSize));
						}
					}
					
					if (!ValidStr(f->Face()))
					{
						f->Face(Html->DefFont()->Face());
					}

					// printf("FONT: %s,%i\n", f->Face(), f->PointSize());

					Font = Html->FontCache->AddFont(f);
				}
			}
			break;
		}
		case TAG_STRONG:	
		case TAG_B:
		{
			Font = new GFont(Font ? *Font : *Def);
			if (Font)
			{
				Font->Bold(true);
				Font = Html->FontCache->AddFont(Font);
			}
			break;
		}
		case TAG_I:
		{
			Font = new GFont(Font ? *Font : *Def);
			if (Font)
			{
				Font->Italic(true);
				Font = Html->FontCache->AddFont(Font);
			}
			break;
		}
		case TAG_U:
		{
			Font = new GFont(Font ? *Font : *Def);
			if (Font)
			{
				Font->Underline(true);
				Font = Html->FontCache->AddFont(Font);
			}
			break;
		}
	}
}

void GTag::SetCssStyle(char *Style)
{
	if (Style)
	{
		char *Comment = 0;
		while (Comment = strstr(Style, "/*"))
		{
			char *End = strstr(Comment+2, "*/");
			if (!End)
				break;
			for (char *c = Comment; c<=End+2; c++)
				*c = ' ';
		}

		GToken Vars(Style, ";");
		for (int v=0; v<Vars.Length(); v++)
		{
			GToken Var(Vars[v], ":");
			if (Var.Length() == 2)
			{
				char *VarName = TrimStr(Var[0]);
				if (VarName)
				{
					CssStyle Style = (CssStyle)(int)GHtmlStatic::Inst->StyleMap.Find(VarName);
					DeleteArray(VarName);
					char *Value = Var[1];
					while (*Value && strchr(" \t", *Value)) Value++;
					char *ValueEnd = Value + strlen(Value);
					while (ValueEnd > Value && strchr(" \t\r\n", ValueEnd[-1]))
					{
						ValueEnd[-1] = 0;
						ValueEnd--;
					}

					switch (Style)
					{
						case CSS_TEXT_ALIGN:
						{
							if (!stricmp(Value, "center"))
								AlignX = AlignCenter;
							else if (!stricmp(Value, "left"))
								AlignX = AlignLeft;
							else if (!stricmp(Value, "right"))
								AlignX = AlignRight;
							break;
						}
						case CSS_VERTICAL_ALIGN:
						{
							if (!stricmp(Value, "top"))
								AlignY = AlignTop;
							else if (!stricmp(Value, "middle"))
								AlignY = AlignMiddle;
							else if (!stricmp(Value, "bottom"))
								AlignY = AlignBottom;
							break;
						}
						case CSS_MARGIN:
						{
							GToken t(Value);
							if (t.Length() == 1)
							{
								MarginLeft.Set(Value);
								MarginRight = MarginTop = MarginBottom = MarginLeft;
							}
							else
							{
								if (t[0]) MarginTop.Set(t[0]);
								if (t[1]) MarginRight.Set(t[1]);
								if (t[2]) MarginBottom.Set(t[2]);
								if (t[3]) MarginLeft.Set(t[3]);
							}
							break;
						}
						case CSS_MARGIN_LEFT:
						{
							MarginLeft.Set(Value);
							break;
						}
						case CSS_MARGIN_RIGHT:
						{
							MarginRight.Set(Value);
							break;
						}
						case CSS_MARGIN_TOP:
						{
							MarginTop.Set(Value);
							break;
						}
						case CSS_MARGIN_BOTTOM:
						{
							MarginBottom.Set(Value);
							break;
						}

						case CSS_PADDING:
						{
							GToken t(Value);
							if (t.Length() == 1)
							{
								PaddingLeft.Set(Value);
								PaddingBottom = PaddingRight = PaddingTop = PaddingLeft;
							}
							else
							{
								if (t[0]) PaddingTop.Set(t[0]);
								if (t[1]) PaddingRight.Set(t[1]);
								if (t[2]) PaddingBottom.Set(t[2]);
								if (t[3]) PaddingLeft.Set(t[3]);
							}
							break;
						}
						case CSS_PADDING_LEFT:
						{
							PaddingLeft.Set(Value);
							break;
						}
						case CSS_PADDING_TOP:
						{
							PaddingTop.Set(Value);
							break;
						}
						case CSS_PADDING_RIGHT:
						{
							PaddingRight.Set(Value);
							break;
						}
						case CSS_PADDING_BOTTOM:
						{
							PaddingBottom.Set(Value);
							break;
						}

						case CSS_BORDER:
						{
							BorderLeft.Set(Value);
							BorderRight.Set(Value);
							BorderTop.Set(Value);
							BorderBottom.Set(Value);
							break;
						}
						case CSS_BORDER_LEFT:
						{
							BorderLeft.Set(Value);
							break;
						}
						case CSS_BORDER_TOP:
						{
							BorderTop.Set(Value);
							break;
						}
						case CSS_BORDER_RIGHT:
						{
							BorderRight.Set(Value);
							break;
						}
						case CSS_BORDER_BOTTOM:
						{
							BorderBottom.Set(Value);
							break;
						}

						case CSS_COLOUR:
						{
							if (TagId != TAG_TABLE)
							{
								ParseColour(Value, Fore);
							}
							break;
						}
						case CSS_BACKGROUND:
						case CSS_BACKGROUND_COLOUR:
						{
							bool Status = ParseColour(Value, Back);

							if (TagId == TAG_BODY)
							{
								if (Status)
								{
									Html->SetBackColour(Back);
								}

								if (Html->Environment)
								{
									Html->Environment->GetImageUri(Value, &Image);
								}
							}
							break;
						}
						case CSS_BACKGROUND_REPEAT:
						{
							if (stricmp(Value, "repeat") == 0)
							{
								BackgroundRepeat = BgRepeat;
							}
							else if (stricmp(Value, "repeat-x") == 0)
							{
								BackgroundRepeat = BgRepeatX;
							}
							else if (stricmp(Value, "repeat-y") == 0)
							{
								BackgroundRepeat = BgRepeatY;
							}
							else if (stricmp(Value, "no-repeat") == 0)
							{
								BackgroundRepeat = BgNoRepeat;
							}
							break;
						}
						case CSS_FONT:
						{
							GAutoPtr<GFont> f(NewFont());
							if (f && ValidStr(Value))
							{
								char *FaceType = 0;
								bool SetFace = false;
								
								GToken t(Value, " \t\r\n");
								for (int i=0; i<t.Length(); i++)
								{
									char *p = t[i];
									if (stricmp(p, "bold") == 0 || stricmp(p, "bolder") == 0)
									{
										f->Bold(true);
									}
									else if (stricmp(p, "italic") == 0)
									{
										f->Italic(true);
									}
									else if (stricmp(p, "oblique") == 0)
									{
										// Not supported
									}
									else if (stricmp(p, "normal") == 0)
									{
										f->Bold(false);
										f->Italic(false);
										f->Underline(false);
									}
									else if (stricmp(p, "small-caps") == 0)
									{
										// Not supported
									}
									else if (stricmp(p, "serif") == 0 ||
											stricmp(p, "sans-serif") == 0 ||
											stricmp(p, "cursive") == 0 ||
											stricmp(p, "fantasy") == 0 ||
											stricmp(p, "monospace") == 0)
									{
										FaceType = p;
									}
									else if (isdigit(*p))
									{
										// Size?
										if (stristr(p, "pt"))
										{
											f->PointSize(atoi(p));
										}
										
										// Bolding?
									}
									else if (isalpha(*p))
									{
										char *s = TrimStr(p, ",");
										if (s)
										{
											// Face name?
											f->Face(s);
											SetFace = true;
											DeleteArray(s);
										}
									}
								}
								
								if (FaceType && !SetFace)
								{
									// Yes this is a hack..
									if (stricmp(FaceType, "monospace") == 0)
										f->Face("courier");
									else if (stricmp(FaceType, "serif") == 0)
										f->Face("times");
									else if (stricmp(FaceType, "sans-serif") == 0)
										f->Face("arial");
								}

								Font = Html->FontCache->AddFont(f.Release());
							}
							break;
						}
						case CSS_FONT_FAMILY:
						{
							GAutoPtr<GFont> f(NewFont());
							if (f && ValidStr(Value))
							{
								GToken t(Value, ",");
								for (int i=0; i<t.Length(); i++)
								{
									char *p = t[i];
									if (stricmp(p, "serif") == 0 ||
										stricmp(p, "sans-serif") == 0 ||
										stricmp(p, "cursive") == 0 ||
										stricmp(p, "fantasy") == 0 ||
										stricmp(p, "monospace") == 0)
									{
									}
									else
									{
										char *s = TrimStr(p);
										if (s)
										{
											f->Face(s);
											DeleteArray(s);
											if (f->Create(0, -1, 0))
												break;
										}
									}
								}

								Font = Html->FontCache->AddFont(f.Release());
							}
							break;
						}
						case CSS_FONT_SIZE:
						{
							GAutoPtr<GFont> f(NewFont());
							if (f)
							{
								char Units[64] = "";
								float Dist = 0;
								
								if (ParseDistance(Value, Dist, Units))
								{
									if (strchr(Units, '%'))
									{
									}
									else if (stricmp(Units, "px") == 0)
									{
										f->PointSize(Dist * 0.8);
									}
									else
									{
										f->PointSize(Dist);
									}

									Font = Html->FontCache->AddFont(f.Release());
								}
							}
							break;
						}
						case CSS_FONT_WEIGHT:
						{
							GAutoPtr<GFont> f(NewFont());
							if (f)
							{
								if (stristr(Value, "bold"))
								{
									f->Bold(true);
								}

								Font = Html->FontCache->AddFont(f.Release());
							}
							break;
						}
						case CSS_FONT_STYLE:
						{
							GAutoPtr<GFont> f(NewFont());
							if (f)
							{
								if (stricmp(Value, "normal") == 0)
								{
									f->Bold(false);
									f->Italic(false);
									f->Underline(false);
								}
								else if (stricmp(Value, "italic") == 0)
								{
									f->Italic(true);
								}

								Font = Html->FontCache->AddFont(f.Release());
							}
							break;
						}
						case CSS_WIDTH:
						{
							Width.Set(Value);
							break;
						}
						case CSS_HEIGHT:
						{
							Height.Set(Value);
							break;
						}
					}
				}
			}
		}
	}
}

char16 *GTag::CleanText(char *s, int Len, bool ConversionAllowed, bool KeepWhiteSpace)
{
	static char *DefaultCs = "iso-8859-1";
	char16 *t = 0;

	#if 0

	if (s && Len > 0)
	{
		bool DocAndCsTheSame = false;
		if (Html->DocCharSet && Html->Charset)
		{
			DocAndCsTheSame = stricmp(Html->DocCharSet, Html->Charset) == 0;
		}

		bool Has8 = false;
		bool Has16 = false;
		GArray<char16> Out;
		if (ConversionAllowed)
		{
			for (char *i = s; i < s + Len && *i; )
			{
				switch (*i)
				{
					case '&':
					{
						i++;
						
						char *e;
						for (e = i; *e && *e != ';'; e++);
						char16 *Var = LgiNewUtf8To16(i, SubtractPtr(e, i));
						if (Var)
						{
							char16 Char = 0;
							if (*Var == '#')
							{
								// Unicode Number
								Char = atoi(i + 1);
								i = e + 1;
							}
							else
							{
								// Named Char
								Char = Static->VarMap[Var];
								if (Char)
								{
									i = e + 1;
								}
								else
								{
									printf("%s:%i - Convert '%S' failed out of %i values.\n",
										__FILE__, __LINE__, Var, Static->VarMap.Length());
									Char = i[-1];
								}
							}

							if (Char)
							{
								if (Char >= 0x80)
									Has8 = true;
								if (Char >= 0x100)
									Has16 = true;

								Out.Add(Char);
							}

							DeleteArray(Var);
						}
						break;
					}
					case ' ':
					case '\t':
					case '\n':
					{
						if (KeepWhiteSpace)
						{
							Out.Add(*i++);
							break;
						}
						// else fall thru
					}
					case '\r':
					{
						// Whitespace
						Out.Add(' ');
						while (*i && IsWhiteSpace(*i))
						{
							i++;
						}
						break;
					}
					default:
					{
						// Normal char

						/*
						char *Start = i;
						while (*i &&
								*i != '&' &&
								*i != ' ' &&
								*i != '\t' &&
								*i != '\r' &&
								*i != '\n' &&
								i < s + Len)
						{
							i++;
						}

						int Bytes = i - Start;
						char16 *w = 0;
						if (Html->DocCharSet && !Html->OverideDocCharset)
						{
							char *DocText = (char*)LgiNewConvertCp(Html->DocCharSet, Start, Html->Charset ? Html->Charset : (char*)"utf-8", Bytes);
							if (DocText)
							{
								w = (char16*) LgiNewConvertCp(LGI_W_CHARSET, DocText, Html->DocCharSet, -1);
								DeleteArray(DocText);
							}
							else goto NormalText;
						}
						else
						{
							NormalText:
							w = (char16*) LgiNewConvertCp(LGI_W_CHARSET, Start, Html->Charset ? Html->Charset : (char*)"iso-8859-1", Bytes);
						}

						for (char16 *n = w; n && *n; n++)
						{
							Out.Add(*n);
						}
						*/

						if (*i >= 0x80)
							Has8 = true;

						Out.Add(*i++);
						break;
					}
				}
			}
		}

		if (Has16)
		{
			t = NewStrW(&Out[0], Out.Length());
		}
		else
		{
			GArray<uchar> Out8;
			Out8.Length(Out.Length() + 1);
			for (int o=0; o<Out.Length(); o++)
				Out8[o] = Out[o];
			Out8.Add(0);

			char *Start = (char*) &Out8[0];
			int Bytes = Out8.Length();
			if (Html->DocCharSet && !Html->OverideDocCharset && Html->Charset)
			{
				#if 0
				
				char *DocText = (char*)LgiNewConvertCp(Html->Charset, Start, Html->DocCharSet ? Html->DocCharSet : DefaultCs, Bytes);
				if (DocText)
				{
					t = (char16*) LgiNewConvertCp(LGI_W_CHARSET, DocText, Html->Charset, -1);
					DeleteArray(DocText);
				}
				else goto NormalText;

				#else

				char *DocText = (char*)LgiNewConvertCp(Html->DocCharSet, Start, Html->Charset ? Html->Charset : DefaultCs, Bytes);
				if (DocText)
				{
					t = (char16*) LgiNewConvertCp(LGI_W_CHARSET, DocText, Html->DocCharSet, -1);
					DeleteArray(DocText);
				}
				else goto NormalText;

				#endif
			}
			else
			{
				NormalText:
				t = (char16*) LgiNewConvertCp(LGI_W_CHARSET, Start, Html->Charset ? Html->Charset : DefaultCs, Bytes);
			}
		}
	}

	#else

	if (s && Len > 0)
	{
		bool Has8 = false;
		if (Len >= 0)
		{
			for (int n = 0; n < Len; n++)
			{
				if (s[n] & 0x80)
				{
					Has8 = true;
					break;
				}
			}
		}
		else
		{
			for (int n = 0; s[n]; n++)
			{
				if (s[n] & 0x80)
				{
					Has8 = true;
					break;
				}
			}
		}
		
		bool DocAndCsTheSame = false;
		if (Html->DocCharSet && Html->Charset)
		{
			DocAndCsTheSame = stricmp(Html->DocCharSet, Html->Charset) == 0;
		}

		if (Html->DocCharSet &&
			Html->Charset &&
			!Html->OverideDocCharset)
		{
			char *DocText = (char*)LgiNewConvertCp(Html->DocCharSet, s, Html->Charset, Len);
			t = (char16*) LgiNewConvertCp(LGI_WideCharset, DocText, Html->DocCharSet, -1);
			DeleteArray(DocText);
		}
		else if (Html->DocCharSet)
		{
			t = (char16*) LgiNewConvertCp(LGI_WideCharset, s, Html->DocCharSet, Len);
		}
		else
		{
			t = (char16*) LgiNewConvertCp(LGI_WideCharset, s, Html->Charset ? Html->Charset : DefaultCs, Len);
		}

		if (t && ConversionAllowed)
		{
			char16 *o = t;
			for (char16 *i=t; *i; )
			{
				switch (*i)
				{
					case '&':
					{
						i++;
						
						char16 *e;
						for (e = i; *e && *e != ';'; e++);
						char16 *Var = NewStrW(i, SubtractPtr(e, i));
						if (Var)
						{
							if (*Var == '#')
							{
								// Unicode Number
								char n[32] = "";
								void *In = Var + 1;
								int Len = StrlenW(Var + 1) * sizeof(char16);
								if (LgiBufConvertCp(n, "iso-8859-1", sizeof(n), In, "utf-16", Len))
								{
									char16 Ch = atoi(n);
									*o++ = Ch;
								}
								i = e;
							}
							else
							{
								// Named Char
								char16 Char = GHtmlStatic::Inst->VarMap.Find(Var);
								if (Char)
								{
									*o++ = Char;
									i = e;
								}
								else
								{
									i--;
									*o++ = *i;
								}
							}

							DeleteArray(Var);
						}
						break;
					}
					case '\r':
					{
						break;
					}
					case ' ':
					case '\t':
					case '\n':
					{
						if (KeepWhiteSpace)
						{
							*o++ = *i;
						}
						else
						{
							*o++ = ' ';

							// Skip furthur whitespace
							while (i[1] && IsWhiteSpace(i[1]))
							{
								i++;
							}
						}
						break;
					}
					default:
					{
						// Normal char
						*o++ = *i;
						break;
					}
				}

				if (*i) i++;
				else break;
			}
			*o++ = 0;
		}
	}

	#endif

	if (t && StrlenW(t) == 0)
	{
		DeleteArray(t);
	}

	return t;
}

char *GTag::ParseText(char *Doc)
{
	Back = LC_WORKSPACE;
	TagId = TAG_BODY;
	Tag = NewStr("body");
	Info = GetTagInfo(Tag);
	char *OriginalCp = NewStr(Html->Charset);

	Html->SetBackColour(LC_WORKSPACE);
	
	GStringPipe Utf16;
	char *s = Doc;
	while (true)
	{
		if (*s == '\r')
		{
			s++;
		}
		else if (*s == '<')
		{
			// Process tag
			char *e = s;
			e++;
			while (*e && *e != '>')
			{
				if (*e == '\"' || *e == '\'')
				{
					char *q = strchr(e + 1, *e);
					if (q) e = q + 1;
					else e++;
				}
				else e++;
			}
			if (*e == '>') e++;

			// Output tag
			Html->SetCharset("iso-8859-1");
			char16 *t = CleanText(s, (int)e-(int)s, false);
			if (t)
			{
				Utf16.Push(t);
				DeleteArray(t);
			}
			s = e;
		}
		else if (!*s || *s == '\n')
		{
			// Output previous line
			char16 *Line = Utf16.NewStrW();
			if (Line)
			{
				GTag *t = new GTag(Html, this);
				if (t)
				{
					t->Fore = LC_TEXT;
					t->Text = Line;
				}
			}

			if (*s == '\n')
			{
				s++;
				GTag *t = new GTag(Html, this);
				if (t)
				{
					t->TagId = TAG_BR;
					t->Tag = NewStr("br");
					t->Info = GetTagInfo(t->Tag);
				}
			}
			else break;
		}
		else
		{
			// Seek end of text
			char *e = s;
			while (*e && *e != '\r' && *e != '\n' && *e != '<') e++;
			
			// Output text
			Html->SetCharset(OriginalCp);
			char16 *t = CleanText(s, (int)e-(int)s, false);
			if (t)
			{
				Utf16.Push(t);
				DeleteArray(t);
			}			
			s = e;
		}
	}
	
	Html->SetCharset(OriginalCp);
	DeleteArray(OriginalCp);

	return 0;
}

char *GTag::NextTag(char *s)
{
	while (s && *s)
	{
		char *n = strchr(s, '<');
		if (n)
		{
			if (isalpha(n[1]) || strchr("!/", n[1]) || n[1] == '?')
			{
				return n;
			}

			s = n + 1;
		}
		else break;
	}

	return 0;
}

char *GTag::ParseHtml(char *Doc, int Depth, bool InPreTag, bool *BackOut)
{
	#if CRASH_TRACE
	LgiTrace("::ParseHtml Doc='%.10s'\n", Doc);
	#endif

	if (Depth >= 1024)
	{
		// Bail
		return Doc + strlen(Doc);
	}

	bool IsFirst = true;
	for (char *s=Doc; s && *s; )
	{
		char *StartTag = s;

		if (*s == '<')
		{
			if (s[1] == '?')
			{
				// Dynamic content
				s += 2;
				while (*s && IsWhiteSpace(*s)) s++;

				if (strnicmp(s, "xml:namespace", 13) == 0)
				{
					// Ignore Outlook generated HTML tag
					char *e = strchr(s, '/');
					while (e)
					{
						if (e[1] == '>'
							||
							(e[1] == '?' && e[2] == '>'))
						{
							if (e[1] == '?')
								s = e + 3;
							else
								s = e + 2;
							break;
						}

						e = strchr(e + 1, '/');
					}

					if (!e)
						LgiAssert(0);
				}
				else
				{
					char *Start = s;
					while (*s && (!(s[0] == '?' && s[1] == '>')))
					{
						if (strchr("\'\"", *s))
						{
							char d = *s++;
							s = strchr(s, d);
							if (s) s++;
							else break;
						}
						else s++;
					}

					if (s)
					{
						if (s[0] == '?' &&
							s[1] == '>' &&
							Html->Environment)
						{
							char *e = s - 1;
							while (e > Start && IsWhiteSpace(*e)) e--;
							e++;
							char *Code = NewStr(Start, (int)e-(int)Start);
							if (Code)
							{
								char *Result = Html->Environment->OnDynamicContent(Code);
								if (Result)
								{
									char *p = Result;
									do
									{
										GTag *c = new GTag(Html, this);
										if (c)
										{
											p = c->ParseHtml(p, Depth + 1, InPreTag);
										}
										else break;
									}
									while (ValidStr(p));

									DeleteArray(Result);
								}

								DeleteArray(Code);
							}

							s += 2;
						}
					}
				}
			}
			else if (s[1] == '!' &&
					s[2] == '-' &&
					s[3] == '-')
			{
				// Comment
				s = strstr(s, "-->");
				if (s) s += 3;
			}
			else if (s[1] != '/')
			{
				// Start tag
				if (Parent && IsFirst)
				{
					// My tag
					s = ParseName(++s, &Tag);

					bool TagClosed = false;
					s = ParsePropList(s, this, TagClosed);

					if (Tag)
					{
						if (stricmp("th", Tag) == 0)
						{
							DeleteArray(Tag);
							Tag = NewStr("td");
						}
						
						Info = GetTagInfo(Tag);
						if (Info)
						{
							TagId = Info->Id;
							IsBlock = Info->Flags & TI_BLOCK;
							if (TagId == TAG_PRE)
							{
								InPreTag = true;
							}
						}
						
						if (TagId == TAG_SCRIPT)
						{
							char *End = stristr(s, "</script>");
							if (End)
							{
								if (Html->Environment)
								{
									*End = 0;
									char *Lang = 0, *Type = 0;
									Get("language", Lang);
									Get("type", Type);
									Html->Environment->OnCompileScript(s, Lang, Type);
									*End = '<';
								}

								s = End;
							}
						}

						if (TagId == TAG_TABLE &&
							Parent->TagId == TAG_TABLE)
						{
							// Um no...
							if (BackOut)
							{
								GTag *l = Html->OpenTags.Last();
								if (l && l->TagId == TAG_TABLE)
								{
									Html->OpenTags.Delete(l);
								}

								*BackOut = true;
								return StartTag;
							}
						}
						
						SetStyle();

						if (TagId == TAG_STYLE)
						{
							char *End = stristr(s, "</style>");
							if (End)
							{
								char *Css = NewStr(s, SubtractPtr(End, s));
								if (Css)
								{
									Html->AddCss(Css);
								}

								s = End;
							}
							else
							{
								// wtf?
							}
						}

						/* FIXME???
						if (TagId == TAG_P)
						{
							GTag *p;
							if (p = Html->GetOpenTag("p"))
							{
								return s;
							}
						}
						*/

						if (TagClosed || Info->NeverCloses())
						{
							return s;
						}

						if (Info->ReattachTo)
						{
							GToken T(Info->ReattachTo, ",");
							char *Reattach = Info->ReattachTo;
							for (int i=0; i<T.Length(); i++)
							{
								if (Parent->Tag &&
									stricmp(Parent->Tag, T[i]) == 0)
								{
									Reattach = 0;
									break;
								}
							}

							if (Reattach)
							{
								if (TagId == TAG_HEAD)
								{
									// Ignore it..
									return s;
								}
								else
								{
									GTag *Last = 0;
									for (GTag *t=Html->OpenTags.Last(); t; t=Html->OpenTags.Prev())
									{
										if (t->Tag)
										{
											if (stricmp(t->Tag, Tag) == 0)
											{
												Html->OpenTags.Delete(t);
												t = Html->OpenTags.Current();
												if (!t) t = Html->OpenTags.Last();
											}

											if (t && t->Tag && stricmp(t->Tag, Reattach) == 0)
											{
												Last = t;
												break;
											}
										}

										if (!t || t->TagId == TAG_TABLE)
											break;
									}

									if (Last)
									{
										Last->Attach(this);
									}
								}
							}
						}

						Html->OpenTags.Insert(this);
					}
				}
				else
				{
					// Child tag
					GTag *c = new GTag(Html, this);
					if (c)
					{
						bool BackOut = false;

						s = c->ParseHtml(s, Depth + 1, InPreTag, &BackOut);
						if (BackOut)
						{
							return s;
						}
					}
				}
			}
			else
			{
				// End tag
				char *PreTag = s;
				s += 2;
				if (Tag)
				{
					// This code segment detects out of order HTML tags
					// and skips them. If we didn't do this then the parser
					// would get stuck on a Tag which has already been closed
					// and would return to the very top of the recursion.
					//
					// e.g.
					//		<font>
					//			<b>
					//			</font>
					//		</b>
					char *e = strchr(s, '>');
					if (e)
					{
						*e = 0;
						GTag *Open = Html->GetOpenTag(s);
						*e = '>';
						if (Open)
						{
							Open->WasClosed = true;
						}
						else
						{
							s = e + 1;
							continue;
						}
					}

					// Compare against our tag
					char *t = Tag;
					while (*s && *t && toupper(*s) == toupper(*t))
					{
						s++;
						t++;
					}

					if (*s == '>')
					{
						GTag *t;
						while (t = Html->OpenTags.Last())
						{
							Html->OpenTags.Delete(t);
							if (t == this)
							{
								break;
							}
						}
						s++;
						if (Parent)
						{
							return s;
						}
					}
				}

				if (Parent)
				{
					return PreTag;
				}
			}
		}
		else if (*s)
		{
			// Text child
			char *n = NextTag(s);
			int Len = n ? SubtractPtr(n, s) : strlen(s);
			GAutoWString Txt(CleanText(s, Len, true, InPreTag));
			if (Txt)
			{
				if (ValidStrW(Txt))
				{
					if (InPreTag)
					{
						for (char16 *s=Txt; s && *s; )
						{
							char16 *e = StrchrW(s, '\n');
							if (!e) e = s + StrlenW(s);
							int Chars = ((int)e - (int)s)/sizeof(char16);

							GTag *c = new GTag(Html, this);
							if (c)
							{
								c->Text = NewStrW(s, Chars);
								
								if (*e == '\n')
								{
									GTag *c = new GTag(Html, this);
									if (c)
									{
										c->Tag = NewStr("br");
										c->Info = GetTagInfo(c->Tag);
										if (c->Info)
										{
											c->TagId = c->Info->Id;
										}
									}
								}
							}

							s = *e ? e + 1 : e;
						}
					}
					else if (Tags.Length() == 0 &&
							(!Info || !Info->NoText()) &&
							!Text)
					{
						Text = Txt.Release();
					}
					else
					{
						GTag *c = new GTag(Html, this);
						if (c)
						{
							c->Text = Txt.Release();
						}
					}
				}
			}

			s = n;
		}

		IsFirst = false;
	}

	#if CRASH_TRACE
	LgiTrace("::ParseHtml end\n");
	#endif

	return 0;
}

void GTag::ZeroTableElements()
{
	if (TagId == TAG_TABLE ||
		TagId == TAG_TR ||
		TagId == TAG_TD)
	{
		Size.x = 0;
		Size.y = 0;
		MinContent = 0;
		MaxContent = 0;

		for (GTag *t = Tags.First(); t; t = Tags.Next())
		{
			t->ZeroTableElements();
		}
	}
}

GdcPt2 GTag::GetTableSize()
{
	GdcPt2 s(0, 0);
	
	if (Cells)
	{
		Cells->GetSize(s.x, s.y);
	}

	return s;
}

GTag *GTag::GetTableCell(int x, int y)
{
	GTag *t = this;
	while (	t &&
			!t->Cells &&
			t->Parent)
	{
		t = t->Parent;
	}
	
	if (t && t->Cells)
	{
		return t->Cells->Get(x, y);
	}

	return 0;
}

// This function gets the largest and smallest peice of content
// in this cell and all it's children.
bool GTag::GetWidthMetrics(uint16 &Min, uint16 &Max)
{
	bool Status = true;

	// Break the text into words and measure...
	if (Text)
	{
		int MinContent = 0;
		int MaxContent = 0;
		
		GFont *f = GetFont();
		if (f)
		{
			for (char16 *s = Text; s && *s; )
			{
				// Skip whitespace...
				while (*s && StrchrW(WhiteW, *s)) s++;
				
				// Find end of non-whitespace
				char16 *e = s;
				while (*e && !StrchrW(WhiteW, *e)) e++;
				
				// Find size of the word
				int Len = e - s;
				GDisplayString ds(f, s, Len);
				int X = ds.X();
				MinContent = max(MinContent, X);
				MaxContent += X + 4;
				
				// Move to the next word.
				s = (*e) ? e + 1 : 0;
			}
		}
		
		Min = max(Min, MinContent) + MarginLeft + MarginRight + (CellPadding << 1);
		Max = max(Max, MaxContent) + MarginLeft + MarginRight + (CellPadding << 1);
	}

	// Specific tag handling?
	switch (TagId)
	{
		case TAG_IMG:
		{
			if (Width.IsValid())
			{
				int w = Width;
				Min = max(Min, w);
				Max = max(Max, w);
			}
			else if (Image)
			{
				Min = Max = Image->X();
			}
			else
			{
				Min = max(Min, Size.x);
				Max = max(Max, Size.x);
			}
			break;
		}
		case TAG_TD:
		{
			if (Width.IsValid())
			{
				if (Width.IsDynamic())
				{
					Min = max(Min, Width);
					Max = max(Max, Width);
				}
				else
				{
					int w = Width;
					Min = Max = w;
				}
			}
			break;
		}
		case TAG_TABLE:
		{
			if (Width.IsValid() && !Width.IsDynamic())
			{
				// Fixed width table...
				int w = Width;
				Min = Max = w;
				return true;
			}
			else
			{
				//int Border = CellSpacing + BorderLeft + BorderRight;
				GdcPt2 s;
				GCellStore c(this);
				c.GetSize(s.x, s.y);

				// Auto layout table
				GArray<int> ColMin, ColMax;
				for (int y=0; y<s.y; y++)
				{
					for (int x=0; x<s.x; x++)
					{
						GTag *t = c.Get(x, y);
						if (t)
						{
							uint16 a = 0, b = 0;
							if (t->GetWidthMetrics(a, b))
							{
								ColMin[x] = max(ColMin[x], a);
								ColMax[x] = max(ColMax[x], b);
							}
							
							x += t->Span.x;
						}
						else break;
					}
				}
				
				int MinSum = 0, MaxSum = 0;
				for (int i=0; i<s.x; i++)
				{
					MinSum += ColMin[i];
					MaxSum += ColMax[i];
				}
				
				Min = max(Min, MinSum);
				Max = max(Max, MaxSum);
				return true;
			}
			break;
		}
	}

	GTag *c;
	if (c = Tags.First())
	{
		uint16 Width = 0;
		for (; c; c = Tags.Next())
		{
			uint16 x = 0;
			Status &= c->GetWidthMetrics(Min, x);
			if (c->TagId == TAG_BR)
			{
				Max = max(Max, Width);
				Width = 0;
			}
			else
			{
				Width += x;
			}
		}
		Max = max(Max, Width);
	}

	switch (TagId)
	{
		case TAG_TD:
		{
			Min += CellPadding << 1;
			Max += CellPadding << 1;
			break;
		}
	}
	
	return Status;
}

void DistributeSize(GArray<int> &a, int Start, int Span, int Size, int Border)
{
	// Calculate the current size of the cells
	int Cur = -Border;
	for (int i=0; i<Span; i++)
	{
		Cur += a[Start+i] + Border;
	}

	// Is there more to allocate?
	if (Cur < Size)
	{
		// Distribute size amongst the rows
		int Needs = Size - Cur;
		int Part = Needs / Span;
		for (int k=0; k<Span; k++)
		{
			int Add = k < Span - 1 ? Part : Needs;
			a[Start+k] = a[Start+k] + Add;
			Needs -= Add;
		}
	}
}

template <class T>
T Sum(GArray<T> &a)
{
	T s = 0;
	for (int i=0; i<a.Length(); i++)
		s += a[i];
	return s;
}

#define DEBUG_LAYOUT	0
void GTag::LayoutTable(GFlowRegion *f)
{
	GdcPt2 s;

	if (!Cells)
	{
		Cells = new GCellStore(this);
		#if DEBUG_LAYOUT
		if (Cells && Debug)
			Cells->Dump();
		#endif
	}
	if (Cells)
	{
		Cells->GetSize(s.x, s.y);
		if (s.x == 0 || s.y == 0)
		{
			return;
		}

		ZeroTableElements();
		
		// New table layout code
		BorderLeft.Get(f, Font, true);
		BorderTop.Get(f, Font, true);
		BorderRight.Get(f, Font, true);
		BorderBottom.Get(f, Font, true);

		int AvailableX = Width.IsValid() ? Width.Get(f, Font, true) : f->X();
		#if DEBUG_LAYOUT
		if (Debug)
			LgiTrace("AvailableX=%i\n", AvailableX);
		#endif

		// The col and row sizes
		GArray<int> MinCol, MaxCol, MaxRow;
		GArray<bool> FixedCol;
		
		// Size detection pass
		int y;
		for (y=0; y<s.y; y++)
		{
			for (int x=0; x<s.x; )
			{
				GTag *t = Cells->Get(x, y);
				if (t)
				{
					if (t->Cell.x == x && t->Cell.y == y)
					{
						if (!t->Width.IsDynamic() &&
							!t->MinContent &&
							!t->MaxContent)
						{
							if (FixedCol.Length() < x)
							{
								FixedCol[x] = false;
							}
							
							if (t->Width.IsValid())
							{
								t->MinContent = t->MaxContent = t->Width.Get(f, Font, true);
								FixedCol[x] = true;
							}
							else if (!t->GetWidthMetrics(t->MinContent, t->MaxContent))
							{
								t->MinContent = 16;
								t->MaxContent = 16;
							}
							
							#if DEBUG_LAYOUT
							if (Debug)
								LgiTrace("Content[%i,%i] min=%i max=%i\n", x, y, t->MinContent, t->MaxContent);
							#endif
						}

						if (t->Span.x == 1)
						{
							MinCol[x] = max(MinCol[x], t->MinContent);
							MaxCol[x] = max(MaxCol[x], t->MaxContent);
						}
					}
					
					x += t->Span.x;
				}
				else break;
			}
		}
		
		// How much space used so far?
		int TotalX = BorderLeft + BorderRight + CellSpacing;
		int x;
		for (x=0; x<s.x; x++)
		{
			TotalX += MinCol[x] + CellSpacing;
		}

		#if DEBUG_LAYOUT
		if (Debug)
			LgiTrace("Detect: TotalX=%i\n", TotalX);
		#endif
		
		// Process dynamic width cells
		GArray<float> Percents;
		for (y=0; y<s.y; y++)
		{
			for (int x=0; x<s.x; )
			{
				GTag *t = Cells->Get(x, y);
				if (t)
				{
					if (t->Cell.x == x && t->Cell.y == y)
					{
						if (t->Width.IsDynamic() &&
							!t->MinContent &&
							!t->MaxContent)
						{
							if (t->Width.GetUnits() == LengthPercentage)
							{
								Percents[t->Cell.x] = max(t->Width.GetRaw(), Percents[t->Cell.x]);
							}
							float Total = Sum<float>(Percents);
							if (Total > 100.0)
							{
								// Yarrrrh. The web be full of incongruity.
								float Sub = Total - 100.0;
								Percents[Percents.Length()-1] -= Sub;

								char p[32];
								sprintf(p, "%.1f%%", Percents[Percents.Length()-1]);
								t->Width.Set(p);
							}

							t->GetWidthMetrics(t->MinContent, t->MaxContent);
							
							int x = t->Width.Get(f, Font, true);
							t->MinContent = max(x, t->MinContent);
							t->MaxContent = max(x, t->MaxContent);
							
							#if DEBUG_LAYOUT
							if (Debug)
								LgiTrace("DynWidth [%i,%i] = %i->%i\n", t->Cell.x, t->Cell.y, t->MinContent, t->MaxContent);
							#endif
						}
						else
						{
							uint16 Min = t->MinContent;
							uint16 Max = t->MaxContent;
							t->GetWidthMetrics(Min, Max);
							if (Min > t->MinContent)
								t->MinContent = Min;
							if (Min > t->MaxContent)
								t->MaxContent = Min;
						}

						if (t->Span.x == 1)
						{
							MinCol[x] = max(MinCol[x], t->MinContent);
							MaxCol[x] = max(MaxCol[x], t->MaxContent);
						}
					}
					
					x += t->Span.x;
				}
				else break;
			}
		}

		TotalX = BorderLeft + BorderRight;
		for (x=0; x<s.x; x++)
		{
			TotalX += MinCol[x] + CellSpacing;
		}

		#if DEBUG_LAYOUT
		if (Debug)
			LgiTrace("Dynamic: TotalX=%i\n", TotalX);
		
		#define DumpCols() \
		if (Debug) \
		{ \
			LgiTrace("%s:%i - Columns TotalX=%i AvailableX=%i\n", _FL, TotalX, AvailableX); \
			for (int i=0; i<MinCol.Length(); i++) \
			{ \
				LgiTrace("\t[%i] = %i/%i\n", i, MinCol[i], MaxCol[i]); \
			} \
		}

		DumpCols();
		#else
		#define DumpCols()
		#endif

		// Process spanned cells
		if (TotalX < AvailableX)
		{
			for (y=0; y<s.y; y++)
			{
				for (int x=0; x<s.x; )
				{
					GTag *t = Cells->Get(x, y);
					if (t && t->Cell.x == x && t->Cell.y == y)
					{
						if (t->Span.x > 1 || t->Span.y > 1)
						{
							int i;
							int ColMin = -CellSpacing;
							int ColMax = -CellSpacing;
							for (i=0; i<t->Span.x; i++)
							{
								ColMin += MinCol[x + i] + CellSpacing;
								ColMax += MaxCol[x + i] + CellSpacing;
							}

							// Generate an array of unfixed column indexes
							GArray<int> Unfixed;
							for (i = 0; i < t->Span.x; i++)
							{
								if (!FixedCol[x+i])
								{
									Unfixed[Unfixed.Length()] = x + i;
								}
							}
							
							// Bump out minimums
							if (ColMin < t->MinContent)
							{
								int Total = t->MinContent - ColMin;

								if (Unfixed.Length())
								{
									int Add = Total / Unfixed.Length();
									for (i=0; i<Unfixed.Length(); i++)
									{
										int a = i ? Add : Total - (Add * (Unfixed.Length() - 1));
										
										MinCol[Unfixed[i]] = MinCol[Unfixed[i]] + a;
										TotalX += a;
									}
								}
								else
								{
									int Add = Total / t->Span.x;
									for (i=0; i<t->Span.x; i++)
									{
										int a = i ? Add : Total - (Add * (t->Span.x - 1));
										MinCol[x + i] = MinCol[x + i] + a;
										TotalX += a;
									}
								}
							}

							// Bump out maximums
							if (t->MaxContent > ColMin)
							{
								int MaxAdd = t->MaxContent - ColMin;
								int MaxAvail = AvailableX - TotalX;
								int Total = min(MaxAdd, MaxAvail);
								
								/*
								printf("bumpmax %i,%i mincol=%i add=%i avail=%i total=%i Unfixed=%i\n",
									x, y,
									ColMin,
									MaxAdd,
									MaxAvail,
									Total,
									Unfixed.Length());
								*/

								if (Unfixed.Length())
								{
									int Add = Total / Unfixed.Length();
									for (i=0; i<Unfixed.Length(); i++)
									{
										int a = i ? Add : Total - (Add * (Unfixed.Length() - 1));

										MinCol[Unfixed[i]] = MinCol[Unfixed[i]] + a;
										TotalX += a;
									}
								}
								else
								{
									int Add = Total / t->Span.x;
									for (i=0; i<t->Span.x; i++)
									{
										int a = i ? Add : Total - (Add * (t->Span.x - 1));
										MinCol[x + i] = MinCol[x + i] + a;
										TotalX += a;
									}
								}
							}
						}

						x += t->Span.x;
					}
					else break;
				}
			}
		}

		DumpCols();
		
		// Deallocate space if overused
		if (TotalX > AvailableX)
		{
			// Take some from the largest column
			int Largest = 0;
			for (int i=0; i<s.x; i++)
			{
				if (MinCol[i] > MinCol[Largest])
				{
					Largest = i;
				}
			}
			int Take = TotalX - AvailableX;
			if (Take < MinCol[Largest])
			{
				MinCol[Largest] = MinCol[Largest] - Take;
				TotalX -= Take;
			}
		}

		DumpCols();

		// Allocate any unused but available space...
		if (TotalX < AvailableX)
		{
			// Some available space still
			// printf("Alloc unused space, Total=%i Available=%i\n", TotalX, AvailableX);

			// Allocate to columns that fully fit
			int LaidOut = 0;
			for (int x=0; x<s.x; x++)
			{
				int NeedsX = MaxCol[x] - MinCol[x];
				// printf("Max-Min: %i-%i = %i\n", MaxCol[x], MinCol[x], NeedsX);
				if (NeedsX > 0 && NeedsX < AvailableX - TotalX)
				{
					MinCol[x] = MinCol[x] + NeedsX;
					TotalX += NeedsX;
					LaidOut++;
				}
			}

			// Allocate to columns that still need room
			int RemainingCols = s.x - LaidOut;
			if (RemainingCols > 0)
			{
				int SpacePerCol = (AvailableX - TotalX) / RemainingCols;
				if (SpacePerCol > 0)
				{
					for (int x=0; x<s.x; x++)
					{
						int NeedsX = MaxCol[x] - MinCol[x];
						if (NeedsX > 0)
						{
							MinCol[x] = MinCol[x] + SpacePerCol;
							TotalX += SpacePerCol;
						}
					}
				}
			}
			
			if (TotalX < AvailableX && Width.IsValid())
			{
				// Force allocation of space

				// Add some to the largest column
				int Largest = 0;
				for (int i=0; i<s.x; i++)
				{
					if (MinCol[i] > MinCol[Largest])
					{
						Largest = i;
					}
				}
				int Add = AvailableX - TotalX;
				MinCol[Largest] = MinCol[Largest] + Add;
				TotalX += Add;
			}
		}

		DumpCols();

		// Allocate remaining space if explicit table width
		if (Width.IsValid() &&
			TotalX < AvailableX)
		{
			int Add = (AvailableX - TotalX) / s.x;
			for (int x=0; x<s.x; x++)
			{
				MinCol[x] = MinCol[x] + Add;
				TotalX += Add;
			}
		}

		DumpCols();
		
		// Layout cell contents to get the height of all the cells
		for (y=0; y<s.y; y++)
		{
			for (int x=0; x<s.x; )
			{
				GTag *t = Cells->Get(x, y);
				if (t)
				{
					if (t->Cell.x == x && t->Cell.y == y)
					{
						GRect Box(0, 0, -CellSpacing, 0);
						for (int i=0; i<t->Span.x; i++)
						{
							Box.x2 += MinCol[x+i] + CellSpacing;
						}
						
						GFlowRegion r(Html, Box);
						t->OnFlow(&r);
						t->Size.y += CellPadding;
						
						if (t->Height.IsValid() &&
							t->Height.GetUnits() != LengthPercentage)
						{
							int h = t->Height.Get(f, Font);
							t->Size.y = max(h, t->Size.y);

							DistributeSize(MaxRow, y, t->Span.y, t->Size.y, CellSpacing);
						}

						#if DEBUG_LAYOUT
						if (Debug)
							LgiTrace("[%i,%i]=%i,%i Rx=%i\n", t->Cell.x, t->Cell.y, t->Size.x, t->Size.y, Rx);
						#endif
					}

					x += t->Span.x;
				}
				else break;
			}
		}

		// Calculate row height
		for (y=0; y<s.y; y++)
		{
			for (int x=0; x<s.x; )
			{
				GTag *t = Cells->Get(x, y);
				if (t)
				{
					if (t->Cell.x == x && t->Cell.y == y)
					{
						if (!(t->Height.IsValid() && t->Height.GetUnits() != LengthPercentage))
						{
							DistributeSize(MaxRow, y, t->Span.y, t->Size.y, CellSpacing);
						}
					}

					x += t->Span.x;
				}
				else break;
			}			
		}
		
		// Cell positioning
		int Cx = BorderLeft + CellSpacing, Cy = BorderTop + CellSpacing;
		for (y=0; y<s.y; y++)
		{
			GTag *Prev = 0;
			for (int x=0; x<s.x; )
			{
				GTag *t = Cells->Get(x, y);
				if (!t && Prev)
				{
					// Add missing cell
					GTag *Row = Prev->Parent;
					if (Row && Row->TagId == TAG_TR)
					{
						t = new GTag(Html, Row);
						if (t)
						{
							t->TagId = TAG_TD;
							t->Tag = NewStr("td");
							t->Info = GetTagInfo("td");
							t->Cell.x = x;
							t->Cell.y = y;
							t->Span.x = 1;
							t->Span.y = 1;
							t->Back = DefaultMissingCellColour;

							Cells->Set(this);
						}
						else break;
					}
					else break;
				}
				if (t)
				{
					if (t->Cell.x == x && t->Cell.y == y)
					{
						t->Pos.x = Cx;
						t->Pos.y = Cy;
						t->Size.x = -CellSpacing;
						for (int i=0; i<t->Span.x; i++)
						{
							int w = MinCol[x + i] + CellSpacing;
							t->Size.x += w;
							Cx += w;
						}
						t->Size.y = -CellSpacing;						
						for (int n=0; n<t->Span.y; n++)
						{
							t->Size.y += MaxRow[y+n] + CellSpacing;
						}
						
						Size.x = max(Cx + BorderRight, Size.x);
					}
					else
					{
						Cx += t->Size.x + CellSpacing;
					}
					
					x += t->Span.x;
				}
				else break;
				Prev = t;
			}
			
			Cx = CellSpacing;
			Cy += MaxRow[y] + CellSpacing;
		}

		switch (GetAlign(true))
		{
			case AlignCenter:
			{
				int Ox = (f->X()-Size.x) >> 1;
				Pos.x = f->x1 + max(Ox, 0);
				break;
			}
			case AlignRight:
			{
				Pos.x = f->x2 - Size.x;
				break;
			}
			default:
			{
				Pos.x = MarginLeft.Get(f, Font) + f->x1;
				break;
			}
		}
		Pos.y = f->y1;

		Size.y = Cy + BorderBottom;
	}
}

GRect GTag::ChildBounds()
{
	GRect b(0, 0, -1, -1);

	GTag *t = Tags.First();
	if (t)
	{
		b = t->GetRect();
		while (t = Tags.Next())
		{
			GRect c = t->GetRect();
			b.Union(&c);
		}
	}

	return b;
}

int GTag::AbsX()
{
	int a = 0;
	for (GTag *t=this; t; t=t->Parent)
	{
		a += t->Pos.x;
	}
	return a;
}

int GTag::AbsY()
{
	int a = 0;
	for (GTag *t=this; t; t=t->Parent)
	{
		a += t->Pos.y;
	}
	return a;
}

void GTag::SetSize(GdcPt2 &s)
{
	Size = s;
}

GArea::~GArea()
{
	DeleteObjects();
}

GRect *GArea::TopRect(GRegion *c)
{
	GRect *Top = 0;
	
	for (int i=0; i<c->Length(); i++)
	{
		GRect *r = (*c)[i];
		if (!Top || (r && (r->y1 < Top->y1)))
		{
			Top = r;
		}
	}

	return Top;
}

void GArea::FlowText(GTag *Tag, GFlowRegion *Flow, GFont *Font, char16 *Text, CssAlign Align)
{
	if (!Flow || !Text || !Font)
		return;


	while (*Text)
	{
		GFlowRect *Tr = new GFlowRect;
		if (!Tr)
			break;

		Tr->Tag = Tag;
	Restart:
		Tr->x1 = Flow->cx;
		Tr->y1 = Flow->y1;

		if (Flow->x1 == Flow->cx && *Text == ' ') Text++;
		Tr->Text = Text;

		GDisplayString ds(Font, Text);
		int Chars = ds.CharAt(Flow->X());
		bool Wrap = false;
		if (Text[Chars])
		{
			// Word wrap
			int n = Chars;
			while (n > 0 && !StrchrW(WhiteW, Text[n]))
			{
				n--;
			}

			if (n == 0)
			{
				// printf("x1=%i cx=%i flow->x=%i text='%.20S'\n", Flow->x1, Flow->cx, Flow->X(), Text + Tr->Len);
				if (Flow->x1 == Flow->cx)
				{
					// Already started from the margin and it's too long to 
					// fit across the entire page, just let it hang off the right edge.

					// Seek to the end of the word
					for (Tr->Len = Chars; Text[Tr->Len] && !StrchrW(WhiteW, Text[Tr->Len]); Tr->Len++)
					{
					}

					// Wrap...
					if (*Text == ' ') Text++;
				}
				else
				{
					// Not at the start of the margin
					Flow->FinishLine();							
					goto Restart;
				}
			}
			else
			{
				Tr->Len = n;
			}

			Wrap = true;
		}
		else
		{
			// Fits..
			Tr->Len = Chars;
		}

		GDisplayString ds2(Font, Tr->Text, Tr->Len);
		Tr->x2 = ds2.X();
		Tr->y2 = ds2.Y();
		if (Wrap)
		{
			Flow->cx = Flow->x1;
			Flow->y1 += Tr->y2 + 1;
			Tr->x2 = Flow->x2 - Tag->RelX();
		}
		else
		{
			Tr->x2 += Tr->x1 - 1;
			Flow->cx = Tr->x2 + 1;
		}
		Tr->y2 += Tr->y1;
		Flow->y2 = max(Flow->y2, Tr->y2 + 1);
		
		Insert(Tr);
		Flow->Insert(Tr);
		
		Text += Tr->Len;

		Tag->Size.x = max(Tag->Size.x, Tr->x2);
		Tag->Size.y = max(Tag->Size.y, Tr->y2);

		#if 0
		if (Wrap)
		{
			// New line here... process alignment.
			switch (Align)
			{
				case AlignCenter:
				{
					GFlowRect *r = ItemAt(LineStart);
					if (r)
					{
						int MinX = r->x1, MaxX = r->x2;
						while (r = Next())
						{
							MinX = min(MinX, r->x1);
							MaxX = max(MaxX, r->x2);
						}

						int Tx = MaxX - MinX;
						int Fx = Flow->x2 - Flow->x1;
						int Offset = (Fx - Tx) / 2;
						for (r = ItemAt(LineStart); r; r = Next())
						{
							r->Offset(Offset, 0);
						}
					}
					break;
				}
			}

			LineStart = Length();
		}
		#endif

		if (Tr->Len == 0)
			break;
	}
}

void GTag::OnFlow(GFlowRegion *InputFlow)
{
	if (!Visible) return;

	GFlowRegion *Flow = InputFlow;
	GFont *f = GetFont();
	GFlowRegion Local(Html);
	bool Restart = true;

	Size.x = 0;
	Size.y = 0;

	switch (TagId)
	{
		case TAG_IMG:
		{
			Restart = false;

			CssAlign a = GetAlign(true);
			Pos.y = Flow->y1;
			int ImgX = Width.IsValid() ? Width.Get(Flow, GetFont()) : (Image ? Image->X() : 0);
			switch (a)
			{
				case AlignCenter:
				{
					int Fx = Flow->x2 - Flow->x1;
					Pos.x = Flow->x1 + ((Fx - ImgX) / 2);
					break;
				}
				case AlignRight:
				{
					Pos.x = Flow->x2 - ImgX;
					break;
				}
				default:
				{
					Pos.x = Flow->cx;
					break;
				}
			}
			break;
		}
		case TAG_HR:
		{
			Flow->FinishLine();
			
			Pos.x = Flow->x1;
			Pos.y = Flow->y1 + 7;
			Size.x = Flow->X();
			Size.y = 2;
			
			Flow->cx ++;
			Flow->y2 += 16;

			Flow->FinishLine();
			return;
			break;
		}
		case TAG_TABLE:
		{
			Flow->EndBlock();
			LayoutTable(Flow);
			Flow->y1 += Size.y;
			Flow->y2 = Flow->y1;
			Flow->cx = Flow->x1;
			BoundParents();
			return;
		}
	}

	int OldFlowMy = Flow->my;
	int DWidth = 0;
	if (IsBlock)
	{
		// This is a block level element, so end the previous non-block elements
		Flow->EndBlock();
		if (TagId == TAG_P)
		{
			if (!OldFlowMy && Text)
			{
				Flow->FinishLine(true);
			}
		}
		
		// Set the width if any
		if (Width.IsValid())
		{
			Size.x = Width.Get(Flow, f, true);
		}

		// Indent the margin...
		Flow->Indent(Font, MarginLeft, MarginTop, MarginRight, MarginBottom, true);

		if (TagId == TAG_DIV)
		{
			// What the hell does this do?
			if (Size.x)
			{
				DWidth = Flow->X();
				Flow->X(Size.x);
				DWidth -= Flow->X();
			}
		}

		Pos.x = Flow->x1;
		Pos.y = Flow->y1;

		if (TagId == TAG_TD)
		{
			Flow->x1 += CellPadding;
			Flow->cx += CellPadding;
			Flow->x2 -= CellPadding;
			Flow->y1 += CellPadding;
		}

		Flow->x1 -= Pos.x;
		Flow->x2 -= Pos.x;
		Flow->cx -= Pos.x;

		Flow->y1 -= Pos.y;
		Flow->y2 -= Pos.y;

		Flow->Indent(f, BorderLeft, BorderTop, BorderRight, BorderBottom);
		Flow->Indent(f, PaddingLeft, PaddingTop, PaddingRight, PaddingBottom);
	}

	if (f)
	{
		// Clear the previous text layout...
		TextPos.DeleteObjects();
		
		if (TagId == TAG_LI)
		{
			// Insert the list marker
			if (!PreText)
			{
				if (Parent && Parent->TagId == TAG_OL)
				{
					int Index = Parent->Tags.IndexOf(this);
					char Txt[32];
					sprintf(Txt, "%i. ", Index + 1);
					PreText = LgiNewUtf8To16(Txt);
				}
				else
				{
					PreText = NewStrW(GHtmlListItem);
				}
			}

			TextPos.FlowText(this, Flow, f, PreText, AlignLeft);
		}

		if (Text)
		{
			// Flow in the rest of the text...
			TextPos.FlowText(this, Flow, f, Text, GetAlign(true));
		}
	}

	// Flow children
	for (GTag *t=Tags.First(); t; t=Tags.Next())
	{
		t->OnFlow(Flow);

		if (TagId == TAG_TR)
		{
			Flow->x2 -= min(t->Size.x, Flow->X());
		}
	}

	int OldFlowY2 = Flow->y2;
	if (IsBlock)
	{
		Flow->EndBlock();

		Flow->Outdent(Font, PaddingLeft, PaddingTop, PaddingRight, PaddingBottom);
		Flow->Outdent(Font, BorderLeft, BorderTop, BorderRight, BorderBottom);
		Flow->x2 += DWidth;

		if (TagId == TAG_P)
		{
			Flow->Outdent(Font, MarginLeft, MarginTop, MarginRight, MarginBottom, true);
		}
		else
		{
			Flow->Outdent(Font, MarginLeft, MarginTop, MarginRight, MarginBottom, true);
		}

		Flow->y1 = Flow->y2;
	}

	switch (TagId)
	{
		case TAG_IMG:
		{
			if (Width.IsValid())
			{
				int w = Width.Get(Flow, f);
				Size.x = min(w, Flow->X());
			}
			else if (Image)
			{
				Size.x = Image->X();
			}
			else
			{
				Size.x = DefaultImgSize;
			}

			if (Height.IsValid())
			{
				Size.y = Height.Get(Flow, f);
			}
			else if (Image)
			{
				Size.y = Image->Y();
			}
			else
			{
				Size.y = DefaultImgSize;
			}

			Flow->cx += Size.x;
			Flow->y2 = max(Flow->y1 + Size.y, Flow->y2);
			break;
		}
		case TAG_TR:
		{
			// Flow->EndBlock();
			Flow->x2 = Flow->x1 + Local.X();
			break;
		}
		case TAG_BR:
		{
			Flow->FinishLine();
			Size.y = Flow->y2 - OldFlowY2;
			break;
		}
		case TAG_DIV:
		{
			Size.y = Flow->y2;
			break;
		}
		case TAG_TD:
		{
			Size.x = Flow->X();
			Size.y = Flow->y1;
			break;
		}
	}

	BoundParents();

	if (Restart)
	{
		Flow->x1 += Pos.x;
		Flow->x2 += Pos.x;
		Flow->cx += Pos.x;

		Flow->y1 += Pos.y;
		Flow->y2 += Pos.y;
	}
}

bool GTag::PeekTag(char *s, char *tag)
{
	bool Status = false;

	if (s && tag)
	{
		if (*s == '<')
		{
			char *t = 0;
			ParseName(++s, &t);
			if (t)
			{
				Status = stricmp(t, tag) == 0;
			}

			DeleteArray(t);
		}
	}

	return Status;
}

GTag *GTag::GetTable()
{
	GTag *t = 0;
	for (t=Parent; t && t->TagId != TAG_TABLE; t = t->Parent);
	return t;
}

void GTag::BoundParents()
{
	if (Parent)
	{
		for (GTag *n=this; n; n=n->Parent)
		{
			if (n->Parent)
			{
				n->Parent->Size.x = max(n->Parent->Size.x, n->Pos.x + n->Size.x);
				n->Parent->Size.y = max(n->Parent->Size.y, n->Pos.y + n->Size.y);
			}
		}
	}
}

void GTag::OnPaintBorder(GSurface *pDC)
{
	if (BorderLeft)
	{
		pDC->Colour(BorderLeft.Colour, 32);
		int OldStyle = pDC->LineStyle();
		for (int i=0; i<BorderLeft; i++)
		{
			pDC->LineStyle(BorderLeft.LineStyle, BorderLeft.LineReset);
			pDC->Line(i, 0, i, Size.y-1);
		}
		pDC->LineStyle(OldStyle);
	}
	if (BorderTop)
	{
		pDC->Colour(BorderTop.Colour, 32);
		int OldStyle = pDC->LineStyle();
		for (int i=0; i<BorderTop; i++)
		{
			pDC->LineStyle(BorderTop.LineStyle, BorderTop.LineReset);
			pDC->Line(0, i, Size.x-1, i);
		}
		pDC->LineStyle(OldStyle);
	}
	if (BorderRight)
	{
		pDC->Colour(BorderRight.Colour, 32);
		int OldStyle = pDC->LineStyle();
		for (int i=0; i<BorderRight; i++)
		{
			pDC->LineStyle(BorderRight.LineStyle, BorderRight.LineReset);
			pDC->Line(Size.x-i-1, 0, Size.x-i-1, Size.y-1);
		}
		pDC->LineStyle(OldStyle);
	}
	if (BorderBottom)
	{
		pDC->Colour(BorderBottom.Colour, 32);
		int OldStyle = pDC->LineStyle(BorderBottom.LineStyle);
		for (int i=0; i<BorderBottom; i++)
		{
			pDC->LineStyle(BorderBottom.LineStyle, BorderBottom.LineReset);
			pDC->Line(0, Size.y-i-1, Size.x-1, Size.y-i-1);
		}
		pDC->LineStyle(OldStyle);
	}

}

void GTag::OnPaint(GSurface *pDC)
{
	if (!Visible) return;
	int Px, Py;
	pDC->GetOrigin(Px, Py);

	switch (TagId)
	{
		case TAG_BODY:
		{
			Selected = false;
			
			if (Image)
			{
				COLOUR b = GetBack();
				if (b != GT_TRANSPARENT)
				{
					pDC->Colour(b, 32);
					pDC->Rectangle();
				}

				switch (BackgroundRepeat)
				{
					case BgRepeat:
					{
						for (int y=0; y<Html->Y(); y += Image->Y())
						{
							for (int x=0; x<Html->X(); x += Image->X())
							{
								pDC->Blt(Px + x, Py + y, Image);
							}
						}
						break;
					}
					case BgRepeatX:
					{
						for (int x=0; x<Html->X(); x += Image->X())
						{
							pDC->Blt(Px + x, Py, Image);
						}
						break;
					}
					case BgRepeatY:
					{
						for (int y=0; y<Html->Y(); y += Image->Y())
						{
							pDC->Blt(Px, Py + y, Image);
						}
						break;
					}
					case BgNoRepeat:
					{
						pDC->Blt(Px, Py, Image);
						break;
					}
				}
			}			
			break;
		}
		case TAG_HEAD:
		{
			// Nothing under here to draw.
			return;
		}
		case TAG_HR:
		{
			pDC->Colour(LC_MED, 24);
			pDC->Rectangle(0, 0, Size.x - 1, Size.y - 1);
			break;
		}
		case TAG_TR:
		case TAG_TBODY:
		case TAG_META:
		{
			// Draws nothing...
			break;
		}
		case TAG_IMG:
		{
			GRect Clip(0, 0, Size.x-1, Size.y-1);
			pDC->ClipRgn(&Clip);
			
			if (Image)
			{
				int Old = pDC->Op(GDC_ALPHA);

				/*
				printf("Img %ix%i@%i bits, Alpha=%p (pDC=%i bits, scr=%i)\n",
					Image->X(),
					Image->Y(),
					Image->GetBits(),
					Image->AlphaDC(),
					pDC->GetBits(), pDC->IsScreen());
				*/
				
				pDC->Blt(0, 0, Image);

				pDC->Op(Old);
			}
			else if (Size.x > 1 && Size.y > 1)
			{
				GRect b(0, 0, Size.x-1, Size.y-1);
				COLOUR Col = GdcMixColour(LC_MED, LC_LIGHT, 0.2);

				// Border
				pDC->Colour(LC_MED, 24);
				pDC->Box(&b);
				b.Size(1, 1);
				pDC->Box(&b);
				b.Size(1, 1);
				pDC->Colour(Col, 24);
				pDC->Rectangle(&b);

				if (Size.x >= 16 && Size.y >= 16)
				{
					// Red 'x'
					int Cx = b.x1 + (b.X()/2);
					int Cy = b.y1 + (b.Y()/2);
					GRect c(Cx-4, Cy-4, Cx+4, Cy+4);
					
					pDC->Colour(GdcMixColour(Rgb24(255, 0, 0), Col, 0.3), 24);
					pDC->Line(c.x1, c.y1, c.x2, c.y2);
					pDC->Line(c.x1, c.y2, c.x2, c.y1);
					pDC->Line(c.x1, c.y1 + 1, c.x2 - 1, c.y2);
					pDC->Line(c.x1 + 1, c.y1, c.x2, c.y2 - 1);
					pDC->Line(c.x1 + 1, c.y2, c.x2, c.y1 + 1);
					pDC->Line(c.x1, c.y2 - 1, c.x2 - 1, c.y1);
				}
			}
			
			pDC->ClipRgn(0);
			break;
		}
		case TAG_TABLE:
		{
			OnPaintBorder(pDC);

			if (DefaultTableBorder != GT_TRANSPARENT)
			{
				GRect r(BorderLeft, BorderTop, Size.x-BorderRight-1, Size.y-BorderBottom-1); 

				#if 1
				GRegion c(r);
				
				if (Cells)
				{
					List<GTag> AllTd;
					Cells->GetAll(AllTd);
					for (GTag *t=AllTd.First(); t; t=AllTd.Next())
					{
						r.Set(0, 0, t->Size.x-1, t->Size.y-1);
						for (; t && t!=this; t=t->Parent)
						{
							r.Offset(t->Pos.x, t->Pos.y);
						}
						c.Subtract(&r);
					}
				}

				if (Back == GT_TRANSPARENT)
				{
					pDC->Colour(DefaultTableBorder, 32);
				}
				else
				{
					pDC->Colour(Back, 32);
				}
				
				for (GRect *p=c.First(); p; p=c.Next())
				{
					pDC->Rectangle(p);
				}
				
				#else
				
				pDC->Colour(DefaultTableBorder, 32);
				pDC->Rectangle(&r);

				#endif				
			}
			break;
		}
		default:
		{
			COLOUR fore = GetFore();
			COLOUR back =	Back == GT_TRANSPARENT &&
							Info &&
							Info->Block() ? GetBack() : Back;

			if (back != GT_TRANSPARENT)
			{
				bool IsAlpha = A32(back) < 0xff;
				int Op = GDC_SET;
				if (IsAlpha)
				{
					Op = pDC->Op(GDC_ALPHA);
				}
				pDC->Colour(back, 32);

				if (IsBlock)
				{
					pDC->Rectangle(0, 0, Size.x-1, Size.y-1);
				}
				else
				{
					for (GFlowRect *Tr=TextPos.First(); Tr; Tr=TextPos.Next())
					{
						pDC->Rectangle(Tr);
					}
				}

				if (IsAlpha)
				{
					pDC->Op(Op);
				}
			}

			OnPaintBorder(pDC);

			GFont *f = GetFont();
			if (f && TextPos.Length() > 0)
			{
				#define FontColour(s) \
				f->Transparent(!s); \
				if (s) \
					f->Colour(LC_SEL_TEXT, LC_SELECTION); \
				else \
					f->Colour(	fore != GT_TRANSPARENT ? Rgb32To24(fore) : Rgb32To24(DefaultTextColour), \
								Back != GT_TRANSPARENT ? Rgb32To24(Back) : Rgb32To24(LC_WORKSPACE));

				if (Html->HasSelection() &&
					(Selection >= 0 || Cursor >= 0) &&
					Selection != Cursor)
				{
					int Min = -1;
					int Max = -1;

					int Base = GetTextStart();
					if (Cursor >= 0 && Selection >= 0)
					{
						Min = min(Cursor, Selection) + Base;
						Max = max(Cursor, Selection) + Base;
					}
					else if (Selected)
					{
						Max = max(Cursor, Selection) + Base;
					}
					else
					{
						Min = max(Cursor, Selection) + Base;
					}

					GRect CursorPos;
					CursorPos.ZOff(-1, -1);

					for (GFlowRect *Tr=TextPos.First(); Tr; Tr=TextPos.Next())
					{
						int Start = SubtractPtr(Tr->Text, Text);
						int Done = 0;
						int x = Tr->x1;

						while (Done < Tr->Len)
						{
							int c = Tr->Len - Done;

							FontColour(Selected);

							// Is this a selection edge point?
							if (		!Selected &&
										Min - Start >= Done &&
										Min - Start < Done + Tr->Len)
							{
								Selected = !Selected;
								c = Min - Start - Done;
							}
							else if (	Selected &&
										Max - Start >= Done &&
										Max - Start <= Tr->Len)
							{
								Selected = !Selected;
								c = Max - Start - Done;
							}

							// Draw the text run
							GDisplayString ds(f, Tr->Text + Done, c);
							ds.Draw(pDC, x, Tr->y1);
							x += ds.X();
							Done += c;

							// Is this is end of the tag?
							if (Tr->Len == Done)
							{
								// Is it also a selection edge?
								if (		!Selected &&
											Min - Start == Done)
								{
									Selected = !Selected;
								}
								else if (	Selected &&
											Max - Start == Done)
								{
									Selected = !Selected;
								}
							}

							if (Cursor >= 0)
							{
								// Is this the cursor, then draw it and save it's position
								if (Cursor == Start + Done - Base)
								{
									Html->d->CursorPos.Set(x, Tr->y1, x + 1, Tr->y2);

									if (Html->d->CursorPos.x1 > Tr->x2)
										Html->d->CursorPos.Offset(Tr->x2 - Html->d->CursorPos.x1, 0);

									CursorPos = Html->d->CursorPos;
									Html->d->CursorPos.Offset(AbsX(), AbsY());
								}
							}

						}
					}

					if (Html->d->CursorVis && CursorPos.Valid())
					{
						pDC->Colour(LC_TEXT, 24);
						pDC->Rectangle(&CursorPos);
					}
				}
				else if (Cursor >= 0)
				{
					FontColour(Selected);

					int Base = GetTextStart();
					for (GFlowRect *Tr=TextPos.First(); Tr; Tr=TextPos.Next())
					{
						int Pos = SubtractPtr(Tr->Text, Text) - Base;

						GDisplayString ds(f, Tr->Text, Tr->Len);
						ds.Draw(pDC, Tr->x1, Tr->y1);

						if
						(
							(
								Tr->Text == PreText
								&&
								!ValidStrW(Text)
							)
							||
							(
								Cursor >= Pos
								&&
								Cursor <= Pos + Tr->Len
							)
						)
						{
							int Off = Tr->Text == PreText ? StrlenW(PreText) : Cursor - Pos;
							pDC->Colour(LC_TEXT, 24);
							GRect c;
							if (Off)
							{
								GDisplayString ds(f, Tr->Text, Off);
								int x = ds.X();
								if (x >= Tr->X())
									x = Tr->X()-1;
								c.Set(Tr->x1 + x, Tr->y1, Tr->x1 + x + 1, Tr->y1 + f->GetHeight());
							}
							else
							{
								c.Set(Tr->x1, Tr->y1, Tr->x1 + 1, Tr->y1 + f->GetHeight());
							}
							Html->d->CursorPos = c;
							if (Html->d->CursorVis)
								pDC->Rectangle(&c);
							Html->d->CursorPos.Offset(AbsX(), AbsY());
						}
					}
				}
				else
				{
					FontColour(Selected);

					for (GFlowRect *Tr=TextPos.First(); Tr; Tr=TextPos.Next())
					{
						GDisplayString ds(f, Tr->Text, Tr->Len);
						ds.Draw(pDC, Tr->x1, Tr->y1);
					}
				}
			}
			break;
		}
	}

	for (GTag *t=Tags.First(); t; t=Tags.Next())
	{
		pDC->SetOrigin(Px - t->Pos.x, Py - t->Pos.y);
		t->OnPaint(pDC);
		pDC->SetOrigin(Px, Py);
	}
}

//////////////////////////////////////////////////////////////////////
GHtml::GHtml(int id, int x, int y, int cx, int cy, GDocumentEnv *e)
	:
	GDocView(e),
	ResObject(Res_Custom)
{
	d = new GHtmlPrivate;
	ViewWidth = -1;
	MemDC = 0;
	SetId(id);
	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	Cursor = 0;
	Selection = 0;
	SetBackColour(Rgb24To32(LC_WORKSPACE));
	PrevTip = 0;
	DocCharSet = 0;

	_New();
}

GHtml::~GHtml()
{
	_Delete();
	DeleteArray(Charset);
	DeleteArray(DocCharSet);
	DeleteObj(d);
}

void GHtml::_New()
{
	#if LUIS_DEBUG
	LgiTrace("%s:%i html(%p).src(%p)'\n", __FILE__, __LINE__, this, Source);
	#endif

	d->Content.x = d->Content.y = 0;
	Tag = 0;
	Source = 0;
	DeleteArray(DocCharSet);
	// if (!Charset) Charset = NewStr("utf-8");

	IsHtml = true;
	FontCache = new GFontCache;
	if (FontCache)
	{
		GFont *f = new GFont;
		if (f)
		{
			*f = *GetFont();
			LgiAssert(ValidStr(f->Face()));
			FontCache->AddFont(f);
		}
	}
	SetScrollBars(false, false);
}

void GHtml::_Delete()
{
	#if LUIS_DEBUG
	LgiTrace("%s:%i html(%p).src(%p)='%30.30s'\n", __FILE__, __LINE__, this, Source, Source);
	#endif

	SetBackColour(Rgb24To32(LC_WORKSPACE));

	CssMap.Empty();
	OpenTags.Empty();
	DeleteArray(Source);
	DeleteObj(Tag);
	DeleteObj(FontCache);
	DeleteObj(MemDC);
}

GFont *GHtml::DefFont()
{
	return FontCache ? FontCache->FontAt(0) : 0;
}

char *White = " \t\r\n";
#define SkipWs() while (*c && strchr(White, *c)) c++;

char *SkipComment(char *c)
{
	// Skip comment
	SkipWs();
	if (c[0] == '/' &&
		c[1] == '*')
	{
		char *e = strstr(c + 2, "*/");
		if (e) c = e + 2;
		else c += 2;
	}
	return c;
}

void GHtml::AddCss(char *Css)
{
	char *c=Css;	
	if (!Css) return;

	SkipWs();
	if (*c == '<')
	{
		c++;
		while (*c && !strchr(White, *c)) c++;
	}

	for (; c && *c; )
	{
		c = SkipComment(c);

		// read selector
		List<char> Selector;
		char *s = SkipComment(c);
		char *e = s;
		GStringPipe p;
		while (*e)
		{
			if (IsWhiteSpace(*e))
			{
				e = SkipComment(e);
				p.Push(" ");
			}
			else if (*e == '{' || *e == ',')
			{
				char *Raw = p.NewStr();
				if (Raw)
				{
					char *End = Raw + strlen(Raw) - 1;
					while (End > Raw && IsWhiteSpace(*End))
						*End-- = 0;

					Selector.Insert(Raw);
				}
				else break;

				if (*e == '{')
				{
					c = e;
					break;
				}

				e = SkipComment(e + 1);
			}
			else
			{
				p.Push(e++, 1);
			}
		}

		SkipWs();

		// read styles
		if (*c++ == '{')
		{
			SkipWs();

			char *Start = c;
			while (*c && *c != '}') c++;
			char *Style = NewStr(Start, SubtractPtr(c, Start));
			for (char *Sel=Selector.First(); Sel; Sel=Selector.Next())
			{
				if (Sel[0] == '.' && Sel[1] == '.')
				{
					CssMap[Sel + 1] = Style;
				}
				else
				{
					CssMap[Sel] = Style;
				}
			}
			c++;

			DeleteArray(Style);
			Selector.DeleteArrays();
		}
		else
		{
			Selector.DeleteArrays();
			break;
		}
	}

CssDone:
	DeleteArray(Css);
}

void GHtml::Parse()
{
	if (!Tag)
	{
		Tag = new GTag(this, 0);
	}

	SetBackColour(Rgb24To32(LC_WORKSPACE));
	if (Tag)
	{
		OpenTags.Empty();

		if (IsHtml)
		{
			Tag->ParseHtml(Source, 0);

			// Add body tag if not specified...
			GTag *Html = Tag->GetTagByName("html");
			
			if (!Html)
			{
				Html = new GTag(this, 0);
				if (Html)
				{
					Html->SetTag("html");

					Html->Tags.Insert(Tag);
					Tag->Parent = Html;
					Tag = Html;
				}
			}

			if (Html)
			{
				GTag *Body = 0;

				for (GTag *t = Html->Tags.First(); t; t = Html->Tags.Next())
				{
					if (t->TagId == TAG_BODY)
					{
						Body = t;
					}
				}

				if (!Body)
				{
					Body = new GTag(this, Html);
					if (Body)
					{
						Body->SetTag("body");

						if (Html->Text)
						{
							GTag *Content = new GTag(this, Body);
							if (Content)
							{
								Content->Text = Html->Text;
								Html->Text = 0;
							}
						}

						for (GTag *t = Html->Tags.First(); t;)
						{
							if (t->TagId != TAG_HEAD &&
								t != Body)
							{
								Html->Tags.Delete(t);
								Body->Tags.Insert(t);
								t->Parent = Body;

								t = Html->Tags.Current();
							}
							else
							{
								t = Html->Tags.Next();
							}
						}
					}
				}

				if (Body && Environment)
				{
					char *OnLoad;
					if (Body->Get("onload", OnLoad))
					{
						Environment->OnExecuteScript(OnLoad);
					}
				}
			}
		}
		else
		{
			Tag->ParseText(Source);
		}
	}

	ViewWidth = -1;
	Invalidate();
}

bool GHtml::NameW(char16 *s)
{
	GAutoPtr<char, true> utf(LgiNewUtf16To8(s));
	return Name(utf);
}

char16 *GHtml::NameW()
{
	GObject::Name(Source);
	return GObject::NameW();
}

bool GHtml::Name(char *s)
{
	#if LUIS_DEBUG
	LgiTrace("%s:%i html(%p).src(%p)='%30.30s'\n", __FILE__, __LINE__, this, Source, Source);
	#endif

	_Delete();
	_New();

	Source = NewStr(s);

	#if LUIS_DEBUG
	LgiTrace("%s:%i html(%p).src(%p)='%30.30s'\n", __FILE__, __LINE__, this, Source, Source);
	#endif

	if (Source)
	{
		IsHtml = false;

		// Detect HTML
		char *s = Source;
		while (s = strchr(s, '<'))
		{
			char *t = 0;
			s = ParseName(++s, &t);
			if (t && GetTagInfo(t))
			{
				DeleteArray(t);
				IsHtml = true;
				break;
			}
			DeleteArray(t);
		}

		#if 0
		GFile f;
		f.Open("c:\\temp\\broken.html", O_WRITE);
		{
			f.Write(Source, strlen(Source));
			f.Close();
		}
		#endif

		// Parse
		Parse();
	}

	Invalidate();

	return true;
}

char *GHtml::Name()
{
	#if LUIS_DEBUG
	LgiTrace("%s:%i html(%p).src(%p)='%30.30s'\n", __FILE__, __LINE__, this, Source, Source);
	#endif

	if (!Source)
	{
		GStringPipe s(1024);
		Tag->CreateSource(s);
		Source = s.NewStr();
	}

	return Source;
}

int GHtml::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case M_COPY:
		{
			Copy();
			break;
		}
		case M_IMAGE_LOADED:
		{
			char *Uri = (char*)MsgA(Msg);
			GSurface *Img = (GSurface*)MsgB(Msg);

			LgiTrace("M_IMAGE_LOADED img=%p\n", Img);

			if (Uri && Img && Tag)
			{
				int Used = 0;
				Tag->ImageLoaded(Uri, Img, Used);
				if (Used)
				{
					ViewWidth = 0;
					OnPosChange();
					Invalidate();

					Img = 0;
				}
			}

			LgiTrace("....img=%p\n", Img);

			DeleteArray(Uri);
			DeleteObj(Img);
			break;
		}
	}

	return GDocView::OnEvent(Msg);
}

int GHtml::OnNotify(GViewI *c, int f)
{
	switch (c->GetId())
	{
		case IDC_VSCROLL:
		{
			Invalidate();
			break;
		}
	}

	return GLayout::OnNotify(c, f);
}

void GHtml::OnPosChange()
{
	GLayout::OnPosChange();
	if (ViewWidth != X())
	{
		Invalidate();
	}
}

GdcPt2 GHtml::Layout()
{
	GRect Client = GetClient();
	if (ViewWidth != Client.X())
	{
		GRect Client = GetClient();
		GFlowRegion f(this, Client);

		// Flow text, width is different
		Tag->OnFlow(&f);
		// f.FinishLine();
		d->Content.x = ViewWidth = Client.X();
		d->Content.y = f.y2;
		

		// Set up scroll box
		int Sy = f.y2 > Y();
		int LineY = GetFont()->GetHeight();
		
		SetScrollBars(false, Sy);
		if (Sy && VScroll)
		{
			int y = Y();
			int p = max(y / LineY, 1);
			int fy = f.y2 / LineY;
			VScroll->SetPage(p);
			VScroll->SetLimits(0, fy);
		}
	}

	return d->Content;
}

void GHtml::OnPaint(GSurface *ScreenDC)
{
	#if LGI_EXCEPTIONS
	try
	{
	#endif
		GRect Client = GetClient();
		GRect p = GetPos();

		#if GHTML_USE_DOUBLE_BUFFER
		if (!MemDC ||
			(MemDC->X() < Client.X() || MemDC->Y() < Client.Y()))
		{
			DeleteObj(MemDC);
			MemDC = new GMemDC;
			if (MemDC && !MemDC->Create(Client.X() + 10, Client.Y() + 10, 32)) // GdcD->GetBits()
			{
				DeleteObj(MemDC);
			}
		}
		#endif

		GSurface *pDC = MemDC ? MemDC : ScreenDC;

		pDC->Colour(GetBackColour(), 32);
		pDC->Rectangle();

		if (Tag)
		{
			//for (int r=0; r<2; r++)
			//{
				Layout();
			// }

			if (VScroll)
			{
				int LineY = GetFont()->GetHeight();
				pDC->SetOrigin(0, VScroll->Value() * LineY);
			}

			Tag->OnPaint(pDC);
		}

		#if GHTML_USE_DOUBLE_BUFFER
		if (MemDC)
		{
			pDC->SetOrigin(0, 0);
			ScreenDC->Blt(0, 0, MemDC);
		}
		#endif
	#if LGI_EXCEPTIONS
	}
	catch (...)
	{
		printf("GHtml paint crash\n");
	}
	#endif
}

GTag *GHtml::GetOpenTag(char *Tag)
{
	if (Tag)
	{
		for (GTag *t=OpenTags.Last(); t; t=OpenTags.Prev())
		{
			if (t->Tag)
			{
				if (stricmp(t->Tag, Tag) == 0)
				{
					return t;
				}

				if (stricmp(t->Tag, "table") == 0)
				{
					// stop looking... we don't close tags outside
					// the table from inside.
					break;
				}				
			}
		}
	}

	return 0;
}

bool GHtml::HasSelection()
{
	if (Cursor && Selection)
	{
		return	Cursor->Cursor >= 0 &&
				Selection->Selection >= 0 &&
				!(Cursor == Selection && Cursor->Cursor == Selection->Selection);
	}

	return false;
}

void GHtml::UnSelectAll()
{
	bool i = false;
	if (Cursor)
	{
		Cursor->Cursor = -1;
		i = true;
	}
	if (Selection)
	{
		Selection->Selection = -1;
		i = true;
	}
	if (i)
	{
		Invalidate();
	}
}

void GHtml::SelectAll()
{
}

GTag *GHtml::GetLastChild(GTag *t)
{
	if (t)
	{
		for (GTag *i = t->Tags.Last(); i; )
		{
			GTag *c = i->Tags.Last();
			if (c)
				i = c;
			else
				return i;
		}
	}

	return 0;
}

GTag *GHtml::PrevTag(GTag *t)
{
	// This returns the previous tag in the tree as if all the tags were
	// listed via recursion using "in order".

	// Walk up the parent chain looking for a prev
	for (GTag *p = t; p; p = p->Parent)
	{
		// Does this tag have a next?
		if (p->Parent)
		{
			int Idx = p->Parent->Tags.IndexOf(p);
			GTag *Prev = p->Parent->Tags[Idx - 1];
			if (Prev)
			{
				GTag *Last = GetLastChild(Prev);
				return Last ? Last : Prev;
			}
		}
	}

	return 0;
}

GTag *GHtml::NextTag(GTag *t)
{
	// This returns the next tag in the tree as if all the tags were
	// listed via recursion using "in order".

	// Does this have a child tag?
	if (t->Tags.First())
	{
		return t->Tags.First();
	}
	else
	{
		// Walk up the parent chain
		for (GTag *p = t; p; p = p->Parent)
		{
			// Does this tag have a next?
			if (p->Parent)
			{
				int Idx = p->Parent->Tags.IndexOf(p);
				GTag *Next = p->Parent->Tags[Idx + 1];
				if (Next)
				{
					return Next;
				}
			}
		}
	}

	return 0;
}

int GHtml::GetTagDepth(GTag *Tag)
{
	// Returns the depth of the tag in the tree.
	int n = 0;
	for (GTag *t = Tag; t; t = t->Parent)
	{
		n++;
	}
	return n;
}

bool GHtml::IsCursorFirst()
{
	// Returns true if the cursor is before the selection point.
	//
	// There are 2 points in the selection, the start and the end, the
	// cursor can be the start or the end, there selection is always the
	// other end of the block.
	if (Cursor && Selection)
	{
		if (Cursor == Selection)
		{
			return Cursor->Cursor < Selection->Selection;
		}
		else
		{
			int CDepth = GetTagDepth(Cursor);
			int SDepth = GetTagDepth(Selection);
			GTag *Cur = Cursor;
			GTag *Sel = Selection;
			while (Sel && SDepth > CDepth)
			{
				Sel = Sel->Parent;
				SDepth--;
			}
			while (Cur && CDepth > SDepth)
			{
				Cur = Cur->Parent;
				CDepth--;
			}
			if (Cur && Sel)
			{
				int CIdx = Cur->Parent ? Cur->Parent->Tags.IndexOf(Cur) : 0;
				int SIdx = Sel->Parent ? Sel->Parent->Tags.IndexOf(Sel) : 0;
				if (CIdx <= SIdx)
				{
					return true;
				}
			}
		}
	}

	return false;
}

void GHtml::SetLoadImages(bool i)
{
	if (i ^ GetLoadImages())
	{
		GDocView::SetLoadImages(i);
		SendNotify(GTVN_SHOW_IMGS_CHANGED);
	}
}

char *GHtml::GetSelection()
{
	char *s = 0;

	GBytePipe p;

// printf("GHtml::GetSelection 0\n");

	Tag->CopyClipboard(p);

// printf("GHtml::GetSelection 1\n");

	int Len = p.GetSize();
	if (Len > 0)
	{
		char16 *t = (char16*)p.New(2);
		if (t)
		{
			int Len = StrlenW(t);
			for (int i=0; i<Len; i++)
			{
				if (t[i] == 0xa0) t[i] = ' ';
			}
			s = LgiNewUtf16To8(t);
			DeleteArray(t);
		}
	}

// printf("GHtml::GetSelection 2\n");

	return s;
}

bool GHtml::Copy()
{
	char *s = GetSelection();
	if (s)
	{
		GClipBoard c(this);
		
		char16 *w = LgiNewUtf8To16(s);
		if (w) c.TextW(w);
		c.Text(s, w == 0);

		DeleteArray(w);
		DeleteArray(s);
		return true;
	}

	return false;
}

bool FindCallback(GFindReplaceCommon *Dlg, bool Replace, void *User)
{
	GHtml *h = (GHtml*)User;
	return h->OnFind(Dlg);
}

void BuildTagList(GArray<GTag*> &t, GTag *Tag)
{
	t.Add(Tag);
	for (GTag *c = Tag->Tags.First(); c; c = Tag->Tags.Next())
	{
		BuildTagList(t, c);
	}
}

bool GHtml::OnFind(class GFindReplaceCommon *Params)
{
	bool Status = false;

	if (!Params->Find)
		return Status;

	if (!Cursor)
		Cursor = Tag;

	char16 *Find = LgiNewUtf8To16(Params->Find);
	if (Cursor && Find)
	{
		GArray<GTag*> Tags;
		BuildTagList(Tags, Tag);
		int Start = Tags.IndexOf(Cursor);
		for (int i=0; i<Tags.Length(); i++)
		{
			int Idx = (Start + i) % Tags.Length();
			GTag *s = Tags[Idx];

			if (s->Text)
			{
				char16 *Hit;
				
				if (Params->MatchCase)
					Hit = StrstrW(s->Text, Find);
				else
					Hit = StristrW(s->Text, Find);
				
				if (Hit)
				{
					// found something...
					UnSelectAll();

					Selection = Cursor = s;
					Cursor->Cursor = Hit - s->Text;
					Selection->Selection = Cursor->Cursor + StrlenW(Find);
					
					Invalidate();
					Status = true;
					break;
				}
			}
		}
	}
	DeleteArray(Find);

	return Status;
}

bool GHtml::OnKey(GKey &k)
{
	bool Status = false;
	
	if (k.Down())
	{
		int Dy = 0;
		int LineY = GetFont()->GetHeight();
		int Page = GetClient().Y() / LineY;

		switch (k.c16)
		{
			case 'f':
			case 'F':
			{
				if (k.Ctrl())
				{
					GFindDlg Dlg(this, 0, FindCallback, this);
					Dlg.DoModal();
					Status = true;
				}
				break;
			}
			case 'c':
			case 'C':
			#ifdef WIN32
			case VK_INSERT:
			#endif
			{
				if (k.Ctrl())
				{
					Copy();
					Status = true;
				}
				break;
			}
			case VK_UP:
			{
				Dy = -1;
				Status = true;
				break;
			}
			case VK_DOWN:
			{
				Dy = 1;
				Status = true;
				break;
			}
			case VK_PAGEUP:
			{
				Dy = -Page;
				Status = true;
				break;
			}
			case VK_PAGEDOWN:
			{
				Dy = Page;
				Status = true;
				break;
			}
			case VK_HOME:
			{
				Dy = VScroll ? -VScroll->Value() : 0;
				Status = true;
				break;
			}
			case VK_END:
			{
				if (VScroll)
				{
					int64 Low, High;
					VScroll->Limits(Low, High);
					Dy = (High - Page) - VScroll->Value();
				}
				Status = true;
				break;
			}
		}

		if (Dy && VScroll)
		{
			VScroll->Value(VScroll->Value() + Dy);
			Invalidate();
		}
	}

	return Status;
}

int GHtml::ScrollY()
{
	return GetFont()->GetHeight() * (VScroll ? VScroll->Value() : 0);
}

void GHtml::OnMouseClick(GMouse &m)
{
	Capture(m.Down());
	SetPulse(m.Down() ? 200 : -1);

	if (m.Down())
	{
		Focus(true);

		int Offset = ScrollY();
		bool TagProcessedClick = false;
		int Index = -1;

		GTag *Over = GetTagByPos(m.x, m.y + Offset, &Index);
		if (m.Left())
		{
			if (m.Double())
			{
				d->WordSelectMode = true;

				if (Cursor)
				{
					// Extend the selection out to the current word's boundries.
					Selection = Cursor;
					Selection->Selection = Cursor->Cursor;

					if (Cursor->Text)
					{
						int Base = Cursor->GetTextStart();
						while (Cursor->Text[Base + Cursor->Cursor])
						{
							char16 c = Cursor->Text[Base + Cursor->Cursor];

							if (strchr(WordDelim, c) || StrchrW(WhiteW, c))
								break;

							Cursor->Cursor++;
						}
					}
					if (Selection->Text)
					{
						int Base = Selection->GetTextStart();
						while (Selection->Selection > 0)
						{
							char16 c = Selection->Text[Base + Selection->Selection - 1];

							if (strchr(WordDelim, c) || StrchrW(WhiteW, c))
								break;

							Selection->Selection--;
						}
					}

					Invalidate();
				}
			}
			else
			{
				d->WordSelectMode = false;
				UnSelectAll();
				Selection = Cursor = Over;
				if (Cursor)
				{
					Selection->Selection = Cursor->Cursor = Index;
				}
			}
		}

		if (Over)
		{
			TagProcessedClick = Over->OnMouseClick(m);
		}

		if (!TagProcessedClick &&
			m.IsContextMenu())
		{
			GSubMenu *RClick = new GSubMenu;
			if (RClick)
			{
				#define IDM_DUMP			100
				#define IDM_COPY_SRC		101
				#define IDM_VIEW_SRC		102
				#define IDM_EXTERNAL		103
				#define IDM_COPY			104
				#define IDM_VIEW_IMAGES		105
				#define IDM_CHARSET_BASE	1000

				RClick->AppendItem					(LgiLoadString(L_TEXTCTRL_COPY, "Copy"), IDM_COPY, HasSelection());
				GMenuItem *Vs = RClick->AppendItem	(LgiLoadString(L_VIEW_SOURCE, "View Source"), IDM_VIEW_SRC, Source != 0);
				RClick->AppendItem					(LgiLoadString(L_COPY_SOURCE, "Copy Source"), IDM_COPY_SRC, Source != 0);
				GMenuItem *Load = RClick->AppendItem(LgiLoadString(L_VIEW_IMAGES, "View External Images"), IDM_VIEW_IMAGES, true);
				if (Load) Load->Checked(GetLoadImages());
				RClick->AppendItem					(LgiLoadString(L_VIEW_IN_DEFAULT_BROWSER, "View in Default Browser"), IDM_EXTERNAL, Source != 0);
				GSubMenu *Cs = RClick->AppendSub	(LgiLoadString(L_CHANGE_CHARSET, "Change Charset"));
				if (Cs)
				{
					int n=0;
					for (GCharset *c = LgiGetCsList(); c->Charset; c++, n++)
					{
						Cs->AppendItem(c->Charset, IDM_CHARSET_BASE + n, c->IsAvailable());
					}
				}
				
				#ifdef _DEBUG
				RClick->AppendSeparator();
				RClick->AppendItem("Dump", IDM_DUMP, Tag != 0);
				#endif

				if (Vs)
				{
					Vs->Checked(!IsHtml);
				}

				if (GetMouse(m, true))
				{
					int Id = 0;
					switch (Id = RClick->Float(this, m.x, m.y))
					{
						case IDM_COPY:
						{
							Copy();
							break;
						}
						case IDM_VIEW_SRC:
						{
							LgiCheckHeap();
							if (Vs)
							{
								DeleteObj(Tag);
								IsHtml = !IsHtml;
								Parse();
							}
							LgiCheckHeap();
							break;
						}
						case IDM_COPY_SRC:
						{
							if (Source)
							{
								GClipBoard c(this);
								if (Is8Bit(Source))
								{
									char *u8 = (char*)LgiNewConvertCp("utf-8", Source, DocCharSet ? DocCharSet : (char*)"windows-1252");
									if (u8)
									{
										c.Text(u8);
										DeleteArray(u8);
									}
								}
								else
								{
									c.Text(Source);
								}
							}
							break;
						}
						case IDM_VIEW_IMAGES:
						{
							SetLoadImages(!GetLoadImages());
							if (GetLoadImages() && Tag)
							{
								Tag->LoadImages();
							}
							break;
						}
						case IDM_DUMP:
						{
							if (Tag)
							{
								char *s = Tag->Dump();
								if (s)
								{
									GClipBoard c(this);
									c.Text(s);
									DeleteObj(s);
								}
							}
							break;
						}
						case IDM_EXTERNAL:
						{
							char Path[256];
							if (Source && LgiGetSystemPath(LSP_TEMP, Path, sizeof(Path)))
							{
								char f[32];
								sprintf(f, "_%i.html", LgiRand(1000000));
								LgiMakePath(Path, sizeof(Path), Path, f);
								
								LgiCheckHeap();

								GFile F;
								if (F.Open(Path, O_WRITE))
								{
									GStringPipe Ex;
									bool Error = false;
									
									F.SetSize(0);

									for (char *s=Source; s && *s;)
									{
										char *cid = stristr(s, "cid:");
										while (cid && !strchr("\'\"", cid[-1]))
										{
											cid = stristr(cid+1, "cid:");
										}

										if (cid)
										{
											char Delim = cid[-1];
											char *e = strchr(cid, Delim);
											if (e)
											{
												*e = 0;
												if (strchr(cid, '\n'))
												{
													*e = Delim;
													Error = true;
													break;
												}
												else
												{
													char File[256] = "";
													if (Environment)
													{
														Environment->GetImageUri(cid, 0, File, sizeof(File));
													}
													
													*e = Delim;
													Ex.Push(s, (int)cid-(int)s);
													if (File[0])
													{
														char *d;
														while (d = strchr(File, '\\'))
														{
															*d = '/';
														}
														
														Ex.Push("file:///");
														Ex.Push(File);
													}
													s = e;
												}
											}
											else
											{
												Error = true;
												break;
											}
										}
										else
										{
											Ex.Push(s);
											break;
										}
									}

									LgiCheckHeap();

									if (!Error)
									{
										char *Final = Ex.NewStr();
										if (Final)
										{
											F.Write(Final, strlen(Final));
											DeleteArray(Final);
											F.Close();
											LgiExecute(Path);
										}
									}
								}
							}
							break;
						}
						default:
						{
							if (Id >= IDM_CHARSET_BASE)
							{
								GCharset *c = LgiGetCsList() + (Id - IDM_CHARSET_BASE);
								if (c->Charset)
								{
									DeleteArray(Charset);
									Charset = NewStr(c->Charset);
									OverideDocCharset = true;

									char *Src = Source;
									Source = 0;
									_Delete();
									_New();
									Source = Src;
									Parse();

									Invalidate();

									SendNotify(GTVN_CODEPAGE_CHANGED);
								}								
							}
							break;
						}
					}
				}

				DeleteObj(RClick);
			}
		}
	}
	else // Up Click
	{
		if (Selection && Cursor &&
			Selection == Cursor &&
			Selection->Selection == Cursor->Cursor)
		{
			Selection->Selection = -1;
			Selection = 0;
		}
	}
}

GTag *GHtml::GetTagByPos(int x, int y, int *Index)
{
	if (Tag)
	{
		GTagHit Hit;
		Hit.Block = 0;
		Hit.Hit = 0;
		Hit.Index = -1;
		Hit.Near = 0x7fffffff;
		
		bool Status = Tag->GetTagByPos(x, y, &Hit);

		if (Status || Hit.Near < 3)
		{
			if (Index) *Index = Hit.Index;
			return Hit.Hit;
		}
	}

	return 0;
}

void GHtml::OnMouseWheel(double Lines)
{
	GFont *f = FontCache->FontAt(0);
	if (f && VScroll)
	{
		VScroll->Value(VScroll->Value() + Lines);
		Invalidate();
	}
}

void GHtml::OnMouseMove(GMouse &m)
{
	int Offset = ScrollY();
	int Index = -1;
	GTag *Tag = GetTagByPos(m.x, m.y + Offset, &Index);
	if (Tag)
	{
		if (PrevTip &&
			PrevTip != Tag)
		{			
			Tip.DeleteTip(PrevTip->TipId);
			PrevTip->TipId = 0;
			PrevTip = 0;
		}

		char *Uri = 0;
		if (Tag->IsAnchor(&Uri))
		{
			GRect c = GetClient();
			c.Offset(-c.x1, -c.y1);
			if (c.Overlap(m.x, m.y))
			{
				GLayout::SetCursor(LCUR_PointingHand);
			}

			if (Uri)
			{
				if (!Tip.GetParent())
				{
					Tip.Attach(this);
				}

				GRect r = Tag->GetRect(false);
				r.Offset(0, -Offset);
				PrevTip = Tag;
				PrevTip->TipId = Tip.NewTip(Uri, r);
			}

			DeleteArray(Uri);
		}

		if (Cursor && Tag->TagId != TAG_BODY && IsCapturing())
		{
			if (!Selection)
			{
				Selection = Cursor;
				Selection->Selection = Cursor->Cursor;
				Cursor = Tag;
				Cursor->Cursor = Index;

				Invalidate();
			}
			else if ((Cursor != Tag) ||
					 (Cursor->Selection != Index))
			{
				if (Cursor)
				{
					Cursor->Cursor = -1;
				}

				Cursor = Tag;
				Cursor->Cursor = Index;

				if (d->WordSelectMode && Cursor->Text)
				{
					int Base = Cursor->GetTextStart();
					if (IsCursorFirst())
					{
						// Extend the cursor up the document to include the whole word
						while (Cursor->Cursor > 0)
						{
							char16 c = Cursor->Text[Base + Cursor->Cursor - 1];

							if (strchr(WordDelim, c) || StrchrW(WhiteW, c))
								break;

							Cursor->Cursor--;
						}
					}
					else
					{
						// Extend the cursor down the document to include the whole word
						while (Cursor->Text[Base + Cursor->Cursor])
						{
							char16 c = Cursor->Text[Base + Cursor->Cursor];

							if (strchr(WordDelim, c) || StrchrW(WhiteW, c))
								break;

							Cursor->Cursor++;
						}
					}
				}

				Invalidate();
			}
		}
	}
}

void GHtml::OnPulse()
{
	if (VScroll && IsCapturing())
	{
		int Fy = DefFont() ? DefFont()->GetHeight() : 16;
		GMouse m;
		if (GetMouse(m, false))
		{
			GRect c = GetClient();
			int Lines = 0;
			
			if (m.y < c.y1)
			{
				// Scroll up
				Lines = (c.y1 - m.y + Fy - 1) / -Fy;
			}
			else if (m.y > c.y2)
			{
				// Scroll down
				Lines = (m.y - c.y2 + Fy - 1) / Fy;
			}
			
			if (Lines)
			{
				VScroll->Value(VScroll->Value() +  Lines);
				Invalidate();
			}
		}
	}
}

GRect *GHtml::GetCursorPos()
{
	return &d->CursorPos;
}

void GHtml::SetCursorVis(bool b)
{
	if (d->CursorVis ^ b)
	{
		d->CursorVis = b;
		Invalidate();
	}
}

bool GHtml::GetCursorVis()
{
	return d->CursorVis;
}

GDom *ElementById(GTag *t, char *id)
{
	if (t && id)
	{
		char *i;
		if (t->Get("id", i) && stricmp(i, id) == 0)
			return t;

		for (GTag *c = t->Tags.First(); c; c = t->Tags.Next())
		{
			GDom *n = ElementById(c, id);
			if (n) return n;
		}
	}

	return 0;
}

GDom *GHtml::getElementById(char *Id)
{
	return ElementById(Tag, Id);
}

bool GHtml::GetLinkDoubleClick()
{
	return d->LinkDoubleClick;
}

void GHtml::SetLinkDoubleClick(bool b)
{
	d->LinkDoubleClick = b;
}

////////////////////////////////////////////////////////////////////////
class GHtml_Factory : public GViewFactory
{
	GView *NewView(char *Class, GRect *Pos, char *Text)
	{
		if (stricmp(Class, "GHtml") == 0)
		{
			return new GHtml(-1, 0, 0, 100, 100, new GDefaultDocumentEnv);
		}

		return 0;
	}

} GHtml_Factory;
