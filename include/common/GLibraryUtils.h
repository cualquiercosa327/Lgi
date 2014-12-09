#ifndef GFILTER_UTILS_H
#define GFILTER_UTILS_H

/*
#if defined(_DEBUG) && defined(WIN32)
#define BreakPoint	_asm { int 3 }
#else
#define BreakPoint	LgiAssert(0);
#endif
*/

#define MissingDynImport	LgiAssert(0);

#ifndef CALL_TYPE
#define CALL_TYPE			// __stdcall
#endif

#define DynFunc0(ret, func)								\
	ret func()											\
	{													\
		typedef ret (CALL_TYPE *p_##func)();			\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p();									\
		}												\
		MissingDynImport								\
		return 0;										\
	}

#define DynFunc1(ret, func, argtype, argname)			\
	ret func(argtype argname)							\
	{													\
		typedef ret (CALL_TYPE *p_##func)(argtype);		\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p(argname);							\
		}												\
		MissingDynImport								\
		return 0;										\
	}

#define DynFunc2(ret, func, argtype1, argname1, argtype2, argname2) \
	ret func(argtype1 argname1, argtype2 argname2)		\
	{													\
		typedef ret (CALL_TYPE *p_##func)(argtype1, argtype2);	\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p(argname1, argname2);				\
		}												\
		MissingDynImport								\
		return 0;										\
	}

#define DynFunc3(ret, func, argtype1, argname1,			\
							argtype2, argname2,			\
							argtype3, argname3)			\
	ret func(argtype1 argname1,							\
			 argtype2 argname2,							\
			 argtype3 argname3)							\
	{													\
		typedef ret (CALL_TYPE *p_##func)(argtype1,		\
								argtype2,				\
								argtype3);				\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p(argname1, argname2, argname3);		\
		}												\
		MissingDynImport								\
		return 0;										\
	}

#define DynFunc4(ret, func, argtype1, argname1,			\
							argtype2, argname2,			\
							argtype3, argname3,			\
							argtype4, argname4)			\
	ret func(argtype1 argname1,							\
			 argtype2 argname2,							\
			 argtype3 argname3,							\
			 argtype4 argname4)							\
	{													\
		typedef ret (CALL_TYPE *p_##func)(argtype1,				\
								argtype2,				\
								argtype3,				\
								argtype4);				\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p(argname1, argname2,				\
					 argname3, argname4);				\
		}												\
		MissingDynImport								\
		return 0;										\
	}

#define DynFunc5(ret, func, argtype1, argname1,			\
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
		typedef ret (CALL_TYPE *p_##func)(argtype1,				\
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
		MissingDynImport								\
		return 0;										\
	}

#define DynFunc6(ret, func, argtype1, argname1,			\
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
		typedef ret (CALL_TYPE *p_##func)(argtype1,		\
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
		MissingDynImport								\
		return 0;										\
	}


#define DynFunc9(ret, func, argtype1, argname1,		    \
							argtype2, argname2,			\
							argtype3, argname3,			\
							argtype4, argname4,			\
							argtype5, argname5,			\
							argtype6, argname6,			\
							argtype7, argname7,			\
							argtype8, argname8,			\
							argtype9, argname9)		    \
	ret func(argtype1 argname1,							\
			 argtype2 argname2,							\
			 argtype3 argname3,							\
			 argtype4 argname4,							\
			 argtype5 argname5,							\
			 argtype6 argname6,							\
			 argtype7 argname7,							\
			 argtype8 argname8,							\
			 argtype9 argname9)						    \
	{													\
		typedef ret (CALL_TYPE *p_##func)(argtype1,		\
								argtype2,				\
								argtype3,				\
								argtype4,				\
								argtype5,				\
								argtype6,				\
								argtype7,				\
								argtype8,				\
								argtype9);				\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p(argname1, argname2,				\
					 argname3, argname4,				\
					 argname5, argname6,				\
					 argname7, argname8,				\
					 argname9);				            \
		}												\
		MissingDynImport								\
		return 0;										\
	}

#define DynFunc10(ret, func, argtype1, argname1,		\
							argtype2, argname2,			\
							argtype3, argname3,			\
							argtype4, argname4,			\
							argtype5, argname5,			\
							argtype6, argname6,			\
							argtype7, argname7,			\
							argtype8, argname8,			\
							argtype9, argname9,			\
							argtype10, argname10)		\
	ret func(argtype1 argname1,							\
			 argtype2 argname2,							\
			 argtype3 argname3,							\
			 argtype4 argname4,							\
			 argtype5 argname5,							\
			 argtype6 argname6,							\
			 argtype7 argname7,							\
			 argtype8 argname8,							\
			 argtype9 argname9,							\
			 argtype10 argname10)						\
	{													\
		typedef ret (CALL_TYPE *p_##func)(argtype1,		\
								argtype2,				\
								argtype3,				\
								argtype4,				\
								argtype5,				\
								argtype6,				\
								argtype7,				\
								argtype8,				\
								argtype9,				\
								argtype10);				\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p(argname1, argname2,				\
					 argname3, argname4,				\
					 argname5, argname6,				\
					 argname7, argname8,				\
					 argname9, argname10);				\
		}												\
		MissingDynImport								\
		return 0;										\
	}

#endif
