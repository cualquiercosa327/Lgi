#include "Lgi.h"
#include "LgiResEdit.h"
#include "GTableLayout.h"
#include "GTextLabel.h"
#include "GEdit.h"
#include "GTextView3.h"

/////////////////////////////////////////////////////////////////////////////
enum Ids {
    IDC_STATIC = -1,
    IDC_NAME = 100,
    IDC_STYLE
};
class ResCssUi : public GTableLayout
{
    ResCss *Css;
    GEdit *Name;
    GTextView3 *Style;
    
public:
    ResCssUi(ResCss *css)
    {
        Css = css;
        int y = 0;
        GLayoutCell *c;
        
        if ((c = GetCell(0, y++)))
            c->Add(new GText(IDC_STATIC, 0, 0, -1, -1, "Name:"));

        if ((c = GetCell(0, y++)))
        {
            c->Add(Name = new GEdit(IDC_NAME, 0, 0, 80, 20, 0));
            Name->Name(Css->Name());
        }

        if ((c = GetCell(0, y++)))
            c->Add(new GText(IDC_STATIC, 0, 0, -1, -1, "Style:"));

        if ((c = GetCell(0, y++)))
        {
            c->Add(Style = new GTextView3(IDC_STYLE, 0, 0, 80, 20, 0));
            Style->Name(Css->Style);
            Style->Sunken(true);
        }
        
        InvalidateLayout();
    }
    
    ~ResCssUi()
    {
        Save();
        Css->Ui = NULL;
    }
    
    void Save()
    {
        Css->Name(Name->Name());
        Css->Style.Reset(NewStr(Style->Name()));
    }
};

/////////////////////////////////////////////////////////////////////////////
ResCss::ResCss(AppWnd *w, int type) : Resource(w, type)
{
    Name("Css Style");
    Ui = 0;
}

ResCss::~ResCss()
{
}

void ResCss::Create(GXmlTag *load, SerialiseContext *Ctx)
{
    if (load && Ctx)
    {
        Read(load, *Ctx);
    }
}

void ResCss::OnShowLanguages()
{
}

GView *ResCss::CreateUI()
{
    return Ui = new ResCssUi(this);
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
    return true;
}

bool ResCss::Read(GXmlTag *t, SerialiseContext &Ctx)
{
    if (!t->IsTag("style"))
        return false;
    
    Name(t->GetAttr("name"));
    Style.Reset(NewStr(t->Content));
    
    return true;
}

bool ResCss::Write(GXmlTag *t, SerialiseContext &Ctx)
{
    if (Ui)
        Ui->Save();
    
    t->SetTag("style");
    t->SetAttr("name", Name());
    t->Content = NewStr(Style);
    return true;
}
