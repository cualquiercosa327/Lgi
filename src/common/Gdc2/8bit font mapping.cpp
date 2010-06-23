#include "Lgi.h"

/* Windows code pages:

	ANSI_CHARSET			1252
	RUSSIAN_CHARSET			1251
	EE_CHARSET				1250
	GREEK_CHARSET			1253
	TURKISH_CHARSET			1254
	BALTIC_CHARSET			1257
	HEBREW_CHARSET			1255
	ARABIC _CHARSET			1256
	SHIFTJIS_CHARSET		932
	HANGEUL_CHARSET			949
	GB2313_CHARSET			936
	CHINESEBIG5_CHARSET		950

*/

short _gdc_usascii_mapping[128] =
{
	//80     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F
	0xc7, 0xfc, 0xe9, 0xe2, 0xe4, 0xe0, 0xe5, 0xe7, 0xea, 0xeb, 0xe8, 0xef, 0xee, 0xec, 0xc4, 0xc5,
	//90-9F
	0xc9, 0xe6, 0xc6, 0xf4, 0xf6, 0xf2, 0xfb, 0xf9, 0xff, 0xd6, 0xdc, 0xa2, 0xa3, 0xa5, 0x99, 0x83,
	//A0-AF
	0xe1, 0xed, 0xf3, 0xfa, 0xf1, 0xd1, 0xb2, 0xb0, 0xbf, 0xad, 0xac, 0xbd, 0xbc, 0xa1, 0xab, 0xbb,
	//B0-BF
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	//C0-CF
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	//D0-DF
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	//E0-EF
	0x61, 0xdf, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	//F0-FF
	0x81, 0xb1, 0x81, 0x81, 0x81, 0x81, 0xf7, 0x81, 0x81, 0x95, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
};

short _gdc_ISO_8859_2_mapping[128] =
{
	//80     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	//90-9F
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	//A0-AF
	0xa0, 0xa5, 0xa2, 0xa3, 0xa4, 0xbc, 0x8c, 0xa7, 0xa8, 0x8a, 0xaa, 0x8d, 0x8f, 0xad, 0x8e, 0xaf,
	//B0-BF
	0xb0, 0xb9, 0xb2, 0xb3, 0xb4, 0xbe, 0x9c, 0xa1, 0xb8, 0x9a, 0xba, 0x9d, 0x9f, 0xbd, 0x9e, 0xbf,
	//C0-CF
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	//D0-DF
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	//E0-EF
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	//F0-FF
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
};

#if defined WIN32
#define WinDef(d) d
#else
#define WinDef(d) 0
#endif

GCodePageInfo CodePages[] =
{
	{GDCF_ASCII,		0, "Ascii",						"us-ascii",		WinDef(ANSI_CHARSET),		0,		_gdc_usascii_mapping},
	{GDCF_ISO_8859_1,	0, "ISO 8859-1 (West Europe)",	"iso-8859-1",	WinDef(ANSI_CHARSET),		0,		0},
	{GDCF_ISO_8859_2,	0, "ISO 8859-2 (East Europe)",	"iso-8859-2",	WinDef(EASTEUROPE_CHARSET),	0,		_gdc_ISO_8859_2_mapping},
	{GDCF_ISO_8859_3,	0, "ISO 8859-3 (Latin Script)",	"iso-8859-3",	WinDef(TURKISH_CHARSET),	0,		0},
	{GDCF_ISO_8859_4,	0, "ISO 8859-4 (Baltic)",		"iso-8859-4",	WinDef(BALTIC_CHARSET),		0,		0},
	{GDCF_ISO_8859_5,	0, "ISO 8859-5 (Russian)",		"iso-8859-5",	WinDef(RUSSIAN_CHARSET),	0,		0},
	{GDCF_ISO_8859_6,	0, "ISO 8859-6 (Arabic)",		"iso-8859-6",	WinDef(ARABIC_CHARSET),		0,		0},
	{GDCF_ISO_8859_7,	0, "ISO 8859-7 (Greek)",		"iso-8859-7",	WinDef(GREEK_CHARSET),		0,		0},
	{GDCF_ISO_8859_8,	0, "ISO 8859-8 (Hebrew)",		"iso-8859-8",	WinDef(HEBREW_CHARSET),		0,		0},
	{GDCF_ISO_8859_9,	0, "ISO 8859-9 (Turkish)",		"iso-8859-9",	WinDef(TURKISH_CHARSET),	0,		0},
	{GDCF_WIN_1250,		0, "Win1250 (Central Europe)",	"windows-1250",	WinDef(EASTEUROPE_CHARSET),	1250,	0},
	{GDCF_WIN_1252,		0, "Win1252 (Windows Latin 1)",	"windows-1252",	WinDef(ANSI_CHARSET),		1252,	0},
	{GDCF_SHIFTJIS,		CP_FLG_DOUBLE_BYTE | CP_FLG_NOT_IMPL,
						   "Shift JIS",					"windows-932",	WinDef(SHIFTJIS_CHARSET),	0,		0},
	{GDCF_CHINESEBIG5,	CP_FLG_DOUBLE_BYTE | CP_FLG_NOT_IMPL,
						   "Chinese BIG5",				"windows-950",	WinDef(CHINESEBIG5_CHARSET), 0,		0},
	{GDCF_THAI,			CP_FLG_DOUBLE_BYTE | CP_FLG_NOT_IMPL,
						   "Thai",						"windows-874",	WinDef(THAI_CHARSET),		0,		0},
	{GDCF_MAC,			CP_FLG_NOT_IMPL,
						   "Apple Mac",	0, 0},
	{-1, 0, 0, 0, 0, 0, 0} // EOF marker
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
GCodePageInfo *GdcGetCodePageInfo(int CodePage)
{
	for (GCodePageInfo *c = CodePages; c->Id >= 0; c++)
	{
		if (c->Id == CodePage)
		{
			return c;
		}
	}

	return 0;
}

char *GdcGetCodePageDescription(int CodePage)
{
	GCodePageInfo *cp = GdcGetCodePageInfo(CodePage);
	return cp ? cp->DisplayName : 0;
}

char *GdcGetCodePageMimeType(int CodePage)
{
	GCodePageInfo *cp = GdcGetCodePageInfo(CodePage);
	return cp ? cp->MimeType : 0;
}

bool GdcIsCodePageImplemented(int CodePage)
{
	GCodePageInfo *cp = GdcGetCodePageInfo(CodePage);
	return cp ? cp->Flags == 0 : false;
}

int GdcAnsiToLgiCodePage(int AnsiCodePage)
{
	for (GCodePageInfo *c = CodePages; c->Id >= 0; c++)
	{
		if (c->AnsiCodePage == AnsiCodePage)
		{
			return c->Id;
		}
	}

	return GDCF_WIN_1252; // a good a default as any
}

bool GdcConvertCodePage(int ToCp, int FromCp, char *Str, int Len)
{
	bool Status = false;
	uchar *s = (uchar*) Str;

	if (s AND
		ToCp != FromCp)
	{
		bool HasExtended = false;
		if (Len < 0) Len = strlen(Str);
		for (int i=0; i<Len; i++)
		{
			if (s[i] & 0x80)
			{
				HasExtended = true;
				break;
			}
		}

		if (HasExtended)
		{
			GCodePageInfo *To = GdcGetCodePageInfo(ToCp);
			GCodePageInfo *From = GdcGetCodePageInfo(FromCp);
			if (To AND
				From AND
				To->OsCodePage == From->OsCodePage)
			{
				short Remap[128];
				short *Map = 0;
				bool Reverse = false;

				if (To->Translation)
				{
					// Reverse map
					int i;
					for (i=0; i<128; i++) Remap[i] = i+128;
					for (i=0; i<128; i++)
					{
						Remap[(To->Translation[i]-128) & 0x7f] = 0x80 + i;
					}
					Map = Remap;
				}
				else if (From->Translation)
				{
					Map = From->Translation;
				}

				if (Map)
				{
					for (int i=0; i<Len; i++)
					{
						if (s[i] >= 0x80)
						{
							s[i] = Map[ (s[i]-0x80) & 0x7f ];
						}
					}

					Status = true;
				}
			}
		}
	}

	return Status;
}

bool GdcGetCodePageMappingTable(int CodePage, short *&Table)
{
	GCodePageInfo *cp = GdcGetCodePageInfo(CodePage);
	if (cp AND cp->Map8)
	{
		Table = cp->Map8;
		return true;
	}

	return false;
}

