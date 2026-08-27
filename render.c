#include "jogo.h"

static void desenharBola(const RecursosMapa* r,const Bola* b)
{
    if(!r->bolas||b->coletada)return;
    static const int sx[QTD_CORES_BOLA]={117,536,955,1374,1793};
    static const int sy[QTD_CORES_BOLA]={219,219,219,219,219};
    static const int sw[QTD_CORES_BOLA]={262,262,262,262,263};
    static const int sh[QTD_CORES_BOLA]={259,259,259,259,259};
    float t=42;
    al_draw_scaled_bitmap(r->bolas,sx[b->cor],sy[b->cor],sw[b->cor],sh[b->cor],b->x-t/2,b->y-t/2,t,t,0);
}

static void desenharScooby(const Scooby* s,const Bola* b)
{
    const Animacao* a=&s->idle;
    if(s->carregandoBola)a=&s->carregar[b->cor];else if(s->latindo)a=&s->bark;else if(s->mordendo)a=&s->bite;else if(s->correndo)a=&s->run;else if(s->movendo)a=&s->walk;
    desenharAnimacao(a,s->direcaoSprite,s->corpo.x,s->corpo.y,.37f);
}

static void desenharMaria(const Maria* m)
{
    const Animacao* a=&m->idle;
    if(m->estado==MARIA_CAPTURAR)a=&m->pick;else if(m->estado==MARIA_PERSEGUIR)a=&m->run;else if(m->movendo)a=&m->walk;
    desenharAnimacao(a,m->direcaoSprite,m->corpo.x,m->corpo.y,.31f);
}

static void objetos(const Fase* f,float min,float max){for(int i=0;i<f->quantidadeObjetos;i++){float y=baseYObjeto(&f->objetos[i]);if(y>=min&&y<max)desenharObjeto(f,&f->objetos[i]);}}

void desenharCena(const Fase* f,const RecursosMapa* r,const Scooby* s,const Maria* m,const Bola* b,const EventoSom* som,bool debug,int vidas,int faseAtual)
{
    al_clear_to_color(al_map_rgb(22,20,22));
    if(f->fundo)al_draw_scaled_bitmap(f->fundo,0,0,al_get_bitmap_width(f->fundo),al_get_bitmap_height(f->fundo),MAPA_X,MAPA_Y,MAPA_TELA_W,MAPA_TELA_H,0);
    bool sp=s->corpo.y<=m->corpo.y;float y1=sp?s->corpo.y:m->corpo.y,y2=sp?m->corpo.y:s->corpo.y;
    objetos(f,-9999,y1);desenharBola(r,b);if(sp)desenharScooby(s,b);else desenharMaria(m);objetos(f,y1,y2);if(sp)desenharMaria(m);else desenharScooby(s,b);objetos(f,y2,99999);

    for(int i=0;i<3;i++){ALLEGRO_COLOR c=i<vidas?al_map_rgb(220,65,90):al_map_rgb(65,65,70);float x=35+i*38,y=40;al_draw_filled_circle(x-6,y,9,c);al_draw_filled_circle(x+6,y,9,c);al_draw_filled_triangle(x-15,y+2,x+15,y+2,x,y+21,c);}
    for(int i=0;i<QTD_FASES;i++){ALLEGRO_COLOR c=i<=faseAtual?al_map_rgb(240,190,70):al_map_rgb(70,70,75);al_draw_filled_rectangle(28+i*28,92,46+i*28,110,c);}

    if(debug)
    {
        al_draw_circle(m->corpo.x,m->corpo.y,m->alcanceAudicao,al_map_rgba(70,135,255,140),2);
        float a1=m->corpo.direcao-m->anguloVisao/2,a2=m->corpo.direcao+m->anguloVisao/2;ALLEGRO_COLOR amarelo=al_map_rgba(255,220,70,190);
        al_draw_line(m->corpo.x,m->corpo.y,m->corpo.x+cosf(a1)*m->alcanceVisao,m->corpo.y+sinf(a1)*m->alcanceVisao,amarelo,2);
        al_draw_line(m->corpo.x,m->corpo.y,m->corpo.x+cosf(a2)*m->alcanceVisao,m->corpo.y+sinf(a2)*m->alcanceVisao,amarelo,2);
        al_draw_arc(m->corpo.x,m->corpo.y,m->alcanceVisao,a1,m->anguloVisao,amarelo,2);
        for(int i=0;i<f->quantidadeObstaculos;i++){const Obstaculo* o=&f->obstaculos[i];al_draw_rectangle(o->x,o->y,o->x+o->largura,o->y+o->altura,o->bloqueiaVisao?al_map_rgb(255,80,80):al_map_rgb(80,255,110),1);}
        al_draw_rectangle(f->saida.x,f->saida.y,f->saida.x+f->saida.largura,f->saida.y+f->saida.altura,al_map_rgb(80,255,230),3);
        if(som->ativo)al_draw_circle(som->x,som->y,som->alcance,al_map_rgba(255,80,80,100),2);
        for(int i=m->indiceCaminho;i<m->quantidadeCaminho;i++)al_draw_filled_circle(m->caminho[i].x,m->caminho[i].y,3,al_map_rgb(190,80,255));
    }
    al_flip_display();
}

void desenharTelaFinal(EstadoJogo e)
{
    al_clear_to_color(e==JOGO_VITORIA?al_map_rgb(45,110,70):al_map_rgb(120,45,55));float x=LARGURA_TELA/2.0f,y=ALTURA_TELA/2.0f;
    if(e==JOGO_VITORIA){al_draw_filled_circle(x,y,110,al_map_rgb(245,205,70));al_draw_filled_circle(x,y,75,al_map_rgb(255,230,120));al_draw_filled_triangle(x-35,y+15,x,y-45,x+35,y+15,al_map_rgb(65,125,80));}
    else{al_draw_line(x-70,y-70,x+70,y+70,al_map_rgb(245,220,220),15);al_draw_line(x+70,y-70,x-70,y+70,al_map_rgb(245,220,220),15);}al_flip_display();
}
