class TtfTable;
class GdcTtf;

class LgiClass GdcFontMetrics {
public:
	long tmHeight; 
	long tmAscent; 
	long tmDescent; 
	long tmInternalLeading; 
	long tmExternalLeading; 
	long tmAveCharWidth; 
	long tmMaxCharWidth; 
	long tmWeight; 
	long tmOverhang; 
	long tmDigitizedAspectX; 
	long tmDigitizedAspectY; 
	char tmFirstChar; 
	char tmLastChar; 
	char tmDefaultChar; 
	char tmBreakChar; 
	char tmItalic; 
	char tmUnderlined; 
	char tmStruckOut; 
	char tmPitchAndFamily; 
	char tmCharSet; 
};

class LgiClass GTypeFace {
protected:
	uint Flags;			// FNT_xxx flags
	uint Height; 
	uint Width; 
	uint Escapement; 
	uint Orientation; 
	uint Weight; 
	uchar CharSet; 
	uchar OutPrecision; 
	uchar ClipPrecision; 
	uchar Quality; 
	uchar PitchAndFamily; 
	char FaceName[33];

	// Output
	COLOUR ForeCol;
	COLOUR BackCol;
	bool Trans;

	int TabSize;

public:
	GTypeFace()
	{
		ForeCol = 0;
		BackCol = 0xFFFFFF;
		Trans = FALSE;
		TabSize = 0;
	}
	
	virtual ~GTypeFace() {}

	virtual void Colour(COLOUR Fore, COLOUR Back = 0xFFFFFFFF) { ForeCol = Fore; BackCol = Back; }
	void Fore(COLOUR f) { ForeCol = f; }
	COLOUR Fore() { return ForeCol; }
	void Back(COLOUR b) { BackCol = b; }
	COLOUR Back() { return BackCol; }
	void Transparent(bool t) { Trans = t; }

	void SetTabs(int tabsize)
	{
		TabSize = tabsize;
	}

	virtual bool Load(GFile &F) { return FALSE; }
	virtual bool Save(GFile &F) { return FALSE; }
};

typedef unsigned long				TTF_FIXED;
typedef signed short				TTF_FWORD;
typedef unsigned short				TTF_UFWORD;
typedef signed short				TTF_F2DOT14;

class LgiClass TtfFileHeader {
public:
	TTF_FIXED Version;			// 0x00010000 for version 1.0.
	ushort	NumTables;			// Number of tables. 
	ushort	SearchRange;			// (Maximum power of 2 £ numTables) x 16.
	ushort	EntrySelector;			// Log2(maximum power of 2 £ numTables).
	ushort	RangeShift;			// NumTables x 16-searchRange.

	bool Read(GFile &F);
	bool Write(GFile &F);
	void Dump();
};

class LgiClass TtfTable {
public:
	char	Tag[4];				// 4-byte identifier.
	ulong	CheckSum;			// CheckSum for this table.
	ulong	Offset;				// Offset from beginning of TrueType font file.
	ulong	Length;				// Length of this table.

	void	*Table;

	virtual bool Read(GFile &F);
	virtual bool Write(GFile &F);
	virtual void Dump();
};

class LgiClass TtfObj {
public:
	GdcTtf *Parent;
	TtfObj() { Parent = 0; }
	void *FindTag(char *t);
};

class LgiClass TtfHeader : public TtfObj {
public:
	ulong	Version;			// 0x00010000 for version 1.0.
	ulong	Revision;			// Set by font manufacturer.
	ulong	CheckSumAdjustment;		// To compute: set it to 0, sum the entire font as ULONG, then store 0xB1B0AFBA - sum.
	ulong	MagicNumber;			// Set to 0x5F0F3CF5.
	ushort	Flags;				// 0x0001 - baseline for font at y=0
						// 0x0002 - left sidebearing at x=0
						// 0x0004 - instructions may depend on point size
						// 0x0008 - force ppem to integer values for all internal scaler math, may use fractional ppem sizes if this bit is clear
						// 0x0010 - instructions may alter advance width (the advance widths might not scale linearly)
						// Note: All other bits must be zero.
	ushort	UnitsPerEm;			// Valid range is from 16 to 16384
	quad	InternationalDate;		// (8-byte field).
	quad	Modified;			// International date (8-byte field).
	TTF_FWORD xMin;				// For all glyph bounding boxes.
	TTF_FWORD yMin;				// For all glyph bounding boxes.
	TTF_FWORD xMax;				// For all glyph bounding boxes.
	TTF_FWORD yMax;				// For all glyph bounding boxes.
	ushort	MacStyle;			// 0x0001 - bold (if set to 1)
						// 0x0002 - italic (if set to 1)
						// Bits 2-15 reserved (set to 0).
	ushort	LowestRecPPEM;			// Smallest readable size in pixels.
	short	FontDirectionHint;		// 0   Fully mixed directional glyphs
						// 1   Only strongly left to right
						// 2   Like 1 but also contains neutrals
						// -1   Only strongly right to left
						// -2   Like -1 but also contains neutrals.
	short	IndexToLocFormat;		// 0 for short offsets
						// 1 for long.
	short	GlyphDataFormat;		// 0 for current format.

	bool Read(GFile &F);
	bool Write(GFile &F);
	void Dump();
};

class LgiClass TtfMaxProfile : public TtfObj {
public:
	ulong	Version;			// 0x00010000 for version 1.0.
	ushort	NumGlyphs;			// The number of glyphs in the font.
	ushort	MaxPoints;			// Maximum points in a non-composite glyph.
	ushort	MaxContours;			// Maximum contours in a non-composite glyph.
	ushort	MaxCompositePoints;		// Maximum points in a composite glyph.
	ushort	MaxCompositeContours;		// Maximum contours in a composite glyph.
	ushort	MaxZones;			// 1 if instructions do not use the twilight zone (Z0), or 2 if instructions do use Z0; should be set to 2 in most cases.
	ushort	MaxTwilightPoints;		// Maximum points used in Z0.
	ushort	MaxStorage;			// Number of Storage Area locations. 
	ushort	MaxFunctionDefs;		// Number of FDEFs.
	ushort	MaxInstructionDefs;		// Number of IDEFs.
	ushort	MaxStackElements;		// Maximum stack depth .
	ushort	MaxSizeOfInstructions;		// Maximum byte count for glyph instructions.
	ushort	MaxComponentElements;		// Maximum number of components referenced at "top level" for any composite glyph.
	ushort	MaxComponentDepth;		// Maximum levels of recursion; 1 for simple components.

	bool Read(GFile &F);
	bool Write(GFile &F);
	void Dump();
};

class LgiClass TtfLocation : public TtfObj {

	int Entries;
	int Type;
	void *Data;

public:
	TtfLocation();
	virtual ~TtfLocation();

	int operator [](int i);
	bool Read(GFile &F);
	bool Write(GFile &F);
	void Dump();
};

class LgiClass DataList {

	int Alloc;

public:
	int Size;
	double *Data;

	DataList()
	{
		Size = 0;
		Alloc = 0;
		Data = 0;
	}

	virtual ~DataList()
	{
		DeleteArray(Data);
	}

	void AddValue(double n)
	{
		if (Size >= Alloc)
		{
			int NewAlloc = (Size + 16) & (~0xF);
			double *Temp = NEW(double[NewAlloc]);
			if (Temp)
			{
				memcpy(Temp, Data, sizeof(double)*Size);
				DeleteArray(Data);
				Data = Temp;
				Alloc = NewAlloc;
			}
			else
			{
				return;
			}
		}

		if (Size > 0)
		{
			for (int i=0; i<Size; i++)
			{
				if (Data[i] > n)
				{
					memmove(Data + i + 1, Data + i, sizeof(double)*(Size-i));
					Data[i] = n;
					Size++;
					return;
				}
			}
			Data[Size] = n;
		}
		else
		{
			Data[0] = n;
		}

		Size++;
	}
};

class LgiClass TtfContour {

	class CPt {
	public:
		double x, y;
	};

	// From glyph
	int *x, *y;
	uchar *Flags;
	int XOff, YOff;
	
	// Out stuff
	int Points;
	int Alloc;
	CPt *Point;

	bool SetPoints(int p);
	void Bezier(CPt *p, double Threshold = 0.5);

public:
	TtfContour();
	~TtfContour();

	void Setup(int *x, int *y, uchar *Flags, int MinX, int MinY);
	bool Create(int Pts, double XScale, double YScale);
	bool RasterX(int Size, DataList *List);
	bool RasterY(int Size, DataList *List);

	void DebugDraw(GSurface *pDC, int Sx, int Sy);
};

class LgiClass TtfGlyph : public TtfObj {

	int Points;
	uchar *Temp;

public:
	TtfGlyph();
	~TtfGlyph();

	short	Contours;			// If  the number of contours is greater than or equal to zero,
						// this is a single glyph, if negative, this is a composite glyph.
	TTF_FWORD xMin;				// Minimum x for coordinate data.
	TTF_FWORD yMin;				// Minimum y for coordinate data.
	TTF_FWORD xMax;				// Maximum x for coordinate data.
	TTF_FWORD yMax;				// Maximum y for coordinate data.

	// simple glyph
	ushort	*EndPtsOfContours;		// Array of last points of each contour; n  is the number of contours.
	ushort	InstructionLength;		// Total number of bytes for instructions.
	uchar	*Instructions;			// Array of instructions for each glyph; n  is the number of instructions.
	uchar	*Flags;				// Array of flags for each coordinate in outline; n  is the number of flags.
	int	*X;				// First coordinates relative to (0,0)
	int	*Y;				// others are relative to previous point.

	// complex glyph
	// yeah right... later i think... maybe

	int GetX() { return xMax - xMin; }
	int GetY() { return yMax - yMin; }

	bool Read(GFile &F);
	bool Write(GFile &F);
	void Dump();
	void Draw(GSurface *pDC, int x, int y, int Scale);
	int DrawEm(GSurface *pDC, int X, int Y, int EmUnits, double PixelsPerEm);
	bool Rasterize(	GSurface *pDC,
			GRect *pDest,
			double xppem,
			double yppem,
			int BaseLine);
};

class LgiClass TtfMap {
public:
	TtfMap() {}
	virtual ~TtfMap() {}
	virtual int operator[](int i) { return 0; }
	virtual bool Read(GFile &F) { return FALSE; }
	virtual bool Write(GFile &F) { return FALSE; }
	virtual void Dump() {}
};

class LgiClass TtfCMapTable {
public:
	TtfCMapTable();
	~TtfCMapTable();

	ushort	PlatformID;
	ushort	EncodingID;
	ulong	Offset;

	ushort Format;
	TtfMap	*Map;

	bool Read(GFile &F);
	bool Write(GFile &F);
	void Dump();
};

class LgiClass TtfCMapByteEnc : public TtfMap {

	ushort	Format;				// set to 0
	ushort	Length;				// length in bytes of the subtable
	ushort	Version;
	uchar	Map[256];

public:
	int operator[](int i);
	bool Read(GFile &F);
	bool Write(GFile &F);
	void Dump();
};

class LgiClass TtfCMapHighByte : public TtfMap {

	class SubHeader {
	public:
		ushort	FirstCode;		// First valid low byte for this subHeader
		ushort	EntryCount;		// Number of valid low bytes for this subHeader
		short	IdDelta;
		ushort	IdRangeOffset;
	};

	ushort	Format;				// set to 2
	ushort	Length;
	ushort	Version;
	ushort	subHeaderKeys[256];		// Array that maps high bytes to subHeaders:
						// value is subHeader index * 8.
	SubHeader *Header;
	ushort	*GlyphIndexArray;

public:
	int operator[](int i) { return 0; }
	bool Read(GFile &F) { return FALSE; }
	bool Write(GFile &F) { return FALSE; }
	void Dump() {}
};

class LgiClass TtfCMapSegDelta : public TtfMap {

	int SegCount;
	int IdCount;

	ushort	Format;				// set to 4
	ushort	Length;
	ushort	Version;
	ushort	SegCountX2;			// 2 x segCount.
	ushort	SearchRange;		// 2 x (2**floor(log2(segCount)))
	ushort	EntrySelector;		// log2(searchRange/2)
	ushort	RangeShift;			// 2 x segCount - searchRange
	ushort	*EndCount;			// End characterCode for each segment,last =0xFFFF.
	ushort	*StartCount;		// Start character code for each segment.
	short	*IdDelta;			// Delta for all character codes in segment.
	ushort	*IdRangeOffset;		// Offsets into glyphIdArray or 0
								// the GlyphIdArray is appended to the
								// end of the IdRangeOffset array

public:
	TtfCMapSegDelta();
	~TtfCMapSegDelta();

	int operator[](int i);
	bool Read(GFile &F);
	bool Write(GFile &F);
	void Dump();
};

class LgiClass TtfCMap : public TtfObj {

	TtfMap *Fravorite;

public:
	ushort	Version;
	ushort	Tables;
	TtfCMapTable *Table;

	TtfCMap();
	~TtfCMap();
	int operator[](int i) { return (Fravorite) ? (*Fravorite)[i] : 0; }
	bool Read(GFile &F);
	bool Write(GFile &F);
	void Dump();
};

class LgiClass TtfRaster : public TtfObj {

	friend class GSurface;

	int XPixelsPerEm;
	int YPixelsPerEm;
	int Glyphs;

public:
	int *BaseLine;			// pixels down to baseline in bitmap
	GRect *pSource;		// where glyph is stored in the bitmap
	GSurface *pDC;		// the bitmap

	TtfRaster();
	~TtfRaster();

	double GetXPixelsPerEm() { return XPixelsPerEm; }
	double GetYPixelsPerEm() { return YPixelsPerEm; }
	bool Rasterize(double xPPEm, double yPPEm, int OverSample);
	int DrawChar(GSurface *pDC, int x, int y, int Char);
};

class LgiClass GdcTtf : public GTypeFace {

	friend class TtfObj;

protected:
	int Tables;
	TtfTable *TableList;
	TtfTable *FindTag(char *t);
	TtfTable *SeekTag(char *t, GFile *F);

	TtfHeader Header;
	TtfMaxProfile Profile;
	TtfLocation Location;
	TtfGlyph **Glyph;
	TtfCMap Map;

	int Rasters;
	TtfRaster **Raster;
	TtfRaster *FindRaster(int Point, int XDpi = 96, int YDpi = -1);

public:
	GdcTtf();
	virtual ~GdcTtf();

	virtual bool Load(GFile &F);
	virtual bool Save(GFile &F);
	virtual bool Rasterize(	int Point,
							int StyleFlags,
							int OverSample = 2,
							int XDpi = 96,
							int YDpi = -1);

	void Test(GSurface *pDC);
	virtual void Size(int *x, int *y, char *Str, int Len = -1, int Flags = 0);
	int X(char *Str, int Len = -1, int Flags = 0);
	int Y(char *Str, int Len = -1, int Flags = 0);

	// Text drawing functions
	virtual bool SelectPoint(int Pt) { return FALSE; }
	virtual void Text(GSurface *pDC, int x, int y, char *Str, int Len = -1);
};

#define LFONT_LIGHT					FW_LIGHT
#define LFONT_NORMAL				FW_NORMAL
#define LFONT_BOLD					FW_BOLD

class LgiClass GFont : public GdcTtf {

	HFONT	hFont;
	float	*Widths;

	void SetWidths();

public:
	GFont();
	~GFont();

	HFONT Handle() { return hFont; }

	bool Load(GFile &F) { return TRUE; }
	bool Save(GFile &F) { return FALSE; }

	bool Create(char *Face,
				int Height,
				int Width = 0,
				int Weight = LFONT_NORMAL,
				uint32 Italic = false,
				uint32 Underline = false,
				uint32 StrikeOut = false,
				uint32 CharSet = ANSI_CHARSET,
				int Escapement = 0,
				int Orientation = 0,
				uint32 OutputPrecision = OUT_DEFAULT_PRECIS,
				uint32 ClipPrecision = CLIP_DEFAULT_PRECIS,
				uint32 Quality = ANTIALIASED_QUALITY,
				uint32 PitchAndFamily = DEFAULT_PITCH);

	bool CreateFont(LOGFONT *LogFont);

	void Text(	GSurface *pDC,
				int x, int y,
				char *Str,
				int Len = -1,
				GRect *r = NULL); // ASCII version
	void Size(int *x, int *y, char *Str, int Len = -1, int Flags = 0);
	int CharAt(int x, char *Str, int Len = -1);
};



