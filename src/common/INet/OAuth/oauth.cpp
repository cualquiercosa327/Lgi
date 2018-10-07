#include "Lgi.h"
#include "oauth.h"

struct OAuthPriv
{
	GAutoString ClientId;
	GAutoString ClientSecret;
	GAutoString RedirectURI;
};

OAuth::OAuth(const char *ClientId, const char *ClientSecret, const char *RedirectURI)
{
	d = new OAuthPriv;
	d->ClientId.Reset(NewStr(ClientId));
	d->ClientSecret.Reset(NewStr(ClientSecret));
	d->RedirectURI.Reset(NewStr(RedirectURI));
}

OAuth::~OAuth()
{
	DeleteObj(d);
}

void OAuth::ObtainAccessToken()
{
}

void OAuth::SendAccessToken()
{
}

void OAuth::RefreshAccessToken()
{
}


