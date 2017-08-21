#ifdef WIN32
class GChar : public LListItem
{
	ushort i;
	GFont *Fnt;

public:
	GChar(ushort I, GFont *fnt)
	{
		i = I;
		Fnt = fnt;

		char Num[8];
		sprintf(Num, "%i", i);
		SetText(Num, 0);
	}

	void OnPaintColumn(GSurface *pDC, GRect &r, int Col, GListColumn *c, COLOUR Fore, COLOUR Back)
	{
		if (Col == 1)
		{
			ushort s[2] = {i, 0};
			Fnt->TextW(pDC, r.x1, r.y1, s, 1, &r);
		}
		else
		{
			LListItem::OnPaintColumn(pDC, r, Col, c, Fore, Back);
		}
	}

	void OnMeasure(GMeasureInfo *Info)
	{
		Info->y = 32;
	}
};
#endif

