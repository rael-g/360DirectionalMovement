// Plugin de movimento 360 para Starfield: gira o personagem para encarar a
// direcao em que ele se move, em vez de deixar ele andar de costas.
//
// Le as variaveis do grafo de animacao do jogador e loga cada vez que uma muda.
// O objetivo e descobrir, empiricamente, qual variavel descreve a direcao do
// movimento e qual controla girar-para-encarar vs. andar de costas.
//
// Nao usa o CommonLibSF compilado: so os fatos que ele documenta.
//   - PlayerCharacter singleton .... Address Library ID 922868
//   - BSStringPool::GetEntry ....... Address Library ID 1186742
//   - IAnimationGraphManagerHolder . sub-objeto em TESObjectREFR + 0x60
//   - GetGraphVariableImpl{Float,Int,Bool} .. vtable 0x12, 0x13, 0x14

#include <windows.h>
#include <shlobj.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "PluginAPI.h"
#include "sfse_version.h"

namespace {

// ---------------------------------------------------------------- log

std::string PastaSFSE()
{
    PWSTR docs = nullptr;
    if (!SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docs)))
        return ".";
    char narrow[MAX_PATH]{};
    WideCharToMultiByte(CP_UTF8, 0, docs, -1, narrow, MAX_PATH, nullptr, nullptr);
    CoTaskMemFree(docs);
    std::string dir = std::string(narrow) + "\\My Games\\" SAVE_FOLDER_NAME "\\SFSE";
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir;
}

std::string CaminhoDoLog()
{
    PWSTR docs = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docs))) {
        char narrow[MAX_PATH]{};
        WideCharToMultiByte(CP_UTF8, 0, docs, -1, narrow, MAX_PATH, nullptr, nullptr);
        CoTaskMemFree(docs);
        std::string dir = std::string(narrow) + "\\My Games\\" SAVE_FOLDER_NAME "\\SFSE\\Logs";
        CreateDirectoryA(dir.c_str(), nullptr);
        return dir + "\\sf360.log";
    }
    return "sf360.log";
}

void Loga(const char* fmt, ...)
{
    static std::string caminho = CaminhoDoLog();
    FILE* f = std::fopen(caminho.c_str(), "a");
    if (!f) return;
    va_list args;
    va_start(args, fmt);
    std::vfprintf(f, fmt, args);
    va_end(args);
    std::fputc('\n', f);
    std::fclose(f);
}

// ------------------------------------------------------------- config
//
// Cada chave liga/desliga uma peca isolada. Serve para o teste comparativo:
// desligar uma e ver o que volta a quebrar e a unica forma de saber se ela
// esta pagando o proprio preco. O arquivo e criado com os padroes na primeira
// execucao, ao lado do log.

struct Config
{
    bool  ganchoPost = true;    // escreve tambem no PostUpdate (0x16)
    bool  recapturaAssenta = true;  // recaptura o desvio quando o rumo estabiliza
    bool  recapturaTroca = true;    // recaptura quando o jogador troca de direcao
    float passoMax = 0.5f;         // radianos por atualizacao; 0 = teleporte
    float velMin = 0.5f;            // Speed acima do qual conta como movimento
    float limiarPuloZ = 1.0f;       // |velocidade Z| acima da qual conta como no ar
};

Config g_cfg;

std::string CaminhoDaConfig() { return PastaSFSE() + "\\sf360.ini"; }

void CarregaConfig()
{
    const std::string caminho = CaminhoDaConfig();
    FILE* f = std::fopen(caminho.c_str(), "r");
    if (!f) {
        f = std::fopen(caminho.c_str(), "w");
        if (f) {
            // Escrito a partir dos proprios defaults do struct: template com
            // numero fixo ja divergiu do codigo uma vez (passoMax=0.38).
            const Config p;
            std::fprintf(f,
                "; sf360 - cada chave isola uma peca. 1 liga, 0 desliga.\n"
                "ganchoPost=%d\n"
                "recapturaAssenta=%d\n"
                "recapturaTroca=%d\n"
                "passoMax=%.3f\n"
                "velMin=%.3f\n"
                "limiarPuloZ=%.3f\n",
                p.ganchoPost, p.recapturaAssenta, p.recapturaTroca,
                p.passoMax, p.velMin, p.limiarPuloZ);
            std::fclose(f);
        }
        return;
    }
    char linha[256];
    while (std::fgets(linha, sizeof linha, f)) {
        if (linha[0] == ';' || linha[0] == '\n') continue;
        char chave[64]{}; float valor = 0.0f;
        if (std::sscanf(linha, "%63[^=]=%f", chave, &valor) != 2) continue;
        const bool lig = valor != 0.0f;
        if (!std::strcmp(chave, "ganchoPost")) g_cfg.ganchoPost = lig;
        else if (!std::strcmp(chave, "recapturaAssenta")) g_cfg.recapturaAssenta = lig;
        else if (!std::strcmp(chave, "recapturaTroca")) g_cfg.recapturaTroca = lig;
        else if (!std::strcmp(chave, "passoMax")) g_cfg.passoMax = valor;
        else if (!std::strcmp(chave, "velMin")) g_cfg.velMin = valor;
        else if (!std::strcmp(chave, "limiarPuloZ")) g_cfg.limiarPuloZ = valor;
    }
    std::fclose(f);
}

// ------------------------------------------------- address library (V5)

// Cabecalho V5: fileVersion i32, gameVersion u32[4], name char[64],
// pointerSize i32, dataFormat i32, offsetCount i32. Depois um vetor plano de
// u32 indexado pelo proprio ID; zero significa "nao existe nesta versao".
constexpr std::size_t kCabecalhoV5 = 4 + 16 + 64 + 4 + 4 + 4;

std::vector<std::uint32_t> g_offsets;

bool CarregaVersionLib(const char* caminho)
{
    FILE* f = std::fopen(caminho, "rb");
    if (!f) return false;

    std::int32_t formato = 0;
    std::fread(&formato, 4, 1, f);
    if (formato != 5) {
        Loga("versionlib: formato %d nao suportado (esperado 5)", formato);
        std::fclose(f);
        return false;
    }

    std::uint32_t versao[4]{};
    char          nome[64]{};
    std::int32_t  tamPonteiro = 0, formatoDados = 0, contagem = 0;
    std::fread(versao, 4, 4, f);
    std::fread(nome, 1, 64, f);
    std::fread(&tamPonteiro, 4, 1, f);
    std::fread(&formatoDados, 4, 1, f);
    std::fread(&contagem, 4, 1, f);

    if (contagem <= 0) {
        std::fclose(f);
        return false;
    }

    g_offsets.resize(static_cast<std::size_t>(contagem));
    std::fseek(f, static_cast<long>(kCabecalhoV5), SEEK_SET);
    const auto lidos = std::fread(g_offsets.data(), 4, g_offsets.size(), f);
    std::fclose(f);

    Loga("versionlib: %u.%u.%u.%u '%s', %d entradas, %zu lidas",
         versao[0], versao[1], versao[2], versao[3], nome, contagem, lidos);
    return lidos == g_offsets.size();
}

std::uintptr_t Base()
{
    static std::uintptr_t base =
        reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    return base;
}

std::uintptr_t Endereco(std::uint32_t id)
{
    if (id >= g_offsets.size() || !g_offsets[id]) return 0;
    return Base() + g_offsets[id];
}

// --------------------------------------------------------- grafo

constexpr std::uint32_t kIdPlayerSingleton = 922868;
constexpr std::uint32_t kIdGetEntry = 1186742;

// deslocamento do sub-objeto IAnimationGraphManagerHolder dentro de TESObjectREFR
constexpr std::ptrdiff_t kHolderOffset = 0x60;

// indices na vtable de IAnimationGraphManagerHolder
constexpr std::size_t kVtFloat = 0x12;
constexpr std::size_t kVtInt = 0x13;
constexpr std::size_t kVtBool = 0x14;

using GetEntry_t = void (*)(void*& saida, const char* texto, bool caseSensitive);

void* Interna(const char* nome)
{
    static auto func = reinterpret_cast<GetEntry_t>(Endereco(kIdGetEntry));
    if (!func) return nullptr;
    void* entrada = nullptr;
    func(entrada, nome, false);  // BSFixedString e case-insensitive
    return entrada;
}

// Faixa de enderecos do executavel, lida do cabecalho PE. Serve para conferir
// que um ponteiro de vtable aponta mesmo para dentro do jogo.
bool DentroDoModulo(const void* p)
{
    static std::uintptr_t inicio = 0, fim = 0;
    if (!fim) {
        const auto base = Base();
        auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        inicio = base;
        fim = base + nt->OptionalHeader.SizeOfImage;
    }
    const auto v = reinterpret_cast<std::uintptr_t>(p);
    return v >= inicio && v < fim;
}

// Durante o carregamento o objeto do jogador e destruido e recriado. Ler o
// ponteiro e chamar um metodo virtual nesse intervalo significa saltar para um
// endereco qualquer. A conferencia da vtable custa quase nada e evita isso.
void* Jogador()
{
    const auto addr = Endereco(kIdPlayerSingleton);
    if (!addr) return nullptr;
    void* p = *reinterpret_cast<void**>(addr);
    if (!p) return nullptr;
    if (reinterpret_cast<std::uintptr_t>(p) < 0x10000) return nullptr;
    auto vtable = *reinterpret_cast<void**>(p);
    if (!DentroDoModulo(vtable)) return nullptr;
    return p;
}

template <class T, std::size_t Indice>
bool LeVariavel(void* holder, void* nomeInternado, T& saida)
{
    using func_t = bool (*)(void*, void* const*, T*);
    auto vtable = *reinterpret_cast<void***>(holder);
    if (!DentroDoModulo(vtable) || !DentroDoModulo(vtable[Indice])) return false;
    auto func = reinterpret_cast<func_t>(vtable[Indice]);
    return func(holder, &nomeInternado, &saida);
}

// ------------------------------------------------- tarefa por frame
//
// Roda na thread principal, todo frame, via SFSETaskInterface::AddTaskPermanent.
// TUDO que toca objeto do jogo tem que estar aqui: uma thread propria chamando
// metodos virtuais do jogo corre em paralelo com a destruicao e recriacao
// desses objetos, e ainda disputa o lock do pool de strings com a thread de
// render. Foi o que provavelmente travou o jogo no carregamento.

// TESObjectREFR::data em +0x80; OBJ_REFR comeca com NiPoint3 angle, z em +8
constexpr std::ptrdiff_t kAnguloZ = 0x80 + 8;

// OBJ_REFR: angle (NiPoint3, 12 bytes) e depois location, entao +0x80+0x0C
constexpr std::ptrdiff_t kPosicao = 0x80 + 0x0C;

// So para OBSERVAR. O controle nao usa posicao — e lenta demais para a
// largada. Mas sem ela o log so sabe o que o grafo diz sobre o movimento, e
// nao para onde o personagem de fato foi: e circular, e foi assim que a
// deslizada ficou invisivel do 5h ao 5j.
float PosicaoZ(void* jogador)
{
    return reinterpret_cast<const float*>(
        reinterpret_cast<char*>(jogador) + kPosicao)[2];
}

void Posicao(void* jogador, float& x, float& y)
{
    auto p = reinterpret_cast<const float*>(
        reinterpret_cast<char*>(jogador) + kPosicao);
    x = p[0];
    y = p[1];
}

// Actor+0x228 -> AIProcess; +0x08 -> MiddleHighProcessData; +0x458 ->
// bhkCharacterController. O controlador nao esta mapeado no CommonLibSF, entao
// o campo de velocidade e desconhecido — a varredura abaixo procura ele.
void* Controlador(void* jogador)
{
    auto proc = *reinterpret_cast<void**>(reinterpret_cast<char*>(jogador) + 0x228);
    if (!proc || reinterpret_cast<std::uintptr_t>(proc) < 0x10000) return nullptr;
    auto mid = *reinterpret_cast<void**>(reinterpret_cast<char*>(proc) + 0x08);
    if (!mid || reinterpret_cast<std::uintptr_t>(mid) < 0x10000) return nullptr;
    auto ctrl = *reinterpret_cast<void**>(reinterpret_cast<char*>(mid) + 0x458);
    if (!ctrl || reinterpret_cast<std::uintptr_t>(ctrl) < 0x10000) return nullptr;
    // vtable dentro do modulo: mesma conferencia que ja salva o ponteiro do
    // jogador durante o carregamento.
    if (!DentroDoModulo(*reinterpret_cast<void**>(ctrl))) return nullptr;
    return ctrl;
}

float AnguloZ(void* jogador)
{
    return *reinterpret_cast<float*>(reinterpret_cast<char*>(jogador) + kAnguloZ);
}

// leva um angulo em radianos para (-pi, pi]
// degrau do quantizador de input: 0..7, um oitavo de volta cada
int Degrau(float turnos)
{
    int d = static_cast<int>(std::lround(turnos * 8.0f)) % 8;
    return d < 0 ? d + 8 : d;
}

float Encurta(float a)
{
    while (a > 3.14159265f) a -= 6.2831853f;
    while (a < -3.14159265f) a += 6.2831853f;
    return a;
}

constexpr float kVolta = 6.2831853f;

void EscreveAnguloZBruto(void* jogador, float v)
{
    while (v < 0.0f) v += kVolta;
    while (v >= kVolta) v -= kVolta;
    *reinterpret_cast<float*>(reinterpret_cast<char*>(jogador) + kAnguloZ) = v;
}

// PreUpdateAnimationGraphManager, indice 0x15 do IAnimationGraphManagerHolder
// (o mesmo vtable de onde leio Direction no 0x12). O jogo chama esse metodo
// UMA vez por atualizacao do grafo, logo antes do grafo consumir o estado do
// ator.
//
// Ate aqui eu escrevia em angle.z ~9x por atualizacao, via AddTaskPermanent,
// em instantes arbitrarios em relacao a esse ponto: parte das escritas caia
// depois da leitura do grafo e nao valia nada, parte caia antes e valia. Isso
// e indistinguivel de uma disputa e e a suspeita para o tremilique. Aqui a
// escrita e uma so, no ultimo instante em que ainda conta.
constexpr std::size_t kIndicePreUpdate = 0x15;

// O jogo ainda move o corpo DEPOIS do PreUpdate: no 6c o alvo era 333.7 e o
// corpo parava em 352.9, 19 graus fora, estaveis. Escrever tambem no Post
// (0x16), logo apos o grafo, e ter a ultima palavra.
constexpr std::size_t kIndicePostUpdate = 0x16;

using PreUpdate_t = void (*)(void*, const void*);

PreUpdate_t g_preUpdateOriginal{ nullptr };
PreUpdate_t g_postUpdateOriginal{ nullptr };
void*       g_holderDoJogador{ nullptr };
void*       g_jogadorDoGancho{ nullptr };
bool        g_temAlvo{ false };
float       g_alvo{ 0.0f };
// Angulo efetivamente escrito. O alvo pode saltar; este persegue o alvo a um
// passo maximo por atualizacao, que e o que o jogo faz quando gira sozinho
// (13-19 graus por atualizacao, medido no 5h). Teleportar era o que deixava a
// troca de direcao seca — e nao tinha nada a ver com as animacoes.
float       g_atual{ 0.0f };
bool        g_saltar{ false };
// um pouco acima do giro proprio do jogo, para o corpo nao ficar para tras
constexpr float kPassoMax = 0.38f;   // ~22 graus por atualizacao

// Persegue o alvo pelo caminho curto e devolve o angulo a escrever.
float PassoAteOAlvo()
{
    if (g_saltar) { g_atual = g_alvo; g_saltar = false; return g_atual; }
    float d = g_alvo - g_atual;
    while (d > 3.14159265f) d -= 6.2831853f;
    while (d < -3.14159265f) d += 6.2831853f;
    if (g_cfg.passoMax <= 0.0f) { g_atual = g_alvo; return g_atual; }
    if (d > g_cfg.passoMax) d = g_cfg.passoMax;
    if (d < -g_cfg.passoMax) d = -g_cfg.passoMax;
    g_atual += d;
    return g_atual;
}
volatile long g_chamadas{ 0 };

void PreUpdateGancho(void* holder, const void* mgr)
{
    // A original primeiro: e um metodo do jogo e pode ter efeito colateral.
    if (g_preUpdateOriginal) g_preUpdateOriginal(holder, mgr);

    // O vtable e da classe, nao da instancia. Sem essa guarda a gente giraria
    // qualquer NPC que compartilhe o vtable do PlayerCharacter.
    if (holder != g_holderDoJogador || !g_jogadorDoGancho) return;

    InterlockedIncrement(&g_chamadas);
    // O passo anda UMA vez por atualizacao do grafo, e este gancho e chamado
    // uma vez por atualizacao (gan=1, medido no 6a). O Post so repete o mesmo
    // valor para ter a ultima palavra.
    if (g_temAlvo) EscreveAnguloZBruto(g_jogadorDoGancho, PassoAteOAlvo());
}

// Offset achado por varredura no 6e: o par de floats em +0x0D0 do
// bhkCharacterController bateu com o rumo real do deslocamento em 193 de 208
// atualizacoes (93%), em todas as corridas. E a velocidade.
// Os tres melhores da varredura do 6e. O 6f escreveu so no 0x0D0 e nao mudou
// nada, entao ou ele nao e a velocidade efetiva ou o jogo sobrescreve depois.
// Aqui o proprio build testa: alterna o alvo a cada arrancada e loga qual foi.

void* g_controlador{ nullptr };

float RumoDoVetor(void* ctrl, std::ptrdiff_t off)
{
    auto v = reinterpret_cast<const float*>(reinterpret_cast<char*>(ctrl) + off);
    if (!std::isfinite(v[0]) || !std::isfinite(v[1])) return -999.0f;
    const float m2 = v[0] * v[0] + v[1] * v[1];
    if (m2 < 0.01f) return -999.0f;
    return std::atan2(v[0], v[1]) * 57.2958f;
}



void PostUpdateGancho(void* holder, const void* mgr)
{
    if (g_postUpdateOriginal) g_postUpdateOriginal(holder, mgr);
    if (holder != g_holderDoJogador || !g_jogadorDoGancho) return;
    if (!g_temAlvo) return;
    EscreveAnguloZBruto(g_jogadorDoGancho, g_atual);
    // Depois do grafo: o jogo acabou de recalcular a velocidade, entao e aqui
    // que corrigir vale.
}

// Troca uma entrada de vtable. Nao precisa de Address Library nem de
// trampolim: o ponteiro vem do objeto vivo.
bool InstalaGancho(void* jogador)
{
    void* holder = reinterpret_cast<char*>(jogador) + kHolderOffset;
    void** vt = *reinterpret_cast<void***>(holder);
    if (!DentroDoModulo(vt)) { Loga("gancho: vtable %p fora do modulo", vt); return false; }

    const auto troca = [&](std::size_t idx, void* nosso, PreUpdate_t& guarda) {
        void* original = vt[idx];
        if (!DentroDoModulo(original)) {
            Loga("gancho: entrada 0x%zX = %p fora do modulo", idx, original);
            return false;
        }
        DWORD antes = 0;
        if (!VirtualProtect(&vt[idx], sizeof(void*), PAGE_READWRITE, &antes)) {
            Loga("gancho: VirtualProtect falhou em 0x%zX (%lu)", idx, GetLastError());
            return false;
        }
        guarda = reinterpret_cast<PreUpdate_t>(original);
        vt[idx] = nosso;
        DWORD lixo = 0;
        VirtualProtect(&vt[idx], sizeof(void*), antes, &lixo);
        Loga("gancho instalado: vtable=%p [0x%zX] original=%p -> %p",
             vt, idx, original, nosso);
        return true;
    };

    if (!troca(kIndicePreUpdate, reinterpret_cast<void*>(&PreUpdateGancho),
               g_preUpdateOriginal))
        return false;
    // O Post e um bonus: se nao der, o Pre sozinho ja e o comportamento do 6c.
    if (g_cfg.ganchoPost)
        troca(kIndicePostUpdate, reinterpret_cast<void*>(&PostUpdateGancho),
              g_postUpdateOriginal);

    g_holderDoJogador = holder;
    g_jogadorDoGancho = jogador;
    return true;
}

// Varredura do controlador. Em vez de chutar o offset da velocidade, le cada
// par de floats como se fosse (x,y) e conta quantas atualizacoes o rumo desse
// par bate com o rumo REAL do deslocamento (tirado da posicao, que nao passa
// pelo grafo). O campo certo bate quase sempre; os outros batem por acaso.




// Varredura do alvo de rotacao. Janela curta e so depois da primeira corrida
// completa: no 7b ela rodava tambem durante o carregamento, com o objeto do
// jogador ainda meio construido, e derrubava o jogo.



class TarefaPorFrame : public SFSETaskInterface::ITaskDelegate
{
public:
    void Run() override
    {
        void* jogador = Jogador();
        if (!jogador) return;

        void* holder = reinterpret_cast<char*>(jogador) + kHolderOffset;
        g_controlador = Controlador(jogador);

        // O objeto do jogador e recriado em load de save; o vtable pode ser
        // outro. Reinstala quando o ponteiro muda, e nao so uma vez na carga.
        if (jogador != g_jogadorDoGancho) {
            g_temAlvo = false;
            if (!InstalaGancho(jogador)) {
                m_ganchoFalhou = true;
            } else {
                m_ganchoFalhou = false;
                m_chamadasAntes = 0;
            }
            // O objeto do jogador e recriado ao carregar um save, mas esta
            // tarefa e global e sobrevive. Sem zerar aqui, o mod voltava com
            // `m_emMovimento` da sessao anterior, a largada nunca disparava e
            // ele ficava inerte ate a primeira parada.
            m_emMovimento = false;
            m_assentado = false;
            m_temAlvo = false;
            m_ultimoDir = -1.0f;
        }
        if (!m_velocidade) m_velocidade = Interna("Speed");
        if (!m_direcao) m_direcao = Interna("Direction");
        if (!m_primeiraPessoa) m_primeiraPessoa = Interna("IsFirstPerson");
        if (!m_cameraYaw) m_cameraYaw = Interna("fCameraYaw");
        if (!m_velocidade || !m_direcao) return;

        float px = 0.0f, py = 0.0f;
        Posicao(jogador, px, py);
        const float ddx = px - m_ultimoX;
        const float ddy = py - m_ultimoY;
        m_ultimoX = px;
        m_ultimoY = py;
        // Acumula: a posicao nao muda no mesmo quadro em que `Direction` muda,
        // entao amostrar o delta no instante da atualizacao dava sempre zero.
        m_acumX += ddx;
        m_acumY += ddy;

        // A altura e o unico sinal de "no ar" que sobrou. O vetor em +0x0D0 do
        // controlador so tem o par X/Y: o terceiro float leu 0.00 nas 168 notas
        // do rc11, entao nao e a velocidade vertical.
        const float pz = PosicaoZ(jogador);
        m_velZ = (m_ultimoZ > -1e9f) ? (pz - m_ultimoZ) : 0.0f;
        m_ultimoZ = pz;

        float vel = 0.0f, dir = 0.0f;
        if (!LeVariavel<float, kVtFloat>(holder, m_velocidade, vel)) return;
        if (!LeVariavel<float, kVtFloat>(holder, m_direcao, dir)) return;

        float pp = 0.0f;
        if (m_primeiraPessoa)
            LeVariavel<float, kVtFloat>(holder, m_primeiraPessoa, pp);

        float camAgora = 0.0f;
        const bool temCamAgora = m_cameraYaw
            && LeVariavel<float, kVtFloat>(holder, m_cameraYaw, camAgora);

        if (vel <= g_cfg.velMin || pp >= 0.5f) {
            if (m_emMovimento) Despeja();
            m_emMovimento = false;
            m_temAlvo = false;
            // Sem isto, parar e arrancar de novo NA MESMA DIRECAO deixava
            // `dir` igual ao guardado, `dadoNovo` dava falso e a largada
            // inteira era pulada: o mod seguia com o desvio da corrida
            // anterior. Era a deslizada longa a partir da segunda arrancada.
            m_ultimoDir = -1.0f;
            return;
        }

        // `Direction` e a direcao do movimento relativa ao corpo, em voltas.
        // Medido no 5g, em modo passivo: e EXATO e sem atraso — reto para tras
        // da 0.500 (180.0 graus) e para o lado 0.250 (90.0 graus), estaveis
        // desde a primeira atualizacao do movimento.
        //
        // Era isto que faltava. Derivar o rumo da posicao exige duas amostras
        // acima de um deslocamento minimo, entao o giro so podia sair 2-3
        // passos depois da largada: a deslizada nao era do jogo, era o atraso
        // do sinal que eu tinha escolhido.
        const float erro = Encurta(dir * kVolta);
        const float ang = AnguloZ(jogador);

        // A tarefa roda ~9x por atualizacao do jogo e `dir` so muda junto com a
        // atualizacao. Agir de novo no mesmo valor e correcao em malha aberta.
        const bool dadoNovo = (dir != m_ultimoDir);
        m_ultimoDir = dir;
        if (dadoNovo) {
            m_medX = m_acumX; m_medY = m_acumY;
            m_acumX = 0.0f;   m_acumY = 0.0f;
        }

        // No pulo o rumo nao representa a intencao do jogador. Nao atualiza o
        // alvo, mas segue segurando o ultimo: soltar e que torcia o corpo
        // sempre para o mesmo lado.
        // iSyncJumpState resolve mas fica cravado em 0, e a sonda de 12 nomes
        // so achou bIsLanding, que acende tarde demais. A velocidade Z do
        // bhkCharacterController acende no instante em que os pes saem do chao.
        const bool pulando = std::fabs(m_velZ) > g_cfg.limiarPuloZ;

        // A camera e o unico sinal de intencao que a nossa acao nao contamina:
        // girar o corpo nao mexe nela (cam ficou em -3.120 nas 205 notas do
        // 6a, com o boneco rodando feito piao).
        float cam = 0.0f;
        const bool temCam = m_cameraYaw
                            && LeVariavel<float, kVtFloat>(holder, m_cameraYaw, cam);

        // Ao aterrissar, recaptura. No ar o alvo fica congelado e o jogo
        // mexe no corpo em pontos onde a gente nao escreve, entao o desvio
        // chega no chao velho — e o desvio leve depois do pulo.
        if (m_estavaPulando && !pulando) m_assentado = false;
        m_estavaPulando = pulando;

        // O rc12 logou velZ=0.00 em tudo porque a unica chamada de Nota mora
        // dentro do bloco `!pulando`: os quadros de voo nunca eram anotados.
        // cod=5 e a nota do ar, e so por ela da para calibrar o limiar.
        // Anota por movimento vertical QUALQUER, nao pelo limiar: se o limiar
        // estiver errado, so a nota abaixo dele mostra isso.
        if (std::fabs(m_velZ) > 0.01f && temCam)
            Nota(holder, pulando ? 5 : 6, ang, erro, ang + erro);

        if (dadoNovo && !pulando && temCam) {
            if (!m_emMovimento) {
                // UNICA leitura de `Direction` que vale. Daqui em diante ela
                // esta envenenada: a direcao do movimento gira junto com o
                // corpo, entao realimentar por ela tem ganho POSITIVO — foi o
                // piao do 6a, e era o tremilique de antes com autoridade menor.
                m_emMovimento = true;
                g_atual = ang;
                g_saltar = true;   // largada e instantanea; so a troca e suave
                m_desvio = Encurta((ang + erro) - cam);
                m_assentado = false;
                m_rumoAnt = ang + erro;
                Nota(holder, 1, ang, erro, ang + erro);
            } else {
                // `Direction` sobe de zero na largada: no 6c o rumo foi
                // -26.3, 291.5, 333.5, 329.5, 327.4 e so entao ficou cravado.
                // Capturar no quadro 0 travava a corrida inteira num valor
                // errado — era o desvio constante. Recaptura UMA vez, quando
                // o rumo para de mudar. Continua malha aberta: um tiro so, e
                // o giro segue saindo no quadro zero, sem reintroduzir atraso.
                // `dir` e quantizado em oitavos: 0.125, 0.250, 0.500...
                // Depois que o corpo assenta ele fica em ~0 (corpo encarando o
                // movimento). Sair de la significa que o JOGADOR trocou de
                // direcao, e ai o desvio velho nao vale mais: era isso que
                // segurava o corpo 45 graus fora e produzia a diagonal longa.
                const float rumoAgora = ang + erro;
                if (m_assentado && g_cfg.recapturaTroca) {
                    // O input do jogador so assume oitavos de volta. O desvio
                    // que a NOSSA escrita provoca e menor que meio oitavo e
                    // arredonda para o mesmo degrau: no rc2 ele oscilava entre
                    // 0.045 e 0.955, que sao os dois o degrau 0. Comparar
                    // degrau, e nao distancia, separa as duas causas sem
                    // atrasar nada — exigir persistencia atrasava a troca de
                    // verdade e deixava o boneco andar de costas no meio.
                    if (Degrau(dir) != Degrau(m_dirRef)) {
                        m_assentado = false;
                        m_rumoAnt = rumoAgora;
                    }
                }
                if (!m_assentado && g_cfg.recapturaAssenta) {
                    // Estabilidade sozinha nao basta: no ar o rumo TAMBEM
                    // estabiliza (voa-se em linha reta), so que o `erro` de la
                    // nao descreve nada, e o desvio errado gravado no voo e o
                    // que aterrissa torto. Confere contra o deslocamento real
                    // medido pela posicao — no chao os dois batem (100.0 vs
                    // 99.9 nas notas), no voo divergem. Assim a recaptura ruim
                    // morre sem precisar detectar o pulo.
                    const float d2 = m_medX * m_medX + m_medY * m_medY;
                    const bool confere =
                        d2 > 1e-6f
                        && std::fabs(Encurta(rumoAgora - std::atan2(m_medX, m_medY)))
                               < kRumoConfere;
                    if (confere && fabs(Encurta(rumoAgora - m_rumoAnt)) < kRumoEstavel) {
                        m_desvio = Encurta(rumoAgora - cam);
                        m_assentado = true;
                        m_dirRef = dir;
                    }
                    m_rumoAnt = rumoAgora;
                }
                Nota(holder, m_assentado ? 3 : 4, ang, erro, rumoAgora);

                // Varre so quando houve deslocamento de verdade: sem isso o
                // rumo real e ruido e a contagem premia offset qualquer.
                // Varredura desligada: agora nos escrevemos em +0x0D0, entao
                // correlacionar esse campo com o deslocamento seria medir a
                // nossa propria escrita.
            }
        }

        // Fora do gate de dado novo: numa corrida reta o `dir` fica constante,
        // e se o alvo so fosse recalculado aqui dentro o corpo parava de
        // acompanhar a camera justamente quando ela gira.
        if (m_emMovimento && temCam) {
            m_alvo = cam + m_desvio;
            m_temAlvo = true;
        }


        // Segurar em todo quadro e o que da autoridade: o jogo reage a nossa
        // escrita girando de volta, e reescrever ~9x por atualizacao cancela.
        // MODO PASSIVO: nao escreve nada. Serve para comparar as duas medidas
        // do rumo do movimento com o jogo em vanilla, onde o 5g ja mostrou que
        // elas concordam. Se concordarem aqui e discordarem com o controle
        // ligado, quem quebra a relacao somos nos.
        // A escrita nao acontece mais aqui. Esta tarefa so decide o alvo; quem
        // escreve e o gancho, uma vez por atualizacao do grafo. Se o gancho
        // nao instalou, cai no caminho antigo para nao ficar sem mod nenhum.
        g_alvo = m_alvo;
        g_temAlvo = (!kSomenteObservar && m_temAlvo && !m_ganchoFalhou);

        if (!kSomenteObservar && m_temAlvo && m_ganchoFalhou)
            EscreveAnguloZ(jogador, m_alvo);
    }

    void Destroy() override {}

private:
    // Diagnostico: guarda as primeiras atualizacoes de cada arrancada em
    // memoria e so escreve no disco quando a corrida termina. Codigos:
    // 1 largada, 3 regime (desvio ja recapturado), 4 rumo ainda assentando,
    // 9 deslocamento
    // abaixo do minimo. Todos os angulos em graus, ja depois dos limites.
    // gan = quantas vezes o gancho rodou desde a nota anterior. Se o
    // PreUpdate e mesmo uma vez por atualizacao do grafo, isso tem que dar 1.
    // Se der ~9, o ponto esta errado e a teoria do timing cai aqui, no log,
    // sem precisar de teste no jogo.
    struct Nota_t { int cod; float ang, erro, rumo, med, desl, dir, cam, desv, pulo, v0, vel; long gan; };
    static constexpr std::size_t kNotas = 40;
    static constexpr std::size_t kArrancadas = 8;
    Nota_t      m_notas[kNotas]{};
    std::size_t m_nNotas{ 0 };
    std::size_t m_arrancada{ 0 };

    // -999 significa "a variavel nao pode ser lida". Distinguir isso de zero
    // importa: uma variavel ausente lida como 0 ja me enganou antes.
    float Le(void* holder, void* nome)
    {
        float v = -999.0f;
        if (nome) LeVariavel<float, kVtFloat>(holder, nome, v);
        return v;
    }

    // igual ao Le(), mas pelo acessor inteiro
    float LeInt(void* holder, void* nome)
    {
        std::int32_t v = 0;
        if (!nome || !LeVariavel<std::int32_t, kVtInt>(holder, nome, v))
            return -999.0f;
        return static_cast<float>(v);
    }

    void Nota(void* holder, int cod, float ang, float erro, float rumo)
    {
        if (m_arrancada >= kArrancadas || m_nNotas >= kNotas) return;
        const float d2 = m_medX * m_medX + m_medY * m_medY;
        // -999 = deslocamento pequeno demais para o rumo significar algo
        const float med = (d2 > 1e-6f)
                        ? std::atan2(m_medX, m_medY) * 57.2958f : -999.0f;
        m_notas[m_nNotas++] = { cod, ang * 57.2958f, erro * 57.2958f,
                                rumo * 57.2958f, med, std::sqrt(d2),
                                Le(holder, m_direcao),
                                Le(holder, m_cameraYaw),
                                m_desvio * 57.2958f,
                                m_velZ,
                                g_controlador ? RumoDoVetor(g_controlador, 0x0D0) : -999.0f,
                                Le(holder, m_velocidade),
                                g_chamadas - m_chamadasAntes };
        m_chamadasAntes = g_chamadas;
    }

    void Despeja()
    {
        if (!m_nNotas || m_arrancada >= kArrancadas) { m_nNotas = 0; return; }
        Loga("[arrancada %zu]", ++m_arrancada);
        for (std::size_t i = 0; i < m_nNotas; ++i) {
            const auto& n = m_notas[i];
            Loga("  %2zu cod=%d ang=%7.1f erro=%7.1f rumo=%7.1f "
                 "med=%7.1f d=%6.3f dir=%6.3f desv=%7.1f v0=%7.1f vel=%6.2f velZ=%6.2f gan=%ld",
                 i, n.cod, n.ang, n.erro, n.rumo, n.med, n.desl, n.dir,
                 n.desv, n.v0, n.vel, n.pulo, n.gan);
        }
        m_nNotas = 0;
    }

    // A corrida para tras ficava torta ~5 graus para um lado: e exatamente o
    // erro que a zona morta permite em regime. Estreitada para ~2 graus.
    static constexpr bool  kSomenteObservar = false;
    // `Speed` sobe de zero na largada. Em 0.5 a gente so percebia o movimento
    // depois que o jogo ja tinha travado a direcao usando o angulo velho do
    // corpo — o corpo virava uma atualizacao tarde e os pes iam para o rumo
    // antigo durante toda a aceleracao (6h, arrancada 1: corpo em 28.7,
    // movimento real em 164, que e o "de costas" do angulo de ANTES do giro).
    static constexpr float kVelMin = 0.01f;
    // ~3 graus: no 6c os passos do rumo caem de 40+ para menos de 2 ao assentar
    static constexpr float kRumoEstavel = 0.052f;
    // 30 graus: folga larga o bastante para o ruido do deslocamento medido,
    // estreita o bastante para barrar o voo, onde a divergencia e grande.
    static constexpr float kRumoConfere = 0.52f;
    // meio degrau do quantizador (1/8 de volta): separa troca real de ruido

    static void EscreveAnguloZ(void* jogador, float v)
    {
        while (v < 0.0f) v += kVolta;
        while (v >= kVolta) v -= kVolta;
        *reinterpret_cast<float*>(reinterpret_cast<char*>(jogador) + kAnguloZ) = v;
    }

    void* m_velocidade{ nullptr };
    void* m_direcao{ nullptr };
    void* m_cameraYaw{ nullptr };
    void* m_primeiraPessoa{ nullptr };
    bool  m_ganchoFalhou{ true };
    long  m_chamadasAntes{ 0 };
    bool  m_emMovimento{ false };
    bool  m_temAlvo{ false };
    float m_alvo{ 0.0f };
    float m_desvio{ 0.0f };
    float m_rumoAnt{ 0.0f };
    bool  m_assentado{ false };
    float m_dirRef{ 0.0f };
    bool  m_estavaPulando{ false };
    float m_velZ{ 0.0f };
    float m_ultimoZ{ -1e30f };
    float m_ultimoDir{ -1.0f };
    float m_ultimoX{ 0.0f }, m_ultimoY{ 0.0f };
    float m_medX{ 0.0f }, m_medY{ 0.0f };
    float m_acumX{ 0.0f }, m_acumY{ 0.0f };
};

TarefaPorFrame g_tarefa;

}  // namespace

extern "C" {

__declspec(dllexport) SFSEPluginVersionData SFSEPlugin_Version =
{
    SFSEPluginVersionData::kVersion,

    2,
    "sf360",
    "raelg",

    SFSEPluginVersionData::kAddressIndependence_AddressLibraryV2,
    SFSEPluginVersionData::kStructureIndependence_NoStructs,
    { RUNTIME_VERSION_1_16_244, 0 },

    0,
    0, 0,
};

__declspec(dllexport) bool SFSEPlugin_Load(const SFSEInterface* sfse)
{
    CarregaConfig();
    Loga("--- sf360 0.1.0 ---");
    Loga("config: ganchoPost=%d recapturaAssenta=%d recapturaTroca=%d "
         "passoMax=%.3f",
         g_cfg.ganchoPost, g_cfg.recapturaAssenta, g_cfg.recapturaTroca,
         g_cfg.passoMax);
    Loga("sfse %u.%u.%u | runtime %u.%u.%u",
         GET_EXE_VERSION_MAJOR(sfse->sfseVersion),
         GET_EXE_VERSION_MINOR(sfse->sfseVersion),
         GET_EXE_VERSION_BUILD(sfse->sfseVersion),
         GET_EXE_VERSION_MAJOR(sfse->runtimeVersion),
         GET_EXE_VERSION_MINOR(sfse->runtimeVersion),
         GET_EXE_VERSION_BUILD(sfse->runtimeVersion));

    char exe[MAX_PATH]{};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string dir(exe);
    dir = dir.substr(0, dir.find_last_of('\\'));
    const std::string lib = dir + "\\Data\\SFSE\\Plugins\\versionlib-1-16-244-0.bin";

    if (!CarregaVersionLib(lib.c_str())) {
        Loga("FALHOU ao carregar %s", lib.c_str());
        return true;  // carrega mesmo assim, para o log explicar o motivo
    }

    Loga("player singleton = %p | GetEntry = %p",
         reinterpret_cast<void*>(Endereco(kIdPlayerSingleton)),
         reinterpret_cast<void*>(Endereco(kIdGetEntry)));

    // A interface de tarefas do SFSE executa na thread principal a cada frame.
    // E o que dispensa hook: nao precisamos interceptar funcao nenhuma para
    // rodar no ritmo do jogo.
    auto tarefas = static_cast<SFSETaskInterface*>(
        sfse->QueryInterface(kInterface_Task));
    if (tarefas && tarefas->AddTaskPermanent) {
        tarefas->AddTaskPermanent(&g_tarefa);
        Loga("tarefa por frame registrada (interface %u)", tarefas->interfaceVersion);
    } else {
        Loga("FALHOU: sem interface de tarefas do SFSE");
    }

    return true;
}

}  // extern "C"

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
