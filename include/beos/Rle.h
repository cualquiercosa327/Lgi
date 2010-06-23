

#define GDC_RLE_COLOUR				0x0001
#define GDC_RLE_MONO				0x0002
#define GDC_RLE_READONLY			0x0004

class GdcRleDC : public GdcDibSection {
protected:
	int	Sx;
	int	Sy;
	int	Bits;

	int	Flags;
	int	Length;
	int	Alloc;
	uchar	*Data;
	uchar	**ScanLine;

	BOOL SetLength(int Len);
	BOOL FindScanLines();
	void Empty();

public:
	GdcRleDC();
	virtual ~GdcRleDC();

	BOOL Create(int x, int y, int Bits, int LineLen = 0);
	BOOL CreateInfo(int x, int y, int Bits);

	void Update(int Flags);
	void Draw(GSurface *Dest, int x, int y);
	BOOL Read(GFile &F);
	BOOL Write(GFile &F);
};

