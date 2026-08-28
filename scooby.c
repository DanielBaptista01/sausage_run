#include "jogo.h"

#define SCOOBY_COLISAO_EPSILON 0.25f
#define SCOOBY_CORRECAO_LOCAL_MAX 18

bool bolaNaFrenteDoScooby(const Scooby* s,const Bola* b)
{
    if(!s||!b||b->coletada)return false;
    float d=distancia(s->corpo.x,s->corpo.y,b->x,b->y);if(d>82)return false;
    float a=atan2f(b->y-s->corpo.y,b->x-s->corpo.x);
    return fabsf(normalizarAngulo(a-s->corpo.direcao))<=85.0f*PI/180.0f;
}

void iniciarLatido(Scooby* s,EventoSom* som,RecursosAudio* audio)
{
    if(!s||s->latindo||s->mordendo||s->carregandoBola)return;
    s->latindo=true;s->movendo=false;s->correndo=false;reiniciarAnimacao(&s->bark);
    emitirSom(som,SOM_LATIDO,s->corpo.x,s->corpo.y,420);
    if(audio)tocarEfeitoPosicional(audio->scoobyLatido,s->corpo.x,.95f);
}

void iniciarMordida(Scooby* s,const Bola* b,RecursosAudio* audio)
{
    if(!s||!b||s->latindo||s->mordendo||s->carregandoBola)return;
    s->mordendo=true;s->movendo=false;s->correndo=false;s->coletaPendente=bolaNaFrenteDoScooby(s,b);reiniciarAnimacao(&s->bite);
    if(audio)tocarEfeitoPosicional(audio->scoobyMordida,s->corpo.x,.72f);
}

static void passo(Scooby* s,RecursosAudio* audio,int anterior,bool corrida,Animacao* a)
{
    if(!s||!audio||!a||a->frameAtual==anterior||(a->frameAtual!=1&&a->frameAtual!=3))return;
    tocarEfeitoPosicional(corrida?audio->scoobyCorrida:audio->scoobyPasso,s->corpo.x,corrida?.40f:.28f);
}

static bool overlapPositivo(Retangulo a,Retangulo b,float* overlapX,float* overlapY)
{
    float esquerda=fmaxf(a.x,b.x);
    float direita=fminf(a.x+a.largura,b.x+b.largura);
    float topo=fmaxf(a.y,b.y);
    float fundo=fminf(a.y+a.altura,b.y+b.altura);
    float ox=direita-esquerda;
    float oy=fundo-topo;
    if(overlapX)*overlapX=ox;
    if(overlapY)*overlapY=oy;
    return ox>SCOOBY_COLISAO_EPSILON&&oy>SCOOBY_COLISAO_EPSILON;
}

/*
 * Salvaguarda contra estados legados/interpenetracao criada por troca de
 * direcao. O movimento normal continua preventivo; esta rotina so atua quando
 * o frame JA comeca invalido. Primeiro tenta os MTVs exatos dos obstaculos
 * sobrepostos e, se dois obstaculos formarem uma quina, procura a menor
 * correcao local livre. Nunca deixa uma sobreposicao persistir no frame
 * seguinte quando existe uma separacao local valida.
 */
static bool corrigirPenetracaoScooby(Scooby* s,const Fase* f)
{
    if(!s||!f)return false;
    if(!scoobyColide(s,s->corpo.x,s->corpo.y,f))return true;

    HitboxScooby hs=obterHitboxScooby(s,s->corpo.x,s->corpo.y);
    Retangulo h=hs.corpo;
    float melhorDx=0,melhorDy=0,melhorDist=1e30f;
    bool achou=false;

    for(int i=0;i<f->quantidadeObstaculos;i++)
    {
        const Obstaculo* o=&f->obstaculos[i];
        if(!o->bloqueiaMovimento)continue;
        Retangulo r={o->x,o->y,o->largura,o->altura};
        float ox,oy;
        if(!overlapPositivo(h,r,&ox,&oy))continue;

        float candidatos[4][2]={
            {r.x-(h.x+h.largura)-SCOOBY_COLISAO_EPSILON,0},
            {r.x+r.largura-h.x+SCOOBY_COLISAO_EPSILON,0},
            {0,r.y-(h.y+h.altura)-SCOOBY_COLISAO_EPSILON},
            {0,r.y+r.altura-h.y+SCOOBY_COLISAO_EPSILON}
        };

        for(int k=0;k<4;k++)
        {
            float dx=candidatos[k][0],dy=candidatos[k][1];
            float nx=s->corpo.x+dx,ny=s->corpo.y+dy;
            if(scoobyColide(s,nx,ny,f))continue;
            float d2=dx*dx+dy*dy;
            if(d2<melhorDist){melhorDist=d2;melhorDx=dx;melhorDy=dy;achou=true;}
        }
    }

    if(achou)
    {
        s->corpo.x+=melhorDx;
        s->corpo.y+=melhorDy;
        return !scoobyColide(s,s->corpo.x,s->corpo.y,f);
    }

    /* Quinas/multiplos obstaculos: busca pequena, deterministica e local. */
    for(int raio=1;raio<=SCOOBY_CORRECAO_LOCAL_MAX;raio++)
    {
        static const int dirs[8][2]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
        for(int k=0;k<8;k++)
        {
            float nx=s->corpo.x+(float)(dirs[k][0]*raio);
            float ny=s->corpo.y+(float)(dirs[k][1]*raio);
            if(!scoobyColide(s,nx,ny,f))
            {
                s->corpo.x=nx;
                s->corpo.y=ny;
                return true;
            }
        }
    }

    return false;
}

/*
 * Mudar a direcao altera fisicamente a hitbox. A nova orientacao so e aceita
 * se couber na posicao atual ou se um pequeno MTV puder separa-la de um
 * obstaculo. Caso contrario, preservamos a direcao anterior para que Scooby
 * ainda consiga deslizar/afastar-se em vez de entrar no movel e softlockar.
 */
static bool aplicarDirecaoSegura(Scooby* s,Direcao nova,const Fase* f)
{
    if(!s||!f)return false;
    if(nova==s->direcaoSprite)return true;

    Direcao anterior=s->direcaoSprite;
    float xAnterior=s->corpo.x,yAnterior=s->corpo.y;
    s->direcaoSprite=nova;

    if(!scoobyColide(s,s->corpo.x,s->corpo.y,f))return true;
    if(corrigirPenetracaoScooby(s,f))return true;

    s->corpo.x=xAnterior;
    s->corpo.y=yAnterior;
    s->direcaoSprite=anterior;
    corrigirPenetracaoScooby(s,f);
    return false;
}

void atualizarScooby(Scooby* s,const ALLEGRO_KEYBOARD_STATE* teclado,const Fase* f,
                     Bola* b,EventoSom* som,RecursosAudio* audio,float dt)
{
    if(!s||!teclado||!f||!b)return;

    /* Invariante de entrada do frame: nunca iniciar o update penetrando. */
    corrigirPenetracaoScooby(s,f);

    s->movendo=false;s->correndo=false;

    if(s->latindo){if(atualizarAnimacaoUmaVez(&s->bark,dt))s->latindo=false;return;}
    if(s->mordendo)
    {
        if(atualizarAnimacaoUmaVez(&s->bite,dt))
        {
            s->mordendo=false;
            if(s->coletaPendente&&bolaNaFrenteDoScooby(s,b))
            {
                b->coletada=true;s->carregandoBola=true;emitirSom(som,SOM_INTERACAO,s->corpo.x,s->corpo.y,135);
                reiniciarAnimacao(&s->carregar[b->cor]);if(audio)tocarEfeitoPosicional(audio->coletaBola,s->corpo.x,.82f);
            }
            s->coletaPendente=false;
        }
        return;
    }

    float dx=0,dy=0;if(al_key_down(teclado,ALLEGRO_KEY_W))dy-=1;if(al_key_down(teclado,ALLEGRO_KEY_S))dy+=1;
    if(al_key_down(teclado,ALLEGRO_KEY_A))dx-=1;if(al_key_down(teclado,ALLEGRO_KEY_D))dx+=1;
    s->movendo=fabsf(dx)>.01f||fabsf(dy)>.01f;
    s->correndo=s->movendo&&al_key_down(teclado,ALLEGRO_KEY_LSHIFT);

    if(s->movendo)
    {
        float n=sqrtf(dx*dx+dy*dy);dx/=n;dy/=n;
        Direcao desejada=direcaoSpritePorMovimento(dx,dy,s->direcaoSprite);
        bool direcaoAplicada=aplicarDirecaoSegura(s,desejada,f);
        if(direcaoAplicada)s->corpo.direcao=atan2f(dy,dx);

        float v=s->correndo?235.0f:135.0f;

        /* moverScooby testa X e Y separadamente ANTES de gravar a posicao. */
        moverScooby(s,dx*v*dt,dy*v*dt,f);

        /* Segunda salvaguarda: nenhum update termina em overlap persistente. */
        corrigirPenetracaoScooby(s,f);
    }

    if(s->cooldownSomCorrida>0)s->cooldownSomCorrida-=dt;
    if(s->correndo&&s->cooldownSomCorrida<=0){emitirSom(som,SOM_CORRIDA,s->corpo.x,s->corpo.y,220);s->cooldownSomCorrida=.30f;}

    Animacao* a=&s->idle;if(s->carregandoBola)a=&s->carregar[b->cor];else if(s->correndo)a=&s->run;else if(s->movendo)a=&s->walk;
    int fr=a->frameAtual;atualizarAnimacaoLoop(a,dt);if(s->movendo)passo(s,audio,fr,s->correndo,a);
}

static float clampfLocal(float v,float lo,float hi)
{
    if(v<lo)return lo;
    if(v>hi)return hi;
    return v;
}

void atualizarScoobyTransicao(Scooby* s,const Fase* f,const Bola* b,Ponto alvo,float dt)
{
    if(!s||!f||!b)return;

    s->latindo=false;s->mordendo=false;s->coletaPendente=false;s->correndo=false;

    /*
     * Esta funcao so roda em JOGO_TRANSICAO_FASE. O limite horizontal usa a
     * extensao REAL da hitbox composta na direcao corrente, para que corpo e
     * cabeca atravessem apenas a abertura do checkpoint.
     */
    HitboxScooby h0=obterHitboxScooby(s,0.0f,0.0f);
    float minRel=fminf(h0.corpo.x,h0.cabeca.x);
    float maxRel=fmaxf(h0.corpo.x+h0.corpo.largura,
                       h0.cabeca.x+h0.cabeca.largura);
    float extEsq=-minRel;
    float extDir=maxRel;

    float minX=f->triggerSaida.x+extEsq;
    float maxX=f->triggerSaida.x+f->triggerSaida.largura-extDir;
    if(maxX<minX){float meio=f->triggerSaida.x+f->triggerSaida.largura*.5f;minX=maxX=meio;}
    alvo.x=clampfLocal(alvo.x,minX,maxX);

    float dx=alvo.x-s->corpo.x;
    float dy=alvo.y-s->corpo.y;
    float d=sqrtf(dx*dx+dy*dy);

    if(d>2.5f)
    {
        dx/=d;dy/=d;
        s->movendo=true;
        s->corpo.direcao=atan2f(dy,dx);
        s->direcaoSprite=direcaoSpritePorMovimento(dx,dy,s->direcaoSprite);

        float passoMax=55.0f*dt;
        if(passoMax>d)passoMax=d;

        /* Excecao controlada: a transicao pode atravessar a borda inferior. */
        s->corpo.x+=dx*passoMax;
        s->corpo.y+=dy*passoMax;
        s->corpo.x=clampfLocal(s->corpo.x,minX,maxX);
    }
    else
        s->movendo=false;

    atualizarAnimacaoLoop(s->carregandoBola?&s->carregar[b->cor]:&s->walk,dt);
}
