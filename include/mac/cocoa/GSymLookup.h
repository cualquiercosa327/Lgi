/// \file
/// \author Matthew Allen
#ifndef _GSYMLOOKUP_H_
#define _GSYMLOOKUP_H_

#include <dlfcn.h>
#include <mach/vm_types.h>
#include <sys/uio.h>

#include <cxxabi.h>
#include "GProcess.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _POSIX_C_SOURCE
#error "This shouldn't be defined."
#endif
#ifndef _DLFCN_H_
#error "Include dlfcn.h"
#endif

#if __LP64__
#define _BACKTRACE_FORMAT "%-4d%-35s 0x%16p %s + %u"
#define _BACKTRACE_FORMAT_SIZE 82
#else
#define _BACKTRACE_FORMAT "%-4d%-35s 0x%08p %s + %u"
#define _BACKTRACE_FORMAT_SIZE 65
#endif

#if defined(__i386__) || defined(__x86_64__)
#define FP_LINK_OFFSET 1
#elif defined(__ppc__) || defined(__ppc64__)
#define FP_LINK_OFFSET 2
#else
#error  ********** Unimplemented architecture
#endif

#define	INSTACK(a)	((a) >= stackbot && (a) <= stacktop)
#if defined(__ppc__) || defined(__ppc64__) || defined(__x86_64__)
#define	ISALIGNED(a)	((((uintptr_t)(a)) & 0xf) == 0)
#elif defined(__i386__)
#define	ISALIGNED(a)	((((uintptr_t)(a)) & 0xf) == 8)
#endif

/// Lookup the file/line information for an instruction pointer value
class GSymLookup
{
public:
	typedef void *Addr;

private:
	__attribute__((noinline))
	void
	_thread_stack_pcs(vm_address_t *buffer, unsigned max, unsigned *nb, unsigned skip)
	{
		void *frame, *next;
		pthread_t self = pthread_self();
		void *stacktop = pthread_get_stackaddr_np(self);
		void *stackbot = (char*)stacktop - pthread_get_stacksize_np(self);

		*nb = 0;

		/* make sure return address is never out of bounds */
		(char*&)stacktop -= (FP_LINK_OFFSET + 1) * sizeof(void *);

		/*
		 * The original implementation called the first_frame_address() function,
		 * which returned the stack frame pointer.  The problem was that in ppc,
		 * it was a leaf function, so no new stack frame was set up with
		 * optimization turned on (while a new stack frame was set up without
		 * optimization).  We now inline the code to get the stack frame pointer,
		 * so we are consistent about the stack frame.
		 */
	#if defined(__i386__) || defined(__x86_64__)
		frame = __builtin_frame_address(0);
	#elif defined(__ppc__) || defined(__ppc64__)
		/* __builtin_frame_address IS BROKEN IN BEAKER: RADAR #2340421 */
		__asm__ volatile("mr %0, r1" : "=r" (frame));
	#endif
		if(!INSTACK(frame) || !ISALIGNED(frame))
		return;
	#if defined(__ppc__) || defined(__ppc64__)
		/* back up the stack pointer up over the current stack frame */
		next = *(void **)frame;
		if(!INSTACK(next) || !ISALIGNED(next) || next <= frame)
		return;
		frame = next;
	#endif
		while (skip--) {
		next = *(void **)frame;
		if(!INSTACK(next) || !ISALIGNED(next) || next <= frame)
			return;
		frame = next;
		}
		while (max--) {
			buffer[*nb] = *(vm_address_t *)(((void **)frame) + FP_LINK_OFFSET);
			(*nb)++;
		next = *(void **)frame;
		if(!INSTACK(next) || !ISALIGNED(next) || next <= frame)
			return;
		frame = next;
		}
	}


	int
	_backtrace_snprintf(char* buf, size_t size, int frame, const void* addr, const Dl_info* info)
	{
		char symbuf[19];
		const char* image = "???";
		const char* symbol = symbuf;

		if (info->dli_fname) {
			image = strrchr(info->dli_fname, '/') + 1;
			if (image == NULL) image = info->dli_fname;
		}
		
		if (info->dli_sname) {
			symbol = info->dli_sname;
		} else {
			snprintf(symbuf, sizeof(symbuf), "0x%p", info->dli_saddr);
		}
		
		return snprintf(buf, size,
				_BACKTRACE_FORMAT,
				frame,
				image,
				addr,
				symbol,
				(int) ((char*)addr - (char*)info->dli_saddr)) + 1;
	}

	char**
	backtrace_symbols(Addr* buffer, int size)
	{
		int i;
		size_t total_bytes;
		char** result;
		char** ptrs;
		intptr_t strs;
		Dl_info* info = (Dl_info*) calloc(size, sizeof (Dl_info));
		
		if (info == NULL)
			return NULL;
		
		// Compute the total size for the block that is returned.
		// The block will contain size number of pointers to the
		// symbol descriptions.
		total_bytes = sizeof(char*) * size;
		
		// Plus each symbol description
		for (i = 0 ; i < size; ++i) {
			dladdr(buffer[i], &info[i]);
			total_bytes += _BACKTRACE_FORMAT_SIZE + 1;
			if (info[i].dli_sname) total_bytes += strlen(info[i].dli_sname);
		}
		
		result = (char**)malloc(total_bytes);
		if (result == NULL) {
			free(info);
			return NULL;
		}
		
		// Fill in the array of pointers and append the strings for
		// each symbol description.
		
		ptrs = result;
		strs = ((intptr_t)result) + sizeof(char*) * size;
		
		for (i = 0; i < size; ++i) {
			ptrs[i] = (char*)strs;
			strs += _backtrace_snprintf((char*)strs, total_bytes, i, buffer[i], &info[i]);
		}
		
		free(info);
		
		return result;
	}

	void backtrace_symbols_fd(void* const* buffer, int size, int fd)
	{
		int i;
		char buf[BUFSIZ];
		Dl_info info;
		struct iovec iov[2];

		iov[0].iov_base = buf;

		iov[1].iov_base = (void*)"\n";
		iov[1].iov_len = 1;

		for (i = 0; i < size; ++i) {
			memset(&info, 0, sizeof(info));
			dladdr(buffer[i], &info);

			iov[0].iov_len = _backtrace_snprintf(buf, sizeof(buf), i, buffer[i], &info);
			
			writev(fd, iov, 2);
		}
	}


public:
	GSymLookup()
	{
	}
	
	~GSymLookup()
	{
	}
	
	bool GetStatus()
	{
		return true;
	}
	
	/// Looks up the file and line related to an instruction pointer address
	/// \returns non zero on success
	bool Lookup
	(
		/// The return string buffer
		char *buf,
		/// The buffer's length
		int buflen,
		/// The addresses
		Addr *Ip,
		/// Numbers of addresses
		int IpLen
	)
	{
		#if 0

		Dl_info dli;
		if (dladdr(Ip, &dli))
		{
			// char * abi::__cxa_demangle( const char *mangled_name, char *output_buffer, size_t *length, int *status );
			ptrdiff_t       offset;
			int c = 0;
			
			if (dli.dli_fname && dli.dli_fbase)
			{
				offset = (ptrdiff_t)Ip - (ptrdiff_t)dli.dli_fbase;
				c = snprintf(buf, buflen, "%s+0x%x", dli.dli_fname, offset );
			}
			if (dli.dli_sname && dli.dli_saddr)
			{
				offset = (ptrdiff_t)Ip - (ptrdiff_t)dli.dli_saddr;
				c += snprintf(buf+c, buflen-c, "(%s+0x%x)", dli.dli_sname, offset );
			}
			
			if (c > 0)
			{
				snprintf(buf+c, buflen-c, " [%p]", Ip);
			}

			return true;
		}
		else printf("%s:%i - dladdr failed for %p\n", _FL, Ip);
		return false;

		#else

		char Ws[] = " \t\r\n";
		char **Syms = backtrace_symbols(Ip, IpLen);
		if (!Syms)
			return false;

		for (int i=0; i<IpLen; i++)
		{
			char *Sym = Syms[i];
			int Ch = 0;
			
			if (0)
			{
				char *c = Sym;
				char Binary[256];
				GAutoString Method;
				char *End = 0;
				int Idx = 0;
				
				for (int i=0; *c; i++)
				{
					char *Start = c;
					while (*c && !strchr(Ws, *c)) c++;
					
					switch (i)
					{
						case 0:
						{
							Idx = atoi(Start);
							break;
						}
						case 1:
						{
							memcpy(Binary, Start, c-Start);
							Binary[c-Start] = 0;
							break;
						}
						case 3:
						{
							char Args[256];
							sprintf(Args, "-n %.*s", (int) (c-Start), Start);
							GProcess p;
							GStringPipe Out;
							if (p.Run("c++filt", Args, 0, true, 0, &Out) &&
								Method.Reset(Out.NewStr()))
							{
								char *e = Method + strlen(Method);
								while (e > Method && strchr(Ws, e[-1]))
									*--e = 0;
							}
							else
							{
								Method.Reset(NewStr(Start, c-Start));
							}
							
							End = c;
							break;
						}
					}
					
					while (*c && strchr(Ws, *c)) c++;
				}

				Ch = snprintf(buf, buflen, "\t%i\t%s\t%s%s\n", Idx, Binary, Method.Get(), End);
			}
			else
			{
				Ch = snprintf(buf, buflen, "%s\n", Sym);
			}
			buf += Ch;
			buflen -= Ch;
		}
		
		free(Syms);
		return true;
		
		#endif
	}
	
	/// Gets the current stack
	int BackTrace(long Eip, long Ebp, Addr *buffer, int max_frames)
	{
		#if 0

		void **frame = (void **)__builtin_frame_address(0);
		void **bp = ( void **)(*frame);
		void *ip = frame[1];
		int i;

		for ( i = 0; bp && ip && i < max_frames; i++ )
		{
			*(buffer++) = ip;
			ip = bp[1];
			bp = (void**)(bp[0]);
		}

		return i;

		#else

		unsigned int num_frames;
		_thread_stack_pcs((vm_address_t*)buffer, max_frames, &num_frames, 1);
		while (num_frames >= 1 && buffer[num_frames-1] == NULL)
			num_frames -= 1;

		return num_frames;
		#endif
	}
};

#endif