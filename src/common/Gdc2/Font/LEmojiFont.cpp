#include "Lgi.h"
#include "LEmojiFont.h"
#include "Emoji.h"
#include "GFontPriv.h"

#define FILE_NAME "EmojiMap.png"

struct LEmojiFontPriv
{
	GAutoString Fn;
	GAutoPtr<GSurface> Img;
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
	ssize_t Len = len;

	x = 0;
	y = EMOJI_CELL_SIZE;
	for (auto s = (const uint16*)Str; *s && Len > 0; )
	{
		uint32 u32 = LgiUtf16To32(s, Len);
		if (u32)
			x += EMOJI_CELL_SIZE;
	}
}

int LEmojiFont::_CharAt(int x, OsChar *Str, int Len, LgiPxToIndexType Type)
{
	return -1;
}

void LEmojiFont::_Draw(GSurface *pDC, int x, int y, OsChar *Str, int len, GRect *r, GColour &fore)
{
	if (!priv->Img)
		return;

	ssize_t Bytes = len * sizeof(*Str);

	pDC->Op(GDC_ALPHA);

	for (auto s = (const uint16*)Str; *s && Bytes > 0; )
	{
		uint32 u32 = LgiUtf16To32(s, Bytes);
		if (!u32)
			break;

		auto Idx = EmojiToIconIndex(&u32, 1);
		if (Idx >= 0)
		{
			int Cx = Idx % EMOJI_GROUP_X;
			int Cy = Idx / EMOJI_GROUP_X;

			GRect r(0, 0, EMOJI_CELL_SIZE-1, EMOJI_CELL_SIZE-1);
			r.Offset(Cx * EMOJI_CELL_SIZE, Cy * EMOJI_CELL_SIZE);

			pDC->Blt(x, y, priv->Img, &r);

			x += EMOJI_CELL_SIZE;
		}
		else LgiAssert(!"Invalid char");
	}
}

int LEmojiFont::GetHeight()
{
	return EMOJI_CELL_SIZE;
}

bool LEmojiFont::Create(const char *Face, int PtSize, GSurface *pSurface)
{
	if (!priv->Fn)
	{
		priv->Fn.Reset(LgiFindFile(FILE_NAME));
		if (!priv->Fn)
		{
			GFile::Path p(LSP_APP_INSTALL);
			p--;
			p += "Lgi\\trunk\\src\\common\\Text\\Emoji\\EmojiMap.png";
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
				if (EmojiToIconIndex(&u, 1))
					d->GlyphMap[u>>3] |= 1 << (u & 7);
			}
			for (uint32 u=0x1f004; u<=0x1f6c5; u++)
			{
				if (EmojiToIconIndex(&u, 1))
					d->GlyphMap[u>>3] |= 1 << (u & 7);
			}
		}
	}

	return priv->Img != NULL;
}
