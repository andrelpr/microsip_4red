#!/usr/bin/env python3
"""
[fork] Gera src/microsip/fork_config.h a partir de variaveis de ambiente (CI).

  FORK_VERSION            ex.: 3.22.16-fork.12       (padrao: "dev")
  FORK_PROVISIONING_URL   URL base de provisionamento embutida (opcional)
  FORK_INI_KEY            chave de criptografia das senhas no microsip.ini (opcional, recomendado)

Uso: python build/gen_fork_config.py <caminho/fork_config.h>
"""
import os
import sys


def c_string(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def main() -> int:
    out = sys.argv[1] if len(sys.argv) > 1 else "src/microsip/fork_config.h"
    version = os.environ.get("FORK_VERSION", "").strip() or "dev"
    url = os.environ.get("FORK_PROVISIONING_URL", "").strip().rstrip("/")
    key = os.environ.get("FORK_INI_KEY", "").strip()

    lines = [
        "/* Gerado automaticamente por build/gen_fork_config.py - nao editar */",
        "#pragma once",
        f"#define _FORK_VERSION {c_string(version)}",
    ]
    if url:
        if not url.startswith("https://"):
            print("AVISO: FORK_PROVISIONING_URL deveria usar https://", file=sys.stderr)
        lines.append(f"#define _FORK_PROVISIONING_URL {c_string(url)}")
    if key:
        # literal wide: o MicroSIP faz (LPCTSTR)_GLOBAL_KEY
        lines.append(f"#define _GLOBAL_KEY L{c_string(key)}")
    else:
        print("AVISO: FORK_INI_KEY nao definido; usando a chave padrao do codigo publico", file=sys.stderr)

    with open(out, "w", encoding="utf-8", newline="\r\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"{out}: versao={version} url={'(definida)' if url else '(vazia)'} chave={'(definida)' if key else '(padrao)'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
