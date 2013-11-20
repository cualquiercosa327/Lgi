#ifndef _GTABLEDB_H
#define _GTABLEDB_H

#include "GVariant.h"

#define ID(c)		c

class GTableDb : public GDom
{
	struct GTableDbPriv *d;

public:
	class Table;

	enum HeaderTypes
	{
		DbIndexHeader = ID('indx'),
		DbFieldDef = ID('flds'),
		DbKeys = ID('keys'),
		DbRow = ID('data'),
		DbEmpty = ID('empt'),
	};
	
	class Block
	{
		union
		{
			uint32 IntId;
			char StrId[4];
		};
		uint32 Size;
	};
	
	class FieldDef
	{
		Block Hdr;		// DbFieldDef
		uint32 Id;		// Identifier of data
		uint8 Type;		// GVariantType
		char Name[1];	// String name
	};
	
	class Key
	{
		uint32 Offset;
		uint16 BlockId;
		char Data[1];
	};

	class Field
	{
	public:
		int Id;
		GAutoString Name;
		GVariantType Type;
	};

	class Row
	{
		Table *t;
		
	public:
		Row(Table *tbl, uint64 id);
		~Row();
		
		int32 GetInt(int FieldId);
		int64 GetInt64(int FieldId);
		const char *GetStr(int FieldId);

		bool SetInt(int FieldId, int32 value);
		bool SetInt64(int FieldId, int64 value);
		bool GetStr(int FieldId, const char *value);
	};

	class Table : public GBase, public GDom
	{
		struct GTablePriv *d;
		
	public:
		Table(GTableDbPriv *db, const char *Name);
		~Table();

		// Table level operations
		bool DeleteTable();
		bool AddFile(const char *Path, int Number, bool IsIndex);

		// Field operations
		bool AddField(char *Name, int FieldId, GVariantType Type);
		bool DeleteField(int FieldId);
		Field *GetField(int FieldId);
		Field *GetFieldAt(int Index);
		
		// Row operations
		Row *NewRow(GVariant *Key);
		bool DeleteRow(int Index);
		Row *operator[](int index);
	};

	GTableDb(const char *BaseFolder);
	~GTableDb();
	
	/// Open a base folder
	bool Open(const char *BaseFolder);
	
	/// Clear out loaded tables
	void Empty();
	
	/// Set a log file
	void SetLogStream(GStream *log);
	
	/// An array of available tables
	GArray<Table*> &Tables();
	/// Create a new table
	Table *CreateTable(const char *Name);
	
	/// This function flushes all writes to the disk.
	bool Commit();
	
	/// \returns true if the DB is good condition
	bool IsOk();
};

#endif