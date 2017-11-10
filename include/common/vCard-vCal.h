#ifndef _VCARD_VCAL_H
#define _VCARD_VCAL_H

// #include "ScribeDefs.h"
#include "Store3.h"
#include "GToken.h"

#define FIELD_PERMISSIONS				2000

class VIoPriv;
class VIo
{
public:
	struct Parameter
	{
		GString Field;
		GString Value;
		
		void Set(const char *f, const char *v)
		{
			Field = f;
			Value = v;
		}
	};
	
	struct TimeZoneSection
	{
		int From, To;
		LDateTime Start;
		GString Rule;
	};
	
	struct TimeZoneInfo
	{
		GString Name;
		TimeZoneSection Normal;
		TimeZoneSection Daylight;
	};

	class ParamArray : public GArray<Parameter>
	{
	public:
		ParamArray(const char *Init = NULL)
		{
			if (Init)
			{
				GString s = Init;
				GString::Array a = s.SplitDelimit(",");
				for (unsigned i=0; i<a.Length(); i++)
					New().Set("type", a[i]);
			}
		}
	
		const char *Find(const char *s)
		{
			for (unsigned i=0; i<Length(); i++)
			{
				Parameter &p = (*this)[i];
				if (p.Field.Equals(s))
					return p.Value;
			}
			return NULL;
		}

		void Empty()
		{
			Length(0);
		}
	};

protected:
	VIoPriv *d;

	bool ParseDate(LDateTime &out, char *in);
	bool ParseDuration(LDateTime &Out, int &Sign, char *In);

	void Fold(GStreamI &o, char *i, int pre_chars = 0);
	char *Unfold(char *In);
	char *UnMultiLine(char *In);

	bool ReadField(GStreamI &s, GString &Name, ParamArray *Type, GString &Data);
	void WriteField(GStreamI &s, const char *Name, ParamArray *Type, char *Data);

public:
	VIo();
	~VIo();
};

class VCard : public VIo
{
public:
	bool Import(GDataPropI *c, GStreamI *s);
	bool Export(GDataPropI *c, GStreamI *s);
};

class VCal : public VIo
{
    bool IsCal;

public:
    VCal()
    {
        IsCal = false;
    }

	bool Import(GDataPropI *c, GStreamI *s);
	bool Export(GDataPropI *c, GStreamI *s);
};

#endif
