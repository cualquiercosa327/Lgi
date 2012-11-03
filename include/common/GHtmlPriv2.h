#ifndef _GHTML2_PRIV_H_
#define _GHTML2_PRIV_H_

#include "GCss.h"
#include "GToken.h"

namespace Html2
{

//////////////////////////////////////////////////////////////////////////////////
// Enums                                                                        //
//////////////////////////////////////////////////////////////////////////////////
enum HtmlTag
{
	CONTENT,
	CONDITIONAL,
	ROOT,
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
	TAG_HR,
	TAG_IFRAME,
	TAG_LINK,
	TAG_BIG,
	TAG_INPUT,
	TAG_SELECT,
	TAG_LABEL,
	TAG_FORM,
	TAG_LAST
};

enum TagInfoFlags
{
	TI_NONE			= 0x00,
	TI_NEVER_CLOSES	= 0x01,
	TI_NO_TEXT		= 0x02,
	TI_BLOCK		= 0x04,
	TI_TABLE		= 0x08,
};

//////////////////////////////////////////////////////////////////////////////////
// Structs & Classes                                                            //
//////////////////////////////////////////////////////////////////////////////////
class GFlowRect;
class GFlowRegion;

struct GTagHit
{
	GTag *Hit; // Tag that was hit, or nearest tag, otherwise NULL
	int Near; // 0 if a direct hit, >0 is near miss, -1 if invalid.

	GFlowRect *Block; // Text block hit
	int Index; // If Block!=NULL then index into text, otherwise -1.

	GTagHit()
	{
		Hit = 0;
		Block = 0;
		Near = -1;
		Index = -1;
	}

	bool operator <(GTagHit &h)
	{
		if (!Hit)
			return false;

		if (!h.Hit)
			return true;

		LgiAssert(	Near >= 0 &&
					h.Near >= 0);

		if (Near < h.Near)
			return true;

		return false;
	}
};

struct GInfo
{
public:
	HtmlTag Id;
	const char *Tag;
	const char *ReattachTo;
	int Flags;

	bool NeverCloses()	{ return TestFlag(Flags, TI_NEVER_CLOSES); }
	bool NoText()		{ return TestFlag(Flags, TI_NO_TEXT); }
	bool Block()		{ return TestFlag(Flags, TI_BLOCK); }
};

class GLength
{
protected:
	float d;
	float PrevAbs;
	GCss::LengthType u;

public:
	GLength();
	GLength(char *s);

	bool IsValid();
	bool IsDynamic();
	float GetPrevAbs() { return PrevAbs; }
	operator float();
	GLength &operator =(float val);
	GCss::LengthType GetUnits();
	void Set(char *s);
	float Get(GFlowRegion *Flow, GFont *Font, bool Lock = false);
	float GetRaw() { return d; }
};

class GLine : public GLength
{
public:
	int LineStyle;
	int LineReset;
	GCss::ColorDef Colour;

	GLine();
	~GLine();

	GLine &operator =(int i);
	void Set(char *s);
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

	int Start();
	bool OverlapX(int x) { return x >= x1 && x <= x2; }
	bool OverlapY(int y) { return y >= y1 && y <= y2; }
	bool OverlapX(GFlowRect *b) { return !(b->x2 < x1 || b->x1 > x2); }
	bool OverlapY(GFlowRect *b) { return !(b->y2 < y1 || b->y1 > y2); }
};

class GArea : public List<GFlowRect>
{
public:
	~GArea();

	GRect Bounds();
	GRect *TopRect(GRegion *c);
	void FlowText(GTag *Tag, GFlowRegion *c, GFont *Font, char16 *Text, GCss::LengthType Align);
};

class GCellStore
{
	typedef GArray<GTag*> CellArray;
	GArray<CellArray> c;

public:
	GCellStore(GTag *Table);

	void GetSize(int &x, int &y);
	void GetAll(List<GTag> &All);
	GTag *Get(int x, int y);
	bool Set(GTag *t);
	
	void Dump();
};

class GTag : public GDom, public GCss
{
public:
	enum HtmlControlType
	{
		CtrlNone,
		CtrlPassword,
		CtrlEmail,
		CtrlText,
		CtrlButton,
		CtrlSubmit,
		CtrlSelect,
	};

protected:
	static bool Selected;
	friend class HtmlEdit;

	GHashTbl<const char*, char*> Attr;

	// Forms
	GViewI *Ctrl;
	GVariant CtrlValue;
	HtmlControlType CtrlType;

	// Text
	GAutoWString Txt, PreTxt;

	// Debug stuff
	void _Dump(GStringPipe &Buf, int Depth);
	void _TraceOpenTags();

	// Private methods
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
	bool OnUnhandledColor(GCss::ColorDef *def, const char *&s);
	
	COLOUR _Colour(bool Fore);
	COLOUR GetFore() { return _Colour(true); }
	COLOUR GetBack() { return _Colour(false); }

public:
	// Object
	HtmlTag TagId;
	char *Tag; // My tag

	GToken Class;
	const char *HtmlId;

	GAutoString Condition;
	GInfo *Info;
	int TipId;
	bool WasClosed;
	DisplayType Disp;

	// Heirarchy
	GHtml2 *Html;
	GTag *Parent;
	List<GTag> Tags;
	bool HasChild(GTag *c);
	bool Attach(GTag *Child, int Idx = -1);
	void Detach();
	GTag *GetBlockParent(int *Idx = 0);
	GFont *GetFont();

	// Style
	GdcPt2 Pos;
	GdcPt2 Size;
	GFont *Font;
	
	// Images
	GAutoPtr<GSurface> Image;
	void SetImage(const char *uri, GSurface *i);
	void LoadImage(const char *Uri); // Load just this URI
	void LoadImages(); // Recursive load all image URI's
	void ImageLoaded(char *uri, GSurface *img, int &Used);

	// Table stuff
	GdcPt2 Cell;
	GdcPt2 Span;
	uint16 MinContent, MaxContent;
	GCss::LengthType XAlign;
	GCellStore *Cells;
	#ifdef _DEBUG
	int Debug;
	#endif

	// Text
	int Cursor; // index into text of the cursor
	int Selection; // index into the text of the selection edge
	GArea TextPos;

	GTag(GHtml2 *h, GTag *p);
	~GTag();

	// Events
	void OnChange(PropType Prop);
	bool OnClick();

	// Attributes
	bool Get(const char *attr, const char *&val) { val = Attr.Find(attr); return val != 0; }
	void Set(const char *attr, const char *val);

	// Methods
	char16 *Text() { return Txt; }
	void Text(char16 *t) { Txt.Reset(t); TextPos.Empty(); }
	char16 *PreText() { return PreTxt; }
	void PreText(char16 *t) { PreTxt.Reset(t); TextPos.Empty(); }

	int GetTextStart();
	char *Dump();
	char16 *CleanText(const char *s, int len, bool ConversionAllowed = true, bool KeepWhiteSpace = false);
	char *ParseHtml(char *Doc, int Depth, bool InPreTag = false, bool *BackOut = 0);
	char *ParseText(char *Doc);
	
	/// Configures the tag's styles.
	void SetStyle();
	/// Called to apply CSS selectors on initialization and also when properties change at runtime.
	void Restyle();
	/// Match a simple CSS selector against the current object
	bool MatchSimpleSelector(GCss::Selector *Sel, int PartIdx);
	/// Match all the CSS selectors against the current object (calls MatchSimpleSelector one or more times)
	bool MatchFullSelector(GCss::Selector *Sel);
	
	/// Takes the CSS styles, parses and stores them in the current object,
	//// overwriting any duplicate properties.
	void SetCssStyle(const char *Style);
	/// Positions the tag according to the flow region passed in
	void OnFlow(GFlowRegion *Flow);
	/// Paints the border of the tag
	void OnPaintBorder(
		/// The surface to paint on
		GSurface *pDC,
		/// [Optional] The size of the border painted
		GRect *Px = NULL);
	void OnPaint(GSurface *pDC);
	void SetSize(GdcPt2 &s);
	void SetTag(const char *Tag);
	GTagHit GetTagByPos(int x, int y);
	GTag *GetTagByName(const char *Name);
	void CopyClipboard(GBytePipe &p);
	GTag *IsAnchor(GAutoString *Uri);
	bool CreateSource(GStringPipe &p, int Depth = 0, bool LastWasBlock = true);
	void Find(int TagType, GArray<GTag*> &Tags);
	GTag *GetAnchor(char *Name);

	// Control handling
	GTag *FindCtrlId(int Id);
	int OnNotify(int f);

	// GDom impl
	bool GetVariant(const char *Name, GVariant &Value, char *Array = 0);
	bool SetVariant(const char *Name, GVariant &Value, char *Array = 0);

	// Window
	bool OnMouseClick(GMouse &m);
	void Invalidate();

	// Positioning
	int RelX() { return Pos.x + (int)MarginLeft().Value; }
	int RelY() { return Pos.y + (int)MarginTop().Value; }
	int AbsX();
	int AbsY();
	GRect GetRect(bool Client = true);
	GCss::LengthType GetAlign(bool x);

	// Tables
	GTag *GetTableCell(int x, int y);
	GdcPt2 GetTableSize();
};

}

#endif