/*

Notes:
	The scroll bar position is always in document co-ordinates. Not zoomed co-ords.


*/

#include <math.h>
#include "Lgi.h"
#include "GZoomView.h"
#include "GScrollBar.h"
#include "GThreadEvent.h"
#include "GPalette.h"

#define MAX_FACTOR		32

enum Messages
{
	M_RENDER_FINISHED = M_USER + 800,
};

static float ZoomToScale(int Zoom)
{
	if (Zoom < 0)
		return 1.0f / (1 - Zoom);
	else if (Zoom > 1)
		return (float) (Zoom + 1);
	return 1.0f;
}

struct ZoomTile : public GMemDC
{
	bool Dirty;
	
	GColourSpace MapBits(int Bits)
	{
		if (Bits > 32)
			Bits >>= 1;
		
		if (Bits <= 8)
			return System32BitColourSpace;
		
		return GBitsToColourSpace(Bits);
	}
	
	ZoomTile(int Size, int Bits) : GMemDC(Size, Size, MapBits(Bits))
	{
		Dirty = true;
	}
};

typedef ZoomTile *SuperTilePtr;

class GZoomViewPriv // : public GThread, public GMutex
{
	int Zoom;
	GAutoPtr<GMemDC> TileCache;

public:
	/// If this is true, then we own the pDC object.
	bool OwnDC;
	
	/// The image surface we are displaying
	GSurface *pDC;

	/// The parent view that owns us
	GZoomView *View;
	
	/// The callback object...
	GZoomViewCallback *Callback;

	/// This is the zoom factor. However:
	///     -3 = 1/4 (scale down)
	///     -2 = 1/3
	///     -1 = 1/2
	///     0  = exact pixels
	///     1  = 2x
	///     2  = 3x
	///     3  = 4x (scale up)
	float Scale() { return ZoomToScale(Zoom); }
	int Factor() { return Zoom < 0 ? 1 - Zoom : Zoom + 1; }

	/// Number of tiles allocated
	int TotalTiles;
	
	/// Width and height of the tile...
	int TileSize;
	
	/// Size of image in tiles
	GdcPt2 Tiles;
	
	/// The array of tiles. Unused entries will be NULL.
	ZoomTile ***Tile;

	// Type of sampling to use
	GZoomView::SampleMode SampleMode;
	
	// Default zooming behaviour
	GZoomView::DefaultZoomMode DefaultZoom;

	// Threading stuff
	enum WorkerMode
	{
		ExitNow,
		Waiting,
		Active,
		Stopping,
		Stopped,
	} Mode;

	GZoomViewPriv(GZoomView *view, GZoomViewCallback *callback)
	{
		View = view;
		Callback = callback;
		Mode = Waiting;
		Zoom = 0;
		TileSize = 128;
		
		Tiles.x = Tiles.y = 0;
		Tile = NULL;
		
		OwnDC = false;
		pDC = NULL;
		
		SampleMode = GZoomView::SampleNearest;
		DefaultZoom = GZoomView::ZoomFitBothAxis;
	}
	
	~GZoomViewPriv()
	{
		if (OwnDC)
			DeleteObj(pDC);
			
		EmptyTiles();
	}

	GRect DocToScreen(GRect s)
	{
		int f = Factor();
		if (Zoom < 0)
		{
			// Scaling down (zoomed out)
			s.x1 /= f;
			s.y1 /= f;
			s.x2 = (s.x2 + f - 1) / f;
			s.y2 = (s.y2 + f - 1) / f;
		}
		else if (Zoom > 0)
		{
			// Scaling up (zooming in)
			s.x1 *= f;
			s.y1 *= f;
			s.x2 = ((s.x2 + 1) * f) - 1;
			s.y2 = ((s.y2 + 1) * f) - 1;
		}
		
		return s;
	}

	GRect ScreenToDoc(GRect s)
	{
		int f = Factor();
		if (Zoom > 0)
		{
			// Scaling down (zoomed out)
			s.x1 /= f;
			s.y1 /= f;
			s.x2 = (s.x2 + f - 1) / f;
			s.y2 = (s.y2 + f - 1) / f;
		}
		else if (Zoom < 0)
		{
			// Scaling up (zooming in)
			s.x1 *= f;
			s.y1 *= f;
			s.x2 *= f;
			s.y2 *= f;
		}
		return s;
	}
	
	GdcPt2 ScreenToDoc(GdcPt2 p)
	{
		int f = Factor();
		if (GetZoom() > 0)
		{
			// Image is scaled up, so divide to get doc coords
			p.x = (p.x + f - 1) / f;
			p.y = (p.y + f - 1) / f;
		}
		else if (GetZoom() < 0)
		{
			// Image is scaled down
			p.x *= f;
			p.y *= f;
		}
		return p;
	}

	GdcPt2 DocToScreen(GdcPt2 p)
	{
		int f = Factor();
		if (GetZoom() < 0)
		{
			// Scaled down
			p.x = (p.x + f - 1) / f;
			p.y = (p.y + f - 1) / f;
		}
		else if (GetZoom() > 0)
		{
			// Scaled up
			p.x *= f;
			p.y *= f;
		}
		return p;
	}

	int GetZoom()
	{
		return Zoom;
	}

	void SetZoom(int z)
	{
		if (Zoom != z)
		{
			Zoom = z;
			EmptyTiles();
		}
	}
	
	void SetDefaultZoom()
	{
		if (pDC)
		{
			GRect c = View->GetClient();
			int z;

			bool Loop = true;
			for (z=-20; Loop && z<=0; z++)
			{
				float s = ZoomToScale(z);
				int x = (int) (s * pDC->X());
				int y = (int) (s * pDC->Y());
				
				switch (DefaultZoom)
				{
					case GZoomView::ZoomFitBothAxis:
					{
						if (x >= c.X() || y >= c.Y())
						{
							z--;
							Loop = false;
						}
						break;
					}
					case GZoomView::ZoomFitX:
					{
						if (x >= c.X())
						{
							z--;
							Loop = false;
						}
						break;
					}
					case GZoomView::ZoomFitY:
					{
						if (y >= c.Y())
						{
							z--;
							Loop = false;
						}
						break;
					}
				}
			}
			
			SetZoom(min(z, 0));
			ResetTiles();
		}
	}
	
	void EmptyTiles()
	{
		if (Tile)
		{
			for (int x=0; x<Tiles.x; x++)
			{
				for (int y=0; y<Tiles.y; y++)
				{
					delete Tile[x][y];
				}
				delete [] Tile[x];
			}
			delete [] Tile;
			Tile = NULL;
		}
	}

	void ResetTiles()
	{
		EmptyTiles();
		
		if (pDC)
		{
			GRect full = DocToScreen(pDC->Bounds());
			
			// Pick a suitable tile size
			TileSize = 128;
			if (Zoom >= 2)
			{
				// When scaling up the tile size needs to be
				// an even multiple of the factor. Otherwise
				// we have part of a scaled pixel in one tile
				// and part in another. Which is messy to handle.
				while (TileSize % Factor())
					TileSize++;
			}
			
			Tiles.x = (full.X() + TileSize - 1) / TileSize;
			Tiles.y = (full.Y() + TileSize - 1) / TileSize;
			Tile = new SuperTilePtr*[Tiles.x];
			for (int x=0; x<Tiles.x; x++)
			{
				Tile[x] = new SuperTilePtr[Tiles.y];
				memset(Tile[x], 0, sizeof(SuperTilePtr) * Tiles.y);
			}
		}
	}

	union Pal32
	{
		uint32 u32;
		System32BitPixel p32;
	};
	
	template <typename OutPx, typename InPx>
	void ScaleDown24(OutPx *out, InPx *in, int len, int Factor)
	{
		if (SampleMode == GZoomView::SampleNearest)
		{
			InPx *end = in + len;

			while (in < end)
			{
				out->r = in->r;
				out->g = in->g;
				out->b = in->b;
				in += Factor;
				out++;
			}
		}
		/*
		else if (SampleMode == GZoomView::SampleAverage)
		{
			System24BitPixel *src[MAX_FACTOR];
			for (int i=0; i<Factor; i++)
			{
				src[i] = ((System24BitPixel*) (*Src)[yy + i]) + Rgn.x1;
			}
			
			int Fsq = Factor * Factor;
			while (dst < end)
			{
				int r = 0, g = 0, b = 0;
				
				for (int f=0; f<Factor; f++)
				{
					System24BitPixel *e = src[f] + Factor;
					while (src[f] < e)
					{
						r += src[f]->r;
						g += src[f]->g;
						b += src[f]->b;
						src[f]++;
					}
				}
				
				dst->r = r / Fsq;
				dst->g = g / Fsq;
				dst->b = b / Fsq;
				
				dst++;
			}
		
		}
		*/
	}

	template <typename OutPx, typename InPx>
	void ScaleDown24To32(OutPx *out, InPx *in, int len, int Factor)
	{
		if (SampleMode == GZoomView::SampleNearest)
		{
			InPx *end = in + len;

			while (in < end)
			{
				out->r = in->r;
				out->g = in->g;
				out->b = in->b;
				out->a = 0xff;
				in += Factor;
				out++;
			}
		}
	}

	template <typename InPx>
	void ScaleDown24(GColourSpace outCs, void *out, InPx *in, int len, int Factor)
	{
		switch (outCs)
		{
			#define DownCase(type) \
				case Cs##type: \
					ScaleDown24((G##type*)out, in, len, Factor); \
					break;
			DownCase(Rgb24)
			DownCase(Bgr24)
			DownCase(Rgbx32)
			DownCase(Bgrx32)
			DownCase(Xrgb32)
			DownCase(Xbgr32)
			#undef DownCase

			#define DownCase(type) \
				case Cs##type: \
					ScaleDown24To32((G##type*)out, in, len, Factor); \
					break;
			DownCase(Rgba32)
			DownCase(Bgra32)
			DownCase(Argb32)
			DownCase(Abgr32)
			#undef DownCase
			
			default:
				LgiAssert(0);
				break;
		}
	}
	
	void ScaleDown(GSurface *Dst, GSurface *Src, int Sx, int Sy, int Factor)
	{
		// ImageViewTarget *App = View->GetApp();
		uchar *DivLut = Div255Lut;

		Pal32 pal[256];
		if (Src->GetColourSpace() == CsIndex8)
		{
			GPalette *inPal = Src->Palette();
			for (int i=0; i<256; i++)
			{
				GdcRGB *rgb = inPal ? (*inPal)[i] : NULL;
				if (rgb)
				{
					pal[i].p32.r = rgb->r;
					pal[i].p32.g = rgb->g;
					pal[i].p32.b = rgb->b;
				}
				else
				{
					pal[i].p32.r = i;
					pal[i].p32.g = i;
					pal[i].p32.b = i;
				}

				pal[i].p32.a = 255;
			}

			#if 0 // def _DEBUG
			Dst->Colour(GColour(255, 0, 255));
			Dst->Rectangle();
			#endif
		}

		// Now copy the right pixels over using the selected sampling method...
		for (int y=0; y<Dst->Y(); y++)
		{
			int yy = Sy + (y * Factor);
			if (yy >= Src->Y())
				continue;

			int Ex = Sx + (TileSize * Factor);
			if (Ex >= Src->X())
				Ex = Src->X();

			switch (Src->GetColourSpace())
			{
				case CsIndex8:
				{
					if (SampleMode == GZoomView::SampleNearest)
					{
						uint8 *src = (*Src)[yy];
						uint8 *end = src + Ex;
						uint32 *dst = (uint32*) (*Dst)[y];
						src += Sx;
						
						LgiAssert(Dst->GetColourSpace() == System32BitColourSpace);
						
						while (src < end)
						{
							*dst++ = pal[*src].u32;
							src += Factor;
						}
					}
					else if (SampleMode == GZoomView::SampleMax)
					{
						uint8 *s[32];
						LgiAssert(Factor < 32);
						for (int f=0; f<Factor; f++)
						{
							s[f] = (*Src)[yy + f];
							if (s[f])
								s[f] += Sx;
						}
						uint8 *scan_end = s[0] + (Ex - Sx);
						
						uint32 *dst_start = (uint32*) (*Dst)[y];
						uint32 *dst = dst_start;
						while (s[0] < scan_end)
						{
							register uint8 val = 0;
							
							for (int oy=0; oy<Factor && s[oy]; oy++)
							{
								register uint8 *src = s[oy];
								register uint8 *px_end = src + Factor;
								while (src < px_end)
								{
									if (*src > val)
										val = *src;
									src++;
								}
								s[oy] = src;
							}
							
							*dst++ = pal[val].u32;
						}
					}
					else
					{
						LgiAssert(!"Impl me.");
					}
					break;
				}
				case CsRgb15:
				case CsRgb16:
				case CsBgr15:
				case CsBgr16:
				{
					uint16 *src = (uint16*) (*Src)[yy];
					uint16 *dst = (uint16*) (*Dst)[y];
					uint16 *end = src + Ex;
					src += Sx;
					
					if (SampleMode == GZoomView::SampleNearest)
					{
						while (src < end)
						{
							*dst++ = *src;
							src += Factor;
						}
					}
					else
					{
						LgiAssert(!"Impl me.");
					}
					break;
				}
					
				#define ScaleDownCase(type, bits) \
					case Cs##type: \
						ScaleDown##bits(Dst->GetColourSpace(), (*Dst)[y], (G##type*) (*Src)[yy] + Sx, Ex - Sx, Factor); \
						break;

				ScaleDownCase(Rgbx32, 24);
				ScaleDownCase(Xrgb32, 24);
				ScaleDownCase(Bgrx32, 24);
				ScaleDownCase(Xbgr32, 24);

				ScaleDownCase(Rgb24, 24);
				ScaleDownCase(Bgr24, 24);
					
				case System32BitColourSpace:
				{
					System32BitPixel *src = (System32BitPixel*) (*Src)[yy];
					System32BitPixel *dst = (System32BitPixel*) (*Dst)[y];
					System32BitPixel *end = src + Ex;
					src += Sx;

					#define rop(c) dst->c = (DivLut[DivLut[dst->c * dst->a] * o] + DivLut[src->c * src->a]) * 255 / ra;

					LgiAssert(Dst->GetColourSpace() == Src->GetColourSpace());
					if (SampleMode == GZoomView::SampleNearest)
					{
						while (src < end)
						{
							if (src->a == 255)
							{
								*dst = *src;
							}
							else if (src->a)
							{
								uint8 o = 255 - src->a;
								int ra = (dst->a + src->a) - DivLut[dst->a * src->a];
								rop(r);
								rop(g);
								rop(b);
								dst->a = ra;
							}
							
							dst++;
							src += Factor;
						}
					}
					else
					{
						LgiAssert(!"Impl me.");
					}

					#undef rop
					break;
				}
				case CsBgr48:
				{
					GBgr48 *src = (GBgr48*) (*Src)[yy];
					GBgr24 *dst = (GBgr24*) (*Dst)[y];
					GBgr48 *end = src + Ex;
					src += Sx;

					LgiAssert(Dst->GetColourSpace() == CsBgr24);

					if (SampleMode == GZoomView::SampleNearest)
					{
						while (src < end)
						{
							dst->r = src->r >> 8;
							dst->g = src->g >> 8;
							dst->b = src->b >> 8;
							dst++;
							src += Factor;
						}
					}
					else
					{
						LgiAssert(!"Impl me.");
					}

					break;
				}
				case CsBgra64:
				{
					GBgra64 *s = (GBgra64*) (*Src)[yy];
					GBgra32 *d = (GBgra32*) (*Dst)[y];
					GBgra64 *end = s + Ex;
					s += Sx;

					LgiAssert(Dst->GetColourSpace() == CsBgra32);

					if (SampleMode == GZoomView::SampleNearest)
					{
						while (s < end)
						{
							if (s->a == 0xffff)
							{
								// Copy pixel
								d->r = s->r >> 8;
								d->g = s->g >> 8;
								d->b = s->b >> 8;
								d->a = s->a >> 8;
							}
							else if (s->a)
							{
								// Composite with dest
								uint8 o = (0xffff - s->a) >> 8;
								uint8 dc, sc;
								
								Rgb16to8PreMul(r);
								Rgb16to8PreMul(g);
								Rgb16to8PreMul(b);
							}
							
							d++;
							s += Factor;
						}
					}
					else
					{
						LgiAssert(!"Impl me.");
					}

					break;
				}
				default:
				{
					LgiAssert(!"Not impl");
					break;
				}
			}
		}
	}

	/// Scale pixels up into a tile
	void ScaleUp
	(
		/// The tile to write to
		GSurface *Dst,
		/// The source bitmap we are displaying
		GSurface *Src,
		/// X offset into source for tile's data
		int Sx,
		/// Y offset into source
		int Sy,
		/// The scaling factor to apply
		int Factor
	)
	{
		COLOUR Dark = GdcD->GetColour(Rgb24(128, 128, 128), Dst);
		COLOUR Light = GdcD->GetColour(Rgb24(192, 192, 192), Dst);
		int f = Factor - 1;
		int HasGrid = Callback ? Callback->DisplayGrid() : false;
		int HasTile = Callback ? Callback->DisplayTile() : false;

		// Now copy the right pixels over...
		int Pix = TileSize / Factor;
		LgiAssert(Dst->X() % Factor == 0);
		
		Pal32 pal[256];
		switch (Src->GetColourSpace())
		{
			default:
			{
				LgiAssert(0);
				break;
			}
			case CsIndex8:
			{
				GPalette *inPal = Src->Palette();
				for (int i=0; i<256; i++)
				{
					GdcRGB *rgb = inPal ? (*inPal)[i] : NULL;
					if (rgb)
					{
						pal[i].p32.r = rgb->r;
						pal[i].p32.g = rgb->g;
						pal[i].p32.b = rgb->b;
					}
					else
					{
						pal[i].p32.r = i;
						pal[i].p32.g = i;
						pal[i].p32.b = i;
						printf("Greyscae\n");
					}
					pal[i].p32.a = 255;
				}
				break;
			}
			case System16BitColourSpace:
			case System24BitColourSpace:
			case System32BitColourSpace:
			{
				if (Callback && (!TileCache || TileCache->X() != TileSize || TileCache->Y() != TileSize))
				{
					TileCache.Reset(new GMemDC(TileSize, TileSize, System32BitColourSpace));
				}
				
				if (TileCache)
				{
					GRect s;
					s.ZOff(TileSize-1, TileSize-1);
					s.Offset(Sx, Sy);
					
					GdcPt2 off(Sx, Sy);
					Callback->DrawBackground(TileCache, off, NULL);
					TileCache->Op(GDC_ALPHA);
					TileCache->Blt(0, 0, Src, &s);
				}
				break;
			}
		}
		
		for (int y=0; y<Pix; y++)
		{
			int DstX = 0;
			int DstY = y * Factor;
			int SrcY = Sy + y;
			if (SrcY < 0 || SrcY >= Src->Y())
				continue;
				
			int EndX = Sx + Pix;
			if (EndX > Src->X())
				EndX = Src->X();

			switch (Src->GetColourSpace())
			{
				case CsIndex8:
				{
					uint8 *src = (*Src)[SrcY];
					uint8 *end = src + EndX;
					src += Sx;
					
					while (src < end)
					{
						Dst->Colour(pal[*src++].u32, 32);
						Dst->Rectangle(DstX, DstY, DstX+f, DstY+f);
						DstX += Factor; 
					}
					break;                                
				}
				case CsRgb15:
				case CsRgb16:
				case CsBgr15:
				case CsBgr16:
				{
					uint16 *src = (uint16*) (*Src)[SrcY];
					uint16 *end = src + EndX;
					src += Sx;
					
					while (src < end)
					{
						Dst->Colour(*src++);
						Dst->Rectangle(DstX, DstY, DstX+f, DstY+f);
						DstX += Factor; 
					}
					break;                                
				}
				case System24BitColourSpace:
				{
					System24BitPixel *src = ((System24BitPixel*) (*Src)[SrcY]);
					System24BitPixel *end = src + EndX;
					src += Sx;
					
					while (src < end)
					{
						Dst->Colour(Rgb24(src->r, src->g, src->b));
						Dst->Rectangle(DstX, DstY, DstX+f, DstY+f);
						DstX += Factor; 
						src++;
					}
					break;
				}
				case CsBgr48:
				{
					GBgr48 *src = ((GBgr48*) (*Src)[SrcY]);
					GBgr48 *end = src + EndX;
					src += Sx;
					
					while (src < end)
					{
						Dst->Colour(Rgb24(src->r >> 8, src->g >> 8, src->b >> 8));
						Dst->Rectangle(DstX, DstY, DstX+f, DstY+f);
						DstX += Factor; 
						src++;
					}
					break;
				}
				case CsRgb48:
				{
					GRgb48 *src = ((GRgb48*) (*Src)[SrcY]);
					GRgb48 *end = src + EndX;
					src += Sx;
					
					while (src < end)
					{
						Dst->Colour(Rgb24(src->r >> 8, src->g >> 8, src->b >> 8));
						Dst->Rectangle(DstX, DstY, DstX+f, DstY+f);
						DstX += Factor; 
						src++;
					}
					break;
				}
				case CsRgba64:
				{
					GRgba64 *src = ((GRgba64*) (*Src)[SrcY]);
					GRgba64 *end = src + EndX;
					src += Sx;
					
					Dst->Op(GDC_ALPHA);
					while (src < end)
					{
						Dst->Colour(Rgba32(src->r >> 8, src->g >> 8, src->b >> 8, src->a >> 8));
						Dst->Rectangle(DstX, DstY, DstX+f, DstY+f);
						DstX += Factor; 
						src++;
					}
					break;
				}
				case CsBgra64:
				{
					GBgra64 *src = ((GBgra64*) (*Src)[SrcY]);
					GBgra64 *end = src + EndX;
					src += Sx;
					
					Dst->Op(GDC_ALPHA);
					while (src < end)
					{
						Dst->Colour(Rgba32(src->r >> 8, src->g >> 8, src->b >> 8, src->a >> 8));
						Dst->Rectangle(DstX, DstY, DstX+f, DstY+f);
						DstX += Factor; 
						src++;
					}
					break;
				}
				case System32BitColourSpace:
				{
					System32BitPixel *src, *end;
					if (TileCache)
					{
						src = (System32BitPixel*) (*TileCache)[y];
						end = src + (EndX - Sx);
					}
					else
					{
						src = (System32BitPixel*) (*Src)[SrcY];
						end = src + EndX;
						src += Sx;
					}
					
					if (HasGrid)
					{
						uint32 *s = (uint32*)src;
						uint32 *e = (uint32*)end;
						if (Factor == 2)
						{
							uint32 *dst0 = (uint32*) (*Dst)[DstY];
							uint32 *dst1 = (uint32*) (*Dst)[DstY+1];

							LgiAssert(Src->GetColourSpace() == Dst->GetColourSpace());
							
							if (dst1)
							{
								while (s < e)
								{
									*dst0++ = *s;
									*dst0++ = *s;
									*dst1++ = *s++;
									*dst1++ = Light;
								}
							}
							else if (dst0)
							{
								while (s < e)
								{
									*dst0++ = *s;
									*dst0++ = *s++;
								}
							}
						}
						else if (Factor == 3)
						{
							uint32 *dst0 = (uint32*) (*Dst)[DstY];
							uint32 *dst1 = (uint32*) (*Dst)[DstY+1];
							uint32 *dst2 = (uint32*) (*Dst)[DstY+2];

							LgiAssert(Src->GetColourSpace() == Dst->GetColourSpace());
							
							if (dst2)
							{
								while (s < e)
								{
									*dst0++ = *s;
									*dst0++ = *s;
									*dst0++ = *s;
									
									*dst1++ = *s;
									*dst1++ = *s;
									*dst1++ = Light;
									
									*dst2++ = *s++;
									*dst2++ = Light;
									*dst2++ = Dark;
								}
							}
							else if (dst1)
							{
								while (s < e)
								{
									*dst0++ = *s;
									*dst0++ = *s;
									*dst0++ = *s;
									
									*dst1++ = *s;
									*dst1++ = *s++;
									*dst1++ = Light;
								}
							}
							else if (dst0)
							{
								while (s < e)
								{
									*dst0++ = *s;
									*dst0++ = *s;
									*dst0++ = *s++;
								}
							}
						}
						else
						{
							// General case
							int ff = f - 1;
							while (src < end)
							{
								Dst->Colour(Rgba32(src->r, src->g, src->b, src->a));
								Dst->Rectangle(DstX, DstY, DstX+ff, DstY+ff);
								DstX += Factor; 
								src++;
							}
							
							((GMemDC*)Dst)->HLine(0, Dst->X(), DstY+f, Light, Dark);
						}
					}
					else
					{
						while (src < end)
						{
							Dst->Colour(Rgba32(src->r, src->g, src->b, src->a));
							Dst->Rectangle(DstX, DstY, DstX+f, DstY+f);
							DstX += Factor; 
							src++;
						}
					}
					break;
				}
				default:
				{
					LgiAssert(!"Not impl");
					break;
				}
			}
		}
		
		GMemDC *Mem = dynamic_cast<GMemDC*>(Dst);
		if (Mem && Factor > 3)
		{
			if (HasGrid)
			{
				for (int x = Factor - 1; x<Dst->X(); x += Factor)
				{
					Mem->VLine(x, 0, Dst->Y(), Light, Dark);
				}
				for (int y = Factor - 1; y<Dst->Y(); y += Factor)
				{
					Mem->HLine(0, Dst->X(), y, Light, Dark);
				}
			}

			if (HasTile)
			{
				COLOUR DarkBlue = GdcD->GetColour(Rgb24(0, 0, 128), Dst);
				COLOUR LightBlue = GdcD->GetColour(Rgb24(0, 0, 255), Dst);
				int TileX = 16;
				int TileY = 16;

				if (Callback)
				{
					switch (Callback->TileType())
					{
						case 0:
						{
							TileX = TileY = 16;
							break;
						}
						case 1:
						{
							TileX = TileY = 32;
							break;
						}
						default:
						{
							TileX = Callback->TileX();
							TileY = Callback->TileY();
							break;
						}
					}
				}

				int i = TileX - (Sx % TileX);
				for (int x = i * Factor - 1; x<Dst->X(); x += Factor * TileX)
				{
					Mem->VLine(x, 0, Dst->Y(), LightBlue, DarkBlue);
				}

				i = TileY - (Sy % TileY);
				for (int y = i * Factor - 1; y<Dst->Y(); y += Factor * TileY)
				{
					Mem->HLine(0, Dst->X(), y, LightBlue, DarkBlue);
				}
			}
		}
	}

	void UpdateTile(int x, int y)
	{
		LgiAssert(x >= 0 && x < Tiles.x);
		LgiAssert(y >= 0 && y < Tiles.y);
		ZoomTile *Dst = Tile[x][y];
		GSurface *Src = pDC;
		if (Dst && Src)
		{
			// Draw background
			if (Callback)
            {
                GRect r = Dst->Bounds();
                GdcPt2 off(x, y);
				Callback->DrawBackground(Dst, off, &r);
            }
			else
			{
				Dst->Colour(LC_WORKSPACE, 24);
				Dst->Rectangle();
			}
			
			int f = Factor();
			if (Zoom < 0)
			{
				ScaleDown(	Dst, Src,
							(x * TileSize) * f,
							(y * TileSize) * f,
							f);
			}
			else if (Zoom > 0)
			{
				ScaleUp(	Dst, Src,
							(x * TileSize) / f,
							(y * TileSize) / f,
							f);
			}
			else
			{
				// 1:1
				GRect s;
				s.ZOff(TileSize-1, TileSize-1);
				s.Offset(x * TileSize, y * TileSize);
				
				Dst->Op(GDC_ALPHA);
				Dst->Blt(0, 0, Src, &s);
			}

			Dst->Dirty = false;
		}
	}
};

GZoomView::GZoomView(GZoomViewCallback *callback) : ResObject(Res_Custom)
{
	d = new GZoomViewPriv(this, callback);
	Sunken(true);
}

GZoomView::~GZoomView()
{
	DeleteObj(d);
}

bool GZoomView::OnLayout(GViewLayoutInfo &Inf)
{
	Inf.Width.Min = -1;
	Inf.Width.Max = -1;
	Inf.Height.Min = -1;
	Inf.Height.Max = -1;
	
	return true;
}

void GZoomView::UpdateScrollBars(GdcPt2 *MaxScroll, bool ResetPos)
{
	GSurface *Src = d->pDC;
	if (!Src)
	{
		SetScrollBars(false, false);
		return;
	}
	
	GRect c = GetClient();    
	int Factor = d->Factor();
	int Fmin1 = Factor - 1;

	GdcPt2 DocSize(Src->X(), Src->Y());		
	GdcPt2 DocClientSize(c.X(), c.Y());
	DocClientSize = d->ScreenToDoc(DocClientSize);
	SetScrollBars(DocSize.x > DocClientSize.x, DocSize.y > DocClientSize.y);

	if (HScroll)
	{
		HScroll->SetLimits(0, DocSize.x);
		HScroll->SetPage(DocClientSize.x);
		if (ResetPos) HScroll->Value(0);
	}
	if (VScroll)
	{
		VScroll->SetLimits(0, DocSize.y);
		VScroll->SetPage(DocClientSize.y);
		if (ResetPos) VScroll->Value(0);
	}
	if (MaxScroll)
	{
		MaxScroll->x = DocSize.x - DocClientSize.x;
		MaxScroll->y = DocSize.y - DocClientSize.y;
	}
}

void GZoomView::OnPosChange()
{
	UpdateScrollBars();
}

void GZoomView::SetSurface(GSurface *dc, bool own)
{
	if (d->OwnDC)
		DeleteObj(d->pDC);
	
	d->pDC = dc;
	d->OwnDC = own;
	
	Reset();
}

GSurface *GZoomView::GetSurface()
{
	return d->pDC;
}

void GZoomView::Update(GRect *Where)
{
	#if DEBUG_THREADING
	if (!d->Dirty)
		LgiTrace("%i setting DIRTY\n", LgiGetCurrentThread());
	#endif

	GRect w;
	if (Where)
	{
		// Some tiles only
		w = d->DocToScreen(*Where);
		w.x1 /= d->TileSize;
		w.y1 /= d->TileSize;
		w.x2 /= d->TileSize;
		w.y2 /= d->TileSize;
		
		GRect b(0, 0, d->Tiles.x-1, d->Tiles.y-1);
		w.Bound(&b);
	}
	else
	{
		// All tiles
		w.ZOff(d->Tiles.x-1, d->Tiles.y-1);
	}

	if (d->Tile && w.Valid())
	{
		for (int y=w.y1; y<=w.y2; y++)
		{
			for (int x=w.x1; x<=w.x2; x++)
			{
				if (d->Tile[x][y])
					d->Tile[x][y]->Dirty = true;
			}
		}
	}
	
	Invalidate();
}

void GZoomView::Reset()
{
	d->EmptyTiles();
	d->SetDefaultZoom();
	UpdateScrollBars(NULL, true);
	Invalidate();
}

int GZoomView::GetBlockSize()
{
	return d->GetZoom() > 0 ? 16 : 1;
}

GZoomView::ViewportInfo GZoomView::GetViewport()
{
	ViewportInfo v;
	
	v.Zoom = d->GetZoom();
	v.Sy = VScroll ? (int)VScroll->Value() : 0;
	v.Sx = HScroll ? (int)HScroll->Value() : 0;
	
	return v;
}

void GZoomView::SetDefaultZoomMode(DefaultZoomMode m)
{
	d->DefaultZoom = m;
}

GZoomView::DefaultZoomMode GZoomView::GetDefaultZoomMode()
{
	return d->DefaultZoom;
}

void GZoomView::ScrollToPoint(GdcPt2 DocCoord)
{
	if (!d->pDC)
		return;
	if (HScroll)
	{
		int DocX = d->pDC->X();
		int Page = HScroll->Page();
		int MaxVal = DocX - (Page - 1);
		int64 x1 = HScroll->Value();
		int64 x2 = x1 + Page;
		if (DocCoord.x < x1)
			HScroll->Value(max(DocCoord.x, 0));
		else if (DocCoord.x > x2)
			HScroll->Value(min(DocCoord.x-Page+1, MaxVal));
	}
	if (VScroll)
	{
		int DocY = d->pDC->Y();
		int Page = VScroll->Page();
		int MaxVal = DocY - (Page - 1);
		int64 y1 = VScroll->Value();
		int64 y2 = y1 + Page;
		if (DocCoord.y < y1)
			VScroll->Value(max(DocCoord.y, 0));
		else if (DocCoord.y > y2)
			VScroll->Value(min(DocCoord.y-Page+1, MaxVal));
	}
}

void GZoomView::SetSampleMode(SampleMode sm)
{
	d->SampleMode = sm;
}

void GZoomView::SetViewport(ViewportInfo i)
{
	d->SetZoom(i.Zoom);
	if (!d->Tile)
		d->ResetTiles();

	GSurface *Src = d->pDC;
	if (Src)
	{
		GdcPt2 MaxScroll;
		UpdateScrollBars(&MaxScroll);
		if (HScroll)
		{
			if (i.Sx < 0)
				i.Sx = 0;
			if (i.Sx > MaxScroll.x)
				i.Sx = MaxScroll.x;
			HScroll->Value(i.Sx);
		}
		if (VScroll)
		{
			if (i.Sy < 0)
				i.Sy = 0;
			if (i.Sy > MaxScroll.y)
				i.Sy = MaxScroll.y;
			VScroll->Value(i.Sy);
		}
		
		// FIXME... mark stuff dirty
		Invalidate();
	}
}

void GZoomView::SetCallback(GZoomViewCallback *cb)
{
	d->Callback = cb;
}

bool GZoomView::Convert(GPointF &p, int x, int y)
{
	int Sx = 0, Sy = 0;
	int Factor = d->Factor();
	GetScrollPos(Sx, Sy);
	
	if (d->GetZoom() > 0)
	{
		// Scaled up
		p.x = (double)Sx + ((double)x / Factor);
		p.y = (double)Sy + ((double)y / Factor);
	}
	else if (d->GetZoom() < 0)
	{
		// Scaled down
		p.x = Sx + (x * Factor);
		p.y = Sy + (y * Factor);
	}
	else
	{
		p.x = Sx + x;
		p.y = Sy + y;
	}
	
	GSurface *Src = d->pDC;
	if (Src &&
		p.x >= 0 &&
		p.x <= Src->X() &&
		p.y >= 0 &&
		p.y <= Src->Y())
	{
		return true;
	}
	
	return false;
}

void GZoomView::OnMouseClick(GMouse &m)
{
	if (m.Down())
		Focus(true);
}

bool GZoomView::OnMouseWheel(double Lines)
{
	GMouse m;
	GetMouse(m);
	
	if (m.Ctrl())
	{
		GPointF DocPt;
		bool In = Convert(DocPt, m.x, m.y);

		// Zoom the graphic
		if (Lines < 0)
		{
			d->SetZoom(d->GetZoom() + 1);
		}
		else if (Lines > 0)
		{
			d->SetZoom(d->GetZoom() - 1);
		}
		else return true;
		d->ResetTiles();
		
		if (d->Callback)
		{
			char Msg[256];
			sprintf_s(
				Msg,
				sizeof(Msg),
				"Zoom: %s%i",
				d->GetZoom() < 0 ? "1/" : (d->GetZoom() == 0 ? "1:" : ""), d->Factor());
			d->Callback->SetStatusText(Msg);
		}
		
		// Is the cursor over the image???
		if (In)
		{
			GSurface *Src = d->pDC;
			if (Src)
			{
				GRect c = GetClient();
				int Factor = d->Factor();

				int NewSx, NewSy;
				
				if (d->GetZoom() > 0)
				{
					// Scale up
					NewSx = (int) (DocPt.x - (m.x / Factor));
					NewSy = (int) (DocPt.y - (m.y / Factor));
				}
				else if (d->GetZoom() < 0)
				{
					// Scale down
					NewSx = (int)DocPt.x - (m.x * Factor);
					NewSy = (int)DocPt.y - (m.y * Factor);
				}
				else
				{
					// 1:1
					NewSx = (int) (DocPt.x - m.x);
					NewSy = (int) (DocPt.y - m.y);
				}
				
				GdcPt2 ScaledDocSize(Src->X(), Src->Y());
				ScaledDocSize = d->DocToScreen(ScaledDocSize);

				SetScrollBars(ScaledDocSize.x > c.X(), ScaledDocSize.y > c.Y());

				/*
				GdcPt2 MaxScroll(	HScroll ? ScaledDocSize.x - c.X() : 0,
									VScroll ? ScaledDocSize.y - c.Y() : 0);
				MaxScroll = d->ScreenToDoc(MaxScroll);
				if (NewSx > MaxScroll.x)
					NewSx = MaxScroll.x;
				if (NewSy > MaxScroll.y)
					NewSy = MaxScroll.y;
				*/

				GdcPt2 ScaledClient(c.X(), c.Y());
				ScaledClient = d->ScreenToDoc(ScaledClient);

				if (HScroll)
				{
					HScroll->SetLimits(0, Src->X());
					HScroll->SetPage(ScaledClient.x);
					HScroll->Value(NewSx);
				}
				if (VScroll)
				{
					VScroll->SetLimits(0, Src->Y());
					VScroll->SetPage(ScaledClient.y);
					VScroll->Value(NewSy);
				}

				#if 0
				{
					GPointF NewDocPt;
					bool In = Convert(NewDocPt, m.x, m.y);

					/*
					LgiTrace("Scroll: doc=%i,%i cli=%i,%i maxx=%i maxy=%i\n",
						Src->X(), Src->Y(),
						c.X(), c.Y(),
						MaxScroll.x, MaxScroll.y);
					*/

					LgiTrace("Zoom: DocPt=%.2f,%.2f->%.2f,%.2f Mouse=%i,%i Factor: %i Zoom: %i DocSize: %i,%i NewScroll: %i,%i\n",
						DocPt.x, DocPt.y,
						NewDocPt.x, NewDocPt.y,
						m.x, m.y,
						Factor,
						d->GetZoom(),
						Src->X(), Src->Y(),
						NewSx, NewSy);
				}
				#endif
			}
		}

		// Update the screen        
		// FIXME... mark stuff dirty
		Invalidate();
		SendNotify(NotifyViewportChanged);
	}
	else
	{
		// Normal wheel
		if (m.Shift())
		{
			// Scroll in the X direction...
			if (HScroll)
			{
				HScroll->Value((int64) (HScroll->Value() + (Lines * 5)));
				SendNotify(NotifyViewportChanged);
			}
		}
		else
		{
			// Scroll in the Y direction...
			if (VScroll)
			{
				VScroll->Value((int64) (VScroll->Value() + (Lines * 5)));
				SendNotify(NotifyViewportChanged);
			}
		}
	}
	
	return true;
}

void GZoomView::OnPulse()
{
	GMouse m;
	if (!GetMouse(m))
	{
		LgiTrace("%s:%i - GetMouse failed.\n", _FL);
		return;
	}

	GRect c = GetClient();
	bool Inside = c.Overlap(m.x, m.y);

	if (!Inside)
	{
		float s = ZoomToScale(d->GetZoom());

		// Scroll window to show pixel under the mouse...
		bool Update = false;
		if (VScroll)
		{
			int Amount = 0;
			if (m.y < 0)
				Amount = (int) (m.y / s);
			else if (m.y > c.y2)
				Amount = (int) ((m.y - c.y2) / s);
				
			if (Amount)
			{
				VScroll->Value(VScroll->Value() + Amount);
				SendNotify(NotifyViewportChanged);
				Update = true;
			}
		}
		if (HScroll)
		{
			int Amount = 0;
			if (m.x < 0)
				Amount = (int) (m.x / s);
			else if (m.x > c.x2)
				Amount = (int) ((m.x - c.x2) / s);

			if (Amount)
			{
				HScroll->Value(HScroll->Value() + Amount);
				SendNotify(NotifyViewportChanged);
				Update = true;
			}
		}
		
		if (Update)
		{
			OnMouseMove(m);
		}
	}
}

GMessage::Param GZoomView::OnEvent(GMessage *m)
{
	switch (MsgCode(m))
	{
		case M_RENDER_FINISHED:
		{
			return 0;
		}
	}
	
	return GLayout::OnEvent(m);
}

int GZoomView::OnNotify(GViewI *v, int f)
{
	switch (v->GetId())
	{
		case IDC_VSCROLL:
		case IDC_HSCROLL:
		{
			Invalidate();
			#ifdef WIN32
			UpdateWindow(Handle());
			#endif
			SendNotify(NotifyViewportChanged);
			break;
		}
	}

	return 0;
}

void GZoomView::OnPaint(GSurface *pDC)
{
	#if 0
	// coverage test
	pDC->Colour(GColour(255, 0, 255));
	pDC->Rectangle();
	#endif


	GRect c = GetClient();
	GRegion Rgn(c);

	GSurface *Src = d->pDC;
	if (Src)
	{
		if (!d->Tile)
			d->ResetTiles();

		int Sx = 0, Sy = 0;
		GetScrollPos(Sx, Sy);

		// Get the image bounds and scroll it into position (view coords)
		GRect ScaledDocSize = Src->Bounds();
		ScaledDocSize = d->DocToScreen(ScaledDocSize);

		GRect s = ScaledDocSize;
		
		// Scroll positions are in doc px, so scale them here to screen
		GdcPt2 ScaledScroll(Sx, Sy);
		ScaledScroll = d->DocToScreen(ScaledScroll);
		s.Offset(-ScaledScroll.x, -ScaledScroll.y);

		// Work out the visible tiles...
		GRect vis = s;
		vis.Bound(&c);
		vis.Offset(ScaledScroll.x, ScaledScroll.y);
		
		GRect tile;
		tile.x1 = vis.x1 / d->TileSize;
		tile.y1 = vis.y1 / d->TileSize;
		tile.x2 = vis.x2 / d->TileSize;
		tile.y2 = vis.y2 / d->TileSize;
		
		// Check the visible tiles are in range
		LgiAssert(tile.x1 >= 0 && tile.x1 < d->Tiles.x);
		LgiAssert(tile.y1 >= 0 && tile.y1 < d->Tiles.y);
		LgiAssert(tile.x2 >= 0 && tile.x2 < d->Tiles.x);
		LgiAssert(tile.y2 >= 0 && tile.y2 < d->Tiles.y);
		
		// Make sure we have tiles available and they are up to date
		int Bits = Src->GetBits() <= 8 ? 32 : Src->GetBits();
		for (int y=tile.y1; y<=tile.y2; y++)
		{
			for (int x=tile.x1; x<=tile.x2; x++)
			{
				if (!d->Tile[x][y] || d->Tile[x][y]->GetBits() != Bits)
				{
					DeleteObj(d->Tile[x][y]);
					d->Tile[x][y] = new ZoomTile(d->TileSize, Bits);
				}
				if (d->Tile[x][y] &&
					d->Tile[x][y]->Dirty)
				{
					d->UpdateTile(x, y);
				}
			}
		}
		
		// Paint our tiles...
		pDC->SetOrigin(ScaledScroll.x, ScaledScroll.y);
		for (int y=tile.y1; y<=tile.y2; y++)
		{
			bool LastY = y == d->Tiles.y - 1;
			
			for (int x=tile.x1; x<=tile.x2; x++)
			{
				if (d->Tile[x][y])
				{
					int px = x * d->TileSize;
					int py = y * d->TileSize;
					GRect r(px, py, px + d->TileSize - 1, py + d->TileSize - 1);
					
					if (LastY || x == d->Tiles.x - 1)
					{
						// We have to clip off the part of the tile that doesn't 
						// contain image.
						
						// Work out how much image is in the tile
						GRect img = r;
						if (img.x2 >= ScaledDocSize.X())
							img.x2 = ScaledDocSize.X();
						if (img.y2 >= ScaledDocSize.Y())
							img.y2 = ScaledDocSize.Y();
						
						// Work out the image pixels in tile co-ords
						GRect tile_source(0, 0, img.X()-1, img.Y()-1);
						
						// Also work out where this is on the screen
						GRect screen_source = tile_source;
						screen_source.Offset(px, py);
						
						// Blt the tile image pixels to the screen
						pDC->Blt(px, py, d->Tile[x][y], &tile_source);

						#if 0
						pDC->Colour(GColour(0, 0, 255));
						pDC->Box(&screen_source);
						#endif

						screen_source.Offset(-ScaledScroll.x, -ScaledScroll.y);
						Rgn.Subtract(&screen_source);
					}
					else
					{
						// Full tile of image data
						pDC->Blt(px, py, d->Tile[x][y]);

						#if 0
						pDC->Colour(GColour(64, 0, 255));
						pDC->Box(&r);
						#endif

						r.Offset(-ScaledScroll.x, -ScaledScroll.y);
						Rgn.Subtract(&r);
					}
				}
				else LgiAssert(!"Missing tile?");
			}
		}
	}

	pDC->SetOrigin(0, 0);
	for (GRect *r = Rgn.First(); r; r = Rgn.Next())
	{
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle(r);
		#if 0
		pDC->Colour(GColour(255, 0, 255));
		pDC->Box(r);
		#endif
	}
}

class GZoomViewFactory : public GViewFactory
{
public:
	GView *NewView(const char *Class, GRect *Pos, const char *Text)
	{
		if (!_stricmp(Class, "GZoomView"))
			return new GZoomView(NULL);
		
		return NULL;
	}
} ZoomViewFactory;