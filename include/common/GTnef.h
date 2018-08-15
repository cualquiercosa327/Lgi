#ifndef _TNEF_H_
#define _TNEF_H_

class TnefFileInfo
{
public:
	char *Name;
	int64 Start;
	int64 Size;

	TnefFileInfo()
	{
		Name = 0;
		Start = Size = 0;
	}

	~TnefFileInfo()
	{
		DeleteArray(Name);
	}
};

bool TnefReadIndex(GStreamI *Tnef, GArray<TnefFileInfo*> &Index);
bool TnefExtract(GStreamI *Tnef, GStream *Out, TnefFileInfo *File);

#endif