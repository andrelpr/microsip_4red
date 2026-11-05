/*
 * [fork] Provisionamento centralizado.
 *
 * Fluxo:
 *   1) Usuario informa ramal + senha (tela simplificada).
 *   2) GET {url}/{ramal}  com  Authorization: Basic base64(ramal:senha)
 *   3) Servidor responde JSON com conta, configuracoes e um token do dispositivo.
 *   4) Periodicamente: GET {url}/{ramal} com Authorization: Bearer {token}
 *      Se o conteudo mudou, aplica e reinicia o MicroSIP (somente sem chamadas).
 *
 * Estado fica na secao [Provisioning] do microsip.ini:
 *   url=https://prov.exemplo.com.br/microsip/v1/acme
 *   ext=1001
 *   token=...
 *   hash=...
 *   interval=21600
 *   allowCommands=0     (1 = permite o servidor definir chaves cmd*, que executam programas)
 *   simpleAccount=1     (0 = usa a tela de conta original do MicroSIP)
 *
 * Veja docs/provisionamento.md para o formato completo.
 */

#pragma once

#include "stdafx.h"

namespace Provisioning {

    struct Config {
        CString url;
        CString ext;
        CString token;
        CString hash;
        int interval;
        bool allowCommands;
        bool simpleAccount;
    };

    struct Response {
        DWORD status;      // codigo HTTP; 0 = falha de rede/TLS
        CStringA body;
        CString error;     // descricao da falha de rede, se houver
    };

    Config Load();
    bool IsEnabled();
    bool IsSimpleAccount();

    // Requisicao bloqueante. Se password vazio, usa o token salvo.
    Response Fetch(const CString& url, const CString& ext, const CString& password, const CString& token);

    // Hash do conteudo relevante (account + settings) para detectar mudancas.
    CString ContentHash(const CStringA& body);

    // Grava o JSON recebido no microsip.ini e recarrega as configuracoes em memoria.
    // Retorna false e preenche errorMsg se o JSON for invalido.
    bool Apply(const CStringA& body, const CString& ext, CString& errorMsg);

    // Atualizacao em segundo plano: executa Fetch em outra thread e envia
    // PostMessage(hWnd, message, (WPARAM)Response*, 0). Quem recebe faz delete.
    void FetchAsync(HWND hWnd, UINT message);
}
