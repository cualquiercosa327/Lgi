#ifndef _OAUTH_H_
#define _OAUTH_H_

class OAuth
{
	struct OAuthPriv *d;

public:
	OAuth(const char *ClientId, const char *ClientSecret, const char *RedirectURI);
	virtual ~OAuth();
	
	void ObtainAccessToken();
	void SendAccessToken();
	void RefreshAccessToken();
};

#endif