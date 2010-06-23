// SCP - Simple Calendar Protocol
//
// As much as I hate inventing my own stuff, CAP is just
// way over complicated. I'm not spending a year of my
// life implementing some lame protocol, that hasn't got
// any implementations out there.
//
// If it ever becomes widely used then I'll drop this
// protocol like a hot potatoe
#ifndef _IScp_h_
#define _IScp_h_

// Includes
#include "INet.h"
#include "GDateTime.h"

// The standard port for SCP..
#define SCP_PORT				8010
#define SCP_HTTP_VER			100 // VER * 100
#define SCP_RETRIES				10
#define SCP_SOCKET_TIMEOUT		10	// seconds

enum ScpCommand
{
	SCP_NULL				= 0,
	SCP_RESPONSE			= 1,	// standard response from server to any request
	SCP_LOGIN				= 2,	// login request from client
	SCP_QUIT				= 3,	// quit request from client
	SCP_SEARCH				= 4,	// search request from client
	SCP_RESULT				= 5,	// search result from serve
	SCP_CREATE				= 6,
	SCP_LOAD				= 7,
	SCP_STORE				= 8,	// store data request from client
	SCP_DELETE				= 9, 	// delete data request from client
	SCP_POLL				= 10,	// get a list of changed objects
	SCP_NOT_SCP				= 11	// not an SCP request, can respond with HTML error message
};

enum ScpObject
{
	SCP_NONE				= 0,
	SCP_USER				= 1,
	SCP_EVENT				= 2
};

// Classes
class IScpData;
class IScpServer;

class IScpUsage
{
public:
	virtual char *GetUsage(IScpServer *s) { return 0; }
};

class IScp
{
	bool Server;

protected:
	GUri Proxy;
	GUri Host;
	GStringPipe Info;

	bool WriteData(GSocketI *&s, IScpData *d);
	bool ReadData(GSocketI *&s, IScpData *&d, int *HttpErr = 0);

public:
	IScp(bool server);
	virtual ~IScp();

	void SetProxy(char *s) { Proxy = s; }
	char *GetInfo() { return Info.NewStr(); }
};

class IScpSearch
{
public:
	int UserId;			// Owner of calendar
	ScpObject Type;		// Type of object to return
	GDateTime Start;	// Optional start time
	GDateTime End;		// Optional end time
	char *Param;		// Optional string parameter

	IScpSearch();
	~IScpSearch();
};

class IScpData
{
	friend class IScpClient;
	friend class IScpServer;
	friend class IScp;

	ScpCommand Cmd;
	int Uid;
	static List<IScpData> Uids;
	ObjProperties Props;

public:
	ScpObject Type;		// Object type
	int UserId;			// Owner
	char *Data;			// Type specific

	// Methods	
	IScpData(ScpCommand c = SCP_NULL);
	~IScpData();

	// Uid
	int GetUid() { return Uid; }
	void SetUid(int i) { Uid = i; }
	static IScpData *GetObject(int id);

	// Field Access
	char *GetStr(char *Field);
	void SetStr(char *Field, char *s);
	int GetInt(char *Field);
	void SetInt(char *Field, int i);
};

class IScpClient : public IScp
{
	GSocketI *s;
	int Session;

	GSocketI *&Socket(bool Open = true);
	bool Request(IScpData *out, IScpData *&in);

public:
	bool *Loop;

	IScpClient();
	~IScpClient();

	bool Connect(GSocketI *sock, char *Server, char *User, char *Pass);
	bool Quit();
	bool Search(IScpSearch *Search, IScpData *&Results);
	bool Load(int Uid, IScpData *Data);
	bool Save(IScpData *Data);
	bool Delete(IScpData *Data);
	bool Poll(List<int> &Changed);
};

class IScpServer : public IScp
{
	IScpUsage *Usage;

protected:
	virtual bool LoggedIn(int SeshId) { return false; }

	bool Respond(GSocketI *s, int Code);

public:
	IScpServer(IScpUsage *usage = 0);
	~IScpServer();

	bool OnIdle(GSocketI *s);

	virtual bool OnLogin(char *User, char *Pass, int Session) { return false; }
	virtual void OnQuit() { }
	virtual bool OnSearch(IScpSearch *Search, IScpData *Response) { return false; }
	virtual bool OnLoad(int Uid, IScpData *Data) { return false; }
	virtual int OnSave(IScpData *Data) { return false; }
	virtual bool OnDelete(IScpData *Data) { return false; }
	virtual bool OnPoll(IScpData *Data) { return false; }
};


#endif
