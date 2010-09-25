#ifndef _GHTML_PRIV_H_
#define _GHTML_PRIV_H_

#include "GProperties.h"

class GTag;

//
// Enums
//
enum HtmlTag
{
	CONTENT,
	TAG_UNKNOWN,
	TAG_HTML,
	TAG_HEAD,
	TAG_BODY,
	TAG_B,
	TAG_I,
	TAG_U,
	TAG_P,
	TAG_BR,
	TAG_UL,
	TAG_OL,
	TAG_LI,
	TAG_FONT,
	TAG_A,
	TAG_TABLE,
		TAG_TR,
		TAG_TD,
	TAG_IMG,
	TAG_DIV,
	TAG_SPAN,
	TAG_CENTER,
	TAG_META,
	TAG_TBODY,
	TAG_STYLE,
	TAG_SCRIPT,
	TAG_STRONG,
	TAG_BLOCKQUOTE,
	TAG_PRE,
	TAG_H1,
	TAG_H2,
	TAG_H3,
	TAG_H4,
	TAG_H5,
	TAG_H6,
	TAG_HR
};

enum GLengthUnit
{
	LengthNull = 0,
	LengthPixels,
	LengthPercentage,
	LengthPoint,
	LengthEm,
	LengthEx,
	LengthRemaining
};

enum TagInfoFlags
{
	TI_NONE			= 0x00,
	TI_NEVER_CLOSES	= 0x01,
	TI_NO_TEXT		= 0x02,
	TI_BLOCK		= 0x04,
	TI_TABLE		= 0x08,
};

struct GTagHit
{
	GTag *Hit;
	class GFlowRect *Block;
	int Near;
	int Index;
};

struct GInfo
{
public:
	HtmlTag Id;
	char *Tag;
	char *ReattachTo;
	int Flags;

	bool NeverCloses()	{ return TestFlag(Flags, TI_NEVER_CLOSES); }
	bool NoText()		{ return TestFlag(Flags, TI_NO_TEXT); }
	bool Block()		{ return TestFlag(Flags, TI_BLOCK); }
};

//
// Classes
//
class GFlowRegion;

class GLength
{
protected:
	float d;
	float PrevAbs;
	GLengthUnit u;

public:
	GLength();
	GLength(char *s);

	bool IsValid();
	bool IsDynamic();
	float GetPrevAbs() { return PrevAbs; }
	operator float();
	GLength &operator =(float val);
	GLengthUnit GetUnits();
	void Set(char *s);
	float Get(GFlowRegion *Flow, GFont *Font, bool Lock = false);
	float GetRaw() { return d; }
};

class GLine : public GLength
{
public:
	int LineStyle;
	int LineReset;
	COLOUR Colour;

	GLine();
	~GLine();

	GLine &operator =(int i);
	void Set(char *s);
};

class GCellStore
{
	class Cell
	{
	public:
		int x, y;
		GTag *Tag;
	};

	List<Cell> Cells;

public:
	GCellStore(GTag *Table);
	~GCellStore()
	{
		Cells.DeleteObjects();
	}

	void GetSize(int &x, int &y);
	void GetAll(List<GTag> &All);
	GTag *Get(int x, int y);
	bool Set(GTag *t);
	
	void Dump();
};

class GFlowRect : public GRect
{
public:
	GTag *Tag;
	char16 *Text;
	int Len;

	GFlowRect()
	{
		Tag = 0;
		Text = 0;
		Len = 0;
	}

	~GFlowRect()
	{
	}
};

class GArea : public List<GFlowRect>
{
public:
	~GArea();

	GRect *TopRect(GRegion *c);
	void FlowText(GTag *Tag, GFlowRegion *c, GFont *Font, char16 *Text, CssAlign Align);
};

class GTag : public GDom, public ObjProperties
{
	static bool Selected;
	friend class HtmlEdit;

	// Debug stuff
	void _Dump(GStringPipe &Buf, int Depth);
	void _TraceOpenTags();

	// Private methods
	GFont *GetFont();
	GFont *NewFont();
	int NearestChar(GFlowRect *Fr, int x, int y);
	GTag *HasOpenTag(char *t);
	GTag *PrevTag();
	GRect ChildBounds();
	bool GetWidthMetrics(uint16 &Min, uint16 &Max);
	void LayoutTable(GFlowRegion *f);
	void BoundParents();
	bool PeekTag(char *s, char *tag);
	GTag *GetTable();
	char *NextTag(char *s);
	void ZeroTableElements();
	
	COLOUR _Colour(bool Fore);
	COLOUR GetFore() { return _Colour(true); }
	COLOUR GetBack() { return _Colour(false); }

public:
	// Object
	HtmlTag TagId;
	char *Tag; // My tag
	char *HtmlId;
	GInfo *Info;
	int TipId;
	bool WasClosed;
	bool IsBlock;

	// Heirarchy
	GHtml *Html;
	GTag *Parent;
	List<GTag> Tags;

	void Attach(GTag *Child, int Idx = -1);
	void Detach();
	GTag *GetBlockParent(int *Idx = 0);

	// Style
	bool Visible;
	GLength Width, Height;
	GdcPt2 Pos;
	GdcPt2 Size;
	COLOUR Fore, Back, BorderColour;
	CssBackgroundRepeat BackgroundRepeat;
	GFont *Font;

	GLine BorderLeft;
	GLine BorderTop;
	GLine BorderRight;
	GLine BorderBottom;
	
	GLength MarginLeft;
	GLength MarginTop;
	GLength MarginRight;
	GLength MarginBottom;
	
	GLength PaddingLeft;
	GLength PaddingTop;
	GLength PaddingRight;
	GLength PaddingBottom;

	CssAlign AlignX;
	CssAlign AlignY;
	CssAlign GetAlign(bool x);

	// Images
	GSurface *Image;
	void SetImage(char *uri, GSurface *i);
	void LoadImage(char *Uri);
	void LoadImages();
	void ImageLoaded(char *uri, GSurface *img, int &Used);

	// Table stuff
	GdcPt2 Cell;
	GdcPt2 Span;
	uint8 CellSpacing;
	uint8 CellPadding;
	// uint8 TableBorder;
	uint16 MinContent, MaxContent;
	GCellStore *Cells;
	#ifdef _DEBUG
	bool Debug;
	#endif

	// Text
	int Cursor; // index into text of the cursor
	int Selection; // index into the text of the selection edge
	char16 *Text, *PreText;
	GArea TextPos;

	GTag(GHtml *h, GTag *p);
	~GTag();

	// Methods
	int GetTextStart();
	char *Dump();
	char16 *CleanText(char *s, int len, bool ConversionAllowed = true, bool KeepWhiteSpace = false);
	char *ParseHtml(char *Doc, int Depth, bool InPreTag = false, bool *BackOut = 0);
	char *ParseText(char *Doc);
	void SetStyle();
	void SetCssStyle(char *Style);
	void OnFlow(GFlowRegion *Flow);
	void OnPaintBorder(GSurface *pDC);
	void OnPaint(GSurface *pDC);
	void SetSize(GdcPt2 &s);
	void SetTag(char *Tag);
	bool GetTagByPos(int x, int y, GTagHit *Hit);
	GTag *GetTagByName(char *Name);
	void CopyClipboard(GBytePipe &p);
	GTag *IsAnchor(char **Uri);
	bool CreateSource(GStringPipe &p, int Depth = 0, bool LastWasBlock = true);
	void SetText(char16 *NewText);
	bool GetVariant(char *Name, GVariant &Value, char *Array = 0);
	bool SetVariant(char *Name, GVariant &Value, char *Array = 0);

	// Window
	bool OnMouseClick(GMouse &m);
	void Invalidate();

	// Positioning
	int RelX() { return Pos.x + MarginLeft; }
	int RelY() { return Pos.y + MarginTop; }
	int AbsX();
	int AbsY();
	GRect GetRect(bool Client = true);

	// Tables
	GTag *GetTableCell(int x, int y);
	GdcPt2 GetTableSize();
};

#endif