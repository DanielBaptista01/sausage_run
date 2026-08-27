#include "jogo.h"

void emitirSom(EventoSom* som,TipoSom tipo,float x,float y,float alcance){som->tipo=tipo;som->x=x;som->y=y;som->alcance=alcance;som->tempoRestante=.35f;som->ativo=true;som->processado=false;}
void atualizarSom(EventoSom* som,float dt){if(!som->ativo)return;som->tempoRestante-=dt;if(som->tempoRestante<=0){som->ativo=false;som->tipo=SOM_NENHUM;som->processado=false;}}
bool mariaOuveSom(const Maria* m,const EventoSom* s){if(!s->ativo)return false;float a=s->alcance;if(a>m->alcanceAudicao)a=m->alcanceAudicao;return distancia(m->corpo.x,m->corpo.y,s->x,s->y)<=a;}

static bool linhaBloqueada(float ox,float oy,float dx,float dy,const Fase* f)
{
    for(int p=1;p<55;p++){float t=(float)p/55.0f,x=ox+(dx-ox)*t,y=oy+(dy-oy)*t;for(int i=0;i<f->quantidadeObstaculos;i++){const Obstaculo* o=&f->obstaculos[i];if(o->bloqueiaVisao&&pontoDentroObstaculo(x,y,*o))return true;}}
    return false;
}

bool mariaVeScooby(const Maria* m,const Scooby* s,const Fase* f)
{
    if(distancia(m->corpo.x,m->corpo.y,s->corpo.x,s->corpo.y)>m->alcanceVisao)return false;
    float a=atan2f(s->corpo.y-m->corpo.y,s->corpo.x-m->corpo.x);
    if(fabsf(normalizarAngulo(a-m->corpo.direcao))>m->anguloVisao/2.0f)return false;
    return !linhaBloqueada(m->corpo.x,m->corpo.y,s->corpo.x,s->corpo.y,f);
}

static int gridCol(float x){int c=(int)((x-MAPA_X)/TAM_CELULA);if(c<0)c=0;if(c>=GRID_COLS)c=GRID_COLS-1;return c;}
static int gridRow(float y){int r=(int)(y/TAM_CELULA);if(r<0)r=0;if(r>=GRID_ROWS)r=GRID_ROWS-1;return r;}
static Ponto centro(int c,int r){return(Ponto){MAPA_X+c*TAM_CELULA+TAM_CELULA/2.0f,r*TAM_CELULA+TAM_CELULA/2.0f};}

static bool bloqueada(const Fase* f,int c,int r)
{
    if(c<0||c>=GRID_COLS||r<0||r>=GRID_ROWS)return true;Ponto p=centro(c,r);if(p.y<75||p.y>ALTURA_TELA-12)return true;
    for(int i=0;i<f->quantidadeObstaculos;i++){const Obstaculo* o=&f->obstaculos[i];float mg=15;if(p.x>=o->x-mg&&p.x<=o->x+o->largura+mg&&p.y>=o->y-mg&&p.y<=o->y+o->altura+mg)return true;}return false;
}

static void destinoLivre(const Fase* f,int* tc,int* tr)
{
    if(!bloqueada(f,*tc,*tr))return;int oc=*tc,orow=*tr;for(int raio=1;raio<=4;raio++)for(int dr=-raio;dr<=raio;dr++)for(int dc=-raio;dc<=raio;dc++){int c=oc+dc,r=orow+dr;if(!bloqueada(f,c,r)){*tc=c;*tr=r;return;}}
}

static bool calcularCaminho(Maria* m,const Fase* f,float destinoX,float destinoY)
{
    int sc=gridCol(m->corpo.x),sr=gridRow(m->corpo.y),tc=gridCol(destinoX),tr=gridRow(destinoY);destinoLivre(f,&tc,&tr);
    bool v[GRID_ROWS][GRID_COLS]={false};short ac[GRID_ROWS][GRID_COLS],ar[GRID_ROWS][GRID_COLS];int qc[GRID_TOTAL],qr[GRID_TOTAL];
    for(int r=0;r<GRID_ROWS;r++)for(int c=0;c<GRID_COLS;c++){ac[r][c]=-1;ar[r][c]=-1;}
    int ini=0,fim=1;qc[0]=sc;qr[0]=sr;v[sr][sc]=true;const int dc[4]={1,-1,0,0},dr[4]={0,0,1,-1};bool achou=false;
    while(ini<fim){int c=qc[ini],r=qr[ini++];if(c==tc&&r==tr){achou=true;break;}for(int i=0;i<4;i++){int nc=c+dc[i],nr=r+dr[i];if(nc<0||nc>=GRID_COLS||nr<0||nr>=GRID_ROWS||v[nr][nc])continue;if(bloqueada(f,nc,nr)&&!(nc==tc&&nr==tr))continue;v[nr][nc]=true;ac[nr][nc]=(short)c;ar[nr][nc]=(short)r;qc[fim]=nc;qr[fim++]=nr;}}
    m->quantidadeCaminho=0;m->indiceCaminho=0;if(!achou)return false;Ponto inv[MAX_CAMINHO];int qtd=0,c=tc,r=tr;
    while(!(c==sc&&r==sr)&&qtd<MAX_CAMINHO){inv[qtd++]=centro(c,r);int pc=ac[r][c],pr=ar[r][c];if(pc<0||pr<0)break;c=pc;r=pr;}
    for(int i=qtd-1;i>=0;i--)m->caminho[m->quantidadeCaminho++]=inv[i];m->ultimoAlvoCaminhoX=destinoX;m->ultimoAlvoCaminhoY=destinoY;m->tempoRecalcularCaminho=.28f;return m->quantidadeCaminho>0;
}

static void moverMaria(Maria* m,const Fase* f,float x,float y,float dt,float mult)
{
    m->movendo=false;m->tempoRecalcularCaminho-=dt;bool mudou=distancia(x,y,m->ultimoAlvoCaminhoX,m->ultimoAlvoCaminhoY)>45;
    if(m->tempoRecalcularCaminho<=0||m->indiceCaminho>=m->quantidadeCaminho||mudou)calcularCaminho(m,f,x,y);
    if(m->indiceCaminho>=m->quantidadeCaminho)return;Ponto a=m->caminho[m->indiceCaminho];float dx=a.x-m->corpo.x,dy=a.y-m->corpo.y,d=sqrtf(dx*dx+dy*dy);if(d<8){m->indiceCaminho++;return;}dx/=d;dy/=d;m->corpo.direcao=atan2f(dy,dx);m->direcaoSprite=direcaoSpritePorMovimento(dx,dy,m->direcaoSprite);m->movendo=true;moverPersonagem(&m->corpo,dx*m->corpo.velocidade*mult*dt,dy*m->corpo.velocidade*mult*dt,f);
}

static void alvoBusca(Maria* m,const Fase* f)
{
    for(int t=0;t<12;t++){float a=(float)(rand()%628)/100.0f,r=55+(float)(rand()%120),x=m->ultimaPosicaoVistaX+cosf(a)*r,y=m->ultimaPosicaoVistaY+sinf(a)*r;x=fmaxf(MAPA_X+30,fminf(MAPA_X+MAPA_TELA_W-30,x));y=fmaxf(90,fminf(ALTURA_TELA-30,y));if(!bloqueada(f,gridCol(x),gridRow(y))){m->alvoX=x;m->alvoY=y;m->tempoRecalcularCaminho=0;return;}}
    m->alvoX=m->ultimaPosicaoVistaX;m->alvoY=m->ultimaPosicaoVistaY;
}

void atualizarMaria(Maria* m,const Scooby* s,EventoSom* som,const Fase* f,RecursosAudio* audio,float dt)
{
    EstadoMaria estadoAntes=m->estado;
    m->movendo=false;m->capturaConcluida=false;

    if(m->estado==MARIA_CAPTURAR)
    {
        if(atualizarAnimacaoUmaVez(&m->pick,dt))m->capturaConcluida=true;
        return;
    }

    bool viu=mariaVeScooby(m,s,f);
    if(viu)
    {
        m->estado=MARIA_PERSEGUIR;
        m->alvoX=s->corpo.x;m->alvoY=s->corpo.y;
        m->ultimaPosicaoVistaX=s->corpo.x;m->ultimaPosicaoVistaY=s->corpo.y;
    }
    else
    {
        if(m->estado==MARIA_PERSEGUIR)
        {
            m->estado=MARIA_PROCURAR;m->alvoX=m->ultimaPosicaoVistaX;m->alvoY=m->ultimaPosicaoVistaY;
            m->tempoBusca=4;m->tempoNovoAlvoBusca=.8f;m->tempoRecalcularCaminho=0;
        }
        if(som->ativo&&!som->processado&&mariaOuveSom(m,som))
        {
            m->estado=MARIA_INVESTIGAR;m->alvoX=som->x;m->alvoY=som->y;
            m->ultimaPosicaoVistaX=som->x;m->ultimaPosicaoVistaY=som->y;
            m->tempoRecalcularCaminho=0;som->processado=true;
        }
    }

    if(audio && m->estado!=estadoAntes && (m->estado==MARIA_INVESTIGAR||m->estado==MARIA_PERSEGUIR))
        tocarEfeitoPosicional(audio->mariaAlerta,m->corpo.x,0.52f);

    switch(m->estado)
    {
        case MARIA_PATRULHA:{Ponto a=f->waypoints[m->waypointAtual];moverMaria(m,f,a.x,a.y,dt,1.0f);if(distancia(m->corpo.x,m->corpo.y,a.x,a.y)<32){m->waypointAtual=(m->waypointAtual+1)%f->quantidadeWaypoints;m->tempoRecalcularCaminho=0;}break;}
        case MARIA_INVESTIGAR:moverMaria(m,f,m->alvoX,m->alvoY,dt,1.08f);if(distancia(m->corpo.x,m->corpo.y,m->alvoX,m->alvoY)<34){m->estado=MARIA_PROCURAR;m->ultimaPosicaoVistaX=m->alvoX;m->ultimaPosicaoVistaY=m->alvoY;m->tempoBusca=3.2f;m->tempoNovoAlvoBusca=0;}break;
        case MARIA_PERSEGUIR:moverMaria(m,f,s->corpo.x,s->corpo.y,dt,1.28f);break;
        case MARIA_PROCURAR:m->tempoBusca-=dt;m->tempoNovoAlvoBusca-=dt;if(m->tempoBusca<=0){m->estado=MARIA_PATRULHA;m->tempoRecalcularCaminho=0;}else{if(m->tempoNovoAlvoBusca<=0||distancia(m->corpo.x,m->corpo.y,m->alvoX,m->alvoY)<30){alvoBusca(m,f);m->tempoNovoAlvoBusca=.9f;}moverMaria(m,f,m->alvoX,m->alvoY,dt,.95f);}break;
        case MARIA_CAPTURAR:break;
    }

    if(m->cooldownPassoAudio>0)m->cooldownPassoAudio-=dt;
    if(m->movendo&&m->cooldownPassoAudio<=0)
    {
        if(audio)
        {
            if(m->estado==MARIA_PERSEGUIR)tocarEfeitoPosicional(audio->mariaCorrida,m->corpo.x,0.28f);
            else tocarEfeitoPosicional(audio->mariaPasso,m->corpo.x,0.20f);
        }
        m->cooldownPassoAudio=(m->estado==MARIA_PERSEGUIR)?0.22f:0.38f;
    }

    if(m->estado!=MARIA_CAPTURAR&&distancia(m->corpo.x,m->corpo.y,s->corpo.x,s->corpo.y)<38)
    {
        m->estado=MARIA_CAPTURAR;m->movendo=false;reiniciarAnimacao(&m->pick);
        if(audio)tocarEfeitoPosicional(audio->captura,m->corpo.x,0.78f);
        return;
    }

    if(m->estado==MARIA_PERSEGUIR)atualizarAnimacaoLoop(&m->run,dt);
    else if(m->movendo)atualizarAnimacaoLoop(&m->walk,dt);
    else atualizarAnimacaoLoop(&m->idle,dt);
}
