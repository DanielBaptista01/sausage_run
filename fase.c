#include "jogo.h"

static float clamp01(float v){return v<0?0:v>1?1:v;}

static void objeto(Fase* f, int sx,int sy,int sw,int sh,
                   float x,float y,float escala,
                   float bx,float by,float bw,float bh,
                   bool bloqueia,bool visao,bool atras,int inset,int visualId)
{
    if(!f||f->quantidadeObjetos>=MAX_OBJETOS)return;
    ObjetoMapa* o=&f->objetos[f->quantidadeObjetos++];
    memset(o,0,sizeof(*o));
    o->tipoVisual=OBJ_SPRITE;
    o->sx=sx;o->sy=sy;o->sw=sw;o->sh=sh;o->insetFonte=inset;o->visualId=visualId;
    o->mapaX=x;o->mapaY=y;o->escala=escala;
    o->baseX=clamp01(bx);o->baseY=clamp01(by);o->baseW=clamp01(bw);o->baseH=clamp01(bh);
    if(o->baseX+o->baseW>1)o->baseW=1-o->baseX;
    if(o->baseY+o->baseH>1)o->baseH=1-o->baseY;
    o->depthAnchorY=clamp01(o->baseY+o->baseH);
    o->bloqueiaMovimento=bloqueia;o->bloqueiaVisao=visao;o->permitePassarAtras=atras;
}

/*
 * Alguns elementos visuais fazem parte de um recorte composto do atlas, mas
 * ocupam bases fisicas independentes no mapa. Nesses casos usamos um objeto
 * somente de colisao. visualId=-1 faz com que ele participe de colisao/F1,
 * mas nao desenhe uma segunda copia do sprite.
 */
static void colisorSomente(Fase* f,float x,float y,float w,float h,bool visao)
{
    if(!f||f->quantidadeObjetos>=MAX_OBJETOS||w<=0||h<=0)return;

    ObjetoMapa* o=&f->objetos[f->quantidadeObjetos++];
    memset(o,0,sizeof(*o));
    o->tipoVisual=OBJ_SPRITE;
    o->sx=0;o->sy=0;o->sw=(int)w;o->sh=(int)h;
    o->insetFonte=0;
    o->visualId=-1;
    o->mapaX=x;o->mapaY=y;o->escala=1.0f;
    o->baseX=0;o->baseY=0;o->baseW=1;o->baseH=1;
    o->depthAnchorY=1.0f;
    o->bloqueiaMovimento=true;
    o->bloqueiaVisao=visao;
    o->permitePassarAtras=false;
}

static void obstaculo(Fase* f,float x,float y,float w,float h,bool visao,bool parede)
{
    if(!f||f->quantidadeObstaculos>=MAX_OBSTACULOS||w<=0||h<=0)return;
    f->obstaculos[f->quantidadeObstaculos++]=(Obstaculo){x,y,w,h,true,visao,parede};
}

static Retangulo areaFonte(float x,float y,float w,float h)
{
    return (Retangulo){mapaParaTelaX(x),mapaParaTelaY(y),w*MAPA_ESCALA,h*MAPA_ESCALA};
}

static void waypoint(Fase* f,float x,float y)
{
    if(f->quantidadeWaypoints<MAX_WAYPOINTS)
        f->waypoints[f->quantidadeWaypoints++]=(Ponto){mapaParaTelaX(x),mapaParaTelaY(y)};
}

static void spawnBola(Fase* f,float x,float y)
{
    if(f->quantidadeSpawnsBola<MAX_SPAWNS_BOLA)
        f->spawnsBola[f->quantidadeSpawnsBola++]=(Ponto){mapaParaTelaX(x),mapaParaTelaY(y)};
}

static Retangulo baseObjeto(const ObjetoMapa* o)
{
    float dx=mapaParaTelaX(o->mapaX),dy=mapaParaTelaY(o->mapaY);
    float dw=o->sw*MAPA_ESCALA*o->escala,dh=o->sh*MAPA_ESCALA*o->escala;
    return (Retangulo){dx+dw*o->baseX,dy+dh*o->baseY,dw*o->baseW,dh*o->baseH};
}

static void paredesPerimetro(Fase* f)
{
    Retangulo a=f->areaJogavel;
    const float e=12.0f;

    /* areaJogavel representa piso real; parede/rodape ficam fora dela. */
    obstaculo(f,a.x,a.y,a.largura,e,true,true);
    obstaculo(f,a.x,a.y,e,a.altura,true,true);
    obstaculo(f,a.x+a.largura-e,a.y,e,a.altura,true,true);

    /*
     * Borda inferior sempre solida em gameplay. A unica travessia acontece
     * em atualizarScoobyTransicao(), depois do trigger correto ser ativado.
     */
    obstaculo(f,a.x,a.y+a.altura-e,a.largura,e,true,true);
}

void reconstruirColisoesFase(Fase* f)
{
    if(!f)return;
    f->quantidadeObstaculos=0;

    for(int i=0;i<f->quantidadeObjetos;i++)
    {
        ObjetoMapa* o=&f->objetos[i];
        if(!o->bloqueiaMovimento)continue;
        Retangulo b=baseObjeto(o);
        obstaculo(f,b.x-2,b.y-2,b.largura+4,b.altura+4,o->bloqueiaVisao,false);
    }

    paredesPerimetro(f);
}

void desenharObjeto(const Fase* f,const ObjetoMapa* o)
{
    if(!f||!o||!f->folhaObjetos||o->visualId<0)return;

    int inset=o->insetFonte;
    int sx=o->sx+inset,sy=o->sy+inset,sw=o->sw-inset*2,sh=o->sh-inset*2;
    if(sw<=0||sh<=0)return;

    int fw=al_get_bitmap_width(f->folhaObjetos),fh=al_get_bitmap_height(f->folhaObjetos);
    if(sx<0||sy<0||sx+sw>fw||sy+sh>fh)return;

    float e=MAPA_ESCALA*o->escala;
    float dx=mapaParaTelaX(o->mapaX)+inset*e,dy=mapaParaTelaY(o->mapaY)+inset*e;

    al_draw_scaled_bitmap(f->folhaObjetos,sx,sy,sw,sh,dx,dy,sw*e,sh*e,0);
}

float baseYObjeto(const ObjetoMapa* o)
{
    if(!o||o->visualId<0)return -9999;
    return mapaParaTelaY(o->mapaY)+o->sh*MAPA_ESCALA*o->escala*o->depthAnchorY;
}

bool validarObjetosFase(const Fase* f)
{
    if(!f||!f->folhaObjetos)return false;

    int w=al_get_bitmap_width(f->folhaObjetos),h=al_get_bitmap_height(f->folhaObjetos);
    bool ok=true;

    for(int i=0;i<f->quantidadeObjetos;i++)
    {
        const ObjetoMapa* o=&f->objetos[i];

        /* Collider sem visual nao possui source rectangle no atlas. */
        if(o->visualId<0)continue;

        int sx=o->sx+o->insetFonte;
        int sy=o->sy+o->insetFonte;
        int sw=o->sw-o->insetFonte*2;
        int sh=o->sh-o->insetFonte*2;

        if(sw<=0||sh<=0||sx<0||sy<0||sx+sw>w||sy+sh>h)
        {
            printf("ERRO %s objeto %d crop [%d,%d,%d,%d] folha=%dx%d\n",
                   f->nome,i,sx,sy,sw,sh,w,h);
            ok=false;
        }
    }

    return ok;
}

static void cozinha(Fase* f)
{
    f->nome="Cozinha";f->caminhoFundo="mapa/cozinha.png";f->caminhoObjetos="mapa/cozinha_objetos.png";f->tipoSaida=SAIDA_ESCADA;

    /* Piso real: a faixa superior de parede/azulejo nao e caminhavel. */
    f->areaJogavel=areaFonte(82,300,1284,675);
    f->triggerSaida=areaFonte(500,895,155,70);
    f->alvoEntradaSaida=(Ponto){mapaParaTelaX(578),mapaParaTelaY(965)};

    objeto(f,25,19,243,354,100,145,1.00,.05,.60,.90,.38,true,true,true,2,0);
    objeto(f,326,71,486,269,360,145,1.00,.01,.53,.98,.46,true,true,true,2,1);
    objeto(f,864,96,275,251,850,150,.96,.04,.52,.92,.46,true,true,true,2,2);
    objeto(f,1164,14,234,363,1140,135,.90,.04,.56,.92,.42,true,true,true,2,3);
    objeto(f,55,401,341,380,330,430,.94,.05,.47,.90,.51,true,true,true,2,4);
    objeto(f,838,396,160,385,790,425,.94,.04,.45,.92,.53,true,true,true,2,5);
    objeto(f,451,615,192,243,110,710,.84,.04,.52,.92,.46,true,true,true,2,6);
    objeto(f,700,708,105,124,960,730,.88,.10,.48,.80,.48,true,false,false,2,7);
    objeto(f,1000,667,81,203,1050,690,.90,.17,.72,.66,.25,false,false,false,1,8);
    objeto(f,240,817,177,241,1200,750,.78,.15,.68,.70,.30,true,false,false,2,9);

    f->spawnScooby=(Ponto){mapaParaTelaX(705),mapaParaTelaY(840)};
    f->spawnMaria=(Ponto){mapaParaTelaX(1015),mapaParaTelaY(550)};
    waypoint(f,600,450);waypoint(f,800,450);waypoint(f,1050,500);waypoint(f,1100,720);waypoint(f,750,800);waypoint(f,450,800);
    spawnBola(f,400,450);spawnBola(f,650,500);spawnBola(f,1150,600);spawnBola(f,850,820);
}

static void sala(Fase* f)
{
    f->nome="Sala";f->caminhoFundo="mapa/sala.png";f->caminhoObjetos="mapa/sala_objetos.png";f->tipoSaida=SAIDA_ESCADA;

    /*
     * O rodape/papel de parede superior terminam aproximadamente em y=300
     * da imagem fonte. Antes, y=125 deixava toda a parede dentro do piso e
     * permitia Scooby/Maria caminharem sobre janela e parede.
     */
    f->areaJogavel=areaFonte(72,300,1290,675);

    /* Trigger alinhado com a abertura real da escada, nao deslocado à direita. */
    f->triggerSaida=areaFonte(1035,875,180,90);
    f->alvoEntradaSaida=(Ponto){mapaParaTelaX(1125),mapaParaTelaY(965)};

    objeto(f,52,19,188,494,90,285,.84,.05,.58,.90,.40,true,true,true,2,0);
    objeto(f,353,66,277,245,300,145,.93,.04,.55,.92,.43,true,true,true,2,1);
    objeto(f,723,71,451,397,690,120,.82,.04,.55,.92,.43,true,true,true,2,2);
    objeto(f,1213,23,202,363,1160,140,.90,.05,.57,.90,.41,true,true,true,2,3);
    objeto(f,258,469,372,213,410,450,1.00,.04,.45,.92,.53,true,true,true,2,4);
    objeto(f,695,500,244,184,500,690,.88,.04,.45,.92,.53,true,false,true,2,5);
    objeto(f,960,545,135,130,185,770,.82,.12,.54,.76,.43,true,false,false,2,6);
    objeto(f,1112,461,154,213,980,525,.90,.08,.50,.84,.48,true,true,true,2,7);
    objeto(f,1291,500,122,163,1165,575,.84,.10,.50,.80,.48,true,false,false,2,8);
    objeto(f,485,689,256,386,720,735,.60,.20,.72,.60,.25,true,false,false,2,9);

    /*
     * Caixa de brinquedos ao lado da mesa de centro: o desenho faz parte de
     * um recorte composto, mas sua base nao estava coberta pelo collider do
     * objeto principal. Este collider aparece no F1 e impede atravessamento.
     */
    colisorSomente(f,715,785,175,90,false);

    f->spawnScooby=(Ponto){mapaParaTelaX(420),mapaParaTelaY(770)};
    f->spawnMaria=(Ponto){mapaParaTelaX(800),mapaParaTelaY(500)};
    waypoint(f,500,420);waypoint(f,650,500);waypoint(f,900,500);waypoint(f,900,650);waypoint(f,750,780);waypoint(f,400,700);
    spawnBola(f,250,350);spawnBola(f,600,400);spawnBola(f,1200,550);spawnBola(f,950,760);
}

static void banheiro(Fase* f)
{
    f->nome="Banheiro";f->caminhoFundo="mapa/banheiro.png";f->caminhoObjetos="mapa/banheiro_objetos.png";f->tipoSaida=SAIDA_PORTA;
    f->areaJogavel=areaFonte(78,320,1285,655);
    f->triggerSaida=areaFonte(585,880,280,85);
    f->alvoEntradaSaida=(Ponto){mapaParaTelaX(725),mapaParaTelaY(965)};

    objeto(f,42,19,437,478,65,130,.88,.02,.06,.96,.91,true,true,false,2,0);
    objeto(f,539,92,167,359,1000,145,.92,.05,.50,.90,.48,true,true,true,2,1);
    objeto(f,809,36,174,431,590,300,.90,.04,.48,.92,.50,true,true,true,2,2);
    objeto(f,1063,58,279,193,1085,105,.84,.04,.48,.92,.50,true,true,true,2,3);
    objeto(f,1022,275,176,240,1020,500,.90,.05,.48,.90,.50,true,true,false,2,4);
    objeto(f,1230,300,198,200,1210,520,.78,.05,.46,.90,.52,true,true,false,2,5);
    objeto(f,42,501,397,410,90,590,.83,.03,.50,.94,.48,true,true,true,2,6);
    objeto(f,558,546,176,163,750,680,.76,.06,.40,.88,.58,true,false,false,2,7);
    objeto(f,805,551,133,154,900,770,.78,.08,.38,.84,.60,true,false,false,2,8);
    objeto(f,1000,567,192,160,1035,775,.76,.05,.38,.90,.60,true,false,false,2,9);

    f->spawnScooby=(Ponto){mapaParaTelaX(460),mapaParaTelaY(820)};
    f->spawnMaria=(Ponto){mapaParaTelaX(835),mapaParaTelaY(540)};
    waypoint(f,500,450);waypoint(f,750,450);waypoint(f,950,450);waypoint(f,1000,650);waypoint(f,850,700);waypoint(f,500,720);
    spawnBola(f,850,650);spawnBola(f,560,780);spawnBola(f,780,560);spawnBola(f,980,650);
}

static void quarto(Fase* f)
{
    f->nome="Quarto";f->caminhoFundo="mapa/quarto.png";f->caminhoObjetos="mapa/quarto_objetos.png";f->tipoSaida=SAIDA_ESCADA;

    /* A faixa de papel de parede superior fica fora da area de piso. */
    f->areaJogavel=areaFonte(82,245,1280,730);
    f->triggerSaida=areaFonte(1040,875,210,90);
    f->alvoEntradaSaida=(Ponto){mapaParaTelaX(1145),mapaParaTelaY(965)};

    /* Source rectangles medidos no atlas REAL mapa/quarto_objetos.png (100x75). */
    objeto(f, 0, 1,32,28, 105,150,11.3f,.04,.50,.92,.47,true,true,true,0,0);
    objeto(f,33, 7,20,20, 545,155,10.6f,.06,.48,.88,.48,true,true,true,0,1);
    objeto(f,55, 1,18,26, 910,145,10.8f,.06,.50,.88,.47,true,true,true,0,2);
    objeto(f,75, 6,10,20,1190,160,10.8f,.10,.55,.80,.40,true,true,true,0,3);
    objeto(f,87, 9,11,18,1190,470, 9.8f,.08,.48,.84,.48,true,true,true,0,4);
    objeto(f, 2,29,29,20, 465,555,10.2f,.05,.34,.90,.62,true,true,true,0,5);
    objeto(f,34,28,23,20, 120,600, 9.8f,.05,.46,.90,.50,true,true,true,0,6);
    objeto(f,60,32,20,16, 900,585,10.0f,.06,.38,.88,.58,true,true,true,0,7);
    objeto(f,82,30,17,17,1120,610, 9.6f,.08,.35,.84,.60,true,false,false,0,8);
    objeto(f, 3,50,13,10, 300,785,10.0f,.08,.34,.84,.60,true,false,false,0,9);
    objeto(f,44,52, 7, 9, 745,790, 9.5f,.08,.35,.84,.60,true,false,false,0,10);

    f->spawnScooby=(Ponto){mapaParaTelaX(500),mapaParaTelaY(760)};
    f->spawnMaria=(Ponto){mapaParaTelaX(900),mapaParaTelaY(470)};
    waypoint(f,450,430);waypoint(f,700,430);waypoint(f,1000,430);waypoint(f,1050,650);waypoint(f,750,800);waypoint(f,350,700);
    spawnBola(f,650,500);spawnBola(f,1000,550);spawnBola(f,500,650);spawnBola(f,1000,780);
}

void configurarFases(Fase fases[QTD_FASES])
{
    if(!fases)return;
    memset(fases,0,sizeof(Fase)*QTD_FASES);

    cozinha(&fases[0]);
    sala(&fases[1]);
    banheiro(&fases[2]);
    quarto(&fases[3]);

    for(int i=0;i<QTD_FASES;i++)
        reconstruirColisoesFase(&fases[i]);
}
