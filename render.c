#include "jogo.h"

typedef enum { ITEM_OBJETO, ITEM_BOLA, ITEM_SCOOBY, ITEM_MARIA } TipoItem;
typedef struct { TipoItem tipo; float y; int indice; } ItemRender;

static void ordenar(ItemRender* a,int n)
{
    for(int i=1;i<n;i++){ItemRender k=a[i];int j=i-1;while(j>=0&&a[j].y>k.y){a[j+1]=a[j];j--;}a[j+1]=k;}
}

static void bola(const RecursosMapa* r,const Bola* b)
{
    if(!r||!r->bolas||!b||b->coletada)return;
    static const int sx[5]={117,536,955,1374,1793};
    static const int sw[5]={262,262,262,262,263};
    int c=b->cor;if(c<0||c>=5)c=0;float t=38;
    al_draw_scaled_bitmap(r->bolas,sx[c],219,sw[c],259,b->x-t*.5f,b->y-t*.78f,t,t,0);
}

static const Animacao* animScooby(const Scooby* s,const Bola* b)
{
    if(s->carregandoBola)return &s->carregar[b->cor];
    if(s->latindo)return &s->bark;if(s->mordendo)return &s->bite;if(s->correndo)return &s->run;if(s->movendo)return &s->walk;return &s->idle;
}
static const Animacao* animMaria(const Maria* m)
{
    if(m->estado==MARIA_CAPTURAR)return &m->pick;if(m->estado==MARIA_PERSEGUIR)return &m->run;if(m->movendo)return &m->walk;return &m->idle;
}

static void indicadorSaida(const Fase* f,float t)
{
    float p=.55f+.45f*sinf(t*5),cx=f->triggerSaida.x+f->triggerSaida.largura*.5f;
    ALLEGRO_COLOR c=al_map_rgba(255,225,70,(unsigned char)(100+p*110));
    al_draw_rectangle(f->triggerSaida.x,f->triggerSaida.y,f->triggerSaida.x+f->triggerSaida.largura,f->triggerSaida.y+f->triggerSaida.altura,c,3);
    float yy=f->triggerSaida.y+18;
    al_draw_filled_triangle(cx,yy+24,cx-15,yy,cx+15,yy,c);
}

static void hud(const RecursosMapa* r,const Fase* f,const Scooby* s,const Bola* b,int vidas,int fase,float tutorial,EstadoJogo estado)
{
    if(!r||!r->fonte)return;
    al_draw_filled_rounded_rectangle(15,14,430,118,10,10,al_map_rgba(15,15,20,188));
    char txt[256];snprintf(txt,sizeof(txt),"Fase %d/4 - %s",fase+1,f->nome);al_draw_text(r->fonte,al_map_rgb(255,240,205),28,25,0,txt);
    snprintf(txt,sizeof(txt),"Vidas: %d   Bola: %s",vidas,NOMES_CORES[b->cor]);al_draw_text(r->fonte,al_map_rgb(245,205,215),28,50,0,txt);
    al_draw_text(r->fonte,al_map_rgb(210,235,255),28,76,0,s->carregandoBola?"Objetivo: alcance a saida indicada.":"Objetivo: encontre e pegue a bolinha.");

    if(tutorial>0&&fase==0&&estado==JOGO_RODANDO)
    {
        al_draw_filled_rounded_rectangle(355,625,925,700,10,10,al_map_rgba(10,10,15,210));
        al_draw_text(r->fonte,al_map_rgb(255,255,255),640,638,ALLEGRO_ALIGN_CENTRE,"WASD mover | Shift correr | Espaco latir | E pegar bola");
        al_draw_text(r->fonte,al_map_rgb(255,225,120),640,666,ALLEGRO_ALIGN_CENTRE,"Encontre a bola e escape pela passagem real sem Maria capturar Scooby.");
    }
    if(estado==JOGO_PAUSADO)
    {
        al_draw_filled_rectangle(0,0,LARGURA_TELA,ALTURA_TELA,al_map_rgba(0,0,0,165));
        al_draw_text(r->fonte,al_map_rgb(255,255,255),640,330,ALLEGRO_ALIGN_CENTRE,"PAUSADO");
        al_draw_text(r->fonte,al_map_rgb(220,220,225),640,365,ALLEGRO_ALIGN_CENTRE,"Esc para continuar");
    }
}

static void caixa(Retangulo r,ALLEGRO_COLOR c,float esp){al_draw_rectangle(r.x,r.y,r.x+r.largura,r.y+r.altura,c,esp);}

static bool intersecaoDebug(Retangulo a,Retangulo b,Retangulo* inter)
{
    float x1=fmaxf(a.x,b.x),y1=fmaxf(a.y,b.y);
    float x2=fminf(a.x+a.largura,b.x+b.largura),y2=fminf(a.y+a.altura,b.y+b.altura);
    if(x2-x1<=.05f||y2-y1<=.05f)return false;
    if(inter)*inter=(Retangulo){x1,y1,x2-x1,y2-y1};
    return true;
}

static void debugDraw(const Fase* f,const Scooby* s,const Maria* m,const Bola* b,const EventoSom* som,const RecursosMapa* r)
{
    for(int rr=0;rr<GRID_ROWS;rr++)for(int cc=0;cc<GRID_COLS;cc++)
    {
        float x=MAPA_X+cc*TAM_CELULA,y=MAPA_Y+rr*TAM_CELULA;
        float cx=x+TAM_CELULA*.5f,cy=y+TAM_CELULA*.5f;
        if(pontoDentroRetangulo(cx,cy,f->areaJogavel))
        {
            bool livre=pontoLivreParaPersonagem(&m->corpo,f,cx,cy);
            al_draw_rectangle(x,y,x+TAM_CELULA,y+TAM_CELULA,
                livre?al_map_rgba(100,180,120,28):al_map_rgba(255,50,50,45),.5f);
        }
    }

    caixa(f->areaJogavel,al_map_rgb(80,220,255),2);
    for(int i=0;i<f->quantidadeObstaculos;i++)
    {
        const Obstaculo* o=&f->obstaculos[i];
        ALLEGRO_COLOR c=o->parede?al_map_rgb(180,40,40):(o->bloqueiaVisao?al_map_rgb(255,135,35):al_map_rgb(255,70,70));
        caixa((Retangulo){o->x,o->y,o->largura,o->altura},c,o->parede?2:1);
    }

    caixa(f->triggerSaida,al_map_rgb(50,255,235),3);
    al_draw_filled_circle(f->alvoEntradaSaida.x,f->alvoEntradaSaida.y,5,al_map_rgb(50,255,235));

    for(int i=0;i<f->quantidadeSpawnsBola;i++)
    {al_draw_circle(f->spawnsBola[i].x,f->spawnsBola[i].y,8,al_map_rgb(70,150,255),2);}
    for(int i=0;i<f->quantidadeWaypoints;i++)
    {al_draw_filled_circle(f->waypoints[i].x,f->waypoints[i].y,4,al_map_rgb(235,70,255));}

    for(int i=m->indiceCaminho;i<m->quantidadeCaminho;i++)
    {
        if(i>m->indiceCaminho)al_draw_line(m->caminho[i-1].x,m->caminho[i-1].y,m->caminho[i].x,m->caminho[i].y,al_map_rgb(170,70,255),2);
        al_draw_filled_circle(m->caminho[i].x,m->caminho[i].y,3,al_map_rgb(190,80,255));
    }

    /* Verde = exatamente a hitbox fisica efetiva do Scooby. */
    HitboxScooby hs=obterHitboxScooby(s,s->corpo.x,s->corpo.y);
    Retangulo hm=hitboxPersonagem(&m->corpo,m->corpo.x,m->corpo.y);
    caixa(hs.corpo,al_map_rgb(70,255,95),2);
    caixa(hm,al_map_rgb(255,235,60),2);
    al_draw_filled_circle(m->corpo.x,m->corpo.y,4,al_map_rgb(255,230,40));
    if(!b->coletada)al_draw_circle(b->x,b->y,RAIO_BOLA,al_map_rgb(70,150,255),2);

    /* Diagnostico critico: qualquer penetracao real fica vermelha no F1. */
    int overlaps=0;
    for(int i=0;i<f->quantidadeObstaculos;i++)
    {
        const Obstaculo* o=&f->obstaculos[i];
        if(!o->bloqueiaMovimento)continue;
        Retangulo inter;
        if(intersecaoDebug(hs.corpo,(Retangulo){o->x,o->y,o->largura,o->altura},&inter))
        {
            overlaps++;
            al_draw_filled_rectangle(inter.x,inter.y,inter.x+inter.largura,inter.y+inter.altura,al_map_rgba(255,20,20,155));
            caixa(inter,al_map_rgb(255,255,255),2);
        }
    }

    al_draw_circle(m->corpo.x,m->corpo.y,m->alcanceAudicao,al_map_rgba(70,135,255,140),2);
    float a1=m->corpo.direcao-m->anguloVisao*.5f,a2=m->corpo.direcao+m->anguloVisao*.5f;ALLEGRO_COLOR y=al_map_rgba(255,220,70,190);
    al_draw_line(m->corpo.x,m->corpo.y,m->corpo.x+cosf(a1)*m->alcanceVisao,m->corpo.y+sinf(a1)*m->alcanceVisao,y,2);
    al_draw_line(m->corpo.x,m->corpo.y,m->corpo.x+cosf(a2)*m->alcanceVisao,m->corpo.y+sinf(a2)*m->alcanceVisao,y,2);
    al_draw_arc(m->corpo.x,m->corpo.y,m->alcanceVisao,a1,m->anguloVisao,y,2);
    if(som&&som->ativo)al_draw_circle(som->x,som->y,som->alcance,al_map_rgba(255,80,80,100),2);

    if(r&&r->fonte)
    {
        char txt[320];snprintf(txt,sizeof(txt),"IA: %s | caminho %d/%d | stuck %.2fs (%d) | bola %s | Scooby %s (%d)",
            nomeEstadoMaria(m->estado),m->indiceCaminho,m->quantidadeCaminho,m->tempoSemProgresso,m->eventosAntiStuck,b->coletada?"coletada":"no mapa",overlaps?"OVERLAP":"LIVRE",overlaps);
        al_draw_filled_rectangle(450,12,1260,39,al_map_rgba(0,0,0,190));
        al_draw_text(r->fonte,overlaps?al_map_rgb(255,90,90):al_map_rgb(255,255,255),462,20,0,txt);
        if(overlaps>0)
            al_draw_text(r->fonte,al_map_rgb(255,60,60),640,45,ALLEGRO_ALIGN_CENTRE,"OVERLAP DETECTADO - COLISAO INVALIDA");
    }
}

void desenharCena(const Fase* f,const RecursosMapa* r,const Scooby* s,const Maria* m,const Bola* b,
                  const EventoSom* som,bool debug,int vidas,int faseAtual,EstadoJogo estado,float fade,float tutorial)
{
    if(!f||!r||!s||!m||!b)return;
    al_clear_to_color(al_map_rgb(22,20,22));
    if(f->fundo)al_draw_scaled_bitmap(f->fundo,0,0,al_get_bitmap_width(f->fundo),al_get_bitmap_height(f->fundo),MAPA_X,MAPA_Y,MAPA_TELA_W,MAPA_TELA_H,0);
    if(s->carregandoBola&&estado!=JOGO_TRANSICAO_FASE)indicadorSaida(f,(float)al_get_time());

    ItemRender itens[MAX_OBJETOS+3];int n=0;
    for(int i=0;i<f->quantidadeObjetos;i++)itens[n++]=(ItemRender){ITEM_OBJETO,baseYObjeto(&f->objetos[i]),i};
    if(!b->coletada)itens[n++]=(ItemRender){ITEM_BOLA,b->y,0};
    itens[n++]=(ItemRender){ITEM_SCOOBY,s->corpo.y,0};itens[n++]=(ItemRender){ITEM_MARIA,m->corpo.y,0};
    ordenar(itens,n);
    for(int i=0;i<n;i++)
    {
        switch(itens[i].tipo)
        {case ITEM_OBJETO:desenharObjeto(f,&f->objetos[itens[i].indice]);break;case ITEM_BOLA:bola(r,b);break;
         case ITEM_SCOOBY:desenharAnimacao(animScooby(s,b),s->direcaoSprite,s->corpo.x,s->corpo.y);break;
         case ITEM_MARIA:desenharAnimacao(animMaria(m),m->direcaoSprite,m->corpo.x,m->corpo.y);break;}
    }

    if(debug)debugDraw(f,s,m,b,som,r);
    hud(r,f,s,b,vidas,faseAtual,tutorial,estado);

    if(fade>0)al_draw_filled_rectangle(0,0,LARGURA_TELA,ALTURA_TELA,al_map_rgba(0,0,0,(unsigned char)fminf(255,fade)));
    al_flip_display();
}

void desenharTelaFinal(EstadoJogo estado,const RecursosMapa* r)
{
    al_clear_to_color(al_map_rgb(24,20,25));
    if(r&&r->fonte)
    {
        const char* t=estado==JOGO_VITORIA?"VOCE VENCEU!":"GAME OVER";
        al_draw_text(r->fonte,estado==JOGO_VITORIA?al_map_rgb(255,225,90):al_map_rgb(255,100,100),640,315,ALLEGRO_ALIGN_CENTRE,t);
        al_draw_text(r->fonte,al_map_rgb(230,230,230),640,355,ALLEGRO_ALIGN_CENTRE,"R para jogar novamente | Esc para sair");
    }
    al_flip_display();
}

void desenharCarregando(ALLEGRO_DISPLAY* display,ALLEGRO_FONT* fonte)
{
    if(!display)return;al_set_target_backbuffer(display);al_clear_to_color(al_map_rgb(18,17,20));
    if(fonte)al_draw_text(fonte,al_map_rgb(245,235,215),640,345,ALLEGRO_ALIGN_CENTRE,"Carregando fase...");
    al_flip_display();
}
