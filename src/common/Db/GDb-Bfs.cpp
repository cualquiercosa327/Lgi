#include "Lgi.h"
#include "GDb.h"
#include "Bfs/LgiBfs.h"

class GBfsDb : public GDb
{
	Bfs *Db;

public:
	GBfsDb(char *s)
	{
		Db = BfsCreate(s);
		if (Db)
		{
			int File = BfsOpenFile(Db, "/test.txt", S_IREAD);
			if (File)
			{
				int64 Size = BfsGetFileSize(Db, File);

				char Msg[] = "This is the test data string in the file.\n";
				BfsWriteFile(Db, File, Msg, strlen(Msg));

				Size = BfsGetFileSize(Db, File);
			}
		}
	}

	~GBfsDb()
	{
		DeleteObj(Db);
	}

	bool Connect(char *Init)
	{
		return false;
	}

	bool Disconnect()
	{
		return false;
	}

	GDbRecordset *Open(char *Name)
	{
		return false;
	}

	GDbRecordset *TableAt(int i)
	{
		return false;
	}
};

GDb *OpenBfsDatabase(char *s)
{
	return NEW(GBfsDb(s));
}

