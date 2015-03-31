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
		TypeBackground,
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
		PropDisplay = TypeEnum << 8,// DisplayType
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
		PropListStyleType,			// ListStyleTypes
		PropLetterSpacing,
		PropFont,
		PropListStyle,
		PropBorderCollapse,			// BorderCollapseType
		PropBorderTopStyle,
		PropBorderRightStyle,
		PropBorderBottomStyle,
		PropBorderLeftStyle,

		// Length based props
		PropZIndex = TypeLen << 8,
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
		PropBackgroundPos,
		PropTextAlign,
		PropBorderSpacing,
		PropBorderLeftWidth,
		PropBorderTopWidth,
		PropBorderRightWidth,
		PropBorderBottomWidth,
		Prop_CellPadding, // not real CSS, but used by GHtml2 to store 'cellpadding'

		// GRect based props
		PropClip = TypeGRect<<8,
		PropXSubRect,

		// Background meta style
		PropBackground = TypeBackground << 8,

		// ColorDef based
		PropColor = TypeColor << 8,
		PropBackgroundColor,
		PropBorderTopColor,
		PropBorderRightColor,
		PropBorderBottomColor,
		PropBorderLeftColor,

		// ImageDef based
		PropBackgroundImage = TypeImage << 8,
		
		// BorderDef based
		PropBorder = TypeBorder << 8,
		PropBorderStyle,
		PropBorderColor,
		PropBorderTop,
		PropBorderRight,
		PropBorderBottom,
		PropBorderLeft,

		// StringsDef based
		PropFontFamily = TypeStrings << 8,
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
	
	enum ListStyleTypes {
		ListInherit,
		ListNone,
		ListDisc,
		ListCircle,
		ListSquare,
		ListDecimal,
		ListDecimalLeadingZero,
		ListLowerRoman,
		ListUpperRoman,
		ListLowerGreek,
		ListUpperGreek,
		ListLowerAlpha,
		ListUpperAlpha,
		ListArmenian,
		ListGeorgian,
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

	#define _FloatAbs(a) \
		(((a) < 0.0f) ? -(a) : (a))
	#define FloatIsEqual(a, b) \
		(_FloatAbs(a - b) < 0.0001)

	struct LgiClass Len
	{
		LengthType Type;
		float Value;

		Len(const char *init = 0)
		{
			Type = LenInherit;
			Value = 0.0;
			if (init)
				Parse(init, PropNull);
		}

		Len(LengthType t, float v = 0.0)
		{
			Type = t;
			Value = v;
		}
		
		Len &operator =(const Len &l)
		{
			Type = l.Type;
			Value = l.Value;
			return *this;
		}

		bool Parse(const char *&s, PropType Prop = PropNull, ParsingStyle Type = ParseStrict);
		bool IsValid() { return Type != LenInherit; }
		bool IsDynamic() { return	Type == LenPercent ||
									Type == LenInherit ||
									Type == LenAuto ||
									Type == SizeSmaller ||
									Type == SizeLarger; }
		bool operator ==(const Len &l) { return Type == l.Type && FloatIsEqual(Value, l.Value); }
		bool operator !=(const Len &l) { return !(*this == l); }
		bool ToString(GStream &p);
		int ToPx(int Box = 0, GFont *Font = 0, int Dpi = -1);
	};

	struct LgiClass ColorStop
	{
		float Pos;
		uint32 Rgb32;
		
		bool operator ==(const ColorStop &c)
		{
			return FloatIsEqual(Pos, c.Pos) && Rgb32 == c.Rgb32;
		}
		bool operator !=(const ColorStop &c) { return !(*this == c); }
	};

	struct LgiClass ColorDef
	{
		ColorType Type;
		uint32 Rgb32;
		GArray<ColorStop> Stops;

		ColorDef(ColorType ct = ColorInherit, uint32 rgb32 = 0)
		{
			Type = ct;
			Rgb32 = rgb32;
		}

		bool IsValid() { return Type != ColorInherit; }
		bool Parse(const char *&s);
		ColorDef &operator =(const ColorDef &c)
		{
			Type = c.Type;
			Rgb32 = c.Rgb32;
			Stops = c.Stops;
			return *this;
		}
		bool operator ==(const ColorDef &c)
		{
			if (Type != c.Type)
				return false;
			if (Type == ColorRgb)
				return Rgb32 == c.Rgb32;
			if (Stops.Length() != c.Stops.Length())
				return false;
			for (uint32 i=0; i<Stops.Length(); i++)
			{
				if (Stops[i] != c.Stops.ItemAt(i))
					return false;
			}
			return true;
		}
		bool operator !=(const ColorDef &c) { return !(*this == c); }
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
		bool Important;

		BorderDef(GCss *css = 0, const char *init = 0)
		{
			Style = BorderNone;
			Important = true;
			if (init)
				Parse(css, init);
		}

		BorderDef(const BorderDef &db) : Len(db)
		{
			Color = db.Color;
			Style = db.Style;
			Important = db.Important;
		}
		
		bool Parse(GCss *Css, const char *&s);
		bool ParseStyle(const char *&s);
		
		BorderDef &operator =(const BorderDef &b)
		{
			Style = b.Style;
			Color = b.Color;
			Type = b.Type;
			Value = b.Value;
			Important = b.Important;
			return *this;
		}
	};

	enum ImageType {
		ImageInherit,
		ImageNone,
		ImageUri,
		ImageOwn,
		ImageRef,
	};

	struct LgiClass ImageDef
	{
		ImageType Type;
		GAutoString Uri;
		GSurface *Img;

		ImageDef()
		{
			Type = ImageInherit;
			Img = 0;
		}

		ImageDef(const ImageDef &o)
		{
			Img = NULL;
			*this = o;
		}

		~ImageDef()
		{
			if (Type == ImageOwn)
				DeleteObj(Img);
		}

		bool Parse(const char *&s);
		bool operator !=(const ImageDef &i);		
		ImageDef &operator =(const ImageDef &o);
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

			for (unsigned i=0; i<s.Length(); i++)
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

			for (unsigned i=0; i<Length(); i++)
			{
				char *a = (*this)[i];
				char *b = s[i];

				if (_stricmp(a, b))
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
						GAutoString n(NewStr(Start, s-Start));
						if (ValidStr(n))
						{
							if (_stricmp(n, "inherit"))
								Add(n.Release());
						}
						else LgiAssert(!"Not a valid string.");
					}

					if (s) s++;
				}
				else
				{
					const char *Start = s;
					while (*s && !strchr(Delimiters, *s)) s++;

					if (s > Start)
					{
						GAutoString n(NewStr(Start, s-Start));
						if (ValidStr(n))
						{
							if (_stricmp(n, "inherit"))
								Add(n.Release());
						}
						else LgiAssert(!"Not a valid string.");
					}
				}
			}

			return true;
		}
	};
	
	class Store;

	/// This class parses and stores a selector. The job of matching selectors and
	/// hashing them is still the responsibility of the calling library. If an application
	/// needs some code to do that it can optionally use GCss::Store to do that.
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
		
		enum MediaType
		{
			MediaNull		= 0x0,
			MediaPrint		= 0x1,
			MediaScreen		= 0x2,
		};
		
		struct Part
		{
			PartType Type;
			GAutoString Value;
			GAutoString Param;
			int Media;
			
			Part()
			{
				Type = SelNull;
				Media = MediaNull;
			}
			
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
				Param.Reset(NewStr(s.Param));
				return *this;
			}
		};
		
		GArray<Part> Parts;
		GArray<int> Combs;
		char *Style;
		GAutoString Raw;
		GAutoPtr<class Store> Children;

		Selector() { Style = NULL; }
		bool TokString(GAutoString &a, const char *&s);
		const char *PartTypeToString(PartType p);
		GAutoString Print();
		bool Parse(const char *&s);
		int GetSimpleIndex() { return Combs.Length() ? Combs[Combs.Length()-1] + 1 : 0; }
		bool IsAtMedia();
		bool ToString(GStream &p);
		
		Selector &operator =(const Selector &s);
	};

	/// This hash table stores arrays of selectors by name.
	typedef GArray<GCss::Selector*> SelArray;
	class SelectorMap : public GHashTbl<const char*,SelArray*>
	{
	public:
		SelectorMap() : GHashTbl<const char*,SelArray*>(0, false) {}		
		~SelectorMap() { Empty(); }
	
		void Empty()
		{
			for (SelArray *s = First(); s; s = Next())
			{
				s->DeleteObjects();
				delete s;
			}	
			GHashTbl<const char*,SelArray*>::Empty();
		}
		
		SelArray *Get(const char *s)
		{
			SelArray *a = Find(s);
			if (!a)
				Add(s, a = new SelArray);
			return a;
		}
	};
	
	template<typename T>
	struct ElementCallback
	{
	public:
		/// Returns the element name
		virtual const char *GetElement(T *obj) = 0;
		/// Returns the document unque element ID
		virtual const char *GetAttr(T *obj, const char *Attr) = 0;
		/// Returns the class
		virtual bool GetClasses(GArray<const char *> &Classes, T *obj) = 0;
		/// Returns the parent object
		virtual T *GetParent(T *obj) = 0;
		/// Returns an array of child objects
		virtual GArray<T*> GetChildren(T *obj) = 0;
	};
	
	/// This class parses and stores the CSS selectors and styles.
	class LgiClass Store
	{
	protected:
		/// This code matches a simple part of a selector, i.e. no combinatorial operators involved.
		template<typename T>
		bool MatchSimpleSelector
		(
			/// The full selector.
			GCss::Selector *Sel,
			/// The start index of the simple selector parts. Stop at the first comb operator or the end of the parts.
			int PartIdx,
			/// Our context callback to get properties of the object
			ElementCallback<T> *Context,
			/// The object to match
			T *Obj
		)
		{
			const char *Element = Context->GetElement(Obj);
			
			for (unsigned n = PartIdx; n<Sel->Parts.Length(); n++)
			{
				GCss::Selector::Part &p = Sel->Parts[n];
				switch (p.Type)
				{
					case GCss::Selector::SelType:
					{
						const char *Tag = Context->GetElement(Obj);
						if (!Tag || _stricmp(Tag, p.Value))
							return false;
						break;
					}
					case GCss::Selector::SelUniversal:
					{
						// Match everything
						return true;
						break;
					}
					case GCss::Selector::SelAttrib:
					{
						if (!p.Value)
							return false;

						char *e = strchr(p.Value, '=');
						if (!e)
							return false;

						GAutoString Var(NewStr(p.Value, e - p.Value));
						const char *TagVal = Context->GetAttr(Obj, Var);
						if (!TagVal)
							return false;

						GAutoString Val(NewStr(e + 1));
						if (_stricmp(Val, TagVal))
							return false;
						break;
					}
					case GCss::Selector::SelClass:
					{
						// Check the class matches
						GArray<const char *> Class;
						if (!Context->GetClasses(Class, Obj))
							return false;

						if (Class.Length() == 0)
							return false;

						bool Match = false;
						for (unsigned i=0; i<Class.Length(); i++)
						{
							if (!_stricmp(Class[i], p.Value))
							{
								Match = true;
								break;
							}
						}
						if (!Match)
							return false;
						break;
					}
					case GCss::Selector::SelMedia:
					{
						return false;
						break;
					}
					case GCss::Selector::SelID:
					{
						const char *Id = Context->GetAttr(Obj, "id");
						if (!Id || _stricmp(Id, p.Value))
							return false;
						break;
					}
					case GCss::Selector::SelPseudo:
					{
						const char *Href = NULL;
						if
						(
							(
								!_stricmp(Element, "a")
								&&
								p.Value && !_stricmp(p.Value, "link")
								&&
								(Href = Context->GetAttr(Obj, "href")) != NULL
							)
							||
							(
								p.Value
								&&
								*p.Value == '-'
							)
						)
							break;
							
						return false;
						break;
					}
					default:
					{
						// Comb operator, so return the current match value
						return true;
					}
				}
			}
			
			return true;
		}
		
		/// This code matches a all the parts of a selector.
		template<typename T>
		bool MatchFullSelector(GCss::Selector *Sel, ElementCallback<T> *Context, T *Obj)
		{
			bool Complex = Sel->Combs.Length() > 0;
			int CombIdx = Complex ? Sel->Combs.Length() - 1 : 0;
			uint32 StartIdx = (Complex) ? Sel->Combs[CombIdx] + 1 : 0;
			
			bool Match = MatchSimpleSelector(Sel, StartIdx, Context, Obj);
			if (!Match)
				return false;

			if (Complex)
			{
				T *CurrentParent = Context->GetParent(Obj);
				
				for (; CombIdx >= 0; CombIdx--)
				{
					if (CombIdx >= (int)Sel->Combs.Length())
						break;

					StartIdx = Sel->Combs[CombIdx];
					LgiAssert(StartIdx > 0);

					if (StartIdx >= Sel->Parts.Length())
						break;
					
					GCss::Selector::Part &p = Sel->Parts[StartIdx];
					switch (p.Type)
					{
						case GCss::Selector::CombChild:
						{
							// LgiAssert(!"Not impl.");
							return false;
							break;
						}
						case GCss::Selector::CombAdjacent:
						{
							// LgiAssert(!"Not impl.");
							return false;
							break;
						}
						case GCss::Selector::CombDesc:
						{
							// Does the parent match the previous simple selector
							int PrevIdx = StartIdx - 1;
							while (PrevIdx > 0 && Sel->Parts[PrevIdx-1].IsSel())
							{
								PrevIdx--;
							}
							bool ParMatch = false;
							for (; !ParMatch && CurrentParent; CurrentParent = Context->GetParent(CurrentParent))
							{
								ParMatch = MatchSimpleSelector(Sel, PrevIdx, Context, CurrentParent);
							}
							if (!ParMatch)
								return false;
							break;
						}
						default:
						{
							LgiAssert(!"This must be a comb.");
							return false;
							break;
						}
					}
				}
			}

			return Match;
		}

		// This stores the unparsed style strings. More than one selector
		// may reference this memory.
		GArray<char*> Styles;
		
	public:
		SelectorMap TypeMap, ClassMap, IdMap;
		SelArray Other;
		GAutoString Error;

		~Store()
		{
			Empty();
		}

		/// Empty the data store
		void Empty()
		{
			TypeMap.Empty();
			ClassMap.Empty();
			IdMap.Empty();
			Other.DeleteObjects();
			Error.Reset();

			Styles.DeleteArrays();
		}

		/// Parse general CSS into selectors.		
		bool Parse(const char *&s, int Depth = 0);

		/// Converts store back into string form
		bool ToString(GStream &p);

		/// Use to finding matching selectors for an element.
		template<typename T>
		bool Match(GCss::SelArray &Styles, ElementCallback<T> *Context, T *Obj)
		{
			SelArray *s;

			if (!Context || !Obj)
				return false;
			
			// An array of potential selector matches...
			GArray<SelArray*> Maps;

			// Check element type
			const char *Type = Context->GetElement(Obj);
			if (Type && (s = TypeMap.Find(Type)))
				Maps.Add(s);
			
			// Check the ID
			const char *Id = Context->GetAttr(Obj, "id");
			if (Id && (s = IdMap.Find(Id)))
				Maps.Add(s);
			
			// Check all the classes
			GArray<const char *> Classes;
			Context->GetClasses(Classes, Obj);
			for (unsigned i=0; i<Classes.Length(); i++)
			{
				if ((s = ClassMap.Find(Classes[i])))
					Maps.Add(s);
			}

			// Now from the list of possibles, do the actual checking of selectors...
			for (unsigned i=0; i<Maps.Length(); i++)
			{
				GCss::SelArray *s = Maps[i];
				for (unsigned i=0; i<s->Length(); i++)
				{
					GCss::Selector *Sel = (*s)[i];
					
					if (MatchFullSelector(Sel, Context, Obj))
					{
						// Output the matching selector
						Styles.Add(Sel);
					}
				}
			}

			return true;
		}

		/// For debugging: dumps a description of the store to a stream
		bool Dump(GStream &out);
	};

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	GCss();
	GCss(const GCss &c);
	virtual ~GCss();

	#define Accessor(PropName, Type, Default, BaseProp) \
		Type PropName() { Type *Member = (Type*)Props.Find(Prop##PropName); \
							if (Member) return *Member; \
							else if ((Member = (Type*)Props.Find(BaseProp))) return *Member; \
							return Default; } \
		void PropName(Type t) { Type *Member = (Type*)Props.Find(Prop##PropName); \
								if (Member) *Member = t; \
								else { Props.Add(Prop##PropName, Member = new Type); \
										*Member = t; } \
								OnChange(Prop##PropName); }

	Accessor(Display, DisplayType, DispInherit, PropNull);
	Accessor(Float, FloatType, FloatInherit, PropNull);
	Accessor(Position, PositionType, PosInherit, PropNull);
	Accessor(ZIndex, Len, Len(), PropNull);

	Accessor(TextAlign, Len, Len(), PropNull);
	Accessor(VerticalAlign, Len, Len(), PropNull);

	Accessor(Width, Len, Len(), PropNull);
	Accessor(MinWidth, Len, Len(), PropNull);
	Accessor(MaxWidth, Len, Len(), PropNull);

	Accessor(Height, Len, Len(), PropNull);
	Accessor(MinHeight, Len, Len(), PropNull);
	Accessor(MaxHeight, Len, Len(), PropNull);
	Accessor(LineHeight, Len, Len(), PropNull);

	Accessor(Top, Len, Len(), PropNull);
	Accessor(Right, Len, Len(), PropNull);
	Accessor(Bottom, Len, Len(), PropNull);
	Accessor(Left, Len, Len(), PropNull);

	Accessor(Margin, Len, Len(), PropNull);
	Accessor(MarginTop, Len, Len(), PropNull);
	Accessor(MarginRight, Len, Len(), PropNull);
	Accessor(MarginBottom, Len, Len(), PropNull);
	Accessor(MarginLeft, Len, Len(), PropNull);

	Accessor(Padding, Len, Len(), PropNull);
	Accessor(PaddingTop, Len, Len(), PropPadding);
	Accessor(PaddingRight, Len, Len(), PropPadding);
	Accessor(PaddingBottom, Len, Len(), PropPadding);
	Accessor(PaddingLeft, Len, Len(), PropPadding);

	Accessor(Border, BorderDef, BorderDef(), PropNull);
	Accessor(BorderTop, BorderDef, BorderDef(), PropBorder);
	Accessor(BorderRight, BorderDef, BorderDef(), PropBorder);
	Accessor(BorderBottom, BorderDef, BorderDef(), PropBorder);
	Accessor(BorderLeft, BorderDef, BorderDef(), PropBorder);
	Accessor(BorderSpacing, Len, Len(), PropNull); // 'cellspacing'
	Accessor(BorderCollapse, BorderCollapseType, CollapseInherit, PropBorderCollapse);
	Accessor(_CellPadding, Len, Len(), PropNull); // 'cellpadding' (not CSS)

	Accessor(Overflow, OverflowType, OverflowInherit, PropNull);
	Accessor(Clip, GRect, GRect(0, 0, -1, -1), PropNull);
	Accessor(XSubRect, GRect, GRect(0, 0, -1, -1), PropNull);
	Accessor(Visibility, VisibilityType, VisibilityInherit, PropNull);
	Accessor(ListStyleType, ListStyleTypes, ListInherit, PropNull);

	Accessor(FontFamily, StringsDef, StringsDef(), PropNull);
	Accessor(FontSize, Len, Len(), PropNull);
	Accessor(FontStyle, FontStyleType, FontStyleInherit, PropNull);
	Accessor(FontVariant, FontVariantType, FontVariantInherit, PropNull);
	Accessor(FontWeight, FontWeightType, FontWeightInherit, PropNull);
	Accessor(TextDecoration, TextDecorType, TextDecorInherit, PropNull);

	Accessor(Color, ColorDef, ColorDef(), PropNull);
	Accessor(BackgroundColor, ColorDef, ColorDef(), PropNull);
	Accessor(BackgroundImage, ImageDef, ImageDef(), PropNull);
	Accessor(BackgroundRepeat, RepeatType, RepeatInherit, PropNull);
	Accessor(BackgroundAttachment, AttachmentType, AttachmentInherit, PropNull);
	Accessor(BackgroundX, Len, Len(), PropNull);
	Accessor(BackgroundY, Len, Len(), PropNull);
	Accessor(BackgroundPos, Len, Len(), PropNull);
	
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

	template<typename T>
	T *GetOrCreate(T *&ptr, int PropId)
	{
		ptr = (T*)Props.Find(PropId);
		if (!ptr)
			Props.Add(PropId, ptr = new T);
		return ptr;
	}

    // Inheritance calculation
    typedef GArray<void*> PropArray;
    typedef GHashTbl<int, PropArray*> PropMap;
	
	/// Copies valid properties from the node 'c' into the property collection 'Contrib'.
	/// Usually called for each node up the parent chain until the function returns false;
	bool InheritCollect(GCss &c, PropMap &Contrib);
	/// After calling InheritCollect on all the parent nodes, this method works out the final
	/// value of each property. e.g. multiplying percentages together etc.
    bool InheritResolve(PropMap &Map);
    /* Code sample:
    GCss::PropMap Map;
    Map.Add(PropFontFamily, new GCss::PropArray);
	Map.Add(PropFontSize, new GCss::PropArray);
	Map.Add(PropFontStyle, new GCss::PropArray);
	for (GTag *t = Parent; t; t = t->Parent)
	{
		if (!c.InheritCollect(*t, Map))
			break;
	}	
	GCss c; // Container for final values
	c.InheritResolve(Map);	
	Map.DeleteObjects();
	*/

protected:
	inline void DeleteProp(PropType p, void *Ptr);
	GHashTbl<int, void*> Props;

	static GHashTbl<const char*, PropType> Lut;
	static GHashTbl<int, PropType> ParentProp;

	static const char *PropName(PropType p);

	bool ParseFontStyle(PropType p, const char *&s);
	bool ParseFontVariant(PropType p, const char *&s);
	bool ParseFontWeight(PropType p, const char *&s);
	bool ParseBackgroundRepeat(const char *&s);
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