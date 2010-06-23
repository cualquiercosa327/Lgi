#ifndef _GTAG_H
#define _GTAG_H

#include "GVariant.h"
#include "GMap.h"
#include "GXmlTree.h"

class GNamedVariant : public GVariant, public GObj
{
public:
	GNamedVariant(char *s = 0)
	{
		if (s) Name(s);
	}
};

class GTag : public List<GNamedVariant>, public GDom
{
protected:
	char *Element;
	bool ObscurePasswords;

	GVariantType TypeOf(char *Name);

public:
	GTag(char *e);
	~GTag();

	bool IsNumber(char *s);
	bool operator ==(char *s);
	GTag &operator =(GTag &t);
	GNamedVariant *GetNamed(char *Name);
	void Empty();
	bool Read(GXmlTag *t);
	void Write(GFile &f);
	bool GetVariant(char *Name, GVariant &Value, char *Array = 0);
	bool SetVariant(char *Name, GVariant &Value, char *Array = 0);
	void SerializeUI(GView *Dlg, GMap<char*,int> &Fields, bool To);
};

#endif
