/*hdr
**      FILE:           Store.h
**      AUTHOR:         Matthew Allen
**      DATE:           29/10/98
**      DESCRIPTION:    Tree storage
**
**      Copyright (C) 1998 Matthew Allen
**              fret@memecode.com
*/

#ifndef __STORE_H
#define __STORE_H

#include "Gdc2.h"
#include "GContainers.h"
#include "Progress.h"
#include "StoreCommon.h"

namespace Storage1
{
	#define STORAGE_ITEM_MAGIC		0x123400AA
	#define STORAGE_ITEM_NOSIZE		0x0001
	#define STORAGE_MAGIC			0x123400FF

	class StorageKitImpl;
	class StorageItemImpl;

	struct StorageItemHeader {

		int Magic;
		int Type;
		int ObjSize;
		int AllocatedSize;
		
		int Next;
		int Prev;
		int Parent;
		int Child;

	};

	class StorageItemImpl : public GBase, public StorageItem
	{
		friend class StorageKitImpl;
		friend class StoreRef;

	private:
		int Sizeof();
		bool SetSize(int NewSize);
		bool MoveToLoc(int NewLoc);
		bool WriteHeader(GFile &f);

		StorageKitImpl *Tree;

	public:
		bool Serialize(GFile &f, bool Write, int Flags = 0);

		int StoreType;
		int StoreSize;
		int StoreAllocatedSize;
		int StoreLoc;
		int StoreNext;
		int StorePrev;
		int StoreParent;
		int StoreChild;

		StorageItemImpl *Next;
		StorageItemImpl *Prev;
		StorageItemImpl *Parent;
		StorageItemImpl *Child;

		StorageItemImpl();
		~StorageItemImpl();

		// Heirarchy
		StorageItem *GetNext();
		StorageItem *GetPrev();
		StorageItem *GetParent();
		StorageItem *GetChild();

		StorageItem *CreateNext(StorageObj *Obj);
		StorageItem *CreateChild(StorageObj *Obj);
		StorageItem *CreateSub(StorageObj *Obj);
		bool DeleteChild(StorageItem *Obj);
		bool DeleteAllChildren();

		// Properties
		int GetType();
		void SetType(int i);
		int GetTotalSize();
		int GetObjectSize();
		StorageKit *GetTree();

		// Impl
		bool Save();
		GFile *GotoObject(const char *file, int line);
		bool EndOfObj(GFile &f);
	};

	class Block {
	public:
		StorageItemImpl *Item;
		int Size;
	};

	struct StorageHeader {

		int Magic;
		int Version;
		int Reserved2;
		int FirstLoc;
		int Reserved3[12];
	};

	class StorageKitImpl : public GBase, public StorageKit
	{
		friend class StorageItemImpl;

	private:
		GMutex Lock;
		char *FileName;
		GSubFile File;
		bool Status;
		int StoreLoc;
		int Version;
		StorageItemImpl *Root;
		bool ReadOnly;

		uint UserData;
		StorageValidator *Validator;

		void AddItem(	StorageItemImpl *Item,
						List<Block> &Blocks);
		StorageItemImpl *CreateItem(StorageObj *Obj);
		StorageItemImpl *LoadLocation(int Loc);
		bool Serialize(GFile &f, bool Write);
		bool SerializeItem(StorageItem *Item, bool Write, int Flags = 0)
		{
			return (Item) ? ((StorageItemImpl*)Item)->Serialize(File, Write, Flags) : 0;
		}

	public:
		StorageKitImpl(char *FileName);
		~StorageKitImpl();

		int GetStatus() { return Status; }
		bool GetReadOnly() { return ReadOnly; }
		int GetVersion() { return Version; }
		void SetVersion(int i);
		uint64 GetFileSize();
		bool GetPassword(GPassword *p) { return false; }
		bool SetPassword(GPassword *p) { return false; }
		GMutex *GetLock() { return &Lock; }
		char *GetFileName() { return FileName; }
		
		StorageItem *GetRoot();
		StorageItem *CreateRoot(StorageObj *Obj);

		bool Compact(Progress *p, bool Interactive, StorageValidator *Validator = 0);
		bool DeleteItem(StorageItem *Item);
		bool SeparateItem(StorageItem *Item);
		bool AttachItem(StorageItem *Item, StorageItem *To, NodeRelation Relationship = NodeChild);
	};
}

#endif
