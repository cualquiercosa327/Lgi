//
//  GMenuCommon.cpp
//  LgiCarbon
//
//  Created by Matthew Allen on 30/01/2018.
//
//

#include "Lgi.h"


bool GSubMenu::GetVariant(const char *Name, GVariant &Value, char *Array)
{
	GDomProperty p = LgiStringToDomProp(Name);
	switch (p)
	{
		case ObjLength:
			Value = Length();
			break;
		default:
			return false;
	}
	return true;
}

bool GSubMenu::SetVariant(const char *Name, GVariant &Value, char *Array)
{
	GDomProperty p = LgiStringToDomProp(Name);
	switch (p)
	{
		default:
			return false;
	}
	return true;
}

bool GSubMenu::CallMethod(const char *MethodName, GVariant *ReturnValue, GArray<GVariant*> &Args)
{
	GDomProperty Method = LgiStringToDomProp(MethodName);	
	switch (Method)
	{
		case ::AppendSeparator:
		{
			*ReturnValue = AppendSeparator();
			break;
		}
		case ::AppendItem:
		{
			char *Str		= Args.Length() > 0 ? Args[0]->CastString() : NULL;
			int Id			= Args.Length() > 1 ? Args[1]->CastInt32() : -1;
			bool Enabled	= Args.Length() > 2 ? Args[2]->CastBool() : true;
			int Where		= Args.Length() > 3 ? Args[3]->CastInt32() : -1;
			char *ShortCut	= Args.Length() > 4 ? Args[4]->CastString() : NULL;

			*ReturnValue = AppendItem(Str, Id, Enabled, Where, ShortCut);
			break;
		}
		default:
			return false;
	}
	return true;
}
