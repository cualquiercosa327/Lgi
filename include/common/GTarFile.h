/// \file
/// \author Matthew Allen
/// \data 14/8/2008
#ifndef _GTAR_FILE_H_
#define _GTAR_FILE_H_

#include "GStringClass.h"

/// Tar file parser
class GTarParser
{
public:
	/// A tar file listing.
	///
	/// Someone should probably add all the other fields from GTarHdr
	/// in here with the appropriate parsing but I don't need them (yet).
	struct GTarFile
	{
		GString Dir;
		GString Name;
		LDateTime Modified;
		int64 Size;

		int64 Blocks()
		{
			return (Size / 512) + (Size % 512 ? 1 : 0);
		}
	};

	/// The actual file format
	struct GTarHdr
	{
		char Name[100];
		char Mode[8];
		char Owner[8];
		char Group[8];
		char Size[12];
		char ModifiedTime[12];
		char CheckSum[8];
		char Type;
		char LinkedFile[100];

		char Ustar[6];
		char Ver[2];
		char OwnerUserName[32];
		char OwnerGrpName[32];
		char DeviceMaj[8];
		char DeviceMin[8];
		char FilePrefix[155];

		char Reserved[12];
	};

protected:
	/// Convert octal number to decimal
	int64 Octal(char *o)
	{
		int64 i = 0;
		
		while (*o)
		{
			if (*o < '0' || *o > '7')
				break;

			int s = *o - '0';
			i <<= 3;
			i |= s;
			o++;
		}

		return i;
	}

	/// If this is set, output files to array
	GArray<GTarFile> *Files;
	
	/// If this is set, then extract the files to this folder
	char *ExtractTo;

	/// Called every time a file is parsed
	/// \returns true if this function consumes the body of the file
	virtual bool Process(GTarFile &f, GStreamI *s, bool &SkipOverFile)
	{
		if (Files)
		{
			Files->New() = f;
		}
		if (ExtractTo)
		{
			char o[MAX_PATH];
			
			if (f.Dir)
			{
				LgiMakePath(o, sizeof(o), ExtractTo, f.Dir);
				if (!DirExists(o) &&
					!FileDev->CreateFolder(o, true))
				{
					return false;
				}
				LgiMakePath(o, sizeof(o), o, f.Name);
			}
			else
			{	
				LgiMakePath(o, sizeof(o), ExtractTo, f.Name);
			}
			
			GFile Out;
			if (Out.Open(o, O_WRITE))
			{
				char Buf[512];
				int64 Blocks = f.Blocks();
				int64 ToWrite = f.Size;
				for (int i=0; i<Blocks; i++)
				{
					int r = s->Read(Buf, sizeof(Buf));
					if (r <= 0)
						return false;

					int64 Len = ToWrite < r ? ToWrite : r;
					int w = Out.Write(Buf, (int)Len);
					if (w != Len)
						return false;
					
					ToWrite -= w;
				}

				SkipOverFile = false;
			}
			else
			{
				LgiTrace("%s:%i - Failed to open '%s' for writing.\n", _FL, o);
				return false;
			}
		}

		return true;
	}

	/// Main parsing loop
	bool Parse(GStreamI *File, int Mode)
	{
		if (!File)
			return false;

		GTarHdr Hdr;
		int Size = sizeof(Hdr);
		
		int r;
		while ((r = File->Read(&Hdr, sizeof(Hdr))) > 0)
		{
			int64 FileSize = Octal(Hdr.Size);
			if (FileSize)
			{
				GTarFile t;
				char *d = strrchr(Hdr.Name, '/');
				if (d == Hdr.Name)
					t.Name = Hdr.Name + 1;
				else if (d)
				{
					*d++ = 0;
					t.Name = d;
					t.Dir = Hdr.Name;
					d[-1] = '/';
				}
				else
				{
					t.Name = Hdr.Name;
				}
				
				t.Size = FileSize;

				int64 ModTime = Octal(Hdr.ModifiedTime);
				t.Modified.Set((time_t)ModTime);

				bool SkipOverFile = true;
				if (!Process(t, File, SkipOverFile))
					break;
				
				if (SkipOverFile)
				{
					// Read over the file contents
					int64 Blocks = t.Blocks();
					char Buf[512];
					for (int i=0; i<Blocks; i++)
					{
						r = File->Read(&Buf, sizeof(Buf));
						if (r != sizeof(Buf))
						{
							break;
						}
					}
				}
			}
		}

		return true;
	}

public:
	GTarParser()
	{
		Files = 0;
		ExtractTo = 0;
	}

	/// Get a directory listing of the tar file
	bool DirList(GStreamI *File, GArray<GTarFile> &Out)
	{
		bool Status = false;
		
		Files = &Out;
		Status = Parse(File, O_READ);
		Files = 0;
		
		return Status;
	}

	/// Extract all the tar file contents to a folder
	bool Extract(GStreamI *File, char *OutFolder)
	{
		if (!OutFolder)
			return false;

		ExtractTo = OutFolder;
		bool Status = Parse(File, O_READ);
		ExtractTo = 0;
		return Status;
	}
};

#endif