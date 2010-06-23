#ifndef _VCARD_VCAL_H
#define _VCARD_VCAL_H

#include "ScribeDefs.h"
#include "Store3.h"

#define FIELD_PERMISSIONS				2000

class VIoPriv;
class VIo
{
protected:
	VIoPriv *d;

	bool ParseDate(GDateTime &out, char *in);
	bool ParseDuration(GDateTime &Out, int &Sign, char *In);

	void Fold(GStreamI &o, char *i, int pre_chars = 0);
	char *Unfold(char *In);
	char *UnMultiLine(char *In);

	bool ReadField(GStreamI &s, char **Name, char **Type, char **Data);
	void WriteField(GStreamI &s, char *Name, char *Type, char *Data);

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
public:
	bool Import(GDataPropI *c, GStreamI *s);
	bool Export(GDataPropI *c, GStreamI *s);
};

#endif
