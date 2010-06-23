/*hdr
**      FILE:           GProperties.h
**      AUTHOR:         Matthew Allen
**      DATE:           8/4/98
**      DESCRIPTION:    Property hndling
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#ifndef __PROP_H
#define __PROP_H

#include <string.h>
#include "GContainers.h"
#include "LgiClass.h"

#define OBJ_NULL		0
#define OBJ_INT			1
#define OBJ_FLOAT		2
#define OBJ_STRING		3
#define OBJ_BINARY		4
#define OBJ_CUSTOM		16

typedef union
{
	int Int;
	double Dbl;
	char *Cp;
} MultiValue;

class Prop
{
public:
	char *Name;
	int Type;
	int Size;
	MultiValue Value;

	Prop()
	{
		Size = 0;
		Name = 0;
		Type = OBJ_NULL;
		ZeroObj(Value);
	}

	Prop(char *n)
	{
		if (n)
		{
			Name = new char[strlen(n)+1];
			if (Name)
			{
				strcpy(Name, n);
			}
		}
		else
		{
			Name = 0;
		}

		Type = OBJ_NULL;
		ZeroObj(Value);
	}

	virtual ~Prop()
	{
		DeleteArray(Name);
		EmptyData();
	}

	void EmptyData()
	{
		if (Type == OBJ_STRING OR Type == OBJ_BINARY)
		{
			DeleteArray(Value.Cp);
		}

		Type = OBJ_NULL;
	}

	bool operator ==(Prop &p);
	bool operator ==(char *n);

	bool Serialize(GFile &f, bool Write);
	bool SerializeText(GFile &f, bool Write);
};

class ObjTree;
class ObjProperties;

typedef ObjProperties *pObjProperties;

class ObjProperties :
	public GObject
{
	friend class ObjTree;

	// Tree
	ObjProperties *Parent;
	ObjProperties *Next;
	ObjProperties *Leaf;

	// Node
	Prop *Current;
	List<Prop> Properties;

	Prop *FindProp(char *Name);

public:
	ObjProperties();
	ObjProperties(char *n);
	virtual ~ObjProperties();

	ObjProperties &operator =(ObjProperties &props);

	bool operator ==(char *n)
	{
		if (Name())
		{	
			return stricmp(Name(), (n) ? n : (char*)"") == 0;
		}
		return false;
	}

	bool operator !=(char *n)
	{
		return !(*this == n);
	}

	ObjProperties *GetParent() { return Parent; }
	pObjProperties &GetNext() { return Next; }
	pObjProperties &GetLeaf() { return Leaf; }
	ObjProperties *CreateNext(char *n);
	ObjProperties *CreateLeaf(char *n);
	ObjProperties *FindLeaf(char *n);

	char *KeyName();
	int KeyType();
	void *KeyValue();
	Prop *GetProp();
	int GetPropertyCount() { return Properties.Length(); }

	bool Find(char *Name);
	bool FirstKey();
	bool LastKey();
	bool NextKey();
	bool PrevKey();
	bool DeleteKey(char *Name = 0);
	
	void Empty();
	int SizeofData();

	bool Set(char *Name, int n);
	bool Set(char *Name, double n);
	bool Set(char *Name, char *n);
	bool Set(char *Name, void *Data, int Len);
	bool Set(Prop *p);

	bool Get(char *Name, int &n);
	bool Get(char *Name, double &n);
	bool Get(char *Name, char *&n);
	bool Get(char *Name, void *&Data, int &Len);
	bool Get(Prop *&p);

	bool Serialize(GFile &f, bool Write);
	bool SerializeText(GFile &f, bool Write);
};

class ObjTree : public GObject
{
	ObjProperties *Root;

public:
	ObjTree();
	virtual ~ObjTree();

	ObjProperties *GetRoot();
	ObjProperties *GetLeaf(char *Name, bool Create = false);

	bool Set(char *Name, int n);
	bool Set(char *Name, double n);
	bool Set(char *Name, char *n);

	bool Get(char *Name, int &n);
	bool Get(char *Name, double &n);
	bool Get(char *Name, char *&n);

	void Print();
	bool Serialize(GFile &f, bool Write);
	bool SerializeObj(GFile &f, bool Write);
	bool SerializeText(GFile &f, bool Write);
};

#endif

