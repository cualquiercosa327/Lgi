/*

Format of files:
	
	Tables are broken into a index files and data files.	
	
	Index files: All data is stored in blocks (typically 8kb).
		File name is "<table_name>.<number>.index"
		Each block of data is an array of these RIFF chunks:
			- DbIndexHeader
			- DbFieldDef
			- DbKeys
		Data at the end of the block is all zero.
		All strings are NULL terminated.
		There may be multiple index files for large indexes.
	
		DbIndexHeader format:
			Riff chunk header
			uint32 BlockSize
			uint32 IndexFileSize (Max)
			
		DbFieldDef format:
			Riff chunk header
			uint32 FieldId
			uint8 FieldType
			char *FieldName
		
		DbKeys format:
			Riff chunk header
			Array of keys
		
		Key format:
			uint32 Offset // into the data file
			uint16 DataFileIndex // Index of said data file
			Data // in the format of the key type, usually uint32 or string
		
	Data files:
		The file name is "<table_name>.<number>.data"
		An array of RIFF chunks with no block boundaries.
		Maximum size of file should be kept to 5MB or 10MB so that compacting takes 
		less than a second to write the entire block.
		Available chunks:
		
		DbRow:
			Riff chunk header // DbRow
			An array of data values in the same order as the field defs.
		
		DbEmpty:
			Usually a deleted row, now empty space.

*/
#include "Lgi.h"
#include "GToken.h"
#include "GTableDb.h"


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
struct GTableDbPriv
{
	GAutoString BasePath;
	
	uint32 DefaultIndexBlockSize;
	uint32 DefaultIndexFileSize;
	uint32 DefaultDataFileSize;
	GArray<GTableDb::Table*> Tables;
	GStream *Log;
	
	GTableDbPriv()
	{
		Log = NULL;
		DefaultIndexBlockSize = 8 << 10; // 8 kb
		DefaultIndexFileSize = 1 << 20;
		DefaultDataFileSize = 10 << 20;
	}
	
	void Msg(char *Fmt, ...)
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
};

/////////////////////////////////////////////////////////////////////////////////////////////
GTableDb::Row::Row(GTableDb::Table *tbl, uint64 id)
{
}

GTableDb::Row::~Row()
{
}

int32 GTableDb::Row::GetInt(int FieldId)
{
	return 0;
}

int64 GTableDb::Row::GetInt64(int FieldId)
{
	return 0;
}

const char *GTableDb::Row::GetStr(int FieldId)
{
	return 0;
}

bool GTableDb::Row::SetInt(int FieldId, int32 value)
{
	return 0;
}

bool GTableDb::Row::SetInt64(int FieldId, int64 value)
{
	return 0;
}

bool GTableDb::Row::GetStr(int FieldId, const char *value)
{
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////
struct GIndexFile;
struct GDataFile;

struct GTablePriv
{
	GTableDbPriv *Db;
	GArray<GIndexFile*> Indexes;
	GArray<GDataFile*> Data;
	GArray<GTableDb::Field> Fields;
	uint32 IndexBlockSize;
	uint32 IndexFileSize;
	
	GTablePriv(GTableDbPriv *db);
	~GTablePriv();
	
	bool DeleteTable();
};

struct GFileBase
{
	GTablePriv *Table;
	GAutoString FileName;
	
	GFileBase(GTablePriv *t, const char *fn)
	{
		Table = t;
		FileName.Reset(NewStr(fn));
	}
	
	virtual ~GFileBase()
	{
	}
	
	virtual bool Delete()
	{
		if (!FileDev->Delete(FileName))
			return false;
		
		FileName.Reset();
		return true;
	}
};

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

struct GDataFile : public GFileBase
{
	GDataFile(GTablePriv *t, const char *fn) : GFileBase(t, fn)
	{
	}
};

struct GIndexFile : public GFileBase
{
	GTablePriv *Table;
	GArray<uint8*> Blocks;
	
	GIndexFile(GTablePriv *t, const char *fn) : GFileBase(t, fn)
	{
		Table = t;
	}
	
	~GIndexFile()
	{
		Empty();
	}
	
	bool Empty()
	{
		Blocks.DeleteArrays();
		return true;
	}
	
	bool Delete()
	{
		bool Status = GFileBase::Delete();
		Status &= Empty();
		return Status;
	}
	
	uint8 BlockSize()
	{
		return Table->Db->DefaultIndexBlockSize;
	}
	
	bool Serialize(bool Write)
	{
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
		
		return true;
	}
	
	bool Parse()
	{		
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
		
		return true;
	}
};

GTablePriv::GTablePriv(GTableDbPriv *db)
{
	Db = db;
	IndexBlockSize = Db->DefaultIndexBlockSize;
	IndexFileSize = Db->DefaultIndexFileSize;
}

GTablePriv::~GTablePriv()
{
	Indexes.DeleteObjects();
	Data.DeleteObjects();
}

bool GTablePriv::DeleteTable()
{
	return DeleteBaseFiles(Indexes) && DeleteBaseFiles(Data);
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
	if (IsIndex)
	{
		if (d->Indexes[Number])
			delete d->Indexes[Number];
		if (d->Indexes[Number] = new GIndexFile(d, Path))
			return d->Indexes[Number]->Serialize(false);
	}
	else
	{
		if (d->Data[Number])
			delete d->Data[Number];
		d->Data[Number] = new GDataFile(d, Path);
		return d->Data[Number] != NULL;
	}
	
	return false;
}

bool GTableDb::Table::DeleteTable()
{
	return d->DeleteTable();
}

bool GTableDb::Table::AddField(char *Name, int FieldId, GVariantType Type)
{
	return false;
}

bool GTableDb::Table::DeleteField(int FieldId)
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

GTableDb::Row *GTableDb::Table::NewRow(GVariant *Key)
{
	return false;
}

bool GTableDb::Table::DeleteRow(int Index)
{
	return false;
}

GTableDb::Row *GTableDb::Table::operator[](int index)
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

GTableDb::Table *GTableDb::CreateTable(const char *Name)
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