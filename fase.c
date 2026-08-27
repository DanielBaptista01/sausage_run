#include "jogo.h"

static void adicionarObjeto(Fase* fase, int sx, int sy, int sw, int sh,
                            float mapaX, float mapaY, float escala,
                            float colX, float colY, float colW, float colH,
                            bool colide, bool bloqueiaVisao)
{
    if (fase->quantidadeObjetos >= MAX_OBJETOS) return;
    ObjetoMapa* o = &fase->objetos[fase->quantidadeObjetos++];
    o->sx=sx; o->sy=sy; o->sw=sw; o->sh=sh; o->mapaX=mapaX; o->mapaY=mapaY; o->escala=escala;
    o->colX=colX; o->colY=colY; o->colW=colW; o->colH=colH; o->colide=colide; o->bloqueiaVisao=bloqueiaVisao;
}

static void montarColisoes(Fase* fase)
{
    fase->quantidadeObstaculos = 0;
    for (int i=0;i<fase->quantidadeObjetos;i++)
    {
        const ObjetoMapa* obj=&fase->objetos[i];
        if (!obj->colide || fase->quantidadeObstaculos>=MAX_OBSTACULOS) continue;
        float dx=mapaParaTelaX(obj->mapaX), dy=mapaParaTelaY(obj->mapaY);
        float dw=obj->sw*MAPA_ESCALA*obj->escala, dh=obj->sh*MAPA_ESCALA*obj->escala;
        Obstaculo* o=&fase->obstaculos[fase->quantidadeObstaculos++];
        o->x=dx+dw*obj->colX; o->y=dy+dh*obj->colY; o->largura=dw*obj->colW; o->altura=dh*obj->colH;
        o->bloqueiaMovimento=true; o->bloqueiaVisao=obj->bloqueiaVisao;
    }
}

void desenharObjeto(const Fase* fase,const ObjetoMapa* obj)
{
    float dx=mapaParaTelaX(obj->mapaX), dy=mapaParaTelaY(obj->mapaY);
    float dw=obj->sw*MAPA_ESCALA*obj->escala, dh=obj->sh*MAPA_ESCALA*obj->escala;
    if (fase->folhaObjetos)
        al_draw_scaled_bitmap(fase->folhaObjetos,obj->sx,obj->sy,obj->sw,obj->sh,dx,dy,dw,dh,0);
    else
    {
        al_draw_filled_rectangle(dx,dy,dx+dw,dy+dh,al_map_rgb(132,76,38));
        al_draw_rectangle(dx,dy,dx+dw,dy+dh,al_map_rgb(65,35,24),2.0f);
    }
}

float baseYObjeto(const ObjetoMapa* obj)
{
    return mapaParaTelaY(obj->mapaY)+obj->sh*MAPA_ESCALA*obj->escala*0.88f;
}

static void adicionarWaypoint(Fase* f,float x,float y)
{
    if(f->quantidadeWaypoints<MAX_WAYPOINTS){f->waypoints[f->quantidadeWaypoints].x=mapaParaTelaX(x);f->waypoints[f->quantidadeWaypoints].y=mapaParaTelaY(y);f->quantidadeWaypoints++;}
}
static void adicionarSpawnBola(Fase* f,float x,float y)
{
    if(f->quantidadeSpawnsBola<3){f->spawnsBola[f->quantidadeSpawnsBola].x=mapaParaTelaX(x);f->spawnsBola[f->quantidadeSpawnsBola].y=mapaParaTelaY(y);f->quantidadeSpawnsBola++;}
}

static void configurarCozinha(Fase* f)
{
    f->nome="Cozinha";
    adicionarObjeto(f,25,19,243,354,85,120,1.0f,.10f,.52f,.80f,.46f,true,true);
    adicionarObjeto(f,326,71,486,269,345,120,1.0f,.02f,.48f,.96f,.50f,true,true);
    adicionarObjeto(f,864,96,275,251,875,135,1.0f,.08f,.42f,.84f,.55f,true,true);
    adicionarObjeto(f,1164,14,234,363,1180,90,1.0f,.08f,.48f,.84f,.50f,true,true);
    adicionarObjeto(f,55,401,341,380,320,405,.95f,.08f,.28f,.84f,.68f,true,false);
    adicionarObjeto(f,838,396,160,385,860,400,.95f,.08f,.18f,.84f,.78f,true,true);
    adicionarObjeto(f,451,615,192,243,95,700,.95f,.08f,.42f,.84f,.54f,true,true);
    adicionarObjeto(f,700,708,105,124,1080,720,.9f,.12f,.42f,.76f,.52f,true,false);
    adicionarObjeto(f,975,667,106,203,1200,680,.9f,.20f,.50f,.60f,.45f,true,false);
    adicionarObjeto(f,240,817,177,241,1190,785,.9f,.15f,.58f,.70f,.38f,true,true);
    f->spawnScooby=(Ponto){mapaParaTelaX(665),mapaParaTelaY(930)};
    f->spawnMaria=(Ponto){mapaParaTelaX(1030),mapaParaTelaY(565)};
    f->saida=(Retangulo){mapaParaTelaX(1015),mapaParaTelaY(225),150*MAPA_ESCALA,125*MAPA_ESCALA};
    adicionarWaypoint(f,332,332); adicionarWaypoint(f,694,452); adicionarWaypoint(f,1056,452); adicionarWaypoint(f,1056,814); adicionarWaypoint(f,634,814); adicionarWaypoint(f,272,754);
    adicionarSpawnBola(f,450,450); adicionarSpawnBola(f,750,650); adicionarSpawnBola(f,1050,750);
}

static void configurarSala(Fase* f)
{
    f->nome="Sala";
    adicionarObjeto(f,52,19,188,494,55,265,.85f,.08f,.62f,.84f,.35f,true,true);
    adicionarObjeto(f,353,66,277,245,205,110,.95f,.06f,.48f,.88f,.48f,true,true);
    adicionarObjeto(f,723,71,451,397,735,75,.86f,.07f,.50f,.86f,.45f,true,true);
    adicionarObjeto(f,1213,23,202,363,1205,85,.95f,.08f,.50f,.84f,.48f,true,true);
    adicionarObjeto(f,258,469,372,213,400,435,1.05f,.04f,.34f,.92f,.62f,true,true);
    adicionarObjeto(f,695,500,244,184,470,675,.92f,.06f,.25f,.88f,.68f,true,false);
    adicionarObjeto(f,960,545,135,130,225,770,.9f,.10f,.35f,.80f,.58f,true,false);
    adicionarObjeto(f,1112,461,154,213,1025,560,.95f,.10f,.42f,.80f,.52f,true,true);
    adicionarObjeto(f,1291,500,122,163,1190,585,.9f,.10f,.46f,.80f,.48f,true,false);
    adicionarObjeto(f,485,689,256,386,610,690,.75f,.08f,.50f,.84f,.45f,true,true);
    f->spawnScooby=(Ponto){mapaParaTelaX(265),mapaParaTelaY(900)};
    f->spawnMaria=(Ponto){mapaParaTelaX(650),mapaParaTelaY(300)};
    f->saida=(Retangulo){mapaParaTelaX(1070),mapaParaTelaY(835),260*MAPA_ESCALA,225*MAPA_ESCALA};
    adicionarWaypoint(f,272,392); adicionarWaypoint(f,634,332); adicionarWaypoint(f,996,452); adicionarWaypoint(f,996,694); adicionarWaypoint(f,814,814); adicionarWaypoint(f,332,754);
    adicionarSpawnBola(f,820,545); adicionarSpawnBola(f,980,760); adicionarSpawnBola(f,320,690);
}

static void configurarBanheiro(Fase* f)
{
    f->nome="Banheiro";
    adicionarObjeto(f,42,19,437,478,45,85,.90f,.04f,.18f,.92f,.78f,true,true);
    adicionarObjeto(f,539,92,167,359,950,115,.95f,.12f,.56f,.76f,.40f,true,true);
    adicionarObjeto(f,809,36,174,431,595,290,.92f,.08f,.42f,.84f,.55f,true,true);
    adicionarObjeto(f,1063,58,279,193,1090,60,.88f,.06f,.45f,.88f,.50f,true,true);
    adicionarObjeto(f,1022,275,176,240,1060,455,.95f,.08f,.40f,.84f,.55f,true,true);
    adicionarObjeto(f,1230,300,198,200,1215,500,.88f,.08f,.38f,.84f,.57f,true,true);
    adicionarObjeto(f,42,501,397,410,80,570,.88f,.05f,.54f,.90f,.42f,true,true);
    adicionarObjeto(f,558,546,176,163,735,650,.82f,.08f,.38f,.84f,.56f,true,false);
    adicionarObjeto(f,805,551,133,154,900,760,.82f,.10f,.40f,.80f,.55f,true,false);
    adicionarObjeto(f,1000,567,192,160,1040,760,.82f,.08f,.40f,.84f,.55f,true,false);
    f->spawnScooby=(Ponto){mapaParaTelaX(720),mapaParaTelaY(930)};
    f->spawnMaria=(Ponto){mapaParaTelaX(820),mapaParaTelaY(390)};
    f->saida=(Retangulo){mapaParaTelaX(570),mapaParaTelaY(900),310*MAPA_ESCALA,180*MAPA_ESCALA};
    adicionarWaypoint(f,452,392); adicionarWaypoint(f,814,392); adicionarWaypoint(f,1116,392); adicionarWaypoint(f,1056,754); adicionarWaypoint(f,754,814); adicionarWaypoint(f,452,754);
    adicionarSpawnBola(f,900,650); adicionarSpawnBola(f,520,790); adicionarSpawnBola(f,830,570);
}

static void configurarQuarto(Fase* f)
{
    f->nome="Quarto";
    adicionarObjeto(f,0,1,32,28,70,80,12.9f,.04f,.50f,.92f,.47f,true,true);
    adicionarObjeto(f,33,7,20,20,520,115,13.1f,.06f,.48f,.88f,.48f,true,true);
    adicionarObjeto(f,55,1,18,26,900,80,13.3f,.06f,.50f,.88f,.47f,true,true);
    adicionarObjeto(f,75,6,10,20,1160,115,14.0f,.10f,.55f,.80f,.40f,true,true);
    adicionarObjeto(f,87,9,11,18,1230,385,12.2f,.08f,.48f,.84f,.48f,true,true);
    adicionarObjeto(f,2,29,29,20,455,470,12.8f,.05f,.26f,.90f,.68f,true,false);
    adicionarObjeto(f,34,28,23,20,70,525,12.5f,.05f,.46f,.90f,.50f,true,true);
    adicionarObjeto(f,60,32,20,16,930,525,12.2f,.06f,.38f,.88f,.58f,true,true);
    adicionarObjeto(f,82,30,17,17,1150,590,12.4f,.08f,.35f,.84f,.60f,true,false);
    adicionarObjeto(f,3,50,13,10,300,760,12.5f,.08f,.34f,.84f,.60f,true,false);
    adicionarObjeto(f,44,52,7,9,690,770,12.0f,.08f,.35f,.84f,.60f,true,false);
    f->spawnScooby=(Ponto){mapaParaTelaX(260),mapaParaTelaY(900)};
    f->spawnMaria=(Ponto){mapaParaTelaX(700),mapaParaTelaY(430)};
    f->saida=(Retangulo){mapaParaTelaX(1000),mapaParaTelaY(850),270*MAPA_ESCALA,220*MAPA_ESCALA};
    adicionarWaypoint(f,332,513); adicionarWaypoint(f,754,452); adicionarWaypoint(f,1056,452); adicionarWaypoint(f,1056,754); adicionarWaypoint(f,754,935); adicionarWaypoint(f,272,814);
    adicionarSpawnBola(f,850,680); adicionarSpawnBola(f,1050,790); adicionarSpawnBola(f,500,820);
}

void configurarFases(Fase fases[QTD_FASES],RecursosMapa* recursos)
{
    for(int i=0;i<QTD_FASES;i++){fases[i].fundo=recursos->fundos[i];fases[i].folhaObjetos=recursos->folhasObjetos[i];fases[i].quantidadeObjetos=0;fases[i].quantidadeObstaculos=0;fases[i].quantidadeWaypoints=0;fases[i].quantidadeSpawnsBola=0;}
    configurarCozinha(&fases[0]); configurarSala(&fases[1]); configurarBanheiro(&fases[2]); configurarQuarto(&fases[3]);
    for(int i=0;i<QTD_FASES;i++) montarColisoes(&fases[i]);
}
