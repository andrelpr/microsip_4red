/*
 * [fork] Tela de conta simplificada: Ramal, Senha e (opcional) Servidor.
 * Se houver URL de provisionamento, busca a configuracao completa no servidor;
 * se o provisionamento falhar e o servidor SIP estiver preenchido, configura manualmente.
 */

#pragma once

#include "ThemeDialog.h"
#include "define.h"
#include "resource.h"
#include "settings.h"

class SimpleAccountDlg :
    public CThemeDialog
{
public:
    SimpleAccountDlg(CWnd* pParent = NULL);
    ~SimpleAccountDlg();
    enum { IDD = IDD_ACCOUNT_SIMPLE };

private:
    bool m_provisioning;
    void SetStatus(const CString& text);
    void SaveManual(const CString& ext, const CString& password, const CString& server);

protected:
    virtual BOOL OnInitDialog();
    virtual void PostNcDestroy();
    afx_msg void OnDestroy();
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnClose();
    afx_msg void OnBnClickedOk();
    afx_msg void OnBnClickedCancel();
    afx_msg void OnBnClickedAdvanced();
    afx_msg void OnPasswordChange();
    afx_msg void OnNMClickShowPassword(NMHDR* pNMHDR, LRESULT* pResult);
    DECLARE_MESSAGE_MAP()
};
