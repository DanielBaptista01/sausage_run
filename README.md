# Sausage Run

Jogo 2D top-down em C com Allegro 5. O jogador controla Scooby, um dachshund que precisa encontrar a bolinha de cada cômodo e alcançar a saída sem ser capturado por Maria.

Esta versão segue o plano `sausage_run_plano_correcao_completo.mmd` usado como especificação de correção do projeto.

## Build oficial

O projeto foi padronizado para **x64**:

- `Debug|x64` usa somente bibliotecas Allegro `-debug`.
- `Release|x64` usa somente bibliotecas Allegro release.
- Win32 foi removido da configuração oficial para evitar builds parcialmente configurados.

Addons usados:

- Allegro Core
- Allegro Primitives
- Allegro Image
- Allegro Audio
- Allegro Font

## Controles

| Tecla | Ação |
| --- | --- |
| `W A S D` | Mover Scooby |
| `Shift + W A S D` | Correr e produzir ruído |
| `Espaço` | Latir e criar uma distração sonora |
| `E` | Morder / pegar a bolinha quando ela estiver à frente |
| `F1` | Ativar/desativar debug determinístico |
| `Esc` | Pausar durante a partida; sair nas telas finais |
| `R` | Jogar novamente depois de vitória ou game over |

## Mecânicas principais

- 4 fases: Cozinha, Sala, Banheiro e Quarto.
- Maria patrulha o cômodo procurando Scooby.
- Cone de visão com bloqueio exato por obstáculos.
- Campo circular de audição.
- Corrida produz som de alcance médio.
- Latido produz som de alcance maior e pode ser usado como distração.
- Maria investiga a posição do som, não a posição atual de Scooby.
- Estados da IA: patrulha, investigação, perseguição, busca e captura.
- Pathfinding BFS com clearance baseado na hitbox real da Maria.
- Captura só ocorre se Maria estiver fisicamente próxima e sem móvel bloqueando o caminho.
- Durante a captura Scooby fica sem controle.
- A captura tem prioridade sobre conclusão de fase.
- Três vidas.
- Bolinha em cinco cores.
- Transição de fase com caminhada lenta até a saída e fade out/fade in.
- Saída recebe indicação visual depois que Scooby pega a bolinha.
- HUD dentro do jogo.
- Tutorial inicial.
- Pausa ao pressionar `Esc` ou ao perder o foco da janela.
- Efeitos sonoros procedurais sincronizados aos frames principais das animações.

## Sprites

As animações não usam mais um `FRAME_SPRITE` fixo.

Ao carregar uma sprite sheet, o jogo:

1. lê largura e altura reais com `al_get_bitmap_width/height`;
2. valida se a folha é divisível por 4 frames e 4 direções;
3. calcula `frameW` e `frameH`;
4. usa esses valores para `sx` e `sy`.

Se uma folha não tiver dimensões compatíveis, o erro é exibido claramente no console.

Sprites da bolinha na boca:

- `ScoobySprites/littleBalls/yellow_dog.png`
- `ScoobySprites/littleBalls/green_dog.png`
- `ScoobySprites/littleBalls/purple_dog.png`
- `ScoobySprites/littleBalls/blue_dog.png`
- `ScoobySprites/littleBalls/red_dog.png`

## Colisão e profundidade

- Cada fase possui uma área física caminhável própria.
- Existem paredes invisíveis perimetrais por fase.
- Hitboxes dos personagens representam a região dos pés, não toda a imagem da sprite.
- Móveis usam colisão somente na base física.
- Isso permite Scooby e Maria passarem visualmente atrás de objetos altos.
- Cada objeto possui `anchorY` próprio para ordenação de profundidade.
- Objetos, Maria, Scooby e bolinha entram na mesma fila de renderização ordenada por Y.

## IA / pathfinding

O BFS foi corrigido para:

- mover diretamente quando origem e destino estão na mesma célula;
- guardar o alvo navegável efetivo quando o ponto original é bloqueado;
- inflar obstáculos pela metade da largura/altura da Maria;
- evitar estados de investigação presos em pontos impossíveis.

A linha de visão usa interseção segmento-retângulo, substituindo a amostragem por pontos.

Com `F1`, a seed é fixada para facilitar reprodução dos testes.

## Transição de fase

Quando Scooby alcança a saída carregando a bolinha:

1. controles são bloqueados;
2. Maria para de perseguir/capturar;
3. Scooby anda lentamente até o centro da porta/escada;
4. a tela faz fade para preto;
5. somente com a tela escura a próxima fase é carregada;
6. a nova fase aparece com fade in.

Não existe troca instantânea de fase.

## Recursos

Os assets são resolvidos a partir de uma raiz de recursos detectada uma única vez. O jogo não depende de alterar globalmente o `current directory`.

Para PNGs, o carregamento tenta:

1. `al_load_bitmap`;
2. no Windows, fallback WIC quando necessário.

O fallback de `quarto_objetos_runtime.png` é recriado quando o arquivo existente não pode ser decodificado.

Fundos e folhas de objetos são carregados por fase para reduzir uso de memória. Sprites dos personagens e bolinhas permanecem globais.

## Estrutura do código

- `main.c` — game loop, pausa, vidas, prioridade de captura e transições.
- `jogo.h` — structs, enums, constantes e protótipos.
- `recursos.c` — resolução de paths, sprites, validação, WIC e recursos por fase.
- `fase.c` — objetos, colisões, limites físicos, saídas e level design.
- `scooby.c` — movimentação, corrida, latido, mordida, coleta e saída animada.
- `ia.c` — visão, audição, captura e pathfinding.
- `render.c` — profundidade por Y, HUD, tutorial, debug, pause e fades.
- `util.c` — colisão de personagens, áreas caminháveis e reset.
- `audio.c` — efeitos sonoros procedurais.

## Checklist de validação

Testar todas as quatro fases verificando:

- nenhum personagem atravessa parede externa;
- nenhum objeto lê pixels fora da própria região da sprite sheet;
- profundidade correta na frente/atrás dos móveis;
- bola participa da ordenação por Y;
- Maria não captura através de móveis;
- Scooby fica imóvel durante captura;
- fase não termina durante captura;
- pathfinding contorna obstáculos;
- saída só funciona com a bolinha;
- fade de transição funciona;
- idle/walk/run/bark/bite/carry e Maria idle/walk/run/pick não vazam pixels de linhas vizinhas;
- Debug x64 e Release x64 usam bibliotecas Allegro coerentes.
