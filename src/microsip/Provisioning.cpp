/*
 * [fork] Provisionamento centralizado. Veja Provisioning.h e docs/provisionamento.md
 */

#include "stdafx.h"
#include "Provisioning.h"
#include "settings.h"
#include "global.h"
#include "json.h"
#include <atlenc.h>

#define THIS_FILENAME "Provisioning.cpp"

// definido em settings.cpp
CString IniEncrypt(CString str);

// URL padrao embutida na compilacao (opcional). O instalador/ini pode sobrescrever.
#ifndef _FORK_PROVISIONING_URL
#define _FORK_PROVISIONING_URL ""
#endif

#define PROV_SECTION _T("Provisioning")
#define PROV_DEFAULT_INTERVAL 21600
#define PROV_MIN_INTERVAL 300

namespace {

    CString IniGet(LPCTSTR key, LPCTSTR def = _T(""))
    {
        CString str;
        LPTSTR ptr = str.GetBuffer(2048);
        GetPrivateProfileString(PROV_SECTION, key, def, ptr, 2048, accountSettings.iniFile);
        str.ReleaseBuffer();
        return str;
    }

    // provisioning.ini na pasta do programa: configuracao comum a todos os usuarios do Windows
    CString MachineIniGet(LPCTSTR key, LPCTSTR def = _T(""))
    {
        CString path = accountSettings.exeFile;
        int pos = path.ReverseFind(_T('\\'));
        if (pos != -1) {
            path = path.Left(pos + 1);
        }
        path += _T("provisioning.ini");
        CString str;
        LPTSTR ptr = str.GetBuffer(2048);
        GetPrivateProfileString(PROV_SECTION, key, def, ptr, 2048, path);
        str.ReleaseBuffer();
        return str;
    }

    void IniSet(LPCTSTR key, const CString& value)
    {
        WritePrivateProfileString(PROV_SECTION, key, value, accountSettings.iniFile);
    }

    CString JsonToIniValue(const Json::Value& v)
    {
        CString res;
        if (v.isString()) {
            res = MSIP::Utf8DecodeUni(v.asCString());
        }
        else if (v.isBool()) {
            res = v.asBool() ? _T("1") : _T("0");
        }
        else if (v.isInt()) {
            res.Format(_T("%d"), v.asInt());
        }
        else if (v.isUInt()) {
            res.Format(_T("%u"), v.asUInt());
        }
        else if (v.isDouble()) {
            res.Format(_T("%g"), v.asDouble());
        }
        return res;
    }

    // chaves que executam programas no computador do usuario
    bool IsCommandKey(const CString& key)
    {
        return key.Left(3).CompareNoCase(_T("cmd")) == 0;
    }

    // chaves que nao fazem sentido vir do servidor
    bool IsBlockedSettingsKey(const CString& key)
    {
        return key.CompareNoCase(_T("accountId")) == 0
            || key.CompareNoCase(_T("portable")) == 0;
    }

    struct AsyncParams {
        HWND hWnd;
        UINT message;
        CString url;
        CString ext;
        CString token;
        CString password;
    };

    DWORD WINAPI FetchThread(LPVOID lpParam)
    {
        AsyncParams* p = (AsyncParams*)lpParam;
        Provisioning::Response* r = new Provisioning::Response();
        *r = Provisioning::Fetch(p->url, p->ext, CString(), p->token);
        // token revogado/expirado: tenta com a senha SIP atual
        if ((r->status == 401 || r->status == 403 || p->token.IsEmpty()) && !p->password.IsEmpty()) {
            *r = Provisioning::Fetch(p->url, p->ext, p->password, CString());
        }
        if (!::PostMessage(p->hWnd, p->message, (WPARAM)r, 0)) {
            delete r;
        }
        delete p;
        return 0;
    }
}

namespace Provisioning {

    Config Load()
    {
        Config c;
        // ordem: microsip.ini do usuario > provisioning.ini ao lado do .exe (gravado pelo instalador) > compilado
        c.url = IniGet(_T("url"));
        if (c.url.IsEmpty()) {
            c.url = MachineIniGet(_T("url"), _T(_FORK_PROVISIONING_URL));
        }
        c.url.Trim();
        c.url.TrimRight(_T('/'));
        c.ext = IniGet(_T("ext"));
        c.token = IniGet(_T("token"));
        c.hash = IniGet(_T("hash"));
        c.interval = _wtoi(IniGet(_T("interval")));
        if (c.interval <= 0) {
            c.interval = PROV_DEFAULT_INTERVAL;
        }
        if (c.interval < PROV_MIN_INTERVAL) {
            c.interval = PROV_MIN_INTERVAL;
        }
        c.allowCommands = _wtoi(IniGet(_T("allowCommands"), _T("0"))) != 0;
        c.simpleAccount = _wtoi(IniGet(_T("simpleAccount"), _T("1"))) != 0;
        return c;
    }

    bool IsEnabled()
    {
        return !Load().url.IsEmpty();
    }

    bool IsSimpleAccount()
    {
        return Load().simpleAccount;
    }

    Response Fetch(const CString& baseUrl, const CString& ext, const CString& password, const CString& token)
    {
        Response r;
        r.status = 0;
        if (baseUrl.IsEmpty() || ext.IsEmpty()) {
            r.error = _T("URL de provisionamento ou ramal vazio");
            return r;
        }

        CString url = baseUrl + _T("/") + CString(urlencode(MSIP::Utf8EncodeUni(ext)));

        CString headers = _T("Accept: application/json\r\n");
        if (!password.IsEmpty()) {
            CStringA cred = MSIP::Utf8EncodeUni(ext + _T(":") + password);
            int len = Base64EncodeGetRequiredLength(cred.GetLength(), ATL_BASE64_FLAG_NOCRLF);
            CStringA b64;
            Base64Encode((const BYTE*)cred.GetString(), cred.GetLength(), b64.GetBuffer(len + 1), &len, ATL_BASE64_FLAG_NOCRLF);
            b64.ReleaseBuffer(len);
            headers.AppendFormat(_T("Authorization: Basic %s\r\n"), CString(b64));
        }
        else if (!token.IsEmpty()) {
            headers.AppendFormat(_T("Authorization: Bearer %s\r\n"), token);
        }
        headers.AppendFormat(_T("X-MicroSIP-Version: %s (%s)\r\n"), _T(_GLOBAL_VERSION), _T(_FORK_VERSION));
        TCHAR computerName[MAX_COMPUTERNAME_LENGTH + 1] = {};
        DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
        if (GetComputerName(computerName, &size)) {
            headers.AppendFormat(_T("X-Device-Name: %s\r\n"), computerName);
        }

        try {
            DWORD dwServiceType;
            CString strServer, strObject;
            INTERNET_PORT nPort;
            if (!AfxParseURL(url, dwServiceType, strServer, strObject, nPort)
                || (dwServiceType != AFX_INET_SERVICE_HTTPS && dwServiceType != AFX_INET_SERVICE_HTTP)) {
                r.error = _T("URL de provisionamento invalida");
                return r;
            }
            bool https = dwServiceType == AFX_INET_SERVICE_HTTPS;
            CInternetSession session(_T("MicroSIP-Provisioning"));
            session.SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, 10000);
            session.SetOption(INTERNET_OPTION_RECEIVE_TIMEOUT, 15000);
            CHttpConnection* pHttp = session.GetHttpConnection(strServer, https ? INTERNET_FLAG_SECURE : 0, nPort);
            // ATENCAO: diferente do restante do MicroSIP, aqui o certificado TLS E validado,
            // pois a resposta contem a senha SIP.
            CHttpFile* pFile = pHttp->OpenRequest(CHttpConnection::HTTP_VERB_GET, strObject, NULL, 1, NULL, NULL,
                INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_NO_COOKIES
                | INTERNET_FLAG_NO_AUTH | (https ? INTERNET_FLAG_SECURE : 0));
            if (pFile->SendRequest(headers)) {
                pFile->QueryInfoStatusCode(r.status);
                char buf[4096];
                UINT n;
                while ((n = pFile->Read(buf, sizeof(buf))) > 0) {
                    r.body.Append(buf, n);
                    if (r.body.GetLength() > 1024 * 1024) {
                        break; // resposta absurda
                    }
                }
            }
            pFile->Close();
            delete pFile;
            pHttp->Close();
            delete pHttp;
            session.Close();
        }
        catch (CInternetException* e) {
            TCHAR msg[512] = {};
            e->GetErrorMessage(msg, 512);
            r.error = msg;
            r.status = 0;
            e->Delete();
        }
        PJ_LOG(3, (THIS_FILENAME, "Provisioning GET ext=%s status=%u", (LPCSTR)MSIP::Utf8EncodeUni(ext), r.status));
        return r;
    }

    CString ContentHash(const CStringA& body)
    {
        Json::Value root;
        Json::Reader reader;
        CStringA canonical;
        if (reader.parse((LPCSTR)body, root, false) && root.isObject()) {
            Json::FastWriter writer;
            canonical = writer.write(root["account"]).c_str();
            canonical += writer.write(root["settings"]).c_str();
        }
        else {
            canonical = body;
        }
        // FNV-1a 64 bits: suficiente para detectar mudanca
        unsigned long long h = 1469598103934665603ULL;
        for (int i = 0; i < canonical.GetLength(); i++) {
            h ^= (unsigned char)canonical[i];
            h *= 1099511628211ULL;
        }
        CString res;
        res.Format(_T("%016llx"), h);
        return res;
    }

    bool Apply(const CStringA& body, const CString& ext, CString& errorMsg)
    {
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse((LPCSTR)body, root, false) || !root.isObject()) {
            errorMsg = _T("Resposta do servidor de provisionamento nao e um JSON valido");
            return false;
        }
        const Json::Value& account = root["account"];
        if (!account.isObject() || !account.isMember("username")) {
            errorMsg = _T("Resposta do servidor sem a conta (account.username)");
            return false;
        }
        Config cfg = Load();
        const CString& ini = accountSettings.iniFile;

        // --- conta: substitui a [Account1] inteira
        WritePrivateProfileString(_T("Account1"), NULL, NULL, ini);
        Json::Value::Members keys = account.getMemberNames();
        for (size_t i = 0; i < keys.size(); i++) {
            CString key = MSIP::Utf8DecodeUni(keys[i].c_str());
            CString value = JsonToIniValue(account[keys[i]]);
            if (key.CompareNoCase(_T("password")) == 0) {
                value = IniEncrypt(value);
            }
            WritePrivateProfileString(_T("Account1"), key, value, ini);
        }

        // --- configuracoes gerais: grava apenas as chaves enviadas
        const Json::Value& settings = root["settings"];
        if (settings.isObject()) {
            keys = settings.getMemberNames();
            for (size_t i = 0; i < keys.size(); i++) {
                CString key = MSIP::Utf8DecodeUni(keys[i].c_str());
                if (IsBlockedSettingsKey(key)) {
                    continue;
                }
                if (IsCommandKey(key) && !cfg.allowCommands) {
                    PJ_LOG(3, (THIS_FILENAME, "Provisioning: ignoring command key %s (allowCommands=0)", keys[i].c_str()));
                    continue;
                }
                const Json::Value& v = settings[keys[i]];
                if (v.isNull()) {
                    WritePrivateProfileString(_T("Settings"), key, NULL, ini);
                }
                else {
                    WritePrivateProfileString(_T("Settings"), key, JsonToIniValue(v), ini);
                }
            }
        }
        WritePrivateProfileString(_T("Settings"), _T("accountId"), _T("1"), ini);

        // --- estado do provisionamento
        IniSet(_T("ext"), ext);
        if (root["token"].isString()) {
            IniSet(_T("token"), MSIP::Utf8DecodeUni(root["token"].asCString()));
        }
        if (root["refresh"].isInt() && root["refresh"].asInt() > 0) {
            CString str;
            str.Format(_T("%d"), root["refresh"].asInt());
            IniSet(_T("interval"), str);
        }
        if (root["provisioningUrl"].isString()) {
            // permite ao servidor migrar os clientes para outra URL
            IniSet(_T("url"), MSIP::Utf8DecodeUni(root["provisioningUrl"].asCString()));
        }
        IniSet(_T("hash"), ContentHash(body));

        // --- recarrega em memoria para que o SettingsSave() do fechamento nao sobrescreva
        accountSettings.SettingsLoad();
        accountSettings.accountId = 1;
        accountSettings.AccountLoad(1, &accountSettings.account);
        return true;
    }

    void FetchAsync(HWND hWnd, UINT message)
    {
        Config cfg = Load();
        if (cfg.url.IsEmpty() || cfg.ext.IsEmpty()) {
            return;
        }
        AsyncParams* p = new AsyncParams();
        p->hWnd = hWnd;
        p->message = message;
        p->url = cfg.url;
        p->ext = cfg.ext;
        p->token = cfg.token;
        p->password = accountSettings.account.password;
        if (!CreateThread(NULL, 0, FetchThread, p, 0, NULL)) {
            delete p;
        }
    }
}
