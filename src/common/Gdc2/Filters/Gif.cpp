/*hdr
**      FILE:           Gif.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           8/9/1998
**      DESCRIPTION:    Gif file filter
**
**      Copyright (C) 1997-8, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "Lgi.h"
#include "Lzw.h"
#include "GVariant.h"

#ifdef FILTER_UI
// define the symbol FILTER_UI to access the gif save options dialog
#include "GTransparentDlg.h"
#endif

#define MAX_CODES   				4095

class GdcGif : public GFilter
{
	GSurface *pDC;
	GStream *s;
	int ProcessedScanlines;

	// Old GIF coder stuff
	short linewidth;
	int lines;
	int pass;
	short curr_size;				/* The current code size */
	short clearc;					/* Value for a clear code */
	short ending;					/* Value for a ending code */
	short newcodes;					/* First available code */
	short top_slot;					/* Highest code for current size */
	short slot;						/* Last read code */
	short navail_bytes;				/* # bytes left in block */
	short nbits_left;				/* # bits left in current byte */
	uchar b1;						/* Current byte */
	uchar byte_buff[257];			/* Current block */
	uchar *pbytes;					/* Pointer to next byte in block */
	uchar stack[MAX_CODES+1];		/* Stack for storing pixels */
	uchar suffix[MAX_CODES+1];		/* Suffix table */
	ushort prefix[MAX_CODES+1];		/* Prefix linked list */
	int bad_code_count;

	int get_byte();
	int out_line(uchar *pixels, int linewidth, int interlaced, int BitDepth);
	short init_exp(short size);
	short get_next_code();
	short decoder(int BitDepth, uchar interlaced);

public:
	GdcGif();

	Format GetFormat() { return FmtGif; }
	int GetCapabilites() { return FILTER_CAP_READ | FILTER_CAP_WRITE; }
	IoStatus ReadImage(GSurface *pDC, GStream *In);
	IoStatus WriteImage(GStream *Out, GSurface *pDC);

	bool GetVariant(const char *n, GVariant &v, char *a)
	{
		if (!stricmp(n, LGI_FILTER_TYPE))
		{
			v = "Gif";
		}
		else if (!stricmp(n, LGI_FILTER_EXTENSIONS))
		{
			v = "GIF";
		}
		else return false;

		return true;
	}
};

// Filter factory
// tells the application we're here

class GdcGifFactory : public GFilterFactory
{
	bool CheckFile(char *File, int Access, uchar *Hint)
	{
		if (Hint)
		{
			if (Hint[0] == 'G' &&
				Hint[1] == 'I' &&
				Hint[2] == 'F' &&
				Hint[3] == '8' &&
				Hint[4] == '9')
			{
				return true;
			}
		}
		
		return (File) ? stristr(File, ".gif") != 0 : false;
	}

	GFilter *NewObject()
	{
		return new GdcGif;
	}
}
GifFactory;

// gif error codes
#define OUT_OF_MEMORY			-10
#define BAD_CODE_SIZE			-20
#define READ_ERROR				-1
#define WRITE_ERROR				-2
#define OPEN_ERROR				-3
#define CREATE_ERROR			-4

long code_mask[13] = {
	  0,
	  0x0001,
	  0x0003,
	  0x0007,
	  0x000F,
	  0x001F,
	  0x003F,
	  0x007F,
	  0x00FF,
	  0x01FF,
	  0x03FF,
	  0x07FF,
	  0x0FFF};

int GdcGif::get_byte()
{
	uchar c;
	
	if (s->Read(&c, 1) == 1)
		return c;

	return READ_ERROR;
}

int GdcGif::out_line(uchar *pixels, int linewidth, int interlaced, int BitDepth)
{
	// static int p;
	// if (lines == 0) p = 0;

    if (lines >= pDC->Y())
        return -1;

	switch (pDC->GetBits())
	{
	    case 8:
	    {
	        memcpy((*pDC)[lines], pixels, pDC->X());
	        break;
	    }
	    case 32:
	    {
	        GRgba32 *s = (GRgba32*) (*pDC)[lines];
	        GRgba32 *e = s + pDC->X();
	        GPalette *pal = pDC->Palette();
        	GdcRGB *p = (*pal)[0], *pix;
        	
	        while (s < e)
	        {
	            pix = p + *pixels++;
	            s->r = pix->R;
	            s->g = pix->G;
	            s->b = pix->B;
	            s->a = 255;
	            s++;
	        }
	        break;
	    }
	    default:
	    {
	        LgiAssert(0);
	        break;
	    }
	}
	ProcessedScanlines++;

	if (interlaced)
	{
		switch (pass)
		{
			case 0:
				lines += 8;
				if (lines >= pDC->Y())
				{
					lines = 4;
					pass++;
				}
				break;
				
			case 1:
				lines += 8;
				if (lines >= pDC->Y())
				{
				    lines = 2;
					pass++;
				}
				break;

			case 2:
				lines += 4;
				if (lines >= pDC->Y())
				{
					lines = 1;
					pass++;
				}
				break;
				
			case 3:
				lines += 2;
				break;
		}
	}
	else
	{
		lines++;
	}

	if (Meter)
	{
		int a = Meter->Value() * 100 / pDC->Y();
		int b = lines * 100 / pDC->Y();
		if (abs(a-b) > 5)
		{
			Meter->Value(lines);
		}
	}

	return 0;
}

short GdcGif::init_exp(short size)
{
	curr_size = size + 1;
	top_slot = 1 << curr_size;
	clearc = 1 << size;
	ending = clearc + 1;
	slot = newcodes = ending + 1;
	navail_bytes = nbits_left = 0;

	return 0;
}

short GdcGif::get_next_code()
{
	short i, x;
	ulong ret;

	if (nbits_left == 0)
	{
		if (navail_bytes <= 0)
		{

			/* Out of bytes in current block, so read next block
			 */
			pbytes = byte_buff;
			if ((navail_bytes = get_byte()) < 0)
			{
				return(navail_bytes);
			}
			else if (navail_bytes)
			{
				for (i = 0; i < navail_bytes; ++i)
				{
					if ((x = get_byte()) < 0)
					{
						return(x);
					}

					byte_buff[i] = x;
				}
			}
		}

		b1 = *pbytes++;
		nbits_left = 8;
		--navail_bytes;
	}

	ret = b1 >> (8 - nbits_left);
	while (curr_size > nbits_left)
	{
		if (navail_bytes <= 0)
		{
			/* Out of bytes in current block, so read next block
			 */
			pbytes = byte_buff;

			if ((navail_bytes = get_byte()) < 0)
			{
				return(navail_bytes);
			}
			else if (navail_bytes)
			{
				for (i = 0; i < navail_bytes; ++i)
				{
					if ((x = get_byte()) < 0)
					{
						return(x);
					}

					byte_buff[i] = x;
				}
			}
		}

		b1 = *pbytes++;
		ret |= b1 << nbits_left;
		nbits_left += 8;
		--navail_bytes;
	}

	nbits_left -= curr_size;
	ret &= code_mask[curr_size];

	return ((short) ret);
}

short GdcGif::decoder(int BitDepth, uchar interlaced)
{
	uchar *sp, *bufptr;
	uchar *buf;
	short code, fc, oc, bufcnt;
	short c, size, ret;

	lines = 0;
	pass = 0;

	/* Initialize for decoding a new image...
	 */
	if ((size = get_byte()) < 0)
	{
		return(size);
	}

	if (size < 2 || 9 < size)
	{
		return(BAD_CODE_SIZE);
	}

	init_exp(size);

	/* Initialize in case they forgot to put in a clearc code.
	 * (This shouldn't happen, but we'll try and decode it anyway...)
	 */
	oc = fc = 0;

	/* Allocate space for the decode buffer
	 */
	buf = new uchar[linewidth+1];
	if (buf == NULL)
	{
		return (OUT_OF_MEMORY);
	}

	/* Set up the stack pointer and decode buffer pointer
	 */
	uchar *EndOfStack = stack + sizeof(stack);
	sp = stack;
	bufptr = buf;
	bufcnt = linewidth;

	/* This is the main loop.  For each code we get we pass through the
	 * linked list of prefix codes, pushing the corresponding "character" for
	 * each code onto the stack.  When the list reaches a single "character"
	 * we push that on the stack too, and then start unstacking each
	 * character for output in the correct order.  Special handling is
	 * included for the clearc code, and the whole thing ends when we get
	 * an ending code.
	 */
	while ((c = get_next_code()) != ending)
	{

		/* If we had a file error, return without completing the decode
		 */
		if (c < 0)
		{
			DeleteArray(buf);
			return(0);
		}

		/* If the code is a clearc code, reinitialize all necessary items.
		 */
		if (c == clearc)
		{
			curr_size = size + 1;
			slot = newcodes;
			top_slot = 1 << curr_size;

			/* Continue reading codes until we get a non-clearc code
			 * (Another unlikely, but possible case...)
			 */
			while ((c = get_next_code()) == clearc)
				;

			/* If we get an ending code immediately after a clearc code
			 * (Yet another unlikely case), then break out of the loop.
			 */
			if (c == ending)
			{
				break;
			}

			/* Finally, if the code is beyond the range of already set codes,
			 * (This one had better !happen...  I have no idea what will
			 * result from this, but I doubt it will look good...) then set it
			 * to color zero.
			 */
			if (c >= slot)
			{
				c = 0;
			}

			oc = fc = c;

			/* And let us not forget to put the char into the buffer... And
			 * if, on the off chance, we were exactly one pixel from the end
			 * of the line, we have to send the buffer to the out_line()
			 * routine...
			 */
			*bufptr++ = c;

			if (--bufcnt == 0)
			{
				if ((ret = out_line(buf, linewidth, interlaced, BitDepth)) < 0)
				{
					DeleteArray(buf);
					return (ret);
				}

				bufptr = buf;
				bufcnt = linewidth;
			}
		}
		else
		{

			/* In this case, it's not a clearc code or an ending code, so
			 * it must be a code code...  So we can now decode the code into
			 * a stack of character codes. (Clear as mud, right?)
			 */
			code = c;

			/* Here we go again with one of those off chances...  If, on the
			 * off chance, the code we got is beyond the range of those already
			 * set up (Another thing which had better !happen...) we trick
			 * the decoder into thinking it actually got the last code read.
			 * (Hmmn... I'm not sure why this works...  But it does...)
			 */
			if (code >= slot)
			{
				if (code > slot)
				{
					++bad_code_count;
				}

				code = oc;
				*sp++ = fc;
			}

			/* Here we scan back along the linked list of prefixes, pushing
			 * helpless characters (ie. suffixes) onto the stack as we do so.
			 */
			while (code >= newcodes)
			{
				if (sp >= EndOfStack || code >= MAX_CODES + 1)
				{
					return -1;
				}

				*sp++ = suffix[code];
				code = prefix[code];
			}

			/* Push the last character on the stack, and set up the new
			 * prefix and suffix, and if the required slot number is greater
			 * than that allowed by the current bit size, increase the bit
			 * size.  (NOTE - If we are all full, we *don't* save the new
			 * suffix and prefix...  I'm not certain if this is correct...
			 * it might be more proper to overwrite the last code...
			 */
			*sp++ = code;
			if (slot < top_slot)
			{
				suffix[slot] = fc = code;
				prefix[slot++] = oc;
				oc = c;
			}

			if (slot >= top_slot)
			{
				if (curr_size < 12)
				{
					top_slot <<= 1;
					++curr_size;
				}
			}

			/* Now that we've pushed the decoded string (in reverse order)
			 * onto the stack, lets pop it off and put it into our decode
			 * buffer...  And when the decode buffer is full, write another
			 * line...
			 */
			while (sp > stack)
			{
				*bufptr++ = *(--sp);

				if (--bufcnt == 0)
				{
					if ((ret = out_line(buf, linewidth, interlaced, BitDepth)) < 0)
					{
						DeleteArray(buf);
						return(ret);
					}

					bufptr = buf;
					bufcnt = linewidth;
				}
			}
		}
	}

	ret = 0;

	if (bufcnt != linewidth)
	{
		ret = out_line(buf, (linewidth - bufcnt), interlaced, BitDepth);
	}

	DeleteArray(buf);

	return(ret);
}

GFilter::IoStatus GdcGif::ReadImage(GSurface *pdc, GStream *in)
{
	GFilter::IoStatus Status = IoError;
	pDC = pdc;
	s = in;
	ProcessedScanlines = 0;

	if (pDC && s)
	{
		bad_code_count = 0;

		if (!FindHeader(0, "GIF8?a", s))
		{
			// not a gif file
		}
		else
		{
			bool Transparent = false;
			uchar ImgCode;
			uchar interlace = false;
			uint16 LogicalX = 0;
			uint16 LogicalY = 0;
			uint8 BackgroundColour = 0;
			uint8 PixelAspectRatio = 0;

			// read header
			Read(s, &LogicalX, sizeof(LogicalX));
			Read(s, &LogicalY, sizeof(LogicalY));
			Read(s, &ImgCode, sizeof(ImgCode));
			int bits = (ImgCode & 0x07) + 1;
			Read(s, &BackgroundColour, sizeof(BackgroundColour));
			Read(s, &PixelAspectRatio, sizeof(PixelAspectRatio));

			if (ImgCode & 0x80)
			{
				uchar PalData[768];

				int size = (1 << bits) * 3;
				memset(PalData, 0xFF, sizeof(PalData));
				s->Read(PalData, size);

				/*
				printf("%s:%i - Size=%i {%02.2x,%02.2x,%02.2x %02.2x,%02.2x,%02.2x %02.2x,%02.2x,%02.2x}\n",
					__FILE__, __LINE__, size,
					PalData[0], PalData[1], PalData[2],
					PalData[3], PalData[4], PalData[5], 
					PalData[6], PalData[7], PalData[8]);
				*/

				GPalette *Pal = new GPalette(PalData, size / 3);
				if (Pal)
				{
					pDC->Palette(Pal);
				}
			}

			// Start reading the block stream
			bool Done = false;
			uchar BlockCode = 0;
			uchar BlockLabel = 0;
			uchar BlockSize = 0;

			while (!Done)
			{
				#define Rd(Var) \
					if (!Read(s, &Var, sizeof(Var))) \
					{ \
						Done = true; \
						break; \
					}
					
				Rd(BlockCode);

				switch (BlockCode)
				{
					case 0x2C:
					{
						// Image Descriptor
						uint16 x1, y1, sx, sy;
						uint8 Flags;

						Rd(x1);
						Rd(y1);
						Rd(sx);
						Rd(sy);
						Rd(Flags);

						linewidth = sx; //(sx * bits + 7) / 8;
						interlace = (Flags & 0x40) != 0;

						if (pDC->Create(sx, sy, 8))
						{
							if (Flags & 0x80)
							{
								uchar PalData[768];

								int size = (1 << ((Flags & 7) + 1)) * 3;
								memset(PalData, 0xFF, sizeof(PalData));
								if (s->Read(PalData, size) != size)
								{
									Done = true;
									break;
								}

								GPalette *Pal = new GPalette(PalData, size / 3);
								if (Pal)
								{
									pDC->Palette(Pal);
								}
							}

							// Progress
							if (Meter)
							{
								Meter->SetDescription("scanlines");
								Meter->SetLimits(0, sy-1);
							}

							// Decode image
							decoder(bits, interlace);
							if (ProcessedScanlines == pDC->Y())
							    Status = IoSuccess;
							
							if (Transparent)
							{
								// Setup alpha channel
								pDC->HasAlpha(true);
								GSurface *Alpha = pDC->AlphaDC();
								if (Alpha)
								{
									for (int y=0; y<pDC->Y(); y++)
									{
										uchar *C = (*pDC)[y];
										uchar *A = (*Alpha)[y];

										for (int x=0; x<pDC->X(); x++)
										{
											A[x] = C[x] == BackgroundColour ? 0x00 : 0xff;
										}
									}
								}
							}
						}
						else
						{
							printf("%s:%i - Failed to create output surface.\n", __FILE__, __LINE__);
						}

						Done = true;
						break;
					}
					case 0x21:
					{
						uint8 GraphicControlLabel;
						uint8 BlockSize;
						uint8 PackedFields;
						uint16 Delay;

						Rd(GraphicControlLabel);
						Rd(BlockSize);

						switch (GraphicControlLabel)
						{
							case 0xF9:
							{
								Rd(PackedFields);
								Rd(Delay);
								Rd(BackgroundColour);
								Transparent = (PackedFields & 0x80) != 0;
								break;
							}
							default:
							{
								s->SetPos(s->GetPos() + BlockSize);
								break;
							}
						}

						Rd(BlockSize);
						while (BlockSize)
						{
							int64 NewPos = s->GetPos() + BlockSize;
							if (s->SetPos(NewPos) != NewPos ||
								!Read(s, &BlockSize, sizeof(BlockSize)))
								break;
						}
						break;
					}
					default:
					{
						// unknown block
						Rd(BlockLabel);
						Rd(BlockSize);

						while (BlockSize)
						{
							int NewPos = s->GetPos() + BlockSize;
							if (s->SetPos(NewPos) != NewPos ||
								!Read(s, &BlockSize, sizeof(BlockSize)))
								break;
						}
						break;
					}
				}
			}
		}
	}

	return Status;
}

GFilter::IoStatus GdcGif::WriteImage(GStream *Out, GSurface *pDC)
{
	GVariant Transparent;
	int Back = -1;
	GVariant v;

	if (!Out || !pDC)
		return GFilter::IoError;

	if (pDC->GetBits() > 8)
	{			
		if (Props)
			Props->SetValue(LGI_FILTER_ERROR, v = "The GIF format only supports 1 to 8 bit graphics.");
		return GFilter::IoUnsupportedFormat;
	}

	#ifdef FILTER_UI
	GVariant Parent;

	if (Props)
	{
		Props->GetValue(LGI_FILTER_PARENT_WND, Parent);
		if (Props->GetValue(LGI_FILTER_BACKGROUND, v))
		{
			Back = v.CastInt32();
		}

		if (Parent.Type == GV_GVIEW)
		{
		    // If the source document has an alpha channel then we use 
		    // that to create transparent pixels in the output, otherwise
		    // we ask the user if they want the background transparent...
		    if (pDC->AlphaDC())
		    {
		        Transparent = true;
		        
		        // However we have to pick an unused colour to set as the 
		        // "background" pixel value
		        bool Used[256];
		        ZeroObj(Used);
	            for (int y=0; y<pDC->Y(); y++)
	            {
	                uint8 *p = (*pDC)[y];
	                uint8 *a = (*pDC->AlphaDC())[y];
	                LgiAssert(p && a);
	                if (!p || !a) break;
	                uint8 *e = p + pDC->X();
	                while (p < e)
	                {
	                    if (*a)
	                        Used[*p] = true;
	                    a++;
	                    p++;
	                }
	            }
	            
	            Back = -1;
	            for (int i=0; i<256; i++)
	            {
	                if (!Used[i])
	                {
	                    Back = i;
	                    break;
	                }
	            }
	            
	            if (Back < 0)
	            {
		            if (Props)
			            Props->SetValue(LGI_FILTER_ERROR, v = "No unused colour for transparent pixels??");
		            return IoError;
	            }
		    }
		    else
		    {
			    // put up a dialog to ask about transparent colour
			    GTransparentDlg Dlg((GView*)Parent.Value.Ptr, &Transparent);
			    if (!Dlg.DoModal())
			    {
				    Props->SetValue("Cancel", v = 1);
				    return IoCancel;
			    }
			}
			
			if (Transparent.CastBool() && Back < 0)
			{
			    LgiAssert(!"No background colour available??");
	            if (Props)
		            Props->SetValue(LGI_FILTER_ERROR, v = "Transparency requested, but no background colour set.");
                return IoError;			    
			}
		}
	}
	#endif

	GPalette *Pal = pDC->Palette();

	// Intel byte ordering
	Out->SetSize(0);

	// Header
	Out->Write((void*)"GIF89a", 6);

	// Logical screen descriptor
	int16 s = pDC->X();
	Write(Out, &s, sizeof(s));
	s = pDC->Y();
	Write(Out, &s, sizeof(s));

	bool Ordered = false;
	uint8 c =	((Pal != 0) ? 0x80 : 0) | // global colour table/transparent
				(pDC->GetBits() - 1)    | // bits per pixel
				((Ordered) ? 0x08 : 0)  | // colours are sorted
				(pDC->GetBits() - 1);

	Out->Write(&c, 1);
	c = 0;
	Out->Write(&c, 1); // background colour
	c = 0;
	Out->Write(&c, 1); // aspect ratio

	// global colour table
	if (Pal)
	{
		uchar Buf[768];
		uchar *d = Buf;
		int Colours = 1 << pDC->GetBits();
		for (int i=0; i<Colours; i++)
		{
			GdcRGB *s = (*Pal)[i];
			if (s)
			{
				*d++ = s->R;
				*d++ = s->G;
				*d++ = s->B;
			}
			else
			{
				*d++ = i;
				*d++ = i;
				*d++ = i;
			}
		}

		Out->Write(Buf, Colours * 3);
	}

	if (Transparent.CastBool())
	{
		// Graphic Control Extension
		uchar gce[] = {0x21, 0xF9, 4, 1, 0, 0, Back, 0 };
		Out->Write(gce, sizeof(gce));
	}

	// Image descriptor
	c = 0x2c;
	Out->Write(&c, 1); // Image Separator
	s = 0;
	Write(Out, &s, sizeof(s)); // Image left position
	s = 0;
	Write(Out, &s, sizeof(s)); // Image top position
	s = pDC->X();
	Write(Out, &s, sizeof(s)); // Image width
	s = pDC->Y();
	Write(Out, &s, sizeof(s)); // Image height
	c = 0;
	Out->Write(&c, 1); // Flags

	// Image data
	c = 8;
	Out->Write(&c, 1); // Min code size

	GBytePipe Encode, Pixels;

	// Get input ready
	int Len = (pDC->X() * pDC->GetBits() + 7) / 8;
	uint8 *buf = pDC->AlphaDC() ? new uint8[Len] : 0;
	for (int y=0; y<pDC->Y(); y++)
	{
		uint8 *p = (*pDC)[y];
		if (!p) continue;
		
		if (pDC->AlphaDC())
		{
		    // Preprocess pixels to make the alpha channel into the 
		    // transparent colour.
		    uint8 *a = (*pDC->AlphaDC())[y];
		    uint8 *e = p + pDC->X();
		    uint8 *o = buf;
		    while (p < e)
		    {
		        if (*a++)
		            *o++ = *p;
		        else
		            *o++ = Back;
		        p++;
		    }
		    LgiAssert(o == buf + Len);
		    p = buf;
		}

		Pixels.Write(p, Len);
	}
	DeleteArray(buf);

	// Compress
	Lzw Encoder;
	Encoder.Meter = Meter;
	if (Encoder.Compress(&Encode, &Pixels))
	{
		uchar Buf[256];
		// write data out
		while ((Len = Encode.GetSize()) > 0)
		{
			int l = min(Len, 255);
			if (Encode.Read(Buf, l))
			{
				c = l;
				Out->Write(&c, 1); // Sub block size
				Out->Write(Buf, l);
			}
		}

		c = 0;
		Out->Write(&c, 1); // Terminator sub block
	}

	// Trailer
	c = 0x3b;
	Out->Write(&c, 1);

	return GFilter::IoSuccess;
}

GdcGif::GdcGif()
{
	ProcessedScanlines = 0;
}
