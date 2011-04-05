#ifndef __GLIBRARY_H
#define __GLIBRARY_H

// Os specific wrapper typedefs/defines
#ifdef WIN32

	// Win32
	typedef HMODULE _SysLibHandle;

#else

	// Unix-like systems

	#if defined BEOS
	typedef image_id _SysLibHandle;
	#elif defined(LINUX)||defined(MAC)
	typedef void *_SysLibHandle;
	#else // atheos/linux
	typedef int _SysLibHandle;
	#endif

#endif

// Generic shared library loader
class LgiClass GLibrary
{
	char *FileName;
	_SysLibHandle hLib;

public:
	GLibrary(const char *File = 0);
	virtual ~GLibrary();

	_SysLibHandle Handle() { return hLib; }
	char *GetFileName() { return FileName; }
	virtual bool IsLoaded() { return hLib != 0; }

	bool Load(const char *File);
	bool Unload();
	void *GetAddress(char *Resource);
};

// Runtime linking macros
#define GLibBreakPoint	LgiAssert(0);
#define GLibCallType	// __stdcall

#define GLibFunc0(ret, func)							\
	ret func()											\
	{													\
		typedef ret (GLibCallType *p_##func)();			\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p();									\
		}												\
		GLibBreakPoint									\
		return 0;										\
	}

#define GLibFunc1(ret, func, argtype, argname)			\
	ret func(argtype argname)							\
	{													\
		typedef ret (GLibCallType *p_##func)(argtype);	\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p(argname);							\
		}												\
		GLibBreakPoint									\
		return 0;										\
	}

#define GLibFunc2(ret, func, argtype1, argname1, argtype2, argname2) \
	ret func(argtype1 argname1, argtype2 argname2)		\
	{													\
		typedef ret (GLibCallType *p_##func)(argtype1, argtype2);	\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p(argname1, argname2);				\
		}												\
		GLibBreakPoint									\
		return 0;										\
	}

#define GLibFunc3(ret, func, argtype1, argname1,		\
							argtype2, argname2,			\
							argtype3, argname3)			\
	ret func(argtype1 argname1,							\
			 argtype2 argname2,							\
			 argtype3 argname3)							\
	{													\
		typedef ret (GLibCallType *p_##func)(argtype1,	\
								argtype2,				\
								argtype3);				\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p(argname1, argname2, argname3);		\
		}												\
		GLibBreakPoint									\
		return 0;										\
	}

#define GLibFunc4(ret, func, argtype1, argname1,		\
							argtype2, argname2,			\
							argtype3, argname3,			\
							argtype4, argname4)			\
	ret func(argtype1 argname1,							\
			 argtype2 argname2,							\
			 argtype3 argname3,							\
			 argtype4 argname4)							\
	{													\
		typedef ret (GLibCallType *p_##func)(argtype1,	\
								argtype2,				\
								argtype3,				\
								argtype4);				\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p(argname1, argname2,				\
					 argname3, argname4);				\
		}												\
		GLibBreakPoint									\
		return 0;										\
	}

#define GLibFunc5(ret, func, argtype1, argname1,		\
							argtype2, argname2,			\
							argtype3, argname3,			\
							argtype4, argname4,			\
							argtype5, argname5)			\
	ret func(argtype1 argname1,							\
			 argtype2 argname2,							\
			 argtype3 argname3,							\
			 argtype4 argname4,							\
			 argtype5 argname5)							\
	{													\
		typedef ret (GLibCallType *p_##func)(argtype1,	\
								argtype2,				\
								argtype3,				\
								argtype4,				\
								argtype5);				\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p(argname1, argname2,				\
					 argname3, argname4,				\
					 argname5);							\
		}												\
		GLibBreakPoint									\
		return 0;										\
	}

#define GLibFunc6(ret, func, argtype1, argname1,		\
							argtype2, argname2,			\
							argtype3, argname3,			\
							argtype4, argname4,			\
							argtype5, argname5,			\
							argtype6, argname6)			\
	ret func(argtype1 argname1,							\
			 argtype2 argname2,							\
			 argtype3 argname3,							\
			 argtype4 argname4,							\
			 argtype5 argname5,							\
			 argtype6 argname6)							\
	{													\
		typedef ret (GLibCallType *p_##func)(argtype1,	\
								argtype2,				\
								argtype3,				\
								argtype4,				\
								argtype5,				\
								argtype6);				\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p(argname1, argname2,				\
					 argname3, argname4,				\
					 argname5, argname6);				\
		}												\
		GLibBreakPoint									\
		return 0;										\
	}

#define GLibFunc7(ret, func, argtype1, argname1,		\
							argtype2, argname2,			\
							argtype3, argname3,			\
							argtype4, argname4,			\
							argtype5, argname5,			\
							argtype6, argname6,			\
							argtype7, argname7)			\
	ret func(argtype1 argname1,							\
			 argtype2 argname2,							\
			 argtype3 argname3,							\
			 argtype4 argname4,							\
			 argtype5 argname5,							\
			 argtype6 argname6,							\
			 argtype7 argname7)							\
	{													\
		typedef ret (GLibCallType *p_##func)(argtype1,	\
								argtype2,				\
								argtype3,				\
								argtype4,				\
								argtype5,				\
								argtype6,				\
								argtype7);				\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p(argname1, argname2,				\
					 argname3, argname4,				\
					 argname5, argname6, argname7);		\
		}												\
		GLibBreakPoint									\
		return 0;										\
	}

#endif
