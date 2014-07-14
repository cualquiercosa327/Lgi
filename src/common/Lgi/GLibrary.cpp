#include <stdio.h>

#include "Lgi.h"
#if defined(LINUX)||defined(MAC)
#include <dlfcn.h>
#endif
#include "GToken.h"

GLibrary::GLibrary(const char *File)
{
	FileName = 0;
	hLib = 0;
	if (File)
	{
		Load(File);	
	}
}

GLibrary::~GLibrary()
{
	Unload();
}

bool GLibrary::Load(const char *File)
{
	Unload();

	if (File)
	{
		char f[MAX_PATH];
		int ch = sprintf_s(f, sizeof(f), "%s", File);
		
		// check for default extension..
		// if you specify no extension in the application this will correctly set
		// it for the OS your running thus removing the need for #ifdef's in your
		// app. sweet!
		char *Dot = strrchr(f, '.');
		if (!Dot || strchr(Dot, DIR_CHAR))
		{
			ch += sprintf_s(f+ch, sizeof(f)-ch, ".%s", LGI_LIBRARY_EXT);
		}

		int Len = strlen(f) + 16;
		FileName = new char[Len];
		if (FileName)
		{
			memset(FileName, 0, Len);
			strcpy(FileName, f); // FileName is always longer then 'f'
			
			#if defined WIN32

			SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);
			if (LgiGetOs() == LGI_OS_WIN9X)
			{
				GAutoString a(LgiToNativeCp(f));
				if (a)
					hLib = LoadLibraryA(a);
			}
			else
			{
				GAutoWString w(LgiNewUtf8To16(f));
				if (w)
				{
					hLib = LoadLibraryW(w);
				}
			}
			SetErrorMode(0);

			#ifdef _DEBUG
			if (!hLib)
			{
				DWORD err = GetLastError();
				int asd=0;
			}
			#endif

			#elif defined(LINUX) || defined(MAC) || defined(BEOS)
			
			char *f = strrchr(FileName, DIR_CHAR);
			if (f) f++;
			else f = FileName;
			bool IsLgi = stricmp(f, "lgilgi." LGI_LIBRARY_EXT) == 0 ||
						 stricmp(f, "lgilgid." LGI_LIBRARY_EXT) == 0;			
			if (!IsLgi) // Bad things happen when we load LGI again.
			{
				#ifdef BEOS
				hLib = dlopen(FileName);
				#else
				hLib = dlopen(FileName, RTLD_NOW);
				#endif
				if (!hLib)
				{
					char *e = dlerror();
					if (!stristr(e, "No such file or directory"))
						printf("GLibrary::Load(\"%s\") failed.\n\t%s\n", File, e);

					GToken t("/opt/local/lib", ":");
					printf("DYLD_LIBRARY_PATH=%i paths\n", t.Length());
					for (int i=0; i<t.Length(); i++)
					{
						char full[MAX_PATH];
						LgiMakePath(full, sizeof(full), t[i], f);
						if (FileExists(full))
						{
							#ifdef BEOS
							hLib = dlopen(full);
							#else
							hLib = dlopen(full, RTLD_NOW);
							#endif
							printf("dlopen(%s)=%p\n", full, hLib);
							if (hLib)
								break;
						}
						else
						{
							printf("%s doesn't exist\n", full);
						}

					}

					if (!hLib)
					{
						char *e = dlerror();
						if (!stristr(e, "No such file or directory"))
							printf("GLibrary::Load(\"%s\") failed.\n\t%s\n", File, e);
					}
				}
			}

			#else

			LgiAssert(0);

			#endif
		}
	}

	return hLib != 0;
}

bool GLibrary::Unload()
{
	if (hLib)
	{
		#if defined WIN32
		FreeLibrary(hLib);
		/*
		#elif defined BEOS
		unload_add_on(hLib);
		*/
		#elif defined(LINUX) || defined(MAC) || defined(BEOS)
		dlclose(hLib);
		#else
		LgiAssert(0);
		#endif
		hLib = 0;
	}

	DeleteArray(FileName);

	return true;
}

void *GLibrary::GetAddress(const char *Resource)
{
	void *p = 0;
	
	if (Resource && hLib)
	{
		#if defined WIN32
		p = (void*) GetProcAddress(hLib, Resource);
		/*
		#elif defined BEOS
		get_image_symbol(hLib, Resource, B_SYMBOL_TYPE_TEXT, (void**) &p);
		*/
		#elif defined(LINUX) || defined(MAC) || defined(BEOS)
		p = dlsym(hLib, Resource);
		#else
		LgiAssert(0);
		#endif
		
		if (!p)
		{
			// printf("GLibrary::GetAddress(%s) failed\n", Resource);
		}
	}

	return p;
}

