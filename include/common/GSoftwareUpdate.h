#ifndef _SOFTWARE_UPDATE_H_
#define _SOFTWARE_UPDATE_H_

#include "LDateTime.h"

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
		LDateTime Date;
		bool Cancel;
		
		UpdateInfo()
		{
		    Cancel = false;
		}
	};

	GSoftwareUpdate(const char *SoftwareName, const char *UpdateUri, const char *ProxyUri, const char *OptionalTempPath = 0);
	virtual ~GSoftwareUpdate();

	bool CheckForUpdate(UpdateInfo &Info, GViewI *WithUi, bool IncBetas);
	bool ApplyUpdate(UpdateInfo &Info, bool DownloadOnly, GViewI *WithUi);
	char *GetErrorMessage();
};

#endif