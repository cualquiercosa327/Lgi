/*hdr
**      FILE:           GSelectFont.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           26/9/2001
**      DESCRIPTION:    Font selection dialog
**
**      Copyright (C) 2001, Matthew Allen
**              fret@ozemail.com.au
*/

#include <stdio.h>
#include <stdlib.h>
#include "Lgi.h"

#include "Clipboard.h"

///////////////////////////////////////////////////////////////////////////////////////////
int FontSizes[] = { 8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 48, 72, 100, 0 };

GSelectFont::GSelectFont(GView *parent, char *Face)
{
	Font[0] = 0;
	Size = 0;

	if (Face)
	{
		char *Comma = strchr(Face, ',');
		if (Comma)
		{
			char *FontName = NewStr(Face, (int)Comma-(int)Face);
			if (FontName)
			{
				strcpy(Font, FontName);
				DeleteArray(FontName);
			}

			Size = atoi(Comma+1);
		}
	}
	
	Name("Select Font");
	Pos.ZOff(180, 100);
	MoveToCenter();

	int DefFont = 0;
	int DefSize = 0;
	
	Children.Insert(new GText(-1, 10, 10, -1, -1, "Font:"));
	Children.Insert(CFont = new GCombo(100, 50, 10, 100, 20, "Font"));
	if (CFont)
	{
		int32 numFamilies = count_font_families();
		for (int32 i = 0; i < numFamilies; i++)
		{
			font_family Temp;
			uint32 flags;
	
			if (get_font_family(i, &Temp, &flags) == B_OK)
			{
				CFont->Insert(Temp);
				if (stricmp(Temp, Font) == 0)
				{
					DefFont = i;
				}
	
				/*
				int32 numStyles = count_font_styles(family);
				for (int32 j = 0; j < numStyles; j++)
				{
					font_style style;
					if (get_font_style(family, j, &style, &flags) == B_OK)
					{
					}
				}
				*/
			}
		}
		
		CFont->Value(DefFont);
	}
	
	Children.Insert(new GText(-1, 10, 40, -1, -1, "Size:"));
	Children.Insert(CSize = new GCombo(100, 50, 40, 100, 20, "Size"));
	if (CSize)
	{
		int n=0;
		for (int *i = FontSizes; *i; i++, n++)
		{
			char Str[20];
			sprintf(Str, "%i", *i);
			CSize->Insert(Str);
			if (*i == Size)
			{
				DefSize = n;
			}
		}

		CSize->Value(DefSize);
	}
	
	Children.Insert(new GButton(IDOK, 20, 70, 60, 20, "Ok"));
	Children.Insert(new GButton(IDCANCEL, 90, 70, 60, 20, "Cancel"));
}

GSelectFont::~GSelectFont()
{
}

int GSelectFont::OnNotify(GViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case IDOK:
		{
			if (CFont AND CSize)
			{
				strcpy(Font, CFont->GCombo::Name());
				Size = FontSizes[CSize->Value()];
			}
			
			EndModal(1);
			break;
		}
		case IDCANCEL:
		{
			EndModal(0);
			break;
		}
	}
	
	return 0;
}



