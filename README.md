# Toolchain para plugins SFSE (Starfield), no Arch

Compila DLL Windows x64 com **ABI MSVC** a partir do Linux. ABI MSVC não é
preferência: o `Starfield.exe` e o CommonLibSF são MSVC, e mingw não serve.

Tudo vive na distrobox **`sfse`** (`archlinux:latest`), criada só para isso.

```
distrobox enter sfse
```

## O que está instalado

- Na caixa, via pacman: `clang` (22.1.8), `lld`, `llvm`, `cmake`, `ninja`, `git`, `python`
- `~/.local/bin/xwin` 0.10.0 — binário oficial do release, sha256 conferido
- `~/sfse-toolchain/xwin/` — CRT da MSVC + Windows SDK 10.0.26100 (641 MB),
  baixados da Microsoft pelo xwin com `--accept-license`

Como o distrobox monta a home do host, `~/sfse-toolchain` é o mesmo
`/home/raelg/sfse-toolchain` de fora. O `msvc.cmake` usa caminho absoluto, então
funciona dos dois lados.

## Como usar

```
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=/home/raelg/sfse-toolchain/msvc.cmake .
cmake --build build
```

## Armadilha já resolvida

O xwin **não baixa o CRT de debug** (`msvcrtd.lib`) — a Microsoft não redistribui
essa parte. Um build Debug do CMake pede `/MDd` e falha no link. O `msvc.cmake`
já força `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL` e o default
`RelWithDebInfo`, então "debug" aqui significa apenas símbolos, nunca CRT de debug.
Se aparecer `could not open 'msvcrtd.lib'`, é alguém sobrescrevendo isso.

## Prova de que funciona

`teste/` compila `hello.dll` usando Win32 (`windows.h`), CRT (`cstdio`) e STL
(`std::string`) juntos, mais um `loader.exe` que faz `LoadLibrary` +
`GetProcAddress`. Rodado sob Wine na distrobox `steam`:

```
$ wine loader.exe
RETORNOU: toolchain ok (205097450)
```

DLL confirmada como `PE32+ executable for MS Windows, x86-64`, com
`SFSEPlugin_Version` exportado no ordinal 1.

## `sfse-src/` — a API oficial

Fonte do SFSE 0.2.21 extraído do **próprio mod instalado**
(`cmm/mods/f5b3.../src/sfse-0.2.21.tar.gz`). Nada de reimplementar struct por
engenharia reversa: `sfse/PluginAPI.h` e `sfse_common/sfse_version.h` são os
cabeçalhos autênticos.

Pontos que importam:
- Exportar o **dado** `SFSEPlugin_Version` (`SFSEPluginVersionData`) e a **função**
  `SFSEPlugin_Load`. O SFSE não chama código para decidir compatibilidade — ele lê
  a struct.
- Runtime instalado: `RUNTIME_VERSION_1_16_244`, exatamente o alvo do SFSE 0.2.21.

## `sf360/` — estágio 1 do plugin de movimento 360

Não toca no jogo. Só loga versão de SFSE, versão de runtime e o handle recebido,
em `Documents/My Games/Starfield/SFSE/Logs/sf360.log` (caminho resolvido em runtime
via `SHGetKnownFolderPath`, então funciona dentro do prefixo Proton).

**Provado no jogo em 2026-08-26.** `sfse.txt`:

```
plugin sf360.dll (00000001 sf360 00000001) loaded correctly (handle 3)
```

e o `sf360.log`:

```
--- sf360 carregado ---
sfse 0.2.21 | runtime 1.16.244
interfaceVersion 1 | handle 3
```

### Empacotamento (armadilha que já custou uma tentativa)

Mod de Starfield **não leva pasta `Data/` dentro do zip**. A raiz do arquivo é
despejada direto dentro de `Data/`. Um zip com `Data/SFSE/Plugins/x.dll` acaba
entregue em `Data/data/SFSE/plugins/` e o SFSE nunca enxerga. O certo é:

```
SFSE/Plugins/sf360.dll
```

Isso é convenção do jogo, não defeito do cmm.

## Sobre o CommonLibSF

Não foi necessário até aqui, e para o estágio 1 seria peso morto. Ele só entra no
estágio 2, quando for preciso mexer em estruturas do jogo (ator, grafo de animação).

Contexto do objetivo na memória `starfield-360-movimento`.

## `ferramentas/`

- `ba2.py` — leitor de `.ba2` do Starfield (BTDX/GNRL). `listar` e `extrair`.
- `versionlib.py` — leitor do Address Library. O Starfield 1.16.244 usa o
  **formato V5**, não o V2: cabeçalho de 96 bytes e depois um vetor plano de
  `uint32` indexado pelo próprio ID (offset 0 = não existe nessa versão).
  Muito mais simples que o V2 delta-codificado.

## Endereços usados (Address Library 1.16.244)

| o que | ID | offset |
|---|---|---|
| `PlayerCharacter` singleton | 922868 | `0x5F43230` |
| `BSStringPool::GetEntry` | 1186742 | `0x28CBA80` |
| `BSStringPool::Entry::Release` | 139340 | `0x28CAEA0` |

Outros fatos vindos do CommonLibSF (usados como referência, sem compilá-lo):
- `IAnimationGraphManagerHolder` é sub-objeto de `TESObjectREFR` em **+0x60**
- vtable do holder: `GetGraphVariableImplFloat` **0x12**, `...Int` **0x13**,
  `...Bool` **0x14** — são virtuais, então não precisam de endereço
- `BSFixedString` é `detail::BSFixedString<char, false>`, ou seja
  **case-insensitive**: `GetEntry(entry, nome, false)`
