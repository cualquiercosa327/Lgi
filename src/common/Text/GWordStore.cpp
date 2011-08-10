#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "GWordStore.h"
// #define DBG_MSGS

class GWordStorePriv
{
public:
	char *File;
	GHashTable Table;
	bool Dirty;
	int Items;

	GWordStorePriv()
	{
		Dirty = false;
		Table.IsCase(false);
		Items = 0;
		Table.SetStringPool(true);
	}

	~GWordStorePriv()
	{
		DeleteArray(File);
	}
};

GWordStore::GWordStore(char *file)
{
	d = new GWordStorePriv;
	d->File = NewStr(file);
	if (d->File)
	{
		Serialize(d->File, true);
	}
}

GWordStore::~GWordStore()
{
	if (d->Dirty)
	{
		Serialize(d->File, false);
	}
	DeleteObj(d);
}

char *GWordStore::GetFile()
{
	return d->File;
}

bool GWordStore::Serialize(char *FileName, bool Load)
{
	static const char *ObjName = "GWordStore";
	bool Status = false;
	GFile f;
	
	char *Name = FileName ? FileName : d->File;
	if (Load)
	{
		if (f.Open(Name, O_READ))
		{
			if (!d->File)
			{
				d->File = NewStr(Name);
			}

			bool GotVer = false;
			double FileFormat = 0.0;
			int64 Start = LgiCurrentTime();
			
			GStringPipe p;
			char Buf[4<<10];
			int Words = 0;
			while (!f.Eof())
			{
				int r = f.Read(Buf, sizeof(Buf));
				if (r > 0)
				{
					p.Push(Buf, r);

					while (p.Pop(Buf, sizeof(Buf)))
					{
						if (GotVer)
						{
							char *Comma = strchr(Buf, ',');
							if (Comma)
							{
								*Comma++ = 0;
								d->Table.Add(Buf, (void*) atoi(Comma));
							}
							else
							{
								LgiAssert(0);
							}
						}
						else
						{
							char *Obj;
							if (Obj = stristr(Buf, (char*)ObjName))
							{
								char *v = strchr(Buf + 10, 'v');
								if (v)
								{
									FileFormat = atof(v + 1);
									GotVer = true;
								}
								
								char *i = stristr(Buf, "Items=");
								if (i)
								{
									d->Items = atoi(i+6);
								}

								i = stristr(Buf, "Words=");
								if (i)
								{
									Words = atoi(i+6);
									d->Table.SetSize(Words * 5 / 2);
								}
							}
						}
					}
				}
				else break;
			}

			#if DBG_MSGS
			LgiTrace("Load '%s' time: %ims\n", Name, LgiCurrentTime() - Start);
			#endif
			
			Status = GotVer;
		}
	}	
	else
	{
		if (f.Open(Name, O_WRITE))
		{
			f.SetSize(0);
			
			f.Print("%s v1.00 Items=%i, Words=%i\n", ObjName, d->Items, d->Table.Length());
			
			int i = 0;
			char *Word;
			for (void *Count = d->Table.First(&Word); Count; Count = d->Table.Next(&Word))
			{
				i++;
				f.Print("%s,%i\n", Word, (int)Count);
			}

			#if DBG_MSGS
			LgiTrace("Write to '%s': %i words in %i lines\n", Name, Table.Length(), i);
			#endif
			
			Status = true;
		}
	}
	
	if (Status)
	{
		d->Dirty = false;
	}
	
	return Status;
}

int GWordStore::GetItems()
{
	return d->Items;
}

bool GWordStore::SetItems(int i)
{
	if (i != d->Items)
	{
		d->Items = i;
		d->Dirty = true;
	}
	return true;
}

bool GWordStore::Insert(char *Word)
{
	bool Status = false;
	
	if (Word)
	{
		int c = (int)d->Table.Find(Word);
		Status = d->Table.Add(Word, (void*) (c + 1));
		d->Dirty = true;
	}
	
	return Status;

}

int GWordStore::GetWordCount(char *Word)
{
	return Word ? (int)d->Table.Find(Word) : 0;
}

void GWordStore::Empty()
{
	d->Table.Empty();
	d->Dirty = true;
}

char *GWordStore::First()
{
	char *key;
	return d->Table.First(&key) ? key : 0;
}

char *GWordStore::Next()
{
	char *key;
	return d->Table.Next(&key) ? key : 0;
}

int GWordStore::Length()
{
	return d->Table.Length();
}

#ifdef _DEBUG
int64 GWordStore::Sizeof()
{
	return sizeof(*this) + d->Table.Sizeof();
}
#endif