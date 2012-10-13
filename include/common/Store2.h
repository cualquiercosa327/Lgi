/*hdr
**      FILE:           Store.h
**      AUTHOR:         Matthew Allen
**      DATE:           29/10/98
**      DESCRIPTION:    Tree storage
**
**      Copyright (C) 1998 Matthew Allen
**              fret@memecode.com
*/

#ifndef __STORE2_H
#define __STORE2_H

#include "Gdc2.h"
#include "GContainers.h"
#include "Progress.h"
#include "StoreCommon.h"

class WriterItem;
class GSubFilePtr;

namespace Storage2
{
	#define STORAGE2_ITEM_MAGIC		0x123400AB
	#define STORAGE2_MAGIC			0x123400FE

	class StorageKitImpl;
	class StorageItemImpl;
	class StorageKitImplPrivate;

	struct StorageItemHeader
	{
		uint32 Magic;			// sanity check, always == STORAGE2_ITEM_MAGIC

		// object descriptor
		uint32 Type;			// application defined object type
		uint32 DataLoc;			// location in file of this objects data segment
		uint32 DataSize;		// size of this objects data segment
		
		// heirarchy descriptor
		uint32 ParentLoc;		// file location of objects parent
		uint32 DirLoc;			// file location of the child directory
		uint32 DirCount;		// number of elements in the child directory array
		uint32 DirAlloc;		// numder of allocated elements in child directory

		// file io
		StorageItemHeader();
		bool Serialize(GFile &f, bool Write);
		void SwapBytes();
	};

	class StorageItemImpl : public StorageItem
	{
		friend class StorageKitImpl;
		friend class StorageKitImplPrivate;
		friend class ::StorageObj;
		
		friend class WriterItem;

	private:
		uint32 StoreLoc;
		bool HeaderDirty;
		StorageItemHeader *Header;		// pointer to our block in the parents Dir array
		StorageItemHeader *Dir;			// an array of children
		StorageItemHeader *Temp;		// we own this block of memory

		StorageItemImpl *Next;
		StorageItemImpl *Prev;
		StorageItemImpl *Parent;
		StorageItemImpl *Child;

		StorageKitImpl *Tree;

		bool IncDirSize();
		bool DirChange();

		bool SetDirty(bool Dirty);
		bool SerializeHeader(GFile &f, bool Write);
		bool SerializeObject(GSubFilePtr &f, bool Write);
		
		/// Saves this object and all it's children.
		void CleanAll();

	public:
		StorageItemImpl(StorageItemHeader *header);
		~StorageItemImpl();

		// Heirarchy
		StorageItem *GetNext();
		StorageItem *GetPrev();
		StorageItem *GetParent();
		StorageItem *GetChild();

		StorageItem *CreateNext(StorageObj *Obj);
		StorageItem *CreateChild(StorageObj *Obj);
		StorageItem *CreateSub(StorageObj *Obj);
		bool InsertSub(StorageItem *Obj, int At = -1);
		bool DeleteChild(StorageItem *Obj);
		bool DeleteAllChildren();

		// Properties
		int GetType();
		void SetType(int i);
		int GetTotalSize();
		int GetObjectSize();
		StorageKit *GetTree();

		// Impl
		GFile *GotoObject(const char *file, int line);
		bool EndOfObj(GFile &f);
		bool Save();
		
		// Debug
		int GetStoreLoc() { return StoreLoc; }
		void Dump(int d)
		{
			char s[256];
			memset(s, ' ', d << 1);
			s[d << 1] = 0;
			LgiTrace("%sItem=%p Obj=%p Type=%x\n",
				s, this, Object, Object?Object->Type():0);
			for (StorageItemImpl *c=Child; c; c=c->Next)
			{
				c->Dump(d + 1);
			}
		}
	};

	struct StorageHeader {

		int Magic;				// always STORAGE2_MAGIC
		int Version;			// 0x0
		int Reserved1;			// 0x0
		int Reserved2;			// 0x0
		char Password[32];		// obsured password
		int Reserved3[4];		// 0x0
	};

	class StorageKitImpl :
		public GBase,
		public StorageKit,
		public GMutex
	{
		friend class StorageItemImpl;
		friend class StorageKitImplPrivate;

	protected:
		StorageKitImplPrivate *d;
	
		// File stuff
		char *FileName;
		bool Status;
		int StoreLoc;
		int Version;
		bool ReadOnly;
		GPassword Password;

		class GSubFile *File;
		class GSubFilePtr *CreateFilePtr(const char *file, int line);

		// Root stuff
		StorageItemHeader RootHeader;
		StorageItemImpl *Root;

		bool _ValidLoc(uint64 Loc);
		bool _Serialize(GFile &f, bool Write);
		
	public:
		StorageKitImpl(char *FileName);
		~StorageKitImpl();

		bool IsOk();
		int GetStatus() { return Status; }
		bool GetReadOnly() { return ReadOnly; }
		int GetVersion() { return Version; }
		uint64 GetFileSize();
		bool GetPassword(GPassword *p);
		bool SetPassword(GPassword *p);
		GMutex *GetLock();
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
