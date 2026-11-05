# Provisionamento centralizado

O MicroSIP personalizado busca toda a configuração (conta SIP, botões, codecs, etc.)
em um servidor HTTPS seu. O usuário só digita **ramal e senha**.

## Rota

```
GET https://prov.suaempresa.com.br/microsip/v1/<tenant>/<ramal>
```

- `<tenant>`: identificador do cliente (ex.: `acme`). Um arquivo `data/acme.json` no servidor.
- `<ramal>`: ramal digitado pelo usuário.

A **URL base** que o MicroSIP usa é `https://prov.suaempresa.com.br/microsip/v1/<tenant>`.
O programa acrescenta `/<ramal>`.

### Autenticação

| Situação | Header enviado |
|---|---|
| Primeiro acesso (tela "Configurar ramal") | `Authorization: Basic base64(ramal:senhaSIP)` |
| Atualizações periódicas | `Authorization: Bearer <token>` (recebido na 1ª resposta) |
| Token recusado (401/403) | volta a usar `Basic` com a senha SIP salva |

Headers informativos: `X-MicroSIP-Version`, `X-Device-Name` (nome do computador).

**O certificado TLS é validado.** Use um certificado válido (ex.: Let's Encrypt). Certificado
autoassinado faz o provisionamento falhar.

### Respostas

| Código | Significado | O que o MicroSIP faz |
|---|---|---|
| 200 | JSON de configuração | grava no `microsip.ini` e reinicia |
| 401/403 | ramal/senha (ou token) inválidos | mostra "Ramal ou senha incorretos" |
| 404 | tenant/ramal não existe | mostra a mensagem e permite configurar manualmente |
| 429/5xx/rede | indisponível | tela: permite configurar manualmente se o servidor SIP for preenchido; em segundo plano: tenta de novo em 5 min |

### Corpo da resposta 200

```json
{
  "account": {
    "label": "ACME",
    "server": "pbx.acme.com.br",
    "domain": "pbx.acme.com.br",
    "username": "1001",
    "authID": "1001",
    "password": "senha-sip",
    "displayName": "Recepção",
    "transport": "udp",
    "voicemailNumber": "*97"
  },
  "settings": {
    "denyIncoming": "",
    "autoAnswer": "",
    "recordingButton": 0,
    "audioCodecs": "PCMA/8000/1 PCMU/8000/1 G722/16000/1"
  },
  "token": "string estável por ramal",
  "refresh": 21600
}
```

- **`account`** substitui inteira a seção `[Account1]` do `microsip.ini`. As chaves são as do próprio
  MicroSIP: `label server proxy domain username authID password displayName dialingPrefix dialPlan
  hideCID voicemailNumber transport publicAddr SRTP registerRefresh keepAlive publish ICE allowRewrite
  disableSessionTimer`.
- **`settings`** grava só as chaves enviadas em `[Settings]`. As preferências locais do usuário
  (dispositivos de áudio, volume, posição da janela) são mantidas. Um valor `null` apaga a chave.
- **`token`** precisa ser **estável**. A mudança é detectada comparando `account` + `settings`, não o
  token, mas um token diferente a cada resposta não faz sentido.
- **`refresh`** é o intervalo em segundos entre as verificações (mínimo 300).
- **`provisioningUrl`** é opcional e migra o cliente para outra URL.

Quando o conteúdo de `account` + `settings` muda, o MicroSIP aplica e **reinicia sozinho**, mas
**só sem chamadas em andamento**. Se houver chamada, ele tenta de novo a cada minuto.

### Chaves úteis em `settings`

| Chave | Valores | Efeito |
|---|---|---|
| `denyIncoming` | `""` / `"button"` | esconde / mostra o botão **DND** |
| `autoAnswer` | `""` / `"button"` | esconde / mostra o botão **Atendimento automático** |
| `forwarding` | `""` / `"button"` | botão de desvio (exige `forwardingNumber`) |
| `buttonCONF` | 0/1 | botão CONF no discador |
| `buttonAC` | 0/1 | botão de conferência automática |
| `recordingButton` | 0/1 | botão REC |
| `enableFeatureCodeAT`, `featureCodeAT` | 1, `"*2"` | "Consultar" usa o atxfer do Asterisk em vez de SIP REFER |
| `enableFeatureCodeBT`, `featureCodeBT` | 1, `"##"` | "Transferir" usa o blindxfer do Asterisk |
| `audioCodecs` | ex. `"PCMA/8000/1 PCMU/8000/1"` | codecs, em ordem de preferência |
| `singleMode` | 0/1 | modo de chamada única (a transferência com consulta via SIP exige 0) |
| `cmd*` (`cmdIncomingCall`...) | — | **ignoradas por padrão** (executam programas). Para liberar: `allowCommands=1` no `[Provisioning]` local |

`accountId` e `portable` vindos do servidor são sempre ignorados.

## Onde o MicroSIP lê a URL

Em ordem de prioridade:

1. `%APPDATA%\MicroSIP\microsip.ini`, seção `[Provisioning]`, chave `url`
2. `provisioning.ini` na pasta do programa (gravado pelo instalador: `Setup.exe /S /URL=...`)
3. URL embutida na compilação (variável `FORK_PROVISIONING_URL` do GitHub Actions)

Estado salvo em `[Provisioning]` do `microsip.ini`: `ext`, `token`, `hash`, `interval`.
Outras opções locais: `simpleAccount=0` volta à tela de conta original; `allowCommands=1` está descrita acima.

## Servidor de referência (`server/`)

`server/prov_server.py` usa só a biblioteca padrão do Python 3.8+ e lê um JSON por tenant
(`server/data/exemplo.json`). O arquivo é relido a cada requisição: basta editar.

```bash
# no servidor (Debian/Ubuntu)
sudo useradd -r -s /usr/sbin/nologin microsip-prov
sudo mkdir -p /opt/microsip-prov/data
sudo cp server/prov_server.py /opt/microsip-prov/
sudo cp server/data/exemplo.json /opt/microsip-prov/data/acme.json   # edite!
echo "PROV_SECRET=$(openssl rand -hex 32)" | sudo tee /etc/microsip-prov.env
sudo chmod 600 /etc/microsip-prov.env
sudo chown -R root:microsip-prov /opt/microsip-prov && sudo chmod 750 /opt/microsip-prov/data
sudo chmod 640 /opt/microsip-prov/data/*.json
sudo cp server/deploy/microsip-prov.service /etc/systemd/system/
sudo systemctl enable --now microsip-prov
# nginx + certificado: server/deploy/nginx-prov.conf
# fail2ban (opcional): server/deploy/fail2ban-filter.conf
```

Teste:

```bash
curl -u 1001:troque-esta-senha https://prov.suaempresa.com.br/microsip/v1/acme/1001
```

Formato do JSON do tenant:

```json
{
  "refresh": 21600,
  "defaults": { "account": { ... }, "settings": { ... } },
  "extensions": {
    "1001": { "password": "...", "displayName": "Recepção" },
    "1002": { "password": "...", "authID": "x1002", "settings": { "recordingButton": 1 } },
    "1003": { "password": "...", "disabled": true },
    "1004": { "password": "...", "tokenVersion": 2 }
  }
}
```

- `tokenVersion`: incremente para revogar o token de um ramal (ex.: computador perdido).
- `disabled`: o ramal passa a receber 404.
- Proteção: depois de 30 falhas em 10 min, o IP recebe 429. Para bloqueio no firewall, use fail2ban.

> Próximo passo opcional: em vez de manter senhas no JSON, o servidor pode ler os ramais direto do
> banco do Asterisk/FreePBX (`ps_auths` do PJSIP realtime). Basta trocar a função `load_tenant`.
