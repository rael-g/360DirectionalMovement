# Relatos abertos, por custo

Tudo que está aberto depois da 0.2.1, ordenado pelo que custa consertar e não
pela ordem em que chegou. Fonte de cada item entre parênteses.

O eixo é: quanto disso já dá para tentar com o código que existe, e quanto
exige descobrir algo novo no jogo primeiro.

---

## Barato: um veto no state gate

Todos abaixo são a mesma correção, uma linha cada, e já estão na rc1.

**Agachado fica rápido demais, como câmera flutuante** (Jujub3ans). O gate nunca
mencionou o agachamento, então o mod gira o corpo enquanto o jogo toca a
locomoção furtiva.

`iIsInSneak` foi a primeira tentativa e **não funcionou**: na rc1 o mod
continuou girando agachado. O nome existe na tabela, o valor não descreve o que
se supôs. A rc2 loga o bruto dele para resolver isso numa sessão.

O autor aceita desligar o mod inteiro enquanto agachado, como já é com a arma na
mão, mesmo gostando de andar agachado olhando para trás. Também notou que parar
agachado olhando para trás faz o personagem virar para frente sozinho.

**Pulo com mochila para trás bate em parede invisível** e **desvio de rota
durante o pulo** (autor).

Aqui o veto foi o remédio errado, testado e reprovado na rc1: soltar a rotação
no ar devolve o corpo ao jogo, que o gira para frente no meio do salto. Correr
de costas e pular vira um giro no ar.

O certo é **congelar**: manter o rumo com que o corpo deixou o chão, sem
recapturar e sem seguir a câmera. `iSyncJumpState` funciona como sinal, provado
justamente por o veto ter agido.

Risco: o valor de repouso dessas duas variáveis não foi medido. Precedente do
`iLadderClimbState`, que repousa em -1, faria um teste contra zero desabilitar o
mod no jogo inteiro. Por isso o veto novo só aceita valor **positivo** como
estado ativo.

---

## Barato: já é configuração, faltava expor

**Correr para os lados e para trás** (AleGR93, autor). **Não é correção, é
feature**, decidido pelo autor depois da rc1: o jogo base também não corre de
lado, então o mod não está quebrando nada, está deixando de acrescentar. Sai do
caminho crítico.

Metade é barata: o sprint era vetado no gate por girar bruto demais, e agora tem
`allowSprint`.

A outra metade não é nossa: o jogo só engata sprint com a tecla de frente
pressionada. AleGR93 propõe marcar as outras teclas como capazes de sprint, o
que é entrada, não animação, e provavelmente fora do alcance do gate.

Existe uma chance de sair de graça: como o mod alinha o corpo à direção do
movimento, depois de alinhado o andar já é para frente em relação ao corpo. Se a
condição de sprint do jogo olhar o grafo em vez da tecla, ligar `allowSprint`
pode bastar. É um teste, não uma previsão.

---

## Médio: histerese na troca de direção

**Pernas em riverdance andando de frente para a câmera, no analógico**
(MadBriantist). **Zig zag rápido** (autor). **Alternar frente-direita com
frente-esquerda** (autor, desde a 0.2.1).

Provavelmente o mesmo defeito. A recaptura do rumo dispara a cada troca de
octante, e um analógico atravessa octante no caminho para qualquer lugar. O
corpo persegue cada roçada de fronteira.

`directionHold` exige que a direção nova se sustente alguns milissegundos antes
do corpo obedecer. Padrão 0.08 s na rc1, escolhido por ser cerca de oito
atualizações de grafo no intervalo medido de 10,7 ms. É chute educado e o número
precisa de teste.

**O relato do modelo "batendo em algo invisível" ao girar a câmera andando**
(MadBriantist) não está explicado por isso e continua aberto.

---

## Médio: a dependência das blank files

**Deslizada no início do movimento**, hoje escondida por 506 arquivos vazios que
o zip instala.

A hipótese que junta três coisas: o `rotator_reset` teleporta o corpo para o
rumo novo quando o movimento começa. O jogo então toca a animação de arranque da
direção que a **tecla** pediu, por exemplo `relaxed_runbackward_start`, com o
corpo já virado para lá. O root motion dessa animação empurra para o lado
errado, e isso é a deslizada. Blankar removeu o empurrão junto com a animação.

Se for isso, tirar o teleporte faz a animação de arranque certa tocar sozinha, e
as blank files deixam de ser necessárias. `snapOnStart` desliga o teleporte.

Por isso a rc sai em duas versões, com e sem os arquivos vazios.

**A rc1 não chegou a testar isso.** O log mostra `snapOnStart=1` na única sessão,
então tirar os arquivos vazios só devolveu a deslizada que eles escondiam, que é
o resultado esperado e não diz nada sobre a hipótese. O teste de verdade é o zip
sem vazios **com `snapOnStart=0`**.

Formato medido da deslizada, sem os vazios: correndo para trás, uma escorregada
curta do tamanho de um passo; para os lados, uma longa na diagonal, uns 2,5 m.

---

## Médio: a agachada no giro de 180

Com o mod desligado, o autor confirmou que a agachada **é animação do jogo**: o
personagem freia e passa a andar de costas. Com o mod ligado, o corpo gira por
baixo dela.

AleGR93 propõe deixar a animação terminar antes de ajustar o movimento, o que
exige saber quando ela está tocando. O paliativo do autor, blankar a freada, é
mais barato, mas o arquivo ainda não foi identificado: `runforwardtobackward` já
está na lista de vazios e a agachada continua, então é outro.

---

## Médio: deslizada no modo walk

**No analógico, andando bem devagar, o boneco desliza** (autor).

Pode ser o mesmo root motion das blank files aparecendo numa velocidade em que
o passo é curto, ou pode ser a rotação contra uma animação de caminhada lenta.
Ainda não separado.

---

## Caro: o easing, segundo AleGR93

Ele separou dois giros que a gente vinha tratando como um só, e a distinção é
boa:

**Guiado pela câmera.** Segurar a frente e girar a câmera. O jogo mantém a
animação de andar para frente e aplica uma **inclinação** no corpo, com três
intensidades, uma por velocidade. O mod não faz nada disso.

**Guiado pelas teclas.** Câmera parada, WASD muda a direção. É o que o mod faz, e
onde ele ainda parece mais seco que o original.

O diagnóstico dele: no jogo o giro tem entrada e saída suavizadas, uma curva em
S; no mod a mola amortecida suaviza principalmente a chegada. A referência é o
vídeo que ele mandou, de 2:50 a 6:50.

Ele também nota que sair do sprint para trotar de lado ou para trás troca sem
transição.

Isso é a maior sugestão recebida até hoje e é a mais cara: inclinação é
território de animação, não de rotação, e o mod só sabe escrever ângulo.

---

## Recursos de terceiros avaliados

**Real Time Form Patcher** (nexusmods.com/starfield/mods/8324) e **StarPatcher**
(mods/12855) editam registro e GameSettings em tempo de execução por config, sem
esp. Não servem para o mod, que não mexe em registro. Servem como **bancada**.

A pista: o AleGR93 observou que a velocidade com que o jogo gira o boneco parado
**muda entre andar e trotar**. Velocidade por marcha não parece GMST, parece
campo de registro; engines anteriores da Bethesda guardam isso num registro de
tipo de movimento, com velocidades por marcha incluindo girar no lugar.

Se existir no Starfield e o patcher alcançar, dá para mexer na rotação do jogo
sem compilar e sem publicar esp. **Não confirmado:** não há documentação do
formato. Verificação é abrir o Creation Kit ou o xEdit e procurar a categoria,
meia hora, não um build.

Vale só onde o jogo é quem gira, o giro parado e a inclinação com a câmera. Onde
escrevemos o ângulo direto nada disso é consultado. Entre os dois, o RTFP, que
declara mexer em GameSettings sem desativar conquistas.

**Ez-SFSE-Plugins** (mods/14577) e os templates de CommonLibSF não têm ganho: o
sf360 é C puro, não linka a lib, e a mantém clonada como referência de layout,
que é como a vtable do holder foi lida. Trocar seria recomeçar por nada.

Nenhum deles passa na frente de descobrir se os slots 0x0F e 0x10 são os setters
de variável de grafo. O patcher é atalho para um item; o setter derruba vários.

---

## Caro: o teto de sempre

Giramos escrevendo o ângulo do ator, o que não informa nada ao grafo de
animação. As pernas seguem no ciclo anterior enquanto o corpo vira por baixo.
Quando o jogo gira sozinho, ele arrasta a animação junto.

AleGR93 chegou na mesma conclusão por fora, ao propor "parado, gira primeiro,
depois anda", que é exatamente o que o jogo faz e o mod não.

---

## O que a rc1 testa

Cada item abaixo tem chave própria no ini para poder ser isolado em jogo.

| chave | padrão | serve para |
|---|---|---|
| `yieldWhenSneaking` | 1 | agachado |
| `yieldWhenJumping` | 1 | pulo, mochila, desvio no ar |
| `allowSprint` | 0 | correr de lado, ligar para testar |
| `directionHold` | 0.08 | riverdance, zig zag |
| `snapOnStart` | 1 | desligar junto com o zip sem blank files |

Dois zips: com e sem os arquivos vazios.
