/// \file
/// \author Matthew Allen
/// \data 14/8/2008
#ifndef _GTAR_FILE_H_
#define _GTAR_FILE_H_

#include "ZlibWrapper.h"

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
		GVariant Dir;
		GVariant Name;
		GDateTime Modified;
		int64 Size;

		int Blocks()
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
	/// The compression lib
	Zlib z;

	/// The stream wrapper around the lib
	GZlibFile f;

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
	virtual bool Process(GTarFile &f, GStream &s, bool &SkipFile)
	{
		if (Files)
		{
			Files->New() = f;
		}
		if (ExtractTo)
		{
			char o[MAX_PATH];
			LgiMakePath(o, sizeof(o), ExtractTo, f.Dir.Str());
			if (!DirExists(o))
				FileDev->CreateDirectory(o);
			LgiMakePath(o, sizeof(o), o, f.Name.Str());
			GFile Out;
			if (Out.Open(o, O_WRITE))
			{
				char Buf[512];
				int Blocks = f.Blocks();
				int64 ToWrite = f.Size;
				for (int i=0; i<Blocks; i++)
				{
					int r = s.Read(Buf, sizeof(Buf));
					if (r <= 0)
						return false;

					int Len = ToWrite < r ? ToWrite : r;
					int w = Out.Write(Buf, Len);
					if (w != Len)
						return false;
					
					ToWrite -= w;
				}

				SkipFile = true;
			}
		}

		return true;
	}

	/// Main parsing loop
	bool Parse(char *FileName, int Mode)
	{
		bool Status = false;

		if (Status = f.Open(FileName, Mode))
		{
			GTarHdr Hdr;
			int Size = sizeof(Hdr);
			
			int r;
			while ((r = f.Read(&Hdr, sizeof(Hdr))) > 0)
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
						t.Name = Hdr.Name;
					
					t.Size = FileSize;

					int ModTime = Octal(Hdr.ModifiedTime);
					t.Modified.Set((time_t)ModTime);

					bool SkipFile = true;
					if (!Process(t, f, SkipFile))
						break;
					
					if (SkipFile)
					{
						// Read over the file contents
						int Blocks = t.Blocks();
						char Buf[512];
						for (int i=0; i<Blocks; i++)
						{
							r = f.Read(&Buf, sizeof(Buf));
							if (r != sizeof(Buf))
							{
								break;
							}
						}
					}
				}
			}
		}

		return Status;
	}

public:
	GTarParser() : f(&z)
	{
		Files = 0;
		ExtractTo = 0;
	}

	/// Get a directory listing of the tar file
	bool DirList(char *File, GArray<GTarFile> &Out)
	{
		bool Status = false;
		
		Files = &Out;
		Status = Parse(File, O_READ);
		Files = 0;
		
		return Status;
	}

	/// Extract all the tar file contents to a folder
	bool Extract(char *File, char *OutFolder)
	{
		bool Status = false;
		
		if (OutFolder)
		{
			ExtractTo = OutFolder;
			Status = Parse(File, O_READ);
			ExtractTo = 0;
		}
		
		return Status;
	}
};

#endif