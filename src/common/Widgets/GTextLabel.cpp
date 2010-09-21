#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "GTextLabel.h"
#include "GUtf8.h"

class GTextPrivate : public GSemaphore
{
public:
	int Mx, My;
	List<GDisplayString> Strs;
	bool Wrap;

	GTextPrivate() : GSemaphore("GTextPrivate")
	{
		Wrap = false;
		Mx = My = 0;
	}

	~GTextPrivate()
	{
		Strs.DeleteObjects();
	}

	uint32 NextChar(char *s)
	{
		int Len = 0;
		while (s[Len] AND Len < 6) Len++;
		return LgiUtf8To32((uint8*&)s, Len);
	}

	uint32 PrevChar(char *s)
	{
		if (IsUtf8_Lead(*s) OR IsUtf8_1Byte(*s))
		{
			s--;
			int Len = 1;
			while (IsUtf8_Trail(*s) AND Len < 6) { s--; Len++; }

			return LgiUtf8To32((uint8*&)s, Len);
		}

		return 0;
	}

	void Layout(GFont *f, char *s, int Width)
	{
		if (Lock(_FL))
		{
			Mx = My = 0;
			Strs.DeleteObjects();
			while (s AND *s)
			{
				char *e = strchr(s, '\n');
				if (!e) e = s + strlen(s);
				int Len = (int)e - (int)s;

				GDisplayString *n = new GDisplayString(f, Len ? s : (char*)"", Len ? Len : 1);
				if (Wrap)
				{
					int All = LgiCharLen((OsChar*)*n, sizeof(OsChar) < 2 ? "utf-8" : LGI_WideCharset);
					int Ch = n->CharAt(Width);
					if (Ch <= 0)
					{
					}
					else if (Ch < All)
					{
						// Break string into chunks
						e = LgiSeekUtf8(s, Ch);
						while (e > s)
						{
							uint32 n = PrevChar(e);
							if (LGI_BreakableChar(n))
								break;
							e = LgiSeekUtf8(e, -1, s);
						}
						if (e == s)
						{
							e = LgiSeekUtf8(s, Ch);
							while (*e)
							{
								uint32 n = NextChar(e);
								if (LGI_BreakableChar(n))
									break;
								e = LgiSeekUtf8(e, 1);
							}
						}

						DeleteObj(n);
						n = new GDisplayString(f, s, (int)e - (int)s);
					}
				}

				Strs.Insert(n);
				s = *e == '\n' ? e + 1 : e;

				if (n)
				{
					Mx = max(Mx, n->X());
					My += n->Y();
				}
			}
			Unlock();
		}
	}
};

GText::GText(int id, int x, int y, int cx, int cy, char *name) :
	ResObject(Res_StaticText)
{
	d = new GTextPrivate;
	Name(name);

	if (cx < 0) cx = d->Mx;
	if (cy < 0) cy = d->My;

	GRect r(x, y, x+cx, y+cy);
	SetPos(r);
	SetId(id);
}

GText::~GText()
{
	DeleteObj(d);
}

bool GText::GetWrap()
{
	return d->Wrap;
}

void GText::SetWrap(bool b)
{
	d->Wrap = b;
	d->Layout(GetFont(), GBase::Name(), X());
	Invalidate();
}

bool GText::Name(char *n)
{
	bool Status = GView::Name(n);
	d->Layout(GetFont(), GBase::Name(), X());
	Invalidate();
	return Status;
}

bool GText::NameW(char16 *n)
{
	bool Status = GView::NameW(n);
	d->Layout(GetFont(), GBase::Name(), X());
	Invalidate();
	return Status;
}

void GText::SetFont(GFont *Fnt, bool OwnIt)
{
	GView::SetFont(Fnt, OwnIt);
	d->Layout(GetFont(), GBase::Name(), X());
	Invalidate();
}

int64 GText::Value()
{
	char *n = Name();
	#ifdef _MSC_VER
	return (n) ? _atoi64(n) : 0;
	#else
	return (n) ? atoll(n) : 0;
	#endif
}

void GText::Value(int64 i)
{
	char Str[32];
	sprintf(Str, LGI_PrintfInt64, i);
	Name(Str);
}

void GText::OnPosChange()
{
	if (d->Wrap)
	{
		d->Layout(GetFont(), GBase::Name(), X());
	}
}

void GText::OnPaint(GSurface *pDC)
{
	bool Status = false;

	GViewFill *ForeFill = GetForegroundFill();
	COLOUR Fore32 = ForeFill ? ForeFill->GetC32() : Rgb24To32(LC_TEXT);

	GViewFill *BackFill = GetBackgroundFill();
	COLOUR Back32 = BackFill ? BackFill->GetC32() : Rgb24To32(LC_MED);
	
	GFont *f = GetFont();
	if (d->Lock(_FL))
	{
		if (d->Strs.First())
		{
			f->Transparent(Back32 == 0);
			f->Colour32(Fore32, Back32);

			int y = 0;
			for (GDisplayString *s = d->Strs.First(); s; s = d->Strs.Next())
			{
				if (Enabled())
				{
					GRect r(0, y, X()-1, y + s->Y() - 1);
					s->Draw(pDC, 0, y, &r);
				}
				else
				{
					GRect r(0, y, X()-1, y + s->Y() - 1);
					f->Transparent(Back32 == 0);
					f->Colour32(Rgb24To32(LC_LIGHT), Back32);
					s->Draw(pDC, 1, y+1, &r);
					
					f->Transparent(true);
					f->Colour(LC_LOW, LC_MED);
					s->Draw(pDC, 0, y, &r);
				}
				
				y += s->Y();
			}

			if (y < Y())
			{
				pDC->Colour(Back32, 32);
				pDC->Rectangle(0, y, X()-1, Y()-1);
			}
			Status = true;
		}
		d->Unlock();
	}
	
	if (!Status)
	{
		pDC->Colour(Back32, 32);
		pDC->Rectangle();
	}

	#if 0
	pDC->Colour(Rgb24(0, 0, 255), 24);
	pDC->Box(0, 0, X()-1, Y()-1);
	#endif	
}
