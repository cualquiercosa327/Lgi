#include <stdio.h>

#include "Lgi.h"
#include "GZipFile.h"
#include "Lzw.h"

#define LOCAL_HEADER_MAGIC		0x04034b50
#ifdef WIN32
#pragma pack(1)
#endif

struct ZipLocalHeader
{
public:
	uint32 Magic;		// LOCAL_HEADER_MAGIC
	uint16 Version;
	
	// uint16 Flags;
	uint16 Encrypted : 1;
	uint16 Use8Kwindow : 1;
	uint16 TreesUsed : 1;
	uint16 CrcsFollowData : 1;
	uint16 Reserved1 : 1;
	uint16 PatchedData : 1;
	uint16 Reserved2 : 10;

	uint16 CompressMethod;
	uint16 LastModifiedTime;
	uint16 LastModifiedDate;
	uint32 Crc;
	uint32 CompressedSize;
	uint32 UncompressedSize;
	uint16 FileNameLength;
	uint16 ExtraFieldLength;

	char *FileName;
	char *ExtraField;
	int FilePos;

	int Sizeof()
	{
		return sizeof(*this) - sizeof(FileName) - sizeof(ExtraField) - sizeof(FilePos);
	}

	ZipLocalHeader()
	{
		LgiAssert(Sizeof() == 30);
		ZeroObj(*this);
	}

	~ZipLocalHeader()
	{
		DeleteArray(FileName);
		DeleteArray(ExtraField);
	}
};

class GZipFilePrivate
{
public:
	GFile Zip;
	GArray<ZipLocalHeader*> Files;

	GZipFilePrivate()
	{
	}

	~GZipFilePrivate()
	{
		Empty();
	}

	void Empty()
	{
		for (int i=0; i<Files.Length(); i++)
		{
			DeleteObj(Files[i]);
		}
		Files.Length(0);
	}

	ZipLocalHeader *Find(char *File)
	{
		for (int i=0; File AND i<Files.Length(); i++)
		{
			ZipLocalHeader *h = Files[i];
			if (h AND
				h->FileName AND
				stricmp(h->FileName, File) == 0)
			{
				return h;
			}
		}

		return 0;
	}
};

class GZipDir : public GDirectory
{
	int Cur;
	GArray<ZipLocalHeader*> *Files;

	ZipLocalHeader *c()
	{
		if (Cur >= 0 AND Files AND Cur < Files->Length())
		{
			return (*Files)[Cur];
		}
		return 0;
	}

public:
	GZipDir(GArray<ZipLocalHeader*> *files)
	{
		Cur = -1;
		Files = files;
	}

	int First(char *Name, char *Pattern)
	{
		Cur = 0;
		return c() != 0;
	}

	int Next()
	{
		Cur++;
		return c() != 0;
	}

	int Close()
	{
		Cur = -1;
		return true;
	}

	bool Path(char *s, int BufSize)
	{
		ZipLocalHeader *h = c();
		if (h)
		{
			strsafecpy(s, h->FileName, BufSize);
			return true;
		}
		return false;
	}

	long GetAttributes()
	{
		return 0;
	}

	char *GetName()
	{
		return c() ? c()->FileName : 0;
	}

	int GetUser(bool Group) { return 0; }
	const uint64 GetCreationTime() { return 0; }
	const uint64 GetLastAccessTime() { return 0; }
	const uint64 GetLastWriteTime() { return 0; }
	const uint64 GetSize() { return c() ? c()->UncompressedSize : 0; }
	bool IsDir() { return false; }
	bool IsReadOnly() { return false; }
	bool IsHidden() { return false; }
	GDirectory *Clone() { return new GZipDir(Files); }
	int GetType() { return VT_FILE; }
};

GZipFile::GZipFile(char *zip)
{
	d = new GZipFilePrivate;
	if (zip)
	{
		Open(zip, O_READWRITE);
	}
}

GZipFile::~GZipFile()
{
	Close();
	DeleteObj(d);
}

bool GZipFile::IsOpen()
{
	return d->Zip.IsOpen();
}

bool GZipFile::Open(char *Zip, int Mode)
{
	d->Empty();

	if (d->Zip.Open(Zip, Mode))
	{
		// Read the directory table
		d->Zip.SetPos(0);
		while (!d->Zip.Eof())
		{
			ZipLocalHeader *h = new ZipLocalHeader;
			if (h)
			{
				if (d->Zip.Read(h, h->Sizeof()) == h->Sizeof())
				{
					if (h->Magic == LOCAL_HEADER_MAGIC)
					{
						h->FileName = new char[h->FileNameLength+1];
						if (h->FileName)
						{
							d->Zip.Read(h->FileName, h->FileNameLength);
							h->FileName[h->FileNameLength] = 0;

							#ifdef WIN32
							char *s;
							while (s = strchr(h->FileName, '/'))
							{
								*s = '\\';
							}
							#endif
						}

						if (ValidStr(h->FileName))
						{
							h->ExtraField = new char[h->ExtraFieldLength+1];
							if (h->ExtraField)
							{
								d->Zip.Read(h->ExtraField, h->ExtraFieldLength);
								h->ExtraField[h->ExtraFieldLength] = 0;
							}

							d->Files[d->Files.Length()] = h;

							h->FilePos = d->Zip.GetPos();
							d->Zip.Seek(h->CompressedSize, SEEK_CUR);

							if (h->CrcsFollowData)
							{
								d->Zip.Seek(12, SEEK_CUR);
							}
						}
						else
						{
							DeleteObj(h);
							break;
						}
					}
					else
					{
						DeleteObj(h);
						break;
					}
				}
				else
				{
					DeleteObj(h);
					break;
				}
			}
			else break;
		}
	}

	return d->Files.Length() > 0;
}

void GZipFile::Close()
{
	if (IsOpen())
	{
		d->Zip.Close();
	}
}

GDirectory *GZipFile::List()
{
	return new GZipDir(&d->Files);
}

bool GZipFile::Decompress(char *File, GStream *To)
{
	ZipLocalHeader *h = d->Find(File);
	if (h)
	{
		if (d->Zip.Seek(h->FilePos, SEEK_SET) == h->FilePos)
		{
			switch (h->CompressMethod)
			{
				case 0: // Stored
				{
					LgiAssert(0);
					break;
				}
				case 1: // Shrunk
				{
					Lzw l;
					return l.Decompress(To, &d->Zip);
				}
				case 2: // Reduced
				case 3: // Reduced
				case 4: // Reduced
				case 5: // Reduced
				{
					LgiAssert(0);
					break;
				}
				case 6: // Imploded
				{
					LgiAssert(0);
					break;
				}
				case 8: // Deflated
				{
					LgiAssert(0);
					break;
				}
				case 9: // Deflate64
				{
					LgiAssert(0);
					break;
				}
				case 10: // PKWARE Date Compression
				{
					LgiAssert(0);
					break;
				}
			}
		}
	}

	return false;
}

bool GZipFile::Compress(char *File, GStream *From)
{
	return false;
}

bool GZipFile::Delete(char *File)
{
	return false;
}

