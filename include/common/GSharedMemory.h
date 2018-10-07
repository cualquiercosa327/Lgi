/// \file
/// \author Matthew Allen
#ifndef _GSHARED_MEMORY_H_
#define _GSHARED_MEMORY_H_

/// \brief Shared memory wrapper class.
///
/// This class enables separate processes to access the same peice of 
/// memory. A full discussion of the problems of pre-emptively multi-tasked
/// processes accessing the same memory is beyond the scope of this document
/// but suffice to say you need to know what your doing.
///
/// Initially all the bytes of the memory block are initialized to 0.
class GSharedMemory
{
	class GSharedMemoryPrivate *d;

public:
	/// Construt the shared memory
	GSharedMemory
	(
		/// The unique name of the memory block. Should be the same string used by all processes
		/// wishing to access the same memory.
		const char *Name,
		/// The number of bytes to allocate, should also be the same in all processes connecting
		/// to the shared memory.
		int Size
	);
	/// Disconnect from the shared memory and free resources. This doesn't delete the
	/// shared memory block on Linux.
	virtual ~GSharedMemory();
	
	/// Returns the start of the block.
	void *GetPtr();
	/// Returns the number of bytes addressable.
	int GetSize();
	/// Disconnects from the shared memory resource.
	void Destroy();
};

#endif
