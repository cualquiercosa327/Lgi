#ifndef _HTTP_UI_H_
#define _HTTP_UI_H_

#include "GOptionsFile.h"

class GHttpCallback
{
public:
	virtual ~GHttpCallback() {}

	char *FormDecode(char *s);
	char *HtmlEncode(char *s);
	bool ParseHtmlWithDom(GVariant &Out, GDom *Dom, char *Html);

	virtual int OnRequest(char *Action, char *Uri, char *Headers, char *Body, GVariant &Out) = 0;
};

class GHttpServer
{
	struct GHttpServerPriv *d;

public:
	GHttpServer(GHttpCallback *cb, int port);
	~GHttpServer();
};

#endif