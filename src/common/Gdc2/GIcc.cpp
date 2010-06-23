/**
	\file
	\author Matthew Allen
	\brief Colour management class
*/

/*
	This class requires the little cms library to do the underlying
	work of colour conversion. It does however partially parse the
	ICC profile data and extract the information into a DOM structure
	to allow some sort of access. If you don't have little cms then
	change USE_LCMS to 0.

	If you want to add support for a new tag, first implement a structure
	to mimic the format of the tag like the struct IccDescTag. Then add
	a case for the tag to TagDom::TagDom(...) that instances a new DOM
	object to display the contents of your struct.
*/

#ifdef WIN32
#define USE_LCMS				0
#else
#define USE_LCMS				0
#endif

#include <stdio.h>
#include "Lgi.h"
#include "GIcc.h"

#if USE_LCMS
#include "lcms.h"
#endif

///////////////////////////////////////////////////////////////////////////////
uint32 Swap32(uint32 i)
{
	#if LGI_LITTLE_ENDIAN
	return	((i & 0xff000000) >> 24) |
			((i & 0x00ff0000) >> 8) |
			((i & 0x0000ff00) << 8) |
			((i & 0x000000ff) << 24);
	#else
	return i;
	#endif
}

uint16 Swap16(uint16 i)
{
	#if LGI_LITTLE_ENDIAN
	return	((i & 0xff00) >> 8) |
			((i & 0x00ff) << 8);
	#else
	return i;
	#endif
}

enum IccColourSpace
{
	IccCsXYZ			= 0x58595A20, // 'XYZ '
	IccCslab			= 0x4C616220, // 'Lab '
	IccCsluv			= 0x4C757620, // 'Luv '
	IccCsYCbCr			= 0x59436272, // 'YCbr'
	IccCsYxy			= 0x59787920, // 'Yxy '
	IccCsRgb			= 0x52474220, // 'RGB '
	IccCsGray			= 0x47524159, // 'GRAY'
	IccCsHsv			= 0x48535620, // 'HSV '
	IccCsHls			= 0x484C5320, // 'HLS '
	IccCsCmyk			= 0x434D594B, // 'CMYK'
	IccCsCmy			= 0x434D5920, // 'CMY '
	IccCs2Colour		= 0x32434C52, // '2CLR'
	IccCs3Colour		= 0x33434C52, // '3CLR'
	IccCs4Colour		= 0x34434C52, // '4CLR'
	IccCs5Colour		= 0x35434C52, // '5CLR'
	IccCs6Colour		= 0x36434C52, // '6CLR'
	IccCs7Colour		= 0x37434C52, // '7CLR'
	IccCs8Colour		= 0x38434C52, // '8CLR'
	IccCs9Colour		= 0x39434C52, // '9CLR'
	IccCs10Colour		= 0x41434C52, // 'ACLR'
	IccCs11Colour		= 0x42434C52, // 'BCLR'
	IccCs12Colour		= 0x43434C52, // 'CCLR'
	IccCs13Colour		= 0x44434C52, // 'DCLR'
	IccCs14Colour		= 0x45434C52, // 'ECLR'
	IccCs15Colour		= 0x46434C52, // 'FCLR'
};

enum IccProfileClass
{
	IccProfileInputDevice			= 0x73636E72, // 'scnr'
	IccProfileDisplay				= 0x6D6E7472, // 'mntr'
	IccProfileOutput				= 0x70727472, // 'prtr'
	IccProfileDeviceLink			= 0x6C696E6B, // 'link'
	IccProfileColorSpaceConversion	= 0x73706163, // 'spac'
	IccProfileAbstract				= 0x61627374, // 'abst'
	IccProfileNamed					= 0x6E6D636C, // 'nmcl'
};

struct S15Fixed16
{
	int16 i;
	uint16 f;

	double d()
	{
		return i + ((double)f/0xffff);
	}
};

struct U16Fixed16
{
	uint16 i;
	uint16 f;

	double d()
	{
		return i + ((double)f/0xffff);
	}
};

struct U1Fixed15
{
	uint16 i:1;
	uint16 f:15;

	double d()
	{
		return i + ((double)f/0x7fff);
	}
};

struct U8Fixed8
{
	uint8 i;
	uint8 f;

	double d()
	{
		return i + ((double)f/0xff);
	}
};

struct IccDateTime
{
	uint16 Year;
	uint16 Month;
	uint16 Day;
	uint16 Hour;
	uint16 Minute;
	uint16 Second;
};

struct IccResponse16Number
{
	uint16 Number;
	uint16 Reserved;
	S15Fixed16 Value;
};

struct IccXYZ
{
	S15Fixed16 x;
	S15Fixed16 y;
	S15Fixed16 z;
};

struct IccProfileHeader
{
	uint32 ProfileSize;
	uint32 PreferedCmmType;
	uint32 Version;
	IccProfileClass Class;
	IccColourSpace InputSpace;
	uint32 ConnectionSpace;
	IccDateTime CreationDate;
	uint32 Magic; // 'acsp'
	uint32 PlatformSig;
	uint32 Flags;
	uint32 DeviceManufacturer;
	uint32 DeviceModel;
	uint8 DeviceAttributes[8];
	uint32 RenderingIntent;
	IccXYZ PcsD50;
	uint32 CreatorSig;
	char ProfileId[16];

	uint8 Reserved[28];
};

struct IccTag
{
	uint32 Sig;
	uint32 Offset;
	uint32 Size;
};

struct IccTagTable
{
	uint32 Tags;
	IccTag Tag[1];
};

struct IccLocalString
{
	uint16 Lang;
	uint16 Country;
	uint32 Len;
	uint32 Offset;
};

struct IccMultiLocalUnicode
{
	uint32 Sig;
	uint32 Reserved;
	uint32 Count;
	uint32 RecordSize;
	IccLocalString Str[1];
};

struct IccDescTag
{
	uint32 Sig;
	uint32 Reserved;
	uint32 Size;
	char String[1];

	bool IsOk() { return Swap32(Sig) == 'desc'; }
};

struct NamedColour
{
	char Name[32];
	uint16 PcsCoords[3];
	uint16 Coords[1]; // variable

	NamedColour *Next(int Ch)
	{
		return (NamedColour*) ( ((char*)this) + sizeof(NamedColour) + ((Ch-1) * sizeof(uint16)) );
	}
};

struct IccNameColourTag
{
	uint32 Sig;
	uint32 Reserved;
	uint32 Flags;
	uint32 Count;
	uint32 Coords;
	char Prefix[32];
	char Suffix[32];
	NamedColour Colours[1];

	bool IsOk() { return Swap32(Sig) == 'ncl2'; }
};

struct IccTextTag
{
	uint32 Sig;
	uint32 Reserved;
	char String[1];

	bool IsOk() { return Swap32(Sig) == 'text'; }
};

struct IccCurve
{
	uint32 Sig;
	uint32 Reserved;
	uint32 Count;
	uint16 Values[1];

	bool IsOk() { return Swap32(Sig) == 'curv'; }
};

struct IccXYZTag
{
	uint32 Sig;
	uint32 Reserved;
	IccXYZ Values[1];

	bool IsOk() { return Swap32(Sig) == 'XYZ '; }
};

class ValueDom : public GDom
{
	char *Txt;

public:
	ValueDom(uint32 i, char *name)
	{
		char s[256];
		uint32 is = i;
		sprintf(s, "%s: %i (%04.4s)", name, Swap32(i), &is);
		Txt = NewStr(s);
	}

	ValueDom(IccProfileClass i, char *name)
	{
		char s[256];
		uint32 is = i;
		sprintf(s, "%s: %i (%04.4s)", name, Swap32(i), &is);
		Txt = NewStr(s);
	}

	ValueDom(IccColourSpace i, char *name)
	{
		char s[256];
		uint32 is = i;
		sprintf(s, "%s: %i (%04.4s)", name, Swap32(i), &is);
		Txt = NewStr(s);
	}	

	ValueDom(uint16 i, char *name)
	{
		char s[256];
		sprintf(s, "%s: %i", name, Swap16(i));
		Txt = NewStr(s);
	}

	ValueDom(char *str, char *name)
	{
		char s[256] = "(error)";
		if (name AND str)
			sprintf(s, "%s: %s", name, str);
		else if (name)
			strcpy(s, name);
		else if (str)
			strcpy(s, str);
		Txt = NewStr(s);
	}

	ValueDom(IccDateTime &d, char *name)
	{
		char s[256];
		sprintf(s, "%s: %i/%i/%i %i:%i:%i", name,
			Swap16(d.Day),
			Swap16(d.Month),
			Swap16(d.Year),
			Swap16(d.Hour),
			Swap16(d.Minute),
			Swap16(d.Second));
		Txt = NewStr(s);
	}

	~ValueDom()
	{
		DeleteArray(Txt);
	}

	bool GetVariant(char *Name, GVariant &Value, char *Array)
	{
		if (!Name)
			return false;

		if (stricmp(Name, "Text") == 0)
		{
			Value = Txt;
		}
		else return false;

		return true;
	}
};

#define DomAdd(fld) \
	Dom.Add(new ValueDom(h->fld, #fld));

class LocalStringDom : public GDom
{
	char *Base;
	IccLocalString *Str;

public:
	LocalStringDom(char *b, IccLocalString *s)
	{
		Base = b;
		Str = s;
	}

	bool GetVariant(char *Name, GVariant &Value, char *Array)
	{
		if (!Name)
			return false;

		if (stricmp(Name, "Text") == 0)
		{
			int Off = Swap32(Str->Offset);
			int Len = Swap32(Str->Len);
			char16 *u = NewStrW((char16*)(Base+Off), Len);
			if (!u)
				return false;

			for (char16 *p = u; *p; p++)
				*p = Swap16(*p);

			char *u8 = LgiNewUtf16To8(u);
			DeleteArray(u);

			char s[512];
			sprintf(s, "'%02.2s' = '%s'", &Str->Lang, u8);
			Value = "Localized Unicode";
		}
		else return false;

		return true;
	}
};

class LocalUnicodeDom : public GDom
{
	IccMultiLocalUnicode *h;
	GArray<GDom*> Dom;

public:
	LocalUnicodeDom(IccMultiLocalUnicode *header)
	{
		h = header;
		if (Swap32(h->Sig) == 'mluc')
		{
			for (int i=0; i<Swap32(h->Count); i++)
			{
				Dom.Add(new LocalStringDom((char*)header, h->Str + i));
			}
		}
		else
		{
			Dom.Add(new ValueDom("Incorrect signature", "Error"));
		}
	}

	~LocalUnicodeDom()
	{
		Dom.DeleteObjects();
	}

	bool GetVariant(char *Name, GVariant &Value, char *Array)
	{
		if (!Name)
			return false;

		if (stricmp(Name, "Children") == 0)
		{
			Value.SetList();
			for (int i=0; i<Dom.Length(); i++)
			{
				Value.Value.Lst->Insert(new GVariant(Dom[i]));
			}
		}
		else if (stricmp(Name, "Text") == 0)
		{
			Value = "Localized Unicode";
		}
		else return false;

		return true;
	}
};

class TagDom : public GDom
{
	IccTag *h;
	GArray<GDom*> Dom;
	char *Txt;

public:
	TagDom(IccTag *tag, char *header);
	~TagDom();
	bool GetVariant(char *Name, GVariant &Value, char *Array);
};

class HeaderDom : public GDom
{
	IccProfileHeader *h;
	GArray<GDom*> Dom;

public:
	HeaderDom(IccProfileHeader *header)
	{
		h = header;

		DomAdd(ProfileSize);
		DomAdd(PreferedCmmType);
		DomAdd(Version);
		DomAdd(Class);
		DomAdd(InputSpace);
		DomAdd(ConnectionSpace);
		DomAdd(CreationDate);
		DomAdd(PlatformSig);
		DomAdd(Flags);
		DomAdd(DeviceManufacturer);
		DomAdd(DeviceModel);
		DomAdd(RenderingIntent);
		DomAdd(CreatorSig);
		// DomAdd(ProfileId);
	}

	~HeaderDom()
	{
		Dom.DeleteObjects();
	}

	bool GetVariant(char *Name, GVariant &Value, char *Array)
	{
		if (!Name)
			return false;

		if (stricmp(Name, "Children") == 0)
		{
			Value.SetList();
			for (int i=0; i<Dom.Length(); i++)
			{
				Value.Value.Lst->Insert(new GVariant(Dom[i]));
			}
		}
		else if (stricmp(Name, "Text") == 0)
		{
			Value = "Header";
		}
		else return false;

		return true;
	}
};

class CurveDom : public GDom
{
	IccCurve *c;

public:
	CurveDom(IccCurve *curve)
	{
		c = curve;
	}

	bool GetVariant(char *Name, GVariant &Value, char *Array)
	{
		if (!Name)
			return false;

		if (stricmp(Name, "Text") == 0)
		{
			char s[256];
			int Count = Swap32(c->Count);
			if (Count == 0)
			{
				sprintf(s, "Identity Curve");
			}
			else if (Count == 1)
			{
				U8Fixed8 p = *((U8Fixed8*)c->Values);
				sprintf(s, "Gamma Curve: %f", p.d());
			}
			else
			{
				sprintf(s, "Parametric Curve with %i points", Count);
			}

			Value = s;
		}
		else return false;

		return true;
	}
};

class XyzDom : public GDom
{
	IccXYZTag *x;
	int Len;

public:
	XyzDom(IccXYZTag *xyz, int len)
	{
		x = xyz;
		Len = Swap32(len);
	}

	bool GetVariant(char *Name, GVariant &Value, char *Array)
	{
		if (!Name)
			return false;

		if (stricmp(Name, "Text") == 0)
		{
			char s[1024] = "";

			char *p = s;
			IccXYZ *e = (IccXYZ*) (((char*)x) + Len);
			for (IccXYZ *v = x->Values; v < e; v++)
			{
				if (p > s + sizeof(s) - 64)
					break;
				sprintf(p, "%f,%f,%f ", v->x.d(), v->y.d(), v->z.d());
				p += strlen(p);
			}

			Value = s;
		}
		else return false;

		return true;
	}
};

class ChildDom : public GDom
{
public:
	GArray<GDom*> Dom;
	char *Txt;

	ChildDom()
	{
		Txt = 0;
	}

	~ChildDom()
	{
		DeleteArray(Txt);
	}

	bool GetVariant(char *Name, GVariant &Value, char *Array)
	{
		if (!Name)
			return false;

		if (stricmp(Name, "Text") == 0)
		{
			Value = Txt;
		}
		else if (stricmp(Name, "Children") == 0)
		{
			Value.SetList();
			for (int i=0; i<Dom.Length(); i++)
			{
				Value.Value.Lst->Insert(new GVariant(Dom[i]));
			}
		}
		else if (stricmp(Name, "Expand") == 0)
		{
			Value = false;
		}
		else return false;

		return true;
	}
};

class NclDom : public ChildDom
{
	IccNameColourTag *n;
	int Len;

public:
	NclDom(IccNameColourTag *named, int len)
	{
		n = named;
		n->Count = Swap32(n->Count);
		n->Coords = Swap32(n->Coords);
		Len = Swap32(len);

		char s[1024] = "";
		sprintf(s, "Named colour count: %i", n->Count);
		Txt = NewStr(s);

		NamedColour *c = n->Colours;		
		int Block = 100;
		for (int i=0; i<n->Count; i+=Block)
		{
			ChildDom *v = new ChildDom;
			if (v)
			{
				int End = min(i + Block - 1, n->Count - 1);

				Dom.Add(v);
				sprintf(s, "%i-%i", i, End);
				v->Txt = NewStr(s);

				for (int k=0; k<Block AND i+k<End; k++)
				{
					sprintf(s,
							"%s [%.1f,%.1f,%.1f]",
							c->Name,
							(double)Swap16(c->PcsCoords[0]) / 0xffff,
							(double)Swap16(c->PcsCoords[1]) / 0xffff,
							(double)Swap16(c->PcsCoords[2]) / 0xffff);
					v->Dom.Add(new ValueDom(s, "Name"));
					
					c = c->Next(n->Coords);
				}
			}
		}
	}
};

TagDom::TagDom(IccTag *tag, char *header)
{
	Txt = 0;
	h = tag;
	DomAdd(Offset);
	DomAdd(Size);

	int Off = Swap32(h->Offset);
	uint32 *Ptr = (uint32*) (header + Off);
	switch (Swap32(h->Sig))
	{
		case 'desc':
		{
			if (Swap32(*Ptr) == 'mluc')
			{
				Dom.Add(new LocalUnicodeDom((IccMultiLocalUnicode*)(header + Off )));
			}
			else if (Swap32(*Ptr) == 'desc')
			{
				IccDescTag *Tag = (IccDescTag*) Ptr;
				Dom.Add(new ValueDom(Txt = Tag->String, "Description"));
			}
			break;
		}
		case 'cprt':
		{
			IccTextTag *Tag = (IccTextTag*)Ptr;
			if (Tag->IsOk())
			{
				Dom.Add(new ValueDom(Txt = Tag->String, "Copyright"));
			}
			break;
		}
		case 'rTRC':
		case 'gTRC':
		case 'bTRC':
		{
			IccCurve *c = (IccCurve*) Ptr;
			if (c->IsOk())
			{
				Dom.Add(new CurveDom(c));
			}
			break;
		}
		case 'rXYZ':
		case 'gXYZ':
		case 'bXYZ':
		{
			IccXYZTag *Xyz = (IccXYZTag*) Ptr;
			if (Xyz->IsOk())
			{
				Dom.Add(new XyzDom(Xyz, h->Size));
			}
			break;
		}
		case 'ncl2':
		{
			IccNameColourTag *Ncl = (IccNameColourTag*) Ptr;
			if (Ncl->IsOk())
			{
				Dom.Add(new NclDom(Ncl, h->Size));
			}
			break;
		}
	}
}

TagDom::~TagDom()
{
	Dom.DeleteObjects();
}

bool TagDom::GetVariant(char *Name, GVariant &Value, char *Array)
{
	if (!Name)
		return false;

	if (stricmp(Name, "Children") == 0)
	{
		Value.SetList();
		for (int i=0; i<Dom.Length(); i++)
		{
			Value.Value.Lst->Insert(new GVariant(Dom[i]));
		}
	}
	else if (stricmp(Name, "Text") == 0)
	{
		char s[256];
		sprintf(s, "Tag '%04.4s'", &h->Sig);
		Value = s;
	}
	else if (stricmp(Name, "Expand") == 0)
	{
		Value = (int)(Dom.Length() > 2);
	}
	else if (Txt AND stricmp(Name, "Name") == 0)
	{
		Value = Txt;
	}
	else return false;

	return true;
}

class TagTableDom : public GDom
{
	IccTagTable *t;
	GArray<GDom*> Dom;

public:
	TagTableDom(IccTagTable *table, char *header)
	{
		t = table;
		for (int i=0; i<Swap32(t->Tags); i++)
		{
			Dom.Add(new TagDom(t->Tag + i, header));
		}
	}

	~TagTableDom()
	{
		Dom.DeleteObjects();
	}

	bool GetVariant(char *Name, GVariant &Value, char *Array)
	{
		if (!Name)
			return false;

		if (stricmp(Name, "Children") == 0)
		{
			Value.SetList();
			for (int i=0; i<Dom.Length(); i++)
			{
				Value.Value.Lst->Insert(new GVariant(Dom[i]));
			}
		}
		else if (stricmp(Name, "Text") == 0)
		{
			Value = "Tag Table";
		}
		else if (stricmp(Name, "Name") == 0)
		{
			for (int i=0; i<t->Tags; i++)
			{
				if (Swap32(t->Tag[i].Sig) == 'desc')
				{
					Value = Dom[i];
					break;
				}
			}
		}
		else return false;

		return true;
	}
};

///////////////////////////////////////////////////////////////////////////////
class GIccProfilePrivate
{
public:
	char *Err;
	char *Name;

	int64 Len;
	char *Data;
	#if USE_LCMS
	cmsHPROFILE Profile;
	#endif
	GArray<GDom*> Dom;

	GIccProfilePrivate()
	{
		Err = 0;
		Data = 0;
		Len = 0;
		Name = 0;
		#if USE_LCMS
		Profile = 0;
		#endif
		LgiAssert(sizeof(IccProfileHeader) == 128);
	}

	~GIccProfilePrivate()
	{
		Empty();
	}

	IccProfileHeader *Header()
	{
		return (IccProfileHeader*)Data;
	}

	IccTagTable *TagTable()
	{
		return Data ? (IccTagTable*)(Data + sizeof(IccProfileHeader)) : 0;
	}

	void Empty()
	{
		#if USE_LCMS
		if (Profile)
		{
			cmsCloseProfile(Profile);
			Profile = 0;
		}
		#endif

		Dom.DeleteObjects();
		DeleteArray(Err);
		DeleteArray(Name);
		DeleteArray(Data);
		Len = 0;
	}

	bool GetTag(uint32 Tag, char *&Ptr, int &Size)
	{
		IccTagTable *t = TagTable();
		if (t)
		{
			for (int i=0; i<t->Tags; i++)
			{
				if (t->Tag[i].Sig == Tag)
				{
					Ptr = Data + t->Tag[i].Offset;
					Size = t->Tag[i].Size;
					return true;
				}
			}
		}

		return false;
	}

	void SetErr(char *e)
	{
		DeleteArray(Err);
		Err = NewStr(e);
	}
};

///////////////////////////////////////////////////////////////////////////////
GIccProfile::GIccProfile(char *file)
{
	d = new GIccProfilePrivate;
}

GIccProfile::~GIccProfile()
{
	DeleteObj(d);
}

bool GIccProfile::CreateNamed(char *name)
{
	if (name)
	{
		#if USE_LCMS
		d->Empty();

		if (stricmp(name, "sRGB") == 0)
		{
			d->Profile = cmsCreate_sRGBProfile();
		}

		if (d->Profile)
		{
			return true;
		}
		#endif
	}

	return false;
}

bool GIccProfile::Open(char *file)
{
	GFile f;
	if (file AND f.Open(file, O_READ))
	{
		return Open(&f);
	}
	return false;
}

bool GIccProfile::Open(GStream *Stream)
{
	if (Stream)
	{
		d->Empty();
		d->Len = Stream->GetSize();
		d->Data = new char[d->Len];
		if (d->Data)
		{
			if (Stream->Read(d->Data, d->Len) == d->Len)
			{
				IccProfileHeader *h = d->Header();
				if (Swap32(h->Magic) == 'acsp')
				{
					#if USE_LCMS
					d->Profile = cmsOpenProfileFromMem(d->Data, d->Len);
					#endif

					d->Dom.Add(new HeaderDom(d->Header()));
					d->Dom.Add(new TagTableDom(d->TagTable(), d->Data ));

					GVariant Desc;
					if (GetValue("Children[1].Name.Name", Desc))
					{
						d->Name = NewStr(Desc.Str());
					}

					return true;
				}
				else
				{
					d->SetErr("Not a valid profile.");
				}
			}
			else
			{
				d->SetErr("Failed to read stream.");
			}
		}
		else
		{
			d->SetErr("Mem alloc failed.");
		}
	}
	else
	{
		d->SetErr("Invalid parameter.");
	}
	
	return false;
}

bool GIccProfile::Save(char *file)
{
	GFile f;
	if (f.Open(file, O_WRITE))
	{
		f.SetSize(0);
		return Save(&f);
	}
	return false;
}

bool GIccProfile::Save(GStream *stream)
{
	if (stream)
	{
		if (d->Data AND d->Len > 0)
		{
			return stream->Write(d->Data, d->Len) == d->Len;
		}
	}
	
	return false;
}

char *GIccProfile::GetError()
{
	return d->Err;
}

char *GIccProfile::GetName()
{
	return d->Name;
}

bool GIccProfile::Convert(COLOUR *Out32, COLOUR In32, GIccProfile *Profile)
{
	return false;
}

bool GIccProfile::Convert(GSurface *Dest, GSurface *Src, GIccProfile *Profile)
{
	#if USE_LCMS
	if (!Dest OR !Src OR !Profile)
	{
		d->SetErr("Invalid parameter(s).");
		return false;
	}

	if (Dest->X() != Src->X() OR
		Dest->Y() != Src->Y())
	{
		d->SetErr("Source and Dest images are different sizes.");
		return false;
	}

	if
	(
		NOT
		(
			(Dest->GetBits() == 32 AND Src->GetBits() == 32)
			OR
			(Dest->GetBits() == 24 AND Src->GetBits() == 24)
		)
	)
	{
		d->SetErr("Must be RGB or RGBA images.");
		return false;
	}

	if (!d->Profile)
	{
		d->SetErr("Dest profile error.");
		return false;
	}

	if (!Profile->d->Profile)
	{
		d->SetErr("Src profile error.");
		return false;
	}

	cmsHTRANSFORM t = cmsCreateTransform(	Profile->d->Profile,
											Src->GetBits() == 32 ? TYPE_BGRA_8 : TYPE_BGR_8,
											d->Profile,
											Src->GetBits() == 32 ? TYPE_BGRA_8 : TYPE_BGR_8,
											d->Header() ? d->Header()->RenderingIntent : 0,
											0);
	if (t)
	{
		uchar *Buf = 0;
		int Bytes = 0;
		if (Src == Dest)
		{
			Bytes = (Src->GetBits() / 8) * Src->X();
			Buf = new uchar[Bytes];
		}

		for (int y=0; y<Src->Y(); y++)
		{
			uchar *sp = (*Src)[y];
			uchar *dp = (*Dest)[y];
			if (sp AND dp)
			{
				if (Buf)
				{
					memcpy(Buf, sp, Bytes);
				}

				cmsDoTransform(t, Buf ? Buf : sp, dp, Src->X());
			}
		}

		DeleteArray(Buf);
		cmsDeleteTransform(t);

		return true;
	}
	else
	{
		d->SetErr("Couldn't create colour transform.");
	}

	#else

	d->SetErr("LCMS support not compiled in.");

	#endif
	return false;
}

bool GIccProfile::GetVariant(char *Name, GVariant &Value, char *Array)
{
	if (!Name)
		return false;

	if (stricmp(Name, "Children") == 0)
	{
		if (Array)
		{
			Value = d->Dom[atoi(Array)];
		}
		else
		{
			Value.SetList();
			for (int i=0; i<d->Dom.Length(); i++)
			{
				Value.Value.Lst->Insert(new GVariant(d->Dom[i]));
			}
		}
	}
	else if (stricmp(Name, "Text") == 0)
	{
		Value = "ICC Colour Profile";
	}
	else return false;

	return true;
}

