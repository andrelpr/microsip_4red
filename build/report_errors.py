#!/usr/bin/env python3
"""[fork] Publica os erros de um log do MSBuild como anotacoes do GitHub Actions
(legiveis pela API publica, sem precisar baixar o log). Uso: report_errors.py <log>..."""
import re
import sys

pat = re.compile(r"(error|fatal error|warning MSB)\s*[A-Z]*\d*\s*:", re.I)
seen, out = set(), []
for path in sys.argv[1:]:
    try:
        lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
    except OSError:
        continue
    for line in lines:
        line = line.strip()
        if " error " in line.lower() or pat.search(line):
            key = re.sub(r"\[.*?\]$", "", line)
            if key not in seen:
                seen.add(key)
                out.append(key)
for line in out[:45]:
    msg = line.replace("%", "%25").replace("\r", "").replace("\n", " ")
    print(f"::error::{msg[:900]}")
print(f"{len(out)} erros distintos")
