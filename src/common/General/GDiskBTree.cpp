#include <stdio.h>

#include "Lgi.h"
#include "GDiskBTree.h"

#define NODE_SHIFT			10
#define NODE_SIZE			(1 << NODE_SHIFT)		// 1kb
#define NODE_MAGIC			'ednB'					// 'bnde'
#define TREE_MAGIC			'ertB'					// 'btre'
class GDiskBTreePrivate;

// The disk format of the node
struct BNodeFmt
{
	int32 Magic;			// always NODE_MAGIC
	int32 LeftBlock;		// Block ID of the left node, left shift NODE_SHIFT bits to get offset
	int32 RightBlock;		// Block ID of the right node
	uint16 Items;			// number of items in this block
	uint16 OffsetToLast;	// byte offset from the start of data to the last element

	// On disk the data follows this up to NODE_SIZE bytes in total
};

// The format of node at offset '0'
struct BTreeFmt
{
	int32 Magic;			// always TREE_MAGIC
	int32 DataSize;			// fixed size of data stored in each record
};

class BData
{
public:
	uint8 D[1];

	char *NewStr()
	{
		return ::NewStr((char*) D + 1, D[0]);
	}

	int NameLen()
	{
		return D[0];
	}

	void *Data()
	{
		return D + (D[0] + 1);
	}

	int Sizeof(int DataSize)
	{
		return 1 + D[0] + DataSize;
	}

	BData *Next(int DataSize)
	{
		return (BData*) ((char*)Data() + DataSize);
	}

	int Cmp(char *Key)
	{
		LgiAssert(D[0] != 0);

		uint8 *Data = D + (D[0] + 1);
		uint8 Temp = *Data;
		*Data = 0;
		
		int c = strncmp(Key, (char*)D+1, D[0]);
		
		*Data = Temp;

		return c;
	}

	void Set(char *Key, void *Data, int Len)
	{
		D[0] = strlen(Key);
		memcpy(D + 1, Key, D[0]);
		memcpy(D + 1 + D[0], Data, Len);
	}
};

class BNode
{
public:
	// In memory objects for left and right child nodes. These are NULL until
	// loaded
	BNode *Parent, *Left, *Right;
	// Absolute position in the file.
	int64 Offset;
	// The on disk header
	BNodeFmt H;
	// The on disk data (may be NULL until read in)
	uint8 *D;
	// The owner
	GDiskBTreePrivate *Tree;

	BNode(GDiskBTreePrivate *t);
	~BNode();

	bool Read(int64 Offset, bool Create = false);
	bool Write();
	uint8 *GetData();
	BData *GetFirst();
	BData *GetLast();
	BNode *FindData(char *Key, BData **Ptr = 0);

	void *HasItem(char *Key);
	bool Insert(char *Key, void *Data);
};

class GDiskBTreePrivate
{
public:
	GFile F;
	int DataSize;
	int64 Size;
	BNode *Root;
	GArray<uint8> Bmp;

	GDiskBTreePrivate()
	{
		DataSize = 0;
		Size = 0;
		Root = 0;
	}

	~GDiskBTreePrivate()
	{
		DeleteObj(Root);
	}

	bool GetBit(int i)
	{
		int Mask = 1 << (i & 7);
		return (Bmp[i>>3] & Mask) ? true : false;
	}

	void SetBit(int i, bool b)
	{
		int Mask = 1 << (i & 7);
		if (b)
			Bmp[i>>3] = Bmp[i>>3] | Mask;
		else
			Bmp[i>>3] = Bmp[i>>3] & ~Mask;
			
	}

	BNode *NewNode()
	{
		// Check for unused blocks in the middle of the file
		int Blocks = F.GetSize() >> NODE_SHIFT;
		for (int i=0; i<Blocks; i++)
		{
			if (NOT GetBit(i))
			{
				BNode *New = new BNode(this);
				if (New)
				{
					New->Offset = i << NODE_SHIFT;
					SetBit(i, true);
					return New;
				}
			}
		}

		// Allocate at the end of the file
		BNode *New = new BNode(this);
		if (New)
		{
			New->Offset = F.GetSize();
			SetBit(New->Offset >> NODE_SHIFT, true);
			return New;
		}

		return 0;
	}
};

//////////////////////////////////////////////////////////////////////////////////////
BNode::BNode(GDiskBTreePrivate *t)
{
	Tree = t;
	Left = 0;
	Right = 0;
	Parent = 0;
	D = 0;
}

BNode::~BNode()
{
	DeleteArray(D);
}

BData *BNode::GetFirst()
{
	if (H.Items)
		return (BData*) GetData();
	return 0;
}

BData *BNode::GetLast()
{
	if (H.Items)
		return (BData*) (GetData() + H.OffsetToLast);
	return 0;
}

bool BNode::Read(int64 Off, bool Create)
{
	bool Status = false;

	if (Tree->F.SetPos(Off) == Off)
	{
		bool Ok = Tree->F.Read(&H, sizeof(H)) == sizeof(H);
		if (NOT Ok AND Create)
		{
			H.Magic = NODE_MAGIC;
			H.LeftBlock = 0;
			H.RightBlock = 0;
			H.Items = 0;
			H.OffsetToLast = 0;
			if (Tree->F.SetPos(Off) == Off)
			{
				Ok = Tree->F.Write(&H, sizeof(H)) == sizeof(H);
			}
		}
		if (Ok)
		{
			if (H.Magic == NODE_MAGIC)
			{
				Offset = Off;
				Tree->SetBit(Offset >> NODE_SHIFT, true);

				Status = true;
				if (H.LeftBlock)
				{
					Left = new BNode(Tree);
					if (Left)
					{
						Left->Parent = this;
						Status &= Left->Read(H.LeftBlock << NODE_SHIFT);
					}
				}
				if (H.RightBlock)
				{
					Right = new BNode(Tree);
					if (Right)
					{
						Right->Parent = this;
						Status &= Right->Read(H.RightBlock << NODE_SHIFT);
					}
				}
			}
			else
			{
				printf("%s:%i - Node magic number check failed.\n", __FILE__, __LINE__);
			}
		}
		else
		{
			printf("%s:%i - Failed to read from file.\n", __FILE__, __LINE__);
		}
	}
	else
	{
		printf("%s:%i - Failed to seek to node.\n", __FILE__, __LINE__);
	}

	return Status;
}

bool BNode::Write()
{
	bool Status = false;

	if (Tree->F.SetPos(Offset) == Offset)
	{
		Status = Tree->F.Write(&H, sizeof(H)) == sizeof(H);
		if (Status AND D)
		{
			Status = Tree->F.Write(D, NODE_SIZE - sizeof(H));
		}
	}

	return Status;
}

uint8 *BNode::GetData()
{
	if (NOT D)
	{
		int64 Off = Offset + sizeof(H);
		if (Tree->F.SetPos(Off) == Off)
		{
			int Size = NODE_SIZE - sizeof(H);
			D = new uint8[Size];
			if (D)
			{
				if (Tree->F.Read(D, Size) != Size)
				{
					memset(D, 0, Size);
				}
			}
		}
	}

	return D;
}

BNode *BNode::FindData(char *Key, BData **Ptr)
{
	BNode *Status = 0;
	
	uint8 *Data = GetData();
	if (Data)
	{
		if (H.Items <= 0)
		{
			Status = this;
		}
		else
		{
			BData *First = GetFirst();
			BData *Last = GetLast();
			int Fcmp = First->Cmp(Key);
			if (Fcmp == 0)
			{
				// Found the first node
				Status = this;
				if (Ptr) *Ptr = First;
			}
			else if (Fcmp < 0)
			{
				// Look in left sub-tree
				if (Left)
				{
					Status = Left->FindData(Key, Ptr);
				}
				else
				{
					LgiAssert(0);
				}
			}
			else
			{
				int Lcmp = Last > First ? Last->Cmp(Key) : 1;
				if (Lcmp == 0)
				{
					// Found the last node
					Status = this;
					if (Ptr) *Ptr = Last;
				}
				else if (Lcmp > 0 AND Right)
				{
					// Look in the right sub-tree
					if (Right)
					{
						Status = Right->FindData(Key, Ptr);
					}
					else
					{
						LgiAssert(0);
					}
				}
				else
				{
					// It's in our node... find it.
					Status = this;
					if (Ptr)
					{
						BData *Cur = First->Next(Tree->DataSize);
						for (int i=1; Cur AND i<H.Items; i++)
						{
							if (Cur->Cmp(Key) == 0)
							{
								*Ptr = Cur;
								break;
							}

							Cur = Cur->Next(Tree->DataSize);
						}
					}
				}
			}
		}
	}

	return Status;
}

void *BNode::HasItem(char *Key)
{
	BData *Ptr = 0;
	if (FindData(Key, &Ptr) AND Ptr)
	{
		return Ptr->Data();
	}
	return 0;
}

bool BNode::Insert(char *Key, void *Data)
{
	BNode *Node = 0;
	BData *Ptr = 0;
	if (Node = FindData(Key, &Ptr))
	{
		if (Ptr)
		{
			// Key already exists, just change the data
			memcpy(Ptr->Data(), Data, Tree->DataSize);
			return true;
		}
		else if (Node->GetData())
		{
			// Create the key.

			// Check for freespace
			BData *Last = Node->GetLast();
			int KeyLen = strlen(Key);
			int Used = Last ? Node->H.OffsetToLast + Last->Sizeof(Tree->DataSize) : 0;
			int Available = NODE_SIZE - sizeof(Node->H);
			int Need = 1 + KeyLen + Tree->DataSize;
			if (Available - Used >= Need)
			{
				// It'll fit in the current key...
				BData *Cur = Node->GetFirst();
				for (int i=0; i<H.Items AND Cur; i++)
				{
					if (Cur->Cmp(Key) < 0)
					{
						break;
					}

					Cur = Cur->Next(Tree->DataSize);
				}

				// Insert before current key
				int Offset = Cur ? (int)Cur - (int)Node->GetData() : 0;
				memcpy(Node->GetData() + Offset + Need, Node->GetData() + Offset,  Used - Offset);
				BData *New = Cur ? Cur : (BData*) Node->GetData();
				New->Set(Key, Data, Tree->DataSize);
				if (Cur)
					H.OffsetToLast += Need;
				H.Items++;

				// Save the changes
				Write();
			}
			else
			{
				// Need to split the node to make more space.
				BNode *New = Tree->NewNode();
				if (New)
				{
				}
			}
		}
	}
	
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////
GDiskBTree::GDiskBTree(char *File, int DataSize)
{
	d = new GDiskBTreePrivate;
	d->DataSize = DataSize;
	if (File) Open(File);
}

GDiskBTree::~GDiskBTree()
{
	Close();
	DeleteObj(d);
}

int64 GDiskBTree::GetSize()
{
	return d->Size;
}

bool GDiskBTree::Open(char *File)
{
	bool Status = false;

	Close();

	if (d->F.Open(File, O_READWRITE))
	{
		bool Create = false;
		d->Bmp.Length(0);
		d->SetBit(0, true);
		d->SetBit(1, true);

		BTreeFmt H;
		if (d->F.Read(&H, sizeof(H)) == sizeof(H))
		{
			d->DataSize = H.DataSize;
		}
		else
		{
			H.Magic = TREE_MAGIC;
			H.DataSize = d->DataSize;
			d->F.Write(&H, sizeof(H));
			Create = true;
		}

		if (H.Magic == TREE_MAGIC)
		{
			d->Root = new BNode(d);
			if (d->Root)
			{
				Status = d->Root->Read(NODE_SIZE, Create);
			}
		}
	}

	return Status;
}

bool GDiskBTree::Close()
{
	bool Status = false;

	if (d->F.IsOpen())
		Status = d->F.Close();

	DeleteObj(d->Root);

	return Status;
}

bool GDiskBTree::Insert(char *Key, void *Data)
{
	return d->Root ? d->Root->Insert(Key, Data) : 0;
}

bool GDiskBTree::Delete(char *Key)
{
	return false;
}

void *GDiskBTree::HasItem(char *Key)
{
	return d->Root ? d->Root->HasItem(Key) : 0;
}

bool GDiskBTree::Empty()
{
	bool Status = false;

	if (d->F.IsOpen())
	{
		DeleteObj(d->Root);
		d->F.SetSize(NODE_SIZE);

		d->Bmp.Length(0);
		d->SetBit(0, true);
		d->SetBit(1, true);

		d->Root = new BNode(d);
		if (d->Root)
		{
			Status = d->Root->Read(NODE_SIZE, true);
		}
	}
	
	return Status;
}
