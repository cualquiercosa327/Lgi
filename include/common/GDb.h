/*hdr
**      FILE:           GDb.h
**      AUTHOR:         Matthew Allen
**      DATE:           8/2/2000
**      DESCRIPTION:    Db API definitions
**
**      Copyright (C) 2000 Matthew Allen
**						fret@memecode.com
*/

#ifndef __GDB_H
#define __GDB_H

#include "GVariant.h"

#define Dom_IsConnected		"IsConnected"

// Classes
class GDbField
{
public:
	virtual ~GDbField() {};

	// Cast
	virtual operator char*() { LgiAssert(0); return 0; }
	virtual GDbField &operator =(char*) { LgiAssert(0); return *this; }

	// Properties
	virtual char *Name() { return 0; }
	virtual bool Name(char *Str) { return 0; }
	virtual int Type() { return 0; }
	virtual bool Type(int NewType) { return 0; }
	virtual int Length() { return 0; }
	virtual bool Length(int NewLength) { return 0; }
	virtual char *Description() { return 0; }
	virtual bool Description(char *NewDesc) { return 0; }

	// Set/Get
	virtual bool Set(GVariant &v) { return 0; }
	virtual bool Get(GVariant &v) { return 0; }
};

class GDbRecordset
{
public:
	virtual ~GDbRecordset() {};

	// Props
	virtual char *Name() = 0;

	// Fields
	virtual GDbField &operator [](unsigned Index) = 0;
	virtual GDbField &operator [](const char *Name) = 0;
	virtual GDbField *InsertField(	const char *Name,
									int Type,
									int Length = 0,
									int Index = -1) = 0; // default: append to the end
	virtual bool DeleteField(GDbField *Fld) = 0;
	virtual int Fields() = 0; // return number of fields

	// Markers
	virtual bool End() = 0;

	// New/Edit
	virtual bool AddNew() = 0;
	virtual bool Edit() = 0;
	virtual bool Update() = 0;
	virtual void Cancel() = 0;

	// Moving around the records
	virtual bool MoveFirst() = 0;
	virtual bool MoveNext() = 0;
	virtual bool MovePrev() = 0;
	virtual bool MoveLast() = 0;
	virtual int SeekRecord(int i) = 0; // from the beginning

	// Numbers/Counts
	virtual int RecordIndex() = 0;
	virtual int Records() = 0;
	
	// Deleting
	virtual bool DeleteRecord() = 0; // delete's the current record
};

class GDb : public GDom
{
public:
	virtual ~GDb() {}

	// Attaching to data
	virtual bool Connect(const char *Init) = 0;
	virtual bool Disconnect() = 0;

	// Retrieving data
	virtual GDbRecordset *Open(char *Name) = 0;
	virtual bool Execute(char *Name) { return false; }
	virtual GDbRecordset *TableAt(int i) { return NULL; }
	virtual const char *GetErrorMsg() { return 0; }
};

// Actual implementations
extern GDb *OpenDbfDatabase(char *s);		// add 'GDb-Dbf.cpp' to your project
extern GDb *OpenAdoDatabase(char *s);		// add 'GDb-Ado.cpp' to your project
extern GDb *OpenCsvDatabase(char *s, bool HasHeader = true);		// add 'GDb-Csv.cpp' to your project
extern GDb *OpenTsvDatabase(char *s, bool HasHeader = true);		// add 'GDb-Csv.cpp' to your project
extern GDb *OpenMetakitDatabase(char *s);	// add 'GDb-Metakit.cpp' to your project
extern GDb *OpenBfsDatabase(char *s);		// add 'GDb-Bfs.cpp' and 'Bfs.cpp' to your project
extern GDb *OpenLibmysqlDatabase(char *s);	// add 'GDb-Libmysql.cpp'
											// pass 'mysql://username:password@host/database'
											// (will cause your app to become GPL)
extern GDb *OpenMysqlDirectDatabase(char *s); // add 'GDb-Mysql-direct.cpp'
											// pass 'mysql://username:password@host/database'
											// (will _NOT_ cause your app to become GPL)

// Util
extern char *LgiTsvTok(char *&s);


#endif
