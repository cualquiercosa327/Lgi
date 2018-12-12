#include "Lgi.h"
#include "LEmojiFont.h"
#include "Emoji.h"
#include "GFontPriv.h"
#include "GdcTools.h"

#define FILE_NAME "EmojiMap.png"
// 200,960-219,979

struct LEmojiFontPriv
{
	GAutoString Fn;
	GAutoPtr<GSurface> Img, Scaled;
	GArray<bool> Resampled;
	int Cell;

	LEmojiFontPriv()
	{
		Cell = EMOJI_CELL_SIZE;
	}
};

LEmojiFont::LEmojiFont()
{
	priv = new LEmojiFontPriv;
}

LEmojiFont::~LEmojiFont()
{
	delete priv;
}

void LEmojiFont::_Measure(int &x, int &y, OsChar *Str, int len)
{
	ssize_t Len = len * sizeof(*Str);

	x = 0;
	y = priv->Cell;
	for (auto s = (const uint16*)Str; *s && Len > 0; )
	{
		uint32 u32 = LgiUtf16To32(s, Len);
		if (u32)
			x += priv->Cell;
	}
}

int LEmojiFont::_CharAt(int xPos, OsChar *Str, int len, LgiPxToIndexType Type)
{
	ssize_t Len = len * sizeof(*Str);

	int x = 0;
	int Char = 0;
	for (auto s = (const uint16*)Str; *s && Len > 0; )
	{
		uint32 u32 = LgiUtf16To32(s, Len);
		if (u32)
		{
			if (xPos >= x && xPos < x + priv->Cell)
				return Char;
			x += priv->Cell;
			Char++;
		}
		else break;
	}

	return -1;
}

void LEmojiFont::_Draw(GSurface *pDC, int x, int y, OsChar *Str, int len, GRect *r, GColour &fore)
{
	if (!priv->Img)
		return;

	ssize_t Bytes = len * sizeof(*Str);

	pDC->Op(GDC_ALPHA);
	pDC->Colour(Back());

	for (auto s = (const uint16*)Str; *s && Bytes > 0; )
	{
		uint32 u32 = LgiUtf16To32(s, Bytes);
		if (!u32)
			break;

		GSurface *Src = priv->Scaled ? priv->Scaled : priv->Img;

		auto Idx = EmojiToIconIndex(&u32, 1);
		if (Idx >= 0)
		{
			int Cx = Idx % EMOJI_GROUP_X;
			int Cy = Idx / EMOJI_GROUP_X;

			GRect Icon(0, 0, priv->Cell-1, priv->Cell-1);
			Icon.Offset(Cx * priv->Cell, Cy * priv->Cell);

			if (!Transparent())
			{
				if (r)
					pDC->Rectangle(r);
				else
					pDC->Rectangle(x, y, x + priv->Cell - 1, y + priv->Cell - 1);
			}

			if (priv->Scaled && !priv->Resampled[Idx])
			{
				priv->Resampled[Idx] = true;			
				GAutoPtr<GSurface> s(priv->Scaled->SubImage(Icon));
				GRect Src(0, 0, EMOJI_CELL_SIZE-1, EMOJI_CELL_SIZE-1);
				Src.Offset(Cx * EMOJI_CELL_SIZE, Cy * EMOJI_CELL_SIZE);
				ResampleDC(s, priv->Img.Get(), &Src);
			}

			pDC->Blt(x, y, Src, &Icon);

			x += priv->Cell;
		}
		else LgiAssert(!"Invalid char");
	}
}

int LEmojiFont::GetHeight()
{
	return priv->Cell;
}

bool LEmojiFont::Create(const char *Face, GCss::Len Sz, GSurface *pSurface)
{
	if (Sz.IsValid())
		Size(Sz);

	if (!priv->Fn)
	{
		priv->Fn.Reset(LgiFindFile(FILE_NAME));
		if (!priv->Fn)
		{
			GFile::Path p(LSP_APP_INSTALL);
			p += "..\\Lgi\\trunk\\src\\common\\Text\\Emoji\\EmojiMap.png";
			if (p.Exists())
				priv->Fn.Reset(NewStr(p));
		}
	}

	if (priv->Fn && !priv->Img)
	{
		if (priv->Img.Reset(LoadDC(priv->Fn)))
		{
			LgiAssert(priv->Img->GetBits() == 32);
		}
		else return false;
	}

	if (!d->GlyphMap)
	{
		uint Bytes = (MAX_UNICODE + 1) >> 3;
		d->GlyphMap = new uchar[Bytes];
		if (d->GlyphMap)
		{
			memset(d->GlyphMap, 0, Bytes);
			
			for (uint32 u=0x203c; u<=0x3299; u++)
			{
				if (EmojiToIconIndex(&u, 1) >= 0)
					d->GlyphMap[u>>3] |= 1 << (u & 7);
			}
			for (uint32 u=0x1f004; u<=0x1f6c5; u++)
			{
				if (EmojiToIconIndex(&u, 1) >= 0)
					d->GlyphMap[u>>3] |= 1 << (u & 7);
			}
		}
	}

	auto Dpi = LgiScreenDpi();
	Sz = Size();
	if (Sz.IsValid() && priv->Img)
	{
		int NewCell = (int) (Sz.Value * Dpi / 50);
		int Cx = priv->Img->X() / EMOJI_CELL_SIZE;
		int Cy = priv->Img->Y() / EMOJI_CELL_SIZE;
		int Nx = Cx * NewCell;
		int Ny = Cy * NewCell;

		if
		(
			NewCell != EMOJI_CELL_SIZE &&
			!priv->Scaled &&
			priv->Scaled.Reset(new GMemDC(Nx, Ny, priv->Img->GetColourSpace()))
		)
		{
			priv->Cell = NewCell;
		}
	}

	return priv->Img != NULL;
}
