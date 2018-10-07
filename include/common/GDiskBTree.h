#ifndef _GDiskBTree_h_
#define _GDiskBTree_h_

class GDiskBTree
{
	class GDiskBTreePrivate *d;
	
public:
	GDiskBTree(char *File, int DataSize = sizeof(int));
	virtual ~GDiskBTree();

	// File
	bool Open(char *File);
	bool Close();

	// Data
	int64 GetSize();
	bool Insert(char *Key, void *Data);
	bool Delete(char *Key);
	void *HasItem(char *Key);
	bool Empty();

	// Iterator
	void *First(char **Key = 0);
	void *Next(char **Key = 0);
};

#endif