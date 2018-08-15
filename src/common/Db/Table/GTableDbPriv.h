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

#ifndef _GTABLE_DB_PRIV_H_
#define _GTABLE_DB_PRIV_H_

struct GIndexFile;
struct GDataFile;
struct GTablePriv;
struct GTableDbPriv;

struct GBlockArray
{
	struct Block
	{
		GAutoString File;
		GAutoPtr<char> Data;
		uint32 Id;
		uint32 Used;
		
		Block(int id)
		{
			Id = id;
			Used = 0;
		}
	};

	GArray<Block*> b;
	GHashTbl<int, Block*> m;

	uint32 BlockSize;
	uint8 KeyType;
	uint8 HasRecordHdrs : 1;
	uint8 Sorted : 1;
	
	GAutoString Base;
	const char *Ext;
	
	GBlockArray(uint32 size, uint8 keytype, const char *base, const char *ext);
	~GBlockArray();
	
	Block *New();
	bool Add(const char *File, uint32 id);
};

struct GTableDbPriv
{
	GAutoString BasePath;
	GArray<GTableDb::Table*> Tables;
	GStream *Log;
	
	GTableDbPriv();	
	void Msg(char *Fmt, ...);
};

struct GTablePriv
{
	GTableDbPriv *Db;
	GArray<GTableDb::Field> Fields;
	GAutoPtr<GBlockArray> Data;
	
	GTablePriv(GTableDbPriv *db);
	~GTablePriv();
	
	bool DeleteTable();
};

struct GFileBase
{
	GTablePriv *Table;
	GAutoString FileName;
	
	GFileBase(GTablePriv *t, const char *fn);	
	virtual ~GFileBase();
	
	virtual bool Delete();
};

struct GDataFile : public GFileBase
{
	GDataFile(GTablePriv *t, const char *fn);
};

struct GIndexFile : public GFileBase
{
	GTablePriv *Table;
	GArray<uint8*> Blocks;
	
	GIndexFile(GTablePriv *t, const char *fn);
	~GIndexFile();	
	bool Empty();	
	bool Delete();	
	bool Serialize(bool Write);
	bool Parse();
};

#endif