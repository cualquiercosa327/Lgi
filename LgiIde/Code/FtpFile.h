#ifndef _FTP_FILE_H_
#define _FTP_FILE_H_

#include "LListItemCheckBox.h"

class FtpFile : public LListItem
{
	IFtpEntry *e;
	LListItemCheckBox *v;
	GAutoString Uri;
	
public:
	FtpFile(IFtpEntry *entry, char *uri)
	{
		Uri.Reset(NewStr(uri));
		e = entry;
		v = new LListItemCheckBox(this, 0);
	}

	char *GetText(int c)
	{
		static char buf[256];
		switch (c)
		{
			case 1:
			{
				return e->Name;
			}
			case 2:
			{
				LgiFormatSize(buf, sizeof(buf), e->Size);
				return buf;
			}
			case 3:
			{
				e->Date.Get(buf, sizeof(buf));
				return buf;
			}
		}

		return 0;
	}

	char *DetachUri()
	{
		if (v->Value())
		{
			return Uri.Release();
		}

		return 0;
	}
};

#endif