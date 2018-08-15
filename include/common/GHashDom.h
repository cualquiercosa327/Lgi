#ifndef _GHASHDOM_H_
#define _GHASHDOM_H_

#include "GVariant.h"

class GHashDom : public GHashTbl<const char*, GVariant*>, public GDom
{
public:
	GHashDom(int Size = 0) : GHashTbl<const char*, GVariant*>(Size, false)
	{
	}
	
	~GHashDom()
	{
	    DeleteObjects();
	}

	bool GetVariant(const char *Name, GVariant &Value, char *Array = 0)
	{
		GVariant *v = Find(Name);
		if (v)
		{
			Value = *v;
			return true;
		}
		
		return false;
	}

	bool SetVariant(const char *Name, GVariant &Value, char *Array = 0)
	{
		GVariant *v = Find(Name);
		if (v)
		{
		    *v = Value;
		    return true;
		}
		else if ((v = new GVariant(Value)))
		{
			Add(Name, v);
			return true;
		}
		return false;
	}
};

#endif