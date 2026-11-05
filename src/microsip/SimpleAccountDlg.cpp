/*
 * [fork] Tela de conta simplificada. Veja SimpleAccountDlg.h
 */

#include "stdafx.h"
#include "SimpleAccountDlg.h"
#include "Provisioning.h"
#include "mainDlg.h"
#include "langpack.h"

SimpleAccountDlg::SimpleAccountDlg(CWnd* pParent /*=NULL*/)
    : CThemeDialog(SimpleAccountDlg::IDD, pParent)
{
    m_provisioning = false;
    if (!Create(IDD, pParent)) {
        AfxMessageBox(_T("Failed to create account window on your system"));
        exit(0);
    }
}

SimpleAccountDlg::~SimpleAccountDlg()
{
}

BEGIN_MESSAGE_MAP(SimpleAccountDlg, CThemeDialog)
    ON_WM_CREATE()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_BN_CLICKED(IDOK, &SimpleAccountDlg::OnBnClickedOk)
    ON_BN_CLICKED(IDCANCEL, &SimpleAccountDlg::OnBnClickedCancel)
    ON_BN_CLICKED(IDC_SIMPLE_ADVANCED, &SimpleAccountDlg::OnBnClickedAdvanced)
    ON_EN_CHANGE(IDC_SIMPLE_PASSWORD, &SimpleAccountDlg::OnPasswordChange)
    ON_NOTIFY(NM_CLICK, IDC_SIMPLE_SHOW_PASSWORD, &SimpleAccountDlg::OnNMClickShowPassword)
    ON_NOTIFY(NM_RETURN, IDC_SIMPLE_SHOW_PASSWORD, &SimpleAccountDlg::OnNMClickShowPassword)
END_MESSAGE_MAP()

int SimpleAccountDlg::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (langPack.rtl) {
        ModifyStyleEx(0, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT);
    }
    return 0;
}

BOOL SimpleAccountDlg::OnInitDialog()
{
    m_bAllowDark = true;
    CThemeDialog::OnInitDialog();
    TranslateDialog(this->m_hWnd);

    CString str;
    str.Format(_T("<a>%s</a>"), Translate(_T("display password")));
    GetDlgItem(IDC_SIMPLE_SHOW_PASSWORD)->SetWindowText(str);

    Provisioning::Config cfg = Provisioning::Load();
    m_provisioning = !cfg.url.IsEmpty();

    CString ext, password, server;
    if (accountSettings.accountId > 0) {
        ext = accountSettings.account.username;
        password = accountSettings.account.password;
        server = accountSettings.account.server;
    }
    if (!cfg.ext.IsEmpty()) {
        ext = cfg.ext;
    }
    GetDlgItem(IDC_SIMPLE_EXT)->SetWindowText(ext);
    GetDlgItem(IDC_SIMPLE_PASSWORD)->SetWindowText(password);
    GetDlgItem(IDC_SIMPLE_SERVER)->SetWindowText(server);
    // nao revela a senha ja salva: o link so aparece quando o usuario digita
    GetDlgItem(IDC_SIMPLE_SHOW_PASSWORD)->ShowWindow(password.IsEmpty() ? SW_SHOW : SW_HIDE);

    if (m_provisioning) {
        SetStatus(Translate(_T("Enter your extension and password. The other settings will be downloaded automatically. The SIP server is only needed if automatic setup fails.")));
    }
    else {
        SetStatus(_T(""));
    }

    GetDlgItem(ext.IsEmpty() ? IDC_SIMPLE_EXT : IDC_SIMPLE_PASSWORD)->SetFocus();
    return FALSE;
}

void SimpleAccountDlg::OnDestroy()
{
    mainDlg->simpleAccountDlg = nullptr;
    CThemeDialog::OnDestroy();
}

void SimpleAccountDlg::PostNcDestroy()
{
    CThemeDialog::PostNcDestroy();
    delete this;
}

void SimpleAccountDlg::OnClose()
{
    DestroyWindow();
}

void SimpleAccountDlg::OnBnClickedCancel()
{
    OnClose();
}

void SimpleAccountDlg::OnBnClickedAdvanced()
{
    int id = accountSettings.accountId > 0 ? accountSettings.accountId : -1;
    OnClose();
    mainDlg->OpenAccountDlgAdvanced(id);
}

void SimpleAccountDlg::OnPasswordChange()
{
    GetDlgItem(IDC_SIMPLE_SHOW_PASSWORD)->ShowWindow(SW_SHOW);
}

void SimpleAccountDlg::OnNMClickShowPassword(NMHDR* pNMHDR, LRESULT* pResult)
{
    GetDlgItem(IDC_SIMPLE_SHOW_PASSWORD)->ShowWindow(SW_HIDE);
    CEdit* edit = (CEdit*)GetDlgItem(IDC_SIMPLE_PASSWORD);
    edit->SetPasswordChar(0);
    edit->Invalidate();
    edit->SetFocus();
    *pResult = 0;
}

void SimpleAccountDlg::SetStatus(const CString& text)
{
    GetDlgItem(IDC_SIMPLE_STATUS)->SetWindowText(text);
    GetDlgItem(IDC_SIMPLE_STATUS)->UpdateWindow();
}

void SimpleAccountDlg::OnBnClickedOk()
{
    CString ext, password, server;
    GetDlgItem(IDC_SIMPLE_EXT)->GetWindowText(ext);
    GetDlgItem(IDC_SIMPLE_PASSWORD)->GetWindowText(password);
    GetDlgItem(IDC_SIMPLE_SERVER)->GetWindowText(server);
    ext.Trim();
    password.Trim(); // remove espacos colados por engano
    server.Trim();

    if (ext.IsEmpty() || password.IsEmpty()) {
        SetStatus(Translate(_T("Please enter the extension and the password.")));
        return;
    }
    if (mainDlg->messagesDlg->GetCallsCount(true)) {
        SetStatus(Translate(_T("Finish your calls before changing the account.")));
        return;
    }

    if (m_provisioning) {
        Provisioning::Config cfg = Provisioning::Load();
        SetStatus(Translate(_T("Connecting to the setup server...")));
        GetDlgItem(IDOK)->EnableWindow(FALSE);
        Provisioning::Response r;
        {
            CWaitCursor wait;
            r = Provisioning::Fetch(cfg.url, ext, password, CString());
        }
        GetDlgItem(IDOK)->EnableWindow(TRUE);

        if (r.status == 200) {
            CString error;
            if (Provisioning::Apply(r.body, ext, error)) {
                ShowWindow(SW_HIDE);
                mainDlg->PostMessage(UM_RESTART, 0, 0);
                OnClose();
                return;
            }
            SetStatus(error);
            return;
        }
        if (r.status == 401 || r.status == 403) {
            SetStatus(Translate(_T("Incorrect extension or password.")));
            return;
        }
        CString reason;
        if (r.status == 404) {
            reason = Translate(_T("Extension not found on the setup server."));
        }
        else if (r.status == 0) {
            reason.Format(_T("%s %s"), Translate(_T("Could not connect to the setup server.")), r.error);
        }
        else {
            reason.Format(_T("%s %u"), Translate(_T("The setup server returned an error:")), r.status);
        }
        if (server.IsEmpty()) {
            reason.AppendFormat(_T(" %s"), Translate(_T("Fill in the SIP server to configure manually.")));
            SetStatus(reason);
            GetDlgItem(IDC_SIMPLE_SERVER)->SetFocus();
            return;
        }
        // provisionamento falhou mas o servidor foi informado: segue com a configuracao manual
    }

    if (server.IsEmpty()) {
        SetStatus(Translate(_T("Please enter the SIP server.")));
        GetDlgItem(IDC_SIMPLE_SERVER)->SetFocus();
        return;
    }
    SaveManual(ext, password, server);
}

void SimpleAccountDlg::SaveManual(const CString& ext, const CString& password, const CString& server)
{
    int id = accountSettings.accountId > 0 ? accountSettings.accountId : 1;
    Account acc;
    // carrega a conta existente para preservar as opcoes avancadas (transporte, SRTP, etc.)
    accountSettings.AccountLoad(id, &acc);
    if (acc.server != server || acc.domain.IsEmpty()) {
        acc.domain = server;
    }
    if (acc.username != ext) {
        acc.authID.Empty();
    }
    acc.server = server;
    acc.username = ext;
    acc.password = password;
    acc.rememberPassword = 1;

    ShowWindow(SW_HIDE);
    accountSettings.AccountSave(id, &acc);

    mainDlg->PJAccountDelete(true);
    accountSettings.accountId = id;
    accountSettings.account = acc;
    accountSettings.AccountLoad(accountSettings.accountId, &accountSettings.account);
    mainDlg->OnAccountChanged();
    mainDlg->InitUI();
    accountSettings.SettingsSave();
    mainDlg->PJAccountAdd();
    OnClose();
}
