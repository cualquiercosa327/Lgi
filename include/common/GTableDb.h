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
		DbFieldDef    = ID('flds'),
		DbKeys        = ID('keys'),
		DbRow         = ID('data'),
		DbEmpty       = ID('empt'),
	};
	
	enum FieldFlags
	{
		FIELD_SORTED		= 0x01,
		FIELD_AUTONUMBER	= 0x02,
	};
	
	// Riff chunk
	class Chunk
	{
		union
		{
			uint32 IntId;
			char StrId[4];
		};
		uint32 Size;	// If size is zero, the all the remaining data in 
						// the file or memory is part of the chunk.
	};
	
	struct Base
	{
		bool Io(uint8 &i8, GPointer &p, bool write);
		bool Io(uint16 &i16, GPointer &p, bool write);
		bool Io(uint32 &i32, GPointer &p, bool write);
		bool Io(uint64 &i64, GPointer &p, bool write);
		bool Io(GAutoString &str, GPointer &p, bool write);
	};

	class Field : public Base
	{
	public:
		union {
			uint32 Id;
			char IdStr[4];
		};
		char Null;
		uint8 Type;
		uint8 Flags;
		GAutoString Name;
		
		Field();
		Field(int id, int type, const char *name, int flags = 0);
		
		void Set(int id, int type, const char *name, int flags = 0);
		bool Serialize(GPointer &p, bool Write);
		bool operator !=(const Field &b);
		Field &operator=(const Field &b);
	};
	
	typedef GArray<Field> Schema;
	
	/// \returns TRUE if the schemas are the same.
	static bool CompareSchema(Schema &a, Schema &b);
	/// Copies 'src' to 'dest'
	static bool CopySchema(Schema &dest, Schema &src);

	struct RowId
	{
		int32 BlockId;
		int32 Offset;
		
		bool Empty()
		{
			return Offset < 0;
		}
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
		bool ChangeSchema(Schema &NewFields);
		Field *GetField(int FieldId);
		Field *GetFieldAt(int Index);
		
		// Row operations
		
		/// Create a new row for a auto-numbered table
		RowId NewRow(uint32 *Auto);
		/// Create a new row with the specified integer key
		RowId NewRow(uint32 Key);
		/// Create a new row with the specified string key
		RowId NewRow(const char *Key);

		/// Finds the first matching record for the specified key
		RowId SeekKey(uint32 key);
		/// Finds the first matching record for the specified key
		RowId SeekKey(const char *key);
		
		/// Sets an integer value
		bool Set(RowId row, uint32 Fld, uint32 i);
		/// Sets a string value
		bool Set(RowId row, uint32 Fld, const char *str);

		/// Gets an integer value
		uint32 GetInt32(RowId row, uint32 Fld);
		/// Gets a string value
		const char *GetStr(RowId row, uint32 Fld);
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

	/// Get a specific table
	Table *GetTable(const char *Name);

	/// Create a new table, or if it already exists makes sure the fields are correct.
	/// If the table exists and the fields are different, it will be updated to match the
	/// required format.
	Table *CreateTable
	(
		/// The name of the table (this forms the first part of the filename)
		const char *Name,
		/// The fields the table will have. If it already exists the fields will
		/// be made to match this initial schema.
		const Schema &Sch
	);
	
	/// This function flushes all writes to the disk.
	bool Commit();
	
	/// \returns true if the DB is good condition
	bool IsOk();
};

extern bool RunTableDbTest(GStream *Log);

#endif