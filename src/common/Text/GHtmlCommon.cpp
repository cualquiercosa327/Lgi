#include "Lgi.h"
#include "GHtmlCommon.h"
#include "GDocView.h"

static GHtmlElemInfo TagInfo[] =
{
	{TAG_B,				"b",			0,			GHtmlElemInfo::TI_NONE},
	{TAG_I,				"i",			0,			GHtmlElemInfo::TI_NONE},
	{TAG_U,				"u",			0,			GHtmlElemInfo::TI_NONE},
	{TAG_P,				"p",			0,			GHtmlElemInfo::TI_BLOCK},
	{TAG_BR,			"br",			0,			GHtmlElemInfo::TI_NEVER_CLOSES},
	{TAG_HR,			"hr",			0,			GHtmlElemInfo::TI_BLOCK | GHtmlElemInfo::TI_NEVER_CLOSES},
	{TAG_OL,			"ol",			0,			GHtmlElemInfo::TI_BLOCK},
	{TAG_UL,			"ul",			0,			GHtmlElemInfo::TI_BLOCK},
	{TAG_LI,			"li",			"ul",		GHtmlElemInfo::TI_BLOCK},
	{TAG_FONT,			"font",			0,			GHtmlElemInfo::TI_NONE},
	{TAG_A,				"a",			0,			GHtmlElemInfo::TI_NONE},
	{TAG_TABLE,			"table",		0,			GHtmlElemInfo::TI_BLOCK | GHtmlElemInfo::TI_NO_TEXT | GHtmlElemInfo::TI_TABLE},
	{TAG_TR,			"tr",			"table",	GHtmlElemInfo::TI_BLOCK | GHtmlElemInfo::TI_NO_TEXT | GHtmlElemInfo::TI_TABLE},
	{TAG_TD,			"td",			"tr",		GHtmlElemInfo::TI_BLOCK | GHtmlElemInfo::TI_TABLE},
	{TAG_HEAD,			"head",			"html",		GHtmlElemInfo::TI_NONE | GHtmlElemInfo::TI_SINGLETON},
	{TAG_BODY,			"body",			0,			GHtmlElemInfo::TI_BLOCK | GHtmlElemInfo::TI_NO_TEXT | GHtmlElemInfo::TI_SINGLETON},
	{TAG_IMG,			"img",			0,			GHtmlElemInfo::TI_NEVER_CLOSES},
	{TAG_HTML,			"html",			0,			GHtmlElemInfo::TI_BLOCK | GHtmlElemInfo::TI_NO_TEXT},
	{TAG_DIV,			"div",			0,			GHtmlElemInfo::TI_BLOCK},
	{TAG_SPAN,			"span",			0,			GHtmlElemInfo::TI_NONE},
	{TAG_CENTER,		"center",		0,			GHtmlElemInfo::TI_NONE},
	{TAG_META,			"meta",			0,			GHtmlElemInfo::TI_NEVER_CLOSES},
	{TAG_TBODY,			"tbody",		0,			GHtmlElemInfo::TI_NONE},
	{TAG_STYLE,			"style",		0,			GHtmlElemInfo::TI_NONE},
	{TAG_SCRIPT,		"script",		0,			GHtmlElemInfo::TI_NONE},
	{TAG_STRONG,		"strong",		0,			GHtmlElemInfo::TI_NONE},
	{TAG_BLOCKQUOTE,	"blockquote",	0,			GHtmlElemInfo::TI_BLOCK},
	{TAG_PRE,			"pre",			0,			GHtmlElemInfo::TI_BLOCK},
	{TAG_H1,			"h1",			0,			GHtmlElemInfo::TI_BLOCK},
	{TAG_H2,			"h2",			0,			GHtmlElemInfo::TI_BLOCK},
	{TAG_H3,			"h3",			0,			GHtmlElemInfo::TI_BLOCK},
	{TAG_H4,			"h4",			0,			GHtmlElemInfo::TI_BLOCK},
	{TAG_H5,			"h5",			0,			GHtmlElemInfo::TI_BLOCK},
	{TAG_H6,			"h6",			0,			GHtmlElemInfo::TI_BLOCK},
	{TAG_IFRAME,		"iframe",		0,			GHtmlElemInfo::TI_BLOCK},
	{TAG_LINK,			"link",			0,			GHtmlElemInfo::TI_NONE},
	{TAG_BIG,			"big",			0,			GHtmlElemInfo::TI_NONE},
	{TAG_SELECT,		"select",		0,			GHtmlElemInfo::TI_NONE},
	{TAG_INPUT,			"input",		0,			GHtmlElemInfo::TI_NEVER_CLOSES},
	{TAG_LABEL,			"label",		0,			GHtmlElemInfo::TI_NONE},
	{TAG_FORM,			"form",			0,			GHtmlElemInfo::TI_NONE},
	{TAG_LAST,			NULL,			0,			GHtmlElemInfo::TI_NONE}
};

char16 GHtmlListItem[] = { 0x2022, ' ', 0 };

char16 *WcharToChar16(const wchar_t *i)
{
	static char16 Buf[256];
	char16 *o = Buf;
	while (*i)
	{
		*o++ = *i++;
	}
	*o++ = 0;
	return Buf;
}

GHtmlStatic *GHtmlStatic::Inst = 0;

GHtmlStatic::GHtmlStatic() :
	VarMap(8, true),
	StyleMap(8, false),
	ColourMap(8, false, 0, -1),
	TagMap(CountOf(TagInfo) * 3, false, NULL, NULL)
{
	Refs = 0;
	UnknownElement = NULL;

	// Character entities
	#define DefVar(s, v)				VarMap.Add(WcharToChar16(s), v)
	DefVar(L"quot", 0x22);				// quotation mark = APL quote, U+0022 ISOnum
	DefVar(L"amp", 0x26);				// ampersand, U+0026 ISOnum
	DefVar(L"apos", 0x27);				// apostrophe
	DefVar(L"lt", 0x3C);				// less-than sign, U+003C ISOnum
	DefVar(L"gt", 0x3E);				// greater-than sign, U+003E ISOnum

	#if ShowNbsp
	DefVar(L"nbsp", 0x25cb);			// no-break space
	#else
	DefVar(L"nbsp", 0xa0);				// no-break space = non-breaking space, U+00A0 ISOnum
	#endif

	DefVar(L"iexcl", 0xA1);				// inverted exclamation mark, U+00A1 ISOnum
	DefVar(L"cent", 0xA2);				// cent sign, U+00A2 ISOnum
	DefVar(L"pound", 0xA3);				// pound sign, U+00A3 ISOnum
	DefVar(L"curren", 0xA4);			// currency sign, U+00A4 ISOnum
	DefVar(L"yen", 0xA5);				// yen sign = yuan sign, U+00A5 ISOnum
	DefVar(L"brvbar", 0xA6);			// broken bar = broken vertical bar, U+00A6 ISOnum
	DefVar(L"sect", 0xA7);				// section sign, U+00A7 ISOnum
	DefVar(L"uml", 0xA8);				// diaeresis = spacing diaeresis, U+00A8 ISOdia
	DefVar(L"copy", 0xA9);				// copyright sign, U+00A9 ISOnum
	DefVar(L"ordf", 0xAA);				// feminine ordinal indicator, U+00AA ISOnum
	DefVar(L"laquo", 0xAB);				// left-pointing double angle quotation mark = left pointing guillemet, U+00AB ISOnum
	DefVar(L"not", 0xAC);				// not sign, U+00AC ISOnum
	DefVar(L"shy", 0xAD);				// soft hyphen = discretionary hyphen, U+00AD ISOnum
	DefVar(L"reg", 0xAE);				// registered sign = registered trade mark sign, U+00AE ISOnum
	DefVar(L"macr", 0xAF);				// macron = spacing macron = overline = APL overbar, U+00AF ISOdia
	DefVar(L"deg", 0xB0);				// degree sign, U+00B0 ISOnum
	DefVar(L"plusmn", 0xB1);			// plus-minus sign = plus-or-minus sign, U+00B1 ISOnum
	DefVar(L"sup2", 0xB2);				// superscript two = superscript digit two = squared, U+00B2 ISOnum
	DefVar(L"sup3", 0xB3);				// superscript three = superscript digit three = cubed, U+00B3 ISOnum
	DefVar(L"acute", 0xB4);				// acute accent = spacing acute, U+00B4 ISOdia
	DefVar(L"micro", 0xB5);				// micro sign, U+00B5 ISOnum
	DefVar(L"para", 0xB6);				// pilcrow sign = paragraph sign, U+00B6 ISOnum
	DefVar(L"middot", 0xB7);			// middle dot = Georgian comma = Greek middle dot, U+00B7 ISOnum
	DefVar(L"cedil", 0xB8);				// cedilla = spacing cedilla, U+00B8 ISOdia
	DefVar(L"sup1", 0xB9);				// superscript one = superscript digit one, U+00B9 ISOnum
	DefVar(L"ordm", 0xBA);				// masculine ordinal indicator, U+00BA ISOnum
	DefVar(L"raquo", 0xBB);				// right-pointing double angle quotation mark = right pointing guillemet, U+00BB ISOnum
	DefVar(L"frac14", 0xBC);			// vulgar fraction one quarter fraction one quarter, U+00BC ISOnum
	DefVar(L"frac12", 0xBD);			// vulgar fraction one half fraction one half, U+00BD ISOnum
	DefVar(L"frac34", 0xBE);			// vulgar fraction three quarters fraction three quarters, U+00BE ISOnum
	DefVar(L"iquest", 0xBF);			// inverted question mark turned question mark, U+00BF ISOnum
	DefVar(L"Agrave", 0xC0);			// latin capital letter A with grave latin capital letter A grave, U+00C0 ISOlat1
	DefVar(L"Aacute", 0xC1);			// latin capital letter A with acute, U+00C1 ISOlat1
	DefVar(L"Acirc", 0xC2);				// latin capital letter A with circumflex, U+00C2 ISOlat1
	DefVar(L"Atilde", 0xC3);			// latin capital letter A with tilde, U+00C3 ISOlat1
	DefVar(L"Auml", 0xC4);				// latin capital letter A with diaeresis, U+00C4 ISOlat1
	DefVar(L"Aring", 0xC5);				// latin capital letter A with ring above latin capital letter A ring, U+00C5 ISOlat1
	DefVar(L"AElig", 0xC6);				// latin capital letter AE latin capital ligature AE, U+00C6 ISOlat1
	DefVar(L"Ccedil", 0xC7);			// latin capital letter C with cedilla, U+00C7 ISOlat1
	DefVar(L"Egrave", 0xC8);			// latin capital letter E with grave, U+00C8 ISOlat1
	DefVar(L"Eacute", 0xC9);			// latin capital letter E with acute, U+00C9 ISOlat1
	DefVar(L"Ecirc", 0xCA);				// latin capital letter E with circumflex, U+00CA ISOlat1
	DefVar(L"Euml", 0xCB);				// latin capital letter E with diaeresis, U+00CB ISOlat1
	DefVar(L"Igrave", 0xCC);			// latin capital letter I with grave, U+00CC ISOlat1
	DefVar(L"Iacute", 0xCD);			// latin capital letter I with acute, U+00CD ISOlat1
	DefVar(L"Icirc", 0xCE);				// latin capital letter I with circumflex, U+00CE ISOlat1
	DefVar(L"Iuml", 0xCF);				// latin capital letter I with diaeresis, U+00CF ISOlat1
	DefVar(L"ETH", 0xD0);				// latin capital letter ETH, U+00D0 ISOlat1
	DefVar(L"Ntilde", 0xD1);			// latin capital letter N with tilde, U+00D1 ISOlat1
	DefVar(L"Ograve", 0xD2);			// latin capital letter O with grave, U+00D2 ISOlat1
	DefVar(L"Oacute", 0xD3);			// latin capital letter O with acute, U+00D3 ISOlat1
	DefVar(L"Ocirc", 0xD4);				// latin capital letter O with circumflex, U+00D4 ISOlat1
	DefVar(L"Otilde", 0xD5);			// latin capital letter O with tilde, U+00D5 ISOlat1
	DefVar(L"Ouml", 0xD6);				// latin capital letter O with diaeresis, U+00D6 ISOlat1
	DefVar(L"times", 0xD7);				// multiplication sign, U+00D7 ISOnum
	DefVar(L"Oslash", 0xD8);			// latin capital letter O with stroke = latin capital letter O slash, U+00D8 ISOlat1
	DefVar(L"Ugrave", 0xD9);			// latin capital letter U with grave, U+00D9 ISOlat1
	DefVar(L"Uacute", 0xDA);			// latin capital letter U with acute, U+00DA ISOlat1
	DefVar(L"Ucirc", 0xDB);				// latin capital letter U with circumflex, U+00DB ISOlat1
	DefVar(L"Uuml", 0xDC);				// latin capital letter U with diaeresis, U+00DC ISOlat1
	DefVar(L"Yacute", 0xDD);			// latin capital letter Y with acute, U+00DD ISOlat1
	DefVar(L"THORN", 0xDE);				// latin capital letter THORN, U+00DE ISOlat1
	DefVar(L"szlig", 0xDF);				// latin small letter sharp s = ess-zed, U+00DF ISOlat1
	DefVar(L"agrave", 0xE0);			// latin small letter a with grave latin small letter a grave, U+00E0 ISOlat1
	DefVar(L"aacute", 0xE1);			// latin small letter a with acute, U+00E1 ISOlat1
	DefVar(L"acirc", 0xE2);				// latin small letter a with circumflex, U+00E2 ISOlat1
	DefVar(L"atilde", 0xE3);			// latin small letter a with tilde, U+00E3 ISOlat1
	DefVar(L"auml", 0xE4);				// latin small letter a with diaeresis, U+00E4 ISOlat1
	DefVar(L"aring", 0xE5);				// latin small letter a with ring above latin small letter a ring, U+00E5 ISOlat1
	DefVar(L"aelig", 0xE6);				// latin small letter ae latin small ligature ae, U+00E6 ISOlat1
	DefVar(L"ccedil", 0xE7);			// latin small letter c with cedilla, U+00E7 ISOlat1
	DefVar(L"egrave", 0xE8);			// latin small letter e with grave, U+00E8 ISOlat1
	DefVar(L"eacute", 0xE9);			// latin small letter e with acute, U+00E9 ISOlat1
	DefVar(L"ecirc", 0xEA);				// latin small letter e with circumflex, U+00EA ISOlat1
	DefVar(L"euml", 0xEB);				// latin small letter e with diaeresis, U+00EB ISOlat1
	DefVar(L"igrave", 0xEC);			// latin small letter i with grave, U+00EC ISOlat1
	DefVar(L"iacute", 0xED);			// latin small letter i with acute, U+00ED ISOlat1
	DefVar(L"icirc", 0xEE);				// latin small letter i with circumflex, U+00EE ISOlat1
	DefVar(L"iuml", 0xEF);				// latin small letter i with diaeresis, U+00EF ISOlat1
	DefVar(L"eth", 0xF0);				// latin small letter eth, U+00F0 ISOlat1
	DefVar(L"ntilde", 0xF1);			// latin small letter n with tilde, U+00F1 ISOlat1
	DefVar(L"ograve", 0xF2);			// latin small letter o with grave, U+00F2 ISOlat1
	DefVar(L"oacute", 0xF3);			// latin small letter o with acute, U+00F3 ISOlat1
	DefVar(L"ocirc", 0xF4);				// latin small letter o with circumflex, U+00F4 ISOlat1
	DefVar(L"otilde", 0xF5);			// latin small letter o with tilde, U+00F5 ISOlat1
	DefVar(L"ouml", 0xF6);				// latin small letter o with diaeresis, U+00F6 ISOlat1
	DefVar(L"divide", 0xF7);			// division sign, U+00F7 ISOnum
	DefVar(L"oslash", 0xF8);			// latin small letter o with stroke, latin small letter o slash, U+00F8 ISOlat1
	DefVar(L"ugrave", 0xF9);			// latin small letter u with grave, U+00F9 ISOlat1
	DefVar(L"uacute", 0xFA);			// latin small letter u with acute, U+00FA ISOlat1
	DefVar(L"ucirc", 0xFB);				// latin small letter u with circumflex, U+00FB ISOlat1
	DefVar(L"uuml", 0xFC);				// latin small letter u with diaeresis, U+00FC ISOlat1
	DefVar(L"yacute", 0xFD);			// latin small letter y with acute, U+00FD ISOlat1
	DefVar(L"thorn", 0xFE);				// latin small letter thorn with, U+00FE ISOlat1
	DefVar(L"yuml", 0xFF);				// latin small letter y with diaeresis, U+00FF ISOlat1

	DefVar(L"OElig", 0x0152);			// latin capital ligature OE, U+0152 ISOlat2
	DefVar(L"oelig", 0x0153);			// latin small ligature oe, U+0153 ISOlat2
	DefVar(L"Scaron", 0x0160);			// latin capital letter S with caron, U+0160 ISOlat2
	DefVar(L"scaron", 0x0161);			// latin small letter s with caron, U+0161 ISOlat2
	DefVar(L"Yuml", 0x0178);			// latin capital letter Y with diaeresis, U+0178 ISOlat2
	DefVar(L"fnof", 0x0192);			// Latin small letter f with hook (= function = florin)
	DefVar(L"circ", 0x02C6);			// modifier letter circumflex accent, U+02C6 ISOpub
	DefVar(L"tilde", 0x02DC);			// small tilde, U+02DC ISOdia

	DefVar(L"Alpha", 0x0391);			// Greek capital letter Alpha
	DefVar(L"Beta", 0x0392);			// Greek capital letter Beta
	DefVar(L"Gamma", 0x0393);			// Greek capital letter Gamma
	DefVar(L"Delta", 0x0394);			// Greek capital letter Delta
	DefVar(L"Epsilon", 0x0395);			// capital letter Epsilon
	DefVar(L"Zeta", 0x0396);			// capital letter Zeta
	DefVar(L"Eta", 0x0397);				// capital letter Eta
	DefVar(L"Theta", 0x0398);			// Greek capital letter Theta
	DefVar(L"Iota", 0x0399);			// capital letter Iota
	DefVar(L"Kappa", 0x039A);			// capital letter Kappa
	DefVar(L"Lambda", 0x039B);			// Greek capital letter Lambda
	DefVar(L"Mu", 0x039C);				// capital letter Mu
	DefVar(L"Nu", 0x039D);				// capital letter Nu
	DefVar(L"Xi", 0x039E);				// Greek capital letter Xi
	DefVar(L"Omicron", 0x039F);			// capital letter Omicron
	DefVar(L"Pi", 0x03A0);				// capital letter Pi
	DefVar(L"Rho", 0x03A1);				// capital letter Rho
	DefVar(L"Sigma", 0x03A3);			// Greek capital letter Sigma
	DefVar(L"Tau", 0x03A4);				// capital letter Tau
	DefVar(L"Upsilon", 0x03A5);			// Greek capital letter Upsilon
	DefVar(L"Phi", 0x03A6);				// Greek capital letter Phi
	DefVar(L"Chi", 0x03A7);				// capital letter Chi
	DefVar(L"Psi", 0x03A8);				// Greek capital letter Psi
	DefVar(L"Omega", 0x03A9);			// Greek capital letter Omega
	DefVar(L"alpha", 0x03B1);			// Greek small letter alpha
	DefVar(L"beta", 0x03B2);			// Greek small letter beta
	DefVar(L"gamma", 0x03B3);			// Greek small letter gamma
	DefVar(L"delta", 0x03B4);			// Greek small letter delta
	DefVar(L"epsilon", 0x03B5);			// Greek small letter epsilon
	DefVar(L"zeta", 0x03B6);			// Greek small letter zeta
	DefVar(L"eta", 0x03B7);				// Greek small letter eta
	DefVar(L"theta", 0x03B8);			// Greek small letter theta
	DefVar(L"iota", 0x03B9);			// Greek small letter iota
	DefVar(L"kappa", 0x03BA);			// Greek small letter kappa
	DefVar(L"lambda", 0x03BB);			// Greek small letter lambda
	DefVar(L"mu", 0x03BC);				// Greek small letter mu
	DefVar(L"nu", 0x03BD);				// Greek small letter nu
	DefVar(L"xi", 0x03BE);				// Greek small letter xi
	DefVar(L"omicron", 0x03BF);			// Greek small letter omicron
	DefVar(L"pi", 0x03C0);				// Greek small letter pi
	DefVar(L"rho", 0x03C1);				// Greek small letter rho
	DefVar(L"sigmaf", 0x03C2);			// Greek small letter final sigma
	DefVar(L"sigma", 0x03C3);			// Greek small letter sigma
	DefVar(L"tau", 0x03C4);				// Greek small letter tau
	DefVar(L"upsilon", 0x03C5);			// Greek small letter upsilon
	DefVar(L"phi", 0x03C6);				// Greek small letter phi
	DefVar(L"chi", 0x03C7);				// Greek small letter chi
	DefVar(L"psi", 0x03C8);				// Greek small letter psi
	DefVar(L"omega", 0x03C9);			// Greek small letter omega
	DefVar(L"thetasym", 0x03D1);		// Greek theta symbol
	DefVar(L"upsih", 0x03D2);			// Greek Upsilon with hook symbol
	DefVar(L"piv", 0x03D6);				// Greek pi symbol

	DefVar(L"ensp", 0x2002);			// en space, U+2002 ISOpub
	DefVar(L"emsp", 0x2003);			// em space, U+2003 ISOpub
	DefVar(L"thinsp", 0x2009);			// thin space, U+2009 ISOpub
	DefVar(L"zwnj", 0x200C);			// zero width non-joiner, U+200C NEW RFC 2070
	DefVar(L"zwj", 0x200D);				// zero width joiner, U+200D NEW RFC 2070
	DefVar(L"lrm", 0x200E);				// left-to-right mark, U+200E NEW RFC 2070
	DefVar(L"rlm", 0x200F);				// right-to-left mark, U+200F NEW RFC 2070
	DefVar(L"ndash", 0x2013);			// en dash, U+2013 ISOpub
	DefVar(L"mdash", 0x2014);			// em dash, U+2014 ISOpub
	DefVar(L"lsquo", 0x2018);			// left single quotation mark, U+2018 ISOnum
	DefVar(L"rsquo", 0x2019);			// right single quotation mark, U+2019 ISOnum
	DefVar(L"sbquo", 0x201A);			// single low-9 quotation mark, U+201A NEW
	DefVar(L"ldquo", 0x201C);			// left double quotation mark, U+201C ISOnum
	DefVar(L"rdquo", 0x201D);			// right double quotation mark, U+201D ISOnum
	DefVar(L"bdquo", 0x201E);			// double low-9 quotation mark, U+201E NEW
	DefVar(L"dagger", 0x2020);			// dagger, U+2020 ISOpub
	DefVar(L"Dagger", 0x2021);			// double dagger, U+2021 ISOpub
	DefVar(L"bull", 0x2022);			// bullet (= black small circle)
	DefVar(L"hellip", 0x2026);			// horizontal ellipsis (= three dot leader)
	DefVar(L"permil", 0x2030);			// per mille sign, U+2030 ISOtech
	DefVar(L"prime", 0x2032);			// prime (= minutes = feet)
	DefVar(L"Prime", 0x2033);			// double prime (= seconds = inches)
	DefVar(L"lsaquo", 0x2039);			// single left-pointing angle quotation mark, U+2039 ISO proposed
	DefVar(L"rsaquo", 0x203A);			// single right-pointing angle quotation mark, U+203A ISO proposed
	DefVar(L"oline", 0x203E);			// overline (= spacing overscore)
	DefVar(L"frasl", 0x2044);			// fraction slash (= solidus)
	DefVar(L"euro", 0x20AC);			// euro sign, U+20AC NEW

	DefVar(L"image", 0x2111);			// black-letter capital I (= imaginary part)
	DefVar(L"weierp", 0x2118);			// script capital P (= power set = Weierstrass p)
	DefVar(L"real", 0x211C);			// black-letter capital R (= real part symbol)
	DefVar(L"trade", 0x2122);			// trademark symbol
	DefVar(L"alefsym", 0x2135);			// alef symbol (= first transfinite cardinal)[h]
	DefVar(L"larr", 0x2190);			// leftwards arrow
	DefVar(L"uarr", 0x2191);			// upwards arrow
	DefVar(L"rarr", 0x2192);			// rightwards arrow
	DefVar(L"darr", 0x2193);			// downwards arrow
	DefVar(L"harr", 0x2194);			// left right arrow
	DefVar(L"crarr", 0x21B5);			// downwards arrow with corner leftwards (= carriage return)
	DefVar(L"lArr", 0x21D0);			// leftwards double arrow[i]
	DefVar(L"uArr", 0x21D1);			// upwards double arrow
	DefVar(L"rArr", 0x21D2);			// rightwards double arrow[j]
	DefVar(L"dArr", 0x21D3);			// downwards double arrow
	DefVar(L"hArr", 0x21D4);			// left right double arrow
	DefVar(L"forall", 0x2200);			// for all
	DefVar(L"part", 0x2202);			// partial differential
	DefVar(L"exist", 0x2203);			// there exists
	DefVar(L"empty", 0x2205);			// empty set (= null set = diameter)
	DefVar(L"nabla", 0x2207);			// nabla (= backward difference)
	DefVar(L"isin", 0x2208);			// element of
	DefVar(L"notin", 0x2209);			// not an element of
	DefVar(L"ni", 0x220B);				// contains as member
	DefVar(L"prod", 0x220F);			// n-ary product (= product sign)[k]
	DefVar(L"sum", 0x2211);				// n-ary summation[l]
	DefVar(L"minus", 0x2212);			// minus sign
	DefVar(L"lowast", 0x2217);			// asterisk operator
	DefVar(L"radic", 0x221A);			// square root (= radical sign)
	DefVar(L"prop", 0x221D);			// proportional to
	DefVar(L"infin", 0x221E);			// ISOtech	infinity
	DefVar(L"ang", 0x2220);				// ISOamso	angle
	DefVar(L"and", 0x2227);				// logical and (= wedge)
	DefVar(L"or", 0x2228);				// logical or (= vee)
	DefVar(L"cap", 0x2229);				// intersection (= cap)
	DefVar(L"cup", 0x222A);				// union (= cup)
	DefVar(L"int", 0x222B);				// ISOtech	integral
	DefVar(L"there4", 0x2234);			// therefore sign
	DefVar(L"sim", 0x223C);				// tilde operator (= varies with = similar to)[m]
	DefVar(L"cong", 0x2245);			// congruent to
	DefVar(L"asymp", 0x2248);			// almost equal to (= asymptotic to)
	DefVar(L"ne", 0x2260);				// not equal to
	DefVar(L"equiv", 0x2261);			// identical to; sometimes used for 'equivalent to'
	DefVar(L"le", 0x2264);				// less-than or equal to
	DefVar(L"ge", 0x2265);				// greater-than or equal to
	DefVar(L"sub", 0x2282);				// subset of
	DefVar(L"sup", 0x2283);				// superset of[n]
	DefVar(L"nsub", 0x2284);			// not a subset of
	DefVar(L"sube", 0x2286);			// subset of or equal to
	DefVar(L"supe", 0x2287);			// superset of or equal to
	DefVar(L"oplus", 0x2295);			// circled plus (= direct sum)
	DefVar(L"otimes", 0x2297);			// circled times (= vector product)
	DefVar(L"perp", 0x22A5);			// up tack (= orthogonal to = perpendicular)[o]
	DefVar(L"sdot", 0x22C5);			// dot operator[p]
	DefVar(L"lceil", 0x2308);			// left ceiling (= APL upstile)
	DefVar(L"rceil", 0x2309);			// right ceiling
	DefVar(L"lfloor", 0x230A);			// left floor (= APL downstile)
	DefVar(L"rfloor", 0x230B);			// right floor
	DefVar(L"lang", 0x2329);			// left-pointing angle bracket (= bra)[q]
	DefVar(L"rang", 0x232A);			// right-pointing angle bracket (= ket)[r]
	DefVar(L"loz", 0x25CA);				// ISOpub	lozenge
	DefVar(L"spades", 0x2660);			// black spade suit[f]
	DefVar(L"clubs", 0x2663);			// black club suit (= shamrock)[f]
	DefVar(L"hearts", 0x2665);			// black heart suit (= valentine)[f]
	DefVar(L"diams", 0x2666);			// black diamond suit[f]

	// Supported styles
	#define DefStyle(s, v) StyleMap.Add((char*)#s, v)
	DefStyle(color, GCss::PropColor);
	DefStyle(background, GCss::PropBackground);
	DefStyle(background-color, GCss::PropBackgroundColor);
	DefStyle(background-repeat, GCss::PropBackgroundRepeat);
	DefStyle(font, GCss::PropFont);
	DefStyle(text-align, GCss::PropTextAlign);
	DefStyle(vertical-align, GCss::PropVerticalAlign);
	DefStyle(font-size, GCss::PropFontSize);
	DefStyle(font-weight, GCss::PropFontWeight);
	DefStyle(font-family, GCss::PropFontFamily);
	DefStyle(font-style, GCss::PropFontStyle);
	DefStyle(width, GCss::PropWidth);
	DefStyle(height, GCss::PropHeight);
	DefStyle(margin, GCss::PropMargin);
	DefStyle(margin-left, GCss::PropMarginLeft);
	DefStyle(margin-right, GCss::PropMarginRight);
	DefStyle(margin-top, GCss::PropMarginTop);
	DefStyle(margin-bottom, GCss::PropMarginBottom);
	DefStyle(padding, GCss::PropPadding);
	DefStyle(padding-left, GCss::PropPaddingLeft);
	DefStyle(padding-top, GCss::PropPaddingTop);
	DefStyle(padding-right, GCss::PropPaddingRight);
	DefStyle(padding-bottom, GCss::PropPaddingBottom);
	DefStyle(border, GCss::PropBorder);
	DefStyle(border-left, GCss::PropBorderLeft);
	DefStyle(border-right, GCss::PropBorderRight);
	DefStyle(border-top, GCss::PropBorderTop);
	DefStyle(border-bottom, GCss::PropBorderBottom);
	
	// Html colours
	#define DefColour(s, v) ColourMap.Add((char*)#s, v)

	DefColour(LC_BLACK, LC_BLACK);
	DefColour(LC_DKGREY, LC_DKGREY);
	DefColour(LC_MIDGREY, LC_MIDGREY);
	DefColour(LC_LTGREY, LC_LTGREY);
	DefColour(LC_WHITE, LC_WHITE);
	DefColour(ThreeDDarkShadow, LC_SHADOW);
	DefColour(ThreeDShadow, LC_LOW);
	DefColour(ThreeDFace, LC_MED);
	DefColour(ThreeDLightShadow, LC_HIGH);
	DefColour(ThreeDHighlight, LC_LIGHT);
	DefColour(ButtonFace, LC_DIALOG);
	DefColour(AppWorkspace, LC_WORKSPACE);
	DefColour(WindowText, LC_TEXT);
	DefColour(Highlight, LC_FOCUS_SEL_BACK);
	DefColour(HighlightText, LC_FOCUS_SEL_FORE);
	DefColour(ActiveBorder, LC_ACTIVE_TITLE);
	DefColour(ActiveCaption, LC_ACTIVE_TITLE_TEXT);
	DefColour(InactiveBorder, LC_INACTIVE_TITLE);
	DefColour(InactiveCaptionText, LC_INACTIVE_TITLE_TEXT);
	DefColour(NonFocusHighlight, LC_NON_FOCUS_SEL_BACK);
	DefColour(NonFocusHighlightText, LC_NON_FOCUS_SEL_FORE);

	DefColour(AliceBlue, Rgb24(0xF0, 0xF8, 0xFF));
	DefColour(AntiqueWhite, Rgb24(0xFA, 0xEB, 0xD7));
	DefColour(Aqua, Rgb24(0x00, 0xFF, 0xFF));
	DefColour(Aquamarine, Rgb24(0x7F, 0xFF, 0xD4));
	DefColour(Azure, Rgb24(0xF0, 0xFF, 0xFF));
	DefColour(Beige, Rgb24(0xF5, 0xF5, 0xDC));
	DefColour(Bisque, Rgb24(0xFF, 0xE4, 0xC4));
	DefColour(Black, Rgb24(0x00, 0x00, 0x00));
	DefColour(BlanchedAlmond, Rgb24(0xFF, 0xEB, 0xCD));
	DefColour(Blue, Rgb24(0x00, 0x00, 0xFF));
	DefColour(BlueViolet, Rgb24(0x8A, 0x2B, 0xE2));
	DefColour(Brown, Rgb24(0xA5, 0x2A, 0x2A));
	DefColour(BurlyWood, Rgb24(0xDE, 0xB8, 0x87));
	DefColour(CadetBlue, Rgb24(0x5F, 0x9E, 0xA0));
	DefColour(Chartreuse, Rgb24(0x7F, 0xFF, 0x00));
	DefColour(Chocolate, Rgb24(0xD2, 0x69, 0x1E));
	DefColour(Coral, Rgb24(0xFF, 0x7F, 0x50));
	DefColour(CornflowerBlue, Rgb24(0x64, 0x95, 0xED));
	DefColour(Cornsilk, Rgb24(0xFF, 0xF8, 0xDC));
	DefColour(Crimson, Rgb24(0xDC, 0x14, 0x3C));
	DefColour(Cyan, Rgb24(0x00, 0xFF, 0xFF));
	DefColour(DarkBlue, Rgb24(0x00, 0x00, 0x8B));
	DefColour(DarkCyan, Rgb24(0x00, 0x8B, 0x8B));
	DefColour(DarkGoldenRod, Rgb24(0xB8, 0x86, 0x0B));
	DefColour(DarkGray, Rgb24(0xA9, 0xA9, 0xA9));
	DefColour(DarkGreen, Rgb24(0x00, 0x64, 0x00));
	DefColour(DarkKhaki, Rgb24(0xBD, 0xB7, 0x6B));
	DefColour(DarkMagenta, Rgb24(0x8B, 0x00, 0x8B));
	DefColour(DarkOliveGreen, Rgb24(0x55, 0x6B, 0x2F));
	DefColour(Darkorange, Rgb24(0xFF, 0x8C, 0x00));
	DefColour(DarkOrchid, Rgb24(0x99, 0x32, 0xCC));
	DefColour(DarkRed, Rgb24(0x8B, 0x00, 0x00));
	DefColour(DarkSalmon, Rgb24(0xE9, 0x96, 0x7A));
	DefColour(DarkSeaGreen, Rgb24(0x8F, 0xBC, 0x8F));
	DefColour(DarkSlateBlue, Rgb24(0x48, 0x3D, 0x8B));
	DefColour(DarkSlateGray, Rgb24(0x2F, 0x4F, 0x4F));
	DefColour(DarkTurquoise, Rgb24(0x00, 0xCE, 0xD1));
	DefColour(DarkViolet, Rgb24(0x94, 0x00, 0xD3));
	DefColour(DeepPink, Rgb24(0xFF, 0x14, 0x93));
	DefColour(DeepSkyBlue, Rgb24(0x00, 0xBF, 0xFF));
	DefColour(DimGray, Rgb24(0x69, 0x69, 0x69));
	DefColour(DodgerBlue, Rgb24(0x1E, 0x90, 0xFF));
	DefColour(FireBrick, Rgb24(0xB2, 0x22, 0x22));
	DefColour(FloralWhite, Rgb24(0xFF, 0xFA, 0xF0));
	DefColour(ForestGreen, Rgb24(0x22, 0x8B, 0x22));
	DefColour(Fuchsia, Rgb24(0xFF, 0x00, 0xFF));
	DefColour(Gainsboro, Rgb24(0xDC, 0xDC, 0xDC));
	DefColour(GhostWhite, Rgb24(0xF8, 0xF8, 0xFF));
	DefColour(Gold, Rgb24(0xFF, 0xD7, 0x00));
	DefColour(GoldenRod, Rgb24(0xDA, 0xA5, 0x20));
	DefColour(Gray, Rgb24(0x80, 0x80, 0x80));
	DefColour(Green, Rgb24(0x00, 0x80, 0x00));
	DefColour(GreenYellow, Rgb24(0xAD, 0xFF, 0x2F));
	DefColour(HoneyDew, Rgb24(0xF0, 0xFF, 0xF0));
	DefColour(HotPink, Rgb24(0xFF, 0x69, 0xB4));
	DefColour(IndianRed , Rgb24(0xCD, 0x5C, 0x5C));
	DefColour(Indigo , Rgb24(0x4B, 0x00, 0x82));
	DefColour(Ivory, Rgb24(0xFF, 0xFF, 0xF0));
	DefColour(Khaki, Rgb24(0xF0, 0xE6, 0x8C));
	DefColour(Lavender, Rgb24(0xE6, 0xE6, 0xFA));
	DefColour(LavenderBlush, Rgb24(0xFF, 0xF0, 0xF5));
	DefColour(LawnGreen, Rgb24(0x7C, 0xFC, 0x00));
	DefColour(LemonChiffon, Rgb24(0xFF, 0xFA, 0xCD));
	DefColour(LightBlue, Rgb24(0xAD, 0xD8, 0xE6));
	DefColour(LightCoral, Rgb24(0xF0, 0x80, 0x80));
	DefColour(LightCyan, Rgb24(0xE0, 0xFF, 0xFF));
	DefColour(LightGoldenRodYellow, Rgb24(0xFA, 0xFA, 0xD2));
	DefColour(LightGrey, Rgb24(0xD3, 0xD3, 0xD3));
	DefColour(LightGreen, Rgb24(0x90, 0xEE, 0x90));
	DefColour(LightPink, Rgb24(0xFF, 0xB6, 0xC1));
	DefColour(LightSalmon, Rgb24(0xFF, 0xA0, 0x7A));
	DefColour(LightSeaGreen, Rgb24(0x20, 0xB2, 0xAA));
	DefColour(LightSkyBlue, Rgb24(0x87, 0xCE, 0xFA));
	DefColour(LightSlateBlue, Rgb24(0x84, 0x70, 0xFF));
	DefColour(LightSlateGray, Rgb24(0x77, 0x88, 0x99));
	DefColour(LightSteelBlue, Rgb24(0xB0, 0xC4, 0xDE));
	DefColour(LightYellow, Rgb24(0xFF, 0xFF, 0xE0));
	DefColour(Lime, Rgb24(0x00, 0xFF, 0x00));
	DefColour(LimeGreen, Rgb24(0x32, 0xCD, 0x32));
	DefColour(Linen, Rgb24(0xFA, 0xF0, 0xE6));
	DefColour(Magenta, Rgb24(0xFF, 0x00, 0xFF));
	DefColour(Maroon, Rgb24(0x80, 0x00, 0x00));
	DefColour(MediumAquaMarine, Rgb24(0x66, 0xCD, 0xAA));
	DefColour(MediumBlue, Rgb24(0x00, 0x00, 0xCD));
	DefColour(MediumOrchid, Rgb24(0xBA, 0x55, 0xD3));
	DefColour(MediumPurple, Rgb24(0x93, 0x70, 0xD8));
	DefColour(MediumSeaGreen, Rgb24(0x3C, 0xB3, 0x71));
	DefColour(MediumSlateBlue, Rgb24(0x7B, 0x68, 0xEE));
	DefColour(MediumSpringGreen, Rgb24(0x00, 0xFA, 0x9A));
	DefColour(MediumTurquoise, Rgb24(0x48, 0xD1, 0xCC));
	DefColour(MediumVioletRed, Rgb24(0xC7, 0x15, 0x85));
	DefColour(MidnightBlue, Rgb24(0x19, 0x19, 0x70));
	DefColour(MintCream, Rgb24(0xF5, 0xFF, 0xFA));
	DefColour(MistyRose, Rgb24(0xFF, 0xE4, 0xE1));
	DefColour(Moccasin, Rgb24(0xFF, 0xE4, 0xB5));
	DefColour(NavajoWhite, Rgb24(0xFF, 0xDE, 0xAD));
	DefColour(Navy, Rgb24(0x00, 0x00, 0x80));
	DefColour(OldLace, Rgb24(0xFD, 0xF5, 0xE6));
	DefColour(Olive, Rgb24(0x80, 0x80, 0x00));
	DefColour(OliveDrab, Rgb24(0x6B, 0x8E, 0x23));
	DefColour(Orange, Rgb24(0xFF, 0xA5, 0x00));
	DefColour(OrangeRed, Rgb24(0xFF, 0x45, 0x00));
	DefColour(Orchid, Rgb24(0xDA, 0x70, 0xD6));
	DefColour(PaleGoldenRod, Rgb24(0xEE, 0xE8, 0xAA));
	DefColour(PaleGreen, Rgb24(0x98, 0xFB, 0x98));
	DefColour(PaleTurquoise, Rgb24(0xAF, 0xEE, 0xEE));
	DefColour(PaleVioletRed, Rgb24(0xD8, 0x70, 0x93));
	DefColour(PapayaWhip, Rgb24(0xFF, 0xEF, 0xD5));
	DefColour(PeachPuff, Rgb24(0xFF, 0xDA, 0xB9));
	DefColour(Peru, Rgb24(0xCD, 0x85, 0x3F));
	DefColour(Pink, Rgb24(0xFF, 0xC0, 0xCB));
	DefColour(Plum, Rgb24(0xDD, 0xA0, 0xDD));
	DefColour(PowderBlue, Rgb24(0xB0, 0xE0, 0xE6));
	DefColour(Purple, Rgb24(0x80, 0x00, 0x80));
	DefColour(Red, Rgb24(0xFF, 0x00, 0x00));
	DefColour(RosyBrown, Rgb24(0xBC, 0x8F, 0x8F));
	DefColour(RoyalBlue, Rgb24(0x41, 0x69, 0xE1));
	DefColour(SaddleBrown, Rgb24(0x8B, 0x45, 0x13));
	DefColour(Salmon, Rgb24(0xFA, 0x80, 0x72));
	DefColour(SandyBrown, Rgb24(0xF4, 0xA4, 0x60));
	DefColour(SeaGreen, Rgb24(0x2E, 0x8B, 0x57));
	DefColour(SeaShell, Rgb24(0xFF, 0xF5, 0xEE));
	DefColour(Sienna, Rgb24(0xA0, 0x52, 0x2D));
	DefColour(Silver, Rgb24(0xC0, 0xC0, 0xC0));
	DefColour(SkyBlue, Rgb24(0x87, 0xCE, 0xEB));
	DefColour(SlateBlue, Rgb24(0x6A, 0x5A, 0xCD));
	DefColour(SlateGray, Rgb24(0x70, 0x80, 0x90));
	DefColour(Snow, Rgb24(0xFF, 0xFA, 0xFA));
	DefColour(SpringGreen, Rgb24(0x00, 0xFF, 0x7F));
	DefColour(SteelBlue, Rgb24(0x46, 0x82, 0xB4));
	DefColour(Tan, Rgb24(0xD2, 0xB4, 0x8C));
	DefColour(Teal, Rgb24(0x00, 0x80, 0x80));
	DefColour(Thistle, Rgb24(0xD8, 0xBF, 0xD8));
	DefColour(Tomato, Rgb24(0xFF, 0x63, 0x47));
	DefColour(Turquoise, Rgb24(0x40, 0xE0, 0xD0));
	DefColour(Violet, Rgb24(0xEE, 0x82, 0xEE));
	DefColour(VioletRed, Rgb24(0xD0, 0x20, 0x90));
	DefColour(Wheat, Rgb24(0xF5, 0xDE, 0xB3));
	DefColour(White, Rgb24(0xFF, 0xFF, 0xFF));
	DefColour(WhiteSmoke, Rgb24(0xF5, 0xF5, 0xF5));
	DefColour(Yellow, Rgb24(0xFF, 0xFF, 0x00));
	DefColour(YellowGreen, Rgb24(0x9A, 0xCD, 0x32));

	// Tag info hash
	for (GHtmlElemInfo *t = TagInfo; t->Tag; t++)
	{
		TagMap.Add(t->Tag, t);
	}
	UnknownElement = TagInfo + CountOf(TagInfo) - 1;
}

GHtmlStatic::~GHtmlStatic()
{
}

GHtmlElemInfo *GHtmlStatic::GetTagInfo(const char *Tag)
{
	GHtmlElemInfo *i = TagMap.Find(Tag);
	return i ? i : UnknownElement;
}

/////////////////////////////////////////////////////////////////////////////
GHtmlElement::GHtmlElement(GHtmlElement *parent)
{
	TagId = CONTENT;
	Parent = parent;
	Info = NULL;
	WasClosed = false;
}

GHtmlElement::~GHtmlElement()
{
	Children.DeleteObjects();
}

bool GHtmlElement::Attach(GHtmlElement *Child, int Idx)
{
	if (TagId == CONTENT)
	{
		LgiAssert(!"Can't nest content tags.");
		return false;
	}

	if (!Child)
	{
		LgiAssert(!"Can't insert NULL tag.");
		return false;
	}

	Child->Detach();
	Child->Parent = this;
	if (!Children.HasItem(Child))
	{
		Children.AddAt(Idx, Child);
	}

	return true;
}

void GHtmlElement::Detach()
{
	if (Parent)
	{
		Parent->Children.Delete(this, true);
		Parent = NULL;
	}
}

bool GHtmlElement::HasChild(GHtmlElement *c)
{
	for (unsigned i=0; i<Children.Length(); i++)
	{
		if (Children[i] == c)
			return true;
			
		if (Children[i]->HasChild(c))
			return true;
	}
	
	return false;
}

