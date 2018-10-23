#ifndef _LPERM_H_
#define _LPERM_H_

#include "LHashTable.h"

struct LPermissions
{
	struct _Win
	{
		uint32 ReadOnly : 1;	// FILE_ATTRIBUTE_READONLY
		uint32 Hidden : 1;
		uint32 System : 1;
		uint32 Reserved1 : 1;

		uint32 Folder : 1;		// FILE_ATTRIBUTE_DIRECTORY
		uint32 Archive : 1;
		uint32 Device : 1;
		uint32 Normal : 1;

		uint32 Temporary : 1;	// FILE_ATTRIBUTE_TEMPORARY
		uint32 Sparse : 1;
		uint32 ReparsePoint : 1;
		uint32 Compressed : 1;

		uint32 Offline : 1;		// FILE_ATTRIBUTE_OFFLINE
		uint32 ContentNotIndexed : 1;
		uint32 Encrypted : 1;
		uint32 IntegrityStream : 1;

		uint32 Virtual : 1;		// FILE_ATTRIBUTE_VIRTUAL
		uint32 NoScrub : 1;
		uint32 Ea : 1;
	};

	struct _Unix
	{
		uint32 GlobalExecute : 1;	// S_IXOTH
		uint32 GlobalWrite : 1;		// S_IWOTH
		uint32 GlobalRead : 1;		// S_IROTH
		uint32 GlobalReserved : 1;
		
		uint32 GroupExecute : 1;	// S_IXGRP
		uint32 GroupWrite : 1;		// S_IWGRP
		uint32 GroupRead : 1;		// S_IRGRP
		uint32 GroupReserved : 1;

		uint32 UserExecute : 1;		// S_IXUSR
		uint32 UserWrite : 1;		// S_IWUSR
		uint32 UserRead : 1;		// S_IRUSR
		uint32 UserReserved : 1;

		uint32 Sticky : 1;
		uint32 SetGid : 1;
		uint32 SetUid : 1;
	};

	bool IsWindows; // switch between Win and Unix members in the union
	union
	{
		uint32 u32;
		_Win Win;
		_Unix Unix;
	};

	LPermissions()
	{
		IsWindows = false;
		u32 = 0;
	}

	typedef LHashTbl<IntKey<uint32>,const char*> FlagMap;
	FlagMap &EnumFlags()
	{
		static FlagMap fWin, fUnix;
		FlagMap &m = IsWindows ? fWin : fUnix;
		if (m.Length() == 0)
		{
			for
			(
				auto s : GString
						(
							IsWindows
							?
							"0x1,ReadOnly\n0x2,Hidden\n0x4,System\n"
							"0x10,Folder\n0x20,Archive\n0x40,Device\n0x80,Normal\n"
							"0x100,Temporary\n0x200,Sparse\n0x400,ReparsePoint\n0x800,Compressed\n"
							"0x1000,Offline\n0x2000,ContentNotIndexed\n0x4000,Encrypted\n0x8000,IntegrityStream\n"
							"0x10000,Virtual\n0x20000,NoScrub\n0x40000,Ea\n"
							:
							"0x1,GlobalExecute\n0x2,GlobalWrite\n0x4,GlobalRead\n"
							"0x10,GroupExecute\n0x20,GroupWrite\n0x40,GroupRead\n"
							"0x100,UserExecute\n0x200,UserWrite\n0x400,UserRead\n"
							"0x1000,Sticky\n0x2000,SetGid\n0x4000,SetUid"
						).Split("\n")
			)
			{
				auto p = s.Split(",");
				fWin.Add(htoi(p[0].Get()),p[1]);
			}
		}
		return m;
	}

	bool UnitTest()
	{
		#ifdef WINDOWS
		u32 = FILE_ATTRIBUTE_READONLY;
		#else
		u32 = 1;
		#endif
		if (Win.System || !Win.ReadOnly || Win.Hidden) return false;
		#ifdef WINDOWS
		u32 = FILE_ATTRIBUTE_HIDDEN;
		if (Win.System || Win.ReadOnly || !Win.Hidden) return false;
		u32 = FILE_ATTRIBUTE_SYSTEM;
		if (!Win.System || Win.ReadOnly || Win.Hidden) return false;
		#endif

		#ifndef WINDOWS
		u32 = S_IRUSR;
		#else
		u32 = 0x400;
		#endif
		if (!Unix.UserRead || Unix.UserWrite || Unix.UserExecute) return false;

		return true;
	}
};

#endif