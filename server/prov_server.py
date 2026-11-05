#!/usr/bin/env python3
"""
Servidor de provisionamento do MicroSIP personalizado (implementacao de referencia).

Somente biblioteca padrao do Python 3.8+. Rode atras de um nginx com HTTPS
(o MicroSIP valida o certificado; a resposta contem a senha SIP).

Rota:
    GET /microsip/v1/<tenant>/<ramal>
    Authorization: Basic base64(<ramal>:<senha SIP>)   -> primeiro acesso
    Authorization: Bearer <token>                      -> atualizacoes periodicas

Respostas:
    200 JSON {account, settings, token, refresh}
    401 credenciais invalidas      404 tenant/ramal inexistente
    429 muitas falhas deste IP     400 requisicao invalida

Dados: um arquivo JSON por tenant em DATA_DIR (veja data/exemplo.json).
Os arquivos sao relidos a cada requisicao: basta editar, nao precisa reiniciar.

Variaveis de ambiente:
    PROV_DATA_DIR   pasta dos JSON (padrao: ./data)
    PROV_SECRET     segredo para assinar tokens (OBRIGATORIO em producao)
    PROV_BIND       endereco (padrao: 127.0.0.1)
    PROV_PORT       porta (padrao: 8088)
    PROV_TRUST_PROXY  1 = usa X-Forwarded-For/X-Real-IP do nginx como IP do cliente

Log de falhas no formato (para fail2ban):
    AUTH_FAIL ip=1.2.3.4 tenant=acme ext=1001
"""

import base64
import hashlib
import hmac
import json
import logging
import os
import re
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

DATA_DIR = os.environ.get("PROV_DATA_DIR", os.path.join(os.path.dirname(os.path.abspath(__file__)), "data"))
SECRET = os.environ.get("PROV_SECRET", "")
BIND = os.environ.get("PROV_BIND", "127.0.0.1")
PORT = int(os.environ.get("PROV_PORT", "8088"))
TRUST_PROXY = os.environ.get("PROV_TRUST_PROXY", "1") == "1"

ROUTE = re.compile(r"^/microsip/v1/([A-Za-z0-9_-]{1,64})/([A-Za-z0-9_.+-]{1,64})/?$")
DEFAULT_REFRESH = 6 * 3600

# limite de falhas de autenticacao por IP
FAIL_WINDOW = 600
FAIL_MAX = 30  # alto: um escritorio inteiro sai pelo mesmo IP publico
_fails = {}
_fails_lock = threading.Lock()

log = logging.getLogger("prov")


def load_tenant(tenant):
    path = os.path.join(DATA_DIR, tenant + ".json")
    if not os.path.isfile(path):
        return None
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def make_token(tenant, ext, ext_data):
    # token estavel: muda apenas se "tokenVersion" do ramal mudar (revogacao)
    version = str(ext_data.get("tokenVersion", 1))
    msg = f"{tenant}:{ext}:{version}".encode()
    return hmac.new(SECRET.encode(), msg, hashlib.sha256).hexdigest()


def build_config(tenant, ext, data, ext_data):
    defaults = data.get("defaults", {})
    account = dict(defaults.get("account", {}))
    account.update(ext_data.get("account", {}))
    account["username"] = ext
    account.setdefault("authID", ext_data.get("authID", ext))
    account["password"] = ext_data["password"]
    if "displayName" in ext_data:
        account["displayName"] = ext_data["displayName"]
    if not account.get("domain") and account.get("server"):
        account["domain"] = account["server"]

    settings = dict(defaults.get("settings", {}))
    settings.update(ext_data.get("settings", {}))

    return {
        "account": account,
        "settings": settings,
        "token": make_token(tenant, ext, ext_data),
        "refresh": int(data.get("refresh", DEFAULT_REFRESH)),
    }


def too_many_fails(ip):
    now = time.time()
    with _fails_lock:
        hits = [t for t in _fails.get(ip, []) if now - t < FAIL_WINDOW]
        _fails[ip] = hits
        return len(hits) >= FAIL_MAX


def register_fail(ip):
    with _fails_lock:
        _fails.setdefault(ip, []).append(time.time())


class Handler(BaseHTTPRequestHandler):
    server_version = "MicroSIP-Prov/1.0"

    def client_ip(self):
        if TRUST_PROXY:
            ip = self.headers.get("X-Real-IP") or (self.headers.get("X-Forwarded-For") or "").split(",")[0].strip()
            if ip:
                return ip
        return self.client_address[0]

    def send_json(self, status, obj):
        body = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def deny(self, ip, tenant, ext, status=401):
        register_fail(ip)
        log.warning("AUTH_FAIL ip=%s tenant=%s ext=%s", ip, tenant, ext)
        self.send_json(status, {"error": "unauthorized"})

    def do_GET(self):
        ip = self.client_ip()
        m = ROUTE.match(self.path.split("?", 1)[0])
        if not m:
            self.send_json(404, {"error": "not found"})
            return
        tenant, ext = m.group(1), m.group(2)

        if too_many_fails(ip):
            log.warning("RATE_LIMIT ip=%s tenant=%s ext=%s", ip, tenant, ext)
            self.send_json(429, {"error": "too many attempts"})
            return

        try:
            data = load_tenant(tenant)
        except (OSError, ValueError) as e:
            log.error("tenant %s: arquivo invalido: %s", tenant, e)
            self.send_json(500, {"error": "server configuration error"})
            return

        ext_data = (data or {}).get("extensions", {}).get(ext)
        if not data or not ext_data or ext_data.get("disabled"):
            # mesma resposta para tenant e ramal inexistentes; conta como falha
            register_fail(ip)
            log.warning("NOT_FOUND ip=%s tenant=%s ext=%s", ip, tenant, ext)
            self.send_json(404, {"error": "not found"})
            return

        auth = self.headers.get("Authorization", "")
        ok = False
        if auth.startswith("Basic "):
            try:
                user, _, pwd = base64.b64decode(auth[6:]).decode("utf-8").partition(":")
            except (ValueError, UnicodeDecodeError):
                user, pwd = "", ""
            ok = user == ext and hmac.compare_digest(pwd.encode(), str(ext_data["password"]).encode())
        elif auth.startswith("Bearer "):
            ok = hmac.compare_digest(auth[7:].strip().encode(), make_token(tenant, ext, ext_data).encode())

        if not ok:
            self.deny(ip, tenant, ext)
            return

        log.info("OK ip=%s tenant=%s ext=%s device=%s version=%s", ip, tenant, ext,
                 self.headers.get("X-Device-Name", "-"), self.headers.get("X-MicroSIP-Version", "-"))
        self.send_json(200, build_config(tenant, ext, data, ext_data))

    def log_message(self, fmt, *args):
        # nao registra a linha de acesso padrao (o Authorization nao aparece, mas evitamos ruido)
        pass


def main():
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    if not SECRET:
        if os.environ.get("PROV_ALLOW_NO_SECRET") != "1":
            print("ERRO: defina PROV_SECRET (ex.: openssl rand -hex 32)", file=sys.stderr)
            return 1
        log.warning("PROV_SECRET vazio: use apenas para testes")
    httpd = ThreadingHTTPServer((BIND, PORT), Handler)
    log.info("escutando em %s:%d, dados em %s", BIND, PORT, DATA_DIR)
    httpd.serve_forever()
    return 0


if __name__ == "__main__":
    sys.exit(main())
