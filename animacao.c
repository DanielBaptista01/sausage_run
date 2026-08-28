#include "jogo.h"

typedef struct {
    int area;
    int minX,minY,maxX,maxY;
} ComponenteAlpha;

/*
 * As folhas sao 4x4, mas alguns assets gerados possuem personagens que
 * ultrapassam visualmente a fronteira nominal entre linhas. A celula nominal
 * continua sendo a referencia de posicionamento/anchor; o source rectangle
 * pode ser refinado por componentes alpha e, no perfil carry RIGHT, pode
 * atravessar somente a borda superior da propria linha para recuperar a
 * cabeca que existe fisicamente acima dela no PNG.
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

/*
 * Recorte normal por componentes. No perfil estrito (usado somente no WALK
 * da Maria), componentes desconectados a mais de 8 px do corpo principal sao
 * rejeitados. Isso remove especificamente a cabeca da linha seguinte que
 * invade fisicamente a celula atual da sheet, sem alterar escala ou anchor.
 */
static SourceRect sourcePorComponentes(const Animacao* a,const ALLEGRO_LOCKED_REGION* lock,
                                        int w,int h,int direcao,int frame,bool estrito)
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
    int limDist=estrito?8:(cel.sw>cel.sh?cel.sw:cel.sh)/8;
    int areaGrande=principal.area/2;
    int areaProxima=principal.area/(estrito?80:45);
    if(areaProxima<(estrito?12:20))areaProxima=estrito?12:20;

    for(int i=0;i<qtd;i++)
    {
        if(i==maior)continue;
        int dist=distanciaBBoxes(&principal,&comps[i]);
        bool manter;
        if(estrito)
            manter=comps[i].area>=areaProxima&&dist<=limDist;
        else
            manter=comps[i].area>=areaGrande ||
                   (comps[i].area>=areaProxima&&dist<=limDist);
        if(!manter)continue;
        if(comps[i].minX<minX)minX=comps[i].minX;
        if(comps[i].minY<minY)minY=comps[i].minY;
        if(comps[i].maxX>maxX)maxX=comps[i].maxX;
        if(comps[i].maxY>maxY)maxY=comps[i].maxY;
    }

    int pad=estrito?2:3;
    minX-=pad;minY-=pad;maxX+=pad;maxY+=pad;
    if(minX<guarda)minX=guarda;if(minY<guarda)minY=guarda;
    if(maxX>cel.sw-1-guarda)maxX=cel.sw-1-guarda;
    if(maxY>cel.sh-1-guarda)maxY=cel.sh-1-guarda;

    SourceRect r={cel.sx+minX,cel.sy+minY,maxX-minX+1,maxY-minY+1};
    if(r.sw<cel.sw/4||r.sh<cel.sh/4)
        return (SourceRect){cel.sx+guarda,cel.sy+guarda,cel.sw-guarda*2,cel.sh-guarda*2};
    return r;
}

/*
 * Carry RIGHT: nas cinco sheets a quarta linha comeca visualmente antes da
 * fronteira matematica de 3/4 da imagem. Ex.: em uma folha 1254x1254 a linha
 * nominal inicia em y=940, mas a cabeca do cachorro ja existe por volta de
 * y=901. Um crop limitado a y>=940 necessariamente corta testa/orelhas.
 *
 * Procuramos apenas dentro da coluna do frame e numa janela vertical que sobe
 * 1/3 de celula. O maior componente alpha dessa janela e o cachorro completo;
 * fragmentos da linha anterior permanecem componentes menores e nao entram no
 * retangulo final. A direcao LEFT nao passa por esta funcao.
 */
static SourceRect sourceCarryRight(const Animacao* a,const ALLEGRO_LOCKED_REGION* lock,
                                   int w,int h,int frame)
{
    SourceRect cel=celulaNominal(a,w,h,DIRECAO_RIGHT,frame);
    if(!lock)return cel;

    int x0=cel.sx,x1=cel.sx+cel.sw;
    int y0=cel.sy-cel.sh/3;
    int y1=cel.sy+cel.sh;
    if(y0<0)y0=0;if(y1>h)y1=h;
    int rw=x1-x0,rh=y1-y0;
    if(rw<=0||rh<=0)return cel;

    int total=rw*rh;
    unsigned char* visit=(unsigned char*)calloc((size_t)total,1);
    int* fila=(int*)malloc(sizeof(int)*(size_t)total);
    if(!visit||!fila){free(visit);free(fila);return cel;}

    ComponenteAlpha melhor={0,0,0,0,0};
    for(int ly=0;ly<rh;ly++)
    for(int lx=0;lx<rw;lx++)
    {
        int idx=ly*rw+lx;
        if(visit[idx])continue;
        visit[idx]=1;
        if(!pixelVisivel(lock,x0+lx,y0+ly))continue;

        int ini=0,fim=0;
        fila[fim++]=idx;
        ComponenteAlpha c={0,lx,ly,lx,ly};
        while(ini<fim)
        {
            int p=fila[ini++];int px=p%rw,py=p/rw;
            c.area++;
            if(px<c.minX)c.minX=px;if(px>c.maxX)c.maxX=px;
            if(py<c.minY)c.minY=py;if(py>c.maxY)c.maxY=py;
            for(int oy=-1;oy<=1;oy++)for(int ox=-1;ox<=1;ox++)
            {
                if(!ox&&!oy)continue;
                int nx=px+ox,ny=py+oy;
                if(nx<0||ny<0||nx>=rw||ny>=rh)continue;
                int ni=ny*rw+nx;
                if(visit[ni])continue;
                visit[ni]=1;
                if(pixelVisivel(lock,x0+nx,y0+ny))fila[fim++]=ni;
            }
        }
        if(c.area>melhor.area)melhor=c;
    }

    free(visit);free(fila);
    if(melhor.area<100)return cel;

    int pad=4;
    int minX=melhor.minX-pad,minY=melhor.minY-pad;
    int maxX=melhor.maxX+pad,maxY=melhor.maxY+pad;
    if(minX<0)minX=0;if(minY<0)minY=0;
    if(maxX>=rw)maxX=rw-1;if(maxY>=rh)maxY=rh-1;
    return (SourceRect){x0+minX,y0+minY,maxX-minX+1,maxY-minY+1};
}

static bool validarAnimacao(const Animacao* a,const char* caminho,bool carry)
{
    if(!a||!a->imagem)return false;
    int w=al_get_bitmap_width(a->imagem),h=al_get_bitmap_height(a->imagem);
    if(w<QTD_FRAMES*16||h<QTD_DIRECOES*16)return false;
    for(int d=0;d<QTD_DIRECOES;d++)for(int f=0;f<QTD_FRAMES;f++)
    {
        SourceRect cel=celulaNominal(a,w,h,d,f),r=a->source[d][f];
        bool dentroImagem=r.sw>0&&r.sh>0&&r.sx>=0&&r.sy>=0&&r.sx+r.sw<=w&&r.sy+r.sh<=h;
        bool dentroColuna=r.sx>=cel.sx&&r.sx+r.sw<=cel.sx+cel.sw;
        bool dentroCelulaY=r.sy>=cel.sy&&r.sy+r.sh<=cel.sy+cel.sh;
        bool carryRightValido=carry&&d==DIRECAO_RIGHT&&dentroColuna&&
            r.sy>=cel.sy-cel.sh/2&&r.sy+r.sh<=cel.sy+cel.sh;
        if(!dentroImagem||!dentroColuna||(!dentroCelulaY&&!carryRightValido))
        {
            printf("ERRO sourceRect %s dir=%d frame=%d [%d,%d,%d,%d] cel=[%d,%d,%d,%d]\n",
                   caminho,d,f,r.sx,r.sy,r.sw,r.sh,cel.sx,cel.sy,cel.sw,cel.sh);
            return false;
        }
    }
    return true;
}

bool carregarAnimacao(Animacao* a,const char* caminho,float tempoFrame,float escalaVisual,float anchorX,float anchorY)
{
    if(!a)return false;
    memset(a,0,sizeof(*a));
    a->imagem=carregarBitmapFlexivel(caminho);
    if(!a->imagem)return false;

    int w=al_get_bitmap_width(a->imagem),h=al_get_bitmap_height(a->imagem);
    bool mariaWalk=strcmp(caminho,"mariaSprites/walk.png")==0;
    bool carry=strstr(caminho,"ScoobySprites/littleBalls/")!=NULL;

    a->framesPorLinha=QTD_FRAMES;a->linhasDirecao=QTD_DIRECOES;
    a->frameW=w/QTD_FRAMES;a->frameH=h/QTD_DIRECOES;
    a->linhaDirecao[DIRECAO_DOWN]=0;a->linhaDirecao[DIRECAO_UP]=1;
    a->linhaDirecao[DIRECAO_LEFT]=2;a->linhaDirecao[DIRECAO_RIGHT]=3;

    ALLEGRO_LOCKED_REGION* lock=al_lock_bitmap(a->imagem,ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,ALLEGRO_LOCK_READONLY);
    for(int d=0;d<QTD_DIRECOES;d++)for(int f=0;f<QTD_FRAMES;f++)
    {
        SourceRect cel=celulaNominal(a,w,h,d,f);
        a->source[d][f]=lock?sourcePorComponentes(a,lock,w,h,d,f,mariaWalk):
            (SourceRect){cel.sx+2,cel.sy+2,cel.sw-4,cel.sh-4};
    }
    if(lock&&carry)
        for(int f=0;f<QTD_FRAMES;f++)a->source[DIRECAO_RIGHT][f]=sourceCarryRight(a,lock,w,h,f);
    if(lock)al_unlock_bitmap(a->imagem);

    a->frameAtual=0;a->acumulador=0;a->tempoFrame=tempoFrame;
    a->escalaVisual=escalaVisual;a->anchorNormX=anchorX;a->anchorNormY=anchorY;a->ultimoFrameSom=-1;

    if(!validarAnimacao(a,caminho,carry))
    {al_destroy_bitmap(a->imagem);a->imagem=NULL;return false;}

    printf("Sprite OK: %s folha=%dx%d perfil=%s | RIGHT:",caminho,w,h,
           carry?"carry-right-expandido":(mariaWalk?"maria-walk-estrito":"componentes"));
    for(int f=0;f<QTD_FRAMES;f++)
    {SourceRect r=a->source[DIRECAO_RIGHT][f];printf(" f%d=[%d,%d,%d,%d]",f,r.sx,r.sy,r.sw,r.sh);}
    printf("\n");
    return true;
}

void reiniciarAnimacao(Animacao* a)
{if(a){a->frameAtual=0;a->acumulador=0;a->ultimoFrameSom=-1;}}

void atualizarAnimacaoLoop(Animacao* a,float dt)
{if(!a||!a->imagem||a->tempoFrame<=0)return;a->acumulador+=dt;while(a->acumulador>=a->tempoFrame){a->acumulador-=a->tempoFrame;a->frameAtual=(a->frameAtual+1)%QTD_FRAMES;}}

bool atualizarAnimacaoUmaVez(Animacao* a,float dt)
{if(!a||!a->imagem)return true;a->acumulador+=dt;if(a->acumulador>=a->tempoFrame){a->acumulador-=a->tempoFrame;a->frameAtual++;if(a->frameAtual>=QTD_FRAMES){reiniciarAnimacao(a);return true;}}return false;}

void desenharAnimacao(const Animacao* a,Direcao direcao,float x,float y)
{
    if(!a||!a->imagem)return;
    int d=(int)direcao,f=a->frameAtual;
    if(d<0||d>=QTD_DIRECOES)d=DIRECAO_DOWN;if(f<0||f>=QTD_FRAMES)f=0;
    int w=al_get_bitmap_width(a->imagem),h=al_get_bitmap_height(a->imagem);
    SourceRect cel=celulaNominal(a,w,h,d,f),r=a->source[d][f];float s=a->escalaVisual;
    float origemX=x-cel.sw*s*a->anchorNormX,origemY=y-cel.sh*s*a->anchorNormY;
    float dx=origemX+(r.sx-cel.sx)*s,dy=origemY+(r.sy-cel.sy)*s;
    al_draw_scaled_bitmap(a->imagem,r.sx,r.sy,r.sw,r.sh,dx,dy,r.sw*s,r.sh*s,0);
}

bool carregarSprites(Scooby* s,Maria* m)
{
    if(!s||!m)return false;

    /* Compatibilidade para validacoes genericas. O gameplay usa a hitbox
       composta de obterHitboxScooby(). */
    s->corpo.hitboxLargura=42.0f;s->corpo.hitboxAltura=20.0f;
    s->corpo.hitboxOffsetX=0.0f;s->corpo.hitboxOffsetY=-53.0f;

    if(!carregarAnimacao(&s->idle,"ScoobySprites/idle.png",.20f,.37f,.50f,.90f))return false;
    if(!carregarAnimacao(&s->walk,"ScoobySprites/walk.png",.12f,.37f,.50f,.90f))return false;
    if(!carregarAnimacao(&s->run,"ScoobySprites/run.png",.085f,.37f,.50f,.90f))return false;
    if(!carregarAnimacao(&s->bark,"ScoobySprites/bark.png",.085f,.37f,.50f,.90f))return false;
    if(!carregarAnimacao(&s->bite,"ScoobySprites/bite.png",.08f,.37f,.50f,.90f))return false;
    static const char* carry[QTD_CORES_BOLA]={
        "ScoobySprites/littleBalls/yellow_dog.png",
        "ScoobySprites/littleBalls/green_dog.png",
        "ScoobySprites/littleBalls/purple_dog.png",
        "ScoobySprites/littleBalls/blue_dog.png",
        "ScoobySprites/littleBalls/red_dog.png"
    };
    for(int i=0;i<QTD_CORES_BOLA;i++)
        if(!carregarAnimacao(&s->carregar[i],carry[i],.14f,.37f,.50f,.90f))return false;

    if(!carregarAnimacao(&m->idle,"mariaSprites/idle.png",.20f,.31f,.50f,.93f))return false;
    if(!carregarAnimacao(&m->walk,"mariaSprites/walk.png",.13f,.31f,.50f,.93f))return false;
    if(!carregarAnimacao(&m->run,"mariaSprites/run.png",.095f,.31f,.50f,.93f))return false;
    if(!carregarAnimacao(&m->pick,"mariaSprites/pick.png",.11f,.31f,.50f,.93f))return false;
    return true;
}

void destruirAnimacaoInterna(Animacao* a)
{if(a&&a->imagem){al_destroy_bitmap(a->imagem);a->imagem=NULL;}}
