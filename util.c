#include "jogo.h"

const char* NOMES_CORES[QTD_CORES_BOLA] = {
    "amarela", "verde", "roxa", "azul", "vermelha"
};

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
    return x >= r.x && x <= r.x + r.largura &&
           y >= r.y && y <= r.y + r.altura;
}

bool pontoDentroObstaculo(float x, float y, Obstaculo o)
{
    return x >= o.x && x <= o.x + o.largura &&
           y >= o.y && y <= o.y + o.altura;
}

Direcao direcaoSpritePorMovimento(float dx, float dy, Direcao atual)
{
    if (fabsf(dx) < 0.001f && fabsf(dy) < 0.001f) return atual;
    if (fabsf(dx) > fabsf(dy))
        return dx > 0 ? DIRECAO_RIGHT : DIRECAO_LEFT;
    return dy > 0 ? DIRECAO_DOWN : DIRECAO_UP;
}

/*
 * x/y do personagem representam o contato com o chao.
 * A hitbox fisica ocupa somente a regiao dos pes/patas, acima desse ponto.
 */
Retangulo hitboxPersonagem(const Personagem* p, float x, float y)
{
    if (!p) return (Retangulo){ 0, 0, 0, 0 };

    return (Retangulo){
        x - p->largura * 0.5f,
        y - p->altura,
        p->largura,
        p->altura
    };
}

static bool retangulosSobrepostos(Retangulo a, Retangulo b)
{
    return a.x < b.x + b.largura &&
           a.x + a.largura > b.x &&
           a.y < b.y + b.altura &&
           a.y + a.altura > b.y;
}

bool personagemDentroArea(const Personagem* p, float x, float y, const Fase* fase)
{
    if (!p || !fase) return false;

    Retangulo h = hitboxPersonagem(p, x, y);
    const Retangulo* a = &fase->areaJogavel;

    return h.x >= a->x &&
           h.x + h.largura <= a->x + a->largura &&
           h.y >= a->y &&
           h.y + h.altura <= a->y + a->altura;
}

bool personagemColide(const Personagem* p, float novoX, float novoY, const Fase* fase)
{
    if (!p || !fase) return true;
    if (!personagemDentroArea(p, novoX, novoY, fase)) return true;

    Retangulo h = hitboxPersonagem(p, novoX, novoY);

    for (int i = 0; i < fase->quantidadeObstaculos; i++)
    {
        const Obstaculo* o = &fase->obstaculos[i];
        if (!o->bloqueiaMovimento) continue;

        Retangulo r = { o->x, o->y, o->largura, o->altura };
        if (retangulosSobrepostos(h, r)) return true;
    }

    return false;
}

void moverPersonagem(Personagem* p, float dx, float dy, const Fase* fase)
{
    if (!p || !fase) return;

    float novoX = p->x + dx;
    if (!personagemColide(p, novoX, p->y, fase))
        p->x = novoX;

    float novoY = p->y + dy;
    if (!personagemColide(p, p->x, novoY, fase))
        p->y = novoY;
}

bool spawnEhValido(const Personagem* p, float x, float y, const Fase* fase, float margem)
{
    if (!p || !fase) return false;

    Personagem teste = *p;
    teste.largura += margem * 2.0f;
    teste.altura += margem;

    return !personagemColide(&teste, x, y, fase);
}

bool procurarPontoLivreProximo(const Personagem* p, const Fase* fase, Ponto origem, Ponto* resultado)
{
    if (!p || !fase || !resultado) return false;

    if (spawnEhValido(p, origem.x, origem.y, fase, 8.0f))
    {
        *resultado = origem;
        return true;
    }

    const float passo = 24.0f;
    for (int raio = 1; raio <= 12; raio++)
    {
        for (int dy = -raio; dy <= raio; dy++)
        {
            for (int dx = -raio; dx <= raio; dx++)
            {
                if (abs(dx) != raio && abs(dy) != raio) continue;

                Ponto pTeste = {
                    origem.x + dx * passo,
                    origem.y + dy * passo
                };

                if (spawnEhValido(p, pTeste.x, pTeste.y, fase, 8.0f))
                {
                    *resultado = pTeste;
                    return true;
                }
            }
        }
    }

    return false;
}

static bool pontoNavegavelGenerico(const Personagem* p, const Fase* fase, Ponto ponto)
{
    return spawnEhValido(p, ponto.x, ponto.y, fase, 4.0f);
}

void validarConfiguracaoFase(Fase* fase, const Scooby* scooby, const Maria* maria, int indiceFase)
{
    if (!fase || !scooby || !maria) return;

    Ponto corrigido;

    if (!spawnEhValido(&scooby->corpo, fase->spawnScooby.x, fase->spawnScooby.y, fase, 10.0f))
    {
        printf("[FASE %d %s] spawn Scooby invalido: %.1f, %.1f\n",
               indiceFase + 1, fase->nome, fase->spawnScooby.x, fase->spawnScooby.y);

        if (procurarPontoLivreProximo(&scooby->corpo, fase, fase->spawnScooby, &corrigido))
        {
            fase->spawnScooby = corrigido;
            printf("  -> corrigido automaticamente para %.1f, %.1f\n", corrigido.x, corrigido.y);
        }
    }

    if (!spawnEhValido(&maria->corpo, fase->spawnMaria.x, fase->spawnMaria.y, fase, 10.0f))
    {
        printf("[FASE %d %s] spawn Maria invalido: %.1f, %.1f\n",
               indiceFase + 1, fase->nome, fase->spawnMaria.x, fase->spawnMaria.y);

        if (procurarPontoLivreProximo(&maria->corpo, fase, fase->spawnMaria, &corrigido))
            fase->spawnMaria = corrigido;
    }

    for (int i = 0; i < fase->quantidadeWaypoints; i++)
    {
        if (!pontoNavegavelGenerico(&maria->corpo, fase, fase->waypoints[i]))
        {
            Ponto original = fase->waypoints[i];
            if (procurarPontoLivreProximo(&maria->corpo, fase, original, &corrigido))
            {
                fase->waypoints[i] = corrigido;
                printf("[FASE %d %s] waypoint %d ajustado para %.1f, %.1f\n",
                       indiceFase + 1, fase->nome, i, corrigido.x, corrigido.y);
            }
        }
    }

    for (int i = 0; i < fase->quantidadeSpawnsBola; i++)
    {
        Personagem hitBola = { 0 };
        hitBola.largura = 28.0f;
        hitBola.altura = 18.0f;

        if (!spawnEhValido(&hitBola, fase->spawnsBola[i].x, fase->spawnsBola[i].y, fase, 4.0f))
        {
            if (procurarPontoLivreProximo(&hitBola, fase, fase->spawnsBola[i], &corrigido))
                fase->spawnsBola[i] = corrigido;
        }
    }
}

void resetarPersonagensNaFase(Scooby* scooby, Maria* maria, Bola* bola,
                              const Fase* fase, int faseAtual, bool novaCor)
{
    if (!scooby || !maria || !bola || !fase) return;

    Ponto spawnS = fase->spawnScooby;
    if (!spawnEhValido(&scooby->corpo, spawnS.x, spawnS.y, fase, 6.0f))
        procurarPontoLivreProximo(&scooby->corpo, fase, spawnS, &spawnS);

    Ponto spawnM = fase->spawnMaria;
    if (!spawnEhValido(&maria->corpo, spawnM.x, spawnM.y, fase, 6.0f))
        procurarPontoLivreProximo(&maria->corpo, fase, spawnM, &spawnM);

    scooby->corpo.x = spawnS.x;
    scooby->corpo.y = spawnS.y;
    scooby->corpo.direcao = -PI / 2.0f;
    scooby->direcaoSprite = DIRECAO_UP;
    scooby->movendo = false;
    scooby->correndo = false;
    scooby->latindo = false;
    scooby->mordendo = false;
    scooby->carregandoBola = false;
    scooby->coletaPendente = false;
    scooby->cooldownSomCorrida = 0.0f;

    reiniciarAnimacao(&scooby->idle);
    reiniciarAnimacao(&scooby->walk);
    reiniciarAnimacao(&scooby->run);
    reiniciarAnimacao(&scooby->bark);
    reiniciarAnimacao(&scooby->bite);
    for (int i = 0; i < QTD_CORES_BOLA; i++)
        reiniciarAnimacao(&scooby->carregar[i]);

    maria->corpo.x = spawnM.x;
    maria->corpo.y = spawnM.y;
    maria->corpo.direcao = PI;
    maria->direcaoSprite = DIRECAO_LEFT;
    maria->estado = MARIA_PATRULHA;
    maria->waypointAtual = 0;
    maria->tempoBusca = 0.0f;
    maria->tempoNovoAlvoBusca = 0.0f;
    maria->quantidadeCaminho = 0;
    maria->indiceCaminho = 0;
    maria->tempoRecalcularCaminho = 0.0f;
    maria->ultimoAlvoCaminhoX = -9999.0f;
    maria->ultimoAlvoCaminhoY = -9999.0f;
    maria->alvoNavegavelX = maria->corpo.x;
    maria->alvoNavegavelY = maria->corpo.y;
    maria->movendo = false;
    maria->capturaConcluida = false;
    maria->antiStuckTempo = 0.0f;
    maria->antiStuckUltimoX = maria->corpo.x;
    maria->antiStuckUltimoY = maria->corpo.y;

    reiniciarAnimacao(&maria->idle);
    reiniciarAnimacao(&maria->walk);
    reiniciarAnimacao(&maria->run);
    reiniciarAnimacao(&maria->pick);

    if (fase->quantidadeSpawnsBola > 0)
    {
        int spawn = rand() % fase->quantidadeSpawnsBola;
        bola->x = fase->spawnsBola[spawn].x;
        bola->y = fase->spawnsBola[spawn].y;
    }
    else
    {
        bola->x = spawnS.x + 100.0f;
        bola->y = spawnS.y;
    }

    bola->coletada = false;
    if (novaCor)
        bola->cor = (faseAtual + rand()) % QTD_CORES_BOLA;
}

bool chegouNaSaidaComBola(const Scooby* scooby, const Fase* fase)
{
    if (!scooby || !fase || !scooby->carregandoBola) return false;

    Retangulo h = hitboxPersonagem(&scooby->corpo, scooby->corpo.x, scooby->corpo.y);
    float peX = h.x + h.largura * 0.5f;
    float peY = h.y + h.altura;

    return pontoDentroRetangulo(peX, peY, fase->saida);
}
