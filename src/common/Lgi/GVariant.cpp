#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "GVariant.h"
#include "GToken.h"

GVariant::GVariant()
{
	Type = GV_NULL;
	User = 0;
	ZeroObj(Value);
}

GVariant::GVariant(int i)
{
	Type = GV_INT32;
	User = 0;
	Value.Int = i;
}

GVariant::GVariant(GVariant const &v)
{
	Type = GV_NULL;
	User = 0;
	ZeroObj(Value);

	*this = v;
}

GVariant::GVariant(int64 i)
{
	Type = GV_INT64;
	User = 0;
	Value.Int64 = i;
}

GVariant::GVariant(double i)
{
	Type = GV_DOUBLE;
	User = 0;
	Value.Dbl = i;
}

GVariant::GVariant(char *s)
{
	Type = GV_STRING;
	User = 0;
	Value.String = NewStr(s);
}

GVariant::GVariant(char16 *s)
{
	Type = GV_WSTRING;
	User = 0;
	Value.WString = NewStrW(s);
}

GVariant::GVariant(void *p)
{
	Type = GV_NULL;
	User = 0;
	*this = p;
}

GVariant::GVariant(GDom *p)
{
	Type = GV_NULL;
	User = 0;
	*this = p;
}

GVariant::GVariant(GDom *p, char *name)
{
	Type = GV_NULL;
	User = 0;
	SetDomRef(p, name);
}

GVariant::GVariant(GDateTime *d)
{
	Type = GV_NULL;
	User = 0;
	*this = d;
}

GVariant::GVariant(GOperator Op)
{
	Type = GV_OPERATOR;
	User = 0;
	Value.Op = Op;
}

GVariant::~GVariant()
{
	Empty();
}

bool GVariant::operator ==(GVariant &v)
{
	switch (Type)
	{
		case GV_NULL:
			return v.Type == Type;
		case GV_INT32:
			return Value.Int == v.CastInt32();
		case GV_INT64:
			return Value.Int64 == v.CastInt64();
		case GV_BOOL:
			return Value.Bool == v.CastBool();
		case GV_DOUBLE:
			return Value.Dbl == v.CastDouble();
		case GV_STRING:
		{
			char *s = v.Str();
			if (Value.String && s)
				return !strcmp(Value.String, s);
			break;
		}
		case GV_WSTRING:
		{
			char16 *w = v.WStr();
			if (Value.WString && w)
				return !StrcmpW(Value.WString, w);
			break;
		}
		case GV_BINARY:
		{
			if (v.Type == Type &&
				Value.Binary.Data == v.Value.Binary.Data &&
				Value.Binary.Length == v.Value.Binary.Length)
			{
				return true;
			}
			break;
		}
		case GV_LIST:
		{
			if (!Value.Lst || !v.Value.Lst)
				return false;

			if (Value.Lst->Length() != v.Value.Lst->Length())
				return false;

			GVariant *a, *b;
			for (a = Value.Lst->First(), b = v.Value.Lst->First();
				a && b;
				a = Value.Lst->Next(), b = v.Value.Lst->Next())
			{
				if (!(*a == *b))
					break;
			}
			return !a && !b;
		}
		case GV_DOMREF:
		{
			return Value.DomRef.Dom == v.Value.DomRef.Dom &&
				Value.DomRef.Name != 0 &&
				v.Value.DomRef.Name != 0 &&
				!stricmp(Value.DomRef.Name, v.Value.DomRef.Name);
		}
		case GV_DATETIME:
		{
			if (Value.Date && v.Value.Date)
			{
				return Value.Date->Compare(v.Value.Date) == 0;
			}
			break;
		}
		case GV_DOM:
			return Value.Dom == v.Value.Dom;
		case GV_OPERATOR:
			return Value.Op == v.Value.Op;
		case GV_CUSTOM:
			return Value.Custom == v.Value.Custom;
		case GV_GSURFACE:
			return Value.Surface == v.Value.Surface;
		case GV_GVIEW:
			return Value.View == v.Value.View;
		case GV_GMOUSE:
			return Value.Mouse == v.Value.Mouse;
		case GV_GKEY:
			return Value.Key == v.Value.Key;
		case GV_VOID_PTR:
			return Value.Ptr == v.Value.Ptr;
		case GV_HASHTABLE:
		{
			LgiAssert(0);
			break;
		}
	}

	return false;
}

GVariant &GVariant::operator =(GDateTime *d)
{
	Empty();

	if (d)
	{
		Type = GV_DATETIME;
		Value.Date = new GDateTime;
		if (Value.Date)
		{
			*Value.Date = *d;
			// if (Dirty) *Dirty = true;
		}
	}

	return *this;
}

GVariant &GVariant::operator =(int i)
{
	Empty();
	Type = GV_INT32;
	Value.Int = i;
	// if (Dirty) *Dirty = true;

	return *this;
}

GVariant &GVariant::operator =(bool i)
{
	Empty();
	Type = GV_BOOL;
	Value.Bool = i;
	// if (Dirty) *Dirty = true;

	return *this;
}

GVariant &GVariant::operator =(int64 i)
{
	Empty();
	Type = GV_INT64;
	Value.Int64 = i;
	// if (Dirty) *Dirty = true;

	return *this;
}

GVariant &GVariant::operator =(double i)
{
	Empty();
	Type = GV_DOUBLE;
	Value.Dbl = i;
	// if (Dirty) *Dirty = true;

	return *this;
}

GVariant &GVariant::operator =(char *s)
{
	Empty();
	if (s)
	{
		Type = GV_STRING;
		Value.String = NewStr(s);
	}
	// if (Dirty) *Dirty = true;

	return *this;
}

GVariant &GVariant::operator =(char16 *s)
{
	Empty();
	if (s)
	{
		Type = GV_WSTRING;
		Value.WString = NewStrW(s);
	}
	// if (Dirty) *Dirty = true;

	return *this;
}

GVariant &GVariant::operator =(void *p)
{
	Empty();
	if (p)
	{
		Type = GV_VOID_PTR;
		Value.Ptr = p;
		// if (Dirty) *Dirty = true;
	}

	return *this;
}

GVariant &GVariant::operator =(GDom *p)
{
	Empty();
	if (p)
	{
		Type = GV_DOM;
		Value.Dom = p;
		// if (Dirty) *Dirty = true;
	}

	return *this;
}

GVariant &GVariant::operator =(GView *p)
{
	Empty();
	if (p)
	{
		Type = GV_GVIEW;
		Value.View = p;
		// if (Dirty) *Dirty = true;
	}

	return *this;
}

GVariant &GVariant::operator =(GMouse *p)
{
	Empty();
	if (p)
	{
		Type = GV_GMOUSE;
		Value.Mouse = p;
		// if (Dirty) *Dirty = true;
	}

	return *this;
}

GVariant &GVariant::operator =(GKey *p)
{
	Empty();
	if (p)
	{
		Type = GV_GKEY;
		Value.Key = p;
		// if (Dirty) *Dirty = true;
	}

	return *this;
}

GVariant &GVariant::operator =(GVariant const &i)
{
	Empty();
	Type = i.Type;
	User = i.User;

	switch (Type)
	{
		case GV_NULL:
		{
			break;
		}
		case GV_INT32:
		{
			Value.Int = i.Value.Int;
			break;
		}
		case GV_BOOL:
		{
			Value.Bool = i.Value.Bool;
			break;
		}
		case GV_INT64:
		{
			Value.Int64 = i.Value.Int64;
			break;
		}
		case GV_DOUBLE:
		{
			Value.Dbl = i.Value.Dbl;
			break;
		}
		case GV_STRING:
		{
			Value.String = NewStr(((GVariant&)i).Str());
			break;
		}
		case GV_WSTRING:
		{
			Value.WString = NewStrW(i.Value.WString);
			break;
		}
		case GV_BINARY:
		{
			SetBinary(i.Value.Binary.Length, i.Value.Binary.Data);
			break;
		}
		case GV_LIST:
		{
			SetList(i.Value.Lst);
			break;
		}
		case GV_DOM:
		{
			Value.Dom = i.Value.Dom;
			break;
		}
		case GV_VOID_PTR:
		case GV_GVIEW:
		case GV_GMOUSE:
		case GV_GKEY:
		{
			Value.Ptr = i.Value.Ptr;
			break;
		}
		case GV_DATETIME:
		{
			if (i.Value.Date)
			{
				Value.Date = new GDateTime;
				if (Value.Date)
				{
					*Value.Date = *i.Value.Date;
				}
			}
			break;
		}
		case GV_HASHTABLE:
		{
			if (Value.Hash = new GHashTable)
			{
				char *k;
				for (void *p = i.Value.Hash->First(&k); p; p = i.Value.Hash->Next(&k))
				{
					GVariant *var = (GVariant*)p;
					Value.Hash->Add(k, new GVariant(*var));
				}
			}
			break;
		}
		case GV_CUSTOM:
		{
			Value.Custom.Data = i.Value.Custom.Data;
			Value.Custom.Dom = i.Value.Custom.Dom;
			break;
		}
		case GV_GSURFACE:
		{
			Value.Surface = i.Value.Surface;
			break;
		}
		default:
		{
			printf("%s:%i - Unknown variant type '%i'\n", _FL, Type);
			LgiAssert(0);
			break;
		}
	}

	// if (Dirty) *Dirty = true;

	return *this;
}

bool GVariant::SetDomRef(GDom *obj, char *name)
{
	Empty();

	Type = GV_DOMREF;
	Value.DomRef.Dom = obj;
	Value.DomRef.Name = NewStr(name);

	return Value.DomRef.Name != 0;
}

bool GVariant::SetBinary(int Len, void *Data, bool Own)
{
	bool Status = false;

	Empty();

	Type = GV_BINARY;
	Value.Binary.Length = Len;
	if (Own)
	{
		Value.Binary.Data = Data;
		Status = true;
	}
	else
	{
		if (Value.Binary.Data = new uchar[Value.Binary.Length])
		{
			if (Data)
				memcpy(Value.Binary.Data, Data, Value.Binary.Length);
			else
				memset(Value.Binary.Data, 0, Value.Binary.Length);

			Status = true;
		}
	}

	return Status;
}

bool GVariant::SetList(List<GVariant> *Lst)
{
	Empty();
	Type = GV_LIST;

	if ((Value.Lst = new List<GVariant>) && Lst)
	{
		for (GVariant *s=Lst->First(); s; s=Lst->Next())
		{
			GVariant *New = new GVariant;
			if (New)
			{
				*New = *s;
				Value.Lst->Insert(New);
			}
		}
	}

	return Value.Lst != 0;
}

bool GVariant::SetHashTable(GHashTable *Table, bool Copy)
{
	Empty();
	Type = GV_HASHTABLE;

	if (Copy AND Table)
	{
		if (Value.Hash = new GHashTable)
		{
			char *k;
			for (void *p = Table->First(&k); p; p = Table->Next(&k))
			{
				Value.Hash->Add(k, p);
			}
		}
	}
	else
	{
		Value.Hash = Table;
	}

	return Value.Hash != 0;
}

void GVariant::OwnStr(char *s)
{
	Empty();
	Type = s ? GV_STRING : GV_NULL;
	Value.String = s;
}

void GVariant::OwnStr(char16 *w)
{
	Empty();
	Type = w ? GV_WSTRING : GV_NULL;
	Value.WString = w;
}

char *GVariant::ReleaseStr()
{
	char *Ret = Str();
	if (Ret)
	{
		Value.String = 0;
		Type = GV_NULL;
	}
	return Ret;
}

char *GVariant::Str()
{
	if (Type == GV_STRING)
		return Value.String;

	if (Type == GV_WSTRING)
	{
		char *u = LgiNewUtf16To8(Value.WString);
		DeleteArray(Value.WString);
		Type = GV_STRING;
		return Value.String = u;
	}

	return 0;
}

char16 *GVariant::ReleaseWStr()
{
	char16 *Ret = WStr();
	if (Ret)
	{
		Value.WString = 0;
		Type = GV_NULL;
	}
	return Ret;
}

char16 *GVariant::WStr()
{
	if (Type == GV_WSTRING)
		return Value.WString;

	if (Type == GV_STRING)
	{
		char16 *w = LgiNewUtf8To16(Value.String);
		DeleteArray(Value.String);
		Type = GV_WSTRING;
		return Value.WString = w;
	}

	return 0;
}

void GVariant::Empty()
{
	switch (Type)
	{
		case GV_CUSTOM:
		{
			Value.Custom.Data = 0;
			Value.Custom.Dom = 0;
			break;
		}
		case GV_DOMREF:
		{
			DeleteArray(Value.DomRef.Name);
			Value.DomRef.Dom = 0;
			break;
		}
		case GV_STRING:
		{
			DeleteArray(Value.String);
			break;
		}
		case GV_WSTRING:
		{
			DeleteArray(Value.WString);
			break;
		}
		case GV_BINARY:
		{
			char *d = (char*) Value.Binary.Data;
			DeleteArray(d);
			Value.Binary.Data = 0;
			break;
		}
		case GV_DATETIME:
		{
			DeleteObj(Value.Date);
			break;
		}
		case GV_LIST:
		{
			if (Value.Lst)
			{
				Value.Lst->DeleteObjects();
				DeleteObj(Value.Lst);
			}
			break;
		}
		case GV_HASHTABLE:
		{
			if (Value.Hash)
			{
				for (GVariant *v = (GVariant*) Value.Hash->First(); v; v = (GVariant*) Value.Hash->Next())
				{
					DeleteObj(v);
				}
				DeleteObj(Value.Hash);
			}
			break;
		}
	}

	Type = GV_NULL;
	ZeroObj(Value);
}

bool GVariant::IsInt()
{
	return Type == GV_INT32 OR Type == GV_INT64;
}

bool GVariant::IsBool()
{
	return Type == GV_BOOL;
}

bool GVariant::IsDouble()
{
	return Type == GV_DOUBLE;
}

bool GVariant::IsString()
{
	return Type == GV_STRING;
}

bool GVariant::IsBinary()
{
	return Type == GV_BINARY;
}

bool GVariant::IsNull()
{
	return Type == GV_NULL;
}

#define IsList() (Type == GV_LIST AND Value.Lst)

GVariant &GVariant::Cast(GVariantType NewType)
{
	if (NewType != Type)
	{
		switch (NewType)
		{
			default:
			{
				// No conversion possible
				break;
			}
			case GV_INT32:
			{
				*this = CastInt32();
				break;
			}
			case GV_INT64:
			{
				*this = CastInt64();
				break;
			}
			case GV_BOOL:
			{
				if (Type == GV_DOUBLE)
				{
					*this = Value.Dbl != 0.0;
				}
				else
				{
					*this = CastInt32() != 0;
				}
				break;
			}
			case GV_DOUBLE:
			{
				*this = CastDouble();
				break;
			}
			case GV_STRING:
			{
				*this = CastString();
				break;
			}
			case GV_DATETIME:
			{
				switch (Type)
				{
					case GV_STRING:
					{
						// String -> GDateTime
						GDateTime *Dt = new GDateTime;
						if (Dt)
						{
							Dt->Set(Value.String);
							Empty();
							Value.Date = Dt;
							Type = NewType;
						}
						break;
					}
					case GV_INT64:
					{
						// Int64 (system date) -> GDateTime
						GDateTime *Dt = new GDateTime;
						if (Dt)
						{
							Dt->Set((uint64)Value.Int64);
							Empty();
							Value.Date = Dt;
							Type = NewType;
						}
						break;
					}
					default:
					{
						// No conversion available
						break;
					}
				}
				break;
			}
		}
	}

	return *this;
}

void *GVariant::CastVoidPtr()
{
	switch (Type)
	{
		case GV_STRING:
			return Value.String;
		case GV_BINARY:
			return Value.Binary.Data;
		case GV_LIST:
			return Value.Lst;
		case GV_DOM:
			return Value.Dom;
		case GV_DOMREF:
			return Value.DomRef.Dom;
		case GV_VOID_PTR:
			return Value.Ptr;
		case GV_DATETIME:
			return Value.Date;
		case GV_HASHTABLE:
			return Value.Hash;
		case GV_CUSTOM:
			return Value.Custom.Data;
		case GV_WSTRING:
			return Value.WString;
		case GV_GSURFACE:
			return Value.Surface;
		case GV_GVIEW:
			return Value.View;
		case GV_GMOUSE:
			return Value.Mouse;
		case GV_GKEY:
			return Value.Key;
	}

	return 0;
}

GDom *GVariant::CastDom()
{
	switch (Type)
	{
		case GV_DOM:
			return Value.Dom;
		case GV_DOMREF:
			return Value.DomRef.Dom;
	}

	return 0;
}

bool GVariant::CastBool()
{
	switch (Type)
	{
		case GV_INT32:
			return Value.Int != 0;
		case GV_INT64:
			return Value.Int64 != 0;
		case GV_BOOL:
			return Value.Bool;
		case GV_DOUBLE:
			return Value.Dbl != 0.0;
		case GV_STRING:
			return Value.String ? atoi(Value.String) != 0 : false;
		case GV_BINARY:
			return Value.Binary.Data != 0;
		case GV_LIST:
			return Value.Lst != 0;
		case GV_DOM:
			return Value.Dom != 0;
		case GV_DOMREF:
			return Value.DomRef.Dom != 0;
		case GV_VOID_PTR:
			return Value.Ptr != 0;
		case GV_GVIEW:
			return Value.View != 0;
		case GV_GMOUSE:
			return Value.Mouse != 0;
		case GV_GKEY:
			return Value.Key != 0;
		case GV_DATETIME:
			return Value.Date != 0;
		case GV_HASHTABLE:
			return Value.Hash != 0;
		case GV_OPERATOR:
			return Value.Op != OpNull;
		case GV_CUSTOM:
			return Value.Custom.Dom != 0 && Value.Custom.Data != 0;
	}

	return false;
}

double GVariant::CastDouble()
{
	switch (Type)
	{
		case GV_BOOL:
			return Value.Bool ? 1.0 : 0.0;
		case GV_DOUBLE:
			return Value.Dbl;
		case GV_INT32:
			return (double)Value.Int;
		case GV_INT64:
			return (double)Value.Int64;
		case GV_STRING:
			return Value.String ? atof(Value.String) : 0;
		case GV_DOMREF:
		{
			static GVariant v;
			if (Value.DomRef.Dom)
			{
				if (Value.DomRef.Dom->GetValue(Value.DomRef.Name, v))
				{
					return v.CastDouble();
				}
			}
			break;
		}
	}
	
	return 0;
}

int32 GVariant::CastInt32()
{
	switch (Type)
	{
		case GV_BOOL:
			return (int32)Value.Bool;
		case GV_DOUBLE:
			return (int32)Value.Dbl;
		case GV_INT32:
			return Value.Int;
		case GV_INT64:
			return Value.Int64;
		case GV_STRING:
			return Value.String ? atoi(Value.String) : 0;
		case GV_DOM:
			return Value.Dom != 0;
		case GV_DOMREF:
		{
			static GVariant v;
			if (Value.DomRef.Dom)
			{
				if (Value.DomRef.Dom->GetValue(Value.DomRef.Name, v))
				{
					return v.CastInt32();
				}
			}
			break;
		}
	}
	
	return 0;
}

int64 GVariant::CastInt64()
{
	switch (Type)
	{
		case GV_BOOL:
			return (int64)Value.Bool;
		case GV_DOUBLE:
			return (int64)Value.Dbl;
		case GV_INT32:
			return Value.Int;
		case GV_INT64:
			return Value.Int64;
		case GV_STRING:
		{
			if (Value.String)
			{
				#ifdef _MSC_VER
				return _atoi64(Value.String);
				#else
				return atoll(Value.String);
				#endif
			}
			break;
		}
		case GV_DOMREF:
		{
			static GVariant v;
			if (Value.DomRef.Dom)
			{
				if (Value.DomRef.Dom->GetValue(Value.DomRef.Name, v))
				{
					return v.CastInt64();
				}
			}
			break;
		}
	}
	
	return 0;
}

char *GVariant::CastString()
{
	char i[40];

	switch (Type)
	{
		case GV_DOMREF:
		{
			static GVariant v;
			if (Value.DomRef.Dom)
			{
				if (Value.DomRef.Dom->GetValue(Value.DomRef.Name, v))
				{
					return v.CastString();
				}
			}
			break;
		}
		case GV_INT32:
		{
			sprintf(i, "%i", Value.Int);
			*this = i;
			return Str();
		}
		case GV_DOUBLE:
		{
			sprintf(i, "%f", Value.Dbl);
			*this = i;
			return Str();
		}
		case GV_BOOL:
		{
			sprintf(i, "%i", Value.Bool);
			*this = i;
			return Str();
		}
		case GV_INT64:
		{
			sprintf(i, LGI_PrintfInt64, Value.Int64);
			*this = i;
			return Str();
		}
		case GV_STRING:
		case GV_WSTRING:
		{
			return Str();
		}
		case GV_DATETIME:
		{
			if (Value.Date)
			{
				char s[64];
				Value.Date->Get(s);
				*this = s;
				return Str();
			}
			break;
		}
	}
	
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////
GDom *GDom::ResolveObject(char *Var, char *Name, char *Array)
{
	GDom *Object = this;

	// Tokenise the string
	GArray<char*> t;
	for (char *s = Var; s AND *s; )
	{
		char *e = s;
		while (*e AND *e != '.')
		{
			if (*e == '[')
			{
				e++;
				while (*e AND *e != ']')
				{
					if (*e == '\"' OR *e == '\'')
					{
						char d = *e++;
						while (*e AND *e != d)
						{
							e++;
						}
						if (*e == d) e++;
					}
					else e++;
				}
				if (*e == ']') e++;
			}
			else e++;
		}
		
		int Len = e - s;
		if (Len < 1 OR Len > 255)
		{
			Object = 0;
			break;
		}
		
		char Part[256];
		memcpy(Part, s, Len);
		Part[Len] = 0;
		while (strchr(" \t", Part[Len-1]))
		{
			Len--;
		}

		t[t.Length()] = NewStr(Part, Len);
		s = *e ? e + 1 : e;
	}

	// Process elements
	for (int i=0; i<t.Length(); i++)
	{
		char Base[256];
		char Index[256] = "";

		// Parse out object name parts
		char *Obj = t[i];
		if (*Obj == '[')
		{
			Object = 0;
			break;
		}
		
		char *ArrStart = strchr(Obj, '[');
		if (ArrStart)
		{
			*ArrStart++ = 0;
			char *ArrEnd = 0;
			if (strchr("\'\"", *ArrStart))
			{
				ArrEnd = strchr(ArrStart + 1, ArrStart[0]);
				ArrStart++;
			}
			else
			{
				ArrEnd = strchr(ArrStart, ']');
			}
			if (ArrEnd)
			{
				*ArrEnd = 0;
				strcpy(Index, ArrStart);
			}
		}
		strcpy(Base, Obj);

		// Process parts
		if (i == t.Length()-1)
		{
			strcpy(Name, Base);
			strcpy(Array, Index);
			
			char *e = Name + strlen(Name) - 1;
			while (e > Name AND strchr(" \t\r\n", *e))
			{
				*e-- = 0;
			}
		}
		else
		{
			GVariant v;
			if (Index[0])
			{
				if (Object->GetVariant(Base, v, Index))
				{
					if (v.Type == GV_LIST)
					{
						int N = atoi(Index);
						GVariant *Element = v.Value.Lst->ItemAt(N);
						if (Element AND Element->Type == GV_DOM)
						{
							Object = Element->Value.Dom;
						}
						else
						{
							Object = 0;
							goto ResolveDone;
						}
					}
					else if (v.Type == GV_DOM)
					{
						Object = v.Value.Dom;
					}
					else
					{
						Object = 0;
						goto ResolveDone;
					}
				}
				else
				{
					Object = 0;
					goto ResolveDone;
				}
			}
			else
			{
				if (Object->GetVariant(Obj, v) AND
					v.Type == GV_DOM)
				{
					Object = v.Value.Dom;
				}
				else
				{
					Object = 0;
					goto ResolveDone;
				}
			}
		}
	}

ResolveDone:
	t.DeleteArrays();
	return Object;
}

bool GDom::GetValue(char *Var, GVariant &Value)
{
	bool Status = false;

	if (Var)
	{
		// GSemaphore *Sem = dynamic_cast<GSemaphore*>(this);
		// LgiTrace("%s:%i - Get_OnAccess(1) '%s' %i\n", _FL, Var, Sem?Sem->GetCount():-1);
		if (_OnAccess(true))
		{
			char Name[256] = "", Arr[256] = "";
			GDom *Object = ResolveObject(Var, Name, Arr);
			if (Object)
			{
				if (Name[0] == 0)
				{
					LgiTrace("%s:%i - Warning name parse failed for '%s'\n",
						__FILE__, __LINE__, Var);
				}
				else
				{			
					Status = Object->GetVariant(Name, Value, ValidStr(Arr) ? Arr : 0);
				}
			}

			// LgiTrace("%s:%i - Get_OnAccess(0) '%s'\n", _FL, Var);
			_OnAccess(false);
		}
		else
		{
			LgiTrace("%s:%i - Locking error\n", _FL);
			LgiAssert(0);
		}
	}

	return Status;
}

bool GDom::SetValue(char *Var, GVariant &Value)
{
	bool Status = false;

	if (Var)
	{
		// GSemaphore *Sem = dynamic_cast<GSemaphore*>(this);
		// LgiTrace("%s:%i - Set_OnAccess(1) '%s' %i\n", _FL, Var, Sem?Sem->GetCount():-1);
		if (_OnAccess(true))
		{
			char Name[256], Arr[256] = "";
			GDom *Object = ResolveObject(Var, Name, Arr);
			if (Object)
			{
				if (Name[0] == 0)
					LgiTrace("%s:%i - Warning name parse failed for '%s'\n", _FL, Var);
				else
				{
					bool Valid = ValidStr(Arr);
					Status = Object->SetVariant(Name, Value, Valid ? Arr : 0);
				}
			}

			// LgiTrace("%s:%i - Set_OnAccess(0) '%s'\n", _FL, Var);
			_OnAccess(false);
		}
		else
		{
			LgiTrace("%s:%i - Locking error\n", _FL);
			LgiAssert(0);
		}
	}

	return Status;
}

