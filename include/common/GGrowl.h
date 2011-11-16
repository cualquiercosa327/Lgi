#ifndef _GGROWL_H_
#define _GGROWL_H_

class LgiClass GGrowl
{
	class GGrowlPriv *d;
	
public:
	struct GNotifyType
	{
		GAutoString Name;
		GAutoString DisplayName; // Optional
		bool Enabled;
		GAutoString IconUrl;
		
		GNotifyType()
		{
			Enabled = true;
		}
	};

	struct GRegister
	{
		GAutoString App;
		GAutoString IconUrl; // Optional
		GArray<GNotifyType> Types;
	};

	struct GNotify
	{
		GAutoString Name;
		GAutoString Title;
		GAutoString Id; // Optional
		GAutoString Text; // Optional
		bool Sticky; // Optional
		int Priority; // Optional
		GAutoString IconUrl; // Optional
		GAutoString CoalescingID; // Optional
		GAutoString CallbackData; // Optional
		GAutoString CallbackDataType; // Optional
		GAutoString CallbackTarget; // Optional
		
		GNotify()
		{
			Sticky = false;
			Priority = 0;
		}
	};


	GGrowl(bool async = true);
	~GGrowl();

	bool Register(GAutoPtr<GRegister> r);
	bool Notify(GAutoPtr<GNotify> n);
};

#endif