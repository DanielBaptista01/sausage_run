#include "jogo.h"

const char* NOMES_CORES[QTD_CORES_BOLA] = {
    "amarela", "verde", "roxa", "azul", "vermelha"
};

float distancia(float x1, float y1, float x2, float y2)
{
    float dx = x2 - x1, dy = y2 - y1;
    return sqrtf(dx * dx + dy * dy);
}

float normalizarAngulo(float a)
{
    while (a > PI) a -= 2.0f * PI;
    while (a < -PI) a += 2.0f * PI;
    return a;
}

float mapaParaTelaX(float x) { return MAPA_X + x * MAPA_ESCALA; }
float mapaParaTelaY(float y) { return MAPA_Y + y * MAPA_ESCALA; }

bool pontoDentroRetangulo(float x, float y, Retangulo r)
{
    return x >= r.x && x <= r.x + r.largura && y >= r.y && y <= r.y + r.altura;
}

bool retangulosIntersectam(Retangulo a, Retangulo b)
{
    return a.x < b.x + b.largura && a.x + a.largura > b.x &&
           a.y < b.y + b.altura && a.y + a.altura > b.y;
}

Direcao direcaoSpritePorMovimento(float dx, float dy, Direcao atual)
{
    if (fabsf(dx) < .001f && fabsf(dy) < .001f) return atual;
    if (fabsf(dx) > fabsf(dy)) return dx > 0 ? DIRECAO_RIGHT : DIRECAO_LEFT;
    return dy > 0 ? DIRECAO_DOWN : DIRECAO_UP;
}

Retangulo hitboxPersonagem(const Personagem* p, float x, float y)
{
    if (!p) return (Retangulo){0,0,0,0};
    return (Retangulo){
        x + p->hitboxOffsetX - p->hitboxLargura * .5f,
        y + p->hitboxOffsetY - p->hitboxAltura * .5f,
        p->hitboxLargura,
        p->hitboxAltura
    };
}

/*
 * Hitbox fisica oficial do Scooby.
 *
 * O adendo atual determina que o retangulo VERDE do F1 seja a unica
 * geometria utilizada pela fisica. Mantemos HitboxScooby por compatibilidade
 * de API, mas cabeca recebe exatamente o mesmo retangulo de corpo. Assim nao
 * existe uma segunda regiao invisivel interferindo em movimento/captura.
 *
 * O perfil continua adaptavel por direcao para acompanhar o volume principal
 * do tronco. A altura maxima efetiva e 28 px (UP/DOWN), valor usado para
 * calibrar os corredores superiores entre parede e base dos moveis.
 */
static Retangulo retCentro(float x,float y,float ox,float oy,float w,float h)
{
    return (Retangulo){x+ox-w*.5f,y+oy-h*.5f,w,h};
}

HitboxScooby obterHitboxScooby(const Scooby* s, float x, float y)
{
    HitboxScooby h={{0,0,0,0},{0,0,0,0}};
    if(!s)return h;

    switch(s->direcaoSprite)
    {
        case DIRECAO_RIGHT:
            h.corpo=retCentro(x,y,-7.0f,-53.0f,42.0f,18.0f);
            break;
        case DIRECAO_LEFT:
            h.corpo=retCentro(x,y,7.0f,-53.0f,42.0f,18.0f);
            break;
        case DIRECAO_UP:
            h.corpo=retCentro(x,y,0.0f,-51.0f,26.0f,28.0f);
            break;
        case DIRECAO_DOWN:
        default:
            h.corpo=retCentro(x,y,0.0f,-56.0f,26.0f,28.0f);
            break;
    }

    /* Compatibilidade: qualquer rotina antiga que consulte cabeca ve a mesma
       geometria da hitbox verde; a uniao fisica continua sendo um so retangulo. */
    h.cabeca=h.corpo;
    return h;
}

static bool retDentroArea(Retangulo h, Retangulo a)
{
    return h.x >= a.x && h.y >= a.y &&
           h.x+h.largura <= a.x+a.largura &&
           h.y+h.altura <= a.y+a.altura;
}

bool scoobyDentroArea(const Scooby* s,float x,float y,const Fase* f)
{
    if(!s||!f)return false;

    /* O anchor logico tambem deve permanecer no piso para impedir a moldura
       inferior; a transicao continua sendo a unica excecao controlada. */
    if(!pontoDentroRetangulo(x,y,f->areaJogavel))return false;

    HitboxScooby h=obterHitboxScooby(s,x,y);
    return retDentroArea(h.corpo,f->areaJogavel);
}

bool scoobyColide(const Scooby* s,float x,float y,const Fase* f)
{
    if(!s||!f||!scoobyDentroArea(s,x,y,f))return true;
    Retangulo h=obterHitboxScooby(s,x,y).corpo;

    for(int i=0;i<f->quantidadeObstaculos;i++)
    {
        const Obstaculo* o=&f->obstaculos[i];
        if(!o->bloqueiaMovimento)continue;
        Retangulo r={o->x,o->y,o->largura,o->altura};
        if(retangulosIntersectam(h,r))return true;
    }
    return false;
}

void moverScooby(Scooby* s,float dx,float dy,const Fase* f)
{
    if(!s||!f)return;
    float nx=s->corpo.x+dx;
    if(!scoobyColide(s,nx,s->corpo.y,f))s->corpo.x=nx;
    float ny=s->corpo.y+dy;
    if(!scoobyColide(s,s->corpo.x,ny,f))s->corpo.y=ny;
}

bool personagemDentroArea(const Personagem* p, float x, float y, const Fase* f)
{
    if (!p || !f) return false;
    Retangulo h = hitboxPersonagem(p, x, y);
    Retangulo a = f->areaJogavel;
    return h.x >= a.x && h.y >= a.y &&
           h.x + h.largura <= a.x + a.largura &&
           h.y + h.altura <= a.y + a.altura;
}

bool personagemColide(const Personagem* p, float x, float y, const Fase* f)
{
    if (!p || !f || !personagemDentroArea(p, x, y, f)) return true;
    Retangulo h = hitboxPersonagem(p, x, y);
    for (int i = 0; i < f->quantidadeObstaculos; i++)
    {
        const Obstaculo* o = &f->obstaculos[i];
        if (!o->bloqueiaMovimento) continue;
        Retangulo r = { o->x, o->y, o->largura, o->altura };
        if (retangulosIntersectam(h, r)) return true;
    }
    return false;
}

void moverPersonagem(Personagem* p, float dx, float dy, const Fase* f)
{
    if (!p || !f) return;
    float nx = p->x + dx;
    if (!personagemColide(p, nx, p->y, f)) p->x = nx;
    float ny = p->y + dy;
    if (!personagemColide(p, p->x, ny, f)) p->y = ny;
}

bool pontoLivreParaPersonagem(const Personagem* p, const Fase* f, float x, float y)
{
    return p && f && !personagemColide(p, x, y, f);
}

bool procurarPontoLivreProximo(const Personagem* p, const Fase* f, Ponto origem, Ponto* resultado)
{
    if (!p || !f || !resultado) return false;
    if (pontoLivreParaPersonagem(p, f, origem.x, origem.y)) { *resultado = origem; return true; }

    const float passo = 18.0f;
    for (int raio = 1; raio <= 20; raio++)
    {
        for (int k = 0; k < 16; k++)
        {
            float ang = 2.0f * PI * (float)k / 16.0f;
            Ponto q = { origem.x + cosf(ang) * passo * raio,
                        origem.y + sinf(ang) * passo * raio };
            if (pontoLivreParaPersonagem(p, f, q.x, q.y)) { *resultado = q; return true; }
        }
    }
    return false;
}

static bool bolaColideObstaculo(const Fase* f, Ponto p)
{
    Retangulo b = { p.x - RAIO_BOLA, p.y - RAIO_BOLA,
                    RAIO_BOLA * 2.0f, RAIO_BOLA * 2.0f };
    if (b.x < f->areaJogavel.x || b.y < f->areaJogavel.y ||
        b.x + b.largura > f->areaJogavel.x + f->areaJogavel.largura ||
        b.y + b.altura > f->areaJogavel.y + f->areaJogavel.altura)
        return true;

    for (int i = 0; i < f->quantidadeObstaculos; i++)
    {
        const Obstaculo* o = &f->obstaculos[i];
        if (!o->bloqueiaMovimento) continue;
        Retangulo r = {o->x,o->y,o->largura,o->altura};
        if (retangulosIntersectam(b, r)) return true;
    }
    return false;
}

bool spawnBolaValido(const Fase* f, const Personagem* scooby,
                     const Personagem* maria, Ponto p)
{
    if (!f || !scooby || !maria) return false;
    if (bolaColideObstaculo(f, p)) return false;
    if (distancia(p.x,p.y,scooby->x,scooby->y) < 105.0f) return false;
    if (distancia(p.x,p.y,maria->x,maria->y) < 90.0f) return false;
    return true;
}

static int celulaCol(float x)
{
    int c = (int)((x - MAPA_X) / TAM_CELULA);
    if (c < 0) c = 0;
    if (c >= GRID_COLS) c = GRID_COLS - 1;
    return c;
}

static int celulaRow(float y)
{
    int r = (int)((y - MAPA_Y) / TAM_CELULA);
    if (r < 0) r = 0;
    if (r >= GRID_ROWS) r = GRID_ROWS - 1;
    return r;
}

static Ponto centroCelula(int c, int r)
{
    return (Ponto){ MAPA_X + c*TAM_CELULA + TAM_CELULA*.5f,
                    MAPA_Y + r*TAM_CELULA + TAM_CELULA*.5f };
}

bool pontoAlcancavel(const Fase* f, const Personagem* p, Ponto origem, Ponto destino)
{
    if (!f || !p || !pontoLivreParaPersonagem(p,f,origem.x,origem.y) ||
        !pontoLivreParaPersonagem(p,f,destino.x,destino.y)) return false;

    int sc=celulaCol(origem.x), sr=celulaRow(origem.y);
    int tc=celulaCol(destino.x), tr=celulaRow(destino.y);
    bool vis[GRID_ROWS][GRID_COLS] = {{false}};
    short qc[GRID_TOTAL], qr[GRID_TOTAL];
    int ini=0,fim=0;
    qc[fim]=sc; qr[fim++]=sr; vis[sr][sc]=true;
    const int dc[4]={1,-1,0,0}, dr[4]={0,0,1,-1};

    while(ini<fim)
    {
        int c=qc[ini], r=qr[ini++];
        if(c==tc && r==tr) return true;
        for(int k=0;k<4;k++)
        {
            int nc=c+dc[k], nr=r+dr[k];
            if(nc<0||nc>=GRID_COLS||nr<0||nr>=GRID_ROWS||vis[nr][nc]) continue;
            Ponto q=centroCelula(nc,nr);
            if(!pontoLivreParaPersonagem(p,f,q.x,q.y)) continue;
            vis[nr][nc]=true;
            if(fim<GRID_TOTAL){qc[fim]=nc;qr[fim++]=nr;}
        }
    }
    return false;
}

void validarConfiguracaoFase(Fase* f, const Scooby* s, const Maria* m, int indice)
{
    if (!f || !s || !m) return;
    Ponto ajustado;

    if (scoobyColide(s,f->spawnScooby.x,f->spawnScooby.y,f))
    {
        printf("WARN F%d %s: spawn Scooby invalido para hitbox verde\n",indice+1,f->nome);
        if(procurarPontoLivreProximo(&s->corpo,f,f->spawnScooby,&ajustado)) f->spawnScooby=ajustado;
    }
    if (!pontoLivreParaPersonagem(&m->corpo,f,f->spawnMaria.x,f->spawnMaria.y))
    {
        printf("WARN F%d %s: spawn Maria invalido\n",indice+1,f->nome);
        if(procurarPontoLivreProximo(&m->corpo,f,f->spawnMaria,&ajustado)) f->spawnMaria=ajustado;
    }

    for(int i=0;i<f->quantidadeWaypoints;i++)
    {
        Ponto p=f->waypoints[i];
        if(!pontoLivreParaPersonagem(&m->corpo,f,p.x,p.y))
        {
            printf("WARN %s waypoint %d invalido; ajustando\n",f->nome,i);
            if(procurarPontoLivreProximo(&m->corpo,f,p,&ajustado)) f->waypoints[i]=ajustado;
        }
    }

    if (!pontoDentroRetangulo(f->alvoEntradaSaida.x,f->alvoEntradaSaida.y,f->areaJogavel))
        printf("WARN %s alvo de transicao fora da area caminhavel\n",f->nome);
}

static bool escolherSpawnBola(const Fase* f, const Scooby* s, const Maria* m, Ponto* out)
{
    Ponto validos[MAX_SPAWNS_BOLA]; int qtd=0;
    for(int i=0;i<f->quantidadeSpawnsBola;i++)
    {
        Ponto p=f->spawnsBola[i];
        if(!spawnBolaValido(f,&s->corpo,&m->corpo,p)) continue;
        if(!pontoAlcancavel(f,&s->corpo,(Ponto){s->corpo.x,s->corpo.y},p)) continue;
        validos[qtd++]=p;
    }
    if(qtd>0){*out=validos[rand()%qtd];return true;}

    for(int r=2;r<GRID_ROWS-1;r++)
    for(int c=2;c<GRID_COLS-1;c++)
    {
        Ponto p=centroCelula(c,r);
        if(!spawnBolaValido(f,&s->corpo,&m->corpo,p)) continue;
        if(!pontoAlcancavel(f,&s->corpo,(Ponto){s->corpo.x,s->corpo.y},p)) continue;
        *out=p;
        printf("WARN %s: nenhum spawn preconfigurado valido; fallback grid usado.\n",f->nome);
        return true;
    }
    printf("ERRO %s: NAO EXISTE SPAWN DE BOLA VALIDO E ALCANCAVEL.\n",f->nome);
    return false;
}

void resetarPersonagensNaFase(Scooby* s, Maria* m, Bola* b,
                              const Fase* f, int faseAtual, bool novaCor)
{
    if (!s || !m || !b || !f) return;

    s->corpo.x=f->spawnScooby.x; s->corpo.y=f->spawnScooby.y; s->corpo.direcao=-PI*.5f;
    s->direcaoSprite=DIRECAO_UP; s->movendo=false; s->correndo=false;
    s->latindo=false; s->mordendo=false; s->carregandoBola=false;
    s->coletaPendente=false; s->cooldownSomCorrida=0;
    reiniciarAnimacao(&s->idle);reiniciarAnimacao(&s->walk);reiniciarAnimacao(&s->run);
    reiniciarAnimacao(&s->bark);reiniciarAnimacao(&s->bite);
    for(int i=0;i<QTD_CORES_BOLA;i++)reiniciarAnimacao(&s->carregar[i]);

    m->corpo.x=f->spawnMaria.x; m->corpo.y=f->spawnMaria.y; m->corpo.direcao=PI;
    m->direcaoSprite=DIRECAO_LEFT; m->estado=MARIA_PATRULHA; m->waypointAtual=0;
    m->tempoBusca=0;m->tempoNovoAlvoBusca=0;m->quantidadeCaminho=0;m->indiceCaminho=0;
    m->tempoRecalcularCaminho=0;m->ultimoAlvoCaminhoX=-9999;m->ultimoAlvoCaminhoY=-9999;
    m->alvoNavegavelX=m->corpo.x;m->alvoNavegavelY=m->corpo.y;
    m->movendo=false;m->capturaConcluida=false;m->tempoSemProgresso=0;
    m->ultimaPosicaoProgressoX=m->corpo.x;m->ultimaPosicaoProgressoY=m->corpo.y;
    reiniciarAnimacao(&m->idle);reiniciarAnimacao(&m->walk);reiniciarAnimacao(&m->run);reiniciarAnimacao(&m->pick);

    b->coletada=false;
    if(novaCor) b->cor=(faseAtual+rand())%QTD_CORES_BOLA;
    Ponto spawn;
    if(escolherSpawnBola(f,s,m,&spawn)){b->x=spawn.x;b->y=spawn.y;}
    else { b->x=s->corpo.x+120;b->y=s->corpo.y-60; }
}

bool chegouNaSaidaComBola(const Scooby* s, const Fase* f)
{
    if(!s||!f||!s->carregandoBola)return false;
    return pontoDentroRetangulo(s->corpo.x,s->corpo.y,f->triggerSaida);
}