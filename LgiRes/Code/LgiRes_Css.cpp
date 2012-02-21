#include "Lgi.h"
#include "LgiResEdit.h"

class ResCssUi : public GLayout
{
};

ResCss::ResCss(AppWnd *w, int type) : Resource(w, type)
{
    Name("Css Style");
}

ResCss::~ResCss()
{
}

void ResCss::Create(GXmlTag *load)
{
}

void ResCss::OnShowLanguages()
{
}

GView *ResCss::CreateUI()
{
    return 0;
}

void ResCss::OnRightClick(GSubMenu *RClick)
{
}

void ResCss::OnCommand(int Cmd)
{
}

int ResCss::OnCommand(int Cmd, int Event, OsView hWnd)
{
    return false;
}

bool ResCss::Test(ErrorCollection *e)
{
    return false;
}

bool ResCss::Read(GXmlTag *t, ResFileFormat Format)
{
    return false;
}

bool ResCss::Write(GXmlTag *t, ResFileFormat Format)
{
    return false;
}
