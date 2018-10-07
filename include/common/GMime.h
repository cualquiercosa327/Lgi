#ifndef _GMIME_H_
#define _GMIME_H_

#include "LgiNetInc.h"
#include "GStream.h"
#include "INetTools.h"

extern void CreateMimeBoundary(char *Buf, int BufLen);

class GMime;

class GMimeAction
{
	friend class GMime;

protected:
	// Parent ptr
	GMime *Mime;

public:
	GMimeAction()
	{
		Mime = 0;
	}

	virtual void Empty() {} // reset to initial state
};

class GMimeBuf : public GStringPipe
{
	int Total;
	GStreamI *Src;
	GStreamEnd *End;

public:
	GMimeBuf(GStreamI *src, GStreamEnd *end);
	ssize_t Pop(char *Str, ssize_t BufSize);
};

class GMime
{
	// Header info
	char *Headers;

	// Data info
	ssize_t DataPos;
	ssize_t DataSize;
	LMutex *DataLock;
	GStreamI *DataStore;
	bool OwnDataStore;

	// Other info
	char *TmpPath;
	GMime *Parent;
	GArray<GMime*> Children;

	// Private methods
	bool Lock();
	void Unlock();
	bool CreateTempData();
	char *NewValue(char *&s, bool Alloc = true);
	char *StartOfField(char *s, const char *Feild);
	char *NextField(char *s);
	char *GetTmpPath();

public:
	static const char *DefaultCharset;

	GMime(char *TmpFileRoot = 0);
	virtual ~GMime();

	// Methods
	bool Insert(GMime *m, int pos = -1);
	void Remove();
	ssize_t Length() { return Children.Length(); }
	GMime *operator[](uint32 i);
	GMime *NewChild();
	void DeleteChildren() { Children.DeleteObjects(); }

	void Empty();
	bool SetHeaders(const char *h);
	char *GetHeaders() { return Headers; }
	ssize_t GetLength() { return DataSize; }
	GStreamI *GetData(bool Detach = false);
	bool SetData(bool OwnStream, GStreamI *Input, int RdPos = 0, int RdSize = -1, LMutex *Lock = 0);
	bool SetData(char *Str, int Len);

	// Simple Header Management
	char *Get(const char *Field, bool Short = true, const char *Default = 0); // 'Short'=true returns the value with out subfields
	bool Set(const char *Field, const char *Value); // 'Value' has to include any subfields.
	char *GetSub(const char *Field, const char *Sub);
	bool SetSub(const char *Field, const char *Sub, const char *Value, const char *DefaultValue = 0);

	// Header Shortcuts (uses Get[Sub]/Set[Sub])
	char *GetMimeType()				{ return Get("Content-Type", true, "text/plain"); }
	bool SetMimeType(const char *s)	{ return Set("Content-Type", s); }
	char *GetEncoding()				{ return Get("Content-Transfer-Encoding"); }
	bool SetEncoding(const char *s)	{ return Set("Content-Transfer-Encoding", s); }
	char *GetCharset()				{ return GetSub("Content-Type", "Charset"); }
	bool SetCharset(const char *s)	{ return SetSub("Content-Type", "Charset", s, DefaultCharset); }
	char *GetBoundary()				{ return GetSub("Content-Type", "Boundary"); }
	bool SetBoundary(const char *s)	{ return SetSub("Content-Type", "Boundary", s, DefaultCharset); }
	char *GetFileName();
	bool SetFileName(const char *s)	{ return SetSub("Content-Type", "Name", s, DefaultCharset); }

	// Streaming
	class GMimeText
	{
	public:
		class GMimeDecode : public GPullStreamer, public GMimeAction
		{
		public:
			ssize_t Pull(GStreamI *Source, GStreamEnd *End = 0);
			int Parse(GStringPipe *Source, class ParentState *State = 0);
			void Empty();
		} Decode;

		class GMimeEncode : public GPushStreamer, public GMimeAction
		{
		public:
			ssize_t Push(GStreamI *Dest, GStreamEnd *End = 0);
			void Empty();
		} Encode;

	} Text;

	friend class GMime::GMimeText::GMimeDecode;
	friend class GMime::GMimeText::GMimeEncode;

	class GMimeBinary
	{
	public:
		class GMimeRead : public GPullStreamer, public GMimeAction
		{
		public:
			ssize_t Pull(GStreamI *Source, GStreamEnd *End = 0);
			void Empty();
		} Read;

		class GMimeWrite : public GPushStreamer, public GMimeAction
		{
		public:
			int64 GetSize();
			ssize_t Push(GStreamI *Dest, GStreamEnd *End = 0);
			void Empty();
		} Write;

	} Binary;

	friend class GMime::GMimeBinary::GMimeRead;
	friend class GMime::GMimeBinary::GMimeWrite;
};

#endif
