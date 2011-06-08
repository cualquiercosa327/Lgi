/*hdr
**      FILE:           Prop.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           8/4/98
**      DESCRIPTION:    Property handling
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "LgiOsDefs.h"
#include "GFile.h"
#include "GProperties.h"
#include "GString.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////
bool Prop::Serialize(GFile &f, bool Write)
{
	ushort Us;

	if (Write)
	{
		Us = strlen(Name) + 1;
		f << Us;
		f.Write(Name, Us);
		f << Type;

		switch (Type)
		{
			case OBJ_INT:
			{
				f << Value.Int;
				break;
			}
			case OBJ_FLOAT:
			{
				f << Value.Dbl;
				break;
			}
			case OBJ_STRING:
			{
				Us = Value.Cp ? strlen(Value.Cp) + 1 : 0;
				f << Us;
				f.Write(Value.Cp, Us);
				break;
			}
			case OBJ_BINARY:
			{
				if (!Value.Cp)
					Size = 0;

				f << Size;
				if (Value.Cp)
					f.Write(Value.Cp, Size);
				break;
			}
			default:
			{
				return false;
			}
		}

		return true;
	}

	f >> Us;
	if (Us > 256)
	{
		return false;
	}

	Name = new char[Us];
	if (!Name)
	{
		return false;
	}

	f.Read(Name, Us);
	f >> Type;

	switch (Type)
	{
		case OBJ_INT:
		{
			f >> Value.Int;
			break;
		}
		case OBJ_FLOAT:
		{
			f >> Value.Dbl;
			break;
		}
		case OBJ_STRING:
		{
			f >> Us;
			Value.Cp = new char[Us];
			if (Value.Cp)
			{
				f.Read(Value.Cp, Us);
			}
			break;
		}
		case OBJ_BINARY:
		{
			f >> Size;
			DeleteArray(Value.Cp);
			Value.Cp = new char[Size];
			if (Value.Cp)
			{
				f.Read(Value.Cp, Size);
			}
			break;
		}
		default:
		{
			return false;
		}
	}

	return true;
}

int HexToDec(char c)
{
	if (c >= '0' AND c <= '9')
	{
		return c - '0';
	}
	if (c >= 'A' AND c <= 'F')
	{
		return c - 'A' + 10;
	}
	if (c >= 'a' AND c <= 'f')
	{
		return c - 'a' + 10;
	}
	return -1;
}

bool Prop::operator ==(Prop &p)
{
	bool Status = (*this) == p.Name;
	return Status;
}

bool Prop::operator ==(const char *n)
{
	bool Status = false;
	if (Name AND n)
	{
		Status = stricmp(Name, n) == 0;
	}
	return Status;
}

bool Prop::SerializeText(GFile &f, bool Write)
{
	char Str[1024];

	if (Write)
	{
		switch (Type)
		{
			case OBJ_INT:
			{
				sprintf(Str, "%s=%i\r\n", Name, Value.Int);
				f.Write(Str, strlen(Str));
				break;
			}
			case OBJ_FLOAT:
			{
				sprintf(Str, "%s=%e\r\n", Name, Value.Dbl);
				f.Write(Str, strlen(Str));
				break;
			}
			case OBJ_STRING:
			{
				f.Write(Str, sprintf(Str, "%s=\"", Name));
				
				for (char *s = Value.Cp; s AND *s; s++)
				{
					switch (*s)
					{
						case '\n':
							f.Write((char*)"\\n", 2);
							break;
						case '\t':
							f.Write((char*)"\\t", 2);
							break;
						case '\r':
							f.Write((char*)"\\r", 2);
							break;
						case '\\':
							f.Write((char*)"\\\\", 2);
							break;
						case '\'':
							f.Write((char*)"\\\'", 2);
							break;
						case '\"':
							f.Write((char*)"\\\"", 2);
							break;
						case '?':
							f.Write((char*)"\\?", 2);
							break;
						default:
							f.Write(s, 1);
							break;
					}
				}
				
				f.Write((char*)"\"\r\n", 3);
				break;
			}
			case OBJ_BINARY:
			{
				sprintf(Str, "%s=B(%i,", Name, Size);
				f.Write(Str, strlen(Str));
				for (int i=0; i<Size; i++)
				{
					sprintf(Str, "%2.2X", (uchar)Value.Cp[i]);
					f.Write(Str, 2);
				}
				f.Write((char*)")\r\n", 3);
				break;
			}
			default:
			{
				return false;
			}
		}
	}
	else
	{
		f.ReadStr(Str, sizeof(Str));
		if (strlen(Str)>0)
		{
			char *Equal = strchr(Str, '=');
			if (Equal)
			{
				*Equal = 0;
				Equal++;

				DeleteArray(Name);
				Name = NewStr(Str);
				Type = 0;

				if (strlen(Equal) > 0)
				{
					if (*Equal == '\"')
					{
						Type = OBJ_STRING;
					}
					else if (*Equal == 'B')
					{
						Type = OBJ_BINARY;
					}
					else if (strchr(Equal, '.'))
					{
						Type = OBJ_FLOAT;
					}
					else
					{
						Type = OBJ_INT;
					}

					switch (Type)
					{
						case OBJ_INT:
						{
							Value.Int = atoi(Equal);
							break;
						}
						case OBJ_FLOAT:
						{
							Value.Dbl = atof(Equal);
							break;
						}
						case OBJ_STRING:
						{
							char *Start = Equal + 1;

							GStringPipe p(256);
							
							for (char *s = Start; s AND *s AND *s != '\"'; )
							{
								if (*s == '\\')
								{
									s++;
									switch (*s++)
									{
										case '\\':
											p.Push("\\", 1);
											break;
										case 'n':
											p.Push("\n", 1);
											break;
										case 'r':
											p.Push("\r", 1);
											break;
										case 't':
											p.Push("\t", 1);
											break;
										case '\'':
											p.Push("\'", 1);
											break;
										case '\"':
											p.Push("\"", 1);
											break;
										case '?':
											p.Push("?", 1);
											break;
										default:
											LgiAssert(0);
											return false;
									}
								}
								else
								{
									p.Push(s++, 1);
								}
							}

							Value.Cp = p.NewStr();

							break;
						}
						case OBJ_BINARY:
						{
							if (strnicmp(Equal, "B(", 2) == 0)
							{
								Equal += 2;
								char *Comma = strchr(Equal, ',');
								if (Comma)
								{
									*Comma = 0;
									Size = atoi(Equal);
									Value.Cp = new char[Size];
									if (Value.Cp)
									{
										Comma++;
										for (int i=0; i<Size; i++)
										{
											int High = HexToDec(*Comma++);
											int Low = HexToDec(*Comma++);
											if (High < 0 OR Low < 0) return false;
											Value.Cp[i] = (High << 4) | Low;
										}

										return true;
									}
								}
							}
							
							return false;
							break;
						}
						default:
						{
							return false;
						}
					}

					return true;
				}
			}
		}

		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Object properties class
ObjProperties::ObjProperties()
{
	Current = 0;
	Parent = 0;
	Next = 0;
	Leaf = 0;
}

ObjProperties::ObjProperties(char *n)
{
	Current = 0;
	Parent = 0;
	Next = 0;
	Leaf = 0;

	Name(n);
}

ObjProperties::~ObjProperties()
{
	Empty();
}

ObjProperties &ObjProperties::operator =(ObjProperties &props)
{
	List<Prop>::I n = props.Properties.Start();

	Empty();
	while (n.Each())	
	{
		Prop *p = *n;
		switch (p->Type)
		{
			case OBJ_INT:
			{
				Set(p->Name, p->Value.Int);
				break;
			}
			case OBJ_FLOAT:
			{
				Set(p->Name, p->Value.Dbl);
				break;
			}
			case OBJ_STRING:
			{
				Set(p->Name, p->Value.Cp);
				break;
			}
			case OBJ_BINARY:
			{
				Set(p->Name, p->Value.Cp, p->Size);
				break;
			}
		}
	}

	return *this;
}

Prop *ObjProperties::FindProp(const char *Name)
{
	if (Name)
	{
		List<Prop>::I n = Properties.Start();		
		while (n.Each())
		{
			if (**n == Name)
			{
				return *n;
			}
		}
	}

	return 0;
}

bool ObjProperties::Find(char *Name)
{
	return (Current = FindProp(Name)) != 0;
}

Prop *ObjProperties::GetProp()
{
	return Current;
}

int ObjProperties::KeyType()
{
	if (Current)
	{
		return Current->Type;
	}
	return 0;
}

void *ObjProperties::KeyValue()
{
	if (Current)
	{
		return (void*) &Current->Value;
	}

	return 0;
}

char *ObjProperties::KeyName()
{
	if (Current)
	{
		return Current->Name;
	}

	return 0;
}

bool ObjProperties::FirstKey()
{
	Current = Properties.First();
	return Current != 0;
}

bool ObjProperties::LastKey()
{
	Current = Properties.Last();
	return Current != 0;
}

bool ObjProperties::NextKey()
{
	Current = Properties.Next();
	return Current != 0;
}

bool ObjProperties::PrevKey()
{
	Current = Properties.Prev();
	return Current != 0;
}

bool ObjProperties::DeleteKey(char *Name)
{
	bool Status = false;

	if ( (Name AND Find(Name)) OR
		 (!Name AND Current) )
	{
		Status = Properties.Delete(Current);
		DeleteObj(Current);

		Current = Properties.Current();
	}
	
	return Status;
}

void ObjProperties::Empty()
{
	DeleteObj(Leaf);
	DeleteObj(Next);

	Current = 0;
	
	Properties.DeleteObjects();
}

int ObjProperties::SizeofData()
{
	return (Current) ? Current->Size : 0;
}

bool ObjProperties::Set(const char *Name, int n)
{
	Prop *c = FindProp(Name);
	if (!c)
	{
		c = new Prop(Name);
		if (c)
		{
			Properties.Insert(c);
		}
	}

	if (c)
	{
		c->EmptyData();
		c->Type = OBJ_INT;
		c->Value.Int = n;
		return true;
	}

	return false;
}

bool ObjProperties::Set(const char *Name, double n)
{
	Prop *c = FindProp(Name);
	if (!c)
	{
		c = new Prop(Name);
		if (c)
		{
			Properties.Insert(c);
		}
	}

	if (c)
	{
		c->EmptyData();
		c->Type = OBJ_FLOAT;
		c->Value.Dbl = n;
		return true;
	}

	return false;
}

bool ObjProperties::Set(const char *Name, const char *n)
{
	Prop *c = FindProp(Name);
	if (!c)
	{
		c = new Prop(Name);
		if (c)
		{
			Properties.Insert(c);
		}
	}

	if (c)
	{
		if (n)
		{
			c->EmptyData();
			c->Type = OBJ_STRING;
			c->Size = strlen(n);
			c->Value.Cp = new char[c->Size+1];
			if (c->Value.Cp)
			{
				strcpy(c->Value.Cp, n);
				return true;
			}
		}
		else
		{
			Properties.Delete(c);
			DeleteObj(c);
		}
	}

	return false;
}

bool ObjProperties::Set(const char *Name, void *Data, int Len)
{
	if (Data AND Len > 0)
	{
		Prop *c = FindProp(Name);
		if (!c)
		{
			c = new Prop(Name);
			if (c)
			{
				Properties.Insert(c);
			}
		}

		if (c)
		{
			c->EmptyData();
			c->Size = Len;
			c->Type = OBJ_BINARY;
			c->Value.Cp = new char[Len];
			if (c->Value.Cp)
			{
				memcpy(c->Value.Cp, Data, Len);
				return true;
			}
		}
	}

	return false;
}

bool ObjProperties::Set(Prop *p)
{
	if (p)
	{
		switch (p->Type)
		{
			case OBJ_INT:
			{
				Set(p->Name, p->Value.Int);
				break;
			}
			case OBJ_FLOAT:
			{
				Set(p->Name, p->Value.Dbl);
				break;
			}
			case OBJ_STRING:
			{
				Set(p->Name, p->Value.Cp);
				break;
			}
			case OBJ_BINARY:
			{
				Set(p->Name, p->Value.Cp, p->Size);
				break;
			}
		}

		return true;
	}

	return false;
}

bool ObjProperties::Get(const char *Name, int &n)
{

	Prop *c = FindProp(Name);
	if (c AND c->Type == OBJ_INT)
	{
		n = c->Value.Int;
		return true;
	}

	return false;
}

bool ObjProperties::Get(const char *Name, double &n)
{
	Prop *c = FindProp(Name);
	if (c AND c->Type == OBJ_FLOAT)
	{
		n = c->Value.Dbl;
		return true;
	}

	return false;
}

bool ObjProperties::Get(const char *Name, char *&n)
{
	Prop *c = FindProp(Name);
	if (c AND c->Type == OBJ_STRING)
	{
		n = c->Value.Cp;
		return true;
	}

	return false;
}

bool ObjProperties::Get(const char *Name, void *&Data, int &Len)
{
	Prop *c = FindProp(Name);
	if (c AND c->Type == OBJ_BINARY)
	{
		Data = c->Value.Cp;
		Len = c->Size;
		return true;
	}

	return false;
}

bool ObjProperties::Get(Prop *&p)
{
	if (Current)
	{
		p = Current;
		return true;
	}

	return false;
}

/*

File format:

class ObjProperties {

	ushort Flags;				// 0x1 = has leaf, 0x2 = has next
	ushort Entries;				// number of properties
	ushort LengthOfName;
	char Name[LengthOfName];		// includes terminator
	
	class Prop {
		ushort LengthOfName;
		char Name[LengthOfName];	// includes terminator
		int Type;
		MultiValue Value;

	} Properties[Entries];

	ushort Terminator = 0xABCD;		// End of list marker (error checking)

	if (Flags & PROPLST_LEAF)
	{
		ObjProperties Leaf;
	}

	if (Flags & PROPLST_NEXT)
	{
		ObjProperties Next;
	}
}

*/
#define PROPLST_LEAF			0x1
#define PROPLST_NEXT			0x2

bool ObjProperties::Serialize(GFile &f, bool Write)
{
	bool Status = false;
	ushort Us;

	if (Write)
	{
		Us = 0;
		if (Leaf) Us |= PROPLST_LEAF;
		if (Next) Us |= PROPLST_NEXT;
		f << Us;
		Us = Properties.Length();
		f << Us;
		if (Name())
		{
			Us = strlen(Name()) + 1;
			f << Us;
			f.Write(Name(), Us);
		}
		else
		{
			Us = 0;
			f << Us;
		}

		Prop *p = Properties.First();
		while (p)
		{
			p->Serialize(f, Write);
			p = Properties.Next();
		}

		Us = 0xABCD;
		f << Us;

		Status = true;
		if (Leaf)
		{
			Status = Leaf->Serialize(f, Write);
		}

		if (Status AND Next)
		{
			Status = Next->Serialize(f, Write);
		}
	}
	else // Read
	{
		ushort Flags = 0;
		ushort Entries = 0;
		char *Str = 0;

		f >> Flags;
		f >> Entries;
		f >> Us;
		if (Us > 0)
		{
			Str = new char[Us];
			if (Str)
			{
				f.Read(Str, Us);
				Name(Str);
				DeleteObj(Str);
			}
			else
			{
				return false;
			}
		}
		else
		{
			Name(NULL);
		}

		for (int i=0; i<Entries; i++)
		{
			Prop *p = new Prop;
			if (p)
			{
				if (!p->Serialize(f, Write))
				{
					break;
				}

				if (Find(p->Name))
				{
					DeleteKey();
				}

				Properties.Insert(p);
			}
			else
			{
				return false;
			}
		}

		f >> Us;

		Status = (Us == 0xABCD);

		if (Status)
		{
			if (Flags & PROPLST_LEAF)
			{
				if (!Leaf)
				{
					Leaf = new ObjProperties;
				}

				if (Leaf)
				{
					Status = Leaf->Serialize(f, Write);
					Leaf->Parent = this;
				}
				else
				{
					return false;
				}
			}

			if (Status AND (Flags & PROPLST_NEXT))
			{
				if (!Next)
				{
					Next = new ObjProperties;
				}

				if (Next)
				{
					Status = Next->Serialize(f, Write);
					Next->Parent = this;
				}
				else
				{
					return false;
				}
			}
		}
	}

	return Status;
}

int Prop_Compare(Prop *a, Prop *b, NativeInt data)
{
	return stricmp(a->Name, b->Name);
}

bool ObjProperties::SerializeText(GFile &f, bool Write)
{
	const char *s = "[Properties]";

	if (Write)
	{
		Properties.Sort(Prop_Compare, (NativeInt)this);

		f.SetSize(0);
		f.Write(s, strlen(s));
		f.Write((char*)"\r\n", 2);

		Prop *p = Properties.First();
		while (p)
		{
			p->SerializeText(f, Write);
			p = Properties.Next();
		}
	}
	else
	{
		char Str[256];
		f.ReadStr(Str, sizeof(Str));

		if (strnicmp(s, Str, strlen(s))==0)
		{
			while (!f.Eof())
			{
				Prop *p = new Prop;
				if (p AND 
					p->SerializeText(f, Write))
				{
					Properties.Insert(p);
				}
				else
				{
					DeleteObj(p);
				}
			}
		}
		else return 0;
	}

	return true;
}

ObjProperties *ObjProperties::CreateNext(char *n)
{
	ObjProperties *New = new ObjProperties;
	if (New)
	{
		DeleteObj(Next);
		Next = New;
		Next->Parent = this;
		Next->Name((n) ? n : (char*)"Blank");
	}

	return Next;
}

ObjProperties *ObjProperties::CreateLeaf(char *n)
{
	ObjProperties *New = new ObjProperties;
	if (New)
	{
		DeleteObj(Leaf);
		Leaf = New;
		Leaf->Parent = this;
		Leaf->Name((n) ? n : (char*)"Blank");
	}

	return Leaf;
}

ObjProperties *ObjProperties::FindLeaf(char *n)
{
	char Str[128];
	ObjProperties *p = Leaf;

	strcpy(Str, n);
	char *Dot = strchr(Str, '.');
	char *Token = Str;
	char *NextToken = 0;
	if (Dot)
	{
		NextToken = Dot + 1;
		*Dot = 0;
	}

	while (p)
	{
		if (*p == Token)
		{
			if (NextToken)
			{
				return p->FindLeaf(NextToken);
			}
			return p;
		}

		p = p->GetNext();
	}

	return p;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
ObjTree::ObjTree()
{
	Root = 0;
}

ObjTree::~ObjTree()
{
	DeleteObj(Root);
}

ObjProperties *ObjTree::GetRoot()
{
	if (!Root)
	{
		Root = new ObjProperties;
	}

	return Root;
}

ObjProperties *ObjTree::GetLeaf(char *Name, bool Create)
{
	if (Name)
	{
		ObjProperties *pList = Root;
		ObjProperties *pParent = 0;
		ObjProperties **pp = &Root;
		char *Start = Name;
		char *End = Name;
		char Token[256];

		if (Name[0] == 0)
		{
			return Root;
		}

		while (*Start)
		{
			for (End = Start; *End AND *End != '.'; End++);

			if (*End == '.')
			{
				int Size = End - Start;
				memcpy(Token, Start, Size);
				Token[Size] = 0;
			}
			else
			{
				strcpy(Token, Start);
			}

			if (!(*pp))
			{
				pList = (*pp) = new ObjProperties(Token);
				if (pList)
				{
					pList->Parent = pParent;
				}
			}
			else
			{
				pList = *pp;
			}

			pParent = pList;

			while (pList AND *pList != Token)
			{
				if (pList->GetNext())
				{
					pList = pList->GetNext();
				}
				else if (Create)
				{
					pList = pList->CreateNext(Token);
					if (pList)
					{
						pList->Parent = pParent;
					}
				}
				else
				{
					return 0;
				}
			}

			if (pList AND *pList == Token)
			{
				pp = &pList->GetLeaf();
			}

			Start = End + ((*End) ? 1 : 0);
		}
		
		return pList;
	}

	return NULL;
}

char *StrSnip(char *d, char *s)
{
	if (s)
	{
		strcpy(d, s);
		for (int i=strlen(d)-1; i>=0; i--)
		{
			if (d[i] == '.')
			{
				d[i] = 0;
				return d + (i + 1);
			}
		}
	}

	return 0;
}

bool ObjTree::Set(char *Name, int n)
{
	char Buf[256];
	char *Key = StrSnip(Buf, Name);
	if (Key)
	{
		ObjProperties *Obj = GetLeaf(Buf, true);
		if (Obj)
		{
			return Obj->Set(Key, n);
		}
	}

	return false;
}

bool ObjTree::Set(char *Name, double n)
{
	char Buf[256];
	char *Key = StrSnip(Buf, Name);
	if (Key)
	{
		ObjProperties *Obj = GetLeaf(Buf, true);
		if (Obj)
		{
			return Obj->Set(Key, n);
		}
	}

	return false;
}

bool ObjTree::Set(char *Name, char *n)
{
	char Buf[256];
	char *Key = StrSnip(Buf, Name);
	if (Key)
	{
		ObjProperties *Obj = GetLeaf(Buf, true);
		if (Obj)
		{
			return Obj->Set(Key, n);
		}
	}

	return false;
}

bool ObjTree::Get(char *Name, int &n)
{
	char Buf[256];
	char *Key = StrSnip(Buf, Name);
	if (Key)
	{
		ObjProperties *Obj = GetLeaf(Buf);
		if (Obj AND Obj->Find(Key))
		{
			return Obj->Get(Key, n);
		}
	}

	return false;
}

bool ObjTree::Get(char *Name, double &n)
{
	char Buf[256];
	char *Key = StrSnip(Buf, Name);
	if (Key)
	{
		ObjProperties *Obj = GetLeaf(Buf);
		if (Obj AND Obj->Find(Key))
		{
			return Obj->Get(Key, n);
		}
	}

	return false;
}

bool ObjTree::Get(char *Name, char *&n)
{
	char Buf[256];
	char *Key = StrSnip(Buf, Name);
	if (Key)
	{
		ObjProperties *Obj = GetLeaf(Buf);
		if (Obj AND Obj->Find(Key))
		{
			return Obj->Get(Key, n);
		}
	}

	return false;
}

void PrintObj(ObjProperties *p, int i)
{
	printf("\n");
	
	if (p)
	{
		char Buf[256];

		while (p)
		{
			memset(Buf, ' ', sizeof(Buf));
			sprintf(Buf+i, "%s", p->Name());
			puts(Buf);

			i += 3;
			if (p->FirstKey())
			{
				do {
					memset(Buf, ' ', sizeof(Buf));

					MultiValue *Val = (MultiValue*) p->KeyValue();

					switch (p->KeyType())
					{
						case OBJ_INT:
						{
							sprintf(Buf+i, "int %s = %i", p->KeyName(), Val->Int);
							break;
						}

						case OBJ_FLOAT:
						{
							sprintf(Buf+i, "double %s = %f", p->KeyName(), Val->Dbl);
							break;
						}

						case OBJ_STRING:
						{
							sprintf(Buf+i, "char *%s = '%s'", p->KeyName(), Val->Cp);
							break;
						}
						default:
						{
							sprintf(Buf+i, "<unknown key type>\n", p->KeyName(), Val->Cp);
							break;
						}
					}
					puts(Buf);

				} while (p->NextKey());
			}

			if (p->GetLeaf())
			{
				PrintObj(p->GetLeaf(), i);
			}

			i -= 3;

			p = p->GetNext();
		}
	}
}

void ObjTree::Print()
{
	if (Root)
	{
		PrintObj(Root, 0);
	}
}

bool ObjTree::Serialize(GFile &f, bool Write)
{
	if (!Root AND !Write)
	{
		Root = new ObjProperties;
	}
	
	if (Root)
	{
		return Root->Serialize(f, Write);
	}

	return false;
}

char *Trim(char *s)
{
	const char *Delim = "\r\n\t ";
	while (s AND *s AND strchr(Delim, *s))
	{
		s++;
	}

	return s;
}

bool IsAlpha(char c)
{
	return ((c >= 'a' AND c <= 'z') OR (c >= 'A' AND c <= 'Z'));
}

bool IsDigit(char c)
{
	return (c >= '0' AND c <= '9');
}

bool ObjTree::SerializeObj(GFile &f, bool Write)
{
	bool Status = false;

	char Buf[256];

	if (Write)
	{
	}
	else // Read
	{
		char Group[256] = "";
		ObjProperties *Props = 0;

		while (f.ReadStr(Buf, sizeof(Buf)) > 0)
		{
			char *Comment = strstr(Buf, "//");

			if (Comment)
			{
				*Comment = 0;
			}

			char *s = Trim(Buf);
			if (s[0] == '[')
			{
				int i;

				s++;
				if (strchr(s, ']'))
				{
					for (i=0; i<256 AND *s != ']'; i++)
					{
						Group[i] = *s++;
					}

					Group[i] = 0;

					Props = GetLeaf(Group, true);
				}
				else
				{
					// not a valid group header
				}
			}
			else if (Props)
			{
				if (IsAlpha(*s))
				{
					char Variable[256];
					char *Var = s;
					int VarLen = 0;

					// scan past variable name
					while (IsAlpha(*s) OR IsDigit(*s))
					{
						VarLen++;
						s++;
					}

					memcpy(Variable, Var, VarLen);
					Variable[VarLen] = 0;

					s = Trim(s);
					if (*s == '=')
					{
						s++;

						s = Trim(s);

						if (*s)
						{
							if (IsDigit(*s))
							{
								// int OR double
								bool PeriodFound = strchr(s, '.') != NULL;

								if (PeriodFound)
								{
									// double
									Props->Set(Variable, atof(s));
								}
								else
								{
									// int
									if (strnicmp(s, "0x", 2) == 0)
									{
										// hex
										char NumToHex[] = "0123456789ABCDEFabcdef";
										int Len = 0;
										
										s += 2;
										strupr(s);
										for (;s[Len] AND strchr(NumToHex, s[Len]); Len++);
										Len--;

										if (Len > 0)
										{
											int Val = 0;
											int Shift = 0;
											for (; Len >= 0 AND Shift <= 28; Len--, Shift+=4)
											{
												Val |= (((s[Len] >= '0' AND s[Len] <= '9') ? s[Len] - '0' : s[Len] - 'A' + 10)) << Shift;
											}

											Props->Set(Variable, Val);
										}
									}
									else
									{
										// decimal
										Props->Set(Variable, atoi(s));
									}
								}
							}
							else if (*s == '"')
							{
								// string
								s++;
								char *End = strchr(s, '"');
								if (End)
								{
									*End = 0;
									Props->Set(Variable, s);
								}
							}
						}
					}
				}
			}
		}
		
		Status = true;
	}

	return Status;
}

bool ObjTree::SerializeText(GFile &f, bool Write)
{
	return SerializeObj(f, Write);
}

