/*
**	FILE:			Res.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			25/9/1999
**	DESCRIPTION:	Dialog Resources
**
**
**	Copyright (C) 1999, Matthew Allen
**		fret@memecode.com
*/

////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "Lgi.h"
#include "Res.h"
#include "GVariant.h"
#include "GToken.h"

////////////////////////////////////////////////////////////////////
#ifdef WIN32
#define DEF_CP		"windows-1252"
#define DL(l) l
#else
#define DEF_CP		"iso-8859-1"
#define DL(l) 0 // language id tokens not relevent
#endif

/// A table of all the languages supported.
///
/// If you change this table you HAVE to rerun _LgiGenLangLookup()
/// and insert the output into GFindLang and GFindLangOldId
///
/// \ingroup Resources
GLanguage LgiLanguageTable[] =
{
	{"Abkhazian",				"ab"},
	{"Afar",					"aa"},
	{"Afrikaans",				"af", DL(LANG_AFRIKAANS), 1, "ISO-8859-1"},
	{"Albanian",				"sq", DL(LANG_ALBANIAN), 2},
	{"Amharic",					"am"},
	{"Arabic",					"ar", DL(LANG_ARABIC), 3, "ISO-8859-6"},
	{"Armenian",				"hy"},
	{"Assamese",				"as"},
	{"Aymara",					"ay"},
	{"Azerbaijani",				"az"},
	
	{"Belarusian",				"be", DL(LANG_BELARUSIAN), 5, "ISO-8859-5"},
	{"Bashkir",					"ba"},
	{"Basque",					"eu", DL(LANG_BASQUE), 4, "ISO-8859-1"},
	{"Bengali",					"bn"},
	{"Bhutani",					"dz"},
	{"Bihari",					"bh"},
	{"Bislama",					"bi"},
	{"Breton",					"br"},
	{"Bosnian",					"bs"},
	{"Brazilian Portuguese",	"pt_br", 0, 66},
	{"Bulgarian",				"bg", DL(LANG_BULGARIAN), 6, "ISO-8859-5"},
	{"Burmese",					"my"},
	
	{"Cambodian",				"km"},
	{"Catalan",					"ca", DL(LANG_CATALAN), 7, "ISO-8859-1"},
	{"Chinese",					"zh", DL(LANG_CHINESE), 8},
	{"Chinese Simplified",		"zh_cn"},
	{"Chinese Traditional",		"zh_tw"},
	{"Corsican",				"co"},
	{"Croatian",				"hr", DL(LANG_CROATIAN), 9, "ISO-8859-2"},
	{"Czech",					"cs", DL(LANG_CZECH), 10, "ISO-8859-2"},
	
	{"Danish",					"da", DL(LANG_DANISH), 11, "ISO-8859-1"},
	{"Dutch",					"nl", DL(LANG_DUTCH), 12, "ISO-8859-1"},
	
	{"English",					"en", DL(LANG_ENGLISH), 13, "ISO-8859-1"},
	{"Esperanto",				"eo"},
	{"Estonian",				"et", DL(LANG_ESTONIAN), 14, "ISO-8859-4"},
	
	{"Faeroese",				"fo", DL(LANG_FAEROESE), 15, "ISO-8859-1"},
	{"Farsi",					"fa", DL(LANG_FARSI), 16},
	{"Fiji",					"fj"},
	{"Finnish",					"fi", DL(LANG_FINNISH), 17, "ISO-8859-1"},
	{"French",					"fr", DL(LANG_FRENCH), 18, "ISO-8859-1"},
	{"Frisian",					"fy"},
	
	{"Galician",				"gl", 0, 0, "ISO-8859-3"},
	{"Gaelic",					"gd"},
	{"Gaelic",					"gv"},
	{"Georgian",				"ka"},
	{"German",					"de", DL(LANG_GERMAN), 38, "ISO-8859-1"},
	{"Greek",					"el", DL(LANG_GREEK), 40, "ISO-8859-7"},
	{"Greenlandic",				"kl"},
	{"Guarani",					"gn"},
	{"Gujarati",				"gu"},
	
	{"Hausa",					"ha"},
	{"Hebrew",					"he", DL(LANG_HEBREW), 42, "ISO-8859-8"},
	{"Hindi",					"hi"},
	{"Hungarian",				"hu", DL(LANG_HUNGARIAN), 44, "ISO-8859-2"},
	
	{"Icelandic",				"is", DL(LANG_ICELANDIC), 45, "ISO-8859-1"},
	{"Indonesian",				"id", DL(LANG_INDONESIAN), 46},
	{"Interlingua",				"ia"},
	{"Interlingue",				"ie"},
	{"Inuktitut",				"iu"},
	{"Inupiak",					"ik"},
	{"Irish",					"ga", 0, 0, "ISO-8859-1"},
	{"Italian",					"it", DL(LANG_ITALIAN), 47, "ISO-8859-1"},
	
	{"Japanese",				"ja", DL(LANG_JAPANESE), 48},
	{"Javanese",				"jv"},
	
	{"Kannada",					"kn"},
	{"Kashmiri",				"ks"},
	{"Kazakh",					"kk"},
	{"Kinyarwanda",				"rw"},
	{"Kirghiz",					"ky"},
	{"Kirundi",					"rn"},
	{"Korean",					"ko", DL(LANG_KOREAN), 49},
	{"Kurdish",					"ku"},
	
	{"Laothian",				"lo"},
	{"Latin",					"la"},
	{"Latvian",					"lv", DL(LANG_LATVIAN), 50, "ISO-8859-4"},
	{"Limburgish",				"li"},
	{"Lingala",					"ln"},
	{"Lithuanian",				"lt", DL(LANG_LITHUANIAN), 51, "ISO-8859-4"},
	{"Luganda",					"lg"},

	{"Macedonian",				"mk", 0, 0, "ISO-8859-5"},
	{"Malagasy",				"mg"},
	{"Malay",					"ms"},
	{"Malayalam",				"ml"},
	{"Maltese",					"mt", 0, 0, "ISO-8859-3"},
	{"Maori",					"mi"},
	{"Marathi",					"mr"},
	{"Moldavian",				"mo"},
	{"Mongolian",				"mn"},

	{"Nauru",					"na"},
	{"Nepali",					"ne"},
	{"Norwegian",				"no", DL(LANG_NORWEGIAN), 53, "ISO-8859-1"},

	{"Occitan",					"oc"},
	{"Oriya",					"or"},
	{"Oromo",					"om"},

	{"Pashto",					"ps"},
	{"Polish",					"pl", DL(LANG_POLISH), 54, "ISO-8859-2"},
	{"Portuguese",				"pt", DL(LANG_PORTUGUESE), 55, "ISO-8859-1"},
	{"Punjabi",					"pa"},

	{"Quechua",					"qu"},

	{"Rhaeto-Romance",			"rm"},
	{"Romanian",				"ro", DL(LANG_ROMANIAN), 56, "ISO-8859-2"},
	// Rundi: same as Kirundi
	{"Runyakitara",				"ry"},
	{"Russian",					"ru", DL(LANG_RUSSIAN), 57, "ISO-8859-5"},

	{"Samoan",					"sm"},
	{"Sangro",					"sg"},
	{"Sanskrit",				"sa"},
	{"Serbian",					"sr", DL(LANG_SERBIAN), 58, "ISO-8859-5"},
	{"Serbo-Croatian",			"sh"},
	{"Sesotho",					"st"},
	{"Setswana",				"tn"},
	{"Shona",					"sn"},
	{"Sindhi",					"sd"},
	{"Sinhalese",				"si"},
	{"Siswati",					"ss"},
	{"Slovak",					"sk", DL(LANG_SLOVAK), 59, "ISO-8859-2"},
	{"Slovenian",				"sl", DL(LANG_SLOVENIAN), 60, "ISO-8859-2"},
	{"Somali",					"so"},
	{"Spanish",					"es", DL(LANG_SPANISH), 61, "ISO-8859-1"},
	{"Sundanese",				"su"},
	{"Swahili",					"sw"},
	{"Swedish",					"sv", DL(LANG_SWEDISH), 62, "ISO-8859-1"},

	{"Tagalog",					"tl"},
	{"Tajik",					"tg"},
	{"Tamil",					"ta"},
	{"Tatar",					"tt"},
	{"Telugu",					"te"},
	{"Thai",					"th", DL(LANG_THAI), 63},
	{"Tibetan",					"bo"},
	{"Tigrinya",				"ti"},
	{"Tonga",					"to"},
	{"Tsonga",					"ts"},
	{"Turkish",					"tr", DL(LANG_TURKISH), 64, "ISO-8859-3"},
	{"Turkmen",					"tk"},
	{"Twi",						"tw"},

	{"Uighur",					"ug"},
	{"Ukrainian",				"uk", 0, 0, "ISO-8859-5"},
	{"Urdu",					"ur"},
	{"Uzbek",					"uz"},

	{"Vietnamese",				"vi", DL(LANG_VIETNAMESE), 65},
	{"Volapük",				"vo"}, // Thats a UTF-8 ?

	{"Welsh",					"cy"},
	{"Wolof",					"wo"},

	{"Xhosa",					"xh"},

	{"Yiddish",					"yi"},
	{"Yoruba",					"yo"},

	{"Zulu",					"zu"},

	{0}
};

GLanguage *GFindLang(GLanguageId Id, const char *Name)
{
	if (!Id)
		return LgiLanguageTable;

	if (Id && Id[0] && Id[1])
	{
		// O(1) Language lookup generated by _LgiGenLangLookup() below
		switch (Id[0])
		{
			case 'a':
			{
				switch (tolower(Id[1]))
				{
					case 'b':
						return LgiLanguageTable + 0;
					case 'a':
						return LgiLanguageTable + 1;
					case 'f':
						return LgiLanguageTable + 2;
					case 'm':
						return LgiLanguageTable + 4;
					case 'r':
						return LgiLanguageTable + 5;
					case 's':
						return LgiLanguageTable + 7;
					case 'y':
						return LgiLanguageTable + 8;
					case 'z':
						return LgiLanguageTable + 9;
				}
				break;
			}
			case 'b':
			{
				switch (tolower(Id[1]))
				{
					case 'e':
						return LgiLanguageTable + 10;
					case 'a':
						return LgiLanguageTable + 11;
					case 'n':
						return LgiLanguageTable + 13;
					case 'h':
						return LgiLanguageTable + 15;
					case 'i':
						return LgiLanguageTable + 16;
					case 'r':
						return LgiLanguageTable + 17;
					case 's':
						return LgiLanguageTable + 18;
					case 'g':
						return LgiLanguageTable + 20;
					case 'o':
						return LgiLanguageTable + 127;
				}
				break;
			}
			case 'c':
			{
				switch (tolower(Id[1]))
				{
					case 'a':
						return LgiLanguageTable + 23;
					case 'o':
						return LgiLanguageTable + 27;
					case 's':
						return LgiLanguageTable + 29;
					case 'y':
						return LgiLanguageTable + 140;
				}
				break;
			}
			case 'd':
			{
				switch (tolower(Id[1]))
				{
					case 'z':
						return LgiLanguageTable + 14;
					case 'a':
						return LgiLanguageTable + 30;
					case 'e':
						return LgiLanguageTable + 45;
				}
				break;
			}
			case 'e':
			{
				switch (tolower(Id[1]))
				{
					case 'u':
						return LgiLanguageTable + 12;
					case 'n':
						return LgiLanguageTable + 32;
					case 'o':
						return LgiLanguageTable + 33;
					case 't':
						return LgiLanguageTable + 34;
					case 'l':
						return LgiLanguageTable + 46;
					case 's':
						return LgiLanguageTable + 117;
				}
				break;
			}
			case 'f':
			{
				switch (tolower(Id[1]))
				{
					case 'o':
						return LgiLanguageTable + 35;
					case 'a':
						return LgiLanguageTable + 36;
					case 'j':
						return LgiLanguageTable + 37;
					case 'i':
						return LgiLanguageTable + 38;
					case 'r':
						return LgiLanguageTable + 39;
					case 'y':
						return LgiLanguageTable + 40;
				}
				break;
			}
			case 'g':
			{
				switch (tolower(Id[1]))
				{
					case 'l':
						return LgiLanguageTable + 41;
					case 'd':
						return LgiLanguageTable + 42;
					case 'v':
						return LgiLanguageTable + 43;
					case 'n':
						return LgiLanguageTable + 48;
					case 'u':
						return LgiLanguageTable + 49;
					case 'a':
						return LgiLanguageTable + 60;
				}
				break;
			}
			case 'h':
			{
				switch (tolower(Id[1]))
				{
					case 'y':
						return LgiLanguageTable + 6;
					case 'r':
						return LgiLanguageTable + 28;
					case 'a':
						return LgiLanguageTable + 50;
					case 'e':
						return LgiLanguageTable + 51;
					case 'i':
						return LgiLanguageTable + 52;
					case 'u':
						return LgiLanguageTable + 53;
				}
				break;
			}
			case 'i':
			{
				switch (tolower(Id[1]))
				{
					case 's':
						return LgiLanguageTable + 54;
					case 'd':
						return LgiLanguageTable + 55;
					case 'a':
						return LgiLanguageTable + 56;
					case 'e':
						return LgiLanguageTable + 57;
					case 'u':
						return LgiLanguageTable + 58;
					case 'k':
						return LgiLanguageTable + 59;
					case 't':
						return LgiLanguageTable + 61;
				}
				break;
			}
			case 'j':
			{
				switch (tolower(Id[1]))
				{
					case 'a':
						return LgiLanguageTable + 62;
					case 'v':
						return LgiLanguageTable + 63;
				}
				break;
			}
			case 'k':
			{
				switch (tolower(Id[1]))
				{
					case 'm':
						return LgiLanguageTable + 22;
					case 'a':
						return LgiLanguageTable + 44;
					case 'l':
						return LgiLanguageTable + 47;
					case 'n':
						return LgiLanguageTable + 64;
					case 's':
						return LgiLanguageTable + 65;
					case 'k':
						return LgiLanguageTable + 66;
					case 'y':
						return LgiLanguageTable + 68;
					case 'o':
						return LgiLanguageTable + 70;
					case 'u':
						return LgiLanguageTable + 71;
				}
				break;
			}
			case 'l':
			{
				switch (tolower(Id[1]))
				{
					case 'o':
						return LgiLanguageTable + 72;
					case 'a':
						return LgiLanguageTable + 73;
					case 'v':
						return LgiLanguageTable + 74;
					case 'i':
						return LgiLanguageTable + 75;
					case 'n':
						return LgiLanguageTable + 76;
					case 't':
						return LgiLanguageTable + 77;
					case 'g':
						return LgiLanguageTable + 78;
				}
				break;
			}
			case 'm':
			{
				switch (tolower(Id[1]))
				{
					case 'y':
						return LgiLanguageTable + 21;
					case 'k':
						return LgiLanguageTable + 79;
					case 'g':
						return LgiLanguageTable + 80;
					case 's':
						return LgiLanguageTable + 81;
					case 'l':
						return LgiLanguageTable + 82;
					case 't':
						return LgiLanguageTable + 83;
					case 'i':
						return LgiLanguageTable + 84;
					case 'r':
						return LgiLanguageTable + 85;
					case 'o':
						return LgiLanguageTable + 86;
					case 'n':
						return LgiLanguageTable + 87;
				}
				break;
			}
			case 'n':
			{
				switch (tolower(Id[1]))
				{
					case 'l':
						return LgiLanguageTable + 31;
					case 'a':
						return LgiLanguageTable + 88;
					case 'e':
						return LgiLanguageTable + 89;
					case 'o':
						return LgiLanguageTable + 90;
				}
				break;
			}
			case 'o':
			{
				switch (tolower(Id[1]))
				{
					case 'c':
						return LgiLanguageTable + 91;
					case 'r':
						return LgiLanguageTable + 92;
					case 'm':
						return LgiLanguageTable + 93;
				}
				break;
			}
			case 'p':
			{
				if (Id[1] == 't' && Id[2] == '_' && Id[3] == 'b' && Id[4] == 'r' && Id[5] == 0)
					return LgiLanguageTable + 19;
				switch (tolower(Id[1]))
				{
					case 's':
						return LgiLanguageTable + 94;
					case 'l':
						return LgiLanguageTable + 95;
					case 't':
						return LgiLanguageTable + 96;
					case 'a':
						return LgiLanguageTable + 97;
				}
				break;
			}
			case 'q':
			{
				switch (tolower(Id[1]))
				{
					case 'u':
						return LgiLanguageTable + 98;
				}
				break;
			}
			case 'r':
			{
				switch (tolower(Id[1]))
				{
					case 'w':
						return LgiLanguageTable + 67;
					case 'n':
						return LgiLanguageTable + 69;
					case 'm':
						return LgiLanguageTable + 99;
					case 'o':
						return LgiLanguageTable + 100;
					case 'y':
						return LgiLanguageTable + 101;
					case 'u':
						return LgiLanguageTable + 102;
				}
				break;
			}
			case 's':
			{
				switch (tolower(Id[1]))
				{
					case 'q':
						return LgiLanguageTable + 3;
					case 'm':
						return LgiLanguageTable + 103;
					case 'g':
						return LgiLanguageTable + 104;
					case 'a':
						return LgiLanguageTable + 105;
					case 'r':
						return LgiLanguageTable + 106;
					case 'h':
						return LgiLanguageTable + 107;
					case 't':
						return LgiLanguageTable + 108;
					case 'n':
						return LgiLanguageTable + 110;
					case 'd':
						return LgiLanguageTable + 111;
					case 'i':
						return LgiLanguageTable + 112;
					case 's':
						return LgiLanguageTable + 113;
					case 'k':
						return LgiLanguageTable + 114;
					case 'l':
						return LgiLanguageTable + 115;
					case 'o':
						return LgiLanguageTable + 116;
					case 'u':
						return LgiLanguageTable + 118;
					case 'w':
						return LgiLanguageTable + 119;
					case 'v':
						return LgiLanguageTable + 120;
				}
				break;
			}
			case 't':
			{
				switch (tolower(Id[1]))
				{
					case 'n':
						return LgiLanguageTable + 109;
					case 'l':
						return LgiLanguageTable + 121;
					case 'g':
						return LgiLanguageTable + 122;
					case 'a':
						return LgiLanguageTable + 123;
					case 't':
						return LgiLanguageTable + 124;
					case 'e':
						return LgiLanguageTable + 125;
					case 'h':
						return LgiLanguageTable + 126;
					case 'i':
						return LgiLanguageTable + 128;
					case 'o':
						return LgiLanguageTable + 129;
					case 's':
						return LgiLanguageTable + 130;
					case 'r':
						return LgiLanguageTable + 131;
					case 'k':
						return LgiLanguageTable + 132;
					case 'w':
						return LgiLanguageTable + 133;
				}
				break;
			}
			case 'u':
			{
				switch (tolower(Id[1]))
				{
					case 'g':
						return LgiLanguageTable + 134;
					case 'k':
						return LgiLanguageTable + 135;
					case 'r':
						return LgiLanguageTable + 136;
					case 'z':
						return LgiLanguageTable + 137;
				}
				break;
			}
			case 'v':
			{
				switch (tolower(Id[1]))
				{
					case 'i':
						return LgiLanguageTable + 138;
					case 'o':
						return LgiLanguageTable + 139;
				}
				break;
			}
			case 'w':
			{
				switch (tolower(Id[1]))
				{
					case 'o':
						return LgiLanguageTable + 141;
				}
				break;
			}
			case 'x':
			{
				switch (tolower(Id[1]))
				{
					case 'h':
						return LgiLanguageTable + 142;
				}
				break;
			}
			case 'y':
			{
				switch (tolower(Id[1]))
				{
					case 'i':
						return LgiLanguageTable + 143;
					case 'o':
						return LgiLanguageTable + 144;
				}
				break;
			}
			case 'z':
			{
				if (Id[1] == 'h' && Id[2] == '_' && Id[3] == 'c' && Id[4] == 'n' && Id[5] == 0)
					return LgiLanguageTable + 25;
				if (Id[1] == 'h' && Id[2] == '_' && Id[3] == 't' && Id[4] == 'w' && Id[5] == 0)
					return LgiLanguageTable + 26;
				switch (tolower(Id[1]))
				{
					case 'h':
						return LgiLanguageTable + 24;
					case 'u':
						return LgiLanguageTable + 145;
				}
				break;
			}
		}
	}
	else if (Name)
	{
		for (GLanguage *l=LgiLanguageTable; l->Id; l++)
		{
			if (stricmp(Name, l->Name) == 0)
			{
				return l;
			}
		}
	}

	return 0;
}

GLanguage *GFindOldLang(int OldId)
{
	switch (OldId)
	{
		case 1:
			return LgiLanguageTable + 2;
		case 3:
			return LgiLanguageTable + 5;
		case 5:
			return LgiLanguageTable + 10;
		case 6:
			return LgiLanguageTable + 20;
		case 7:
			return LgiLanguageTable + 23;
		case 10:
			return LgiLanguageTable + 29;
		case 11:
			return LgiLanguageTable + 30;
		case 38:
			return LgiLanguageTable + 45;
		case 4:
			return LgiLanguageTable + 12;
		case 13:
			return LgiLanguageTable + 32;
		case 14:
			return LgiLanguageTable + 34;
		case 40:
			return LgiLanguageTable + 46;
		case 61:
			return LgiLanguageTable + 117;
		case 15:
			return LgiLanguageTable + 35;
		case 16:
			return LgiLanguageTable + 36;
		case 17:
			return LgiLanguageTable + 38;
		case 18:
			return LgiLanguageTable + 39;
		case 9:
			return LgiLanguageTable + 28;
		case 42:
			return LgiLanguageTable + 51;
		case 44:
			return LgiLanguageTable + 53;
		case 45:
			return LgiLanguageTable + 54;
		case 46:
			return LgiLanguageTable + 55;
		case 47:
			return LgiLanguageTable + 61;
		case 48:
			return LgiLanguageTable + 62;
		case 49:
			return LgiLanguageTable + 70;
		case 50:
			return LgiLanguageTable + 74;
		case 51:
			return LgiLanguageTable + 77;
		case 12:
			return LgiLanguageTable + 31;
		case 53:
			return LgiLanguageTable + 90;
		case 66:
			return LgiLanguageTable + 19;
		case 54:
			return LgiLanguageTable + 95;
		case 55:
			return LgiLanguageTable + 96;
		case 56:
			return LgiLanguageTable + 100;
		case 57:
			return LgiLanguageTable + 102;
		case 2:
			return LgiLanguageTable + 3;
		case 58:
			return LgiLanguageTable + 106;
		case 59:
			return LgiLanguageTable + 114;
		case 60:
			return LgiLanguageTable + 115;
		case 62:
			return LgiLanguageTable + 120;
		case 63:
			return LgiLanguageTable + 126;
		case 64:
			return LgiLanguageTable + 131;
		case 65:
			return LgiLanguageTable + 138;
		case 8:
			return LgiLanguageTable + 24;
	}

	return 0;	
}

#ifdef _DEBUG
LgiFunc char *_LgiGenLangLookup()
{
	char s[256];
	GStringPipe p;
	GStringPipe Old;

	Old.Push("switch (OldId)\n{\n");

	p.Push(	"switch (Id[0])\n"
			"{\n");

	for (char i='a'; i<='z'; i++)
	{
		List<int> Index;
		List<GLanguage> d;

		int n=0;
		for (GLanguage *Lang = LgiLanguageTable; Lang->Id; Lang++, n++)
		{
			if (tolower(Lang->Id[0]) == i)
			{
				if (Lang->OldId)
				{
					sprintf_s(s, sizeof(s), "\tcase %i:\n\t\treturn LgiLanguageTable + %i;\n", Lang->OldId, n);
					Old.Push(s);
				}

				d.Insert(Lang);
				Index.Insert(new int(n));
			}
		}

		if (d.First())
		{
			GLanguage *t;

			sprintf_s(s, sizeof(s),
					"\tcase '%c':\n"
					"\t{\n", i);
			p.Push(s);

			n = 0;
			for (t=d.First(); t; t=d.Next(), n++)
			{
				if (strlen(t->Id) > 2)
				{
					p.Push("\t\tif (");
					int k;
					for (k=1; t->Id[k]; k++)
					{
						if (k > 1) p.Push(" AND ");
						sprintf_s(s, sizeof(s), "Id[%i] == '%c'", k, t->Id[k]);
						p.Push(s);
					}
					sprintf_s(s, sizeof(s),
							" AND Id[%i] == 0)\n"
							"\t\t\treturn LgiLanguageTable + %i;\n",
							k,
							*Index[n]);
					p.Push(s);
				}
			}

			p.Push("\t\tswitch (tolower(Id[1]))\n\t\t{\n");

			n = 0;
			for (t=d.First(); t; t=d.Next(), n++)
			{
				if (strlen(t->Id) == 2)
				{
					sprintf_s(s, sizeof(s),
							"\t\t\tcase '%c':\n"
							"\t\t\t\treturn LgiLanguageTable + %i;\n",
							t->Id[1],
							*Index[n]);
					p.Push(s);
				}
			}

			p.Push("\t\t}\n");
			p.Push(	"\t\tbreak;\n"
					"\t}\n");
		}

		Index.DeleteObjects();
	}

	p.Push("}\n\n");

	Old.Push("}\n");
	char *OldText = Old.NewStr();
	if (OldText)
	{
		p.Push(OldText);
		DeleteArray(OldText);
	}

	return p.NewStr();
}
#endif

////////////////////////////////////////////////////////////////////
char Res_Dialog[]		= "Dialog";
char Res_Table[]		= "TableLayout";
char Res_ControlTree[]	= "ControlTree";
char Res_StaticText[]	= "StaticText";
char Res_EditBox[]		= "EditBox";
char Res_CheckBox[]		= "CheckBox";
char Res_Button[]		= "Button";
char Res_Group[]		= "Group";
char Res_RadioBox[]		= "RadioBox";
char Res_TabView[]		= "TabView";
char Res_Tab[]			= "Tab";
char Res_ListView[]		= "ListView";
char Res_Column[]		= "Column";
char Res_ComboBox[]		= "ComboBox";
char Res_TreeView[]		= "TreeView";
char Res_Bitmap[]		= "Bitmap";
char Res_Progress[]		= "Progress";
char Res_Slider[]		= "Slider";
char Res_ScrollBar[]	= "ScrollBar";
char Res_Custom[]		= "Custom";

////////////////////////////////////////////////////////////////////
class ResObjectImpl
{
protected:
	static int TabDepth;
	ResFactory *Factory;

	void TabString(char *Str);
	char *ReadInt(char *s, int &Value);
	// bool IsEndTag(GXmlTag *Tag, char *ObjName);

public:
	enum SStatus
	{
		SError,
		SOk,
		SExclude,
	};

	ResObject *Object;
	ResObjectImpl *GetImpl(ResObject *o)
	{
		return (o) ? o->GetObjectImpl(Factory) : 0;
	}

	ResObjectImpl(ResFactory *factory, ResObject *object);
	ResObjectImpl *CreateCtrl(GXmlTag *Tag, ResObject *Parent);

	// Store
	virtual SStatus Res_Read(GXmlTag *Tag, ResReadCtx &Ctx);
	virtual SStatus Res_Write(GXmlTag *Tag);

	// Ctrl
	void Res_SetPos(int x1, int y1, int x2, int y2);
	void Res_SetPos(GXmlTag *t);
	bool Res_SetStrRef(GXmlTag *t, ResReadCtx *Ctx);
	void Res_SetFlags(GXmlTag *t);

	void WritePos(GXmlTag *t);
	void WriteStrRef(GXmlTag *t);
	void WriteFlags(GXmlTag *t);

	// Children
	void Res_Attach(ResObjectImpl *Parent);
	bool Res_GetChildren(List<ResObjectImpl> *Children, bool Deep);

	// Items (Columns, Tabs etc)
	void Res_Append(ResObjectImpl *Parent);
	bool Res_GetItems(List<ResObjectImpl> *Items);
};


// Each object needs it's own loaded/saver
class ResDialogObj : public ResObjectImpl
{
public:
	ResDialogObj(ResFactory *f, ResObject *o) : ResObjectImpl(f, o) {}
	SStatus Res_Read(GXmlTag *Tag, ResReadCtx &Ctx);
	SStatus Res_Write(GXmlTag *t);
};

class ResTableLayout : public ResObjectImpl
{
public:
	ResTableLayout(ResFactory *f, ResObject *o) : ResObjectImpl(f, o)
	{
	}
	
	SStatus Res_Read(GXmlTag *Tag, ResReadCtx &Ctx);
	SStatus Res_Write(GXmlTag *t);
};

class ResStaticText : public ResObjectImpl
{
public:
	ResStaticText(ResFactory *f, ResObject *o) : ResObjectImpl(f, o) {}
};

class ResEditBox : public ResObjectImpl
{
public:
	ResEditBox(ResFactory *f, ResObject *o) : ResObjectImpl(f, o) {}
	SStatus Res_Read(GXmlTag *Tag, ResReadCtx &Ctx);
	SStatus Res_Write(GXmlTag *t);
};

class ResCheckBox : public ResObjectImpl
{
public:
	ResCheckBox(ResFactory *f, ResObject *o) : ResObjectImpl(f, o) {}
};

class ResButton : public ResObjectImpl
{
public:
	ResButton(ResFactory *f, ResObject *o) : ResObjectImpl(f, o) {}
};

class ResGroup : public ResObjectImpl
{
public:
	ResGroup(ResFactory *f, ResObject *o) : ResObjectImpl(f, o) {}
	SStatus Res_Read(GXmlTag *Tag, ResReadCtx &Ctx);
	SStatus Res_Write(GXmlTag *t);
};

class ResRadio : public ResObjectImpl
{
public:
	ResRadio(ResFactory *f, ResObject *o) : ResObjectImpl(f, o) {}
};

class ResTabView : public ResObjectImpl
{
public:
	ResTabView(ResFactory *f, ResObject *o) : ResObjectImpl(f, o) {}
	SStatus Res_Read(GXmlTag *Tag, ResReadCtx &Ctx);
	SStatus Res_Write(GXmlTag *t);
};

class ResTab : public ResObjectImpl
{
public:
	ResTab(ResFactory *f, ResObject *o) : ResObjectImpl(f, o) {}
	SStatus Res_Read(GXmlTag *Tag, ResReadCtx &Ctx);
	SStatus Res_Write(GXmlTag *t);
};

class ResColumn : public ResObjectImpl
{
public:
	ResColumn(ResFactory *f, ResObject *o) : ResObjectImpl(f, o) {}
	SStatus Res_Read(GXmlTag *Tag, ResReadCtx &Ctx);
	SStatus Res_Write(GXmlTag *t);
};

class ResListView : public ResObjectImpl
{
public:
	ResListView(ResFactory *f, ResObject *o) : ResObjectImpl(f, o) {}
	SStatus Res_Read(GXmlTag *Tag, ResReadCtx &Ctx);
	SStatus Res_Write(GXmlTag *t);
};

class ResComboBox : public ResObjectImpl
{
public:
	ResComboBox(ResFactory *f, ResObject *o) : ResObjectImpl(f, o) {}
};

class ResScrollBar : public ResObjectImpl
{
public:
	ResScrollBar(ResFactory *f, ResObject *o) : ResObjectImpl(f, o) {}
};

class ResTreeView : public ResObjectImpl
{
public:
	ResTreeView(ResFactory *f, ResObject *o) : ResObjectImpl(f, o) {}
};

class ResBitmap : public ResObjectImpl
{
public:
	ResBitmap(ResFactory *f, ResObject *o) : ResObjectImpl(f, o) {}
};

class ResSlider : public ResObjectImpl
{
public:
	ResSlider(ResFactory *f, ResObject *o) : ResObjectImpl(f, o) {}
};

class ResProgress : public ResObjectImpl
{
public:
	ResProgress(ResFactory *f, ResObject *o) : ResObjectImpl(f, o) {}
};

class ResCustom : public ResObjectImpl
{
public:
	ResCustom(ResFactory *f, ResObject *o) : ResObjectImpl(f, o) {}
	SStatus Res_Read(GXmlTag *Tag, ResReadCtx &Ctx);
	SStatus Res_Write(GXmlTag *t);
};

class ResControlTree : public ResObjectImpl
{
public:
	ResControlTree(ResFactory *f, ResObject *o) : ResObjectImpl(f, o) {}
	SStatus Res_Read(GXmlTag *Tag, ResReadCtx &Ctx);
	SStatus Res_Write(GXmlTag *t);
};

////////////////////////////////////////////////////////////////////
ResObject::ResObject(char *Name)
{
	_ObjName = Name;
	_ObjImpl = 0;
}

ResObject::~ResObject()
{
	DeleteObj(_ObjImpl);
}

ResObjectImpl *ResObject::GetObjectImpl(ResFactory *f)
{
	if (_ObjName &&
		!_ObjImpl &&
		f)
	{
		#define Create(CtrlName, Obj) \
			if (stricmp(_ObjName, CtrlName) == 0) \
			{ \
				return _ObjImpl = new Obj(f, this); \
			}

		Create(Res_Dialog, ResDialogObj);
		Create(Res_Table, ResTableLayout);
		Create(Res_StaticText, ResStaticText);
		Create(Res_EditBox, ResEditBox);
		Create(Res_CheckBox, ResCheckBox);
		Create(Res_Button, ResButton);
		Create(Res_Group, ResGroup);
		Create(Res_RadioBox, ResRadio);
		Create(Res_Tab, ResTab);
		Create(Res_TabView, ResTabView);
		Create(Res_ListView, ResListView);
		Create(Res_Column, ResColumn);
		Create(Res_ComboBox, ResComboBox);
		Create(Res_ScrollBar, ResScrollBar);
		Create(Res_TreeView, ResTreeView);
		Create(Res_Bitmap, ResBitmap);
		Create(Res_Slider, ResSlider);
		Create(Res_Progress, ResProgress);
		Create(Res_Custom, ResCustom);
		Create(Res_ControlTree, ResControlTree);
	}

	return _ObjImpl;
}

////////////////////////////////////////////////////////////////////
bool ResFactory::Res_Read(ResObject *Obj, GXmlTag *Tag, ResReadCtx &Ctx)
{
	if (!Obj)
	{
		LgiAssert(0);
		return false;
	}

	ResObjectImpl *Impl = Obj->GetObjectImpl(this);
	if (!Impl)
	{
		LgiAssert(0);
		return false;
	}

	return Impl->Res_Read(Tag, Ctx) >= ResObjectImpl::SOk;
}

bool ResFactory::Res_Write(ResObject *Obj, GXmlTag *Tag)
{
	if (Obj)
	{
		ResObjectImpl *Impl = Obj->GetObjectImpl(this);
		if (Impl)
		{
			return Impl->Res_Write(Tag) >= ResObjectImpl::SOk;
		}
		else LgiAssert(0);
	}
	else LgiAssert(0);
	
	return false;
}

////////////////////////////////////////////////////////////////////
ResObjectImpl::ResObjectImpl(ResFactory *factory, ResObject *object)
{
	Object = object;
	Factory = factory;

	LgiAssert(Object && (NativeInt)Object != 0xcdcdcdcd);
}

ResObjectImpl *ResObjectImpl::CreateCtrl(GXmlTag *Tag, ResObject *Parent)
{
	ResObject *o = Factory->CreateObject(Tag, Parent);
	return (o) ? o->GetObjectImpl(Factory) : 0;
}

ResObjectImpl::SStatus ResObjectImpl::Res_Read(GXmlTag *Tag, ResReadCtx &Ctx)
{
	if (!Tag)
	{
		LgiAssert(0);
		return SError;
	}

	char *ObjName = Object->GetObjectName();
	if (!stricmp(ObjName, "Slider") ||
		!stricmp(Tag->GetTag(), ObjName))
	{
		Res_SetPos(Tag);
		if (!Res_SetStrRef(Tag, &Ctx))
			return SExclude;
		Res_SetFlags(Tag);

		const char *Style = Tag->GetAttr("style");
		if (Style)
		{
			GVariant v = Style;
			GDom *d = Factory->Res_GetDom(Object);
			if (d)
				d->SetValue("style", v);
		}

		return SOk;
	}
	else
	{
		// Custom control needs to inherit from 'ResObject'
		LgiAssert(0);
	}

	return SError;
}

ResObjectImpl::SStatus ResObjectImpl::Res_Write(GXmlTag *t)
{
	t->SetTag(Object->GetObjectName());
	WritePos(t);
	WriteStrRef(t);
	WriteFlags(t);

	return SOk;
}

void ResObjectImpl::Res_SetPos(int x1, int y1, int x2, int y2)
{
	Factory->Res_SetPos(Object, x1, y1, x2, y2);
}

void ResObjectImpl::WritePos(GXmlTag *t)
{
	GRect r = Factory->Res_GetPos(Object);
	t->SetAttr("pos", r.Describe());
}

bool ResObjectImpl::Res_SetStrRef(GXmlTag *t, ResReadCtx *Ctx)
{
	char *s;
	if ((s = t->GetAttr("ref")))
	{
		return Factory->Res_SetStrRef(Object, atoi(s), Ctx);
	}

	LgiAssert(0);
	return false;
}

void ResObjectImpl::Res_SetFlags(GXmlTag *t)
{
	Factory->Res_SetProperties(Object, t);
}

void ResObjectImpl::WriteStrRef(GXmlTag *t)
{
	int Ref = Factory->Res_GetStrRef(Object);
	if (Ref >= 0)
	{
		char s[32];
		sprintf_s(s, sizeof(s), "%i", Ref);
		t->SetAttr("ref", s);
	}
}

void ResObjectImpl::WriteFlags(GXmlTag *t)
{
	Factory->Res_GetProperties(Object, t);
}

void ResObjectImpl::Res_Attach(ResObjectImpl *Parent)
{
	Factory->Res_Attach(Object, Parent->Object);
}

bool ResObjectImpl::Res_GetChildren(List<ResObjectImpl> *Children, bool Deep)
{
	List<ResObject> l;
	bool Status = Factory->Res_GetChildren(Object, &l, Deep);
	if (Status && Children)
	{
		for (ResObject *o = l.First(); o; o = l.Next())
		{
			Children->Insert(GetImpl(o));
		}
	}

	return Status;
}

void ResObjectImpl::Res_Append(ResObjectImpl *Parent)
{
	Factory->Res_Append(Object, Parent->Object);
}

bool ResObjectImpl::Res_GetItems(List<ResObjectImpl> *Items)
{
	List<ResObject> l;
	bool Status = Factory->Res_GetItems(Object, &l);
	if (Status && Items)
	{
		for (ResObject *o = l.First(); o; o = l.Next())
		{
			Items->Insert(GetImpl(o));
		}
	}

	return Status;
}

int ResObjectImpl::TabDepth = 0;
void ResObjectImpl::TabString(char *Str)
{
	if (Str)
	{
		for (int i=0; i<TabDepth; i++)
		{
			*Str++ = 9;
		}
		*Str = 0;
	}
}

char *ResObjectImpl::ReadInt(char *s, int &Value)
{
	char *c = strchr(s, ',');
	if (c)
	{
		*c = 0;
		Value = atoi(s);
		return c+1;
	}
	Value = atoi(s);
	return 0;
}

void ResObjectImpl::Res_SetPos(GXmlTag *t)
{
	char *Str = t->GetAttr("pos");
	if (Str)
	{
		char s[256];
		strcpy_s(s, sizeof(s), Str);

		int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
		char *p = ReadInt(s, x1);
		if (p) p = ReadInt(p, y1);
		if (p) p = ReadInt(p, x2);
		if (p) p = ReadInt(p, y2);

		Res_SetPos(x1, y1, x2, y2);
	}
}

////////////////////////////////////////////////////////////////////
ResObjectImpl::SStatus ResDialogObj::Res_Read(GXmlTag *Tag, ResReadCtx &Ctx)
{
	if (!Tag)
	{
		LgiAssert(0);
		return SError;
	}

	int OffsetType = 0;
	for (GXmlTag *p = Tag; p; p = p->Parent)
	{
		if (!p->Parent)
		{
			char *ot = p->GetAttr("Offset");
			if (ot)
				OffsetType = atoi(ot);
		}
	}

	if (!Tag->IsTag(Res_Dialog))
	{
		LgiAssert(0);
		return SError;
	}

	Res_SetPos(Tag);
	Res_SetStrRef(Tag, &Ctx);

	for (GXmlTag *t = Tag->Children.First(); t; t = Tag->Children.Next())
	{
		ResObjectImpl *Ctrl = CreateCtrl(t, Object);
		if (Ctrl && Ctrl->Res_Read(t, Ctx))
		{
			if (!OffsetType)
			{
				GViewI *v = dynamic_cast<GViewI*>(Ctrl->Object);
				if (v)
				{
					GRect r = v->GetPos();
					r.Offset(-3, -17);
					v->SetPos(r);
				}
			}

			Ctrl->Res_Attach(this);
		}
		else
		{
			LgiTrace("%s:%i - failed the parse the '%s' tag.\n", t->GetTag());
			LgiAssert(0);
			return SError;
		}
	}

	return SOk;
}

ResObjectImpl::SStatus ResDialogObj::Res_Write(GXmlTag *t)
{
	ResObjectImpl::SStatus s = ResObjectImpl::Res_Write(t);

	List<ResObjectImpl> Children;
	if (Res_GetChildren(&Children, false))
	{
		for (ResObjectImpl *c = Children.First(); c; c = Children.Next())
		{
			GXmlTag *a = new GXmlTag;
			if (a)
			{
				ResObjectImpl::SStatus r = c->Res_Write(a);
				if (r == SOk)
				{
					t->InsertTag(a);
				}
				else
				{
					LgiAssert(0);
					DeleteObj(a);
					
					LgiTrace("%s:%i - Res_Write returned %i\n", _FL, r);
					return r;
				}
			}
		}
	}

	return s;
}

////////////////////////////////////////////////////////////////////////////
int CountNumbers(char *s)
{
	GToken t(s, ",");
	return (int)t.Length();
}

ResObjectImpl::SStatus ResTableLayout::Res_Read(GXmlTag *Tag, ResReadCtx &Ctx)
{
	if (!Tag || !Tag->IsTag(Res_Table))
		return SError;
	
	Res_SetPos(Tag);
	if (!Res_SetStrRef(Tag, &Ctx))
		return SExclude;
	Res_SetFlags(Tag);
	const char *Style = Tag->GetAttr("style");
	if (Style)
	{
		GVariant v = Style;
		GDom *d = Factory->Res_GetDom(Object);
		if (d)
			d->SetValue("style", v);
	}

	GDom *d = Factory->Res_GetDom(Object);
	if (d)
	{
		GVariant v;
		int Cx = 0, Cy = 0;

		char *s;
		if ((s = Tag->GetAttr("cols")))
		{
			v = s;
			d->SetValue("cols", v);
			Cx = CountNumbers(s);
		}
		if ((s = Tag->GetAttr("rows")))
		{
			v = s;
			d->SetValue("rows", v);
			Cy = CountNumbers(s);
		}
		v = Tag->GetAttr("style");
		if (v.Str())
			d->SetValue("style", v);

		GRect Bounds(0, 0, Cx-1, Cy-1);
		#define UsedCell(x, y) Used[(Cx * y) + x]
		bool *Used = new bool[Cx * Cy];
		if (Used)
		{
			memset(Used, 0, sizeof(*Used) * Cx * Cy);

			int x = 0, y = 0;
			for (GXmlTag *Tr = Tag->Children.First(); Tr; Tr = Tag->Children.Next())
			{
				if (!Tr->IsTag("Tr"))
					continue;

				for (GXmlTag *Td = Tr->Children.First(); Td; Td = Tr->Children.Next())
				{
					if (!Td->IsTag("Td"))
						continue;

					while (UsedCell(x, y))
					{
						x++;
					}

					int ColSpan = 1;
					int RowSpan = 1;
					if ((s = Td->GetAttr("colspan")))
						ColSpan = atoi(s);
					if ((s = Td->GetAttr("rowspan")))
						RowSpan = atoi(s);

					char CellName[32];
					GDom *Cell = 0;
					v = (void*) &Cell;
					sprintf_s(CellName, sizeof(CellName), "cell[%i,%i]", x, y);
					if (d->SetValue(CellName, v))
					{
						GRect Span(x, y, x + ColSpan - 1, y + RowSpan - 1);
						if (Span.x2 < Cx &&
							Span.y2 < Cy)
						{
							for (int Y=Span.y1; Y<=Span.y2; Y++)
							{
								for (int X=Span.x1; X<=Span.x2; X++)
								{
									UsedCell(X, Y) = true;
								}
							}

							if (ColSpan > 1 || RowSpan > 1)
							{
								v = Span.GetStr();
								Cell->SetValue("span", v);
							}

							if ((s = Td->GetAttr("align")))
								Cell->SetValue("align", v = s);
							if ((s = Td->GetAttr("valign")))
								Cell->SetValue("valign", v = s);
							if ((s = Td->GetAttr("class")))
								Cell->SetValue("class", v = s);
							if ((s = Td->GetAttr("style")))
								Cell->SetValue("style", v = s);

							if (v.SetList())
							{
								for (GXmlTag *Ctrl = Td->Children.First(); Ctrl; Ctrl = Td->Children.Next())
								{
									ResObjectImpl *c = CreateCtrl(Ctrl, Object);
									if (c)
									{
										ResObjectImpl::SStatus Status = c->Res_Read(Ctrl, Ctx);
										if (Status == SOk)
										{
											v.Value.Lst->Insert(new GVariant((void*)c->Object));
										}
										else
										{
											delete c->Object;
										}
									}
								}
							}

							Cell->SetValue("children", v);
						}
						else
						{
							LgiAssert(0);
						}
					}

					x += ColSpan;
				}

				y++;
				x = 0;
			}

			for (y=0; y<Cy; y++)
			{
				for (x=0; x<Cx; x++)
				{
					if (!UsedCell(x, y))
					{
						// Create empty cell for unused slot
						char CellName[32];
						GDom *Cell = 0;
						v = (void*) &Cell;
						sprintf_s(CellName, sizeof(CellName), "cell[%i,%i]", x, y);
						d->SetValue(CellName, v);
					}
				}
			}

			DeleteArray(Used);
		}
	}

	return SOk;
}

ResObjectImpl::SStatus ResTableLayout::Res_Write(GXmlTag *t)
{
	ResObjectImpl::SStatus s = ResObjectImpl::Res_Write(t);

	GDom *d = Factory->Res_GetDom(Object);
	if (d)
	{
		GVariant v;
		int Cx = 0, Cy = 0;
		if (d->GetValue("cols", v))
		{
			t->SetAttr("cols", v.Str());
			Cx = CountNumbers(v.Str());
		}
		if (d->GetValue("rows", v))
		{
			t->SetAttr("rows", v.Str());
			Cy = CountNumbers(v.Str());
		}
		for (int y=0; y<Cy; y++)
		{
			GXmlTag *Tr = new GXmlTag("tr");
			if (Tr)
			{
				t->InsertTag(Tr);

				for (int x=0; x<Cx; )
				{
					char a[32];
					sprintf_s(a, sizeof(a), "cell[%i,%i]", x, y);
					if (d->GetValue(a, v) &&
						v.Type == GV_DOM)
					{
						GDom *c = v.Value.Dom;
						if (c)
						{
							GRect Span;
							if (c->GetValue("span", v) &&
								Span.SetStr(v.Str()))
							{
								if (Span.x1 == x &&
									Span.y1 == y)
								{
									GXmlTag *Td = new GXmlTag("td");
									if (Td)
									{
										Tr->InsertTag(Td);
										if (Span.X() > 1)
											Td->SetAttr("colspan", Span.X());
										if (Span.Y() > 1)
											Td->SetAttr("rowspan", Span.Y());
										
										if (c->GetValue("align", v) &&
											v.Str() &&
											stricmp(v.Str(), "Min") != 0)
										{
											Td->SetAttr("align", v.Str());
										}
										if (c->GetValue("valign", v) &&
											v.Str() &&
											stricmp(v.Str(), "Min") != 0)
										{
											Td->SetAttr("valign", v.Str());
										}
										if (c->GetValue("class", v) && ValidStr(v.Str()))
											Td->SetAttr("class", v.Str());
										if (c->GetValue("style", v) && ValidStr(v.Str()))
											Td->SetAttr("style", v.Str());

										if (c->GetValue("children", v) &&
											v.Type == GV_LIST)
										{
											for (GVariant *n = v.Value.Lst->First(); n; n = v.Value.Lst->Next())
											{
												if (n->Type == GV_VOID_PTR)
												{
													ResObject *Obj = (ResObject*) n->Value.Ptr;
													if (Obj)
													{
														ResObjectImpl *Impl = Obj->GetObjectImpl(Factory);
														if (Impl)
														{
 															GXmlTag *a = new GXmlTag;
															if (a && Impl->Res_Write(a))
															{
																Td->InsertTag(a);
															}
															else DeleteObj(a);
														}
													}
												}
											}
										}
									}
								}

								x = Span.x2 + 1;
							}
							else break;
						}
						else break;
					}
					else break;
				}
			}
		}
	}

	return s;
}

//////////////////////////////////////////////////////////////////////////////////
ResObjectImpl::SStatus ResEditBox::Res_Read(GXmlTag *Tag, ResReadCtx &Ctx)
{
	bool Status = false;
	if (Tag)
	{
		if (stricmp(Tag->GetTag(), Res_EditBox) == 0)
		{
			Status = true;

			Res_SetPos(Tag);
			if (!Res_SetStrRef(Tag, &Ctx))
				return SExclude;
			Factory->Res_SetProperties(Object, Tag);
		}
	}

	return SOk;
}

ResObjectImpl::SStatus ResEditBox::Res_Write(GXmlTag *t)
{
	Factory->Res_GetProperties(Object, t);
	return ResObjectImpl::Res_Write(t);
}

////////////////////////////////////////////////////////////////////////////////
ResObjectImpl::SStatus ResGroup::Res_Read(GXmlTag *Tag, ResReadCtx &Ctx)
{
	ResObjectImpl::SStatus s = ResObjectImpl::Res_Read(Tag, Ctx);

	for (GXmlTag *t = Tag->Children.First(); t; t = t = Tag->Children.Next())
	{
		ResObjectImpl *Ctrl = CreateCtrl(t, Object);
		if (Ctrl && Ctrl->Res_Read(t, Ctx))
		{
			Ctrl->Res_Attach(this);
		}
		else
		{
			LgiAssert(0);
			return SError;
		}
	}

	return s;
}

ResObjectImpl::SStatus ResGroup::Res_Write(GXmlTag *t)
{
	ResObjectImpl::SStatus s = ResObjectImpl::Res_Write(t);

	List<ResObjectImpl> Children;
	if (Res_GetChildren(&Children, false))
	{
		for (ResObjectImpl *c = Children.First(); c; c = Children.Next())
		{
			GXmlTag *a = new GXmlTag;
			if (a && c->Res_Write(a))
			{
				t->InsertTag(a);
			}
			else DeleteObj(a);
		}
	}

	return s;
}

///////////////////////////////////////////////////////////////////////////////////
ResObjectImpl::SStatus ResTab::Res_Read(GXmlTag *Tag, ResReadCtx &Ctx)
{
	if (!Tag || !Tag->IsTag(Res_Tab))
	{
		LgiAssert(0);
		return SError;
	}

	if (!Res_SetStrRef(Tag, &Ctx))
		return SExclude;

	for (GXmlTag *t = Tag->Children.First(); t; t = Tag->Children.Next())
	{
		ResObjectImpl *Ctrl = CreateCtrl(t, Object);
		if (!Ctrl)
		{
			LgiAssert(0);
			return SError;
		}
		
		SStatus e = Ctrl->Res_Read(t, Ctx);
		if (e == SOk)
		{
			Ctrl->Res_Attach(this);
		}
		else if (e == SExclude)
		{
			delete Ctrl->Object;
		}
		else
		{
			return e;
		}
	}

	return SOk;
}

ResObjectImpl::SStatus ResTab::Res_Write(GXmlTag *t)
{
	t->SetTag(Res_Tab);
	WriteStrRef(t);

	List<ResObjectImpl> Children;
	if (Res_GetChildren(&Children, false))
	{
		for (ResObjectImpl *c = Children.First(); c; c = Children.Next())
		{
			GXmlTag *a = new GXmlTag;
			if (a && c->Res_Write(a))
			{
				t->InsertTag(a);
			}
			else DeleteObj(a);
		}
	}

	return SOk;
}

///////////////////////////////////////////////////////////////////////////////////
ResObjectImpl::SStatus ResTabView::Res_Read(GXmlTag *Tag, ResReadCtx &Ctx)
{
	if (!Tag || !Tag->IsTag(Res_TabView))
	{
		LgiAssert(0);
		return SError;
	}

	Res_SetPos(Tag);
	Res_SetStrRef(Tag, &Ctx);

	for (GXmlTag *t = Tag->Children.First(); t; t = Tag->Children.Next())
	{
		if (!t->IsTag(Res_Tab))
		{
			LgiAssert(0);
			return SError;
		}

		ResObjectImpl *Tab = CreateCtrl(t, Object);
		if (!Tab)
		{
			LgiAssert(0);
			return SError;
		}

		if (Tab->Res_Read(t, Ctx) == SOk)
			Res_Append(Tab);
		else
			delete Tab->Object;
	}

	return SOk;
}

ResObjectImpl::SStatus ResTabView::Res_Write(GXmlTag *t)
{
	ResObjectImpl::SStatus s = ResObjectImpl::Res_Write(t);
	
	List<ResObjectImpl> Items;
	if (Res_GetItems(&Items))
	{
		for (ResObjectImpl *Tab = Items.First(); Tab; Tab = Items.Next())
		{
			GXmlTag *a = new GXmlTag;
			if (a && Tab->Res_Write(a))
			{
				t->InsertTag(a);
			}
			else DeleteObj(a);
		}
	}

	return s;
}

///////////////////////////////////////////////////////////////////////////////////
ResObjectImpl::SStatus ResColumn::Res_Read(GXmlTag *Tag, ResReadCtx &Ctx)
{
	if (!Tag || !Tag->IsTag(Res_Column))
	{
		LgiAssert(0);
		return SError;
	}

	char *s = 0;
	if ((s = Tag->GetAttr("width")))
	{
		Res_SetPos(0, 0, atoi(s) - 1, 20);
	}
	Res_SetStrRef(Tag, &Ctx);

	return SOk;
}

ResObjectImpl::SStatus ResColumn::Res_Write(GXmlTag *t)
{
	GRect Pos = Factory->Res_GetPos(Object);

	t->SetTag(Res_Column);
	WriteStrRef(t);
	t->SetAttr("width", Pos.X());

	return SOk;
}

//////////////////////////////////////////////////////////////////////////////
ResObjectImpl::SStatus ResListView::Res_Read(GXmlTag *t, ResReadCtx &Ctx)
{
	if (!t)
	{
		LgiAssert(0);
		return SError;
	}

	if (!Object->GetObjectName() || stricmp(t->GetTag(), Object->GetObjectName()) != 0)
	{
		LgiAssert(0);
		return SError;
	}

	Res_SetPos(t);
	Res_SetStrRef(t, &Ctx);
	Factory->Res_SetProperties(Object, t);

	for (GXmlTag *c = t->Children.First(); c; c = t->Children.Next())
	{
		if (stricmp(c->GetTag(), Res_Column) == 0)
		{
			ResObjectImpl *Col = CreateCtrl(c, Object);
			if (Col && Col->Res_Read(c, Ctx))
			{
				Res_Append(Col);
			}
			else
			{
				LgiAssert(0);
				return SError;
			}
		}
		else
		{
			LgiAssert(0);
			return SError;
		}
	}

	return SOk;
}

ResObjectImpl::SStatus ResListView::Res_Write(GXmlTag *t)
{
	ResObjectImpl::SStatus s = ResObjectImpl::Res_Write(t);

	List<ResObjectImpl> Items;
	if (Res_GetItems(&Items))
	{
		for (ResObjectImpl *c = Items.First(); c; c = Items.Next())
		{
			GXmlTag *a = new GXmlTag;
			if (a && c->Res_Write(a))
			{
				t->InsertTag(a);
			}
			else DeleteObj(a);
		}
	}

	return s;
}

/////////////////////////////////////////////////////////////
ResObjectImpl::SStatus ResCustom::Res_Read(GXmlTag *t, ResReadCtx &Ctx)
{
	if (!t || !t->IsTag(Res_Custom))
	{
		LgiAssert(0);
		return SError;
	}

	Res_SetPos(t);
	Res_SetStrRef(t, &Ctx);

	Factory->Res_SetProperties(Object, t);

	return SOk;
}

ResObjectImpl::SStatus ResCustom::Res_Write(GXmlTag *t)
{
	char Tabs[256];
	TabString(Tabs);

	ResObjectImpl::SStatus s = ResObjectImpl::Res_Write(t);
	Factory->Res_GetProperties(Object, t);

	return s;
}

/////////////////////////////////////////////////////////////
ResObjectImpl::SStatus ResControlTree::Res_Read(GXmlTag *t, ResReadCtx &Ctx)
{
	if (!t || stricmp(t->GetTag(), Res_ControlTree))
	{
		LgiAssert(0);
		return SError;
	}

	Res_SetPos(t);
	Res_SetStrRef(t, &Ctx);

	GDom *d = Factory->Res_GetDom(Object);
	if (!d)
	{
		LgiAssert(0);
		return SError;
	}

	GVariant v;
	d->SetValue("LgiFactory", v = Factory);
	d->SetValue("Tree", v = t);
	return SOk;
}

ResObjectImpl::SStatus ResControlTree::Res_Write(GXmlTag *t)
{
	ResObjectImpl::SStatus s = ResObjectImpl::Res_Write(t);

	LgiAssert(!t->GetAttr("id"));

	GDom *d = Factory->Res_GetDom(Object);
	if (!d)
	{
		LgiAssert(0);
		return SError;
	}

	GVariant v;
	if (d->GetValue("Tree", v))
	{
		GXmlTag *n = dynamic_cast<GXmlTag*>(v.Value.Dom);
		if (n)
		{
			GXmlTag *c;
			while ((c = n->Children.First()))
			{
				t->InsertTag(c);
			}
		}
	}

	return s;
}


