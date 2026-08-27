#ifndef JOGO_H
#define JOGO_H

#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_font.h>

#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LARGURA_TELA 1280
#define ALTURA_TELA 720
#define FPS 60.0f
#define PI 3.14159265358979323846f

#define MAPA_ORIGINAL_W 1448.0f
#define MAPA_ORIGINAL_H 1086.0f
#define MAPA_TELA_H 720.0f
#define MAPA_ESCALA (MAPA_TELA_H / MAPA_ORIGINAL_H)
#define MAPA_TELA_W (MAPA_ORIGINAL_W * MAPA_ESCALA)
#define MAPA_X ((LARGURA_TELA - MAPA_TELA_W) / 2.0f)
#define MAPA_Y 0.0f

#define QTD_FRAMES 4
#define QTD_DIRECOES 4

#define MAX_OBJETOS 32
#define MAX_OBSTACULOS 48
#define MAX_WAYPOINTS 10
#define MAX_CAMINHO 512

#define TAM_CELULA 40
#define GRID_COLS 24
#define GRID_ROWS 18
#define GRID_TOTAL (GRID_COLS * GRID_ROWS)

#define QTD_FASES 4
#define QTD_CORES_BOLA 5
#define SEED_DEBUG 1337u

typedef enum { DIRECAO_DOWN = 0, DIRECAO_UP, DIRECAO_LEFT, DIRECAO_RIGHT } Direcao;
typedef enum { MARIA_PATRULHA, MARIA_INVESTIGAR, MARIA_PERSEGUIR, MARIA_PROCURAR, MARIA_CAPTURAR } EstadoMaria;
typedef enum { SOM_NENHUM, SOM_CORRIDA, SOM_LATIDO, SOM_INTERACAO } TipoSom;
typedef enum { JOGO_RODANDO, JOGO_TRANSICAO_FASE, JOGO_PAUSADO, JOGO_VITORIA, JOGO_GAME_OVER } EstadoJogo;
typedef enum { SAIDA_PORTA, SAIDA_ESCADA } TipoSaida;
typedef enum { TRANSICAO_APROXIMAR, TRANSICAO_FADE_OUT, TRANSICAO_FADE_IN } EtapaTransicao;

typedef struct { float x, y; } Ponto;
typedef struct { float x, y, largura, altura; } Retangulo;

typedef struct {
    ALLEGRO_BITMAP* imagem;
    int frameAtual;
    int frameW, frameH;
    int qtdFrames, qtdDirecoes;
    int gridOffsetX, gridOffsetY;
    int cropInset;
    float acumulador;
    float tempoFrame;
    float anchorX, anchorY;
    int ultimoFrameSom;
} Animacao;

typedef struct {
    float x, y;              /* ponto de contato com o chao / pe */
    float largura, altura;   /* hitbox fisica dos pes */
    float velocidade;
    float direcao;
} Personagem;

typedef struct {
    TipoSom tipo;
    float x, y, alcance, tempoRestante;
    bool ativo, processado;
} EventoSom;

typedef struct {
    float x, y, largura, altura;
    bool bloqueiaMovimento, bloqueiaVisao;
} Obstaculo;

typedef struct {
    int sx, sy, sw, sh;
    int insetFonte;
    float mapaX, mapaY, escala;
    float colX, colY, colW, colH;
    float anchorY;
    bool colide, bloqueiaVisao;
} ObjetoMapa;

typedef struct {
    ALLEGRO_BITMAP* fundo;
    ALLEGRO_BITMAP* folhaObjetos;
    const char* caminhoFundo;
    const char* caminhoObjetos;
    const char* nome;
    ObjetoMapa objetos[MAX_OBJETOS];
    int quantidadeObjetos;
    Obstaculo obstaculos[MAX_OBSTACULOS];
    int quantidadeObstaculos;
    Ponto waypoints[MAX_WAYPOINTS];
    int quantidadeWaypoints;
    Ponto spawnsBola[3];
    int quantidadeSpawnsBola;
    Ponto spawnScooby, spawnMaria;
    Retangulo areaJogavel;
    Retangulo saida;
    Ponto alvoTransicao;
    TipoSaida tipoSaida;
} Fase;

typedef struct {
    ALLEGRO_BITMAP* bolas;
    ALLEGRO_FONT* fonte;
    int faseCarregada;
} RecursosMapa;

typedef struct {
    ALLEGRO_SAMPLE* scoobyPasso;
    ALLEGRO_SAMPLE* scoobyCorrida;
    ALLEGRO_SAMPLE* scoobyLatido;
    ALLEGRO_SAMPLE* scoobyMordida;
    ALLEGRO_SAMPLE* coletaBola;
    ALLEGRO_SAMPLE* mariaPasso;
    ALLEGRO_SAMPLE* mariaCorrida;
    ALLEGRO_SAMPLE* mariaAlerta;
    ALLEGRO_SAMPLE* captura;
    ALLEGRO_SAMPLE* faseCompleta;
    ALLEGRO_SAMPLE* vitoria;
    ALLEGRO_SAMPLE* gameOver;
    bool disponivel;
} RecursosAudio;

typedef struct {
    Personagem corpo;
    Direcao direcaoSprite;
    bool movendo, correndo, latindo, mordendo;
    bool carregandoBola, coletaPendente;
    float cooldownSomCorrida;
    Animacao idle, walk, run, bark, bite;
    Animacao carregar[QTD_CORES_BOLA];
} Scooby;

typedef struct {
    Personagem corpo;
    Direcao direcaoSprite;
    EstadoMaria estado;
    float alcanceVisao, anguloVisao, alcanceAudicao;
    float alvoX, alvoY;
    float alvoNavegavelX, alvoNavegavelY;
    float ultimaPosicaoVistaX, ultimaPosicaoVistaY;
    int waypointAtual;
    float tempoBusca, tempoNovoAlvoBusca;
    Ponto caminho[MAX_CAMINHO];
    int quantidadeCaminho, indiceCaminho;
    float tempoRecalcularCaminho;
    float ultimoAlvoCaminhoX, ultimoAlvoCaminhoY;
    bool movendo, capturaConcluida;
    float antiStuckTempo;
    float antiStuckUltimoX, antiStuckUltimoY;
    Animacao idle, walk, run, pick;
} Maria;

typedef struct {
    float x, y;
    int cor;
    bool coletada;
} Bola;

typedef struct {
    EtapaTransicao etapa;
    float tempo;
    float alphaFade;
    Ponto alvo;
    bool faseTrocada;
} TransicaoFase;

extern const char* NOMES_CORES[QTD_CORES_BOLA];

float distancia(float x1, float y1, float x2, float y2);
float normalizarAngulo(float angulo);
float mapaParaTelaX(float x);
float mapaParaTelaY(float y);
bool pontoDentroRetangulo(float x, float y, Retangulo r);
bool pontoDentroObstaculo(float x, float y, Obstaculo o);
Direcao direcaoSpritePorMovimento(float dx, float dy, Direcao atual);
Retangulo hitboxPersonagem(const Personagem* p, float x, float y);
bool personagemDentroArea(const Personagem* p, float x, float y, const Fase* fase);
bool personagemColide(const Personagem* p, float novoX, float novoY, const Fase* fase);
void moverPersonagem(Personagem* p, float dx, float dy, const Fase* fase);
bool spawnEhValido(const Personagem* p, float x, float y, const Fase* fase, float margem);
bool procurarPontoLivreProximo(const Personagem* p, const Fase* fase, Ponto origem, Ponto* resultado);
void validarConfiguracaoFase(Fase* fase, const Scooby* scooby, const Maria* maria, int indiceFase);
void resetarPersonagensNaFase(Scooby* scooby, Maria* maria, Bola* bola,
                              const Fase* fase, int faseAtual, bool novaCor);
bool chegouNaSaidaComBola(const Scooby* scooby, const Fase* fase);

bool inicializarRaizRecursos(void);
bool resolverCaminhoRecurso(const char* relativo, char* saida, size_t tamanho);
ALLEGRO_BITMAP* carregarBitmapFlexivel(const char* caminho);
bool carregarAnimacao(Animacao* animacao, const char* caminho, float tempoFrame);
void configurarAnchorAnimacao(Animacao* a, float anchorX, float anchorY);
void reiniciarAnimacao(Animacao* animacao);
void atualizarAnimacaoLoop(Animacao* animacao, float dt);
bool atualizarAnimacaoUmaVez(Animacao* animacao, float dt);
void desenharAnimacao(const Animacao* animacao, Direcao direcao, float x, float y, float escala);
bool carregarRecursosMapa(RecursosMapa* r);
bool carregarRecursosFase(RecursosMapa* r, Fase fases[QTD_FASES], int indiceFase);
bool carregarSprites(Scooby* scooby, Maria* maria);
void descarregarFase(Fase* fase);
void destruirRecursos(RecursosMapa* r, Fase fases[QTD_FASES], Scooby* scooby, Maria* maria);

bool criarRecursosAudio(RecursosAudio* audio);
void tocarEfeito(ALLEGRO_SAMPLE* sample, float ganho);
void tocarEfeitoPosicional(ALLEGRO_SAMPLE* sample, float x, float ganho);
void destruirRecursosAudio(RecursosAudio* audio);

void configurarFases(Fase fases[QTD_FASES]);
void reconstruirColisoesFase(Fase* fase);
void desenharObjeto(const Fase* fase, const ObjetoMapa* obj);
float baseYObjeto(const ObjetoMapa* obj);
bool validarObjetosFase(const Fase* fase);

void emitirSom(EventoSom* som, TipoSom tipo, float x, float y, float alcance);
void atualizarSom(EventoSom* som, float dt);
bool mariaOuveSom(const Maria* maria, const EventoSom* som);
bool linhaVisaoLivre(const Fase* fase, float x1, float y1, float x2, float y2);
bool mariaVeScooby(const Maria* maria, const Scooby* scooby, const Fase* fase);
bool mariaPodeCapturar(const Maria* maria, const Scooby* scooby, const Fase* fase);
void atualizarMaria(Maria* maria, const Scooby* scooby, EventoSom* som,
                    const Fase* fase, RecursosAudio* audio, float dt);

bool bolaNaFrenteDoScooby(const Scooby* scooby, const Bola* bola);
void iniciarLatido(Scooby* scooby, EventoSom* som, RecursosAudio* audio);
void iniciarMordida(Scooby* scooby, const Bola* bola, RecursosAudio* audio);
void atualizarScooby(Scooby* scooby, const ALLEGRO_KEYBOARD_STATE* teclado,
                     const Fase* fase, Bola* bola, EventoSom* som,
                     RecursosAudio* audio, float dt);
void atualizarScoobyTransicao(Scooby* scooby, const Fase* fase, const Bola* bola, Ponto alvo, float dt);

void desenharCena(const Fase* fase, const RecursosMapa* recursos,
                  const Scooby* scooby, const Maria* maria, const Bola* bola,
                  const EventoSom* som, bool debug, int vidas, int faseAtual,
                  EstadoJogo estado, float alphaFade, float tempoTutorial);
void desenharTelaFinal(EstadoJogo estado, const RecursosMapa* recursos);
void desenharCarregando(ALLEGRO_DISPLAY* display, ALLEGRO_FONT* fonte);

#endif
