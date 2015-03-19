/*
**	FILE:			GToolbar.cpp
**	AUTHOR:			Matthew Allen
**	DATE:				18/10/2001
**	DESCRIPTION:	Toolbar classes
**
**	Copyright (C) 2001, Matthew Allen
**		fret@memecode.com
*/

#include <stdlib.h>
#include <stdio.h>
#include "Lgi.h"
#include "GToken.h"
#include "GVariant.h"
#include "GDisplayString.h"
#include "GPalette.h"

#define ToolBarHilightColour LC_HIGH

#ifdef WIN32

HPALETTE GetSystemPalette();
bool BltBmpToBmp(HBITMAP hDest, int xDst, int yDst, int cx, int cy, HBITMAP hSrc, int xSrc, int ySrc, DWORD dwRop);
bool BltBmpToDc(HDC DestDC, int xDst, int yDst, int cx, int cy, HBITMAP hSrc, int xSrc, int ySrc,  DWORD dwRop);
bool BltDcToBmp(HBITMAP hDest, int xDst, int yDst, int cx, int cy, HDC SrcDC, int xSrc, int ySrc, DWORD dwRop);

#define AttachButton(b)	Children.Insert(b);

#else

#define AttachButton(b) b->Attach(this);

#endif

#ifdef BEOS
#include <Bitmap.h>
#endif

enum IconCacheType
{
	IconNormal,
	IconHilight,
	IconDisabled
};

COLOUR Map(GSurface *pDC, COLOUR c);

////////////////////////////////////////////////////////////////////////
GImageList *LgiLoadImageList(const char *File, int x, int y)
{
	GImageList *ImgList = 0;
	char *Path = FileExists(File) ? NewStr(File) : LgiFindFile(File);
	if (Path)
	{
		GSurface *pDC = LoadDC(Path);
		if (pDC)
		{
			ImgList = new GImageList(x, y, pDC);
			DeleteObj(pDC);
		}

		DeleteArray(Path);
	}
	else
	{
		printf("LgiLoadImageList: Couldn't find '%s'\n", File);
	}

	return ImgList;
}

GToolBar *LgiLoadToolbar(GViewI *Parent, const char *File, int x, int y)
{
	GToolBar *Toolbar = new GToolBar;
	if (Toolbar)
	{
		char *FileName = LgiFindFile(File);
		if (FileName)
		{
			bool Success = FileName && Toolbar->SetBitmap(FileName, x, y);
			if (!Success)
			{
				LgiMsg(Parent,
						"Can't load '%s' for the toolbar.\n"
						"You can find it in this program's archive.",
						"Lgi::LgiLoadToolbar",
						MB_OK,
						File);
				DeleteObj(Toolbar);
			}

			DeleteArray(FileName);
		}
		else
		{
			LgiMsg(Parent,
					"Can't find the graphic '%s' for the toolbar.\n"
					"You can find it in this program's archive.",
					"Lgi::LgiLoadToolbar",
					MB_OK,
					File);
		}
	}

	return Toolbar;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
#define ImgLst_Empty	0x40000000
#define IgmLst_Add		0x80000000

class GImageListPriv
{
public:
	#if defined BEOS
	GSurface *Mask;
	#endif

	int Sx, Sy;
	GRect *Bounds;

	GImageListPriv()
	{
		Sx = 0;
		Sy = 0;
		Bounds = 0;
	}

	~GImageListPriv()
	{
		DeleteArray(Bounds);
	}
};

template<typename T>
static void ProcessAlpha32(GSurface *pDC)
{
	T k;
	for (int y=0; y<pDC->Y(); y++)
	{
		T *p = (T*) (*pDC)[y];
		T *e = p + pDC->X();
		if (!y) k = *p;

		while (p < e)
		{
			if (p->r == k.r &&
				p->g == k.g &&
				p->b == k.b)
			{
				p->r = 0;
				p->g = 0;
				p->b = 0;
				p->a = 0;
			}
			p++;
		}
	}
}

GImageList::GImageList(int x, int y, GSurface *pDC)
{
	d = new GImageListPriv;
	d->Sx = x;
	d->Sy = y;

	#if WINNATIVE

	/*
	if (LgiGetOs() != LGI_OS_WIN9X)
		d->AlphaBlend = (MsImg32_AlphaBlend)GetProcAddress(GetModuleHandle("Msimg32.dll"), "AlphaBlend");
	*/

	if (pDC &&
		Create(pDC->X(), pDC->Y(), GdcD->AlphaBlend ? 32 : GdcD->GetBits()))
	{
		RGBQUAD *Rgb = GetInfo()->bmiColors;
		bool HasAlpha = pDC->GetBits() == 32;

		if (GdcD->GetBits() <= 8)
		{
			// create palette system palette
			PALETTEENTRY Entries[256];
			HDC hDC = CreateCompatibleDC(0);
			GetSystemPaletteEntries(hDC, 0, 256, Entries);
			DeleteDC(hDC);

			GPalette *Pal = new GPalette(0, 256);
			if (Pal)
			{
				for (int i=0; i<256; i++)
				{
					GdcRGB *p = (*Pal)[i];
					if (p)
					{
						Rgb[i].rgbRed = p->r = Entries[i].peRed;
						Rgb[i].rgbGreen = p->g = Entries[i].peGreen;
						Rgb[i].rgbBlue = p->b = Entries[i].peBlue;
						Rgb[i].rgbReserved = 0;
					}
				}

				Palette(Pal);
			}
		}

		if (pDC->GetBits() == 8 && GetBits() == 8)
		{
			// Set the palette
			HDC hDC = StartDC();
			if (hDC)
			{
				SetDIBColorTable(hDC, 0, 256, Rgb);
				EndDC();
			}

			// gotta remap here
			char Remap[256];
			GPalette *SrcPal = pDC->Palette();
			GPalette *DstPal = Palette();
			if (SrcPal && DstPal)
			{
				// build remap table
				ZeroObj(Remap);
				for (int i=0; i<256; i++)
				{
					GdcRGB *From = (*SrcPal)[i];
					if (From)
					{
						Remap[i] = DstPal->MatchRgb(Rgb24(From->r, From->g, From->b));
					}
				}

				// do blt
				for (int y=0; y<Y(); y++)
				{
					uchar *s = (*pDC)[y];
					uchar *d = (*this)[y];
					for (int x=0; x<X(); x++)
					{
						*d++ = Remap[*s++];
					}
				}
			}
		}
		else
		{
			// just standard copy blt
			Blt(0, 0, pDC);
		}

		if (GdcD->AlphaBlend && !HasAlpha)
		{
			int SrcBits = pDC->GetBits();
			if (SrcBits < 32)
			{
				switch (GetColourSpace())
				{
					#define ImgListCase(name) \
						case Cs##name: ProcessAlpha32<G##name>(this); break
					
					ImgListCase(Rgba32);
					ImgListCase(Bgra32);
					ImgListCase(Argb32);
					ImgListCase(Abgr32);
				}
			}
		}
	}

	#elif defined BEOS

	if (pDC)
	{
		int Bits = (GdcD->GetBits() == 32) ? 32 : 15;
		if (Create(pDC->X(), pDC->Y(), Bits))
		{
			GPalette *Pal = pDC->Palette();
			if (Pal)
			{
				Palette(new GPalette(Pal));
			}

			Blt(0, 0, pDC);
			
			COLOUR InitKey = Get(0, 0);

			for (int cy=0; cy<Y(); cy++)
			{
				switch (GetBits())
				{
					case 8:
					{
						uchar Key = InitKey;
						uchar *p = (*this)[cy];
						for (int cx=0; cx<X(); cx++)
						{
							if (p[cx] == Key)
							{
								p[cx] = B_TRANSPARENT_MAGIC_CMAP8;
							}
						}
						break;
					}
					case 15:
					{
						ushort Key = InitKey;
						ushort *p = (ushort*) (*this)[cy];
						ushort Trans = B_TRANSPARENT_MAGIC_RGBA15;
						
						for (int cx=0; cx<X(); cx++)
						{
							if (p[cx] == Trans)
							{
								p[cx] = Trans+1;
							}
							else if (p[cx] == Key)
							{
								p[cx] = Trans;
							}
						}
						break;
					}
					case 32:
					{
						ulong Key = InitKey;
						ulong *p = (ulong*) ((*this)[cy]);
						for (int cx=0; cx<X(); cx++)
						{
							if (	R32(p[cx]) == R32(Key) &&
									G32(p[cx]) == G32(Key) &&
									B32(p[cx]) == B32(Key))
							{
								p[cx] = B_TRANSPARENT_MAGIC_RGBA32;
							}
						}
						break;
					}
				}

			}
		}
	}

	#else

	if (pDC &&
		Create(pDC->X(), pDC->Y(),
		#if defined(MAC) || defined(__GTK_H__)
		32
		#else
		GdcD->GetBits()
		#endif
		))
	{
		printf("GImageList blt %s -> %s\n",
			GColourSpaceToString(GetColourSpace()),
			GColourSpaceToString(pDC->GetColourSpace()));
		Blt(0, 0, pDC);
		
		#if defined(__GTK_H__)

		GetBounds();

		COLOUR Key = Get(0, 0);
		for (int y=0; y<Y(); y++)
		{
			COLOUR *p = (COLOUR*) (*this)[y];
			LgiAssert(p);
			COLOUR *e = p + X();
			while (p < e)
			{
				if (*p == Key)
					*p = 0;
				p++;
			}
		}
		
		#elif defined(MAC)
		
		COLOUR Key = Get(0, 0);
		uint8 kr = R32(Key);
		uint8 kg = G32(Key);
		uint8 kb = B32(Key);
		
		if (pDC->GetBits() < 32 || A32(Key) != 0)
		{
			for (int y=0; y<Y(); y++)
			{
				System32BitPixel *p = (System32BitPixel*) ((*this)[y]);
				System32BitPixel *e = p + X();
				while (p < e)
				{
					if (p->r == kr &&
						p->g == kg &&
						p->b == kb)
					{
						p->a = 0;
						p->b = 0;
						p->g = 0;
						p->r = 0;
					}
					else
					{
						p->a = 255;
					}
					p++;
				}
			}
		}
		
		#endif
	}

	#endif
}

GImageList::~GImageList()
{
	DeleteObj(d);
}

void GImageList::Draw(GSurface *pDest, int Dx, int Dy, int Image, GColour Background, int Flags)
{
	if (pDest)
	{
		GRect r;
		r.ZOff(d->Sx-1, d->Sy-1);
		r.Offset(Image * d->Sx, 0);

		if ((*this)[0])
		{
			COLOUR Key = Get(0, 0);

			if (Flags & IMGLST_DISABLED)
			{
				COLOUR Low = CBit(pDest->GetBits(), LC_LOW);
				COLOUR High = CBit(pDest->GetBits(), LC_LIGHT);

				for (int y=0; y<r.Y(); y++)
				{
					for (int x=0; x<r.X(); x++)
					{
						#ifdef MAC
						COLOUR c = Get(r.x1 + x, r.y1 + y);
						if (A32(c))
						#else
						if (Get(r.x1 + x, r.y1 + y) != Key)
						#endif
						{
							pDest->Colour(Low);
							pDest->Set(Dx + x, Dy + y);
							pDest->Colour(High);
							pDest->Set(Dx + x + 1, Dy + y + 1);
						}
					}
				}
			}
			else
			{
				#if WINNATIVE

				BOOL Status = false;
				
				if (GetBits() <= 24)
				{
					// No source alpha, have to do colour keying manually.
					GMemDC Tmp;
					GSurface *Dest = pDest;
					GRect drc;
					if (pDest->IsScreen() ||
						GetColourSpace() != pDest->GetColourSpace())
					{
						drc.ZOff(r.X()-1, r.Y()-1);
						Tmp.Create(r.X(), r.Y(), GetBits());
						Tmp.Colour(Background);
						Tmp.Rectangle();
						Dest = &Tmp;
					}
					else
					{
						drc.ZOff(r.X()-1, r.Y()-1);
						drc.Offset(Dx, Dy);
						Dest->Colour(Background);
						Dest->Rectangle(&drc);
					}
										
					Status = true;
					LgiAssert(Dest->GetColourSpace() == GetColourSpace());
					switch (GetColourSpace())
					{
						case CsRgb16:
						case CsBgr16:
						{
							for (int y=0; y<r.Y(); y++)
							{
								uint16 *s = ((uint16*)(*this)[r.y1+y]) + r.x1;
								uint16 *d = ((uint16*)(*Dest)[drc.y1+y]) + drc.x1;
								uint16 *e = d + drc.X();
								while (d < e)
								{
									if (*s != Key)
										*d = *s;
									d++;
									s++;
								}
							}
							break;
						}
						case CsRgba32:
						case CsBgra32:
						case CsArgb32:
						case CsAbgr32:
						{
							for (int y=0; y<r.Y(); y++)
							{
								uint32 *s = ((uint32*)(*this)[r.y1+y]) + r.x1;
								uint32 *d = ((uint32*)(*Dest)[drc.y1+y]) + drc.x1;
								uint32 *e = d + drc.X();
								while (d < e)
								{
									if (*s != Key)
										*d = *s;
									d++;
									s++;
								}
							}
							break;
						}
						default:
						{
							Status = false;
							LgiAssert(0);
							break;
						}
					}					

					if (pDest->IsScreen())
					{
						pDest->Blt(Dx, Dy, &Tmp);
					}
				}
				else if (GdcD->AlphaBlend &&
						!LgiApp->IsWine())
				{
					HDC hDest = pDest->StartDC();
					HDC hSrc = StartDC();

					BLENDFUNCTION Blend;
					Blend.BlendOp = AC_SRC_OVER;
					Blend.BlendFlags = 0;
					Blend.AlphaFormat = AC_SRC_ALPHA;

					GApplicator *pApp;
					if (pDest->Op() == GDC_ALPHA &&
						(pApp = pDest->Applicator()) != 0)
					{
						int Alpha = pApp->GetVar(GAPP_ALPHA_A);
						Blend.SourceConstantAlpha = Alpha;
					}
					else
					{
						Blend.SourceConstantAlpha = 255;
					}
					
					Status = GdcD->AlphaBlend(	hDest,
												Dx, Dy,
												r.X(), r.Y(),
												hSrc,
												r.x1, r.y1,
												r.X(), r.Y(),
												Blend);
					if (!Status)
					{
						int ScreenBits = GdcD->GetBits();
						DWORD err = GetLastError();
						printf("%s:%i - AlphaBlend failed with %i (0x%x), Screen depth=%i\n", _FL, err, err, ScreenBits);
					}

					pDest->EndDC();
					EndDC();
				}

				if (!Status)

				#elif defined BEOS

				GScreenDC *pViewDC = dynamic_cast<GScreenDC*>(pDest);
				if (pDest->IsScreen() && pViewDC)
				{
					BView *owner = pViewDC->Handle();
					BBitmap *Bmp = GetBitmap();

					if (owner && Bmp)
					{
						int x = Image * d->Sx;
						BRect S(x, 0, x+d->Sx-1, d->Sy-1);
						BRect D(Dx, Dy, Dx+(d->Sx-1), Dy+(d->Sx-1));

						drawing_mode Mode = owner->DrawingMode();
						owner->SetDrawingMode(B_OP_OVER);
						owner->DrawBitmap(Bmp, S, D);
						owner->SetDrawingMode(Mode);
					}
				}
				else

				#elif defined __GTK_H__

				if (pDest->SupportsAlphaCompositing())
				{
					pDest->Blt(Dx, Dy, this, &r);
				}
				else
				{
					GMemDC Buf(d->Sx, d->Sy, System32BitColourSpace);
					Buf.Colour(Background);
					Buf.Rectangle();
					Buf.Op(GDC_ALPHA);
					Buf.Blt(0, 0, this, &r);					
					pDest->Blt(Dx, Dy, &Buf);
				}

				return;
				
				#elif defined MAC
				
				pDest->Blt(Dx, Dy, this, &r);
				if (0)

				#endif
				{
					COLOUR Map[256];
					int b = GetBits();
					if (b == 8)
					{
						GPalette *Pal = Palette();
						if (Pal)
						{
							for (int i=0; i<Pal->GetSize(); i++)
							{
								Map[i] = CBit(24, i, 8, Pal);
							}
						}

						for (int y=0; y<r.Y(); y++)
						{
							for (int x=0; x<r.X(); x++)
							{
								COLOUR c = Get(r.x1 + x, r.y1 + y);
								if (c != Key)
								{
									pDest->Colour(Map[c], 24);
									pDest->Set(Dx + x, Dy + y);
								}
							}
						}
					}
					else
					{
						for (int y=0; y<r.Y(); y++)
						{
							// True colour
							for (int x=0; x<r.X(); x++)
							{
								COLOUR c = Get(r.x1 + x, r.y1 + y);
								
								if (c != Key)
								{
									pDest->Colour(c, b);
									pDest->Set(Dx + x, Dy + y);
								}
							}
						}
					}
				}
			}
		}
		else
		{
			// Pixmap!
			pDest->Blt(Dx, Dy, this, &r);
		}
	}
}

int GImageList::TileX()
{
	return d->Sx;
}

int GImageList::TileY()
{
	return d->Sy;
}

int GImageList::GetItems()
{
	return X() / d->Sx;
}

void GImageList::Update(int Flags)
{
}

GRect *GImageList::GetBounds()
{
	if (!d->Bounds && (*this)[0])
	{
		d->Bounds = new GRect[GetItems()];
		if (d->Bounds)
		{
			COLOUR Key = Get(0, 0);

			for (int i=0; i<GetItems(); i++)
			{
				d->Bounds[i].x1 = d->Sx-1;
				d->Bounds[i].y1 = d->Sy-1;
				d->Bounds[i].x2 = 0;
				d->Bounds[i].y2 = 0;

				int Bx = i * d->Sx;
				for (int y=0; y<d->Sy; y++)
				{
					for (int x=0; x<d->Sx; x++)
					{
						if (Get(Bx+x, y) != Key)
						{
							d->Bounds[i].x1 = min(d->Bounds[i].x1, x);
							d->Bounds[i].y1 = min(d->Bounds[i].y1, y);
							d->Bounds[i].x2 = max(d->Bounds[i].x2, x);
							d->Bounds[i].y2 = max(d->Bounds[i].y2, y);
						}
					}
				}
			}
		}
	}

	return d->Bounds;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
class GToolBarPrivate
{
public:
	int Bx, By;
	int Sx, Sy;
	bool Vertical;
	bool Text;
	int LastIndex;
	bool OwnImgList;
	GImageList *ImgList;
	GFont *Font;
	GToolTip *Tip;
	
	// Customization menu
	GDom *CustomDom;
	const char *CustomProp;

	// bitmap cache
	GAutoPtr<GMemDC> IconCache;

	GToolBarPrivate()
	{
		Bx = By = 16;
		Sx = Sy = 10;
		Vertical = false;
		Text = false;
		Font = 0;
		Tip = 0;
		CustomProp = 0;
		CustomDom = 0;
	}

	void FixSeparators(GToolBar *Tb)
	{
		// Fix up separators so that no 2 separators are next to each other. I.e.
		// all the buttons between them are switched off.
		GToolButton *Last = 0;
		bool HasVis = false;
		GAutoPtr<GViewIterator> It(Tb->IterateViews());
		for (GViewI *v = It->First(); v; v = It->Next())
		{
			GToolButton *Btn = dynamic_cast<GToolButton*>(v);
			if (Btn)
			{
				if (Btn->Separator())
				{
					Btn->Visible(HasVis);
					if (HasVis)
					{
						Last = Btn;
					}
					HasVis = false;
				}
				else
				{
					HasVis |= Btn->Visible();
				}
			}
		}
		
		if (Last)
		{
			Last->Visible(HasVis);
		}
	}

	void Customizable(GToolBar *Tb)
	{
		GVariant v;
		if (CustomDom)
		{
			CustomDom->GetValue(CustomProp, v);
		}
			
		char *o;
		if ((o = v.Str()))
		{
			GToken t(o, ",");
			if (t.Length() >= 1)
			{
				Text = stricmp(t[0], "text") == 0;
				
				// Make all controls not visible.
				GViewI *v;
				GAutoPtr<GViewIterator> It(Tb->IterateViews());
				for (v = It->First(); v; v = It->Next())
				{
					GToolButton *Btn = dynamic_cast<GToolButton*>(v);
					if (Btn) v->Visible(false);
				}
				
				// Set sub-set of ctrls visible according to saved ID list
				for (int i=1; i<t.Length(); i++)
				{
					int Id = atoi(t[i]);
					if (Id > 0)
						Tb->SetCtrlVisible(Id, true);
				}
				
				FixSeparators(Tb);
			}
		}
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
struct GToolButtonPriv
{
	GArray<GDisplayString*> Text;
};

GToolButton::GToolButton(int Bx, int By)
{
	d = new GToolButtonPriv;
	Type = TBT_PUSH;
	SetId(IDM_NONE);
	SetDown(false);
	Clicked = false;
	Over = false;
	ImgIndex = -1;
	NeedsRightClick = false;

	GRect r(0, 0, Bx+1, By+1);
	SetPos(r);
	SetParent(0);
	TipId = -1;

	_BorderSize = 0;
}

GToolButton::~GToolButton()
{
	d->Text.DeleteObjects();
	delete d;
}

bool GToolButton::Name(const char *n)
{
	bool s = GView::Name(n);

	/*
	char *i = GView::Name();
	char *o = i;
	while (*i)
	{
		if (*i != '&')
			*o++ = *i;
		*i++;
	}
	*o++ = 0;
	*/

	d->Text.DeleteObjects();

	return s;
}

void GToolButton::Layout()
{
	GToolBar *Par = dynamic_cast<GToolBar*>(GetParent());

	// Text
	char *s = Name();
	if (Par->d->Text &&
		Par->d->Font &&
		s)
	{
		// Write each word centered on a different line
		char Buf[256];
		strcpy_s(Buf, sizeof(Buf), s);

		GToken t(Buf, " ");
		if (t.Length() < 3)
		{
			if (t.Length() > 0)
				d->Text.Add(new GDisplayString(Par->d->Font, t[0]));
			if (t.Length() > 1)
				d->Text.Add(new GDisplayString(Par->d->Font, t[1]));
		}
		else if (t.Length() == 3)
		{
			sprintf_s(Buf, sizeof(Buf), "%s %s", t[0], t[1]);
			GDisplayString *d1 = new GDisplayString(Par->d->Font, Buf);
			sprintf_s(Buf, sizeof(Buf), "%s %s", t[1], t[2]);
			GDisplayString *d2 = new GDisplayString(Par->d->Font, Buf);
			if (d1 && d2)
			{
				if (d1->X() < d2->X())
				{
					DeleteObj(d2);
					d->Text.Add(d1);
					d->Text.Add(new GDisplayString(Par->d->Font, t[2]));
				}
				else
				{
					DeleteObj(d1);
					d->Text.Add(new GDisplayString(Par->d->Font, t[0]));
					d->Text.Add(d2);
				}
			}
		}
		else
		{
			//GDisplayString *Cur = new GDisplayString(Par->d->Font, Buf);
		}
	}
}

void GToolButton::OnPaint(GSurface *pDC)
{
	GToolBar *Par = dynamic_cast<GToolBar*>(GetParent());
	bool e = Enabled();

	if (Par)
	{
		GRect p = GetClient();
		
		#if 0 // def _DEBUG
		pDC->Colour(GColour(255, 0, 255));
		pDC->Rectangle();
		#endif

		// Draw Background
		if (GetId() >= 0)
		{
			// Draw border
			COLOUR Background = e && Over ? ToolBarHilightColour : LC_MED;
			if (Down)
			{
				// Sunken if the button is pressed
				LgiThinBorder(pDC, p, DefaultSunkenEdge);
				pDC->Colour(Background, 24);
				pDC->Box(&p);
			}
			else
			{
				pDC->Colour(Background, 24);
				pDC->Box(&p);
				p.Size(1, 1);
			}

			GRect IconPos;
			if (Par->d->ImgList)
				IconPos.Set(0, 0, Par->d->ImgList->TileX()-1, Par->d->ImgList->TileY()-1);
			else
				IconPos.ZOff(-1, -1);
			GRegion Unpainted(p);
			
			// Center the icon
			if (IconPos.X() < p.X() - 1)
				IconPos.Offset((p.X() - IconPos.X()) >> 1, 0);
			// Offset it if the button is pressed
			if (Down)
				IconPos.Offset(1, 1);

			// Draw any icon.
			if (ImgIndex >= 0)
			{
				if (Par->d->ImgList)
				{
					// Draw cached
					Par->_DrawFromCache(pDC, IconPos.x1, IconPos.y1, ImgIndex, Over, !e);
					Unpainted.Subtract(&IconPos);

					// Fill in the rest of the area
					pDC->Colour(Background, 24);
					for (GRect *r = Unpainted.First(); r; r = Unpainted.Next())
					{
						pDC->Rectangle(r);
					}
				}
				else
				{
					// Draw a red cross indicating no icons.
					pDC->Colour(LC_MED, 24);
					pDC->Rectangle(&p);
					pDC->Colour(Rgb24(255, 0, 0), 24);
					pDC->Line(p.x1, p.y1, p.x2, p.y2);
					pDC->Line(p.x2, p.y1, p.x1, p.y2);
				}
			}
			else
			{
				pDC->Colour(LC_MED, 24);
				pDC->Rectangle(&p);
			}
			

			// Text
			if (Par->d->Text &&
				Par->d->Font)
			{
				if (Name() && !d->Text.Length())
				{
					Layout();
				}

				if (d->Text.Length())
				{
					// Write each word centered on a different line
					int Ty = Down + Par->d->By + 2;
					COLOUR a = e ? LC_TEXT : LC_LOW;
					COLOUR b = LC_MED;

					Par->d->Font->Colour(a, b);
					for (int i=0; i<d->Text.Length(); i++)
					{
						GDisplayString *Ds = d->Text[i];
						Ds->Draw(pDC, Down + ((X()-Ds->X())/2), Ty);
						Ty += Ds->Y();
					}
				}
			}
		}
		else
		{
			// Separator
			int Px = X()-1;
			int Py = Y()-1;
			pDC->Colour(LC_MED, 24);
			pDC->Rectangle();

			if (X() > Y())
			{
				int c = Y()/2-1;
				pDC->Colour(Map(pDC, LC_LOW), 24);
				pDC->Line(2, c, Px-2, c);
				pDC->Colour(Map(pDC, LC_LIGHT), 24);
				pDC->Line(2, c+1, Px-2, c+1);
			}
			else
			{
				int c = X()/2-1;
				pDC->Colour(Map(pDC, LC_LOW), 24);
				pDC->Line(c, 2, c, Py-2);
				pDC->Colour(Map(pDC, LC_LIGHT), 24);
				pDC->Line(c+1, 2, c+1, Py-2);
			}
		}
	}
}

void GToolButton::Image(int i)
{
	if (ImgIndex != i)
	{
		ImgIndex = i;
		Invalidate();
	}
}

void GToolButton::Value(int64 b)
{
	switch (Type)
	{
		case TBT_PUSH:
		{
			// do nothing... can't set value
			break;
		}

		case TBT_TOGGLE:
		{
			if (Value() != b)
			{
				SetDown(b);
				Invalidate();
			}
			break;
		}

		case TBT_RADIO:
		{
			if (GetParent() && b)
			{
				// Clear any other radio buttons that are down
				GToolButton *But;
				GAutoPtr<GViewIterator> it(GetParent()->IterateViews());
				if (it)
				{
					for (	But = dynamic_cast<GToolButton*>(it->IndexOf(this)>=0?this:0);
							But && But->GetId() >= 0;
							But = dynamic_cast<GToolButton*>(it->Next()))
					{
						if (But->Type == TBT_RADIO &&
							But != this &&
							But->Down)
						{
							But->SetDown(false);
							But->Invalidate();
						}
					}

					int Idx = it->IndexOf(this);
					for (	But = dynamic_cast<GToolButton*>(Idx>=0?this:0);
							But && But->GetId() >= 0;
							But = dynamic_cast<GToolButton*>(it->Prev()))
					{
						if (But->Type == TBT_RADIO &&
							But != this &&
							But->Down)
						{
							But->SetDown(false);
							But->Invalidate();
						}
					}
				}
			}

			SetDown(b);
			Invalidate();
			break;
		}
	}
}

void GToolButton::OnCommand()
{
	if (GetParent())
	{
		GToolBar *t = dynamic_cast<GToolBar*>(GetParent());
		if (t) t->OnButtonClick(this);
	}
}

void GToolButton::OnMouseClick(GMouse &m)
{
	GToolBar *ToolBar = dynamic_cast<GToolBar*>(GetParent());

	#if 0
	printf("tool button click %i,%i down=%i, left=%i right=%i middle=%i, ctrl=%i alt=%i shift=%i Double=%i\n",
		m.x, m.y,
		m.Down(), m.Left(), m.Right(), m.Middle(),
		m.Ctrl(), m.Alt(), m.Shift(), m.Double());
	#endif

	if (!NeedsRightClick &&
		ToolBar &&
		ToolBar->IsCustomizable() &&
		m.IsContextMenu())
	{
		m.ToScreen();
		ToolBar->ContextMenu(m);
	}
	else
	{
		// left click action...
		if (GetId() >= 0 && Enabled())
		{
			switch (Type)
			{
				case TBT_PUSH:
				{
					bool Old = Down;
	
					Clicked = m.Down();
					Capture(m.Down());
					
					if (Old && IsOver(m))
					{
						char *n = Name();
						if (m.Left())
						{
							OnCommand();
						}
						SendNotify(m.Flags);
					}

					SetDown(m.Down());
					if (Old != Down)
					{
						Invalidate();
					}
					break;
				}

				case TBT_TOGGLE:
				{
					if (m.Down())
					{
						if (m.Left())
						{
							Value(!Down);
							OnCommand();
						}
						SendNotify(m.Flags);
					}
					break;
				}

				case TBT_RADIO:
				{
					if (m.Down())
					{
						if (!Down && m.Left())
						{
							Value(true);
							OnCommand();
						}
						SendNotify(m.Flags);
					}
					break;
				}
			}
		}
	}
}

void GToolButton::OnMouseEnter(GMouse &m)
{
	if (!Separator() && Enabled())
	{
		Over = true;
		Invalidate();
	}

	if (Clicked)
	{
		SetDown(true);
		Invalidate();
	}
	else
	{
		GToolBar *Bar = dynamic_cast<GToolBar*>(GetParent());
		if (Bar)
		{
			Bar->OnMouseEnter(m);
			if (Bar->d->Tip && TipId < 0)
			{
				TipId = Bar->d->Tip->NewTip(Name(), GetPos());
			}
		}

		if (GetParent())
		{
			GToolBar *ToolBar = dynamic_cast<GToolBar*>(GetParent());
			if (ToolBar) ToolBar->PostDescription(this, Name());
		}
	}
}

void GToolButton::SetDown(bool d)
{
	Down = d;
}

void GToolButton::OnMouseMove(GMouse &m)
{
	#ifdef BEOS
	if (GetParent())
	{
		GToolBar *tb = dynamic_cast<GToolBar*>(GetParent());
		if (tb)
			tb->PostDescription(this, Name());
	}
	#endif
}

void GToolButton::OnMouseExit(GMouse &m)
{
	if (Over)
	{
		Over = false;
		Invalidate();
	}

	if (Clicked)
	{
		SetDown(false);
		Invalidate();
	}
	else if (GetParent())
	{
		GToolBar *ToolBar = dynamic_cast<GToolBar*>(GetParent());
		if (ToolBar) ToolBar->PostDescription(this, "");
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
GToolBar::GToolBar()
{
	d = new GToolBarPrivate;
	Name("LGI_Toolbar");
	_BorderSize = 1;
	_IsToolBar = 1;
	Raised(true);

	// Setup tool button font
	GFontType SysFontType;
	if (SysFontType.GetSystemFont("Small"))
	{
		d->Font = SysFontType.Create();
		if (d->Font)
		{
			d->Font->PointSize(min(d->Font->PointSize(), SysFont->PointSize()));
			d->Font->Colour(0);
			d->Font->Bold(false);
			d->Font->Transparent(true);
		}
	}

	d->LastIndex = 0;
	d->OwnImgList = false;
	d->ImgList = 0;

	#if defined BEOS
	Handle()->SetViewColor(B_TRANSPARENT_COLOR);
	#endif
}

GToolBar::~GToolBar()
{
	DeleteObj(d->Tip);

	if (d->OwnImgList)
		DeleteObj(d->ImgList);

	DeleteObj(d->Font);
	DeleteObj(d);
}

void GToolBar::OnCreate()
{
	#ifndef WIN32
	AttachChildren();
	#endif
}

int GToolBar::GetBx()
{
	return d->Bx;
}

int GToolBar::GetBy()
{
	return d->By;
}

void GToolBar::ContextMenu(GMouse &m)
{
	if (IsCustomizable())
	{
		GSubMenu *Sub = new GSubMenu;
		if (Sub)
		{
			int n = 1;
			GViewI *v;
			for (v = Children.First(); v; v = Children.Next(), n++)
			{
				GToolButton *Btn = dynamic_cast<GToolButton*>(v);
				if (Btn && Btn->Separator())
				{
					Sub->AppendSeparator();
				}
				else
				{
					GMenuItem *Item = Sub->AppendItem(v->Name(), n, true);
					if (Item)
					{
						Item->Checked(v->Visible());
					}
				}
			}
			Sub->AppendSeparator();
			GMenuItem *Txt = Sub->AppendItem(LgiLoadString(L_TOOLBAR_SHOW_TEXT, "Show Text Labels"), 1000, true);
			Txt->Checked(d->Text);

			bool Save = false;
			int Pick = Sub->Float(this, m.x, m.y);
			switch (Pick)
			{
				case 1000:
				{
					d->Text = !d->Text;
					Save = true;
					break;
				}
				default:
				{
					GViewI *Ctrl = Children[Pick - 1];
					if (Ctrl)
					{
						Ctrl->Visible(!Ctrl->Visible());
						Save = true;
					}
					break;
				}
			}
			
			DeleteObj(Sub);
			
			if (Save)
			{
				GStringPipe p(256);
				p.Push((char*) (d->Text ? "text" : "no"));
				for (v = Children.First(); v; v = Children.Next())
				{
					if (v->Visible())
					{
						p.Print(",%i", v->GetId());
					}
				}
				char *o = p.NewStr();
				if (o)
				{
					if (d->CustomDom)
					{
						GVariant v(o);
						d->CustomDom->SetValue(d->CustomProp, v);
					}
					DeleteArray(o);
				}
				
				d->FixSeparators(this);

				for (GViewI *v = Children.First(); v; v = Children.Next())
				{
					GToolButton *b = dynamic_cast<GToolButton*>(v);
					if (b && b->TipId >= 0)
					{
						d->Tip->DeleteTip(b->TipId);
						b->TipId = -1;
					}
				}

				GetWindow()->Pour();
			}
		}
	}
}

bool GToolBar::IsCustomizable()
{
	return d->CustomDom != 0 && d->CustomProp;
}

void GToolBar::Customizable(GDom *Store, const char *Option)
{
	d->CustomDom = Store;
	d->CustomProp = Option;
	d->Customizable(this);
}

bool GToolBar::IsVertical()
{
	return d->Vertical;
}

void GToolBar::IsVertical(bool v)
{
	d->Vertical = v;
}

bool GToolBar::TextLabels()
{
	return d->Text;
}

void GToolBar::TextLabels(bool i)
{
	d->Text = i;
}

GFont *GToolBar::GetFont()
{
	return d->Font;
}

void GToolBar::_BuildCache(GImageList *From)
{
	if (!From)
	{
		LgiAssert(0);
		return;
	}

	d->IconCache.Reset(new GMemDC);
	if (!d->IconCache->Create(From->X(), From->Y() * 3, 32))
	{
		LgiAssert(!"Can't create bitmap.");
		return;
	}

	GColour Background(LC_MED, 24), HilightColour(ToolBarHilightColour, 24);
	d->IconCache->Colour(Background);
	d->IconCache->Rectangle();
	d->IconCache->Colour(HilightColour);
	d->IconCache->Rectangle(0, IconHilight * From->Y(), d->IconCache->X()-1, (IconHilight + 1) * From->Y() - 1);

	d->IconCache->Op(GDC_ALPHA);
	GApplicator *pApp = d->IconCache->Applicator();
	for (int i=0; i<From->GetItems(); i++)
	{
		if (pApp)
			pApp->SetVar(GAPP_ALPHA_A, 255);
		From->Draw(d->IconCache, i * From->TileX(), 0, i, Background);
		From->Draw(d->IconCache, i * From->TileX(), From->TileY(), i, HilightColour);

		if (pApp)
			pApp->SetVar(GAPP_ALPHA_A, 40);
		From->Draw(d->IconCache, i * From->TileX(), From->TileY() * 2, i, Background);
	}
}

void GToolBar::_DrawFromCache(GSurface *pDC, int x, int y, int Index, bool Hilight, bool Disabled)
{
	if (pDC && d->IconCache)
	{
		GRect s;
		int YOffset =	(Hilight && !Disabled ? 1 : 0)
						+
						(Disabled ? 2 : 0);
		
		s.ZOff(d->ImgList->TileX()-1, d->ImgList->TileY()-1);
		s.Offset
		(
			d->ImgList->TileX() * Index,
			d->ImgList->TileY() * YOffset
		);

		// printf("DrawFromCache %i, hi=%i, dis=%i, off=%i, s=%s\n", Index, Hilight, Disabled, YOffset, s.GetStr());
		pDC->Blt(x, y, d->IconCache, &s);
	}
}

bool GToolBar::Pour(GRegion &r)
{
	int PosX = BORDER_SPACER;
	int PosY = BORDER_SPACER;
	int SrcX = 0;
	int SrcY = 0;
	int EndX = 0;
	int EndY = 0;
	int MaxDim = 0;

	GRect ButPos;
	GViewI *But = Children.First();
	while (But)
	{
		if (But->Visible())
		{
			int Tx = 0, Ty = 0;

			if (d->Text)
			{
				GToolButton *Btn = dynamic_cast<GToolButton*>(But);
				if (Btn)
				{
					if (Btn->d->Text.Length() == 0)
					{
						Btn->Layout();
					}
					
					for (int i=0; i<Btn->d->Text.Length(); i++)
					{
						GDisplayString *Ds = Btn->d->Text[i];
						Tx = max(Ds->X() + 4, Tx);
						Ty += Ds->Y();
					}
				}
			}

			ButPos = But->GetPos();
			GToolButton *Button = dynamic_cast<GToolButton*>(But);
			if (Button)
			{
				if (Button->Separator())
				{
					// This will be stretched out later by the code that makes
					// everything the same height.
					ButPos.ZOff(BORDER_SEPARATOR+1, BORDER_SEPARATOR+1);
				}
				else
				{
					if (Button->Image() >= 0)
					{
						// Set initial size to the icon size
						ButPos.ZOff(d->Bx + 2, d->By + 2);
					}
					else
					{
						// Otherwise default to text size
						ButPos.ZOff(7, 0);
					}
					
					Tx += 4;
					if (ButPos.X() < Tx)
					{
						// Make button wider for text label
						ButPos.x2 = Tx - 1;
					}
					
					ButPos.y2 += Ty;
				}
			}
			
			if (d->Vertical)
			{
				MaxDim = max(MaxDim, ButPos.X());
			}
			else
			{
				MaxDim = max(MaxDim, ButPos.Y());
			}

			ButPos.Offset(PosX - ButPos.x1, PosY - ButPos.y1);

			if (But->GetId() == IDM_BREAK)
			{
				ButPos.ZOff(0, 0);
				if (d->Vertical)
				{
					PosX = MaxDim;
					PosY = BORDER_SHADE + BORDER_SPACER;
				}
				else
				{
					PosX = BORDER_SHADE + BORDER_SPACER;
					PosY = MaxDim;
				}
			}
			else
			{
				if (d->Vertical)
				{
					PosY = ButPos.y2 + 1;
				}
				else
				{
					PosX = ButPos.x2 + 1;
				}
			}

			But->SetPos(ButPos);
		}
		else
		{
			GRect p(0, 0, 0, 0);
			But->SetPos(p);
		}
		
		But = Children.Next();
	}

	for (GViewI *w = Children.First(); w; w = Children.Next())
	{
		GRect p = w->GetPos();

		if (d->Vertical)
		{
			if (w->X() < MaxDim)
			{
				p.x2 = p.x1 + MaxDim - 1;
				w->SetPos(p);
			}
		}
		else
		{
			if (w->Y() < MaxDim)
			{
				p.y2 = p.y1 + MaxDim - 1;
				w->SetPos(p);
			}
		}

		EndX = max(EndX, p.x2);
		EndY = max(EndY, p.y2);
	}

	d->Sx = EndX + BORDER_SPACER;
	d->Sy = EndY + BORDER_SPACER;

	int BorderPx = Raised() || Sunken() ? _BorderSize<<1 : 0;

	GRect n;
	n.ZOff(max(7, d->Sx)+BorderPx, max(7, d->Sy)+BorderPx);

	GRect *Best = FindLargestEdge(r, GV_EDGE_TOP);
	if (Best)
	{
		n.Offset(Best->x1, Best->y1);
		n.Bound(Best);
		SetPos(n, true);
		return true;
	}

	return false;
}

void GToolBar::OnButtonClick(GToolButton *Btn)
{
	GViewI *w = (GetNotify()) ? GetNotify() : GetParent();
	if (w && Btn)
	{
		int Id = Btn->GetId();
        w->PostEvent(M_COMMAND, Id, (GMessage::Param) Handle());
	}
}

int GToolBar::PostDescription(GView *Ctrl, const char *Text)
{
	if (GetParent())
	{
        return GetParent()->PostEvent(M_DESCRIBE, (GMessage::Param) Ctrl, (GMessage::Param) Text);
	}
	return 0;
}

GMessage::Result GToolBar::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case M_CHANGE:
		{
			if (GetParent())
				return GetParent()->OnEvent(Msg);
			break;
		}
	}

	return GView::OnEvent(Msg);
}

void GToolBar::OnPaint(GSurface *pDC)
{
	GRect r = GetClient();

	pDC->Colour(LC_MED, 24);
	pDC->Box(&r);
}

void GToolBar::OnMouseClick(GMouse &m)
{
}

void GToolBar::OnMouseEnter(GMouse &m)
{
	if (!d->Tip)
	{
		d->Tip = new GToolTip;
		if (d->Tip)
		{
			d->Tip->Attach(this);
		}
	}
}

void GToolBar::OnMouseExit(GMouse &m)
{
}

void GToolBar::OnMouseMove(GMouse &m)
{
}

bool GToolBar::SetBitmap(char *File, int bx, int by)
{
	bool Status = false;

	GSurface *pDC = LoadDC(File);
	if (pDC)
	{
		Status = SetDC(pDC, bx, by);
		DeleteObj(pDC);
	}

	return Status;
}

bool GToolBar::SetDC(GSurface *pNewDC, int bx, int by)
{
	if (d->OwnImgList)
	{
		DeleteObj(d->ImgList);
	}

	d->Bx = bx;
	d->By = by;

	if (pNewDC)
	{
		d->ImgList = new GImageList(bx, by, pNewDC);
		if (d->ImgList)
		{
			d->OwnImgList = true;
			_BuildCache(d->ImgList);
			return true;
		}
	}
	return false;
}

GImageList *GToolBar::GetImageList()
{
	return d->ImgList;
}

bool GToolBar::SetImageList(GImageList *l, int bx, int by, bool Own)
{
	if (d->OwnImgList)
	{
		DeleteObj(d->ImgList);
	}
	d->OwnImgList = Own;
	d->Bx = bx;
	d->By = by;

	d->ImgList = l;
	_BuildCache(d->ImgList);

	return d->ImgList != 0;
}

GToolButton *GToolBar::AppendButton(const char *Tip, int Id, int Type, int Enabled, int IconId)
{
	bool HasIcon = IconId != TOOL_ICO_NONE;

	GToolButton *But = new GToolButton(d->Bx, d->By);
	if (But)
	{
		But->Name(Tip);
		But->SetId(Id);
		But->Type = Type;
		But->SetParent(this);
		But->Enabled(Enabled);

		if (IconId >= 0)
		{
			But->ImgIndex = IconId;
		}
		else if (IconId == TOOL_ICO_NEXT)
		{
			But->ImgIndex = d->LastIndex++;
		}
		else if (IconId == TOOL_ICO_NONE)
		{
			But->ImgIndex = -1;
		}

		AttachButton(But);
	}

	return But;
}

bool GToolBar::AppendSeparator()
{
	GToolButton *But = new GToolButton(d->Bx, d->By);
	if (But)
	{
		But->SetId(IDM_SEPARATOR);
		But->SetParent(this);
		AttachButton(But);
		return true;
	}
	return false;
}

bool GToolBar::AppendBreak()
{
	GToolButton *But = new GToolButton(d->Bx, d->By);
	if (But)
	{
		But->SetId(IDM_BREAK);
		But->SetParent(this);
		AttachButton(But);
		return true;
	}
	return false;
}

bool GToolBar::AppendControl(GView *Ctrl)
{
	bool Status = false;
	if (Ctrl)
	{
		Ctrl->SetParent(this);
		AttachButton(Ctrl);
		Status = true;
	}
	return Status;
}

void GToolBar::Empty()
{
	for (GViewI *But = Children.First(); But; But = Children.Next())
	{
		DeleteObj(But);
	}
}

#ifdef MAC
bool GToolBar::Attach(GViewI *parent)
{
	return GLayout::Attach(parent);
}
#endif

///////////////////////////////////////////////////////////////////////
COLOUR Map(GSurface *pDC, COLOUR c)
{
	if (pDC && pDC->GetBits() <= 8)
	{
		if (pDC->IsScreen())
		{
			c = CBit(24, c);
		}
		#ifdef WIN32
		else
		{
			HPALETTE hPal = GetSystemPalette();
			if (hPal)
			{
				c = GetNearestPaletteIndex(hPal, c);
				DeleteObject(hPal);
			}
		}
		#endif
	}
	return c;
}

#ifdef WIN32
HPALETTE GetSystemPalette()
{
	HPALETTE hPal = 0;
	LOGPALETTE *Log = (LOGPALETTE*) new uchar[sizeof(LOGPALETTE) + (sizeof(PALETTEENTRY) * 255)];
	if (Log)
	{
		Log->palVersion = 0x300;
		Log->palNumEntries = 256;

		HDC hDC = CreateCompatibleDC(0);
		GetSystemPaletteEntries(hDC, 0, 256, Log->palPalEntry);
		DeleteDC(hDC);

		hPal = CreatePalette(Log);
	}

	return hPal;
}

bool BltBmpToBmp(HBITMAP hDest,
		int xDst,
		int yDst,
		int cx,
		int cy,
		HBITMAP hSrc,
		int xSrc,
		int ySrc,
		DWORD dwRop)
{
	bool Status = false;
	HDC DestDC = CreateCompatibleDC(0);
	HDC SrcDC = CreateCompatibleDC(0);

	if (DestDC && SrcDC)
	{
		hDest = (HBITMAP) SelectObject(DestDC, hDest);
		hSrc = (HBITMAP) SelectObject(SrcDC, hSrc);

		Status = BitBlt(DestDC,
				xDst,
				yDst,
				cx,
				cy,
				SrcDC,
				xSrc,
				ySrc,
				dwRop);

		hDest = (HBITMAP) SelectObject(DestDC, hDest);
		hSrc = (HBITMAP) SelectObject(SrcDC, hSrc);
	}

	if (DestDC)
	{
		DeleteDC(DestDC);
	}
	if (SrcDC)
	{
		DeleteDC(SrcDC);
	}

	return Status;
}

bool BltBmpToDc(HDC DestDC,
		int xDst,
		int yDst,
		int cx,
		int cy,
		HBITMAP hSrc,
		int xSrc,
		int ySrc,
		DWORD dwRop)
{
	bool Status = false;
	HDC SrcDC = CreateCompatibleDC(0);

	if (DestDC && SrcDC)
	{
		hSrc = (HBITMAP) SelectObject(SrcDC, hSrc);

		Status = BitBlt(DestDC,
				xDst,
				yDst,
				cx,
				cy,
				SrcDC,
				xSrc,
				ySrc,
				dwRop);

		hSrc = (HBITMAP) SelectObject(SrcDC, hSrc);
	}

	if (SrcDC)
	{
		DeleteDC(SrcDC);
	}

	return Status;
}

bool BltDcToBmp(HBITMAP hDest,
		int xDst,
		int yDst,
		int cx,
		int cy,
		HDC SrcDC,
		int xSrc,
		int ySrc,
		DWORD dwRop)
{
	bool Status = false;
	HDC DestDC = CreateCompatibleDC(0);

	if (DestDC && SrcDC)
	{
		hDest = (HBITMAP) SelectObject(DestDC, hDest);

		Status = BitBlt(DestDC,
				xDst,
				yDst,
				cx,
				cy,
				SrcDC,
				xSrc,
				ySrc,
				dwRop);

		hDest = (HBITMAP) SelectObject(DestDC, hDest);
	}

	if (DestDC)
	{
		DeleteDC(DestDC);
	}

	return Status;
}
#endif
