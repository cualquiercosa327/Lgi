/// \file
/// \author Matthew Allen <fret@memecode.com>
#ifndef _G_CSS_H_
#define _G_CSS_H_

/// I've using the american spelling for 'color' as opposed to the english 'colour'
/// because the CSS spec is written with 'color' as the spelling.

#include "LgiInc.h"
#include "LgiOsDefs.h"
#include "GMem.h"
#include "Gdc2.h"
#include "GAutoPtr.h"
#include "GString.h"
#include "GHashTable.h"

#pragma pack(push, 1)

/// Css property container
class LgiClass GCss
{
public:
	enum ParsingStyle {
		ParseStrict,
		ParseRelaxed,
	};

	enum PropTypes {
		TypeEnum = 1,
		TypeLen,
		TypeGRect,
		TypeColor,
		TypeImage,
		TypeBorder,
		TypeStrings,
	};

	/// These are all the types of CSS properties catagorised by their data type.
	/// The enum value of the prop is encoded in the bottom 8 bits, the data type of 
	/// the property is encoding in the top remaining top bits.
	enum PropType {
		PropNull = 0,

		// Enum based props
		PropDisplay = TypeEnum<<8,	// DisplayType
		PropFloat,					// FloatType
		PropPosition,				// PositionType
		PropOverflow,				// OverflowType
		PropVisibility,				// VisibilityType
		PropFontStyle,				// FontStyleType
		PropFontVariant,			// FontVariantType
		PropFontWeight,				// FontWeightType
		PropBackgroundRepeat,		// RepeatType
		PropBackgroundAttachment,	// AttachmentType
		PropTextDecoration,			// TextDecorType
		PropWordWrap,				// WordWraptype
		PropListStyleType,
		PropLetterSpacing,
		PropFont,
		PropBorderCollapse,			// BorderCollapseType

		// Length based props
		PropZIndex = TypeLen<<8,
		PropWidth,
		PropMinWidth,
		PropMaxWidth,
		PropHeight,
		PropMinHeight,
		PropMaxHeight,
		PropTop,
		PropRight,
		PropBottom,
		PropLeft,
		PropMargin,
		PropMarginTop,
		PropMarginRight,
		PropMarginBottom,
		PropMarginLeft,
		PropPadding,
		PropPaddingTop,
		PropPaddingRight,
		PropPaddingBottom,
		PropPaddingLeft,
		PropLineHeight,
		PropVerticalAlign,
		PropFontSize,
		PropBackgroundX,
		PropBackgroundY,
		PropTextAlign,
		PropBorderSpacing,
		Prop_CellPadding, // not real CSS, but used by GHtml2 to store 'cellpadding'

		// GRect based props
		PropClip = TypeGRect<<8,
		PropXSubRect,

		// ColorDef based
		PropColor = TypeColor<<8,
		PropBackground,
		PropBackgroundColor,

		// ImageDef based
		PropBackgroundImage = TypeImage<<8,
		
		// BorderDef based
		PropBorder = TypeBorder <<8,
		PropBorderTop,
		PropBorderRight,
		PropBorderBottom,
		PropBorderLeft,

		// StringsDef based
		PropFontFamily = TypeStrings<<8,
	};

	enum BorderCollapseType {
		CollapseInherit,
		CollapseCollapse,
		CollapseSeparate,
	};

	enum WordWrapType {
		WrapNormal,
		WrapBreakWord,
	};

	enum DisplayType {
		DispInherit,
		DispBlock,
		DispInline,
		DispInlineBlock,
		DispListItem,
		DispNone,
	};

	enum PositionType {
		PosInherit,
		PosStatic,
		PosRelative,
		PosAbsolute,
		PosFixed,
	};

	enum LengthType {
		
		// Standard length types
		LenInherit,
		LenAuto,
		LenPx,
		LenPt,
		LenEm,
		LenEx,
		LenCm,
		LenPercent,
		LenNormal,

		// Horizontal alignment types
		AlignLeft,
		AlignRight,
		AlignCenter,
		AlignJustify,

		// Vertical alignment types
		VerticalBaseline,
		VerticalSub,
		VerticalSuper,
		VerticalTop,
		VerticalTextTop,
		VerticalMiddle,
		VerticalBottom,
		VerticalTextBottom,

		// Absolute font sizes
		SizeXXSmall,
		SizeXSmall,
		SizeSmall,
		SizeMedium,
		SizeLarge,
		SizeXLarge,
		SizeXXLarge,

		// Relitive font sizes,
		SizeSmaller,
		SizeLarger,
	};

	enum FloatType {
		FloatInherit,
		FloatLeft,
		FloatRight,
		FloatNone,
	};

	enum OverflowType {
		OverflowInherit,
		OverflowVisible,
		OverflowHidden,
		OverflowScroll,
		OverflowAuto,
	};

	enum VisibilityType {
		VisibilityInherit,
		VisibilityVisible,
		VisibilityHidden,
		VisibilityCollapse
	};

	enum FontStyleType {
		FontStyleInherit,
		FontStyleNormal,
		FontStyleItalic,
		FontStyleOblique,
	};

	enum FontVariantType {
		FontVariantInherit,
		FontVariantNormal,
		FontVariantSmallCaps,
	};

	enum FontWeightType {
		FontWeightInherit,
		FontWeightNormal,
		FontWeightBold,
		FontWeightBolder,
		FontWeightLighter,
		FontWeight100,
		FontWeight200,
		FontWeight300,
		FontWeight400,
		FontWeight500,
		FontWeight600,
		FontWeight700,
		FontWeight800,
		FontWeight900,
	};

	enum TextDecorType {
		TextDecorInherit,
		TextDecorNone,
		TextDecorUnderline,
		TextDecorOverline,
		TextDecorLineThrough,
		TextDecorBlink,
	};

	enum ColorType {
		ColorInherit,
		ColorTransparent,
		ColorRgb,
		ColorLinearGradient,
		ColorRadialGradient,
	};

	enum RepeatType {
		RepeatInherit,
		RepeatBoth,
		RepeatX,
		RepeatY,
		RepeatNone,
	};

	enum AttachmentType {
		AttachmentInherit,
		AttachmentScroll,
		AttachmentFixed,
	};

	enum ImageType {
		ImageInherit,
		ImageOwn,
		ImageRef,
	};

	struct LgiClass Len
	{
		LengthType Type;
		float Value;

		Len(const char *init = 0)
		{
			Type = LenInherit;
			Value = 0.0;
			if (init)
				Parse(init);
		}

		Len(LengthType t, float v = 0.0)
		{
			Type = t;
			Value = v;
		}

		bool Parse(const char *&s, ParsingStyle Type = ParseStrict);
		bool IsValid() { return Type != LenInherit; }
		bool IsDynamic() { return Type == LenPercent || Type == LenInherit || Type == SizeSmaller || Type == SizeLarger; }
		bool operator !=(Len &l) { return Type != l.Type || Value != l.Value; }
		bool ToString(GStream &p);
		int ToPx(int Box = 0, GFont *Font = 0, int Dpi = 96);
	};

	struct LgiClass ColorStop
	{
		float Pos;
		uint32 Rgb32;
	};

	struct LgiClass ColorDef
	{
		ColorType Type;
		uint32 Rgb32;
		GArray<ColorStop> Stops;

		ColorDef(int init = 0)
		{
			Type = init ? ColorRgb : ColorInherit;
			Rgb32 = init;
		}

		bool Parse(const char *&s);
		bool operator !=(ColorDef &c)
		{
			return	Type != c.Type ||
					Rgb32 != c.Rgb32;
		}
		ColorDef &operator =(const ColorDef &c)
		{
			Type = c.Type;
			Rgb32 = c.Rgb32;
			Stops = c.Stops;
			return *this;
		}
		bool ToString(GStream &p);
	};

	enum BorderStyle {
		BorderNone,
		BorderHidden,
		BorderDotted,
		BorderDashed,
		BorderSolid,
		BorderDouble,
		BorderGroove,
		BorderRidge,
		BorderInset,
		BorderOutset,
	};

	struct LgiClass BorderDef : public Len
	{
		ColorDef Color;
		BorderStyle Style;

		BorderDef(const char *init = 0)
		{
			Style = BorderNone;
			if (init)
				Parse(init);
		}

		BorderDef(const BorderDef &db) : Len(db)
		{
			Color = db.Color;
			Style = db.Style;
		}
		
		bool Parse(const char *&s);
		BorderDef &operator =(const BorderDef &b)
		{
			Style = b.Style;
			Color = b.Color;
			Type = b.Type;
			Value = b.Value;
			return *this;
		}
	};

	struct LgiClass ImageDef
	{
		ImageType Type;
		GAutoString Ref;
		GSurface *Img;

		ImageDef()
		{
			Type = ImageInherit;
			Img = 0;
		}

		ImageDef(const ImageDef &o)
		{
			*this = o;
		}

		~ImageDef()
		{
			if (Type == ImageOwn)
				DeleteObj(Img);
		}

		bool Parse(const char *&s);
		bool operator !=(const ImageDef &i)
		{
			if (Type != i.Type)
				return false;
			if (Ref && i.Ref)
				return stricmp(Ref, i.Ref) == 0;
			return true;
		}
		
		ImageDef &operator =(const ImageDef &o)
		{
			Type = o.Type;
			Ref.Reset(NewStr(o.Ref));
			DeleteObj(Img);
			if (o.Img) Img = new GMemDC(o.Img);
			return *this;
		}
	};

	class LgiClass StringsDef : public GArray<char*>
	{
	public:
		StringsDef(char *init = 0)
		{
			if (ValidStr(init))
				*this = init;
			else
				LgiAssert(init == 0);
		}
		StringsDef(const StringsDef &c) { *this = c; }
		~StringsDef() { Empty(); }
		StringsDef &operator =(const char *s) { Parse(s); return *this; }
		void Empty() { DeleteArrays(); }

		StringsDef &operator =(const StringsDef &s)
		{
			Empty();

			for (int i=0; i<s.Length(); i++)
			{
				char *str = ((StringsDef&)s)[i];
				if (ValidStr(str))
					Add(NewStr(str));
				else
					LgiAssert(!"Not a valid string.");
			}

			return *this;			
		}

		bool operator !=(StringsDef &s)
		{
			if (Length() != s.Length())
				return true;

			for (int i=0; i<Length(); i++)
			{
				char *a = (*this)[i];
				char *b = s[i];

				if (stricmp(a, b))
					return true;
			}

			return false;
		}

		bool Parse(const char *&s)
		{
			Empty();
			
			char Delimiters[] = ";, \t\r\n";
			while (s && *s && *s != ';')
			{
				while (*s && strchr(Delimiters, *s)) s++;
				if (*s == '\'' || *s == '\"')
				{
					char Delim = *s++;
					const char *Start = s;
					while (*s && *s != Delim) s++;
					
					if (s > Start)
					{
						char *n = NewStr(Start, s-Start);
						if (ValidStr(n))
							Add(n);
						else
							LgiAssert(!"Not a valid string.");
					}

					if (s) s++;
				}
				else
				{
					const char *Start = s;
					while (*s && !strchr(Delimiters, *s)) s++;

					if (s > Start)
					{
						char *n = NewStr(Start, s-Start);
						if (ValidStr(n))
							Add(n);
						else
							LgiAssert(!"Not a valid string.");
					}
				}
			}

			return true;
		}
	};

	/// This class parses and stores a selector. The job of matching selectors and
	/// hashing them is still the resposibility of the calling library.
	class LgiClass Selector
	{
	public:
		enum PartType
		{
			SelNull,
			
			SelType,
			SelUniversal,
			SelAttrib,
			SelClass,
			SelMedia,
			SelID,
			SelPseudo,

			CombDesc,
			CombChild,
			CombAdjacent,
		};
		
		struct Part
		{
			PartType Type;
			GAutoString Value;
			
			bool IsSel()
			{
				return
					Type == SelType ||
					Type == SelUniversal ||
					Type == SelAttrib ||
					Type == SelClass ||
					Type == SelMedia ||
					Type == SelID ||
					Type == SelPseudo;
			}

			Part &operator =(const Part &s)
			{
				Type = s.Type;
				Value.Reset(NewStr(s.Value));
				return *this;
			}
		};
		
		GArray<Part> Parts;
		GArray<int> Combs;
		char *Style;
		#ifdef _DEBUG
		GAutoString Raw;
		#endif

		Selector() { Style = NULL; }
		void TokString(GAutoString &a, const char *&s);
		const char *PartTypeToString(PartType p);
		GAutoString Print();
		bool Parse(const char *&s);
		int GetSimpleIndex() { return Combs.Length() ? Combs[Combs.Length()-1] + 1 : 0; }
		
		Selector &operator =(const Selector &s);
	};

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	GCss();
	GCss(const GCss &c);
	virtual ~GCss();

	#define Accessor(PropName, Type, Default) \
		Type PropName() { Type *Member = (Type*)Props.Find(Prop##PropName); \
							if (Member) return *Member; \
							return Default; } \
		void PropName(Type t) { Type *Member = (Type*)Props.Find(Prop##PropName); \
								if (Member) *Member = t; \
								else { Props.Add(Prop##PropName, Member = new Type); \
										*Member = t; } \
								OnChange(Prop##PropName); }

	Accessor(Display, DisplayType, DispInherit);
	Accessor(Float, FloatType, FloatInherit);
	Accessor(Position, PositionType, PosInherit);
	Accessor(ZIndex, Len, Len());

	Accessor(TextAlign, Len, Len());
	Accessor(VerticalAlign, Len, Len());

	Accessor(Width, Len, Len());
	Accessor(MinWidth, Len, Len());
	Accessor(MaxWidth, Len, Len());

	Accessor(Height, Len, Len());
	Accessor(MinHeight, Len, Len());
	Accessor(MaxHeight, Len, Len());
	Accessor(LineHeight, Len, Len());

	Accessor(Top, Len, Len());
	Accessor(Right, Len, Len());
	Accessor(Bottom, Len, Len());
	Accessor(Left, Len, Len());

	Accessor(MarginTop, Len, Len());
	Accessor(MarginRight, Len, Len());
	Accessor(MarginBottom, Len, Len());
	Accessor(MarginLeft, Len, Len());

	Accessor(PaddingTop, Len, Len());
	Accessor(PaddingRight, Len, Len());
	Accessor(PaddingBottom, Len, Len());
	Accessor(PaddingLeft, Len, Len());

	Accessor(BorderTop, BorderDef, BorderDef());
	Accessor(BorderRight, BorderDef, BorderDef());
	Accessor(BorderBottom, BorderDef, BorderDef());
	Accessor(BorderLeft, BorderDef, BorderDef());
	Accessor(BorderSpacing, Len, Len()); // 'cellspacing'
	Accessor(_CellPadding, Len, Len()); // 'cellpadding' (not CSS)

	Accessor(Overflow, OverflowType, OverflowInherit);
	Accessor(Clip, GRect, GRect(0, 0, -1, -1));
	Accessor(XSubRect, GRect, GRect(0, 0, -1, -1));
	Accessor(Visibility, VisibilityType, VisibilityInherit);

	Accessor(FontFamily, StringsDef, StringsDef());
	Accessor(FontSize, Len, Len());
	Accessor(FontStyle, FontStyleType, FontStyleInherit);
	Accessor(FontVariant, FontVariantType, FontVariantInherit);
	Accessor(FontWeight, FontWeightType, FontWeightInherit);
	Accessor(TextDecoration, TextDecorType, TextDecorInherit);

	Accessor(Color, ColorDef, ColorDef());
	Accessor(BackgroundColor, ColorDef, ColorDef());
	Accessor(BackgroundImage, ImageDef, ImageDef());
	Accessor(BackgroundRepeat, RepeatType, RepeatInherit);
	Accessor(BackgroundAttachment, AttachmentType, AttachmentInherit);
	Accessor(BackgroundX, Len, Len());
	Accessor(BackgroundY, Len, Len());
	
	void Empty();
	void DeleteProp(PropType p);
	virtual void OnChange(PropType Prop);
	virtual bool Parse(const char *&Defs, ParsingStyle Type = ParseStrict);
	bool operator ==(GCss &c);
	bool operator !=(GCss &c) { return !(*this == c); }
	bool CopyStyle(const GCss &c);
	GCss &operator =(const GCss &c) { CopyStyle(c); return *this; }
	void *PropAddress(PropType p) { return Props.Find(p); }
	GAutoString ToString();

    // Inheritance calculation
    typedef GArray<void*> PropArray;
    typedef GHashTbl<int, PropArray*> PropMap;
	bool InheritCollect(GCss &c, PropMap &Contrib);
    bool InheritResolve(PropMap &Map);

protected:
	inline void DeleteProp(PropType p, void *Ptr);
	GHashTbl<int, void*> Props;
	static GHashTbl<const char*, PropType> Lut;
	static const char *PropName(PropType p);

	bool ParseFontStyle(PropType p, const char *&s);
	bool ParseFontVariant(PropType p, const char *&s);
	bool ParseFontWeight(PropType p, const char *&s);
	virtual bool OnUnhandledColor(ColorDef *def, const char *&s) { return false; }

	template<typename T>
	void StoreProp(PropType id, T *obj, bool own)
	{
		T *e = (T*)Props.Find(id);
		if (e)
		{
			*e = *obj;
			if (own)
				delete obj;
		}
		else if (own)
		{
			Props.Add(id, obj);
		}
		else
		{
			Props.Add(id, new T(*obj));
		}
	}
};

#pragma pack(pop)

#endif