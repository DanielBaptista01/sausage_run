#include "jogo.h"

const char* NOMES_CORES[QTD_CORES_BOLA] = { "amarela", "verde", "roxa", "azul", "vermelha" };

float distancia(float x1, float y1, float x2, float y2)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    return sqrtf(dx * dx + dy * dy);
}

float normalizarAngulo(float angulo)
{
    while (angulo > PI) angulo -= 2.0f * PI;
    while (angulo < -PI) angulo += 2.0f * PI;
    return angulo;
}

float mapaParaTelaX(float x) { return MAPA_X + x * MAPA_ESCALA; }
float mapaParaTelaY(float y) { return MAPA_Y + y * MAPA_ESCALA; }

bool pontoDentroRetangulo(float x, float y, Retangulo r)
{
    return x >= r.x && x <= r.x + r.largura && y >= r.y && y <= r.y + r.altura;
}

bool pontoDentroObstaculo(float x, float y, Obstaculo o)
{
    return x >= o.x && x <= o.x + o.largura && y >= o.y && y <= o.y + o.altura;
}

Direcao direcaoSpritePorMovimento(float dx, float dy, Direcao atual)
{
    if (fabsf(dx) < 0.001f && fabsf(dy) < 0.001f) return atual;
    if (fabsf(dx) > fabsf(dy)) return dx > 0 ? DIRECAO_RIGHT : DIRECAO_LEFT;
    return dy > 0 ? DIRECAO_DOWN : DIRECAO_UP;
}

void limitarAoMapa(Personagem* p)
{
    float minX = MAPA_X + p->largura / 2.0f + 8.0f;
    float maxX = MAPA_X + MAPA_TELA_W - p->largura / 2.0f - 8.0f;
    float minY = 70.0f + p->altura / 2.0f;
    float maxY = ALTURA_TELA - p->altura / 2.0f - 8.0f;
    if (p->x < minX) p->x = minX;
    if (p->x > maxX) p->x = maxX;
    if (p->y < minY) p->y = minY;
    if (p->y > maxY) p->y = maxY;
}

bool personagemColide(const Personagem* p, float novoX, float novoY, const Fase* fase)
{
    float esquerda = novoX - p->largura / 2.0f;
    float direita = novoX + p->largura / 2.0f;
    float topo = novoY - p->altura / 2.0f;
    float baixo = novoY + p->altura / 2.0f;

    for (int i = 0; i < fase->quantidadeObstaculos; i++)
    {
        const Obstaculo* o = &fase->obstaculos[i];
        if (!o->bloqueiaMovimento) continue;
        if (direita > o->x && esquerda < o->x + o->largura &&
            baixo > o->y && topo < o->y + o->altura) return true;
    }
    return false;
}

void moverPersonagem(Personagem* p, float dx, float dy, const Fase* fase)
{
    float novoX = p->x + dx;
    if (!personagemColide(p, novoX, p->y, fase)) p->x = novoX;
    float novoY = p->y + dy;
    if (!personagemColide(p, p->x, novoY, fase)) p->y = novoY;
    limitarAoMapa(p);
}

void resetarPersonagensNaFase(Scooby* scooby, Maria* maria, Bola* bola,
                              const Fase* fase, int faseAtual, bool novaCor)
{
    scooby->corpo.x = fase->spawnScooby.x;
    scooby->corpo.y = fase->spawnScooby.y;
    scooby->corpo.direcao = -PI / 2.0f;
    scooby->direcaoSprite = DIRECAO_UP;
    scooby->movendo = scooby->correndo = scooby->latindo = scooby->mordendo = false;
    scooby->carregandoBola = scooby->coletaPendente = false;
    scooby->cooldownSomCorrida = 0.0f;

    maria->corpo.x = fase->spawnMaria.x;
    maria->corpo.y = fase->spawnMaria.y;
    maria->corpo.direcao = PI;
    maria->direcaoSprite = DIRECAO_LEFT;
    maria->estado = MARIA_PATRULHA;
    maria->waypointAtual = 0;
    maria->tempoBusca = maria->tempoNovoAlvoBusca = 0.0f;
    maria->quantidadeCaminho = maria->indiceCaminho = 0;
    maria->tempoRecalcularCaminho = 0.0f;
    maria->ultimoAlvoCaminhoX = maria->ultimoAlvoCaminhoY = -9999.0f;
    maria->capturaConcluida = false;
    reiniciarAnimacao(&maria->pick);

    int spawn = rand() % fase->quantidadeSpawnsBola;
    bola->x = fase->spawnsBola[spawn].x;
    bola->y = fase->spawnsBola[spawn].y;
    bola->coletada = false;
    if (novaCor) bola->cor = (faseAtual + rand()) % QTD_CORES_BOLA;
}

bool chegouNaSaidaComBola(const Scooby* scooby, const Fase* fase)
{
    return scooby->carregandoBola && pontoDentroRetangulo(scooby->corpo.x, scooby->corpo.y, fase->saida);
}
