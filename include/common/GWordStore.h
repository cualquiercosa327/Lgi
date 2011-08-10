
#ifndef _GWORDSTORE_H_
#define _GWORDSTORE_H_

class GWordStore
{
	class GWordStorePriv *d;

public:
	GWordStore(const char *file = 0);
	~GWordStore();
	
	// Serialization
	bool Serialize(const char *file, bool Load);
	
	// Container
	int GetItems();
	bool SetItems(int s);
	bool Insert(const char *Word);
	int GetWordCount(const char *Word);
	int SetWordCount(const char *Word, int Count);
	void Empty();
	char *GetFile();
	void SetFile(const char *file);

	// Iterate
	char *First();
	char *Next();
	int Length();

	#ifdef _DEBUG
	int64 Sizeof();
	#endif
};

#endif
