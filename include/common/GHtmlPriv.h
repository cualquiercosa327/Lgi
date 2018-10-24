#ifndef _GHTML1_PRIV_H_
#define _GHTML1_PRIV_H_

#include "GCss.h"
#include "GToken.h"
#include "GHtmlParser.h"

class HtmlEdit;

namespace Html1
{
#define DefaultTextColour			Rgb32(0, 0, 0)

//////////////////////////////////////////////////////////////////////////////////
// Structs & Classes                                                            //
//////////////////////////////////////////////////////////////////////////////////
class GFlowRect;
class GFlowRegion;

#define ToTag(t)					dynamic_cast<GTag*>(t)

struct GTagHit
{
	GTag *Direct;		// Tag directly under cursor
	GTag *NearestText;	// Nearest tag with text
	int Near;			// How close in px was the position to NearestText.
						// 0 if a direct hit, >0 is near miss, -1 if invalid.
	bool NearSameRow;	// True if 'NearestText' on the same row as click.
	GdcPt2 LocalCoords;	// The position in local co-ords of the tag

	GFlowRect *Block;	// Text block hit
	ssize_t Index; // If Block!=NULL then index into text, otherwise -1.

	GTagHit()
	{
		Direct = NULL;
		NearestText = NULL;
		NearSameRow = false;
		Block = 0;
		Near = -1;
		Index = -1;
		LocalCoords.x = LocalCoords.y = -1;
	}
	
	void Dump(const char *Desc);
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
	ssize_t Len;

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

class GArea : public GArray<GFlowRect*>
{
public:
	~GArea();

	void Empty() { DeleteObjects(); }
	GRect Bounds();
	GRect *TopRect(GRegion *c);
	void FlowText(GTag *Tag, GFlowRegion *c, GFont *Font, int LineHeight, char16 *Text, GCss::LengthType Align);
};

struct GHtmlTableLayout
{
	typedef GArray<GTag*> CellArray;
	GArray<CellArray> c;
	GTag *Table;
	GdcPt2 s;
	GCss::Len TableWidth;
	
	// Various pixels sizes
	int AvailableX;
	int CellSpacing;
	int BorderX1, BorderX2;
	GRect TableBorder, TablePadding; // in Px

	// The col and row sizes
	GArray<int> MinCol, MaxCol, MaxRow;
	GArray<GCss::Len> SizeCol;

	GHtmlTableLayout(GTag *table);

	void GetSize(int &x, int &y);
	void GetAll(List<GTag> &All);
	GTag *Get(int x, int y);
	bool Set(GTag *t);

	int GetTotalX(int StartCol = 0, int Cols = -1);
	void AllocatePx(int StartCol, int Cols, int MinPx, bool FillWidth);
	void DeallocatePx(int StartCol, int Cols, int MaxPx);
	void LayoutTable(GFlowRegion *f, uint16 Depth);
	
	void Dump();
};

class GTag : public GHtmlElement
{
	friend struct GHtmlTableLayout;
	friend class ::HtmlEdit;
	
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
		CtrlHidden,
	};

	class TextConvertState
	{
		GStream *Out;
		int PrevLineLen;
		GArray<char> Buf;
	
	public:
		int Depth;
		int CharsOnLine;
		
		TextConvertState(GStream *o)
		{
			Out = o;
			Depth = 0;
			CharsOnLine = 0;
			PrevLineLen = 0;
		}
		
		~TextConvertState()
		{
			if (CharsOnLine)
				NewLine();
		}
		
		ssize_t _Write(const void *Ptr, ssize_t Bytes)
		{
			// Check if we have enough space to store the string..
			size_t Total = CharsOnLine + Bytes;
			if (Buf.Length() < Total)
			{
				// Extend the memory buffer
				if (!Buf.Length(Total + 32))
					return -1;
			}
			
			// Store the string into a line buffer
			memcpy(&Buf[CharsOnLine], Ptr, Bytes);
			CharsOnLine += Bytes;

			return Bytes;
		}
		
		ssize_t Write(const void *Ptr, ssize_t Bytes)
		{
			char *start = (char*) Ptr, *cur;
			char *end = start + Bytes;
			const char *eol = "\r\n";
			while (start < end)
			{
				for (cur = start; *cur && cur < end && !strchr(eol, *cur); cur++)
					;
				
				if (!*cur || cur >= end)
					break;
				
				_Write(start, (int) (cur - start));
				
				start = cur;
				while (*start && start < end && strchr(eol, *start))
					start++;
			}
			
			return _Write(start, (int) (end - start));
		}
		
		int GetPrev()
		{
			return PrevLineLen;
		}
		
		void NewLine()
		{
			bool Valid = false;
			const uint8 Ws[] = {' ', '\t', 0xa0, 0};
			
			GUtf8Ptr p(&Buf[0]);
			uint8 *End = (uint8*) &Buf[CharsOnLine];
			while (p.GetPtr() < End)
			{
				if (!strchr((char*)Ws, p))
				{
					Valid = true;
					break;
				}
				p++;
			}
			if (!Valid)
				CharsOnLine = 0;

			Buf[CharsOnLine] = 0;
			if (CharsOnLine || PrevLineLen)
			{
				Out->Write(&Buf[0], CharsOnLine);
				Out->Write("\n", 1);
				PrevLineLen = CharsOnLine;
				CharsOnLine = 0;
			}
		}
	};

protected:
	/// A hash table of attributes.
	///
	/// All strings stored in here should be in UTF-8. Each string is allocated on the heap.
	LHashTbl<ConstStrKey<char,false>, char*> Attr;

	// Forms
	GViewI *Ctrl;
	GVariant CtrlValue;
	HtmlControlType CtrlType;

	// Text
	GAutoWString PreTxt;

	// Debug stuff
	void _Dump(GStringPipe &Buf, int Depth);
	void _TraceOpenTags();

	// Private methods
	GFont *NewFont();
	ssize_t NearestChar(GFlowRect *Fr, int x, int y);
	GTag *HasOpenTag(char *t);
	GTag *PrevTag();
	GRect ChildBounds();
	bool GetWidthMetrics(GTag *Table, uint16 &Min, uint16 &Max);
	void LayoutTable(GFlowRegion *f, uint16 Depth);
	void BoundParents();
	bool PeekTag(char *s, char *tag);
	GTag *GetTable();
	char *NextTag(char *s);
	void ZeroTableElements();
	bool OnUnhandledColor(GCss::ColorDef *def, const char *&s);
	void CenterText();
	bool Serialize(GXmlTag *t, bool Write);
	
	GColour _Colour(bool Fore);
	COLOUR GetFore() { return _Colour(true).c24(); }
	COLOUR GetBack() { return _Colour(false).c24(); }

public:
	// Object
	GString::Array Class;
	const char *HtmlId;

	GAutoString Condition;
	int TipId;

	// Heirarchy
	GHtml *Html;
	bool IsBlock() { return Display() == GCss::DispBlock; }
	GTag *GetBlockParent(ssize_t *Idx = 0);
	GFont *GetFont();

	// Style
	GdcPt2 Pos;
	GdcPt2 Size;
	GFont *Font;
	int LineHeightCache;
	GRect PadPx;
	
	// Images
	bool ImageResized;
	GAutoPtr<GSurface> Image;
	void SetImage(const char *uri, GSurface *i);
	void LoadImage(const char *Uri); // Load just this URI
	void LoadImages(); // Recursive load all image URI's
	void ImageLoaded(char *uri, GSurface *img, int &Used);

	// Table stuff
	struct TblCell
	{
		GdcPt2 Pos;
		GdcPt2 Span;
		GRect BorderPx;
		GRect PaddingPx;
		uint16 MinContent, MaxContent;
		GCss::LengthType XAlign;
		GHtmlTableLayout *Cells;
		
		TblCell()
		{
			Cells = NULL;
			MinContent = 0;
			MaxContent = 0;
			XAlign = GCss::LenInherit;
			BorderPx.ZOff(0, 0);
			PaddingPx.ZOff(0, 0);
		}
		
		~TblCell()
		{
			DeleteObj(Cells);
		}
		
	}	*Cell;

	#ifdef _DEBUG
	int Debug;
	#endif

	// Text
	ssize_t Cursor; // index into text of the cursor
	ssize_t Selection; // index into the text of the selection edge
	GArea TextPos;

	GTag(GHtml *h, GHtmlElement *p);
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

	ssize_t GetTextStart();
	GAutoWString DumpW();
	GAutoString DescribeElement();
	char16 *CleanText(const char *s, ssize_t len, const char *SourceCs, bool ConversionAllowed = true, bool KeepWhiteSpace = false);
	char *ParseText(char *Doc);
	bool ConvertToText(TextConvertState &State);
	
	/// Configures the tag's styles.
	void SetStyle();
	/// Called to apply CSS selectors on initialization and also when properties change at runtime.
	void Restyle();
	/// Recursively call restyle on all nodes in the doc tree
	void RestyleAll();
	
	/// Takes the CSS styles, parses and stores them in the current object,
	//// overwriting any duplicate properties.
	void SetCssStyle(const char *Style);
	/// Positions the tag according to the flow region passed in
	void OnFlow(GFlowRegion *Flow, uint16 Depth);
	/// Paints the border and background of the tag
	void PaintBorderAndBackground(
		/// The surface to paint on
		GSurface *pDC,
		/// The background colour (transparent is OK)
		GColour &Back,
		/// [Optional] The size of the border painted
		GRect *Px = NULL);
	/// This fills 'rgn' with all the rectangles making up the inline tags region
	void GetInlineRegion(GRegion &rgn);
	void OnPaint(GSurface *pDC, bool &InSelection, uint16 Depth);
	void SetSize(GdcPt2 &s);
	void SetTag(const char *Tag);
	void GetTagByPos(GTagHit &TagHit, int x, int y, int Depth, bool InBody, bool DebugLog = false);
	GTag *GetTagByName(const char *Name);
	void CopyClipboard(GMemQueue &p, bool &InSelection);
	GTag *IsAnchor(GAutoString *Uri);
	bool CreateSource(GStringPipe &p, int Depth = 0, bool LastWasBlock = true);
	void Find(int TagType, GArray<GTag*> &Tags);
	GTag *GetAnchor(char *Name);

	// Control handling
	GTag *FindCtrlId(int Id);
	int OnNotify(int f);
	void CollectFormValues(LHashTbl<ConstStrKey<char,false>,char*> &f);

	// GDom impl
	bool GetVariant(const char *Name, GVariant &Value, char *Array = 0);
	bool SetVariant(const char *Name, GVariant &Value, char *Array = 0);

	// Window
	bool OnMouseClick(GMouse &m);
	void Invalidate();

	// Positioning
	int RelX() { return Pos.x + (int)MarginLeft().Value; }
	int RelY() { return Pos.y + (int)MarginTop().Value; }
	GdcPt2 AbsolutePos();
	inline int AbsX() { return AbsolutePos().x; }
	inline int AbsY() { return AbsolutePos().y; }
	GRect GetRect(bool Client = true);
	GCss::LengthType GetAlign(bool x);

	// Tables
	GTag *GetTableCell(int x, int y);
	GdcPt2 GetTableSize();
	void ResetCaches();
};

}

#endif