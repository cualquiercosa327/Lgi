#include <ctype.h>
#include "GCss.h"
#include "LgiCommon.h"

#define SkipWhite(s) 	while (*s && strchr(WhiteSpace, *s)) s++;
#define IsNumeric(s)	((*s) && (strchr("-.", *s) || isdigit(*s)))

GHashTbl<char*, GCss::PropType> GCss::Lut(0, false);
char *GCss::PropName(PropType p)
{
	char *s;
	for (PropType t = Lut.First(&s); t; t = Lut.Next(&s))
	{
		if (p == t)
			return s;
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////
bool ParseWord(char *&s, char *word)
{
	char *doc = s;
	while (*doc && *word)
	{
		if (tolower(*doc) == tolower(*word))
		{
			doc++;
			word++;
		}
		else return false;
	}
	
	if (*word)
		return false;

	if (*doc && (isalpha(*doc) || isdigit(*doc)))
		return false;

	s = doc;
	return true;
}

bool ParseProp(char *&s, char *w)
{
	char *doc = s;
	char *word = w;
	while (*doc && *word)
	{
		if (tolower(*doc) == tolower(*word))
		{
			doc++;
			word++;
		}
		else return false;
	}
	
	if (*word)
		return false;

	SkipWhite(doc);
	if (*doc != ':')
		return false;

	s = doc + 1;
	return true;
}

int ParseComponent(char *&s)
{
	int ret = 0;

	SkipWhite(s);

	if (!strnicmp(s, "0x", 2))
	{
		ret = htoi(s);
		while (*s && (isdigit(*s) || *s == 'x' || *s == 'X')) s++;
	}
	else
	{
		ret = atoi(s);
		while (*s && (isdigit(*s) || *s == '.' || *s == '-')) s++;

		SkipWhite(s);
		if (*s == '%')
		{
			s++;
			ret = ret * 255 / 100;
		}
	}

	SkipWhite(s);

	return ret;
}

char *ParseString(char *&s)
{
	char *ret = 0;

	if (*s == '\'' || *s == '\"')
	{
		char delim = *s++;
		char *e = strchr(s, delim);
		if (!e)
			return 0;

		ret = NewStr(s, e - s);
		s = e + 1;
	}
	else
	{
		char *e = s;
		while (*e && (isalpha(*e) || isdigit(*e) || strchr("_-", *e)))
			e++;

		ret = NewStr(s, e - s);
		s = e;
	}

	return ret;
}

/////////////////////////////////////////////////////////////////////////////
GCss::GCss() : Props(0, false, PropNull)
{
	if (Lut.Length() == 0)
	{
		Lut.Add("display", PropDisplay);
		Lut.Add("float", PropFloat);
		Lut.Add("position", PropPosition);
		Lut.Add("overflow", PropOverflow);
		Lut.Add("visibility", PropVisibility);
		Lut.Add("font-size", PropFontSize);
		Lut.Add("font-style", PropFontStyle);
		Lut.Add("font-variant", PropFontVariant);
		Lut.Add("font-weight", PropFontWeight);
		Lut.Add("background-repeat", PropBackgroundRepeat);
		Lut.Add("background-attachment", PropBackgroundAttachment);
		Lut.Add("z-index", PropZIndex);
		Lut.Add("width", PropWidth);
		Lut.Add("min-width", PropMinWidth);
		Lut.Add("max-width", PropMaxWidth);
		Lut.Add("height", PropHeight);
		Lut.Add("min-height", PropMinHeight);
		Lut.Add("max-height", PropMaxHeight);
		Lut.Add("top", PropTop);
		Lut.Add("right", PropRight);
		Lut.Add("bottom", PropBottom);
		Lut.Add("left", PropLeft);
		Lut.Add("margin-top", PropMarginTop);
		Lut.Add("margin-right", PropMarginRight);
		Lut.Add("margin-bottom", PropMarginBottom);
		Lut.Add("margin-left", PropMarginLeft);
		Lut.Add("padding-top", PropPaddingTop);
		Lut.Add("padding-right", PropPaddingRight);
		Lut.Add("padding-bottom", PropPaddingBottom);
		Lut.Add("padding-left", PropPaddingLeft);
		Lut.Add("line-height", PropLineHeight);
		Lut.Add("vertical-align", PropVerticalAlign);
		Lut.Add("fontsize", PropFontSize);
		Lut.Add("background-x", PropBackgroundX);
		Lut.Add("background-y", PropBackgroundY);
		Lut.Add("clip", PropClip);
		Lut.Add("color", PropColor);
		Lut.Add("background", PropBackgroundColor);
		Lut.Add("background-color", PropBackgroundColor);
		Lut.Add("background-image", PropBackgroundImage);
		Lut.Add("font-family", PropFontFamily);
	}
}

GCss::GCss(const GCss &c) : Props(0, false, PropNull)
{
	*this = c;
}

GCss::~GCss()
{
	Empty();
}

GAutoString GCss::ToString()
{
	GStringPipe p;

	PropType Prop;
	for (void *v = Props.First((int*)&Prop); v; v = Props.Next((int*)&Prop))
	{
		switch (Prop >> 8)
		{
			case TypeEnum:
			{
				char *s = 0;
				switch (Prop)
				{
					case PropFontWeight:
					{
						FontWeightType *b = (FontWeightType*)v;
						switch (*b)
						{
							case FontWeightInherit: s = "inherit"; break;
							case FontWeightNormal: s = "normal"; break;
							case FontWeightBold: s = "bold"; break;
							case FontWeightBolder: s = "bolder"; break;
							case FontWeightLighter: s = "lighter"; break;
							case FontWeight100: s = "100"; break;
							case FontWeight200: s = "200"; break;
							case FontWeight300: s = "300"; break;
							case FontWeight400: s = "400"; break;
							case FontWeight500: s = "500"; break;
							case FontWeight600: s = "600"; break;
							case FontWeight700: s = "700"; break;
							case FontWeight800: s = "800"; break;
							case FontWeight900: s = "900"; break;
						}
						if (s) p.Print("font-weight:%s;", s);
						break;
					}
					case PropFontStyle:
					{
						FontStyleType *t = (FontStyleType*)v;
						switch (*t)
						{
							case FontStyleInherit: s = "inherit"; break;
							case FontStyleNormal: s = "normal"; break;
							case FontStyleItalic: s = "italic"; break;
							case FontStyleOblique: s = "oblique"; break;
						}
						if (s) p.Print("font-style:%s;", s);
						break;
					}
					default:
					{
						LgiAssert(!"Impl me.");
						break;
					}
				}
				
				break;
			}
			case TypeLen:
			{
				Len *l = (Len*)v;
				char *Unit = 0;
				char *Name = PropName(Prop);
				switch (l->Type)
				{
					case LenPx: Unit = "px"; break;
					case LenPt: Unit = "pt"; break;
					case LenEm: Unit = "em"; break;
					case LenEx: Unit = "ex"; break;
					case LenPercent: Unit = "%"; break;
				}

				if (Unit)
				{
					p.Print("%s:%g%s;", Name, l->Value, Unit);
				}
				else
				{
					switch (l->Type)
					{
						case LenInherit: Unit = "Inherit"; break;
						case LenAuto: Unit = "Auto"; break;
						case LenNormal: Unit = "Normal"; break;
					}

					if (Unit)
						p.Print("%s:%s;", Name, Unit);
					else
						LgiAssert(!"Impl me.");
				}
				break;
			}
			case TypeGRect:
			{
				LgiAssert(!"Impl me.");
				break;
			}
			case TypeColor:
			{
				LgiAssert(!"Impl me.");
				break;
			}
			case TypeImage:
			{
				LgiAssert(!"Impl me.");
				break;
			}
			case TypeBorder:
			{
				LgiAssert(!"Impl me.");
				break;
			}
			case TypeStrings:
			{
				LgiAssert(!"Impl me.");
				break;
			}
			default:
			{
				LgiAssert(!"Invalid type.");
				break;
			}
		}
	}

	return GAutoString(p.NewStr());
}

bool GCss::ApplyInherit(GCss &c, GArray<PropType> *Types)
{
	int StillInherit = 0;

	if (Types)
	{
		for (int i=0; i<Types->Length(); i++)
		{
			PropType p = (*Types)[i];
			switch (p >> 8)
			{
				#define InheritEnum(prop, type, inherit) \
					case prop: \
					{ \
						type *Mine = (type*)Props.Find(p); \
						if (!Mine || *Mine == inherit) \
						{ \
							type *Theirs = (type*)c.Props.Find(p); \
							if (Theirs) \
							{ \
								if (!Mine) Props.Add(p, Mine = new type); \
								*Mine = *Theirs; \
							} \
							else StillInherit++; \
						} \
						break; \
					}

				#define InheritClass(prop, type, inherit) \
					case prop: \
					{ \
						type *Mine = (type*)Props.Find(p); \
						if (!Mine || Mine->Type == inherit) \
						{ \
							type *Theirs = (type*)c.Props.Find(p); \
							if (Theirs) \
							{ \
								if (!Mine) Props.Add(p, Mine = new type); \
								*Mine = *Theirs; \
							} \
							else StillInherit++; \
						} \
						break; \
					}



				case TypeEnum:
				{
					switch (p)
					{
						InheritEnum(PropFontStyle, FontStyleType, FontStyleInherit);
						InheritEnum(PropFontVariant, FontVariantType, FontVariantInherit);
						InheritEnum(PropFontWeight, FontWeightType, FontWeightInherit);
						InheritEnum(PropTextDecoration, TextDecorType, TextDecorInherit);
						default:
						{
							LgiAssert(!"Not impl.");
							break;
						}
					}
					break;
				}
				case TypeLen:
				{
					Len *Mine = (Len*)Props.Find(p);
					if (!Mine || Mine->Type == LenInherit || Mine->Type == LenPercent)
					{
						Len *Theirs = (Len*)c.Props.Find(p);
						if (Theirs && Theirs->Type != LenInherit)
						{
							if (!Mine) Props.Add(p, Mine = new Len);
							if (Mine->Type == LenPercent)
							{
								Mine->Type = Theirs->Type;
								Mine->Value = Theirs->Value * Mine->Value / 100.0;
							}
							else if (Mine->Type == LenEm)
							{
								LgiAssert(!"Impl me.");
							}
							else
							{
								*Mine = *Theirs;
							}
						}
						else StillInherit++;
					}
					break;
				}
				case TypeGRect:
				{
					GRect *Mine = (GRect*)Props.Find(p);
					if (!Mine || !Mine->Valid())
					{
						GRect *Theirs = (GRect*)c.Props.Find(p);
						if (Theirs)
						{
							if (!Mine) Props.Add(p, Mine = new GRect);
							*Mine = *Theirs;
						}
						else StillInherit++;
					}
					break;
				}
				case TypeStrings:
				{
					StringsDef *Mine = (StringsDef*)Props.Find(p);
					if (!Mine || Mine->Length() == 0)
					{
						StringsDef *Theirs = (StringsDef*)c.Props.Find(p);
						if (Theirs)
						{
							if (!Mine) Props.Add(p, Mine = new StringsDef);
							*Mine = *Theirs;
						}
						else StillInherit++;
					}
					break;
				}
				InheritClass(TypeColor, ColorDef, ColorInherit);
				InheritClass(TypeImage, ImageDef, ImageInherit);
				InheritClass(TypeBorder, BorderDef, LenInherit);
				default:
				{
					LgiAssert(!"Not impl.");
					break;
				}
			}
		}
	}
	else LgiAssert(!"Not impl.");

	return StillInherit > 0;
}

bool GCss::CopyStyle(const GCss &c)
{
	Empty();

	int Prop;
	GCss &cc = (GCss&)c;
	for (void *p=cc.Props.First(&Prop); p; p=cc.Props.Next(&Prop))
	{
		switch (Prop >> 8)
		{
			#define CopyProp(TypeId, Type) \
				case TypeId: \
				{ \
					Type *n = new Type; \
					*n = *(Type*)p; \
					Props.Add(Prop, n); \
					break; \
				}

			case TypeEnum:
			{
				void *n = new DisplayType;
				*(uint32*)n = *(uint32*)p;
				Props.Add(Prop, n);
				break;
			}
			CopyProp(TypeLen, Len);
			CopyProp(TypeGRect, GRect);
			CopyProp(TypeColor, ColorDef);
			CopyProp(TypeImage, ImageDef);
			CopyProp(TypeBorder, BorderDef);
			CopyProp(TypeStrings, StringsDef);
			default:
			{
				LgiAssert(!"Invalidate property type.");
				return false;
			}
		}
	}

	return true;
}

bool GCss::operator ==(GCss &c)
{
	if (Props.Length() != c.Props.Length())
		return false;

	// Check individual types
	bool Eq = true;
	PropType Prop;
	for (void *Local=Props.First((int*)&Prop); Local && Eq; Local=Props.Next((int*)&Prop))
	{
		void *Other = c.Props.Find(Prop);
		if (!Other)
			return false;

		switch (Prop >> 8)
		{
			#define CmpType(id, type) \
				case id: \
				{ \
					if ( *((type*)Local) != *((type*)Other)) \
						Eq = false; \
					break; \
				}

			CmpType(TypeEnum, uint32);
			CmpType(TypeLen, Len);
			CmpType(TypeGRect, GRect);
			CmpType(TypeColor, ColorDef);
			CmpType(TypeImage, ImageDef);
			CmpType(TypeBorder, BorderDef);
			CmpType(TypeStrings, StringsDef);
			default:
				LgiAssert(!"Unknown type.");
				break;
		}
	}

	return Eq;
}

void GCss::Empty()
{
	int Prop;
	for (void *Data=Props.First(&Prop); Data; Data=Props.Next(&Prop))
	{
		int Type = Prop >> 8;
		switch (Type)
		{
			case TypeEnum:
				delete ((PropType*)Data);
				break;
			case TypeLen:
				delete ((Len*)Data);
				break;
			case TypeGRect:
				delete ((GRect*)Data);
				break;
			case TypeColor:
				delete ((ColorDef*)Data);
				break;
			case TypeImage:
				delete ((ImageDef*)Data);
				break;
			case TypeBorder:
				delete ((BorderDef*)Data);
				break;
			case TypeStrings:
				delete ((StringsDef*)Data);
				break;
			default:
				LgiAssert(!"Unknown property type.");
				break;
		}
	}
	Props.Empty();
}

void GCss::OnChange(PropType Prop)
{
}

bool GCss::Parse(char *&s)
{
	if (!s) return false;

	while (*s && *s != '}')
	{
		// Parse the prop name out
		SkipWhite(s);
		char Prop[32], *p = Prop;
		if (!*s) break;
		while (*s && (isalpha(*s) || strchr("-_", *s)))
			*p++ = *s++;
		*p++ = 0;
		SkipWhite(s);
		if (*s != ':')
			return false;
		s++;			
		PropType PropId = Lut.Find(Prop);
		int PropType = (int)PropId >> 8;
		SkipWhite(s);

		// Do the data parsing based on type
		switch (PropType)
		{
			case TypeEnum:
			{
				switch (PropId)
				{
					case PropDisplay:
					{
						DisplayType *t = (DisplayType*)Props.Find(PropId);
						if (!t) Props.Add(PropId, t = new DisplayType);
						
						if (ParseWord(s, "block")) *t = DispBlock;
						else if (ParseWord(s, "inline")) *t = DispInline;
						else if (ParseWord(s, "inline-block")) *t = DispInlineBlock;
						else if (ParseWord(s, "list-item")) *t = DispListItem;
						else if (ParseWord(s, "none")) *t = DispNone;
						else *t = DispInherit;
						break;
					}
					case PropPosition:
					{
						PositionType *t = (PositionType*)Props.Find(PropId);
						if (!t) Props.Add(PropId, t = new PositionType);
						
						if (ParseWord(s, "static")) *t = PosStatic;
						else if (ParseWord(s, "relative")) *t = PosRelative;
						else if (ParseWord(s, "absolute")) *t = PosAbsolute;
						else if (ParseWord(s, "fixed")) *t = PosFixed;
						else *t = PosInherit;
						break;
					}
					case PropFloat:
					{
						FloatType *t = (FloatType*)Props.Find(PropId);
						if (!t) Props.Add(PropId, t = new FloatType);
						
						if (ParseWord(s, "left")) *t = FloatLeft;
						else if (ParseWord(s, "right")) *t = FloatRight;
						else if (ParseWord(s, "none")) *t = FloatNone;
						else *t = FloatInherit;
						break;
					}
					case PropFontWeight:
					{
						FontWeightType *w = (FontWeightType*)Props.Find(PropId);
						if (!w) Props.Add(PropId, w = new FontWeightType);

						if (ParseWord(s, "Inherit")) *w = FontWeightInherit;
						else if (ParseWord(s, "Normal")) *w = FontWeightNormal;
						else if (ParseWord(s, "Bold")) *w = FontWeightBold;
						else if (ParseWord(s, "Bolder")) *w = FontWeightBolder;
						else if (ParseWord(s, "Lighter")) *w = FontWeightLighter;
						else if (ParseWord(s, "100")) *w = FontWeight100;
						else if (ParseWord(s, "200")) *w = FontWeight200;
						else if (ParseWord(s, "300")) *w = FontWeight300;
						else if (ParseWord(s, "400")) *w = FontWeight400;
						else if (ParseWord(s, "500")) *w = FontWeight500;
						else if (ParseWord(s, "600")) *w = FontWeight600;
						else if (ParseWord(s, "700")) *w = FontWeight700;
						else if (ParseWord(s, "800")) *w = FontWeight800;
						else if (ParseWord(s, "900")) *w = FontWeight900;
						break;
					}
					default:
					{
						LgiAssert(!"Prop parsing support not implemented.");
					}						
				}
				break;
			}
			case TypeLen:
			{
				GAutoPtr<Len> t(new Len);
				if (t->Parse(s, PropId == PropZIndex))
				{
					Len *e = (Len*)Props.Find(PropId);
					if (e) *e = *t;
					else Props.Add(PropId, t.Release());
				}
				else LgiAssert(!"Parsing failed.");
				break;
			}
			case TypeColor:
			{
				GAutoPtr<ColorDef> t(new ColorDef);
				if (t->Parse(s))
				{
					ColorDef *e = (ColorDef*)Props.Find(PropId);
					if (e) *e = *t;
					else Props.Add(PropId, t.Release());
				}
				else LgiAssert(!"Parsing failed.");
				break;
			}
			case TypeStrings:
			{
				GAutoPtr<StringsDef> t(new StringsDef);
				if (t->Parse(s))
				{
					StringsDef *e = (StringsDef*)Props.Find(PropId);
					if (e) *e = *t;
					else Props.Add(PropId, t.Release());
				}
				else LgiAssert(!"Parsing failed.");
				break;
			}
			default:
			{
				LgiAssert(!"Unsupported property type.");
				break;
			}
		}

	
		/*
		// Parse individual properties
		if (ParseProp(s, "width"))
			_width.Parse(s);
		else if (ParseProp(s, "min-width"))
			min_width.Parse(s);
		else if (ParseProp(s, "max-width"))
			max_width.Parse(s);
		else if (ParseProp(s, "height"))
			_height.Parse(s);
		else if (ParseProp(s, "min-height"))
			min_height.Parse(s);
		else if (ParseProp(s, "max-height"))
			max_height.Parse(s);
		else if (ParseProp(s, "top"))
			_top.Parse(s);
		else if (ParseProp(s, "right"))
			_right.Parse(s);
		else if (ParseProp(s, "bottom"))
			_bottom.Parse(s);
		else if (ParseProp(s, "left"))
			_left.Parse(s);
		else if (ParseProp(s, "margin-top"))
			margin_top.Parse(s);
		else if (ParseProp(s, "margin-right"))
			margin_right.Parse(s);
		else if (ParseProp(s, "margin-bottom"))
			margin_bottom.Parse(s);
		else if (ParseProp(s, "margin-left"))
			margin_left.Parse(s);
		else if (ParseProp(s, "padding-top"))
			padding_top.Parse(s);
		else if (ParseProp(s, "padding-right"))
			padding_right.Parse(s);
		else if (ParseProp(s, "padding-bottom"))
			padding_bottom.Parse(s);
		else if (ParseProp(s, "padding-left"))
			padding_left.Parse(s);
		else if (ParseProp(s, "line-height"))
			line_height.Parse(s);
		else if (ParseProp(s, "vertical-align"))
			vertical_align.Parse(s);
		else if (ParseProp(s, "z-index"))
			_zindex.Parse(s);
		else if (ParseProp(s, "font-family"))
			font_family.Reset(ParseString(s));
		else if (ParseProp(s, "font-size"))
			font_size.Parse(s);
		else if (ParseProp(s, "font-style"))
		{
			GAutoString v(ParseString(s));
			if (!v) ;
			else if (!stricmp(v, "inherit"))		font_style = FontStyleInherit;
			else if (!stricmp(v, "normal"))			font_style = FontStyleNormal;
			else if (!stricmp(v, "italic"))			font_style = FontStyleItalic;
			else if (!stricmp(v, "oblique"))		font_style = FontStyleOblique;
		}
		else if (ParseProp(s, "font-variant"))
		{
			GAutoString v(ParseString(s));
			if (!v) ;
			else if (!stricmp(v, "inherit"))		font_variant = FontVariantInherit;
			else if (!stricmp(v, "normal"))			font_variant = FontVariantNormal;
			else if (!stricmp(v, "small-caps"))		font_variant = FontVariantSmallCaps;
		}
		else if (ParseProp(s, "font-weight"))
		{
			GAutoString v(ParseString(s));
			if (!v) ;
			else if (!stricmp(v, "inherit"))		font_weight = FontWeightInherit;
			else if (!stricmp(v, "normal"))			font_weight = FontWeightNormal;
			else if (!stricmp(v, "bold"))			font_weight = FontWeightBold;
			else if (!stricmp(v, "bolder"))			font_weight = FontWeightBolder;
			else if (!stricmp(v, "lighter"))		font_weight = FontWeightLighter;
			else if (!stricmp(v, "100"))			font_weight = FontWeight100;
			else if (!stricmp(v, "200"))			font_weight = FontWeight200;
			else if (!stricmp(v, "300"))			font_weight = FontWeight300;
			else if (!stricmp(v, "400"))			font_weight = FontWeight400;
			else if (!stricmp(v, "500"))			font_weight = FontWeight500;
			else if (!stricmp(v, "600"))			font_weight = FontWeight600;
			else if (!stricmp(v, "700"))			font_weight = FontWeight700;
			else if (!stricmp(v, "800"))			font_weight = FontWeight800;
			else if (!stricmp(v, "900"))			font_weight = FontWeight900;
		}
		else if (ParseProp(s, "color"))
		{
			_colour.Parse(s);
		}
		else if (ParseProp(s, "background-colour") ||
				 ParseProp(s, "background"))
		{
			background_colour.Parse(s);
		}
		else if (ParseProp(s, "background-image"))
		{
			background_image.Parse(s);
		}
		else if (ParseProp(s, "display"))
		{
			GAutoString v(ParseString(s));
			if (!v) ;
			else if (!stricmp(v, "inherit"))		_display = DispInherit;
			else if (!stricmp(v, "block"))			_display = DispBlock;
			else if (!stricmp(v, "inline"))			_display = DispInline;
			else if (!stricmp(v, "inline-block"))	_display = DispInlineBlock;
			else if (!stricmp(v, "list-item"))		_display = DispListItem;
			else if (!stricmp(v, "none"))			_display = DispNone;
		}
		else if (ParseProp(s, "position"))
		{
			GAutoString v(ParseString(s));
			if (!v) ;
			else if (!stricmp(v, "inherit"))		_position = PosInherit;
			else if (!stricmp(v, "static"))			_position = PosStatic;
			else if (!stricmp(v, "relative"))		_position = PosRelative;
			else if (!stricmp(v, "absolute"))		_position = PosAbsolute;
			else if (!stricmp(v, "fixed"))			_position = PosFixed;
		}
		else if (ParseProp(s, "float"))
		{
			GAutoString v(ParseString(s));
			if (!v) ;
			else if (!stricmp(v, "inherit"))		_float = FloatInherit;
			else if (!stricmp(v, "left"))			_float = FloatLeft;
			else if (!stricmp(v, "right"))			_float = FloatRight;
			else if (!stricmp(v, "none"))			_float = FloatNone;
		}
		else
		{
			LgiAssert(!"Unhandled css property");
		}
		*/

		// End of property delimiter
		while (*s && *s != ';') s++;
		if (*s != ';')
			break;
		s++;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////
bool GCss::Len::Parse(char *&s, bool AllowNoUnits)
{
	if (!s) return false;
	
	SkipWhite(s);
	if (ParseWord(s, "inherit")) Type = LenInherit;
	else if (ParseWord(s, "auto")) Type = LenInherit;
	else if (ParseWord(s, "normal")) Type = LenNormal;
	else if (IsNumeric(s))
	{
		Value = atof(s);
		while (IsNumeric(s)) s++;
		SkipWhite(s);
		if (*s == '%') Type = LenPercent;
		else if (ParseWord(s, "px")) Type = LenPx;
		else if (ParseWord(s, "pt")) Type = LenPt;
		else if (ParseWord(s, "em")) Type = LenEm;
		else if (ParseWord(s, "ex")) Type = LenEx;
		else if (AllowNoUnits) Type = LenPx;
		else return false;
	}
	else return false;
		
	return true;
}

bool GCss::ColorDef::Parse(char *&s)
{
	if (!s) return false;

	#define NamedColour(Name, Value) \
		else if (ParseWord(s, #Name)) { Type = ColorRgb; Rgb32 = Value; return true; }
	#define ParseExpect(s, ch) \
		if (*s != ch) return false; \
		else s++;

	SkipWhite(s);
	if (*s == '#')
	{
		s++;
		int v = 0;
		char *e = s;
		while (*e && e < s + 6)
		{
			if (*e >= 'a' && *e <= 'f')
			{
				v <<= 4;
				v |= *e - 'a' + 10;
				e++;
			}
			else if (*e >= 'A' && *e <= 'F')
			{
				v <<= 4;
				v |= *e - 'A' + 10;
				e++;
			}
			else if (*e >= '0' && *e <= '9')
			{
				v <<= 4;
				v |= *e - '0';
				e++;
			}
			else break;
		}

		if (e == s + 3)
		{
			Type = ColorRgb;
			int r = (v >> 8) & 0xf;
			int g = (v >> 4) & 0xf;
			int b = v & 0xf;
			Rgb32 = Rgb32( (r << 4 | r), (g << 4 | g), (b << 4 | b) );
			s = e;
		}
		else if (e == s + 6)
		{
			Type = ColorRgb;
			int r = (v >> 16) & 0xff;
			int g = (v >> 8) & 0xff;
			int b = v & 0xff;
			Rgb32 = Rgb32(r, g, b);
			s = e;
		}
		else return false;
	}
	else if (ParseWord(s, "rgb("))
	{
		int r = ParseComponent(s);
		ParseExpect(s, ',');
		int g = ParseComponent(s);
		ParseExpect(s, ',');
		int b = ParseComponent(s);
		ParseExpect(s, ')');
		
		Type = ColorRgb;
		Rgb32 = Rgb32(r, g, b);
	}
	else if (ParseWord(s, "rgba("))
	{
		int r = ParseComponent(s);
		ParseExpect(s, ',');
		int g = ParseComponent(s);
		ParseExpect(s, ',');
		int b = ParseComponent(s);
		ParseExpect(s, ',');
		int a = ParseComponent(s);
		ParseExpect(s, ')');
		
		Type = ColorRgb;
		Rgb32 = Rgba32(r, g, b, a);
	}
	else if (ParseWord(s, "-webkit-gradient("))
	{
		GAutoString GradientType(ParseString(s));
		ParseExpect(s, ',');
		if (!GradientType)
			return false;
		if (!stricmp(GradientType, "radial"))
		{
		}
		else if (!stricmp(GradientType, "linear"))
		{
			Len StartX, StartY, EndX, EndY;
			if (!StartX.Parse(s) || !StartY.Parse(s))
				return false;
			ParseExpect(s, ',');
			if (!EndX.Parse(s) || !EndY.Parse(s))
				return false;
			ParseExpect(s, ',');
			SkipWhite(s);
			while (*s)
			{
				if (*s == ')')
				{
					Type = ColorLinearGradient;
					break;
				}
				else
				{
					GAutoString Stop(ParseString(s));
					if (!Stop) return false;
					if (!stricmp(Stop, "from"))
					{
					}
					else if (!stricmp(Stop, "to"))
					{
					}
					else if (!stricmp(Stop, "stop"))
					{
					}
					else return false;
				}
			}
		}
		else return false;
	}
	NamedColour(black, Rgb32(0x00, 0x00, 0x00))
	NamedColour(white, Rgb32(0xff, 0xff, 0xff))
	NamedColour(gray, Rgb32(0x80, 0x80, 0x80))
	NamedColour(red, Rgb32(0xff, 0x00, 0x00))
	NamedColour(yellow, Rgb32(0xff, 0xff, 0x00))
	NamedColour(green, Rgb32(0x00, 0x80, 0x00))
	NamedColour(orange, Rgb32(0xff, 0xA5, 0x00))
	NamedColour(blue, Rgb32(0x00, 0x00, 0xff))
	NamedColour(maroon, Rgb32(0x80, 0x00, 0x00))
	NamedColour(olive, Rgb32(0x80, 0x80, 0x00))
	NamedColour(purple, Rgb32(0x80, 0x00, 0x80))
	NamedColour(fuchsia, Rgb32(0xff, 0x00, 0xff))
	NamedColour(lime, Rgb32(0x00, 0xff, 0x00))
	NamedColour(navy, Rgb32(0x00, 0x00, 0x80))
	NamedColour(aqua, Rgb32(0x00, 0xff, 0xff))
	NamedColour(teal, Rgb32(0x00, 0x80, 0x80))
	NamedColour(silver, Rgb32(0xc0, 0xc0, 0xc0))
	else return false;

	return true;
}

bool GCss::ImageDef::Parse(char *&s)
{
	char Path[MAX_PATH];
	LgiGetSystemPath(LSP_APP_INSTALL, Path, sizeof(Path));

	SkipWhite(s);
	if (!strnicmp(s, "url(", 4))
	{
		s += 4;
		char *e = strchr(s, ')');
		if (!e)
			return false;
		GAutoString v(NewStr(s, e - s));
		LgiMakePath(Path, sizeof(Path), Path, v);
	}
	else LgiMakePath(Path, sizeof(Path), Path, s);

	if (Img = LoadDC(Path))
	{
		Type = ImageOwn;
	}

	return Img != 0;
}

bool GCss::BorderDef::Parse(char *&s)
{
	if (!s)
		return false;

	if (!Len::Parse(s, true))		
		return false;

	SkipWhite(s);
	if (!*s || *s == ';')
		return true;

	GAutoString Pat(ParseString(s));
	if (!Pat)
		return false;

	if (!stricmp(Pat, "Hidden")) Style = BorderHidden;
	else if (!stricmp(Pat, "Solid")) Style = BorderSolid;
	else if (!stricmp(Pat, "Dotted")) Style = BorderDotted;
	else if (!stricmp(Pat, "Dashed")) Style = BorderDashed;
	else if (!stricmp(Pat, "Double")) Style = BorderDouble;
	else if (!stricmp(Pat, "Groove")) Style = BorderGroove;
	else if (!stricmp(Pat, "Ridge")) Style = BorderRidge;
	else if (!stricmp(Pat, "Inset")) Style = BorderInset;
	else if (!stricmp(Pat, "Outset")) Style = BorderOutset;
	else Style = BorderSolid;

	if (!Color.Parse(s))
		return false;

	return true;
}
