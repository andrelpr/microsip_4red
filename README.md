# MicroSIP personalizado

Versão própria do [MicroSIP](https://www.microsip.org) (GPL v2), baseada no código-fonte oficial
**3.22.16** (o mais recente publicado pelo autor) e no PJSIP **2.15.1** (o mesmo do binário oficial 3.22.23).

## O que muda em relação ao oficial

| # | Mudança | Arquivos |
|---|---|---|
| 1 | **Fim do "Senha incorreta" permanente.** O PJSIP desiste para sempre depois de um 401/403/404 do servidor (ex.: Asterisk reiniciando). Agora o registro é refeito sozinho a cada 30s, sem limite. Se o ramal nunca registrou na sessão (senha provavelmente errada de verdade), o intervalo dobra até 5 min, para não acionar o fail2ban. Falhas temporárias (408/5xx) também passam a ser repetidas em 30s, em vez de 300s. | `mainDlg.cpp` (`ScheduleRegRetry`, `OnTimerRegRetry`) |
| 2 | **Dois botões de transferência** na janela da chamada: **Transferir** (sem consulta) e **Consultar** (com consulta). Durante a consulta, o botão vira **Completar**. | `MessagesDlg.cpp`, `res/dialog.rc2` |
| 3 | **DND e Atendimento automático escondidos** por padrão (o provisionamento pode reativá-los) | `define.h` |
| 4 | **Tela de conta simplificada**: Ramal, Senha e Servidor. "Avançado..." abre a tela original. | `SimpleAccountDlg.*` |
| 5 | **Provisionamento centralizado** por HTTPS, com atualização periódica | `Provisioning.*`, [docs/provisionamento.md](docs/provisionamento.md), `server/` |
| 6 | Não oferece "atualizar" para a versão oficial | `mainDlg.cpp` (`CheckUpdates`) |
| 7 | Senha salva por outra versão (chave diferente) é descartada, em vez de usada errada | `settings.cpp` |
| 8 | Sem vídeo e sem G.729 (dispensa FFmpeg/SDL/VPX/IPP) | `const.h`, `build/config_site.h` |

Todas as alterações no código estão marcadas com `[fork]`.
O código original está em `vendor/MicroSIP-3.22.16-src.7z` para comparação.

## Estrutura

```
src/microsip/          código do MicroSIP (modificado)
build/config_site.h    configuração do PJSIP
build/gen_fork_config.py  gera fork_config.h (versão, URL, chave) no CI
installer/             script NSIS + idioma pt-BR (com os textos novos)
server/                servidor de provisionamento de referência (Python)
docs/                  documentação
.github/workflows/     compilação automática (GitHub Actions, Windows)
MicroSIP-3.22.23.exe   instalador oficial: o CI extrai dele os sons, o lame_enc.dll e a licença
```

## Compilação (GitHub Actions)

1. Crie um repositório no GitHub (privado ou público) e envie este projeto.
2. Em **Settings → Secrets and variables → Actions**:
   - **Secret** `FORK_INI_KEY`: uma chave aleatória (ex.: `openssl rand -hex 24`).
     **Defina uma vez e nunca mude**: ela criptografa as senhas no `microsip.ini`. Se mudar, todos
     os usuários terão que digitar a senha de novo (ou receber pelo provisionamento).
   - **Variable** `FORK_PROVISIONING_URL` (opcional): URL embutida no programa.
3. Cada push na `main` gera os artefatos **MicroSIP-Setup** (instalador) e **MicroSIP-portable**.
   Uma tag `v1.0.0` publica um Release.

## Instalação nos clientes

```
MicroSIP-Setup-x.y.z.exe                     instalação com telas
MicroSIP-Setup-x.y.z.exe /S /URL=https://prov.suaempresa.com.br/microsip/v1/acme
                                             silenciosa, já apontando para o cliente "acme"
```

O instalador fecha o MicroSIP aberto, instala em `Arquivos de Programas\MicroSIP`, grava a URL em
`provisioning.ini` (vale para todos os usuários do Windows) e configura o programa para iniciar com o
Windows. As configurações dos usuários (`%APPDATA%\MicroSIP`), como contatos e histórico, são mantidas.

**Migração de quem usa o oficial:** a senha salva pelo oficial não pode ser lida (chave diferente).
Com provisionamento, basta o usuário digitar ramal e senha uma vez na tela "Configurar ramal".

## Recomendação no Asterisk

O "Senha incorreta" coletivo indica que o Asterisk respondeu 401/403 para todos ao mesmo tempo.
Vale investigar a causa no `/var/log/asterisk/full` (reload/restart, banco realtime, ACL), e:

- no fail2ban, coloque os IPs fixos dos clientes em `ignoreip`, para que um ramal com senha
  errada não derrube o escritório inteiro.

## Licença

GPL v2, como o MicroSIP original. Quem recebe o programa tem direito ao código-fonte destas alterações.
