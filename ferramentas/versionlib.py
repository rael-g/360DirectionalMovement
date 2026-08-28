"""Leitor do Address Library (versionlib-*.bin), formatos V2 e V5.

V5 (o que o Starfield 1.16.244 usa) e trivial: cabecalho de 96 bytes
(fileVersion int32, gameVersion uint32[4], name char[64], pointerSize int32,
dataFormat int32, offsetCount int32) seguido de um vetor plano de uint32
indexado diretamente pelo ID. Offset 0 significa "nao existe nesta versao".

Uso: versionlib.py <arquivo.bin> [id ...]
"""
import struct
import sys

TAM_CABECALHO_V5 = 4 + 16 + 64 + 4 + 4 + 4


def carregar(caminho):
    with open(caminho, "rb") as f:
        dados = f.read()

    (formato,) = struct.unpack_from("<i", dados, 0)
    if formato != 5:
        raise ValueError(f"formato {formato} nao suportado (so V5)")

    versao = struct.unpack_from("<4I", dados, 4)
    nome = dados[20:84].split(b"\0")[0].decode("utf-8", "replace")
    tam_ponteiro, formato_dados, contagem = struct.unpack_from("<3i", dados, 84)

    corpo = dados[TAM_CABECALHO_V5:]
    offsets = struct.unpack_from(f"<{contagem}I", corpo, 0)

    return {"versao": versao, "nome": nome, "ponteiro": tam_ponteiro,
            "contagem": contagem, "offsets": offsets}


def offset(db, ident):
    if ident >= db["contagem"]:
        return None
    v = db["offsets"][ident]
    return v or None


if __name__ == "__main__":
    db = carregar(sys.argv[1])
    print(f"versao={'.'.join(map(str, db['versao']))} nome={db['nome']!r} "
          f"ponteiro={db['ponteiro']} entradas={db['contagem']}")
    validos = sum(1 for o in db["offsets"] if o)
    print(f"ids com offset: {validos}")
    for a in sys.argv[2:]:
        i = int(a)
        v = offset(db, i)
        print(f"  id {i:<10} -> {'0x%X' % v if v else 'NAO ENCONTRADO'}")
