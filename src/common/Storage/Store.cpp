/*hdr
**      FILE:           Store.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           29/10/98
**      DESCRIPTION:    Tree storage
**
**      Copyright (C) 1998 Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include "Lgi.h"
#include "Store.h"

using namespace Storage1;

///////////////////////////////////////////////////////////////////////////
StorageItemImpl::StorageItemImpl()
{
	StoreType = 0;
	StoreLoc = 0;
	StoreNext = 0;
	StorePrev = 0;
	StoreParent = 0;
	StoreChild = 0;
	StoreSize = 0;
	StoreAllocatedSize = 0;

	Tree = 0;
	Next = 0;
	Prev = 0;
	Parent = 0;
	Child = 0;
	Object = 0;
}

StorageItemImpl::~StorageItemImpl()
{
	DeleteObj(Child);
	DeleteObj(Next);

	if (Object)
	{
		if (Object->GetDirty())
		{
			GFile *f = 0;
			if (Tree)
			{
				f = &Tree->File;
				if (f)
				{
					f->Seek(StoreLoc, SEEK_SET);
					Serialize(*f, true);
					Object->Serialize(*f, true);
				}
			}
		}

		Object->Store = 0;
		DeleteObj(Object);
	}

	if (Parent)
	{
		Parent->Child = 0;
	}

	if (Prev)
	{
		Prev->Next = 0;
	}
}

bool StorageItemImpl::EndOfObj(GFile &f)
{
	int Pos = f.GetPos();
	int ObjStart = StoreLoc + sizeof(StorageItemHeader);
	bool Status = !((Pos >= ObjStart) AND (Pos < (ObjStart + StoreSize)));
	return Status;
}

bool StorageItemImpl::WriteHeader(GFile &f)
{
	StorageItemHeader ItemHeader;

	f.Seek(StoreLoc, SEEK_SET);
	ItemHeader.Magic = STORAGE_ITEM_MAGIC;
	ItemHeader.Type = StoreType;
	ItemHeader.ObjSize = StoreSize;
	ItemHeader.AllocatedSize = StoreAllocatedSize;
	ItemHeader.Next = StoreNext;
	ItemHeader.Prev = StorePrev;
	ItemHeader.Parent = StoreParent;
	ItemHeader.Child = StoreChild;

	return (f.Write(&ItemHeader, sizeof(ItemHeader)) == sizeof(ItemHeader));
}

int StorageItemImpl::GetType()
{
	return StoreType;
}

void StorageItemImpl::SetType(int i)
{
	StoreType = i;
}

int StorageItemImpl::GetTotalSize()
{
	return sizeof(StorageItemHeader) + StoreAllocatedSize;
}

int StorageItemImpl::GetObjectSize()
{
	return StoreSize;
}

StorageKit *StorageItemImpl::GetTree()
{
	return Tree;
}

bool StorageItemImpl::MoveToLoc(int NewLoc)
{
	bool Status = false;
	if (NewLoc > 0)
	{
		if (Tree)
		{
			int SrcLoc = StoreLoc + sizeof(StorageItemHeader);
			int DestLoc = NewLoc + sizeof(StorageItemHeader);
			StorageItemImpl *Item;

			GFile &File = Tree->File;
			int Type = StoreType;
			Status = true;

			// turn current object into free space
			StoreType = -1; // free space
			Status &= WriteHeader(File);
			StoreType = Type;
			
			// get new location
			StoreLoc = NewLoc;

			// if root?
			if (Tree->Root == this)
			{
				// we are root
				Tree->StoreLoc = StoreLoc;
				Tree->Serialize(File, true);
			}

			// update surrounding pointers
			Item = (StorageItemImpl*)GetParent();
			if (Item)
			{
				Item->StoreChild = StoreLoc;
				Status &= Item->Serialize(File, true);
			}

			Item = (StorageItemImpl*)GetChild();
			if (Item)
			{
				Item->StoreParent = StoreLoc;
				Status &= Item->WriteHeader(File);
			}

			Item = (StorageItemImpl*)GetNext();
			if (Item)
			{
				Item->StorePrev = StoreLoc;
				Status &= Item->WriteHeader(File);
			}

			Item = (StorageItemImpl*)GetPrev();
			if (Item)
			{
				Item->StoreNext = StoreLoc;
				Status &= Item->WriteHeader(File);
			}

			if (Status)
			{
				Status &= WriteHeader(File);
				if (Status)
				{
					int DataSize = StoreSize;
					int BufSize = 64 << 10;
					uchar *Buf = new uchar[BufSize];
					if (Buf)
					{
						while (DataSize > 0)
						{
							int r = min(DataSize, BufSize);
							File.Seek(SrcLoc, SEEK_SET);
							r = File.Read(Buf, r);
							File.Seek(DestLoc, SEEK_SET);
							File.Write(Buf, r);

							SrcLoc += r;
							DestLoc += r;
							DataSize -= r;
						}

						DeleteArray(Buf);
					}
				}
			}
		}
	}
	return Status;
}

GFile *StorageItemImpl::GotoObject(const char *file, int line)
{
	GSubFilePtr *f = 0;
	
	if (Tree)
	{
		f = Tree->File.Create(file, line);
		if (f)
		{
			int Pos = StoreLoc + sizeof(StorageItemHeader);
			if (f->Seek(Pos, SEEK_SET) == Pos)
			{
				f->SetStatus(true);
			}
			else
			{
				DeleteObj(f);
			}
		}
	}

	return f;
}

bool StorageItemImpl::Serialize(GFile &f, bool Write, int Flags)
{
	bool Status = false;
	StorageItemHeader ItemHeader;

	if (Write)
	{
		if ((Flags & STORAGE_ITEM_NOSIZE) OR
			SetSize(Sizeof()))
		{
			f.Seek(StoreLoc, SEEK_SET);
			if (f.GetPos() == StoreLoc)
			{
				ItemHeader.Magic = STORAGE_ITEM_MAGIC;
				ItemHeader.Type = StoreType;
				ItemHeader.ObjSize = StoreSize;
				ItemHeader.AllocatedSize = StoreAllocatedSize;
				ItemHeader.Next = StoreNext;
				ItemHeader.Prev = StorePrev;
				ItemHeader.Parent = StoreParent;
				ItemHeader.Child = StoreChild;

				if (f.Write(&ItemHeader, sizeof(ItemHeader)) == sizeof(ItemHeader))
				{
					Status = true;
				}
				#if defined _DEBUG && WIN32
				else
				{
					// write failed!!
					DWORD Error = GetLastError();
					LgiAssert(0);
				}
				#endif
			}
		}
	}
	else
	{
		f.Seek(StoreLoc, SEEK_SET);
		if (f.GetPos() == StoreLoc)
		{
			if (f.Read(&ItemHeader, sizeof(ItemHeader)) == sizeof(ItemHeader))
			{
				if (ItemHeader.Magic == STORAGE_ITEM_MAGIC)
				{
					StoreType = ItemHeader.Type;
					StoreSize = ItemHeader.ObjSize;
					StoreAllocatedSize = ItemHeader.AllocatedSize;
					StoreNext = ItemHeader.Next;
					StorePrev = ItemHeader.Prev;
					StoreParent = ItemHeader.Parent;
					StoreChild = ItemHeader.Child;

					Status = true;
				}
				#ifdef _DEBUG
				else
				{
					char Str[256];
					sprintf(Str, "Load Node failed at: %i", StoreLoc);
					LgiMsg(NULL, Str, "Debug", MB_OK);
				}
				#endif
			}
		}
	}

	return Status;
}

bool StorageItemImpl::Save()
{
	if (SetSize(Object->Sizeof()))
	{
		GFile *f = GotoObject(__FILE__, __LINE__);
		if (f)
		{
			bool Status = Object->Serialize(*f, true);
			DeleteObj(f);
			return Status;
		}
	}

	return false;
}

bool StorageItemImpl::SetSize(int NewSize)
{
	bool Status = true;

	if (NewSize > StoreAllocatedSize)
	{
		if (Tree)
		{
			StorageItemHeader ItemHeader;
			StorageItemImpl *Item;

			GFile &File = Tree->File;
			int Type = StoreType;

			// turn current object into free space
			StoreType = -1; // free space
			Status &= WriteHeader(File);
			StoreType = Type;
			
			// get new location
			StoreLoc = File.GetSize();

			// update surrounding pointers
			Item = (StorageItemImpl*)GetParent();
			if (Item)
			{
				Item->StoreChild = StoreLoc;
				Status &= Item->WriteHeader(File);
			}

			Item = (StorageItemImpl*)GetChild();
			if (Item)
			{
				Item->StoreParent = StoreLoc;
				Status &= Item->WriteHeader(File);
			}

			Item = (StorageItemImpl*)GetNext();
			if (Item)
			{
				Item->StorePrev = StoreLoc;
				Status &= Item->WriteHeader(File);
			}

			Item = (StorageItemImpl*)GetPrev();
			if (Item)
			{
				Item->StoreNext = StoreLoc;
				Status &= Item->WriteHeader(File);
			}

			if (Status)
			{
				StoreSize = NewSize;
				StoreAllocatedSize = NewSize;
				Status &= Serialize(File, true);
				if (Status AND Object)
				{
					File.Seek(StoreLoc + sizeof(StorageItemHeader), SEEK_SET);
					Status &= Object->Serialize(File, true);
				}
			}
		}
		else
		{
			Status = false;
		}
	}

	StoreSize = NewSize;
	return Status;
}

StorageItem *StorageItemImpl::GetNext()
{
	if (StoreNext == StoreLoc)
	{
		StoreNext = 0;
	}
	else if (!Next)
	{
		Next = Tree->LoadLocation(StoreNext);
		if (Next)
		{
			Next->Prev = this;
		}
		// #ifdef _DEBUG
		else if (StoreNext)
		{
			// a bug!
			// patch the next pointer to null.
			StoreNext = 0;
			if (Tree)
			{
				WriteHeader(Tree->File);
			}

			char Str[256];
			sprintf(Str,
					"StorageItemImpl::GetNext(...) failed at %i\n"
					"Corrupted linkage cleaned up.",
					StoreLoc);
			LgiMsg(NULL, Str, "Mail Store Error", MB_OK);
		}
		// #endif
	}

	return Next;
}

StorageItem *StorageItemImpl::GetPrev()
{
	if (StorePrev == StoreLoc)
	{
		StorePrev = 0;
	}
	else if (!Prev)
	{
		Prev = Tree->LoadLocation(StorePrev);
		if (Prev)
		{
			Prev->Next = this;
		}
	}

	return Prev;
}

StorageItem *StorageItemImpl::GetParent()
{
	if (!Parent)
	{
		Parent = (StorageItemImpl*)Tree->LoadLocation(StoreParent);
		if (Parent)
		{
			Parent->Child = this;
		}
	}

	return Parent;
}

StorageItem *StorageItemImpl::GetChild()
{
	if (!Child)
	{
		Child = Tree->LoadLocation(StoreChild);
		if (Child)
		{
			Child->Parent = this;
		}
		// #ifdef _DEBUG
		else if (StoreChild)
		{
			// a bug!
			// patch the next pointer to null.
			StoreChild = 0;
			if (Tree)
			{
				WriteHeader(Tree->File);
			}

			char Str[256];
			sprintf(Str,
					"StorageItemImpl::GetChild(...) failed at %i\n"
					"Corrupted linkage cleaned up.",
					StoreLoc);
			LgiMsg(NULL, Str, "Mail Store Error", MB_OK);
		}
		// #endif
	}

	return Child;
}

StorageItem *StorageItemImpl::CreateNext(StorageObj *Obj)
{
	if (Tree AND Obj)
	{
		Next = (StorageItemImpl*)GetNext();
		if (Next)
		{
			Tree->DeleteItem(Next);
			DeleteObj(Next);
		}

		Next = Tree->CreateItem(Obj);
		if (Next)
		{
			Next->StoreType = Obj->Type();

			// link this object to the next object
			StoreNext = Next->StoreLoc;
			Next->StorePrev = StoreLoc;
			Next->Prev = this;

			// save myself
			Serialize(Tree->File, true);
			
			// save next
			Next->Serialize(Tree->File, true);
		}
	}
	return Next;
}

StorageItem *StorageItemImpl::CreateChild(StorageObj *Obj)
{
	if (Tree AND Obj)
	{
		Child = (StorageItemImpl*)GetChild();
		if (Child)
		{
			Tree->DeleteItem(Child);
			DeleteObj(Child);
		}

		Child = Tree->CreateItem(Obj);
		if (Child)
		{
			Child->StoreType = Obj->Type();

			// link this object to the Child object
			StoreChild = Child->StoreLoc;
			Child->StoreParent = StoreLoc;
			Child->Parent = this;

			// save myself
			Serialize(Tree->File, true, STORAGE_ITEM_NOSIZE);
			
			// save Child
			Child->Serialize(Tree->File, true, STORAGE_ITEM_NOSIZE);
		}
	}
	return Child;
}

StorageItem *StorageItemImpl::CreateSub(StorageObj *Obj)
{
	StorageItemImpl *Next = (StorageItemImpl*)GetChild();
	if (Next)
	{
		Child = Tree->CreateItem(Obj);
		if (Child)
		{
			Child->StoreType = Obj->Type();

			// link this object to the Child object
			StoreChild = Child->StoreLoc;
			Child->StoreParent = StoreLoc;
			Child->Parent = this;

			// link this object to the Next object
			Next->StorePrev = Child->StoreLoc;
			Next->Prev = Child;
			Child->StoreNext = Next->StoreLoc;
			Child->Next = Next;

			// delink the next object from the parent
			Next->Parent = 0;
			Next->StoreParent = 0;

			// save objects
			Serialize(Tree->File, true, STORAGE_ITEM_NOSIZE);
			Child->Serialize(Tree->File, true, STORAGE_ITEM_NOSIZE);
			Next->Serialize(Tree->File, true, STORAGE_ITEM_NOSIZE);
		}
	}
	else
	{
		CreateChild(Obj);
	}
	return Child;
}

bool StorageItemImpl::DeleteChild(StorageItem *Item)
{
	bool Status = false;
	StorageItemImpl *i = (StorageItemImpl*)Item;
	if (i AND
		i->Parent == this)
	{
		if (i->Prev)
		{
			i->Prev->StoreNext = i->StoreNext;
			i->Prev->Next = i->Next;
			i->Prev->Serialize(Tree->File, true, STORAGE_ITEM_NOSIZE);
		}
		if (i->Next)
		{
			i->Next->StorePrev = i->StorePrev;
			i->Next->Prev = i->Prev;
			i->Next->Serialize(Tree->File, true, STORAGE_ITEM_NOSIZE);
		}
		if (Child == i)
		{
			i->Next->Parent = this;
			i->Next->StoreParent = StoreLoc;
			StoreChild = i->Next->StoreLoc;
			Child = i->Next;
			Next->Parent = this;
			Serialize(Tree->File, true, STORAGE_ITEM_NOSIZE);
		}

		i->StoreType = 0;
		i->StoreNext = 0;
		i->StorePrev = 0;
		i->StoreParent = 0;
		Status = i->Serialize(Tree->File, true, STORAGE_ITEM_NOSIZE);
	}

	return Status;
}

bool StorageItemImpl::DeleteAllChildren()
{
	StorageItemImpl *i = (StorageItemImpl*)GetChild();
	while (i AND i->StoreNext)
	{
		i = (StorageItemImpl*)i->GetNext();
	}

	while (i)
	{
		StorageItemImpl *Prev = (StorageItemImpl*)i->GetPrev();
		i->DeleteAllChildren();
		Tree->DeleteItem(i);
		i = Prev;
	}

	return true;
}

int StorageItemImpl::Sizeof()
{
	return (Object) ? Object->Sizeof() : 0;
}

///////////////////////////////////////////////////////////////////////////
StorageKitImpl::StorageKitImpl(char *filename)
	: File(&Lock)
{
	Status = false;
	ReadOnly = false;
	StoreLoc = 0;
	Root = 0;
	UserData = 0;
	Version = 0;

	FileName = filename;
	if (File.Open(FileName, O_READWRITE))
	{
		Status = Serialize(File, File.GetSize() == 0);
	}
	else if (File.Open(FileName, O_READ))
	{
		ReadOnly = true;
		Status = Serialize(File, File.GetSize() == 0);
	}
}

StorageKitImpl::~StorageKitImpl()
{
	DeleteObj(Root);

	File.Close();
	Status = false;
}

uint64 StorageKitImpl::GetFileSize()
{
	return File.GetSize();
}

void StorageKitImpl::SetVersion(int i)
{
	Version = i;
	Serialize(File, true);
}

StorageItem *StorageKitImpl::GetRoot()
{
	if (!Root)
	{
		Root = LoadLocation(StoreLoc);
	}

	return Root;
}

StorageItem *StorageKitImpl::CreateRoot(StorageObj *Obj)
{
	if (!ReadOnly)
	{
		StorageItemImpl *OldRoot = (StorageItemImpl*)GetRoot();
		if (OldRoot)
		{
			DeleteItem(OldRoot);
		}

		Root = CreateItem(Obj);
		if (Root)
		{
			StoreLoc = Root->StoreLoc;
			Serialize(File, true);
		}
		
		return Root;
	}

	return 0;
}

StorageItemImpl *StorageKitImpl::CreateItem(StorageObj *Obj)
{
	if (!ReadOnly)
	{
		StorageItemImpl *Item = new StorageItemImpl;
		if (Item)
		{
			// set items new location: at the end of the file
			Item->Tree = this;
			Item->StoreType = Obj->Type();
			Item->StoreLoc = File.GetSize();
			Item->Object = Obj;
			Item->StoreSize = Item->StoreAllocatedSize = Item->Sizeof();
			Obj->SetDirty();

			// write the item to the disk
			if (Item->Serialize(File, true))
			{
				// write the object to disk for the first time
				GFile *f = Item->GotoObject(__FILE__, __LINE__);
				if (f)
				{
					bool Status = Item->Object->Serialize(*f, true);
					DeleteObj(f);
					if (Status)
					{
						return Item;
					}
				}
			}

			// something failed
			DeleteObj(Item);
		}
	}

	return 0;
}

bool StorageKitImpl::SeparateItem(StorageItem *Item)
{
	bool Status = false;
	StorageItemImpl *ItemImpl = (StorageItemImpl*) Item;

	if (!ReadOnly AND Item)
	{
		StorageItemImpl *Parent = (StorageItemImpl*) ItemImpl->GetParent();
		StorageItemImpl *Next = (StorageItemImpl*) ItemImpl->GetNext();
		StorageItemImpl *Prev = (StorageItemImpl*) ItemImpl->GetPrev();

		if (Parent)
		{
			if (Next)
			{
				// attach the parent to the next
				Parent->StoreChild = Next->StoreLoc;
				Parent->Child = Next;
				Next->StorePrev = 0;
				Next->Prev = 0;
				Next->StoreParent = Parent->StoreLoc;
				Next->Parent = Parent;
			}
			else
			{
				// we are an only child
				Parent->StoreChild = 0;
				Parent->Child = 0;
			}
		}
		else
		{
			if (Next)
			{
				if (Prev)
				{
					Next->StorePrev = Prev->StoreLoc;
					Next->Prev = Prev;
					Prev->StoreNext = Next->StoreLoc;
					Prev->Next = Next;
				}
				else
				{
					Next->StorePrev = 0;
					Next->Prev = 0;
				}
			}
			else if (Prev)
			{
				Prev->StoreNext = 0;
				Prev->Next = 0;
			}
		}

		if (Parent) Parent->Serialize(File, true);
		if (Next) Next->Serialize(File, true);
		if (Prev) Prev->Serialize(File, true);

		ItemImpl->Next = 0;
		ItemImpl->StoreNext = 0;
		ItemImpl->Parent = 0;
		ItemImpl->StoreParent = 0;
		ItemImpl->Prev = 0;
		ItemImpl->StorePrev = 0;
		Status = ItemImpl->Serialize(File, true);
	}

	return Status;
}

bool StorageKitImpl::DeleteItem(StorageItem *ItemVirtual)
{
	bool Status = true;
	StorageItemImpl *Item = (StorageItemImpl*) ItemVirtual;

	if (!ReadOnly AND Item)
	{
		DeleteItem(Item->GetChild());
		DeleteItem(Item->GetNext());

		StorageItemImpl *Prev = (StorageItemImpl*)Item->GetPrev();
		if (Prev)
		{
			Prev->StoreNext = 0;
			Prev->Next = 0;
			Prev->Serialize(File, true);
		}

		StorageItemImpl *Parent = (StorageItemImpl*)Item->GetParent();
		if (Parent)
		{
			Parent->StoreChild = 0;
			Parent->Child = 0;
			Parent->Serialize(File, true);
		}

		Item->StoreType = 0;
		Status = Item->Serialize(File, true);
	}

	return Status;
}

void StorageKitImpl::AddItem(StorageItemImpl *Item, List<Block> &Blocks)
{
	if (Item)
	{
		bool Insert = (Validator) ? Validator->CompactValidate(0, Item) : true;
		if (Insert)
		{
			Block *New = new Block;
			if (New)
			{
				New->Item = Item;
				New->Size = sizeof(StorageItemHeader) + Item->StoreAllocatedSize;

				if (Item->StoreAllocatedSize == 0 OR Item->StoreSize == 0)
				{
					if (Item->Object)
					{
						Item->StoreSize = Item->Object->Sizeof();
						Item->WriteHeader(File);
					}
				}

				Blocks.Insert(New);
			}
		}

		AddItem((StorageItemImpl*) Item->GetChild(), Blocks);
		AddItem((StorageItemImpl*) Item->GetNext(), Blocks);

		if (!Insert)
		{
			SeparateItem(Item);
		}
	}
}

bool StorageKitImpl::Compact(Progress *p, bool Interactive, StorageValidator *validator)
{
	DoEvery Time(1000);
	bool Error = false;
	List<Block> Blocks;
	List<Block> Sorted;

	Validator = validator;

	if (p)
	{
		p->SetDescription("Adding objects...");
		LgiYield();
	}

	StorageItemImpl *Item = (StorageItemImpl*) GetRoot();
	if (Item)
	{
		// get a list of all valid blocks
		AddItem(Item, Blocks);

		if (p)
		{
			p->SetDescription("Sorting objects...");
			p->SetLimits(0, Blocks.Length()-1);
			LgiYield();
		}

		// sort the blocks
		while (Blocks.Length() > 0)
		{
			Block *Min = 0;
			for (Block *b = Blocks.First(); b; b = Blocks.Next())
			{
				if (!Min) Min = b;
				if (Min)
				{
					if (b->Item->StoreLoc < Min->Item->StoreLoc)
					{
						Min = b;
					}
				}
			}

			if (Min)
			{
				Blocks.Delete(Min);
				Sorted.Insert(Min);
			}
			else break;

			if (p AND Time.DoNow())
			{
				p->Value(Sorted.Length());
				LgiYield();
			}
		}

		if (p)
		{
			p->SetDescription("Removing unused space...");
			LgiYield();
		}

		// remove space between blocks
		int n=0;
		for (Block *b = Sorted.First(); b; n++)
		{
			Block *Next = Sorted.Next();
			if (Next)
			{
				int EndOfCurrent = b->Item->StoreLoc + b->Size;
				if (Next->Item->StoreLoc > EndOfCurrent)
				{
					// gap found
					// need to move the Next->Item to EndOfCurrent
					if (!Next->Item->MoveToLoc(EndOfCurrent))
					{
						// somthing went wrong
						// lets just leave before we screw everything up
						Error = true;
						break;
					}
				}
			}
			else
			{
				// last block so we truncate the file at the end of the block
				int EndOfCurrent = b->Item->StoreLoc + b->Size;
				File.SetSize(EndOfCurrent);
			}

			b = Next;

			if (p AND Time.DoNow())
			{
				p->Value(n);
				LgiYield();
			}
		}
	}

	for (Block *b = Sorted.First(); b; b = Sorted.Next())
	{
		DeleteObj(b);
	}
	
	return !Error;
}

StorageItemImpl *StorageKitImpl::LoadLocation(int Loc)
{
	StorageItemImpl *Item = 0;

	if (Loc > 0 AND Status)
	{
		Item = new StorageItemImpl;
		if (Item)
		{
			Item->StoreLoc = Loc;
			if (Item->Serialize(File, false))
			{
				Item->Tree = this;
			}
			else
			{
				DeleteObj(Item);
			}
		}
	}

	return Item;
}

bool StorageKitImpl::Serialize(GFile &f, bool Write)
{
	bool Status = false;
	StorageHeader Header;

	if (Write)
	{
		ZeroObj(Header);
		Header.Magic = STORAGE_MAGIC;
		Header.FirstLoc = StoreLoc;
		Header.Version = Version;

		File.Seek(0, SEEK_SET);
		Status = File.Write(&Header, sizeof(Header)) == sizeof(Header);
	}
	else
	{
		File.Seek(0, SEEK_SET);
		Status = File.Read(&Header, sizeof(Header)) == sizeof(Header);
		if (Status)
		{
			Status = (Header.Magic == STORAGE_MAGIC);
			if (Status)
			{
				StoreLoc = Header.FirstLoc;
				Version = Header.Version;
			}
		}
	}
	return Status;
}

bool StorageKitImpl::AttachItem(StorageItem *ItemVirtual, StorageItem *ToVirtual, NodeRelation Relationship)
{
	bool Status = false;
	StorageItemImpl *Item = (StorageItemImpl*)ItemVirtual;
	StorageItemImpl *To = (StorageItemImpl*)ToVirtual;

	if (Item AND
		To AND
		SeparateItem(Item))
	{
		if (Relationship == NodeNext)
		{
			// reattach the object as the "next"
			// moving the current next object along one

			// "To" is actually going to be Item's "previous" object
			//
			// From:
			//	+ To
			//  + [Next]
			//
			// To:
			//  + To
			//  + Item
			//  + [Next]

			StorageItemImpl *Next = (StorageItemImpl*) To->GetNext();
			if (Next)
			{
				// Item has NewParent as it's prev
				// and Next as it's next
				Item->StoreNext = Next->StoreLoc;
				Item->Next = Next;

				Next->StorePrev = Item->StoreLoc;
				Next->Prev = Item;

				Next->Serialize(File, true);
			}

			To->StoreNext = Item->StoreLoc;
			To->Next = Item;
			Item->StorePrev = To->StoreLoc;
			Item->Prev = To;
		}
		else if (Relationship == NodePrev)
		{
			// reattach the object as the "prev"
			// moving the current prev object along one

			// "To" is actually going to be Item's "next" object
			//
			// From:
			//  + [Prev]
			//	+ To
			//
			// To:
			//  + [Prev]
			//  + Item
			//  + To

			StorageItemImpl *Prev = (StorageItemImpl*) To->GetPrev();
			if (Prev)
			{
				Item->StorePrev = Prev->StoreLoc;
				Item->Prev = Prev;

				Prev->StoreNext = Item->StoreLoc;
				Prev->Next = Item;

				Prev->Serialize(File, true);
			}

			To->StorePrev = Item->StoreLoc;
			To->Prev = Item;
			Item->StoreNext = To->StoreLoc;
			Item->Next = To;
		}
		else if (Relationship == NodeChild)
		{
			// Reattach the object as the "child"
			//
			// From:
			//	+ NewParent
			//		+ Child
			//
			// To:
			//  + NewParent
			//		+ Item
			//		+ Child
			//

			StorageItemImpl *Next = (StorageItemImpl*) To->GetChild();
			if (Next)
			{
				Item->StorePrev = 0;
				Item->Prev = 0;
				Item->StoreNext = Next->StoreLoc;
				Item->Next = Next;

				Next->StorePrev = Item->StoreLoc;
				Next->Prev = Item;
				Next->StoreParent = 0;
				Next->Parent = 0;

				Next->Serialize(File, true);
			}

			To->StoreChild = Item->StoreLoc;
			To->Child = Item;
			Item->StoreParent = To->StoreLoc;
			Item->Parent = To;
		}

		// Write the changes to disk...
		To->Serialize(File, true);
		Status = Item->Serialize(File, true);
	}

	return Status;
}
