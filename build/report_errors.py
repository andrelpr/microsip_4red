#!/usr/bin/env python3
"""[fork] Publica os erros de um log do MSBuild como anotacoes do GitHub Actions
(legiveis pela API publica, sem precisar baixar o log). Uso: report_errors.py <log>...
O GitHub mostra no maximo 10 anotacoes por etapa: os erros sao agrupados em blocos."""
import re
import sys

pat = re.compile(r"(: (fatal )?error [A-Z]+\d+:|error MSB\d+|: error :|LINK : fatal error)", re.I)
seen, out = set(), []
for path in sys.argv[1:]:
    try:
        lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
    except OSError:
        continue
    for line in lines:
        line = line.strip()
        if pat.search(line):
            key = re.sub(r"\s*\[[^\]]*\.vcxproj[^\]]*\]$", "", line)
            key = re.sub(r"^.*?\\microsip\\", "", key)  # encurta caminhos
            if key not in seen:
                seen.add(key)
                out.append(key)
print(f"{len(out)} erros distintos")
chunk, size = [], 0
blocks = []
for line in out:
    line = line[:400]
    if size + len(line) > 3500 and chunk:
        blocks.append(chunk); chunk, size = [], 0
    chunk.append(line); size += len(line)
if chunk:
    blocks.append(chunk)
for b in blocks[:9]:
    msg = "%0A".join(l.replace("%", "%25") for l in b)
    print(f"::error::{msg}")
