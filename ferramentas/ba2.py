"""Leitor de .ba2 do Starfield (BTDX v2/v3, GNRL).

Header de 32 bytes: magic 'BTDX', versao, tipo ('GNRL'), numero de arquivos,
offset da tabela de nomes. Cada registro tem 36 bytes; os nomes vivem no fim,
cada um com prefixo de tamanho em 2 bytes. Compressao zlib por arquivo
(packed_size == 0 significa armazenado sem compressao).

Uso:
  ba2.py listar <arquivo.ba2> [padrao]
  ba2.py extrair <arquivo.ba2> <padrao> <destino>
"""
import struct
import sys
import os
import zlib


def abrir(caminho):
    f = open(caminho, "rb")
    magic, versao, tipo, n_arq, ofs_nomes = struct.unpack("<4sI4sIQ", f.read(24))
    if magic != b"BTDX":
        raise ValueError(f"nao e BA2: {magic!r}")
    if tipo != b"GNRL":
        raise ValueError(f"tipo {tipo!r} nao suportado (so GNRL)")

    # o resto do header de 32 bytes varia entre versoes e nao e usado aqui
    f.seek(32)
    regs = [struct.unpack("<I4sIIQIII", f.read(36)) for _ in range(n_arq)]

    f.seek(ofs_nomes)
    nomes = []
    for _ in range(n_arq):
        (ln,) = struct.unpack("<H", f.read(2))
        nomes.append(f.read(ln).decode("cp1252").replace("\\", "/").lower())

    itens = []
    for nome, r in zip(nomes, regs):
        _hash, _ext, _dirhash, _flags, ofs, packed, unpacked, _align = r
        itens.append({"path": nome, "offset": ofs,
                      "packed": packed, "unpacked": unpacked})
    return f, itens


def ler(f, item):
    f.seek(item["offset"])
    if item["packed"]:
        return zlib.decompress(f.read(item["packed"]))
    return f.read(item["unpacked"])


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    cmd, arquivo = sys.argv[1], sys.argv[2]
    f, itens = abrir(arquivo)
    padrao = sys.argv[3].lower() if len(sys.argv) > 3 else ""

    if cmd == "listar":
        n = 0
        for it in itens:
            if padrao in it["path"]:
                print(f"{it['unpacked']:>10}  {it['path']}")
                n += 1
        print(f"--- {n} de {len(itens)} arquivos", file=sys.stderr)
    elif cmd == "extrair":
        destino = sys.argv[4]
        for it in itens:
            if padrao in it["path"]:
                alvo = os.path.join(destino, it["path"])
                os.makedirs(os.path.dirname(alvo), exist_ok=True)
                with open(alvo, "wb") as g:
                    g.write(ler(f, it))
                print(alvo)
    return 0


if __name__ == "__main__":
    sys.exit(main())
