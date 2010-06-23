#include "Lgi.h"
#include "GSharedMemory.h"
#include "GVariant.h"

#ifdef LINUX
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

class GSharedMemoryPrivate
{
public:
	char *Name;
	int Size;
	void *Ptr;

	#if defined(LINUX)
	int Id;
	#elif defined(WIN32)
	HANDLE hMem;
	#endif
	
	GSharedMemoryPrivate(char *name, int size)
	{
		Name = NewStr(name);
		Size = size;
		Ptr = 0;
		
		#if defined(LINUX)
		int Hash = LgiHash<uchar>((uchar*)Name, 0, true);
		Id = shmget(Hash, Size, 0644);
		bool New = false;
		if (Id < 0)
		{
			// New block
			Id = shmget(Hash, Size, 0644 | IPC_CREAT);
			if (Id >= 0)
			{
				New = true;
			}
		}
		if (Id >= 0)
		{
			Ptr = shmat(Id, (void *)0, 0);
			if (Ptr == (void*)-1)
			{
				Ptr = 0;
			}
			else if (New)
			{
				memset(Ptr, 0, Size);
			}
		}
		#elif defined(WIN32)
		GVariant v = Name;
		hMem = CreateFileMapping(	INVALID_HANDLE_VALUE,
									NULL,
									PAGE_READWRITE,
									0,
									Size,
									#ifdef UNICODE
									v.WStr());
									#else
									Name);
									#endif
		if (hMem)
		{
			Ptr = MapViewOfFile(hMem, FILE_MAP_WRITE, 0, 0, Size);
		}
		#endif
	}
	
	~GSharedMemoryPrivate()
	{
		#if defined(LINUX)
		shmdt(Ptr);
		#elif defined(WIN32)
		Destroy();
		#endif
		
		DeleteArray(Name);
	}
	
	void Destroy()
	{
		#if defined LINUX
		if (Id >= 0)
		{
			shmctl(Id, IPC_RMID, 0);
			Id = -1;
		}
		#elif defined WIN32
		if (Ptr)
		{
			UnmapViewOfFile(Ptr);
			Ptr = 0;
		}
		if (hMem)
		{
			CloseHandle(hMem);
			hMem = 0;
		}
		#endif
		
		Ptr = 0;
	}

	void *GetPtr()
	{
		return Ptr;
	}
};

///////////////////////////////////////////////////////////////////////
GSharedMemory::GSharedMemory(char *Name, int Size)
{
	d = new GSharedMemoryPrivate(Name, Size);
}

GSharedMemory::~GSharedMemory()
{
	DeleteObj(d);
}

void *GSharedMemory::GetPtr()
{
	return d->GetPtr();
}

int GSharedMemory::GetSize()
{
	return d->Size;
}

void GSharedMemory::Destroy()
{
	d->Destroy();
}

