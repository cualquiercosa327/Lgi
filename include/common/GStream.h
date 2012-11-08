/**
	\file
	\author Matthew Allen
	\brief Data streaming class.\n
	Copyright (C), <a href="mailto:fret@memecode.com">Matthew Allen</a>
 */

#ifndef _GSTREAM_H_

#define _GSTREAM_H_

#ifdef _MSC_VER
#include <vadefs.h>
#endif
#include "GDom.h"

/// Stream printf
LgiFunc int GStreamPrintf(GStreamI *s, int flags, const char *Format, va_list &Arg);
LgiFunc int GStreamPrint(GStreamI *s, const char *fmt, ...);

/// \brief Virtual base class for a data source or sink.
class LgiClass GStream : virtual public GStreamI, virtual public GDom
{
public:
	virtual ~GStream() {}

	int Read(void *Ptr, int Size, int Flags = 0) { return 0; }
	int Write(const void *Ptr, int Size, int Flags = 0) { return 0; }
	
	/// \brief Formats a string and then writes it.
	virtual int Print(const char *Format, ...);
};

/// Defines an API for terminating a stream. 
class LgiClass GStreamEnd
{
public:
	virtual ~GStreamEnd() {}

	/// Reset the end point's state
	virtual void Reset() {}

	/// \brief Override to process a data chunk to check for an end
	/// of stream marker.
	/// \return The index into Data of the end of the stream, indexed
	/// from the start of the data, not the start of the current block.
	/// Or -1 if the end of stream is not in the data segment
	virtual int IsEnd
	(
		/// The start of the current data block
		void *Data,
		/// The length in bytes of the current data block
		int Len
	) = 0;
};

/// Stateful parser that matches the start of lines to the 'prefix'
class LgiClass GLinePrefix : public GStreamEnd
{
	bool Start;
	int Pos;
	char *At;
	char *Prefix;
	bool Eol;

public:
	/// Finds some text on the start of a line
	GLinePrefix
	(
		/// The string to look for on the start of the line
		const char *p,
		/// true if you want the index of the end of the line, otherwise
		/// the index of the start of the line is returned
		bool eol = true
	);
	
	~GLinePrefix();

	void Reset();
	int IsEnd(void *s, int Len);
};

/// Read a line
class LgiClass GEndOfLine : public GStreamEnd
{
public:
	int IsEnd(void *s, int Len);
};

/// Generic streaming operator
class LgiClass GStreamer
{
	int StartTime;
	int EndTime;
	int Total;
	int Size;
	char *Buf;

public:
	/// Constructor
	GStreamer
	(
		// Buffer size in bytes, 4 KB by default
		int BufSize = 4 << 10
	);
	virtual ~GStreamer();

	// Properties
	int GetRate();
	int GetTotal();
	int GetElapsedTime();
};

/// API to reads from source
class LgiClass GPullStreamer : public GStreamer
{
public:
	virtual int Pull(GStreamI *Source, GStreamEnd *End = 0) = 0;
};

/// API to writes to a destination
class LgiClass GPushStreamer : public GStreamer
{
public:
	virtual int Push(GStreamI *Dest, GStreamEnd *End = 0) = 0;
};

/// API to read from source and then write to a destination
class LgiClass GCopyStreamer
{
public:
	virtual int Copy(GStreamI *Source, GStreamI *Dest, GStreamEnd *End = 0);
};

/// In memory stream for storing sub-streams or memory blocks
class LgiClass GMemStream : public GStream
{
	char *Mem;
	int64 Len, Pos, Alloc;
	bool Own;
	int GrowBlockSize;

	void _Init();

public:
	/// Builds an empty memory stream
	GMemStream();
	/// Builds memory from sub-stream
	GMemStream
	(
		/// The source stream
		GStreamI *Src,
		/// The starting position in the stream, or -1 for the current position. Use -1 for non-seekable streams like sockets
		int64 Start,
		/// The length of the sub-stream, or -1 to read all the data to the end
		int64 Len
	);
	/// Builds a memory stream by copying from another memory block
	GMemStream
	(
		/// The source memory block
		void *Mem,
		/// The length of the block
		int64 Len,
		/// Whether to copy the block or reference memory we don't own
		bool Copy = true
	);
	/// Growable array to write to
	GMemStream
	(
		int GrowBlockSize
	);

	~GMemStream();
	
	bool IsOpen() { return Mem != 0; }
	int Close();
	int64 GetSize() { return Len; }
	int64 GetPos() { return Pos; }
	int64 SetPos(int64 p) { return Pos = p; }

	/// Opens a file and reads it all into memory
	int Open(const char *Str, int Int);
	/// Changes the size of the memory block, keeping any common bytes
	int64 SetSize(int64 Size);

	bool IsOk();
	int Read(void *Buffer, int Size, int Flags = 0);
	int Write(const void *Buffer, int Size, int Flags = 0);
	int Write(GStream *Out, int Size);
	char *GetBase() { return Mem; }
	GStreamI *Clone();
};

/// Wraps another stream in a GStreamI interface. Useful for
/// giving objects to downstream consumers that they can delete.
class LgiClass GProxyStream : public GStreamI
{
protected:
	GStreamI *s;

public:
	GProxyStream(GStreamI *p)
	{
		s = p;
	}

	int Open(const char *Str, int Int)	{ return s->Open(Str, Int); }
	bool IsOpen()						{ return s->IsOpen(); }
	int Close()							{ return s->Close(); }
	int64 GetSize()						{ return s->GetSize(); }
	int64 SetSize(int64 Size)			{ return s->SetSize(Size); }
	int64 GetPos()						{ return s->GetPos(); }
	int64 SetPos(int64 Pos)				{ return s->SetPos(Pos); }
	int Read(void *b, int l, int f = 0) { return s->Read(b, l, f); }
	int Write(const void *b, int l, int f = 0) { return s->Write(b, l, f); }
	bool GetValue(char *n, GVariant &v) { return s->GetValue(n, v); }
	bool SetValue(char *n, GVariant &v) { return s->SetValue(n, v); }

	GStreamI *Clone()					{ return new GProxyStream(s); }
};

/// A temporary FIFO stream that stores data in memory up until you
/// reach the 'MaxMemSize' limit at which point the class begins storing
/// data in a temporary file on disk.
class LgiClass GTempStream : public GProxyStream
{
	GStream Null;
	int MaxMemSize;

protected:
	class GMemStream *Mem;
	class GFile *Tmp;
	char *TmpFolder;

public:
	GTempStream(char *TmpFolder = 0, int maxMemSize = 1 << 20);
	~GTempStream();

	int GetMaxMemSize() { return MaxMemSize; }
	int Write(const void *Buffer, int Size, int Flags = 0);
	void Empty();
};

#endif
