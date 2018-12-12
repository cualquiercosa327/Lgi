#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "Lgi.h"
#include "GXmlTree.h"
#include "GVariant.h"
#include "GToken.h"

static char GXmlHeader[] = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n";

//////////////////////////////////////////////////////////////////////////////
int _Pools = 0;
int _Normals = 0;

char *GXmlAlloc::Alloc(const char *s, ssize_t len)
{
	if (!s) return NULL;
	if (len < 0) len = (int)strlen(s);
	ssize_t bytes = len + 1;
	char *p = (char*) Alloc(LGI_ALLOC_ALIGN(bytes));
	if (!p) return 0;
	memcpy(p, s, len);
	p[len] = 0;
	return p;
}

class XmlPoolAlloc : public GXmlAlloc
{
	struct Block
	{
		size_t Used, Len;
		char *Ptr;

		Block()
		{
			Used = Len = 0;
			Ptr = 0;
		}

		~Block()
		{
			DeleteArray(Ptr);
		}

		bool Has(size_t Bytes)
		{
			return Used + Bytes <= Len;
		}
	};

	GArray<Block> Mem;

public:
	XmlPoolAlloc()
	{
		_Pools++;
		// LgiTrace("%p::XmlPoolAlloc _Pools=%i, Refs=%i\n", this, _Pools, _GetCount());
	}

	~XmlPoolAlloc()
	{
		_Pools--;
		// LgiTrace("%p::~XmlPoolAlloc _Pools=%i, Refs=%i\n", this, _Pools, _GetCount());
	}

	void *Alloc(size_t Size)
	{
		Block *b = 0;

		int BlockSize = 32 << 10;
		if (Mem.Length() == 0 || !Mem[Mem.Length()-1].Has(Size))
		{
			// New block
			b = &Mem.New();
			b->Len = BlockSize;
			b->Used = 0;
			b->Ptr = new char[b->Len];
		}
		else b = &Mem[Mem.Length()-1];

		char *p = b->Ptr + b->Used;
		b->Used += Size;

		return p;
	}

	void Free(void *Ptr)
	{
	}
};

class XmlNormalAlloc : public GXmlAlloc
{
public:
	XmlNormalAlloc()
	{
		_Normals++;
	}

	~XmlNormalAlloc()
	{
		_Normals--;
	}

	void *Alloc(size_t Size)
	{
		return new char[Size];
	}

	void Free(void *Ptr)
	{
		delete [] ((char*)Ptr);
	}	
};

//////////////////////////////////////////////////////////////////////////////
class GXmlTreePrivate
{
public:
	GXmlFactory *Factory;
	GXmlTag *Current;
	GStreamI *File;
	GString Error;
	int Flags;
	LHashTbl<ConstStrKey<char,false>,char16> Entities;
	LHashTbl<ConstStrKey<char,false>,bool> NoChildTags;
	GArray<char> Buf;
	Progress *Prog;
	
	char *StyleFile;
	char *StyleType;

	bool NoDom()
	{
		return TestFlag(Flags, GXT_NO_DOM);
	}

	GXmlTreePrivate()
	{
		Factory = 0;
		File = 0;
		Flags = 0;
		StyleFile = 0;
		StyleType = 0;
		Current = NULL;
		Prog = NULL;

		Entities.Add("lt", '<');
		Entities.Add("gt", '>');
		Entities.Add("amp", '&');
		Entities.Add("quot", '\"');
		Entities.Add("apos", '\'');
	}
	
	~GXmlTreePrivate()
	{
		DeleteArray(StyleType);
		DeleteArray(StyleFile);
	}
};

//////////////////////////////////////////////////////////////////////////////
const char *EncodeEntitiesAttr	= "\'<>\"\n";
const char *EncodeEntitiesContent	= "\'<>\"";

char *GXmlTree::EncodeEntities(char *s, ssize_t len, const char *extra_characters)
{
	GStringPipe p;
	if (EncodeEntities(&p, s, len, extra_characters))
	{
		return p.NewStr();
	}

	return 0;
}

bool GXmlTree::EncodeEntities(GStreamI *to, char *start, ssize_t len, const char *extra_characters)
{
	if (!start || !to)
		return 0;

	int Amp = (d->Flags & GXT_NO_ENTITIES) ? 10000000 : '&';

	char *end = start + len;
	for (char *s = start; s && *s;)
	{
		char *e = s;
		while (	*e &&
				*e != Amp &&
				*e != '\r' &&
				(!extra_characters || !strchr(extra_characters, *e)) &&
				(len < 0 || e < end))
			e++;
		
		if (e == end || *e)
		{
			to->Write(s, (int) (e - s));
			
			if (e == end)
				break;
			
			switch (*e)
			{
				case '<':
					to->Write((char*)"&lt;", 4);
					break;
				case '>':
					to->Write((char*)"&gt;", 4);
					break;
				case '&':
					to->Write((char*)"&amp;", 5);
					break;
				case '\'':
					to->Write((char*)"&apos;", 6);
					break;
				case '\"':
					to->Write((char*)"&quot;", 6);
					break;
				case '\r':
					// Do nothing...
					break;
				default:
				{
					char Ent[32];
					sprintf_s(Ent, sizeof(Ent), "&#%i;", (uchar)*e);
					to->Write(Ent, strlen(Ent));
					break;
				}
			}
			
			s = e + 1;
		}
		else
		{
			to->Write(s, strlen(s));
			break;
		}
	}

	return true;
}

char *GXmlTree::DecodeEntities(GXmlAlloc *Alloc, char *In, ssize_t Len)
{
	if (!In || !Alloc)
	{
		LgiAssert(!"Param error");
		return NULL;
	}
	
	// char *OriginalIn = In;

	// Setup temporary buffer
	ssize_t BufSize = Len + 32;
	int BufR = BufSize & 0xff;
	if (BufR) BufSize += 256 - BufR;
	if (d->Buf.Length() < BufR)
		d->Buf.Length(BufR);

	char *Start = &d->Buf[0];
	char *Out = Start;
	char *OutEnd = Out + d->Buf.Length();
	char *InEnd = In + Len;
	while (*In && In < InEnd)
	{
		if (Out >= OutEnd - 4)
		{
			// Running out of space, extend the buffer.
			size_t Cur = Out - Start;
			d->Buf.Length(d->Buf.Length() + 256);
			Start = &d->Buf[0];
			Out = Start + Cur;
			OutEnd = Start + d->Buf.Length();
		}

		if (*In != '&')
		{
			*Out++ = *In++;
			continue;
		}

		In++; // Skip the '&'		
		char16 c16 = 0;
		if (*In == '#')
		{
			In++;
			if (*In == 'x')
				c16 = htoi(++In);
			else
				c16 = atoi(In);

			In = strchr(In, ';');
			if (In)
			{
				In++;
			}
			else
			{
				LgiAssert(0);
			}
		}
		else
		{
			ssize_t len;
			char *Col = strnchr(In, ';', 16);

			if (Col && (len = (Col - In)) < 16)
			{
				char tmp[16];
				memcpy(tmp, In, len);
				tmp[len] = 0;
				
				c16 = d->Entities.Find(tmp);
				if (c16)
					In = Col + 1;
			}
			if (!c16)
			{
				// Not a real named entity, so just emit the ampersand.
				*Out++ = '&';
			}
		}

		if (c16)
		{
			GAutoString c8(WideToUtf8(&c16, 1));
			if (c8)
			{
				for (char *c = c8; *c; c++)
					*Out++ = *c;
			}
		}
	}

	return Alloc->Alloc(Start, Out - Start);
}

//////////////////////////////////////////////////////////////////////////////
GAutoRefPtr<XmlNormalAlloc> TagHeapAllocator(new XmlNormalAlloc);

GXmlTag::GXmlTag(const char *tag, GXmlAlloc *alloc)
{
	Allocator = alloc ? alloc : TagHeapAllocator;

	Write = false;
	Parent = NULL;
	Content = NULL;

	Tag = Allocator->Alloc(tag);
}

GXmlTag::GXmlTag(const GXmlTag &t)
{
	Allocator = t.Allocator;
	LgiAssert(Allocator != NULL);

	Tag = NULL;
	Write = false;
	Parent = NULL;
	Content = NULL;

	Copy((GXmlTag&)t);
}

GXmlTag::~GXmlTag()
{
	RemoveTag();
	Empty(true);
}

void GXmlTag::EmptyAttributes()
{
	for (int i=0; i<Attr.Length(); i++)
	{
		Allocator->Free(Attr[i].Name);
		Allocator->Free(Attr[i].Value);
	}
	Attr.Length(0);
}

void GXmlTag::EmptyChildren()
{
	GXmlTag *c;
	while ((c = Children.First()))
	{
		LgiAssert(c->Parent == this);
		DeleteObj(c);
	}
}

void GXmlTag::Empty(bool Deep)
{
	EmptyAttributes();
	Allocator->Free(Content);
	SetTag(NULL);
	
	if (Deep)
		EmptyChildren();
}

bool GXmlTag::Copy(GXmlTag &t, bool Deep)
{
	Empty(Deep);
	
	Allocator = t.Allocator;
	Content = NewStr(t.Content);
	Tag = Allocator->Alloc(t.Tag);

	Attr.Length(t.Attr.Length());
	for (int i=0; i<t.Attr.Length(); i++)
	{
		GXmlAttr &a = t.Attr[i];
		Attr[i].Name = Allocator->Alloc(a.Name);
		Attr[i].Value = Allocator->Alloc(a.Value);
	}
	
	if (Deep)
	{
		List<GXmlTag>::I it = t.Children.begin();
		for (GXmlTag *c = *it; it.In(); c = *++it)
		{
			GXmlTag *n = new GXmlTag;
			if (n)
			{
				n->Copy(*c, Deep);
				InsertTag(n);
			}
		}
	}
	
	return true;
}

bool GXmlTag::XPath(GArray<GXmlTag*> &Results, const char *Path)
{
	return false;
}

GXmlTag &GXmlTag::operator =(GXmlTag &t)
{
	Copy(t);
	return *this;
}

GXmlTag *GXmlTag::CreateTag(const char *Name, char *Content)
{
	GXmlTag *c = GetChildTag(Name, true);
	if (c) c->Content = NewStr(Content);
	return c;
}

const char *GXmlTag::GetTag()
{
	return Tag;
}

void GXmlTag::SetTag(const char *Str)
{
	Allocator->Free(Tag);
	Tag = NULL;
	if (Str)
		Tag = Allocator->Alloc(Str);
}

GXmlTag *GXmlTag::GetChildTag(const char *Name, bool Create, const char *TagSeparator)
{
	GToken p(Name, TagSeparator);

	GXmlTag *t = this;
	for (int i=0; i<p.Length() && t; i++)
	{
		GXmlTag *Child = 0;
		char *Part = p[i];
		
		List<GXmlTag>::I n = t->Children.begin();
		for (GXmlTag *c = *n; n; c = *++n)
		{
			if (c->Tag && stricmp(c->Tag, Part) == 0)
			{
				Child = c;
				break;
			}
		}
		if (!Child)
		{
			if (Create)
			{
				t->InsertTag( Child = new GXmlTag(p[i]) );
				if (!Child)
				{
					return 0;
				}
			}
			else return 0;
		}
		t = Child;
	}
	
	return t;
}

bool GXmlTag::GetVariant(const char *Name, GVariant &Value, char *Array)
{
	/* !_stricmp(Name, "'attribute_name'") // Type: String
	*/

	if (Name)
	{
		List<GXmlTag>::I n = Children.begin();
		for (GXmlTag *c = *n; c; c = *++n)
		{
			if (c->Tag && stricmp(c->Tag, Name) == 0)
			{
				Value = (GDom*) c;
				return true;
			}
		}

		char *v = GetAttr(Name);
		if (v)
		{
			if (v[0] == 'b' &&
				v[1] == 'i' &&
				v[2] == 'n' &&
				v[3] == 'a' &&
				v[4] == 'r' &&
				v[5] == 'y' &&
				v[6] == '(')
			{
				Value.Empty();
				Value.Type = GV_BINARY;
				int Len = 0;
				char *Start = v + 7;
				for (char *s = Start; *s && *s != ')'; s++)
					Len++;
				Len >>= 1;

				Value.Value.Binary.Length = Len;
				Value.Value.Binary.Data = new uchar[Len];
				if (Value.Value.Binary.Data)
				{
					char *s = Start;
					for (int i=0; i<Len; i++)
					{
						#define HexToBin()	\
							(*s >= '0' && *s <= '9') ? *s - '0' : \
							((*s >= 'a' && *s <= 'f') ? *s - 'a' + 10  : 0)

						int High = HexToBin();
						s++;
						int Low = HexToBin();
						s++;
						((uint8*)Value.Value.Binary.Data)[i] = High << 4 | Low;						
					}
				}
			}
			else
			{
				Value = v;
			}
			return true;
		}

		if (stricmp(Name, "text") == 0)
		{
			Value = Content;
			return true;
		}
	}

	return false;	
}

bool GXmlTag::SetVariant(const char *Name, GVariant &Value, char *Array)
{
	if (Name)
	{
		switch (Value.Type)
		{
			case GV_NULL:
			{
				DelAttr(Name);
				break;
			}
			case GV_BOOL:
			{
				char i[32];
				sprintf_s(i, sizeof(i), "%i", Value.Value.Bool);
				SetAttr(Name, i);
				break;
			}
			case GV_INT32:
			{
				char i[32];
				sprintf_s(i, sizeof(i), "%i", Value.Value.Int);
				SetAttr(Name, i);
				break;
			}
			case GV_INT64:
			{
				char i[32];
				sprintf_s(i, sizeof(i), LPrintfInt64, Value.Value.Int64);
				SetAttr(Name, i);
				break;
			}
			case GV_DOUBLE:
			{
				char i[32];
				sprintf_s(i, sizeof(i), "%f", Value.Value.Dbl);
				SetAttr(Name, i);
				break;
			}
			case GV_STRING:
			{
				SetAttr(Name, Value.Str());
				break;
			}
			case GV_BINARY:
			{
				GStringPipe p;
				p.Print("binary(");
				for (int i=0; i<Value.Value.Binary.Length; i++)
				{
					p.Print("%02.2x", ((uint8*)Value.Value.Binary.Data)[i]);
				}
				p.Print(")");
				char *s = p.NewStr();
				SetAttr(Name, s);
				DeleteArray(s);
				break;
			}
			default:
			{
				LgiAssert(0);
				return false;
			}
		}
		
		return true;
	}
	
	return false;
}

void GXmlTag::InsertTag(GXmlTag *t)
{
	if (t)
	{
		t->RemoveTag();
		t->Parent = this;		
		Children.Insert(t);
	}
}

void GXmlTag::RemoveTag()
{
	if (Parent)
	{
		Parent->Children.Delete(this);
	}
	Parent = 0;	
}

int64 GXmlTag::CountTags()
{
	uint64 c = 1;
	List<GXmlTag>::I it = Children.begin();
	for (GXmlTag *t = *it; t; t = *++it)
	{
		c += t->CountTags();
	}

	return c;
}

bool GXmlTag::Dump(int Depth)
{
	#define Tabs() { for (int i=0; i<Depth; i++) printf("    "); }
	
	Tabs();
	LgiTrace("Tag='%s'\n", Tag);
	Depth++;

	for (int i=0; i<Attr.Length(); i++)
	{
		Tabs();
		LgiTrace("Attr: %s='%s'\n", Attr[i].Name, Attr[i].Value);
	}	
	LgiTrace("\n");
	
	for (GXmlTag *t=Children.First(); t; t=Children.Next())
	{
		t->Dump(Depth + 1);
	}
	
	LgiTrace("\n");
	Depth--;
	
	#undef Tabs
	return true;
}

int GXmlTag::GetContentAsInt(int Default)
{
	return Content ? atoi(Content) : Default;
}

bool GXmlTag::SetContent(int i)
{
	char s[32];
	return SetContent(s, sprintf_s(s, sizeof(s), "%i", i));
}

GXmlAttr *GXmlTag::_Attr(const char *Name, bool Wr)
{
	if (!Name)
		return 0;

	// Validate the name...
	for (const char *c = Name; *c; c++)
	{
		if (!IsAlpha(*c) &&
			!IsDigit(*c) &&
			!strchr(":-_()", *c))
		{
			LgiAssert(!"Invalid attribute name.");
			return 0;
		}
	}

	if (Wr && !Allocator)
		Allocator = new XmlNormalAlloc;

	for (int i=0; i<Attr.Length(); i++)
	{
		GXmlAttr *a = &Attr[i];
		if (a->Name && stricmp(a->Name, Name) == 0)
		{
			if (Wr)
			{
				Allocator->Free(a->Value);
				a->Value = 0;
			}
			return a;
		}
	}

	if (!Wr)
		return 0;

	// Create
	GXmlAttr &n = Attr.New();
	n.Name = Allocator->Alloc(Name);
	return &n;
}

bool GXmlTag::DelAttr(const char *Name)
{
	for (int i=0; i<Attr.Length(); i++)
	{
		GXmlAttr &a = Attr[i];
		if (a.Name && !stricmp(a.Name, Name))
		{
			Allocator->Free(a.Name);
			Allocator->Free(a.Value);
			Attr.DeleteAt(i);
			return true;
		}
	}
	return false;
}

bool GXmlTag::SetContent(const char *s, ssize_t len)
{
	char *n = s ? Allocator->Alloc(s, len > 0 ? len : strlen(s)) : NULL;
	if (Content)
	{
		Allocator->Free(Content);
		Content = NULL;
	}
	Content = n;
	return s ? Content != NULL : true;
}

char *GXmlTag::GetAttr(const char *n)
{
	GXmlAttr *a = _Attr(n, false);
	return a ? a->Value : 0;	
}

int GXmlTag::GetAsInt(const char *n)
{
	GXmlAttr *a = _Attr(n, false);
	return a ? atoi(a->Value) : -1;	
}

bool GXmlTag::SetAttr(const char *n, const char *Value)
{
	GXmlAttr *a = _Attr(n, true);
	if (a)
	{
		a->Value = Allocator->Alloc(Value);
		return true;
	}
	return false;
}

bool GXmlTag::SetAttr(const char *n, int Value)
{
	GXmlAttr *a = _Attr(n, true);
	if (a)
	{
		char s[32];
		sprintf_s(s, sizeof(s), "%i", Value);
		a->Value = Allocator->Alloc(s);
		return true;
	}
	return false;
}

bool GXmlTag::SetAttr(const char *n, int64 Value)
{
	GXmlAttr *a = _Attr(n, true);
	if (a)
	{
		char s[32];
		sprintf_s(s, sizeof(s), LPrintfInt64, Value);
		a->Value = Allocator->Alloc(s);
		return true;
	}
	return false;
}

bool GXmlTag::SerializeAttr(const char *Attr, int &Int)
{
	GXmlAttr *a = _Attr(Attr, Write);
	if (a)
	{
		if (Write)
		{
			char s[256];
			sprintf_s(s, sizeof(s), "%i", Int);
			a->Value = Allocator->Alloc(s);
			return a->Value != 0;
		}
		else if (a->Value)
		{
			Int = atoi(a->Value);
			return true;
		}
	}
	return false;
}

bool GXmlTag::SerializeAttr(const char *Name, char *&Str)
{
	GXmlAttr *a = _Attr(Name, Write);
	if (a)
	{
		if (Write)
		{
			if (ValidStr(Str))
			{
				a->Value = Allocator->Alloc(Str);
			}
			else
			{
				return DelAttr(Name);
			}
			
			return a->Value != 0;
		}
		else if (a->Value)
		{
			Str = NewStr(a->Value);
			return true;
		}
	}
	return false;
}

bool GXmlTag::SerializeAttr(const char *Attr, double &Dbl)
{
	GXmlAttr *a = _Attr(Attr, Write);
	if (a)
	{
		if (Write)
		{
			char s[256];
			sprintf_s(s, sizeof(s), "%f", Dbl);
			a->Value = Allocator->Alloc(s);
			return a->Value != 0;
		}
		else if (a->Value)
		{
			Dbl = atof(a->Value);
			return true;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
GXmlTree::GXmlTree(int Flags)
{
	d = new GXmlTreePrivate;
	d->Flags = Flags;
}

GXmlTree::~GXmlTree()
{
	DeleteObj(d);
}

static const char *White = " \t\r\n";
#define SkipWhiteSpace(s) while (*s && strchr(White, *s)) s++

void GXmlTag::ParseAttribute(GXmlTree *Tree, GXmlAlloc *Alloc, char *&t, bool &NoChildren, bool &TypeDef)
{
	while (*t && *t != '>' && *t != '?')
	{
		// Skip white
		SkipWhiteSpace(t);
		
		if (*t == '>')
			break;

		if (*t == '/')
		{
			t++;
			NoChildren = true;
			break;
		}
		
		char *AttrName = t;

		if (TypeDef)
		{
			if (*t == '\"')
			{
				char *Start = ++t;
				t = strchr(t, '\"');
				if (t)
				{
					if (t > Start)
					{
						GXmlAttr &At = Attr.New();
						At.Name = Alloc->Alloc(Start, t - Start);
					}

					t++;
					continue;
				}
				else
				{
					break;
				}
			}
			else if (*t == '[')
			{
				// Read all defs into children of this tag
				t++;
				while (*t && *t != ']')
				{
					bool NoChildren = false;
					GXmlTag *c = Tree->Parse(0, Alloc, t, NoChildren, true);
					if (c)
					{
						InsertTag(c);
					}
					else break;							
				}

				if (*t == ']')
					t++;
				continue;
			}
		}

		// Goto the end of the attr name
		while
		(
			*t &&
			(
				IsAlpha(*t) ||
				IsDigit(*t) ||
				strchr("-._:()", *t) != 0
			)
		)
		{
			t++;
		}

		if (t > AttrName)
		{
			GXmlAttr &At = Attr.New();
			At.Name = Alloc->Alloc(AttrName, t-AttrName);
			
			// Skip white
			SkipWhiteSpace(t);
			
			// Look for equals
			if (*t == '=')
			{
				t++;
				
				// Skip white
				SkipWhiteSpace(t);
				
				if (strchr("\"\'", *t))
				{
					// Delimited string
					char Delim = *t++;
					char *End = strchr(t, Delim);
					if (End)
					{
						if (Tree->d->Flags & GXT_NO_ENTITIES)
						{
							At.Value = Alloc->Alloc(t, End - t);
						}
						else
						{
							At.Value = Tree->DecodeEntities(Alloc, t, End - t);
						}

						/*
						if (At.Value)
						{
							// Strip out '\r' and '\n' in the attribute value.
							char *i = At.Value, *o = At.Value;
							do
							{
								if (*i != '\n' && *i != '\r')
									*o++ = *i;
								else
									*o++ = ' ';
							}
							while (*i++);
						}
						*/

						t = End + 1;
					}
					else
					{
						break;
					}
				}
				else
				{
					// Non delimited string
					char *End = t;
					while (*End && !strchr(White, *End) && *End != '>'  && *End != '/')
                        End++;
					At.Value = Tree->DecodeEntities(Alloc, t, End - t);
					t = End;
				}
			}
		}
		else
		{
			LgiTrace("%s:%i - Error tagname='%s'\n", _FL, Tag);
			LgiAssert(0);
			break;
		}

		// Skip white
		SkipWhiteSpace(t);
	}
}

GXmlTag *GXmlTree::Parse(GXmlTag *Tag, GXmlAlloc *Alloc, char *&t, bool &NoChildren, bool InTypeDef)
{
	bool KeepWs = TestFlag(d->Flags, GXT_KEEP_WHITESPACE);
	char *Start = t;
	GStringPipe Before;
		
	// Skip white
ParsingStart:
	while (*t)
	{
		if (*t == '<' && t[1] != ' ')
		{
			if (t[1] == '!')
			{
				if (t[2] == '-' &&
					t[3] == '-')
				{
					// Start of comment
					if (KeepWs)
					{
						Before.Write(Start, t - Start);
					}

					char *End = strstr(t + 3, "-->");
					if (End)
					{
						t += 3;
						OnParseComment(Tag ? Tag : d->Current, t, End - t);
						t = End + 2;
						if (KeepWs)
						{
							Start = t;
						}
					}
					else
					{
						t = 0;
						break;
					}
				}
				else
				{
					char *End = strchr(t + 1, '>');
					if (End)
					{
						t = End;
					}
				}
			}
			else
			{
				break;
			}
		}

		t++;
	}

	if (KeepWs && t > Start)
	{		
		Before.Write(Start, t - Start);

		if (Before.GetSize() > 0)
		{
			GXmlTag *PreContent = d->Factory ? d->Factory->Create(0) : new GXmlTag;
			if (PreContent)
			{
				PreContent->Allocator = Alloc;

				NoChildren = true;

				if (d->Flags & GXT_NO_ENTITIES)
					PreContent->Content = Before.NewStr();
				else
				{
					GAutoString Tmp(Before.NewStr());
					GAutoRefPtr<GXmlAlloc> LocalAlloc(new XmlNormalAlloc);
					PreContent->Content = DecodeEntities(Tag ? Tag->Allocator : LocalAlloc, Tmp, strlen(Tmp));
				}

				return PreContent;
			}
		}
	}

	if (*t && *t == '<')
	{
		t++;

		SkipWhiteSpace(t);
		
		// Store tagname start
		char *TagName = t;
		// bool TypeDef = *t == '!';

		if (*t == '/')
			t++;

		while
		(
			*t &&
			(
				IsAlpha(*t) ||
				*t == '!' ||
				*t == '?' ||
				*t == '-' ||
				*t == '_' ||
				*t == ':' ||
				*t == '.' ||
				*t == '[' ||
				*t == ']' ||
				(t > TagName && IsDigit(*t))
			)
		)
		{
			t++;
		}

		if (t > TagName)
		{
			if (TagName[0] == '?')
			{
				GAutoString TmpStr(NewStr(TagName, t - TagName));
				GXmlTag Temp(TmpStr, Alloc);
				TmpStr.Reset();
				bool bTrue = true;

				while (*t)
				{				
					SkipWhiteSpace(t);
					if (t[0] == '>' ||
						(t[0] == '?' && t[1] == '>'))
					{
						break;
					}
				
					Temp.ParseAttribute(this, Alloc, t, bTrue, InTypeDef);
				}

				if (Temp.Tag)
				{
					if (stricmp(Temp.Tag, "?xml-stylesheet") == 0)
					{
						char *Type = Temp.GetAttr("type");
						char *File = Temp.GetAttr("href");
						if (Type && File)
						{
							SetStyleFile(File, Type);
						}
					}
				}

				Before.Empty();
				goto ParsingStart;
			}
			else
			{
				// Create the tag object
				char Old = *t;
				*t = 0;
				if (!Tag)
				{
					Tag = d->Factory ? d->Factory->Create(TagName) : new GXmlTag;
					if (!Tag)
						return 0;
						
					Tag->Allocator = Alloc;
				}
				*t = Old;
				
				// Parse attributes into tag
				if (Tag)
				{	
					Tag->Empty(false);
					LgiAssert(Tag->Tag == NULL);
					Tag->Tag = Tag->Allocator->Alloc(TagName, t - TagName);
					NoChildren = Tag->Tag ? Tag->Tag[0] == '?' : false;
					
					Tag->ParseAttribute(this, Alloc, t, NoChildren, InTypeDef);
					
					// Skip white
					SkipWhiteSpace(t);
					
					if (*t == '>')
						t++;
				}
				
				char *ContentStart = t;
				while
				(
					*t &&
					*t != '<' &&
					(!InTypeDef || *t != ']')
				)
				{
					t++;
				}
				if (t > ContentStart)
				{
					if (d->Flags & GXT_NO_ENTITIES)
					{
						Tag->Content = NewStr(ContentStart, t - ContentStart);
					}
					else
					{
						Tag->Content = DecodeEntities(Tag->Allocator, ContentStart, t - ContentStart);
					}

					if (!TestFlag(d->Flags, GXT_KEEP_WHITESPACE) && !ValidStr(Tag->Content))
					{
						Tag->Allocator->Free(Tag->Content);
					}
				}
			}
		}
		else if (*t)
		{
			printf("Xml: stopped at '%-20s'\n", t);
			LgiAssert(0);
		}
	}
	
	return Tag;
}

bool GXmlTree::Read(GXmlTag *Root, GStreamI *File, GXmlFactory *Factory)
{
	if (!Root)
	{
		d->Error = "No root argument.";
		return false;
	}

	if (!File)
	{
		d->Error = "No input stream argument.";
		return false;
	}
	
	GString t = Root->Tag;
	Root->SetTag(NULL);	
	GAutoRefPtr<GXmlAlloc> Allocator(new XmlPoolAlloc);
	Root->Allocator = Allocator;
	Root->SetTag(t);

	int64 Len = File->GetSize();
	if (Len <= 0)
	{
		d->Error.Printf("Input stream is empty: %" PRId64 " bytes.\n", Len);
		return false;
	}

	char *Str = new char[Len+1];
	if (!Str)
	{
		d->Error = "Alloc error.";
		return false;
	}

	ssize_t r = File->Read(Str, Len);
	if (r <= 0)
	{
		d->Error.Printf("Failed to read from input stream: %zd\n", r);
		return false;
	}
	Str[r] = 0;
					
	char *Ptr = Str;
	d->Factory = Factory;
	d->Current = Root;
					
	bool First = true;
	while (d->Current && Ptr && *Ptr)
	{
		bool NoChildren;
		
		GAutoPtr<GXmlTag> t
		(
		    Parse(  First && !TestFlag(d->Flags, GXT_NO_DOM) ? Root : 0,
					Allocator,
					Ptr,
					NoChildren,
					false)
		);
					
		First = false;
		if (t)
		{
			if (t->Tag &&
				t->Tag[0] == '!' &&
				strcmp(t->Tag, "!DOCTYPE") == 0)
			{
				for (GXmlTag *c=t->Children.First(); c; c=t->Children.Next())
				{
					if (c->Tag &&
						strcmp(c->Tag, "!ENTITY") == 0)
					{
						if (c->Attr.Length() == 2)
						{
							GXmlAttr &Ent = c->Attr[0];
							GXmlAttr &Value = c->Attr[1];
							if (Ent.Name &&
								Value.Name &&
								!d->Entities.Find(Ent.Name))
							{
								GVariant v(Value.Name);
								char16 *w = v.WStr();
								if (w)
									d->Entities.Add(Ent.Name, *w);

							}
						}
					}
				}
			}

			if (t->Tag && t->Tag[0] == '/' && d->Current->Tag)
			{
				// End tag
				if (stricmp(t->Tag + 1, d->Current->Tag) == 0)
				{
					d->Current = d->Current->Parent;
				}
				else
				{
					int Lines = 1;
					for (char *k = Ptr; k >= Str; k--)
					{
						if (*k == '\n') Lines++;
					}

					d->Error.Printf("Mismatched '%s' tag, got '%s' instead (Line %i).\n", t->Tag, d->Current->Tag, Lines);

					#ifdef _DEBUG
					GXmlTree Dbg;
					GFile Out;
					if (Out.Open("c:\\temp\\out.xml", O_WRITE))
					{
						Dbg.Write(Root, &Out);
					}
					#endif
					break;
				}

				t.Reset();
			}
			else
			{
				t->Serialize(t->Write = false);

			    GXmlTag *NewTag = t;
			    if (t != Root)
				    d->Current->InsertTag(t.Release());
			    else
			        t.Release();

				if (!TestFlag(d->Flags, GXT_NO_DOM) &&
					!NoChildren &&
					!d->NoChildTags.Find(NewTag->Tag))
				{
					d->Current = NewTag;
				}
			}
		}
		else
		{
		    break;
		}
	}
	
	d->Factory = 0;
	DeleteArray(Str);
	
	return true;
}

void GXmlTree::Output(GXmlTag *t, int Depth)
{
	#define Tabs if (!TestFlag(d->Flags, GXT_NO_PRETTY_WHITESPACE)) \
		{ for (int i=0; i<Depth; i++) d->File->Write((void*)"\t", 1); }

	if (d->Prog)
		d->Prog->Value(d->Prog->Value()+1);
	t->Serialize(t->Write = true);
	Tabs

	// Test to see if the tag is valid
	bool ValidTag = ValidStr(t->Tag) && !IsDigit(t->Tag[0]);
	if (ValidTag)
		GStreamPrint(d->File, "<%s", t->Tag);

	for (int i=0; i<t->Attr.Length(); i++)
	{
		GXmlAttr &a = t->Attr[i];

		// Write the attribute name
		GStreamPrint(d->File, " %s=\"", a.Name);
		
		// Encode the value
		EncodeEntities(d->File, a.Value, -1, EncodeEntitiesAttr);

		// Write the delimiter
		d->File->Write((void*)"\"", 1);

		if (i<t->Attr.Length()-1 && TestFlag(d->Flags, GXT_PRETTY_WHITESPACE))
		{
			d->File->Write((void*)"\n", 1);
			for (int i=0; i<=Depth; i++) d->File->Write((void*)"\t", 1);
		}
	}
	
	// Write the child tags
	bool HasContent = ValidStr(t->Content);
	GXmlTag *c = t->Children.First();
	if (c || HasContent)
	{
		if (ValidTag)
			d->File->Write(">", 1);
		
		if (HasContent)
		{
			EncodeEntities(d->File, t->Content, -1, EncodeEntitiesContent);
			if (c)
			{
				d->File->Write((char*)"\n", 1);
				Tabs
			}
		}

		if (c)
		{
			if (!HasContent)
			{
				d->File->Write((char*)"\n", 1);
			}

			for (; c; c=t->Children.Next())
			{
				Output(c, Depth + (d->NoDom() ? 0 : 1));
			}

			Tabs
		}
	
		if (!d->NoDom())
			GStreamPrint(d->File, "</%s>\n", t->Tag);
	}
	else if (ValidTag)
	{
		if (d->NoDom())
			d->File->Write(">\n", 2);
		else
			d->File->Write(" />\n", 4);
	}
	
	#undef Tabs
}

bool GXmlTree::Write(GXmlTag *Root, GStreamI *File, Progress *Prog)
{
	bool Status = false;
	
	if (Root && File)
	{
		File->SetSize(0);
		d->File = File;
		if ((d->Prog = Prog))
			d->Prog->SetLimits(0, Root->CountTags());

		if (!TestFlag(d->Flags, GXT_NO_HEADER))
			File->Write(GXmlHeader, strlen(GXmlHeader));
		if (d->StyleFile && d->StyleType)
			GStreamPrint(d->File, "<?xml-stylesheet href=\"%s\" type=\"%s\"?>\n", d->StyleFile, d->StyleType);

		Output(Root, 0);

		d->File = NULL;
		d->Prog = NULL;
		Status = true;
	}
	
	return Status;
}

char *GXmlTree::GetErrorMsg()
{
	return d->Error;
}

LHashTbl<ConstStrKey<char,false>,bool> *GXmlTree::NoChildTags()
{
	return &d->NoChildTags;
}

LHashTbl<ConstStrKey<char,false>,char16> *GXmlTree::GetEntityTable()
{
	return &d->Entities;
}

char *GXmlTree::GetStyleFile(char **StyleType)
{
	if (StyleType)
		*StyleType = d->StyleType;
	return d->StyleFile;
}

void GXmlTree::SetStyleFile(char *file, const char *type)
{
	DeleteArray(d->StyleFile);
	DeleteArray(d->StyleType);

	d->StyleFile = NewStr(file);
	d->StyleType = NewStr(type);
}

