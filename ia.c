#include "jogo.h"

void emitirSom(EventoSom* s,TipoSom tipo,float x,float y,float alcance)
{
    if(!s)return;s->tipo=tipo;s->x=x;s->y=y;s->alcance=alcance;s->tempoRestante=.35f;s->ativo=true;s->processado=false;
}
void atualizarSom(EventoSom* s,float dt)
{
    if(!s||!s->ativo)return;s->tempoRestante-=dt;if(s->tempoRestante<=0){s->ativo=false;s->tipo=SOM_NENHUM;s->processado=false;}
}
bool mariaOuveSom(const Maria* m,const EventoSom* s)
{
    if(!m||!s||!s->ativo)return false;float a=fminf(s->alcance,m->alcanceAudicao);return distancia(m->corpo.x,m->corpo.y,s->x,s->y)<=a;
}

static bool segmentoRet(float x1,float y1,float x2,float y2,Retangulo r)
{
    float dx=x2-x1,dy=y2-y1,t0=0,t1=1;
    float p[4]={-dx,dx,-dy,dy};float q[4]={x1-r.x,r.x+r.largura-x1,y1-r.y,r.y+r.altura-y1};
    for(int i=0;i<4;i++)
    {
        if(fabsf(p[i])<.00001f){if(q[i]<0)return false;}
        else
        {
            float t=q[i]/p[i];
            if(p[i]<0){if(t>t1)return false;if(t>t0)t0=t;}
            else{if(t<t0)return false;if(t<t1)t1=t;}
        }
    }
    return true;
}

bool linhaVisaoLivre(const Fase* f,float x1,float y1,float x2,float y2)
{
    if(!f)return false;
    for(int i=0;i<f->quantidadeObstaculos;i++)
    {
        const Obstaculo* o=&f->obstaculos[i];if(!o->bloqueiaVisao)continue;
        Retangulo r={o->x,o->y,o->largura,o->altura};
        if(segmentoRet(x1,y1,x2,y2,r))return false;
    }
    return true;
}

bool mariaVeScooby(const Maria* m,const Scooby* s,const Fase* f)
{
    if(!m||!s||!f)return false;
    if(distancia(m->corpo.x,m->corpo.y,s->corpo.x,s->corpo.y)>m->alcanceVisao)return false;
    float a=atan2f(s->corpo.y-m->corpo.y,s->corpo.x-m->corpo.x);
    if(fabsf(normalizarAngulo(a-m->corpo.direcao))>m->anguloVisao*.5f)return false;
    return linhaVisaoLivre(f,m->corpo.x,m->corpo.y,s->corpo.x,s->corpo.y);
}

bool mariaPodeCapturar(const Maria* m,const Scooby* s,const Fase* f)
{
    if(!m||!s||!f)return false;
    Retangulo a=hitboxPersonagem(&m->corpo,m->corpo.x,m->corpo.y);
    Retangulo b=hitboxPersonagem(&s->corpo,s->corpo.x,s->corpo.y);
    float dx=0,dy=0;
    if(a.x+a.largura<b.x)dx=b.x-(a.x+a.largura);else if(b.x+b.largura<a.x)dx=a.x-(b.x+b.largura);
    if(a.y+a.altura<b.y)dy=b.y-(a.y+a.altura);else if(b.y+b.altura<a.y)dy=a.y-(b.y+b.altura);
    if(sqrtf(dx*dx+dy*dy)>13.0f)return false;
    return linhaVisaoLivre(f,m->corpo.x,m->corpo.y,s->corpo.x,s->corpo.y);
}

static int col(float x){int c=(int)((x-MAPA_X)/TAM_CELULA);if(c<0)c=0;if(c>=GRID_COLS)c=GRID_COLS-1;return c;}
static int row(float y){int r=(int)((y-MAPA_Y)/TAM_CELULA);if(r<0)r=0;if(r>=GRID_ROWS)r=GRID_ROWS-1;return r;}
static Ponto centro(int c,int r){return(Ponto){MAPA_X+c*TAM_CELULA+TAM_CELULA*.5f,MAPA_Y+r*TAM_CELULA+TAM_CELULA*.5f};}
static bool celulaLivre(const Maria* m,const Fase* f,int c,int r)
{
    if(!m||!f||c<0||c>=GRID_COLS||r<0||r>=GRID_ROWS)return false;
    Ponto p=centro(c,r);return pontoLivreParaPersonagem(&m->corpo,f,p.x,p.y);
}

static bool destinoLivre(const Maria* m,const Fase* f,int* tc,int* tr)
{
    if(celulaLivre(m,f,*tc,*tr))return true;
    int oc=*tc,or=*tr;
    for(int raio=1;raio<=8;raio++)
    {
        for(int dr=-raio;dr<=raio;dr++)for(int dc=-raio;dc<=raio;dc++)
        {
            if(abs(dc)!=raio&&abs(dr)!=raio)continue;
            int c=oc+dc,r=or+dr;if(celulaLivre(m,f,c,r)){*tc=c;*tr=r;return true;}
        }
    }
    return false;
}

static bool calcularCaminho(Maria* m,const Fase* f,float destinoX,float destinoY)
{
    int sc=col(m->corpo.x),sr=row(m->corpo.y),tc=col(destinoX),tr=row(destinoY);
    bool originalLivre=pontoLivreParaPersonagem(&m->corpo,f,destinoX,destinoY);
    if(!destinoLivre(m,f,&tc,&tr))return false;

    m->quantidadeCaminho=0;m->indiceCaminho=0;
    if(sc==tc&&sr==tr)
    {
        Ponto alvo=originalLivre?(Ponto){destinoX,destinoY}:centro(tc,tr);
        m->caminho[0]=alvo;m->quantidadeCaminho=1;
        m->alvoNavegavelX=alvo.x;m->alvoNavegavelY=alvo.y;
        m->ultimoAlvoCaminhoX=destinoX;m->ultimoAlvoCaminhoY=destinoY;m->tempoRecalcularCaminho=.22f;
        return true;
    }

    bool vis[GRID_ROWS][GRID_COLS]={{false}};short pc[GRID_ROWS][GRID_COLS],pr[GRID_ROWS][GRID_COLS];
    short qc[GRID_TOTAL],qr[GRID_TOTAL];
    for(int r=0;r<GRID_ROWS;r++)for(int c=0;c<GRID_COLS;c++){pc[r][c]=-1;pr[r][c]=-1;}
    int ini=0,fim=0;qc[fim]=sc;qr[fim++]=sr;vis[sr][sc]=true;
    const int dc[4]={1,-1,0,0},dr[4]={0,0,1,-1};bool achou=false;
    while(ini<fim)
    {
        int c=qc[ini],r=qr[ini++];if(c==tc&&r==tr){achou=true;break;}
        for(int k=0;k<4;k++)
        {
            int nc=c+dc[k],nr=r+dr[k];if(nc<0||nc>=GRID_COLS||nr<0||nr>=GRID_ROWS||vis[nr][nc])continue;
            if(!celulaLivre(m,f,nc,nr))continue;
            vis[nr][nc]=true;pc[nr][nc]=(short)c;pr[nr][nc]=(short)r;
            if(fim<GRID_TOTAL){qc[fim]=nc;qr[fim++]=nr;}
        }
    }
    if(!achou)return false;

    Ponto inv[MAX_CAMINHO];int qtd=0,c=tc,r=tr;
    while(!(c==sc&&r==sr)&&qtd<MAX_CAMINHO)
    {
        inv[qtd++]=centro(c,r);int nc=pc[r][c],nr=pr[r][c];if(nc<0||nr<0)break;c=nc;r=nr;
    }
    for(int i=qtd-1;i>=0&&m->quantidadeCaminho<MAX_CAMINHO;i--)m->caminho[m->quantidadeCaminho++]=inv[i];
    if(originalLivre&&m->quantidadeCaminho<MAX_CAMINHO)m->caminho[m->quantidadeCaminho++]=(Ponto){destinoX,destinoY};
    Ponto efetivo=originalLivre?(Ponto){destinoX,destinoY}:centro(tc,tr);
    m->alvoNavegavelX=efetivo.x;m->alvoNavegavelY=efetivo.y;
    m->ultimoAlvoCaminhoX=destinoX;m->ultimoAlvoCaminhoY=destinoY;m->tempoRecalcularCaminho=.22f;
    return m->quantidadeCaminho>0;
}

static void passoMaria(Maria* m,RecursosAudio* a,int anterior,bool corrida)
{
    Animacao* an=corrida?&m->run:&m->walk;if(an->frameAtual==anterior||(an->frameAtual!=1&&an->frameAtual!=3)||!a)return;
    tocarEfeitoPosicional(corrida?a->mariaCorrida:a->mariaPasso,m->corpo.x,corrida?.28f:.20f);
}

static void antiStuck(Maria* m,float dt)
{
    float d=distancia(m->corpo.x,m->corpo.y,m->ultimaPosicaoProgressoX,m->ultimaPosicaoProgressoY);
    if(m->movendo&&d<1.5f)m->tempoSemProgresso+=dt;
    else if(d>=1.5f){m->tempoSemProgresso=0;m->ultimaPosicaoProgressoX=m->corpo.x;m->ultimaPosicaoProgressoY=m->corpo.y;}
    else m->tempoSemProgresso=0;

    if(m->tempoSemProgresso>.72f)
    {
        m->quantidadeCaminho=0;m->indiceCaminho=0;m->tempoRecalcularCaminho=0;
        m->tempoSemProgresso=0;m->ultimaPosicaoProgressoX=m->corpo.x;m->ultimaPosicaoProgressoY=m->corpo.y;m->eventosAntiStuck++;
        printf("IA anti-stuck: recalculo %d em (%.1f,%.1f)\n",m->eventosAntiStuck,m->corpo.x,m->corpo.y);
    }
}

static void moverMaria(Maria* m,const Fase* f,float x,float y,float dt,float mult)
{
    m->movendo=false;m->tempoRecalcularCaminho-=dt;
    bool mudou=distancia(x,y,m->ultimoAlvoCaminhoX,m->ultimoAlvoCaminhoY)>32.0f;
    if(m->tempoRecalcularCaminho<=0||m->indiceCaminho>=m->quantidadeCaminho||mudou)
    {
        m->quantidadeCaminho=0;m->indiceCaminho=0;
        calcularCaminho(m,f,x,y);
    }
    if(m->indiceCaminho>=m->quantidadeCaminho){antiStuck(m,dt);return;}

    Ponto alvo=m->caminho[m->indiceCaminho];float dx=alvo.x-m->corpo.x,dy=alvo.y-m->corpo.y,d=sqrtf(dx*dx+dy*dy);
    if(d<5.0f){m->indiceCaminho++;antiStuck(m,dt);return;}
    dx/=d;dy/=d;m->corpo.direcao=atan2f(dy,dx);m->direcaoSprite=direcaoSpritePorMovimento(dx,dy,m->direcaoSprite);m->movendo=true;
    moverPersonagem(&m->corpo,dx*m->corpo.velocidade*mult*dt,dy*m->corpo.velocidade*mult*dt,f);
    antiStuck(m,dt);
}

static bool chegou(const Maria* m){return distancia(m->corpo.x,m->corpo.y,m->alvoNavegavelX,m->alvoNavegavelY)<22.0f;}
static void alvoBusca(Maria* m,const Fase* f)
{
    for(int t=0;t<18;t++)
    {
        float a=(float)(rand()%628)/100.0f,r=55+(float)(rand()%145);
        Ponto p={m->ultimaPosicaoVistaX+cosf(a)*r,m->ultimaPosicaoVistaY+sinf(a)*r};
        if(pontoLivreParaPersonagem(&m->corpo,f,p.x,p.y)){m->alvoX=p.x;m->alvoY=p.y;m->tempoRecalcularCaminho=0;return;}
    }
    m->alvoX=m->ultimaPosicaoVistaX;m->alvoY=m->ultimaPosicaoVistaY;
}

const char* nomeEstadoMaria(EstadoMaria e)
{
    switch(e){case MARIA_PATRULHA:return"PATRULHA";case MARIA_INVESTIGAR:return"INVESTIGAR";case MARIA_PERSEGUIR:return"PERSEGUIR";case MARIA_PROCURAR:return"PROCURAR";case MARIA_CAPTURAR:return"CAPTURAR";}return"?";
}

void atualizarMaria(Maria* m,const Scooby* s,EventoSom* som,const Fase* f,RecursosAudio* audio,float dt)
{
    if(!m||!s||!f)return;
    EstadoMaria antes=m->estado;m->movendo=false;m->capturaConcluida=false;
    if(m->estado==MARIA_CAPTURAR){if(atualizarAnimacaoUmaVez(&m->pick,dt))m->capturaConcluida=true;return;}

    bool viu=mariaVeScooby(m,s,f);
    if(viu)
    {
        m->estado=MARIA_PERSEGUIR;m->alvoX=s->corpo.x;m->alvoY=s->corpo.y;m->ultimaPosicaoVistaX=s->corpo.x;m->ultimaPosicaoVistaY=s->corpo.y;
    }
    else
    {
        if(m->estado==MARIA_PERSEGUIR){m->estado=MARIA_PROCURAR;m->alvoX=m->ultimaPosicaoVistaX;m->alvoY=m->ultimaPosicaoVistaY;m->tempoBusca=4.0f;m->tempoNovoAlvoBusca=.6f;m->tempoRecalcularCaminho=0;}
        if(som&&som->ativo&&!som->processado&&mariaOuveSom(m,som))
        {m->estado=MARIA_INVESTIGAR;m->alvoX=som->x;m->alvoY=som->y;m->ultimaPosicaoVistaX=som->x;m->ultimaPosicaoVistaY=som->y;m->tempoRecalcularCaminho=0;som->processado=true;}
    }
    if(audio&&m->estado!=antes&&(m->estado==MARIA_INVESTIGAR||m->estado==MARIA_PERSEGUIR))tocarEfeitoPosicional(audio->mariaAlerta,m->corpo.x,.52f);

    switch(m->estado)
    {
        case MARIA_PATRULHA:
            if(f->quantidadeWaypoints>0)
            {Ponto p=f->waypoints[m->waypointAtual];moverMaria(m,f,p.x,p.y,dt,1.0f);if(distancia(m->corpo.x,m->corpo.y,p.x,p.y)<24){m->waypointAtual=(m->waypointAtual+1)%f->quantidadeWaypoints;m->tempoRecalcularCaminho=0;}}
            break;
        case MARIA_INVESTIGAR:
            moverMaria(m,f,m->alvoX,m->alvoY,dt,1.06f);if(chegou(m)){m->estado=MARIA_PROCURAR;m->ultimaPosicaoVistaX=m->alvoNavegavelX;m->ultimaPosicaoVistaY=m->alvoNavegavelY;m->tempoBusca=3.2f;m->tempoNovoAlvoBusca=0;}
            break;
        case MARIA_PERSEGUIR:moverMaria(m,f,s->corpo.x,s->corpo.y,dt,1.25f);break;
        case MARIA_PROCURAR:
            m->tempoBusca-=dt;m->tempoNovoAlvoBusca-=dt;if(m->tempoBusca<=0){m->estado=MARIA_PATRULHA;m->tempoRecalcularCaminho=0;}
            else{if(m->tempoNovoAlvoBusca<=0||chegou(m)){alvoBusca(m,f);m->tempoNovoAlvoBusca=.85f;}moverMaria(m,f,m->alvoX,m->alvoY,dt,.95f);}break;
        case MARIA_CAPTURAR:break;
    }

    if(mariaPodeCapturar(m,s,f))
    {m->estado=MARIA_CAPTURAR;m->movendo=false;reiniciarAnimacao(&m->pick);if(audio)tocarEfeitoPosicional(audio->captura,m->corpo.x,.78f);return;}

    if(m->estado==MARIA_PERSEGUIR){int fr=m->run.frameAtual;atualizarAnimacaoLoop(&m->run,dt);if(m->movendo)passoMaria(m,audio,fr,true);}
    else if(m->movendo){int fr=m->walk.frameAtual;atualizarAnimacaoLoop(&m->walk,dt);passoMaria(m,audio,fr,false);}
    else atualizarAnimacaoLoop(&m->idle,dt);
}
