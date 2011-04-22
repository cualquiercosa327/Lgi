#ifndef __STORE_COMMON_H
#define __STORE_COMMON_H

#include "GPassword.h"

class StorageItem;
class StorageObj;
class StorageKit;

enum NodeRelation
{
	NodeChild,
	NodeNext,
	NodePrev,
};

#define GSUBFILE_NOBUFFERING		0

class GSubFilePtr;
class GSubFile : public GFile
{
	GSemaphore *Lck;
	GArray<GSubFilePtr*> Ptrs;
	
	#if GSUBFILE_NOBUFFERING
	bool Buffer;
	uint8 *Buf;
	int Block;
	int Shift;
	int64 Cur;
	int64 Pos;
	bool Dirty;

	bool SetBlock(int64 b);
	#endif
	
public:
	typedef GAutoPtr<GSemaphore::Auto> SubLock;

	GSubFile(GSemaphore *lock, bool Buffering = true);
	~GSubFile();

	GSubFilePtr *Create(const char *file, int line);
	void Detach(GSubFilePtr *Ptr);
	SubLock Lock(char *file, int line);

	#if GSUBFILE_NOBUFFERING
	int Open(char *Str = 0, int Int = 0);
	int Read(void *Buffer, int Size, int Flags = 0);
	int Write(const void *Buffer, int Size, int Flags = 0);
	int64 GetPos();
	int64 SetPos(int64 pos);
	#endif
};

class GSubFilePtr : public GFile
{
	friend class GSubFile;
	GSubFile *File;

	// Sub file bound box
	bool Sub; // true if we have a sub file area to serve to callers

	int64 Start;
	int64 Len;
	int64 Pos; // Offset from 'Start' into file.
	
	// Allocation point
	char *SrcFile;
	int SrcLine;
	
	// Our state
	bool OurStatus;
	
	// Actual state
	int64 ActualPos;
	bool ActualStatus;
	bool ActualSwap;
	
	bool SaveState();
	bool RestoreState();
	
public:
	GSubFilePtr(GSubFile *Parent, const char *file, int line);
	~GSubFilePtr();

	/// Sets the valid section of the file.
	bool SetSub
	(
		/// The start of the valid region as an offset from the start of the file.
		int64 start,
		/// The length of the valid region.
		int64 len
	);

	/// Gets the valid sub section of the file.
	bool GetSub(int64 &start, int64 &len);

	/// Removes the sub region so you can access the entire file.
	void ClearSub();
	
	// GFile stuff
	int Open(char *Str = 0, int Int = 0);
	bool IsOpen();
	int Close();
	int64 GetSize();
	int64 SetSize(int64 Size);
	int64 GetPos();
	int64 SetPos(int64 pos);
	int64 Seek(int64 To, int Whence);
	GStreamI *Clone();
	bool Eof();
	bool GetStatus();
	void SetStatus(bool s);
	int Read(void *Buffer, int Size, int Flags = 0);
	int Write(const void *Buffer, int Size, int Flags = 0);
	int ReadStr(char *Buf, int Size);
	int WriteStr(char *Buf, int Size);
};

class StorageItem
{
public:
	StorageObj *Object;

	virtual ~StorageItem() {}

	// Properties
	virtual int GetType() = 0;
	virtual void SetType(int i) = 0;
	virtual int GetTotalSize() = 0;
	virtual int GetObjectSize() = 0;
	virtual StorageKit *GetTree() = 0;

	// Heirarchy
	virtual StorageItem *GetNext() = 0;
	virtual StorageItem *GetPrev() = 0;
	virtual StorageItem *GetParent() = 0;
	virtual StorageItem *GetChild() = 0;
	virtual StorageItem *CreateNext(StorageObj *Obj) = 0;
	virtual StorageItem *CreateChild(StorageObj *Obj) = 0;
	virtual StorageItem *CreateSub(StorageObj *Obj) = 0;
	virtual bool DeleteChild(StorageItem *Obj) = 0;
	virtual bool DeleteAllChildren() = 0;

	// Impl
	virtual bool Save() = 0;
	virtual GFile *GotoObject(const char *file, int line) = 0;
	virtual bool EndOfObj(GFile &f) = 0;
};

class StorageObj
{
	friend class StorageKit;

	bool StoreDirty;

public:
	StorageItem *Store;

	StorageObj()
	{
		StoreDirty = false;
		Store = 0;
	}

	virtual ~StorageObj()
	{
		if (Store)
		{
			Store->Object = 0;
			DeleteObj(Store);
		}
	}
	
	bool GetDirty() { return StoreDirty; }

	virtual int Type() = 0;
	virtual int Sizeof() = 0;
	virtual bool Serialize(GFile &f, bool Write) = 0;
	virtual bool SetDirty(bool d = true) { StoreDirty = d; return true; }
};

class StorageValidator
{
public:
	virtual bool CompactValidate(GView *Parent, StorageItem *Item) = 0;
	virtual void CompactDone(StorageItem *Item) = 0;
};

class StorageKit
{
protected:
	void StorageObj_SetDirty(StorageObj *Obj, bool d)
	{
		Obj->StoreDirty = d;
	}

public:
	virtual ~StorageKit() {}

	// Properties
	virtual int GetStatus() = 0;
	virtual bool GetReadOnly() = 0;
	virtual int GetVersion() = 0;
	virtual uint64 GetFileSize() = 0;
	virtual bool GetPassword(GPassword *p) = 0;
	virtual bool SetPassword(GPassword *p) = 0;
	virtual GSemaphore *GetLock() = 0;
	virtual char *GetFileName() = 0;
	
	// Heirarchy
	virtual StorageItem *GetRoot() = 0;
	virtual StorageItem *CreateRoot(StorageObj *Obj) = 0;
	virtual bool DeleteItem(StorageItem *Item) = 0;
	virtual bool SeparateItem(StorageItem *Item) = 0;
	virtual bool AttachItem(StorageItem *Item, StorageItem *To, NodeRelation Relationship = NodeChild) = 0;

	// Methods
	virtual bool Compact(Progress *p, bool Interactive, StorageValidator *Validator = 0) = 0;
};

#endif
