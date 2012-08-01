#ifndef _VCARD_VCAL_H
#define _VCARD_VCAL_H

#include "ScribeDefs.h"
#include "Store3.h"
#include "GToken.h"

#define FIELD_PERMISSIONS				2000

class VIoPriv;
class VIo
{
protected:
	class TypesList : public GArray<GAutoString>
	{
	public:
		TypesList(const char *init = 0)
		{
			if (init)
			{
				GToken t(init, ",");
				for (uint32 i=0; i<t.Length(); i++)
					New().Reset(NewStr(t[i]));
			}
		}

		bool Find(const char *s)
		{
			for (uint32 i=0; i<Length(); i++)
			{
				if (_stricmp((*this)[i], s) == 0)
					return true;
			}
			return false;
		}

		void Empty()
		{
			Length(0);
		}
	};

	VIoPriv *d;

	bool ParseDate(GDateTime &out, char *in);
	bool ParseDuration(GDateTime &Out, int &Sign, char *In);

	void Fold(GStreamI &o, char *i, int pre_chars = 0);
	char *Unfold(char *In);
	char *UnMultiLine(char *In);

	bool ReadField(GStreamI &s, char **Name, TypesList *Type, char **Data);
	void WriteField(GStreamI &s, const char *Name, TypesList *Type, char *Data);

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
