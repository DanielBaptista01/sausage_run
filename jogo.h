#ifndef JOGO_H
#define JOGO_H

#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_audio.h>

#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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

#define FRAME_SPRITE 313
#define QTD_FRAMES 4

#define MAX_OBJETOS 28
#define MAX_OBSTACULOS 36
#define MAX_WAYPOINTS 8
#define MAX_CAMINHO 512

#define TAM_CELULA 40
#define GRID_COLS 24
#define GRID_ROWS 18
#define GRID_TOTAL (GRID_COLS * GRID_ROWS)

#define QTD_FASES 4
#define QTD_CORES_BOLA 5

typedef enum { DIRECAO_DOWN = 0, DIRECAO_UP, DIRECAO_LEFT, DIRECAO_RIGHT } Direcao;
typedef enum { MARIA_PATRULHA, MARIA_INVESTIGAR, MARIA_PERSEGUIR, MARIA_PROCURAR, MARIA_CAPTURAR } EstadoMaria;
typedef enum { SOM_NENHUM, SOM_CORRIDA, SOM_LATIDO, SOM_INTERACAO } TipoSom;
typedef enum { JOGO_RODANDO, JOGO_VITORIA, JOGO_GAME_OVER } EstadoJogo;

typedef struct { float x, y; } Ponto;
typedef struct { float x, y, largura, altura; } Retangulo;

typedef struct {
    ALLEGRO_BITMAP* imagem;
    int frameAtual;
    float acumulador;
    float tempoFrame;
} Animacao;

typedef struct {
    float x, y;
    float largura, altura;
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
    float mapaX, mapaY, escala;
    float colX, colY, colW, colH;
    bool colide, bloqueiaVisao;
} ObjetoMapa;

typedef struct {
    ALLEGRO_BITMAP* fundo;
    ALLEGRO_BITMAP* folhaObjetos;
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
    Retangulo saida;
} Fase;

typedef struct {
    ALLEGRO_BITMAP* fundos[QTD_FASES];
    ALLEGRO_BITMAP* folhasObjetos[QTD_FASES];
    ALLEGRO_BITMAP* bolas;
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
    float cooldownPassoAudio;
    Animacao idle, walk, run, bark, bite;
    Animacao carregar[QTD_CORES_BOLA];
} Scooby;

typedef struct {
    Personagem corpo;
    Direcao direcaoSprite;
    EstadoMaria estado;
    EstadoMaria estadoAudioAnterior;
    float alcanceVisao, anguloVisao, alcanceAudicao;
    float alvoX, alvoY;
    float ultimaPosicaoVistaX, ultimaPosicaoVistaY;
    int waypointAtual;
    float tempoBusca, tempoNovoAlvoBusca;
    Ponto caminho[MAX_CAMINHO];
    int quantidadeCaminho, indiceCaminho;
    float tempoRecalcularCaminho;
    float ultimoAlvoCaminhoX, ultimoAlvoCaminhoY;
    bool movendo, capturaConcluida;
    float cooldownPassoAudio;
    Animacao idle, walk, run, pick;
} Maria;

typedef struct {
    float x, y;
    int cor;
    bool coletada;
} Bola;

extern const char* NOMES_CORES[QTD_CORES_BOLA];

float distancia(float x1, float y1, float x2, float y2);
float normalizarAngulo(float angulo);
float mapaParaTelaX(float x);
float mapaParaTelaY(float y);
bool pontoDentroRetangulo(float x, float y, Retangulo r);
bool pontoDentroObstaculo(float x, float y, Obstaculo o);
Direcao direcaoSpritePorMovimento(float dx, float dy, Direcao atual);
void limitarAoMapa(Personagem* p);
bool personagemColide(const Personagem* p, float novoX, float novoY, const Fase* fase);
void moverPersonagem(Personagem* p, float dx, float dy, const Fase* fase);
void resetarPersonagensNaFase(Scooby* scooby, Maria* maria, Bola* bola,
                              const Fase* fase, int faseAtual, bool novaCor);
bool chegouNaSaidaComBola(const Scooby* scooby, const Fase* fase);

ALLEGRO_BITMAP* carregarBitmapFlexivel(const char* caminho);
bool carregarAnimacao(Animacao* animacao, const char* caminho, float tempoFrame);
void reiniciarAnimacao(Animacao* animacao);
void atualizarAnimacaoLoop(Animacao* animacao, float dt);
bool atualizarAnimacaoUmaVez(Animacao* animacao, float dt);
void desenharAnimacao(const Animacao* animacao, Direcao direcao, float x, float y, float escala);
bool carregarRecursosMapa(RecursosMapa* r);
bool carregarSprites(Scooby* scooby, Maria* maria);
void destruirRecursos(RecursosMapa* r, Scooby* scooby, Maria* maria);

bool criarRecursosAudio(RecursosAudio* audio);
void tocarEfeito(ALLEGRO_SAMPLE* sample, float ganho);
void tocarEfeitoPosicional(ALLEGRO_SAMPLE* sample, float x, float ganho);
void destruirRecursosAudio(RecursosAudio* audio);

void configurarFases(Fase fases[QTD_FASES], RecursosMapa* recursos);
void desenharObjeto(const Fase* fase, const ObjetoMapa* obj);
float baseYObjeto(const ObjetoMapa* obj);

void emitirSom(EventoSom* som, TipoSom tipo, float x, float y, float alcance);
void atualizarSom(EventoSom* som, float dt);
bool mariaOuveSom(const Maria* maria, const EventoSom* som);
bool mariaVeScooby(const Maria* maria, const Scooby* scooby, const Fase* fase);
void atualizarMaria(Maria* maria, const Scooby* scooby, EventoSom* som,
                    const Fase* fase, RecursosAudio* audio, float dt);

bool bolaNaFrenteDoScooby(const Scooby* scooby, const Bola* bola);
void iniciarLatido(Scooby* scooby, EventoSom* som, RecursosAudio* audio);
void iniciarMordida(Scooby* scooby, const Bola* bola, RecursosAudio* audio);
void atualizarScooby(Scooby* scooby, const ALLEGRO_KEYBOARD_STATE* teclado,
                     const Fase* fase, Bola* bola, EventoSom* som,
                     RecursosAudio* audio, float dt);

void desenharCena(const Fase* fase, const RecursosMapa* recursos,
                  const Scooby* scooby, const Maria* maria, const Bola* bola,
                  const EventoSom* som, bool debug, int vidas, int faseAtual);
void desenharTelaFinal(EstadoJogo estado);

#endif
