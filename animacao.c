#include "jogo.h"

typedef struct {
    int area;
    int minX,minY,maxX,maxY;
} ComponenteAlpha;

/*
 * Cada folha e 4x4. A divisao proporcional serve apenas para delimitar a
 * celula nominal; dentro dela selecionamos os componentes alpha pertencentes
 * ao personagem. Assim fragmentos isolados de uma celula vizinha nao entram
 * no source rectangle, sem alterar o anchor nem a escala entre frames.
 */
static SourceRect celulaNominal(const Animacao* a,int w,int h,int direcao,int frame)
{
    int linha=a->linhaDirecao[direcao];
    int x0=(frame*w)/QTD_FRAMES;
    int x1=((frame+1)*w)/QTD_FRAMES;
    int y0=(linha*h)/QTD_DIRECOES;
    int y1=((linha+1)*h)/QTD_DIRECOES;
    return (SourceRect){x0,y0,x1-x0,y1-y0};
}

static bool pixelVisivel(const ALLEGRO_LOCKED_REGION* lock,int x,int y)
{
    const unsigned char* row=(const unsigned char*)lock->data+y*lock->pitch;
    const unsigned char* px=row+x*4;
    return px[3]>=24;
}

static int distanciaBBoxes(const ComponenteAlpha* a,const ComponenteAlpha* b)
{
    int dx=0,dy=0;
    if(a->maxX<b->minX)dx=b->minX-a->maxX;
    else if(b->maxX<a->minX)dx=a->minX-b->maxX;
    if(a->maxY<b->minY)dy=b->minY-a->maxY;
    else if(b->maxY<a->minY)dy=a->minY-b->maxY;
    return dx>dy?dx:dy;
}

static SourceRect sourcePorComponentes(const Animacao* a,const ALLEGRO_LOCKED_REGION* lock,
                                        int w,int h,int direcao,int frame)
{
    SourceRect cel=celulaNominal(a,w,h,direcao,frame);
    if(!lock||cel.sw<16||cel.sh<16)return cel;

    int total=cel.sw*cel.sh;
    unsigned char* visit=(unsigned char*)calloc((size_t)total,1);
    int* fila=(int*)malloc(sizeof(int)*(size_t)total);
    if(!visit||!fila){free(visit);free(fila);return cel;}

    ComponenteAlpha comps[64];
    int qtd=0;
    int guarda=cel.sw<cel.sh?cel.sw/100:cel.sh/100;
    if(guarda<2)guarda=2;
    if(guarda>5)guarda=5;

    for(int ly=guarda;ly<cel.sh-guarda;ly++)
    for(int lx=guarda;lx<cel.sw-guarda;lx++)
    {
        int idx=ly*cel.sw+lx;
        if(visit[idx])continue;
        visit[idx]=1;
        if(!pixelVisivel(lock,cel.sx+lx,cel.sy+ly))continue;

        int ini=0,fim=0;
        fila[fim++]=idx;
        ComponenteAlpha c={0,lx,ly,lx,ly};

        while(ini<fim)
        {
            int p=fila[ini++];
            int px=p%cel.sw,py=p/cel.sw;
            c.area++;
            if(px<c.minX)c.minX=px;if(px>c.maxX)c.maxX=px;
            if(py<c.minY)c.minY=py;if(py>c.maxY)c.maxY=py;

            for(int oy=-1;oy<=1;oy++)for(int ox=-1;ox<=1;ox++)
            {
                if(!ox&&!oy)continue;
                int nx=px+ox,ny=py+oy;
                if(nx<guarda||ny<guarda||nx>=cel.sw-guarda||ny>=cel.sh-guarda)continue;
                int ni=ny*cel.sw+nx;
                if(visit[ni])continue;
                visit[ni]=1;
                if(pixelVisivel(lock,cel.sx+nx,cel.sy+ny))fila[fim++]=ni;
            }
        }

        if(c.area>=6&&qtd<(int)(sizeof(comps)/sizeof(comps[0])))comps[qtd++]=c;
    }

    free(visit);free(fila);
    if(qtd==0)return (SourceRect){cel.sx+guarda,cel.sy+guarda,cel.sw-guarda*2,cel.sh-guarda*2};

    int maior=0;
    for(int i=1;i<qtd;i++)if(comps[i].area>comps[maior].area)maior=i;
    ComponenteAlpha principal=comps[maior];
    int minX=principal.minX,minY=principal.minY,maxX=principal.maxX,maxY=principal.maxY;
    int limDist=(cel.sw>cel.sh?cel.sw:cel.sh)/8;
    int areaGrande=principal.area/2;
    int areaProxima=principal.area/45;
    if(areaProxima<20)areaProxima=20;

    for(int i=0;i<qtd;i++)
    {
        if(i==maior)continue;
        bool manter=comps[i].area>=areaGrande ||
                    (comps[i].area>=areaProxima&&distanciaBBoxes(&principal,&comps[i])<=limDist);
        if(!manter)continue;
        if(comps[i].minX<minX)minX=comps[i].minX;
        if(comps[i].minY<minY)minY=comps[i].minY;
        if(comps[i].maxX>maxX)maxX=comps[i].maxX;
        if(comps[i].maxY>maxY)maxY=comps[i].maxY;
    }

    /* Pequena folga para antialiasing; nunca reabre a borda da celula. */
    int pad=3;
    minX-=pad;minY-=pad;maxX+=pad;maxY+=pad;
    if(minX<guarda)minX=guarda;if(minY<guarda)minY=guarda;
    if(maxX>cel.sw-1-guarda)maxX=cel.sw-1-guarda;
    if(maxY>cel.sh-1-guarda)maxY=cel.sh-1-guarda;

    SourceRect r={cel.sx+minX,cel.sy+minY,maxX-minX+1,maxY-minY+1};
    if(r.sw<cel.sw/4||r.sh<cel.sh/4)
        return (SourceRect){cel.sx+guarda,cel.sy+guarda,cel.sw-guarda*2,cel.sh-guarda*2};
    return r;
}

static bool validarAnimacao(const Animacao* a,const char* caminho)
{
    if(!a||!a->imagem)return false;
    int w=al_get_bitmap_width(a->imagem),h=al_get_bitmap_height(a->imagem);
    if(w<QTD_FRAMES*16||h<QTD_DIRECOES*16)return false;

    for(int d=0;d<QTD_DIRECOES;d++)for(int f=0;f<QTD_FRAMES;f++)
    {
        SourceRect cel=celulaNominal(a,w,h,d,f),r=a->source[d][f];
        if(r.sw<=0||r.sh<=0||r.sx<cel.sx||r.sy<cel.sy||
           r.sx+r.sw>cel.sx+cel.sw||r.sy+r.sh>cel.sy+cel.sh||
           r.sx<0||r.sy<0||r.sx+r.sw>w||r.sy+r.sh>h)
        {
            printf("ERRO sourceRect %s dir=%d frame=%d [%d,%d,%d,%d] cel=[%d,%d,%d,%d]\n",
                   caminho,d,f,r.sx,r.sy,r.sw,r.sh,cel.sx,cel.sy,cel.sw,cel.sh);
            return false;
        }
    }
    return true;
}

bool carregarAnimacao(Animacao* a,const char* caminho,float tempoFrame,
                      float escalaVisual,float anchorX,float anchorY)
{
    if(!a)return false;
    memset(a,0,sizeof(*a));
    a->imagem=carregarBitmapFlexivel(caminho);
    if(!a->imagem)return false;

    int w=al_get_bitmap_width(a->imagem),h=al_get_bitmap_height(a->imagem);
    a->framesPorLinha=QTD_FRAMES;a->linhasDirecao=QTD_DIRECOES;
    a->frameW=w/QTD_FRAMES;a->frameH=h/QTD_DIRECOES;
    a->linhaDirecao[DIRECAO_DOWN]=0;a->linhaDirecao[DIRECAO_UP]=1;
    a->linhaDirecao[DIRECAO_LEFT]=2;a->linhaDirecao[DIRECAO_RIGHT]=3;

    ALLEGRO_LOCKED_REGION* lock=al_lock_bitmap(a->imagem,ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,ALLEGRO_LOCK_READONLY);
    for(int d=0;d<QTD_DIRECOES;d++)for(int f=0;f<QTD_FRAMES;f++)
    {
        SourceRect cel=celulaNominal(a,w,h,d,f);
        a->source[d][f]=lock?sourcePorComponentes(a,lock,w,h,d,f):
                              (SourceRect){cel.sx+2,cel.sy+2,cel.sw-4,cel.sh-4};
    }
    if(lock)al_unlock_bitmap(a->imagem);

    a->frameAtual=0;a->acumulador=0;a->tempoFrame=tempoFrame;
    a->escalaVisual=escalaVisual;a->anchorNormX=anchorX;a->anchorNormY=anchorY;a->ultimoFrameSom=-1;
    if(!validarAnimacao(a,caminho)){al_destroy_bitmap(a->imagem);a->imagem=NULL;return false;}

    printf("Sprite OK: %s folha=%dx%d recorte por componentes | RIGHT:",caminho,w,h);
    for(int f=0;f<QTD_FRAMES;f++){SourceRect r=a->source[DIRECAO_RIGHT][f];printf(" f%d=[%d,%d,%d,%d]",f,r.sx,r.sy,r.sw,r.sh);}printf("\n");
    return true;
}

void reiniciarAnimacao(Animacao* a){if(a){a->frameAtual=0;a->acumulador=0;a->ultimoFrameSom=-1;}}

void atualizarAnimacaoLoop(Animacao* a,float dt)
{
    if(!a||!a->imagem||a->tempoFrame<=0)return;
    a->acumulador+=dt;
    while(a->acumulador>=a->tempoFrame){a->acumulador-=a->tempoFrame;a->frameAtual=(a->frameAtual+1)%QTD_FRAMES;}
}

bool atualizarAnimacaoUmaVez(Animacao* a,float dt)
{
    if(!a||!a->imagem)return true;
    a->acumulador+=dt;
    if(a->acumulador>=a->tempoFrame){a->acumulador-=a->tempoFrame;a->frameAtual++;if(a->frameAtual>=QTD_FRAMES){reiniciarAnimacao(a);return true;}}
    return false;
}

void desenharAnimacao(const Animacao* a,Direcao direcao,float x,float y)
{
    if(!a||!a->imagem)return;
    int d=(int)direcao,f=a->frameAtual;
    if(d<0||d>=QTD_DIRECOES)d=DIRECAO_DOWN;if(f<0||f>=QTD_FRAMES)f=0;
    int w=al_get_bitmap_width(a->imagem),h=al_get_bitmap_height(a->imagem);
    SourceRect cel=celulaNominal(a,w,h,d,f),r=a->source[d][f];
    float s=a->escalaVisual;
    float origemX=x-cel.sw*s*a->anchorNormX,origemY=y-cel.sh*s*a->anchorNormY;
    float dx=origemX+(r.sx-cel.sx)*s,dy=origemY+(r.sy-cel.sy)*s;
    al_draw_scaled_bitmap(a->imagem,r.sx,r.sy,r.sw,r.sh,dx,dy,r.sw*s,r.sh*s,0);
}

bool carregarSprites(Scooby* s,Maria* m)
{
    if(!s||!m)return false;

    /* Retangulo preto autoritativo: tronco lateral, sem cabeca/cauda. */
    s->corpo.hitboxLargura=76.0f;
    s->corpo.hitboxAltura=26.0f;
    s->corpo.hitboxOffsetX=-13.0f;
    s->corpo.hitboxOffsetY=-53.0f;

    if(!carregarAnimacao(&s->idle,"ScoobySprites/idle.png",.20f,.37f,.50f,.90f))return false;
    if(!carregarAnimacao(&s->walk,"ScoobySprites/walk.png",.12f,.37f,.50f,.90f))return false;
    if(!carregarAnimacao(&s->run,"ScoobySprites/run.png",.085f,.37f,.50f,.90f))return false;
    if(!carregarAnimacao(&s->bark,"ScoobySprites/bark.png",.085f,.37f,.50f,.90f))return false;
    if(!carregarAnimacao(&s->bite,"ScoobySprites/bite.png",.08f,.37f,.50f,.90f))return false;

    static const char* carry[QTD_CORES_BOLA]={
        "ScoobySprites/littleBalls/yellow_dog.png","ScoobySprites/littleBalls/green_dog.png",
        "ScoobySprites/littleBalls/purple_dog.png","ScoobySprites/littleBalls/blue_dog.png",
        "ScoobySprites/littleBalls/red_dog.png"};
    for(int i=0;i<QTD_CORES_BOLA;i++)if(!carregarAnimacao(&s->carregar[i],carry[i],.14f,.37f,.50f,.90f))return false;

    if(!carregarAnimacao(&m->idle,"mariaSprites/idle.png",.20f,.31f,.50f,.93f))return false;
    if(!carregarAnimacao(&m->walk,"mariaSprites/walk.png",.13f,.31f,.50f,.93f))return false;
    if(!carregarAnimacao(&m->run,"mariaSprites/run.png",.095f,.31f,.50f,.93f))return false;
    if(!carregarAnimacao(&m->pick,"mariaSprites/pick.png",.11f,.31f,.50f,.93f))return false;
    return true;
}

void destruirAnimacaoInterna(Animacao* a){if(a&&a->imagem){al_destroy_bitmap(a->imagem);a->imagem=NULL;}}
