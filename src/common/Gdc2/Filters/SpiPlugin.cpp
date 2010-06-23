#include <stdio.h>
#include <string.h>
#include <math.h>

#include "Lgi.h"
#include "GToken.h"
#include "GVariant.h"

//char *PluginDir = "D:\\Documents and Settings\\Administrator.FRET\\Desktop\\Susie";
class GdcSpiPluginFactory;

typedef struct PictureInfo {
        long left,top;
        long width;
        long height;
        WORD x_density;
        WORD y_density;
        short colorDepth;
        HLOCAL hInfo;
} PictureInfo;

// Filter
class GdcSpiPlugin : public GFilter
{
	GdcSpiPluginFactory *Factory;
	char Ext[512];

public:
	GdcSpiPlugin(GdcSpiPluginFactory *f);

	Format GetFormat() { return FmtSpi; }
	int OnProgress(int Num, int Denom);
	int GetCapabilites() { return FILTER_CAP_READ; }
	bool ReadImage(GSurface *pDC, GStream *In);
	bool WriteImage(GStream *s, GSurface *pDC);
	bool GetVariant(char *n, GVariant &v, char *a);
};

// Plugin
class SpiPlugin : public GLibrary
{
public:
	SpiPlugin(char *f) : GLibrary(f)
	{
	}

	int GetPluginInfo(int infono, LPSTR buf, int buflen)
	{
		typedef int (PASCAL *_GetPluginInfo)(int infono, LPSTR buf,int buflen);
		_GetPluginInfo p = (_GetPluginInfo) GetAddress("GetPluginInfo");
		return (p) ? p(infono, buf, buflen) : 0;
	}

	int IsSupported(LPSTR filename, DWORD dw)
	{
		typedef int (PASCAL *_IsSupported)(LPSTR filename,DWORD dw);
		_IsSupported p = (_IsSupported) GetAddress("IsSupported");
		return (p) ? p(filename, dw) : 0;
	}

	int	GetPictureInfo(LPSTR buf, long len, unsigned int flag, struct PictureInfo *lpInfo)
	{
		typedef int (PASCAL *_GetPictureInfo)(LPSTR buf,long len,unsigned int flag,struct PictureInfo *lpInfo);
		_GetPictureInfo p = (_GetPictureInfo) GetAddress("GetPictureInfo");
		return (p) ? p(buf, len, flag, lpInfo) : 0;
	}

	int GetPicture(	LPSTR buf,long len,unsigned int flag,
					HANDLE *pHBInfo,HANDLE *pHBm,
					FARPROC lpPrgressCallback,long lData)
	{
		typedef int (PASCAL *_GetPicture)(	LPSTR buf,long len,unsigned int flag,
											HANDLE *pHBInfo,HANDLE *pHBm,
											FARPROC lpPrgressCallback,long lData);
		_GetPicture p = (_GetPicture) GetAddress("GetPicture");
		return (p) ? p(buf, len, flag, pHBInfo, pHBm, lpPrgressCallback, lData) : 0;
	}
};

// Factory
class GdcSpiPluginFactory : public GFilterFactory
{
	friend class GdcSpiPlugin;
	List<SpiPlugin> Plugins;
	SpiPlugin *Current;

public:
	GdcSpiPluginFactory()
	{
		Current = 0;

		// load all the plugins...
		GArray<char*> Ext, Files;
		Ext.Add("*.spi");

		char PluginDir[256];
		LgiGetExePath(PluginDir, sizeof(PluginDir));
		if (PluginDir[strlen(PluginDir)-1] != DIR_CHAR) strcat(PluginDir, DIR_STR);
		strcat(PluginDir, "Plugins");

		if (LgiRecursiveFileSearch(PluginDir, &Ext, &Files))
		{
			for (int i=0; i<Files.Length(); i++)
			{
				SpiPlugin *Lib = new SpiPlugin(Files[i]);
				if (Lib AND
					Lib->IsLoaded())
				{
					Plugins.Insert(Lib);
				}
			}

			Files.DeleteArrays();
		}		
	}

	~GdcSpiPluginFactory()
	{
		Plugins.DeleteObjects();
	}

	bool CheckFile(char *File, int Access, uchar *Hint)
	{
		if (Access == FILTER_CAP_READ)
		{
			for (SpiPlugin *p=Plugins.First(); p; p=Plugins.Next())
			{
				GFile f;
				if (f.Open(File, O_READ))
				{
					if (p->IsSupported(0, (int)f.Handle()))
					{
						Current = p;
						return true;
					}
				}
			}
		}

		return false;
	}

	GFilter *NewObject()
	{
		return new GdcSpiPlugin(this);
	}

} SpiPluginFactory;

// Impl
GdcSpiPlugin::GdcSpiPlugin(GdcSpiPluginFactory *f)
{
	// Init
	Factory = f;
	Ext[0] = 0;

	if (Factory->Plugins.Length() > 0)
	{
		// List all the types we support
		for (SpiPlugin *p=Factory->Plugins.First(); p; p=Factory->Plugins.Next())
		{
			char Type[256];
			if (p->GetPluginInfo(2, Type, sizeof(Type)) > 0)
			{
				GToken T(Type, ";");
				for (int i=0; i<T.Length(); i++)
				{
					char *Dot=strchr(T[i], '.');
					if (Dot)
					{
						if (strlen(Ext)>0) strcat(Ext, ",");
						strcat(Ext, Dot+1);
					}
				}
			}
		}

		strlwr(Ext);
	}
	else
	{
		strcpy(Ext, "*");
	}
}


int PASCAL SpiProgressCallback(int nNum, int nDenom, long lData)
{
	return ((GdcSpiPlugin*)lData)->OnProgress(nNum, nDenom);
}

int GdcSpiPlugin::OnProgress(int Num, int Denom)
{
	return false;
}

bool GdcSpiPlugin::ReadImage(GSurface *pDC, GStream *In)
{
	bool Status = false;

	if (pDC && In && Factory->Current)
	{
		int64 Len = In->GetSize();
		char *Buf = new char[Len];
		if (Buf)
		{
			In->Read(Buf, Len);

			PictureInfo Info;
			ZeroObj(Info);
			if (Factory->Current->GetPictureInfo(Buf, Len, 1, &Info) == 0)
			{
				HANDLE hInfo = 0;
				HANDLE hBmp = 0;
				if (Factory->Current->GetPicture(Buf, Len, 1, &hInfo, &hBmp, (FARPROC)SpiProgressCallback, (int)this) == 0)
				{
					GMem InfoMem(hInfo);
					GMem BmpMem(hBmp);

					BITMAPINFO *Info = (BITMAPINFO*) InfoMem.Lock();
					char *Bits = (char*) BmpMem.Lock();
					if (Info AND
						Bits AND
						(Info->bmiHeader.biCompression == BI_RGB OR Info->bmiHeader.biCompression == BI_BITFIELDS))
					{
						if (pDC->Create(Info->bmiHeader.biWidth, Info->bmiHeader.biHeight, max(Info->bmiHeader.biPlanes * Info->bmiHeader.biBitCount, 8)))
						{
							int Colours = 0;
							char *Source = (char*) Info->bmiColors;

							// do palette
							if (pDC->GetBits() <= 8)
							{
								if (Info->bmiHeader.biClrUsed > 0)
								{
									Colours = Info->bmiHeader.biClrUsed;
								}
								else
								{
									Colours = 1 << pDC->GetBits();
								}

								GPalette *Pal = new GPalette(NULL, Colours);
								if (Pal)
								{
									GdcRGB *d = (*Pal)[0];
									RGBQUAD *s = (RGBQUAD*) Source;
									if (d)
									{
										for (int i=0; i<Colours; i++, d++, s++)
										{
											d->R = s->rgbRed;
											d->G = s->rgbGreen;
											d->B = s->rgbBlue;
											d->Flags = s->rgbReserved;
										}
									}
									Source = (char*) s;
									pDC->Palette(Pal);
								}
							}

							if (Info->bmiHeader.biCompression == BI_BITFIELDS)
							{
								Source += sizeof(DWORD) * 3;
							}

							// do pixels
							int Bytes = BMPWIDTH(pDC->X() * pDC->GetBits());

							int Size = BmpMem.GetSize();
							LgiAssert(Size > (Bytes * pDC->Y()));

							Source = Bits;

							for (int y=pDC->Y()-1; y>=0; y--)
							{
								uchar *d = (*pDC)[y];
								if (d)
								{
									memcpy(d, Source, Bytes);
								}
								Source += Bytes;
							}

							Status = true;
						}
					}
				}
			}
		}
	}

	return Status;
}

bool GdcSpiPlugin::GetVariant(char *n, GVariant &v, char *a)
{
	if (!stricmp(n, LGI_FILTER_TYPE))
	{
		if (Factory AND Factory->Plugins.Length() > 0)
		{
			v = "Spi Plugin";
		}
		else
		{
			v = "No Spi Plugins";
		}
	}
	else if (!stricmp(n, LGI_FILTER_EXTENSIONS))
	{
		v = Ext;
	}
	else return false;

	return true;
}

bool GdcSpiPlugin::WriteImage(GStream *Out, GSurface *pDC)
{
	return false;
}

