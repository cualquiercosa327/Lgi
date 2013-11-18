#ifndef _GTABLEDB_H
#define _GTABLEDB_H

#include "GVariant.h"

class GTableDb : public GDom
{
	struct GTableDbPriv *d;

public:
	class Table;

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
	public:
		Table(const char *Name);
		~Table();

		// Table level operations
		bool DeleteTable();

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

	GTableDb(const char *BaseFolder, int BlockSize = 5 << 20 /* 5mb */);
	~GTableDb();
	
	/// An array of available tables
	GArray<Table*> &Tables();
	/// Create a new table
	Table *CreateTable(const char *Name);
	
	/// This function flushes all writes to the disk.
	bool Commit();
};

#endif