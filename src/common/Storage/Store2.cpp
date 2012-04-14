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
#include "GSegmentTree.h"
#ifndef WIN32
#include "errno.h"
#endif
#include "GProgressDlg.h"

#define STORE_DIR_ALLOC					8

/////////////////////////////////////////////////////////////////////////////////////////
#include "Store2.h"

#ifndef IDS_STORE2_ERROR_COMPACT

#define IDS_STORE2_ERROR_COMPACT		2000
#define IDS_STORE2_DISCARD_FIX			2001
#define IDS_STORE2_SAVE_DEBUG			2002
#define IDC_STORE2_KEEP					2003
#define IDS_CANCEL						2004

const char *Store2_LgiLoadString(int ref)
{
	switch (ref)
	{
		case IDS_STORE2_ERROR_COMPACT:
			return "An object overlap has been found. This error is caused\nby a bug in the application or storage code or a corruption\ncaused by a crash.\n\nThe contested area is the bytes from %u to %u (length %u)\n\n%s\n%s\nWould you like to:";
		case IDS_STORE2_DISCARD_FIX:
			return "Discard/Fix";
		case IDS_STORE2_SAVE_DEBUG:
			return "Save/Debug";
		case IDS_CANCEL:
			return "Cancel";
		case IDC_STORE2_KEEP:
			return "Keep Item %u";
	}

	return 0;
}

#else

#define Store2_LgiLoadString			LgiLoadString

#endif

using namespace Storage2;

StorageItemHeader::StorageItemHeader()
{
	Magic = STORAGE2_ITEM_MAGIC;

	// object descriptor
	Type = 0;
	DataLoc = 0;
	DataSize = 0;
	
	// heirarchy descriptor
	ParentLoc = 0;
	DirLoc = 0;
	DirCount = 0;
	DirAlloc = 0;
}

bool StorageItemHeader::Serialize(GFile &f, bool Write)
{
	bool Status = true;
	int64 Here = f.GetPos();

	if (Write)
	{
		Magic = STORAGE2_ITEM_MAGIC;

		f << Magic;
		f << Type;
		f << DataLoc;
		f << DataSize;
		f << ParentLoc;
		f << DirLoc;
		f << DirCount;
		f << DirAlloc;
	}
	else
	{
		f >> Magic;
		f >> Type;
		f >> DataLoc;
		f >> DataSize;
		f >> ParentLoc;
		f >> DirLoc;
		f >> DirCount;
		f >> DirAlloc;

		Status = Magic == STORAGE2_ITEM_MAGIC;
		if (!Status)
		{
			printf("%s:%i - Magic wrong %x != %x\n",
				_FL, Magic, STORAGE2_ITEM_MAGIC);
			return false;
		}
	}

	if (Here + 32 != f.GetPos())
	{
		if (!Write)
		{
			int64 Diff = Here + 32 - f.GetPos();
			printf("%s:%i - Wrong length, Here+32=" LGI_PrintfInt64 " != GetPos=" LGI_PrintfInt64
				" Diff=" LGI_PrintfInt64
				" FileSize=" LGI_PrintfInt64 "\n",
				_FL, Here+32, f.GetPos(), Diff, f.GetSize());
		}
			
		return false;
	}
	
	return true;
}

void StorageItemHeader::SwapBytes()
{
	#ifdef __BIG_ENDIAN__
	Magic = LgiSwap32(Magic);
	Type = LgiSwap32(Type);
	DataLoc = LgiSwap32(DataLoc);
	DataSize = LgiSwap32(DataSize);
	ParentLoc = LgiSwap32(ParentLoc);
	DirLoc = LgiSwap32(DirLoc);
	DirCount = LgiSwap32(DirCount);
	DirAlloc = LgiSwap32(DirAlloc);
	#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
StorageItemImpl::StorageItemImpl(StorageItemHeader *header)
{
	// Vars
	StoreLoc = 0;
	HeaderDirty = false;
	Header = header;
	Dir = 0;
	Temp = 0;

	// Pointers
	Tree = 0;
	Next = 0;
	Prev = 0;
	Parent = 0;
	Child = 0;
	Object = 0;
}

StorageItemImpl::~StorageItemImpl()
{
	// LgiTrace("%p::~StorageItemImpl obj=%p type=%x\n", this, Object, Object ? Object->Type()  : 0);

	StorageItemImpl *i, *n;
	for (i = Child; i; i = n)
	{
		n = i->Next;
		i->Next = 0;
		if (n) n->Prev = 0;

		DeleteObj(i);
	}
	for (i = Next; i; i = n)
	{
		n = i->Next;
		i->Next = 0;
		if (n) n->Prev = 0;

		DeleteObj(i);
	}
	DeleteObj(Temp);

	if (Object)
	{
		SetDirty(false);
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

	DeleteArray(Dir);
}

int StorageItemImpl::GetType()
{
	return Header ? Header->Type : 0;
}

void StorageItemImpl::SetType(int i)
{
	if (Header) Header->Type = i;
}

int StorageItemImpl::GetTotalSize()
{
	if (Header)
	{
		return	((1 + Header->DirAlloc - Header->DirCount) * sizeof(StorageItemHeader)) + 
				Header->DataSize;
	}

	return 0;
}

int StorageItemImpl::GetObjectSize()
{
	return (Header) ? Header->DataSize : 0;
}

StorageKit *StorageItemImpl::GetTree()
{
	return Tree;
}

bool StorageItemImpl::EndOfObj(GFile &f)
{
	if (!Header)
	{
		return true;
	}

	int64 Pos = f.GetPos();
	bool Before = Pos < 0;
	bool After = Pos >= Header->DataSize;

	return Before || After;
}

GFile *StorageItemImpl::GotoObject(const char *file, int line)
{
	GSubFilePtr *f = 0;
	
	if (Header &&
		Tree &&
		Tree->_ValidLoc(Header->DataLoc))
	{
		f = Tree->CreateFilePtr(file, line);
		if (f)
		{
			if (f->SetSub(Header->DataLoc, Header->DataSize))
			// if (f->SetPos(Header->DataLoc) == Header->DataLoc) // seek to the objects location
			{
				f->SetStatus(true);
			}
			else
			{
				DeleteObj(f);
			}
		}
		else
		{
			printf("%s:%i - CreateFilePtr failed.\n", _FL);
		}
	}
	else
	{
		printf("%s:%i - Bad Param or loc=%u (type=%x)\n", _FL, Header->DataLoc, Header->Type);
	}

	return f;
}

void StorageItemImpl::CleanAll()
{
	SetDirty(false);

	for (StorageItemImpl *c=Child; c; c=c->Next)
	{
		c->CleanAll();
	}
}

bool StorageItemImpl::SetDirty(bool Dirty)
{
	bool Status = true;

	if (Dirty)
	{
		HeaderDirty = true;
	}
	else if (Tree->IsOk())
	{
		if (Object && Object->GetDirty())
		{
			GSubFilePtr *Ptr = Tree->CreateFilePtr(_FL);
			if (Ptr)
			{
				Status &= SerializeObject(*Ptr, true);
				DeleteObj(Ptr);
			}
		}

		if (HeaderDirty)
		{
			Status &= SerializeHeader(*Tree->File, true);
		}
	}

	return Status;
}

bool StorageItemImpl::SerializeHeader(GFile &f, bool Write)
{
	bool Status = false;

	if (Tree->IsOk() &&
		Header &&
		(!Write OR HeaderDirty))
	{
		if (Tree->Lock(_FL))
		{
			if (Tree->_ValidLoc(StoreLoc))
			{
				f.SetPos(StoreLoc);
				if (f.GetPos() == StoreLoc)
				{
					Status = Header->Serialize(f, Write);
					if (Status)
					{
						HeaderDirty = false;
					}
					else
					{
						printf("%s:%i - Header->Serialize failed at file offset 0x%x .\n", _FL, StoreLoc);
					}

					#if defined(_DEBUG) && defined(_WINDOWS)
					if (!Status)
					{
						// write failed!!
						DWORD Error = GetLastError();
						LgiAssert(0);
					}
					#endif
				}
			}
			else
			{
				printf("%s:%i - _ValidLoc failed.\n", _FL);
			}

			Tree->Unlock();
		}
	}
	else
	{
		Status = true;
	}

	return Status;
}

bool StorageItemImpl::SerializeObject(GSubFilePtr &f, bool Write)
{
	bool Status = false;

	if (Tree->IsOk() &&
		Header &&
		Object)
	{
		if (!Write OR Object->GetDirty())
		{
			if (Write)
			{
				// check size is big enough
				int Size = Object->Sizeof();
				if (Size != Header->DataSize)
				{
					// change required
					if (Size > Header->DataSize)
					{
						Header->DataLoc = f.GetSize();
					}
					Header->DataSize = Size;
					HeaderDirty = true;
				}
			}

			if (Tree->_ValidLoc(Header->DataLoc))
			{
				// write object
				f.SetPos(Header->DataLoc);
				if (f.GetPos() == Header->DataLoc)
				{
					f.SetStatus(true);

					// limit the file to the area occupied by the object
					f.SetSub(Header->DataLoc, Header->DataSize);
					
					// ask object to read/write itself
					Status = Object->Serialize(f, Write);

					// unresticted the file
					int64 CurrentPos = f.GetPos();
					f.ClearSub();

					// Check the output
					if (Status)
					{
						Tree->StorageObj_SetDirty(Object, false);
					}
					else
					{
						// Trucate object to however much it did manage to write 
						// out, for integrity's sake
						Header->DataSize = CurrentPos;
						HeaderDirty = true;
					}

					static bool IgnoreStorageErrors = false;
					if (!IgnoreStorageErrors &&
						Status &&
						CurrentPos != Header->DataSize)
					{
						char Msg[512];
						uint Error = 0;

						strsafecpy(Msg, LgiLoadString(L_STORE_WRITE_ERR, "Storage failed to write the object to disk:\n\n"), sizeof(Msg));

						if (!Status)
						{
							#ifdef WIN32
							Error = GetLastError();
							#else
							Error = errno;
							#endif

							sprintf(Msg+strlen(Msg), LgiLoadString(L_STORE_OS_ERR, "OS error code: %u (0x%x)\n"), Error, Error);
						}

						if (CurrentPos != Header->DataLoc + Header->DataSize)
						{
							int Written = CurrentPos;
							int Diff = Written - Header->DataSize;
							sprintf(Msg+strlen(Msg),
									LgiLoadString(	L_STORE_MISMATCH,
													"Object write size mismatch:\n"
													"\tHeader: %u\n"
													"\tBytes written: %u\n"
													"\tDiff: %i\n"
													"\tType: %x\n"),
									Header->DataSize,
									Written,
									Diff,
									Header->Type);
						}

						LgiTrace("%s", Msg);

						strcpy(	Msg + strlen(Msg),
								LgiLoadString(L_STORE_RESTART, "\nYou should restart the application and then report this problem.\n"
								"Ignore all furthur such errors for this session?"));
						if (LgiMsg(0, Msg, "Storage2 Error", MB_YESNO) == IDYES)
						{
							IgnoreStorageErrors = true;
						}

						LgiAssert(0);
						Status = false;
					}
				}
			}
		}
		else
		{
			Status = true;
		}
	}

	return Status;
}

bool StorageItemImpl::Save()
{
	bool Status = false;

	if (Tree->IsOk() &&
		Header &&
		Object)
	{
		Tree->StorageObj_SetDirty(Object, true);
		
		GSubFilePtr *Sub = Tree->CreateFilePtr(_FL);
		if (Sub)
		{
			Status = SerializeObject(*Sub, true);
			SerializeHeader(*Sub, true);
			DeleteObj(Sub);
		}
	}

	return Status;
}

StorageItem *StorageItemImpl::GetNext()
{
	return Next;
}

StorageItem *StorageItemImpl::GetPrev()
{
	return Prev;
}

StorageItem *StorageItemImpl::GetParent()
{
	return Parent;
}

extern void TraceTime(char *s);

StorageItem *StorageItemImpl::GetChild()
{
	if (Tree->IsOk() &&
		!Child &&
		Header &&
		Header->DirCount > 0 &&
		Header->DirLoc > 0)
	{
		if (Tree->Lock(_FL))
		{
			// Validity check
			if (Header->Magic != STORAGE2_ITEM_MAGIC)
			{
				ZeroObj(*Header);
				Tree->Unlock();
				return 0;
			}

			// Load in the children

			// Allocate directory
			uint32 Pos = Header->DirLoc;
			LgiAssert(Header->DirAlloc <= 40000); // this it arbitary, increase if needed

			Dir = new StorageItemHeader[Header->DirAlloc];
			if (Dir)
			{
				// Seek to directory
				bool GotLoc = Tree->File->SetPos(Header->DirLoc);
				LgiAssert(GotLoc);
				if (GotLoc)
				{
					// Read directory in one big hit... the speed oh the SPEEEED!
					if (Header->DirCount > Header->DirAlloc)
					{
						// This should not be???
						// Oh well, fix it up here by searching through the items
						// on disk looking for the last valid looking entry and call
						// that our 'length'.
						StorageItemHeader Test;
						for (Header->DirCount = 0; true; Header->DirCount++)
						{
							if (Tree->File->Read(&Test, sizeof(Test)) == sizeof(Test))
							{
								Test.SwapBytes();
								if (Test.Magic != STORAGE2_ITEM_MAGIC)
								{
									break;
								}
							}
							else break;
						}

						// We don't know the _actual_ alloc, but making it the same as the
						// length is probably the safest thing to do.
						Header->DirAlloc = Header->DirCount;

						// Reset the file point after ourselves
						Tree->File->SetPos(Header->DirLoc);
					}

					int DirLength = Header->DirCount * sizeof(StorageItemHeader);
					bool GotLength = Tree->File->Read(Dir, DirLength) == DirLength;
					LgiAssert(GotLength);
					if (GotLength)
					{
						// Now allocate a list of children items
						StorageItemImpl *Last = 0;
						bool ChildHeaderDirty = false; // default setting for Item->HeaderDirty
						for (int i=0; i<Header->DirCount; i++)
						{
							// Swap bytes
							Dir[i].SwapBytes();

							// Check the pos is not one of our parent items...
							// Because that would be bad... veeerry bad...
							bool RecursiveLoop = false;
							for (StorageItemImpl *p = this; p; p = p->Parent)
							{
								if (p->StoreLoc == Pos)
								{
									RecursiveLoop = true;
									break;
								}
							}

							if (RecursiveLoop)
							{
								// This node is really one of our parent nodes.
								// Which is obviously invalid, swap it with the last one
								// and reduce the size of the directory
								if (i < Header->DirCount - 1)
								{
									Dir[i] = Dir[Header->DirCount - 1];
								}
								Header->DirCount--;
								ChildHeaderDirty = HeaderDirty = true;
								i--;
							}
							else
							{
								StorageItemImpl *Item = new StorageItemImpl(Dir+i);
								if (Item)
								{
									// Set the child item
									Item->StoreLoc = Pos;
									Item->Parent = this;
									Item->Tree = Tree;
									Item->HeaderDirty = ChildHeaderDirty;
									if (i == 0)
									{
										Child = Item;
									}
									else
									{
										LgiAssert(Last);
										Last->Next = Item;
										Item->Prev = Last;
									}

									Last = Item;
								}
								else break; // mem alloc error
							}

							Pos += sizeof(StorageItemHeader); // 32 bytes
						}
					}
				}
			}

			CleanAll();

			Tree->Unlock();
		}
	}

	return Child;
}

// Always call in a pair with DirChange() to write the changes and unlock the Kit
bool StorageItemImpl::IncDirSize()
{
	// load any children if not already
	GetChild();

	if (Tree->IsOk() &&
		Header)
	{
		if (Tree->Lock(_FL)) // matching Unlock in DirChange
		{
			// add child in
			bool ReallocateDir = !Dir;
			if (Header->DirCount >= Header->DirAlloc)
			{
				// dir needs to grow
				Header->DirLoc = Tree->File->GetSize();
				Header->DirAlloc += STORE_DIR_ALLOC;

				ReallocateDir = true;
			}

			if (ReallocateDir)
			{
				StorageItemHeader *NewDir = new StorageItemHeader[Header->DirAlloc];
				if (NewDir)
				{
					// New dir should be initialized to zero by the constructor

					if (Dir)
					{
						// copy over the contents of the old dir
						memcpy(NewDir, Dir, sizeof(StorageItemHeader) * Header->DirCount);
					}

					// set pointer to new dir
					DeleteArray(Dir);
					Dir = NewDir;
				}
			}
			else
			{
				// zero out the "new" header
				ZeroObj(Dir[Header->DirCount]);
				Dir[Header->DirCount].Magic = STORAGE2_ITEM_MAGIC;
			}

			// Unlock is in DirChange(...)
			return Dir != 0;
		}
	}

	return false;
}

bool StorageItemImpl::DirChange()
{
	bool Status = false;

	if (Tree->IsOk() &&
		!Tree->ReadOnly &&
		Header)
	{
		if (Dir)
		{
			// tell any children the address of their header and loc
			uint32 n = 0;
			for (StorageItemImpl *i = Child; i; i = i->Next)
			{
				i->HeaderDirty = false;
				i->Header = Dir + n;
				i->Header->ParentLoc = StoreLoc;
				i->StoreLoc = Header->DirLoc + (n * sizeof(StorageItemHeader));

				n++;
			}

			// push header out to disk
			bool GotPos = Tree->File->SetPos(Header->DirLoc);
			LgiAssert(GotPos);
			if (GotPos)
			{
				int DirLength = Header->DirAlloc * sizeof(StorageItemHeader);
				#ifdef __BIG_ENDIAN__
				int i;
				for (i=0; i<Header->DirAlloc; i++)
					Dir[i].SwapBytes();
				#endif
				int w = Tree->File->Write(Dir, DirLength);
				#ifdef __BIG_ENDIAN__
				for (i=0; i<Header->DirAlloc; i++)
					Dir[i].SwapBytes();
				#endif
				LgiAssert(w == DirLength);
				if (w == DirLength)
				{
					// success
					Status = true;
				}
			}
		}

		// write header out
		HeaderDirty = true;
		Status = SerializeHeader(*Tree->File, true);

		Tree->Unlock();
	}

	return Status;
}

StorageItem *StorageItemImpl::CreateNext(StorageObj *Obj)
{
	if (Parent)
	{
		Parent->CreateSub(Obj);
	}
	return Next;
}

StorageItem *StorageItemImpl::CreateChild(StorageObj *Obj)
{
	if (Tree->IsOk() &&
		Obj)
	{
		// insert child into directory
		if (IncDirSize())
		{
			if (Header->DirCount > 0)
			{
				// move existing child headers along 1 position
				memmove(Dir+1, Dir, sizeof(StorageItemHeader) * Header->DirCount);

				// clear 1st entry
				memset(Dir, 0, sizeof(StorageItemHeader));
			}

			// add new child at the start
			Child = new StorageItemImpl(Dir);
			LgiAssert(Child);
			if (!Child)
			{
				// The unlock would normal by performed by DirChange
				Tree->Unlock();
				return 0;
			}

			Child->Tree = Tree;
			Child->Parent = this;
			Child->Object = Obj;
			Tree->StorageObj_SetDirty(Child->Object, true);
			Child->SetType(Obj->Type());

			Header->DirCount++;

			// apply changes
			DirChange();
			Child->SetDirty(false);
		}
	}
	return Child;
}

StorageItem *StorageItemImpl::CreateSub(StorageObj *Obj)
{
	StorageItemImpl *New = 0;
	if (Tree->IsOk() &&
		!Tree->ReadOnly &&
		Obj)
	{
		// insert child into directory
		if (IncDirSize())
		{
			// add new child
			New = new StorageItemImpl(Dir + Header->DirCount);
			LgiAssert(New);
			if (!New)
			{
				Tree->Unlock();
				return 0;
			}

			New->Tree = Tree;
			New->Parent = this;
			New->Object = Obj;
			Tree->StorageObj_SetDirty(New->Object, true);
			New->SetType(Obj->Type());

			StorageItemImpl *End = Child;
			while (End && End->Next) End = End->Next;
			if (End)
			{
				End->Next = New;
				New->Prev = End;
			}
			else
			{
				Child = New;
				New->Parent = this;
			}

			Header->DirCount++;

			// apply changes
			DirChange();
			
			if (!New->SetDirty(false))
			{
				// back out the new object... it didn't save to disk
				Tree->SeparateItem(New);
				DeleteObj(New);
			}
		}
	}

	return New;
}

bool StorageItemImpl::InsertSub(StorageItem *ObjVirtual, int At)
{
	StorageItemImpl *Obj = (StorageItemImpl*) ObjVirtual;

	if (Obj &&
		Obj->Temp &&	 // needs Temp pointer for type info
		Header &&
		Tree)
	{
		// insert child into directory
		if (IncDirSize())
		{
			// add new child
			Obj->Parent = this;
			Obj->Tree = Tree;
			Obj->Next = Obj->Prev = 0;

			if (At == 0 OR !Child)
			{
				// Insert at the start
				if (Child)
				{
					Child->Prev = Obj;
				}
				Obj->Next = Child;
				Child = Obj;
				At = 0;
			}
			else
			{
				// Insert at the middle or end

				// Find the object before
				int n=0;
				StorageItemImpl *End = Child;
				while (End)
				{
					if (n == At)
					{
						// This is the one
						break;
					}
					
					if (End->Next)
					{
						// Goto the next node in the list
						n++;
						End = End->Next;
					}
					else
					{
						// No more nodes, so just append
						break;
					}
				}

				if (End)
				{
					if (n == At)
					{
						// Insert before the node 'End'
						Obj->Next = End;
						Obj->Prev = End->Prev;
						if (Obj->Next)
						{
							Obj->Next->Prev = Obj;
						}
						if (Obj->Prev)
						{
							Obj->Prev->Next = Obj;
						}
					}
					else
					{
						// Insert at the end of the list
						End->Next = Obj;
						Obj->Prev = End;
						At = n + 1;
					}
				}
				else
				{
					// Broken object list
					LgiAssert(0);
					Tree->Unlock();
					return false;
				}
			}

			// Move and headers along
			if (At < Header->DirCount)
			{
				// Move all the object after us along one
				memmove( Dir + At + 1, Dir + At, (Header->DirCount - At) * sizeof(*Dir) );
			}

			// Add our header
			Dir[At] = *Obj->Temp;

			// Inc the count
			Header->DirCount++;

			// apply changes
			if (DirChange())
			{
				// copy our old header details in
				//*Obj->Header = *Obj->Temp;
				Obj->Header->ParentLoc = StoreLoc;
				Obj->HeaderDirty = true;
				DeleteObj(Obj->Temp);

				// write changes
				bool Status = Obj->SerializeHeader(*Tree->File, true);
				if (!Status)
				{
					printf("%s:%i - Obj->SerializeHeader failed.\n", _FL);
				}
				return Status;
			}
			else
			{
				printf("%s:%i - DirChange() failed.\n", _FL);
			}
		}
		else
		{
			printf("%s:%i - IncDirSize() failed.\n", _FL);
		}
	}

	return false;
}

bool StorageItemImpl::DeleteAllChildren()
{
	bool Status = false;

	if (Tree->IsOk() &&
		Header)
	{
		if (Tree->Lock(_FL))
		{
			// clear contents of dir on disk
			if (Dir)
			{
				int DirLength = Header->DirAlloc * sizeof(StorageItemHeader);

				// zero out all the Headers..
				memset(Dir, 0, DirLength);

				// write it
				int64 NewLoc = Tree->File->SetPos(Header->DirLoc);
				LgiAssert(NewLoc == Header->DirLoc);
				if (NewLoc == Header->DirLoc)
				{
					#ifdef __BIG_ENDIAN__
					int i;
					for (i=0; i<Header->DirAlloc; i++)
						Dir[i].SwapBytes();
					#endif
					bool WroteDir = Tree->File->Write(Dir, DirLength) == DirLength;
					#ifdef __BIG_ENDIAN__
					for (i=0; i<Header->DirAlloc; i++)
						Dir[i].SwapBytes();
					#endif
					LgiAssert(WroteDir);
				}
				else
				{
					Tree->Unlock();
					goto Exit_DeleteAllChildren;
				}

				// this way a repair can't find the dir and do anything with it.
			}

			// clear grandchildren
			for (StorageItemImpl *Item = Child; Item; Item = Item->Next)
			{
				Item->DeleteAllChildren();
			}

			// clear children
			DeleteObj(Child);
			DeleteArray(Dir);

			// clear dir table (space won't be reclaimed until a compact)
			Header->DirCount = 0;
			Header->DirAlloc = 0;
			Header->DirLoc = 0;

			// apply changes
			DirChange(); // unlocks the Kit

			Child = 0;
		}

	}

Exit_DeleteAllChildren:
	return Status;
}

bool StorageItemImpl::DeleteChild(StorageItem *ObjVirtual)
{
	StorageItemImpl *Obj = (StorageItemImpl*) ObjVirtual;
	bool Status = false;

	if (Obj &&
		Obj->Parent == this &&
		Tree &&
		Tree->Lock(_FL))
	{
		bool HeaderPtrOk = Obj->Header && (Obj->Header >= Dir && Obj->Header < Dir + Header->DirCount);
		LgiAssert(HeaderPtrOk);
		if (HeaderPtrOk)
		{
			int Index = Obj->Header - Dir;

			if (!Obj->Temp)
			{
				Obj->Temp = new StorageItemHeader;
			}
			if (Obj->Temp)
			{
				// save our header out... cause we're going to
				// lose it otherwise;
				*Obj->Temp = *Obj->Header;
			}

			// remove child from list
			if (Child == Obj) Child = Obj->Next;
			if (Obj->Next) Obj->Next->Prev = Obj->Prev;
			if (Obj->Prev) Obj->Prev->Next = Obj->Next;
			Obj->Header = 0;
			Obj->Parent = 0;
			Obj->Next = 0;
			Obj->Prev = 0;

			// remove memory allocated for it's header
			Header->DirCount--;
			if (Header->DirCount > Index)
			{
				memmove(Dir + Index, Dir + (Index + 1), sizeof(StorageItemHeader) * (Header->DirCount - Index));
			}

			// apply changes
			Status = DirChange();
		}
	}

	return Status;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
namespace Storage2
{
	class Block : public GSegment
	{
	public:
		StorageItemImpl *Dir;
		StorageItemImpl *Object;
	};

	class StorageKitImplPrivate
	{
	public:
		StorageKitImpl *Kit;
		
		// Compact stuff
		uint UserData;
		StorageValidator *Validator;
		Progress *_Ui;
		DoEvery _Timer;
		int _CompactPos;
		Block **Blocks;
		
		StorageKitImplPrivate(StorageKitImpl *kit)
		{
			Kit = kit;
			Validator = 0;
			UserData = 0;
			_Ui = 0;
			Blocks = 0;
		}

		~StorageKitImplPrivate()
		{
			DeleteArray(Blocks);
		}

		const char *DescribeType(int Type)
		{
			#define MAGIC_BASE					0xAAFF0000
			#define MAGIC_MAX					13	// One past the end

			// Real names
			const char *Storage2TypeName[] = {
				"BASE",
				"MAIL",
				"CONTACT",
				"FOLDER",
				"MAILBOX",
				"ATTACHMENT",
				"ANY",
				"FILTER",
				"FOLDER_2",
				"CONDITION",
				"ACTION",
				"CALENDAR",
				"ATTENDEE",
				0};

			int i = Type - MAGIC_BASE;
			if (i >= 0 && i<MAGIC_MAX)
			{
				return Storage2TypeName[i];
			}

			// Fall back
			static char Buf[64];
			sprintf(Buf, "0x%X", Type);
			return Buf;
		}

		void Discard(Block *Item)
		{
			if (Kit->IsOk() && Item)
			{
				if (Item->Dir)
				{
					// discard the directory
					Item->Dir->Header->DirLoc = 0;
					Item->Dir->Header->DirCount = 0;
					Item->Dir->Header->DirAlloc = 0;
					Item->Dir->HeaderDirty = true;

					Item->Dir->SerializeHeader(*Kit->File, true);
				}
				else if (Item->Object)
				{
					// discard the object data
					Item->Object->Header->DataLoc = 0;
					Item->Object->Header->DataSize = 0;
					Item->Object->HeaderDirty = true;

					Item->Object->SerializeHeader(*Kit->File, true);
				}
			}
		}

		void ProcessConflict(Block *a, Block *b)
		{
			// object overlaps...
			char Item1Desc[256] = "", Item2Desc[256] = "";
			StorageItemImpl *Item1 = a->Dir ? a->Dir : a->Object;
			if (Item1)
			{
				sprintf(Item1Desc,
						"Item 1:\n"
						"\tType: %s\n"
						"\tStart: %u, Len: %u\n"
						"\tData: %u, Len: %u\n",
						DescribeType(Item1->Header->Type),
						Item1->StoreLoc, (unsigned)sizeof(*Item1->Header),
						Item1->Header->DataLoc, Item1->Header->DataSize);
			}						

			StorageItemImpl *Item2 = b->Dir ? b->Dir : b->Object;
			if (Item2)
			{
				sprintf(Item2Desc,
						"Item 2:\n"
						"\tType: %s\n"
						"\tStart: %u, Len: %lu\n"
						"\tData: %u, Len: %u\n",
						DescribeType(Item2->Header->Type),
						Item2->StoreLoc,
						sizeof(*Item2->Header),
						Item2->Header->DataLoc,
						Item2->Header->DataSize);
			}						

			const char *DlgTitle = "Compact Error";
			GLayout *Wnd = dynamic_cast<GLayout*>(_Ui);
			char Msg[512], KeepItem1[32], KeepItem2[32];

			sprintf(KeepItem1, Store2_LgiLoadString(IDC_STORE2_KEEP), 1);
			sprintf(KeepItem2, Store2_LgiLoadString(IDC_STORE2_KEEP), 2);

			/*
			An object overlap has been found. This error is caused\n
			by a bug in the application or storage code or a corruption\n
			caused by a crash.\n
			\n
			The contested area is the bytes from %u to %u (length %u)\n
			\n
			%s\n
			%s\n
			Would you like to:
			*/
			const char *Fmt = Store2_LgiLoadString(IDS_STORE2_ERROR_COMPACT);
			sprintf(Msg,
					Fmt,
					(uint32)b->Start,
					(uint32)(b->Start + b->Length),
					(uint32)b->Length,
					Item1Desc,
					Item2Desc);

			GAlert Dlg(	Wnd,
						DlgTitle,
						Msg,
						KeepItem1,
						KeepItem2,
						Store2_LgiLoadString(IDS_CANCEL));

			int Response = Dlg.DoModal();
			switch (Response)
			{
				case 1: // Keep 1
				{
					Discard(b);
					break;
				}
				case 2: // Keep 2
				{
					Discard(a);
					break;
				}
				case 3: // cancel
				{
					if (_Ui)
					{
						_Ui->Cancel(true);
					}
					break;
				}
			}
		}

		GSegmentTree Segs;
		void AddSegs(StorageItemImpl *s)
		{
			if (s &&
				(!_Ui OR !_Ui->Cancel()))
			{
				for (StorageItemImpl *Item = s; Item; )
				{
					bool Insert = (Validator) ? Validator->CompactValidate(dynamic_cast<GView*>(_Ui), Item) : true;
					if (Insert)
					{
						if (Item->Header->DirLoc)
						{
							Block *New = new Block;
							if (New)
							{
								New->Dir = Item;
								New->Object = 0;
								New->Start = Item->Header->DirLoc;

								if (Item->Header->DirAlloc < 0)
								{
									Item->Header->DirAlloc = 0;
								}
								New->Length = Item->Header->DirAlloc * sizeof(StorageItemHeader);

								GSegment *Conflict = 0;
								if (!Segs.Insert(New, &Conflict))
								{
									ProcessConflict(New, (Block*)Conflict);
								}
							}
						}

						if (Item->Header->DataLoc)
						{
							Block *New = new Block;
							if (New)
							{
								New->Dir = 0;
								New->Object = Item;
								New->Start = Item->Header->DataLoc;
								
								if (Item->Header->DataSize < 0)
								{
									Item->Header->DataSize = 0;
								}
								New->Length = Item->Header->DataSize;

								GSegment *Conflict = 0;
								if (!Segs.Insert(New, &Conflict))
								{
									ProcessConflict(New, (Block*)Conflict);
								}
							}
						}
					}

					AddSegs((StorageItemImpl*)Item->GetChild());

					_CompactPos += Item->GetTotalSize();

					if (_Timer.DoNow())
					{
						_Ui->Value(_CompactPos);
						LgiYield();
						if (_Ui && _Ui->Cancel())
						{
							break;
						}
					}
					if (Validator) Validator->CompactDone(Item);

					StorageItemImpl *Next = (StorageItemImpl*)Item->GetNext();
					if (!Insert)
					{
						Kit->SeparateItem(Item);
					}
					Item = Next;
				}
			}
		}

		#if 0
		void AddItem(StorageItemImpl *s)
		{
			static int Iter = 0;

			if (s &&
				(!_Ui OR !_Ui->Cancel()))
			{
				for (StorageItemImpl *Item = s; Item; )
				{
					bool Insert = (Validator) ? Validator->CompactValidate(dynamic_cast<GView*>(_Ui), Item) : true;
					if (Insert)
					{
						if (Item->Header->DirLoc)
						{
							Block *New = new Block;
							if (New)
							{
								New->Dir = Item;
								New->Object = 0;
								New->Start = Item->Header->DirLoc;
								New->Length = Item->Header->DirAlloc * sizeof(StorageItemHeader);
								SortItem(New);
							}
						}

						if (Item->Header->DataLoc)
						{
							Block *New = new Block;
							if (New)
							{
								New->Dir = 0;
								New->Object = Item;
								New->Start = Item->Header->DataLoc;
								New->Length = Item->Header->DataSize;
								SortItem(New);
							}
						}
					}

					AddItem((StorageItemImpl*)Item->GetChild());
					if (_Timer.DoNow())
					{
						LgiYield();
						if (_Ui && _Ui->Cancel())
						{
							break;
						}
					}
					if (Validator) Validator->CompactDone(Item);

					StorageItemImpl *Next = (StorageItemImpl*)Item->GetNext();
					if (!Insert)
					{
						Kit->SeparateItem(Item);
					}
					Item = Next;
				}
			}
		}

		bool SortItem(Block *Item)
		{
			if (Item)
			{
				int Index = 0;
				for (Block *b = Blocks.First(); b; b = Blocks.Next(), Index++)
				{
					// check the new block doesn't overlap this block
					if (Item->Start >= b->Start + b->Length OR
						Item->Start + Item->Length <= b->Start)
					{
						// object doesn't overlap
						if (Item->Start < b->Start)
						{
							Blocks.Insert(Item, Index);
							Index = -1;
							break;
						}
					}
					else
					{
						// object overlaps...
						char Item1Desc[256] = "", Item2Desc[256] = "";
						StorageItemImpl *Item1 = Item->Dir ? Item->Dir : Item->Object;
						if (Item1)
						{
							sprintf(Item1Desc,
									"Item 1:\n"
									"\tType: %s\n"
									"\tStart: %u, Len: %u\n"
									"\tData: %u, Len: %u\n",
									DescribeType(Item1->Header->Type),
									Item1->StoreLoc, sizeof(*Item1->Header),
									Item1->Header->DataLoc, Item1->Header->DataSize);
						}						

						StorageItemImpl *Item2 = b->Dir ? b->Dir : b->Object;
						if (Item2)
						{
							sprintf(Item2Desc,
									"Item 2:\n"
									"\tType: %s\n"
									"\tStart: %u, Len: %u\n"
									"\tData: %u, Len: %u\n",
									DescribeType(Item2->Header->Type),
									Item2->StoreLoc, sizeof(*Item2->Header),
									Item2->Header->DataLoc, Item2->Header->DataSize);
						}						

						char *DlgTitle = "Compact Error";
						GLayout *Wnd = dynamic_cast<GLayout*>(_Ui);
						char Msg[512], KeepItem1[32], KeepItem2[32];

						sprintf(KeepItem1, Store2_LgiLoadString(IDC_STORE2_KEEP), 1);
						sprintf(KeepItem2, Store2_LgiLoadString(IDC_STORE2_KEEP), 2);

						sprintf(Msg,
								Store2_LgiLoadString(IDS_STORE2_ERROR_COMPACT),
								b->Start,
								b->Start + b->Length,
								b->Length,
								Item1Desc,
								Item2Desc);

						GAlert Dlg(	Wnd,
									DlgTitle,
									Msg,
									KeepItem1,
									KeepItem2,
									Store2_LgiLoadString(IDS_CANCEL));

						int Response = Dlg.DoModal();
						switch (Response)
						{
							case 1: // Keep 1
							{
								Discard(b);
								return true;
							}
							case 2: // Keep 2
							{
								Discard(Item);
								break;
							}
							case 3: // cancel
							{
								if (_Ui)
								{
									_Ui->Cancel(true);
								}
								return false;
							}
						}
					}
				}

				if (Index >= 0)
				{
					Blocks.Insert(Item);
				}

				StorageItemImpl *Impl = Item->Object ? Item->Object : Item->Dir;
				if (_Ui && Impl)
				{
					_CompactPos += Impl->GetTotalSize();
					if (_Timer.DoNow())
					{
						_Ui->Value(_CompactPos);
						LgiYield();
					}
				}
			}

			return true;
		}
		#endif
	};
}

///////////////////////////////////////////////////////////////////////////
StorageKitImpl::StorageKitImpl(char *filename) : GSemaphore("StorageKitImpl2")
{
	d = new StorageKitImplPrivate(this);
	File = new GSubFile(this, false);
	Status = false;
	ReadOnly = false;
	StoreLoc = 0;
	Root = 0;
	Version = 0;

	LgiAssert(sizeof(StorageItemHeader) == 32);

	FileName = filename;
	if (IsOk())
	{
		if (File->Open(FileName, O_READWRITE))
		{
			#if __BIG_ENDIAN__
			File->SetSwap(true);
			#endif
			Status = _Serialize(*File, File->GetSize() == 0);
		}
		else if (File->Open(FileName, O_READ))
		{
			#if __BIG_ENDIAN__
			File->SetSwap(true);
			#endif
			ReadOnly = true;
			Status = _Serialize(*File, File->GetSize() == 0);
		}
	}
}

StorageKitImpl::~StorageKitImpl()
{
	Lock(_FL);

	DeleteObj(Root);
	DeleteObj(File);
	DeleteObj(d);
	Status = false;
}

bool StorageKitImpl::IsOk()
{
	bool Status = (this != 0) && (File != 0);
	LgiAssert(Status);
	return Status;
}

bool StorageKitImpl::GetPassword(GPassword *p)
{
	bool Status = false;

	if (p)
	{
		*p = Password;
		Status = p->IsValid();
	}

	return Status;
}

bool StorageKitImpl::SetPassword(GPassword *p)
{
	bool Status = false;

	if (IsOk())
	{
		if (p)
		{
			// set password
			Password = *p;
		}
		else
		{
			Password.Set(0);
		}

		// write changes
		Status = _Serialize(*File, true);
	}

	return Status;
}

uint64 StorageKitImpl::GetFileSize()
{
	return IsOk() ? File->GetSize() : 0;
}

StorageItem *Storage2::StorageKitImpl::CreateRoot(StorageObj *Obj)
{
	if (IsOk() &&
		Lock(_FL))
	{
		if (Obj)
		{
			if (!Root)
			{
				Root = new StorageItemImpl(&RootHeader);
			}

			if (Root)
			{
				File->SetSize(0);
				_Serialize(*File, true);

				Root->Tree = this;
				Root->HeaderDirty = true;
				Root->StoreLoc = File->GetSize();
				Root->Object = Obj;
				StorageObj_SetDirty(Root->Object, true);
				Root->Header->Type = Obj->Type();

				Root->SerializeHeader(*File, true);	// this forces the header position
													// to 64, and thus be equal to
													// StoreLoc.
				Root->SetDirty(false);
			}
		}

		Unlock();
	}

	return Root;
}

StorageItem *StorageKitImpl::GetRoot()
{
	if (!Root && IsOk())
	{
		if (Lock(_FL))
		{
			Root = new StorageItemImpl(&RootHeader);
			if (Root)
			{
				Root->Tree = this;
				Root->StoreLoc = 64;
				if (!Root->SerializeHeader(*File, false))
				{
					if (File->GetSize() < 64 + 32)
						Root->SerializeHeader(*File, true);
					else
						DeleteObj(Root);
				}
			}

			Unlock();
		}
	}

	return Root;
}

bool StorageKitImpl::SeparateItem(StorageItem *Item)
{
	bool Status = false;

	if (!ReadOnly && Item)
	{
		if (Lock(_FL))
		{
			if (((StorageItemImpl*)Item)->Parent)
			{
				Status = ((StorageItemImpl*)Item)->Parent->DeleteChild(Item);
			}
			else
			{
				Status = true;
			}

			Unlock();
		}
	}

	return Status;
}

bool StorageKitImpl::DeleteItem(StorageItem *Item)
{
	bool Status = false;

	if (!ReadOnly && Item)
	{
		if (Lock(_FL))
		{
			((StorageItemImpl*)Item)->DeleteAllChildren();
			if (((StorageItemImpl*)Item)->Parent)
			{
				Status = ((StorageItemImpl*)Item)->Parent->DeleteChild(Item);
			}
			else
			{
				Status = true;
			}

			Unlock();
		}
	}

	return Status;
}

bool StorageKitImpl::_ValidLoc(uint64 Loc)
{
	if (File)
	{
		if (Loc >= 64 && Loc <= File->GetSize())
		{
			return true;
		}
		
		#ifdef LINUX
		printf("%s:%i - _ValidLoc failed: Loc=%Lx FileSize=%Lx\n",
			_FL,
			Loc, File->GetSize());
		#endif
	}
	
	return false;
}

bool StorageKitImpl::Compact(Progress *p, bool Interactive, StorageValidator *validator)
{
	bool Status = false;

	d->Validator = validator;
	d->_Ui = p;
	d->_CompactPos = 0;
	if (Root &&
		IsOk() &&
		Lock(_FL))
	{
		DeleteArray(d->Blocks);
		
		// setup ui
		Progress *RemoveSpace = 0;
		if (p)
		{
			GProgressPane *Pane = dynamic_cast<GProgressPane*>(p);
			GProgressDlg *Dlg = Pane ? dynamic_cast<GProgressDlg*>(Pane->GetParent()) : 0;
			RemoveSpace = Dlg ? Dlg->Push() : p;

			p->SetDescription("Building item list...");
			p->SetLimits(0, GetFileSize());
			p->SetScale(1.0 / 1024.0);
			p->SetType("K");
			if (RemoveSpace)
			{
				RemoveSpace->SetDescription("Removing space between objects...");
				RemoveSpace->SetLimits(0, GetFileSize());
				RemoveSpace->SetScale(1.0 / 1024.0);
				RemoveSpace->SetType("K");
			}

			d->_Timer.Init(300);

			LgiYield();
		}

		#if 1
		// Test code
		d->AddSegs(Root);
		d->_CompactPos = 0;
		#endif

		#if 0
		// Search through the folders for data blocks
		d->AddItem(Root);

		// Compare the 2 systems
		if (d->Segs.Items() == d->Blocks.GetItems())
		{
			int n = 0;
			for (Block *b = d->Blocks.First(); b; b = d->Blocks.Next())
			{
				Block *a = Blocks[n++];
				if (a->Start != b->Start OR
					a->Length != b->Length)
				{
					int asd=0;
				}
			}
		}
		else
		{
			LgiAssert(0);
		}
		#endif

		// Add a block for the root header (which has no direct item,
		// the 'item' being the Tree itself)
		Block *b = new Block;
		if (b)
		{
			b->Dir = 0;
			b->Object = 0;
			b->Start = 64;
			b->Length = sizeof(StorageItemHeader); // 1 header only
			d->Segs.Insert(b);
		}

		d->Blocks = (Block**)d->Segs.CreateIndex();
	
		char Msg[256] = "Compact cancelled.";
		if (!p OR !p->Cancel())
		{
			// remove space between the objects
			int OldSize = File->GetSize();
			Block *Next = 0;
			int BufLen = 1 << 20;
			char *Buf = new char[BufLen];
			if (!Buf)
			{
				// memory allocation error
				sprintf(Msg, "Failed to allocate memory: %u bytes.", BufLen);
			}
			else
			{
				int Items = d->Segs.Items();
				for (int Index=0; Index<Items; Index++)
				{
					b = d->Blocks[Index];
					Next = Index < Items-1 ? d->Blocks[Index+1] : 0;

					if (Next)
					{
						// check for space between this object and the next
						if (b->Start + b->Length < Next->Start)
						{
							// some space exists between this object and the next,
							// lets remove it eh?
							uint32 NewPos = b->Start + b->Length;
							
							// Move all the data in the object in 1mb (or less) chunks.
							uint32 i; // this holds the number of bytes moved
							for (i=0; i<Next->Length; )
							{
								uint32 Chunk = min(BufLen, Next->Length - i);
								
								File->SetPos((uint64)Next->Start + i);
								if (File->Read(Buf, Chunk) == Chunk)
								{
									// read ok
									uint32 At = NewPos + i;
									File->SetPos(At);
									if (File->Write(Buf, Chunk) == Chunk)
									{
										// write ok
										i += Chunk;
									}
									else
									{
										// write failed
										sprintf(Msg, "Failed to write to file: %u bytes at position %u.", Chunk, At);
										break;
									}
								}
								else
								{
									// read failed
									sprintf(Msg, "Failed to read from file: %u bytes at position "LGI_PrintfInt64".", Chunk, Next->Start);
									break;
								}
							}
							
							if (i == Next->Length)
							{
								// now update all the relevent variables
								if (Next->Object)
								{
									// object has moved... update header
									Next->Object->Header->DataLoc = NewPos;
									Next->Object->HeaderDirty = true;
									Next->Object->SerializeHeader(*File, true);
								}
								else if (Next->Dir)
								{
									// directory has moved...
									Next->Dir->Header->DirLoc = NewPos;
									Next->Dir->HeaderDirty = true;
									if (Lock(_FL))
									{
										Next->Dir->DirChange();
									}
								}
								else
								{
									// the root object's directory never moves
									// from loc: 64 so this should never occur
									LgiAssert(0);
								}

								// update the block to reflect the changes we just made
								Next->Start = NewPos;
							}
							else
							{
								// On any sort of error we just stop. It's safer and less likely
								// to leave the file in a corrupt state.
								break;
							}
						}
					}
					else
					{
						// end of file... truncate after this object
						File->SetSize(b->Start + b->Length);
						Status = true;
						
						char Size[64];
						int Change = OldSize - File->GetSize();
						LgiFormatSize(Size, Change);
						sprintf(Msg, "Compact complete, %s was recovered.", Size);
					}

					if (RemoveSpace &&
						d->_Timer.DoNow())
					{
						RemoveSpace->Value(b->Start);
						LgiYield();

						if (p && p->Cancel())
						{
							break;
						}
					}
				}
			}
		}
		
		// tell the user what happened
		if (Interactive)
		{
			LgiMsg(dynamic_cast<GView*>(d->_Ui), Msg, Status?(char*)"Compact Successful":(char*)"Compact Error", MB_OK);
		}

		// clean up
		if (p && p->Cancel())
		{
			Status = true;
		}
		d->_Ui = 0;
		DeleteArray(d->Blocks);
		d->Segs.Empty();

		Unlock();
	}

	return Status;
}

/*
	struct StorageHeader {

		int Magic;				// always STORAGE2_MAGIC
		int Version;			// 0x0
		int Reserved1;			// 0x0
		int Reserved2;			// 0x0
		char Password[32];		// obsured password
		int Reserved3[4];		// 0x0
	};
*/
bool StorageKitImpl::_Serialize(GFile &f, bool Write)
{
	bool Status = false;
	StorageHeader Header;

	if (IsOk() && Lock(_FL))
	{
		if (Write)
		{
			ZeroObj(Header);
			Header.Magic = STORAGE2_MAGIC;
			Header.Version = Version;
			Password.Serialize(Header.Password, Write);

			File->SetPos(0);

			#ifdef __BIG_ENDIAN__
			f.SetStatus();
			f << Header.Magic;
			f << Header.Version;
			f << Header.Reserved1;
			f << Header.Reserved2;
			f.Write(Header.Password, sizeof(Header.Password));
			f.Write(Header.Reserved3, sizeof(Header.Reserved3));
			Status = f.GetStatus();
			#else
			Status = File->Write(&Header, sizeof(Header)) == sizeof(Header);
			#endif
		}
		else
		{
			f.SetPos(0);

			#ifdef __BIG_ENDIAN__
			f.SetStatus();
			f >> Header.Magic;
			f >> Header.Version;
			f >> Header.Reserved1;
			f >> Header.Reserved2;
			f.Read(Header.Password, sizeof(Header.Password));
			f.Read(Header.Reserved3, sizeof(Header.Reserved3));
			Status = f.GetStatus();
			#else
			Status = f.Read(&Header, sizeof(Header)) == sizeof(Header);
			#endif
			if (Status && f.GetPos() == sizeof(Header))
			{
				Status = (Header.Magic == STORAGE2_MAGIC);
				if (Status)
				{
					Version = Header.Version;
					Password.Serialize(Header.Password, Write);
				}
			}
		}

		Unlock();
	}

	return Status;
}

bool StorageKitImpl::AttachItem(StorageItem *ItemVirtual, StorageItem *ToVirtual, NodeRelation Relationship)
{
	bool Status = false;
	StorageItemImpl *Item = (StorageItemImpl*)ItemVirtual;
	StorageItemImpl *To = (StorageItemImpl*)ToVirtual;

	if (Item &&
		To)
	{
		if (Item == To)
		{
			// You can't attach to yourself...!?!
			LgiAssert(0);
		}
		else if (Lock(_FL))
		{
			if (Relationship == NodeNext)
			{
				if (!Item->Parent OR
					SeparateItem(Item))
				{
					int At = 0;
					for (StorageItemImpl *i=To; i; i=i->Prev)
					{
						At++;
					}
					Status = To->Parent->InsertSub(Item, At + 1);
					if (!Status)
					{
						printf("%s:%i - InsertSub failed.\n", _FL);
					}
				}
				else
				{
					printf("%s:%i - SeparateItem failed.\n", _FL);
				}
			}
			else if (Relationship == NodePrev)
			{
				if (!Item->Parent OR
					SeparateItem(Item))
				{
					int At = 0;
					for (StorageItemImpl *i=To; i; i=i->Prev)
					{
						At++;
					}
					Status = To->Parent->InsertSub(Item, At);
					if (!Status)
					{
						printf("%s:%i - InsertSub failed.\n", _FL);
					}
				}
				else
				{
					printf("%s:%i - SeparateItem failed.\n", _FL);
				}
			}
			else if (Relationship == NodeChild)
			{
				if (SeparateItem(Item))
				{
					Status = To->InsertSub(Item);
					if (!Status)
					{
						printf("%s:%i - InsertSub failed.\n", _FL);
					}
				}
				else
				{
					printf("%s:%i - SeparateItem failed.\n", _FL);
				}
			}

			Unlock();
		}
	}

	return Status;
}

GSemaphore *StorageKitImpl::GetLock()
{
	return this;
}

GSubFilePtr *StorageKitImpl::CreateFilePtr(const char *file, int line)
{
	return File ? File->Create(file, line) : 0;
}
