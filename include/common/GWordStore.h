
#ifndef _GWORDSTORE_H_
#define _GWORDSTORE_H_

class GWordStore
{
	class GWordStorePriv *d;

public:
	GWordStore(char *file = 0);
	~GWordStore();
	
	// Serialization
	bool Serialize(char *file, bool Load);
	
	// Container
	int GetItems();
	void SetItems(int s);
	bool Insert(char *Word);
	int GetWordCount(char *Word);
	int SetWordCount(char *Word, int Count);
	void Empty();
	char *GetFile();
	void SetFile(char *file);

	// Iterate
	char *First();
	char *Next();
	int Length();

	#ifdef _DEBUG
	int64 Sizeof();
	#endif
};

#endif
