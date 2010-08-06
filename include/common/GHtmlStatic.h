#ifndef _GHTMLSTATIC_H_
#define _GHTMLSTATIC_H_

#include "GCss.h"
#include "GHashTable.h"

extern char16 GHtmlListItem[];
/// Common data for HTML related classes
class GHtmlStatic
{
	friend class GHtmlStaticInst;

public:
	static GHtmlStatic *Inst;

	int Refs;
	GHashTbl<char16*,int>			VarMap;
	GHashTbl<char*,GCss::PropType>	StyleMap;
	GHashTbl<char*,int>				ColourMap;

	GHtmlStatic();
	~GHtmlStatic();
};

/// Static data setup/pulldown
class GHtmlStaticInst
{
public:
	GHtmlStatic *Static;

	GHtmlStaticInst()
	{
		if (!GHtmlStatic::Inst)
		{
			GHtmlStatic::Inst = new GHtmlStatic;
		}
		if (GHtmlStatic::Inst)
		{
			GHtmlStatic::Inst->Refs++;
		}
		Static = GHtmlStatic::Inst;
	}

	~GHtmlStaticInst()
	{
		if (GHtmlStatic::Inst)
		{
			GHtmlStatic::Inst->Refs--;
			if (GHtmlStatic::Inst->Refs == 0)
			{
				DeleteObj(GHtmlStatic::Inst);
			}
		}
	}
};

#endif