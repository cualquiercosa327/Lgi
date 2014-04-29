#ifndef _ZLIB_WRAPPER_H_
#define _ZLIB_WRAPPER_H_

#include "zlib.h"
#include "GLibraryUtils.h"

#ifndef COMP_FUNCTIONS
#define COMP_FUNCTIONS 0  // set to '1' if you need detailed compression functions
#endif

class Zlib : public GLibrary
{
public:
	Zlib() : GLibrary("zlib")
	{
	}

	DynFunc0(const char *, zlibVersion);
	DynFunc2(int, deflate, z_streamp, strm, int, flush);
	DynFunc2(int, inflate, z_streamp, strm, int, flush);

	DynFunc4(int, inflateInit2_, z_streamp, strm, int,  windowBits, const char *, version, int, stream_size);

	#if COMP_FUNCTIONS
	DynFunc1(int, inflateEnd, z_streamp, strm);
	DynFunc1(int, deflateEnd, z_streamp, strm);
	DynFunc3(int, deflateSetDictionary, z_streamp, strm, const Bytef *, dictionary, uInt, dictLength);
	DynFunc2(int, deflateCopy, z_streamp, dest, z_streamp, source);
	DynFunc1(int, deflateReset, z_streamp, strm);
	DynFunc3(int, deflateParams, z_streamp, strm, int, level, int, strategy);
	DynFunc5(int, deflateTune, z_streamp, strm, int, good_length, int, max_lazy, int, nice_length, int, max_chain);
	DynFunc2(uLong, deflateBound, z_streamp, strm, uLong, sourceLen);
	DynFunc3(int, deflatePrime, z_streamp, strm, int, bits, int, value);
	DynFunc2(int, deflateSetHeader, z_streamp, strm, gz_headerp, head);
	DynFunc3(int, inflateSetDictionary, z_streamp, strm, const Bytef *, dictionary, uInt, dictLength);
	DynFunc1(int, inflateSync, z_streamp, strm);
	DynFunc2(int, inflateCopy, z_streamp, dest, z_streamp, source);
	DynFunc1(int, inflateReset, z_streamp, strm);
	DynFunc3(int, inflatePrime, z_streamp, strm, int, bits, int, value);
	DynFunc2(int, inflateGetHeader, z_streamp, strm, gz_headerp, head);
	DynFunc5(int, inflateBack, z_streamp, strm, in_func, in, void FAR *, in_desc, out_func, out, void FAR *, out_desc);
	DynFunc1(int, inflateBackEnd, z_streamp, strm);
	DynFunc0(uLong, zlibCompileFlags);
	DynFunc4(int, compress, Bytef *,dest, uLongf *,destLen, const Bytef *,source, uLong, sourceLen);
	DynFunc5(int, compress2,Bytef *,dest, uLongf *,destLen, const Bytef *,source, uLong, sourceLen, int, level);
	DynFunc1(uLong, compressBound,uLong, sourceLen);
	DynFunc4(int, uncompress,Bytef *,dest,   uLongf *,destLen, const Bytef *,source, uLong, sourceLen);
	#endif
	
	DynFunc2(gzFile, gzopen, const char *,path, const char *,mode);
	DynFunc2(gzFile, gzdopen, int, fd, const char *, mode);
	DynFunc3(int, gzsetparams, gzFile, file, int, level, int, strategy);
	DynFunc3(int, gzread , gzFile, file, voidp, buf, unsigned, len);
	DynFunc3(int, gzwrite, gzFile, file, voidpc, buf, unsigned, len);
	// DynFunc1(intVA,  gzprintf,gzFile, file, const char *,format, ...);
	DynFunc2(int, gzputs, gzFile, file, const char *,s);
	DynFunc3(char *, gzgets, gzFile, file, char *,buf, int, len);
	DynFunc2(int, gzputc, gzFile, file, int, c);
	// DynFunc1(int, gzgetc, gzFile, file);
	DynFunc2(int, gzungetc, int, c, gzFile, file);
	DynFunc2(int, gzflush, gzFile, file, int, flush);
	DynFunc3(z_off_t, gzseek, gzFile, file, z_off_t, offset, int, whence);
	DynFunc1(int, gzrewind, gzFile, file);
	DynFunc1(z_off_t, gztell, gzFile, file);
	DynFunc1(int, gzeof, gzFile, file);
	DynFunc1(int, gzdirect, gzFile, file);
	DynFunc1(int, gzclose, gzFile, file);
	DynFunc2(const char *, gzerror, gzFile, file, int *,errnum);
	DynFunc1(int, gzclearerr,gzFile, file);
};

class GZlibFile : public GStream
{
	Zlib *z;
	gzFile f;

public:
	GZlibFile(Zlib *zlib)
	{
		z = zlib;
		f = 0;
		LgiAssert(z != NULL);
	}

	~GZlibFile()
	{
		Close();
	}

	int Open(char *Str, int Mode)
	{
		Close();

		f = z->gzopen(Str, Mode == O_WRITE ? "wb" : "rb");
		
		return IsOpen();
	}

	bool IsOpen() { return f != 0; }
	int Close()
	{
		if (f)
		{
			z->gzclose(f);
			f = 0;
			return true;
		}
		return false;
	}
	
	int64 GetSize()
	{
		return -1;
	}

	int64 SetSize(int64 Size)
	{
		return -1;
	}

	int64 GetPos()
	{
		if (!IsOpen())
			return -1;

		return z->gztell(f);
	}

	int64 SetPos(int64 Pos)
	{
		if (!IsOpen())
			return -1;

		return z->gzseek(f, (long)Pos, SEEK_SET);
	}
	
	int Read(void *Buffer, int Size, int Flags = 0)
	{
		if (!IsOpen())
			return -1;

		return z->gzread(f, Buffer, Size);
	}
	
	int Write(const void *Buffer, int Size, int Flags = 0)
	{
		if (!IsOpen())
			return -1;

		return z->gzwrite(f, Buffer, Size);
	}
};

#endif