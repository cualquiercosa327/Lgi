#include "Lgi.h"
#include "GToken.h"
#include "GTableDb.h"
#include "GTableDbPriv.h"

/////////////////////////////////////////////////////////////////////////////////////////////
static GAutoString ReadStr(GPointer &p)
{
	char *e = p.s8;
	while (*e)
		e++;
	GAutoString a(NewStr(p.s8, e - p.s8));
	p.s8 = e + 1;
	return a;
}

/////////////////////////////////////////////////////////////////////////////////////////////
#define BaseIntegerSerialize(bits) \
	bool GTableDb::Base::Io(uint##bits &i, GPointer &p, bool write) \
	{ \
		if (write)	*p.u##bits++ = i; \
		else		i = *p.u##bits++; \
		return true; \
	}
BaseIntegerSerialize(8)
BaseIntegerSerialize(16)
BaseIntegerSerialize(32)
BaseIntegerSerialize(64)

bool GTableDb::Base::Io(GAutoString &str, GPointer &p, bool write)
{
	if (write)
	{
		char *c = str;
		while (*c)
		{
			*p.s8++ = *c++;
		}
		*p.s8++ = 0;
	}	
	else
	{
		char *end = p.s8;
		while (*end)
			end++;
		if (!str.Reset(NewStr(p.s8, end - p.s8)))
			return false;
		
		p.s8 = end + 1;
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////
GBlockArray::GBlockArray(uint32 size, uint8 keytype, const char *base, const char *ext)
{
	BlockSize = size;
	KeyType = keytype;
	Sorted = false;
	HasRecordHdrs = true;
	Base.Reset(NewStr(base));
	Ext = ext;
}

GBlockArray::~GBlockArray()
{
	b.DeleteObjects();
}

GBlockArray::Block *GBlockArray::New()
{
	int MaxId = 0, i;
	GArray<bool> Has;
	for (i=0; i<b.Length(); i++)
	{
		MaxId = max(b[i]->Id, MaxId);
		Has[b[i]->Id] = true;
	}
	
	Block *n = NULL;
	for (i=0; i<Has.Length(); i++)
	{
		if (!Has[i])
		{
			n = new Block(i);
			break;
		}
	}
	if (!n)
	{
		n = new Block(MaxId + 1);
	}
	if (n)
	{
		char p[MAX_PATH];
		sprintf_s(p, sizeof(p), "%s.%i.%s", Base.Get(), n->Id, Ext);
		n->File.Reset(NewStr(p));
		n->Data.Reset(new char[BlockSize]);
		
		LgiAssert(!m.Find(n->Id));
		m.Add(n->Id, n);
	}
	
	return n;
}

bool GBlockArray::Add(const char *File, uint32 id)
{
	Block *n = new Block(id);
	if (!n)
	{
		return false;
	}
	
	n->File.Reset(NewStr(File));
	GFile f;
	if (!f.Open(File, O_READ))
	{
		DeleteObj(n);
		return false;
	}
	
	if (!n->Data.Reset(new char[BlockSize]))
	{
		DeleteObj(n);
		return false;
	}
	
	LgiAssert(f.GetSize() <= BlockSize);
	n->Used = f.Read(n->Data, BlockSize);
	LgiAssert(!m.Find(n->Id));
	m.Add(n->Id, n);
	
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////
GTableDb::Field::Field()
{
	Id = 0;
	Null = 0;
	Type = GV_NULL;
}

GTableDb::Field::Field(int id, int type, const char *name, int flags)
{
	Null = 0;
	Id = id;
	Type = type;
	Flags = flags;
	Name.Reset(NewStr(name));
}

void GTableDb::Field::Set(int id, int type, const char *name, int flags)
{
	Null = 0;
	Id = id;
	Type = type;
	Flags = flags;
	Name.Reset(NewStr(name));
}

bool GTableDb::Field::Serialize(GPointer &p, bool Write)
{
	return	Io(Id,    p, Write) &&
			Io(Type,  p, Write) &&
			Io(Flags, p, Write) &&
			Io(Name,  p, Write);
}

bool GTableDb::Field::operator !=(const Field &b)
{
	LgiAssert(Name && b.Name);

	return	Id != b.Id ||
			!Name ||
			!b.Name ||
			stricmp(Name, b.Name) != 0 ||
			Type != b.Type;
}

GTableDb::Field &GTableDb::Field::operator=(const Field &b)
{
	Id = b.Id;
	Type = b.Type;
	Flags = b.Flags;
	Name.Reset(NewStr(b.Name));
	return *this;
}

bool GTableDb::CompareSchema(Schema &a, Schema &b)
{
	if (a.Length() != b.Length())
		return false;
	
	for (int i=0; i<a.Length(); i++)
	{
		if (a[i] != b[i])
			return false;
	}
	
	return true;
}

bool GTableDb::CopySchema(Schema &dest, Schema &src)
{
	dest.Length(0);
	for (int i=0; i<src.Length(); i++)
	{
		dest.New() = src[i];
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////
GTableDbPriv::GTableDbPriv()
{
	Log = NULL;
}

void GTableDbPriv::Msg(char *Fmt, ...)
{
	if (Log)
	{
		char buffer[512];
		va_list arg;
		va_start(arg, Fmt);
		int Ch = vsprintf(buffer, Fmt, arg);
		va_end(arg);

		if (Ch > 0)
			Log->Write(buffer, Ch);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////
GFileBase::GFileBase(GTablePriv *t, const char *fn)
{
	Table = t;
	FileName.Reset(NewStr(fn));
}

GFileBase::~GFileBase()
{
}

bool GFileBase::Delete()
{
	if (!FileDev->Delete(FileName))
		return false;
	
	FileName.Reset();
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////
template<typename T>
static bool DeleteBaseFiles(GArray<T*> &a)
{
	for (int i=0; i<a.Length(); i++)
	{
		if (!a[i]->Delete())
			return false;
	}
	a.DeleteObjects();
	return true;
}

GDataFile::GDataFile(GTablePriv *t, const char *fn) : GFileBase(t, fn)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////
GIndexFile::GIndexFile(GTablePriv *t, const char *fn) : GFileBase(t, fn)
{
	Table = t;
}

GIndexFile::~GIndexFile()
{
	Empty();
}

bool GIndexFile::Empty()
{
	Blocks.DeleteArrays();
	return true;
}

bool GIndexFile::Delete()
{
	bool Status = GFileBase::Delete();
	Status &= Empty();
	return Status;
}

bool GIndexFile::Serialize(bool Write)
{
	/*
	GFile f;
	if (!f.Open(FileName, Write ? O_WRITE : O_READ))
	{
		Table->Db->Msg(	"%s:%i - Failed to open '%s' for %s\n",
						_FL,
						FileName.Get(),
						Write ? "writing" : "reading");
		return false;
	}
	
	int blks;
	if (Write)
	{
		f.SetSize(0);
		blks = Blocks.Length();
	}
	else
	{
		int64 sz = f.GetSize();
		blks = sz / BlockSize();
		Blocks.DeleteArrays();
	}
	
	for (int i=0; i<blks; i++)
	{
		if (Write)
		{
			LgiAssert(Blocks[i]);
		}
		else // Read
		{
			Blocks[i] = new uint8[BlockSize()];
		}
		if (Blocks[i])
		{
			int r = Write ?
					f.Write(Blocks[i], BlockSize()) :
					f.Read(Blocks[i], BlockSize());
			if (r != BlockSize())
			{
				Table->Db->Msg("%s:%i - Error at index block %i, r=%i\n", _FL, i, r);
				return false;
			}
		}
		else
		{
			Table->Db->Msg("%s:%i - No block at %i.\n", _FL, i);
			return false;
		}
	}
	*/
	
	return true;
}

bool GIndexFile::Parse()
{
	/*
	for (int i=0; i<Blocks.Length(); i++)
	{
		GPointer p;
		p.u8 = Blocks[i];
		uint8 *End = p.u8 + Table->IndexBlockSize;		

		while (p.u8 < End)
		{
			switch (*p.u32++)
			{
				case GTableDb::DbIndexHeader:
				{
					uint32 Size = *p.u32++;
					uint8 *Next = p.u8 + Size;
					
					Table->IndexBlockSize = *p.u32++;
					Table->IndexFileSize = *p.u32++;					
					End = Blocks[i] + Table->IndexBlockSize;				
					
					LgiAssert(p.u8 <= Next);
					p.u8 = Next;
					break;
				}
				case GTableDb::DbFieldDef:
				{
					uint32 Size = *p.u32++;
					uint8 *Next = p.u8 + Size;

					GTableDb::Field &f = Table->Fields.New();
					f.Id = *p.u32++;
					f.Type = (GVariantType)*p.u8++;
					f.Name = ReadStr(p);
					
					LgiAssert(p.u8 <= Next);
					p.u8 = Next;
					break;
				}
				case GTableDb::DbEmpty:
				{
					uint32 Size = *p.u32++;
					p.u8 += Size;
					break;
				}
				case 0:
				{
					p.u8 = End;
					break;
				}
				default:
				{
					Table->Db->Msg("%s:%i - Error parsing index: unknown chunk ID '%u'\n", _FL, p.u32[-1]);
					return false;
				}
			}
		}
	}
	*/
	
	return true;
}

///////////////////////////////////////////////////////////////////////
GTablePriv::GTablePriv(GTableDbPriv *db)
{
	Db = db;
}

GTablePriv::~GTablePriv()
{
}

bool GTablePriv::DeleteTable()
{
	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////
GTableDb::Table::Table(GTableDbPriv *db, const char *name)
{
	d = new GTablePriv(db);
	Name(name);
}

GTableDb::Table::~Table()
{
	DeleteObj(d);
}

bool GTableDb::Table::AddFile(const char *Path, int Number, bool IsIndex)
{
	return false;
}

bool GTableDb::Table::DeleteTable()
{
	return d->DeleteTable();
}

bool GTableDb::Table::ChangeSchema(GArray<Field> &NewFields)
{
	return false;
}

GTableDb::Field *GTableDb::Table::GetField(int FieldId)
{
	return false;
}

GTableDb::Field *GTableDb::Table::GetFieldAt(int Index)
{
	return false;
}

GTableDb::RowId GTableDb::Table::NewRow(uint32 *Auto)
{
	RowId r = {0, 0};
	return r;
}

GTableDb::RowId GTableDb::Table::NewRow(uint32 Key)
{
	RowId r = {0, 0};
	return r;
}

GTableDb::RowId GTableDb::Table::NewRow(const char *Key)
{
	RowId r = {0, 0};
	return r;
}

GTableDb::RowId GTableDb::Table::SeekKey(uint32 key)
{
	RowId r = {0, 0};
	return r;
}

GTableDb::RowId GTableDb::Table::SeekKey(const char *key)
{
	RowId r = {0, 0};
	return r;
}

bool GTableDb::Table::Set(RowId row, uint32 Fld, uint32 i)
{
	return false;
}

bool GTableDb::Table::Set(RowId row, uint32 Fld, const char *str)
{
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////
GTableDb::GTableDb(const char *BaseFolder)
{
	d = new GTableDbPriv;
	if (BaseFolder)
		Open(BaseFolder);
}

GTableDb::~GTableDb()
{
	DeleteObj(d);
}

bool GTableDb::Open(const char *BaseFolder)
{
	Empty();
	
	GDirectory Dir;
	GHashTbl<char*, Table*> Tables;
	
	char p[MAX_PATH];
	for (bool b = Dir.First(BaseFolder); b; b = Dir.Next())
	{
		if (!Dir.IsDir())
		{
			GToken t(Dir.GetName(), ".");
			if (t.Length() == 3)
			{
				Table *tbl = Tables.Find(t[0]);
				if (!tbl)
					tbl = new Table(d, t[0]);
				if (tbl && Dir.Path(p, sizeof(p)))
					tbl->AddFile(p, atoi(t[1]), !stricmp(t[2], "index"));
			}
		}
	}
	
	return true;
}

void GTableDb::Empty()
{
	d->Tables.DeleteObjects();
}

void GTableDb::SetLogStream(GStream *log)
{
	d->Log = log;
}

GArray<GTableDb::Table*> &GTableDb::Tables()
{
	return d->Tables;
}

GTableDb::Table *GTableDb::GetTable(const char *Name)
{
	if (!Name)
		return NULL;

	for (int i=0; i<d->Tables.Length(); i++)
	{
		if (!stricmp(Name, d->Tables[i]->Name()))
			return d->Tables[i];
	}
	
	return NULL;
}

GTableDb::Table *GTableDb::CreateTable(const char *Name, const Schema &Sch)
{
	return 0;
}

bool GTableDb::Commit()
{
	return false;
}

bool GTableDb::IsOk()
{
	return true;
}

///////////////////////////////////////////////////////////////////////////////
bool RunTableDbTest(GStream *Log)
{
	{	// Field serialization test
		GArray<char> Mem;
		Mem.Length(4 << 10);
		char Flds[] = {"key \1\0Key\0name\5\0Name\0data\5\0Data\0" };
		GTableDb::Schema sc;
		GPointer p;
		p.s8 = Flds;
		if (!sc.New().Serialize(p, false)) return false;
		if (!sc.New().Serialize(p, false)) return false;
		if (!sc.New().Serialize(p, false)) return false;
		LgiAssert(p.s8 - Flds == sizeof(Flds) - 1);
		
		p.s8 = &Mem[0];
		for (int i=0; i<sc.Length(); i++)
			if (!sc[i].Serialize(p, true))
				return false;
		LgiAssert(p.s8 - &Mem[0] == sizeof(Flds) - 1);
		if (memcmp(&Mem[0], Flds, sizeof(Flds) - 1))
			return false;
			
		GTableDb::Schema sc2;
		if (!GTableDb::CopySchema(sc2, sc))
			return false;
		if (!GTableDb::CompareSchema(sc, sc2))
			return false;
	}


	return true;
}
