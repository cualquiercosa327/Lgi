#ifndef _GHASHDOM_H_
#define _GHASHDOM_H_

#include "GVariant.h"

class GHashDom : public GHashTable, public GDom
{
public:
	GHashDom(int Size = 0) : GHashTable(Size, false) { }
	~GHashDom()
	{
		for (GVariant *v = (GVariant*)First(); v; v = (GVariant*)Next())
		{
			DeleteObj(v);
		}
	}

	bool GetVariant(const char *Name, GVariant &Value, char *Array = 0)
	{
		GVariant *v = (GVariant*)Find(Name);
		if (v)
		{
			Value = *v;
			return true;
		}
		return false;
	}

	bool SetVariant(const char *Name, GVariant &Value, char *Array = 0)
	{
		GVariant *v = new GVariant(Value);
		if (v)
		{
			Add(Name, v);
			return true;
		}
		return false;
	}
};

#endif