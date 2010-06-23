#ifndef _GMIME_H_
#define _GMIME_H_

#include "LgiNetInc.h"
#include "GStream.h"
#include "INetTools.h"

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

class GMime
{
	// Header info
	char *Headers;

	// Data info
	int DataPos;
	int DataSize;
	GSemaphore *DataLock;
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
	char *StartOfField(char *s, char *Feild);
	char *NextField(char *s);
	char *GetTmpPath();

public:
	static char *DefaultCharset;

	GMime(char *TmpFileRoot = 0);
	virtual ~GMime();

	// Methods
	bool Insert(GMime *m, int pos = -1);
	void Remove();
	int Length() { return Children.Length(); }
	GMime *operator[](int i);
	void DeleteChildren() { Children.DeleteObjects(); }

	void Empty();
	bool SetHeaders(char *h);
	char *GetHeaders() { return Headers; }
	int GetLength() { return DataSize; }
	GStreamI *GetData(bool Detach = false);
	bool SetData(GStreamI *d, int p = 0, int s = -1, GSemaphore *l = 0);
	bool SetData(char *Str, int Len);

	// Simple Header Management
	char *Get(char *Field, bool Short = true, char *Default = 0); // 'Short'=true returns the value with out subfields
	bool Set(char *Field, char *Value); // 'Value' has to include any subfields.
	char *GetSub(char *Field, char *Sub);
	bool SetSub(char *Field, char *Sub, char *Value, char *DefaultValue = 0);

	// Header Shortcuts (uses Get[Sub]/Set[Sub])
	char *GetMimeType()			{ return Get("Content-Type", true, "text/plain"); }
	bool SetMimeType(char *s)	{ return Set("Content-Type", s); }
	char *GetEncoding()			{ return Get("Content-Transfer-Encoding"); }
	bool SetEncoding(char *s)	{ return Set("Content-Transfer-Encoding", s); }
	char *GetCharset()			{ return GetSub("Content-Type", "Charset"); }
	bool SetCharset(char *s)	{ return SetSub("Content-Type", "Charset", s, DefaultCharset); }
	char *GetBoundary()			{ return GetSub("Content-Type", "Boundary"); }
	bool SetBoundary(char *s)	{ return SetSub("Content-Type", "Boundary", s, DefaultCharset); }
	char *GetFileName();
	bool SetFileName(char *s)	{ return SetSub("Content-Type", "Name", s, DefaultCharset); }

	// Streaming
	class GMimeText
	{
	public:
		class GMimeDecode : public GPullStreamer, public GMimeAction
		{
			int Parse(GStringPipe *Source, class ParentState *State = 0);
		public:
			int Pull(GStreamI *Source, GStreamEnd *End = 0);
			void Empty();
		} Decode;

		class GMimeEncode : public GPushStreamer, public GMimeAction
		{
		public:
			int Push(GStreamI *Dest, GStreamEnd *End = 0);
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
			int Pull(GStreamI *Source, GStreamEnd *End = 0);
			void Empty();
		} Read;

		class GMimeWrite : public GPushStreamer, public GMimeAction
		{
		public:
			int64 GetSize();
			int Push(GStreamI *Dest, GStreamEnd *End = 0);
			void Empty();
		} Write;

	} Binary;

	friend class GMime::GMimeBinary::GMimeRead;
	friend class GMime::GMimeBinary::GMimeWrite;
};

#endif
