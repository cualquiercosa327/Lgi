#include <stdio.h>

#ifdef WIN32
#include <winsock2.h>
#include <shlobj.h>
#endif
#include "Lgi.h"

//////////////////////////////////////////////////////////////////////////////////
/*
typedef uint32 Elf32_Addr;
typedef uint16 Elf32_Half;
typedef uint32 Elf32_Off;
typedef int32 Elf32_Sword;
typedef uint32 Elf32_Word;
*/

struct ElfProgramHeader
{
	uchar		Magic[4];	// == { 0x7f, 'E', 'L', 'F' }
	uint8		Class;		// 1 == 32bit, 2 == 64bit
	uint8		DataEnc;	// 1 == LSB, 2 == MSB
	uint8		FileVer;	// 1 == current
	char		Pad[8];
	uint8		IdentLen;	// size of struct
	
	uint16		Type;
	uint16		Machine;	// > 0 AND <= 8
	uint32		Version;	// == 1
	uint32		Entry;		// Entry point or 0 if none
	uint32		PhOff;
	uint32		ShOff;
	uint32		Flags;
	uint16		EhSize;
	uint16		PhEntSize;
	uint16		PhNum;
	uint16		ShEntSize;
	uint16		ShNum;
	uint16		ShstRndx;
};

class GExecuteCheck
{
	char *Name;
	bool OwnFile;
	GStreamI *File;
	int64 Start, Len;

	int64 GetPos()
	{
		if (File)
		{
			return File->GetPos() - Start;
		}

		return -1;
	}

	bool SetPos(int64 to)
	{
		if (File)
		{
			int64 o = Start + to;
			if (File->SetPos(o) == o)
			{
				int64 p = GetPos();
				return p >= 0 && p < Len;
			}
		}

		return false;
	}

	bool Read(void *d, int l)
	{
		if (File)
		{
			return File->Read(d, l) == l;
		}

		return false;
	}

	bool PeCheck()
	{
		char Buf[16];
		
		// Check stub
		if (SetPos(0) &&
			Read(Buf, 2) &&
			Buf[0] == 'M' &&
			Buf[1] == 'Z')
		{
			int NewHeaderPos;

			// Move to PE header
			if (SetPos(0x3c) &&
				Read(&NewHeaderPos, 4) &&
				SetPos(NewHeaderPos))
			{
				#ifdef WIN32
				
				// Read header
				IMAGE_NT_HEADERS Header;
				if (Read(&Header, sizeof(Header)) &&
					Header.Signature == IMAGE_NT_SIGNATURE)
				{
					// Check header members
					if (Header.FileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
						Header.OptionalHeader.Magic == 0x010B &&
						Header.OptionalHeader.SizeOfCode > 0)
					{
						/// Looks pretty good..
						return true;
					}
				}
				
				#endif
			}
		}

		return false;
	}

	bool ElfCheck()
	{
		if (SetPos(0))
		{
			ElfProgramHeader h;
			Read(&h, sizeof(h));

			/*
			printf("Magic=%i,%c,%c,%c\n", h.Magic[0], h.Magic[1], h.Magic[2], h.Magic[3]);
			printf("Class=%i\n", h.Class);
			printf("DataEnc=%i\n", h.DataEnc);
			printf("FileVer=%i\n", h.FileVer);
			printf("IdentLen=%i\n", h.IdentLen);
			
			printf("Type=%i\n", h.Type);
			printf("Machine=%i\n", h.Machine);
			printf("Version=%i\n", h.Version);
			printf("Entry=%i\n", h.Entry);
			printf("PhOff=%i\n", h.PhOff);
			printf("ShOff=%i\n", h.ShOff);
			printf("Flags=%i\n", h.Flags);
			printf("EhSize=%i\n", h.EhSize);
			printf("PhEntSize=%i\n", h.PhEntSize);
			printf("PhNum=%i\n", h.PhNum);
			printf("ShEntSize=%i\n", h.ShEntSize);
			printf("ShNum=%i\n", h.ShNum);
			printf("ShstRndx=%i\n", h.ShstRndx);
			*/
			
			if (h.Magic[0] == 0x7f &&
				h.Magic[1] == 'E' &&
				h.Magic[2] == 'L' &&
				h.Magic[3] == 'F' &&
				(h.Class == 1 OR h.Class == 2) &&
				(h.DataEnc == 1 OR h.DataEnc == 2) &&
				h.FileVer == 1 &&
				(h.Machine > 0 && h.Machine <= 8) &&
				h.Version == 1)
			{
				return true;
			}				
		}

		return false;
	}

	bool ScriptCheck()
	{
		if (Name)
		{
			char *Ext = LgiGetExtension(Name);
			if (Ext)
			{
				#ifdef WIN32
				if (stricmp(Ext, "vbs") == 0 OR
					stricmp(Ext, "pif") == 0 OR
					stricmp(Ext, "bat") == 0)
				{
					return true;
				}
				#endif
			}
		}

		return false;
	}

public:
	GExecuteCheck(char *FileName)
	{
		Name = NewStr(FileName);
		OwnFile = true;
		Start = Len = 0;
		File = new GFile;
		if (File && FileName)
		{
			if (File->Open(FileName, O_READ))
			{
				Len = File->GetSize();
			}
		}
	}

	GExecuteCheck(char *name, GStreamI *f, int64 start, int64 len)
	{
		Name = NewStr(name);
		OwnFile = false;
		File = f;
		Start = start;
		Len = len < 0 ? f->GetSize() : len;
	}

	~GExecuteCheck()
	{
		DeleteArray(Name)
		if (OwnFile)
		{
			DeleteObj(File);
		}
	}

	bool IsExecutable()
	{
		return	PeCheck() OR
				ElfCheck() OR
				ScriptCheck();
	}
};

LgiFunc bool LgiIsFileNameExecutable(char *Str)
{
	GExecuteCheck c(Str);
	return c.IsExecutable();
}

LgiFunc bool LgiIsFileExecutable(char *name, GStreamI *f, int64 Start, int64 Len)
{
	GExecuteCheck c(name, f, Start, Len);
	return c.IsExecutable();
}
