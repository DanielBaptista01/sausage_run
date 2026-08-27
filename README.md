# Sausage Run

Jogo 2D top-down em C com Allegro 5. O jogador controla o Dachshund e precisa encontrar a bolinha de cada cômodo sem ser pego pela Maria.

## Mecânicas implementadas

- 4 fases: Cozinha, Sala, Banheiro e Quarto.
- Sprites animadas de Maria e do Dachshund nas quatro direções.
- Dachshund: idle, caminhada, corrida, latido, mordida e bola na boca.
- Cinco cores de bolinha: amarela, verde, roxa, azul e vermelha.
- IA da Maria com estados de patrulha, investigação, perseguição, busca e captura.
- Cone de visão com bloqueio pelos móveis.
- Campo circular de audição.
- Corrida gera ruído de alcance médio.
- Latido gera ruído de alcance maior e funciona como distração: Maria investiga a posição em que ouviu o som, e não a posição atual do cachorro.
- Pathfinding em grade por BFS para Maria contornar os obstáculos.
- Colisão com móveis, eletrodomésticos e objetos.
- Profundidade visual simples: alguns móveis podem aparecer na frente dos personagens durante a passagem.
- Três vidas, progressão de fase, vitória e game over.
- Quarto com cama do casal, berço aberto da Maria e objetos adequados para uma criança pequena.
- Modo de depuração com visão, audição, colisões, saída da fase e caminho da IA.

## Controles

| Tecla | Ação |
| --- | --- |
| `W A S D` | Mover o Dachshund |
| `Shift + W A S D` | Correr e produzir ruído |
| `Espaço` | Latir / criar uma distração sonora |
| `E` | Morder; se a bolinha estiver à frente e próxima, pegá-la |
| `F1` | Mostrar/ocultar debug da IA |
| `R` | Reiniciar depois de vitória ou game over |
| `Esc` | Sair |

## Objetivo

1. Explore o cômodo e encontre a bolinha.
2. Use móveis e objetos para quebrar a linha de visão da Maria.
3. Use o latido para atrair Maria para uma posição e escape pelo lado oposto.
4. Evite correr perto dela, porque a corrida também pode ser ouvida.
5. Aproxime-se da bolinha, olhe na direção dela e pressione `E`.
6. Com a bolinha na boca, alcance a saída do cômodo.
7. Conclua as quatro fases.

## Estrutura do código

- `main.c` — inicialização, game loop, vidas e progressão das fases.
- `jogo.h` — estruturas, enums, constantes e protótipos.
- `recursos.c` — carregamento das imagens e animações.
- `fase.c` — configuração dos cômodos, móveis, colisões, spawns e saídas.
- `scooby.c` — controles, corrida, latido, mordida e coleta da bolinha.
- `ia.c` — visão, audição, máquina de estados e pathfinding da Maria.
- `render.c` — mapas, objetos, personagens, HUD e debug.
- `util.c` — matemática, colisões básicas, reset e progressão.
- `quarto_objetos_data.h` — pequena folha de objetos do quarto embutida para não depender de um PNG adicional.

## Allegro

O código usa:

- Allegro 5 Core
- Allegro Primitives
- Allegro Image

Os bitmaps são procurados no diretório atual e também em `../`, `../../` e `../../../`, para facilitar a execução pelo Visual Studio quando o executável fica em `Debug`, `x64/Debug` ou pastas semelhantes.

## Observação sobre áudio

A audição da Maria já está implementada como lógica de gameplay. O repositório ainda não possui arquivos de áudio de latido/passos, portanto a versão atual não reproduz o som real; latido e corrida já geram os eventos sonoros usados pela IA.
