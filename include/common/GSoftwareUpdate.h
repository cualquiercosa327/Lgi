#ifndef _SOFTWARE_UPDATE_H_
#define _SOFTWARE_UPDATE_H_

#include "GDateTime.h"

class GSoftwareUpdate
{
	struct GSoftwareUpdatePriv *d;

public:
	struct UpdateInfo
	{
		GAutoString Name;
		GAutoString Version;
		GAutoString Build;
		GAutoString Uri;
		GAutoString Proxy;
		GDateTime Date;
		bool Cancel;
		
		UpdateInfo()
		{
		    Cancel = false;
		}
	};

	GSoftwareUpdate(char *SoftwareName, char *UpdateUri, char *ProxyUri, char *OptionalTempPath = 0);
	virtual ~GSoftwareUpdate();

	bool CheckForUpdate(UpdateInfo &Info, GViewI *WithUi, bool IncBetas);
	bool ApplyUpdate(UpdateInfo &Info, bool DownloadOnly, GViewI *WithUi);
	char *GetErrorMessage();
};

#endif