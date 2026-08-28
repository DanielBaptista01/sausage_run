#include "jogo.h"

/*
 * As folhas dos personagens sao organizadas em 4 colunas x 4 direcoes.
 * A divisao proporcional define somente a CELULA NOMINAL. Depois disso,
 * cada celula e analisada pelo canal alpha e o source rectangle e reduzido
 * para a faixa principal do personagem. Fragmentos pequenos e separados
 * pertencentes a uma celula vizinha deixam de fazer parte do source rect.
 *
 * Importante: o recorte de conteudo NAO altera a posicao/escala do frame.
 * desenharAnimacao() conserva a geometria da celula nominal e apenas desloca
 * o sub-retangulo recortado para a posicao correspondente dentro da celula.
 */
static SourceRect celulaNominal(const Animacao* a,
                                int larguraFolha,
                                int alturaFolha,
                                int direcao,
                                int frame)
{
    int linha=a->linhaDirecao[direcao];
    int x0=(frame*larguraFolha)/QTD_FRAMES;
    int x1=((frame+1)*larguraFolha)/QTD_FRAMES;
    int y0=(linha*alturaFolha)/QTD_DIRECOES;
    int y1=((linha+1)*alturaFolha)/QTD_DIRECOES;
    return (SourceRect){x0,y0,x1-x0,y1-y0};
}

static void melhorFaixaDensa(const int* contagem,int n,int pico,int* inicio,int* fim)
{
    if(!contagem||n<=0||!inicio||!fim){return;}

    int minimo=pico/18;
    if(minimo<2)minimo=2;

    long melhorScore=-1;
    int melhorIni=0,melhorFim=n-1;
    int i=0;

    while(i<n)
    {
        while(i<n&&contagem[i]<minimo)i++;
        if(i>=n)break;

        int ini=i;
        long score=0;
        while(i<n&&contagem[i]>=minimo)
        {
            score+=contagem[i];
            i++;
        }
        int end=i-1;

        if(score>melhorScore)
        {
            melhorScore=score;
            melhorIni=ini;
            melhorFim=end;
        }
    }

    if(melhorScore<0)
    {
        *inicio=0;
        *fim=n-1;
        return;
    }

    *inicio=melhorIni;
    *fim=melhorFim;
}

static SourceRect sourcePorConteudo(const Animacao* a,
                                    const ALLEGRO_LOCKED_REGION* lock,
                                    int larguraFolha,
                                    int alturaFolha,
                                    int direcao,
                                    int frame)
{
    SourceRect cel=celulaNominal(a,larguraFolha,alturaFolha,direcao,frame);
    if(!lock||cel.sw<=4||cel.sh<=4)return cel;

    int* linhas=(int*)calloc((size_t)cel.sh,sizeof(int));
    int* colunas=(int*)calloc((size_t)cel.sw,sizeof(int));
    if(!linhas||!colunas)
    {
        free(linhas);free(colunas);
        return cel;
    }

    int picoLinha=0,picoColuna=0;
    const int guarda=1;

    for(int yy=guarda;yy<cel.sh-guarda;yy++)
    {
        const unsigned char* row=(const unsigned char*)lock->data+(cel.sy+yy)*lock->pitch;
        for(int xx=guarda;xx<cel.sw-guarda;xx++)
        {
            /* ABGR_8888_LE possui ordem de memoria R,G,B,A. */
            const unsigned char* px=row+(cel.sx+xx)*4;
            if(px[3]<12)continue;
            linhas[yy]++;
            colunas[xx]++;
        }
        if(linhas[yy]>picoLinha)picoLinha=linhas[yy];
    }

    for(int xx=0;xx<cel.sw;xx++)
        if(colunas[xx]>picoColuna)picoColuna=colunas[xx];

    if(picoLinha<=0||picoColuna<=0)
    {
        free(linhas);free(colunas);
        return cel;
    }

    int cima=0,baixo=cel.sh-1,esq=0,dir=cel.sw-1;
    melhorFaixaDensa(linhas,cel.sh,picoLinha,&cima,&baixo);
    melhorFaixaDensa(colunas,cel.sw,picoColuna,&esq,&dir);

    /*
     * Recoloca uma folga interna suficiente para orelhas/patas/bola, mas
     * muito menor do que a distancia observada ate os fragmentos de outra
     * celula. Para frames ~313 px isso equivale a 17 px de source (~6 px na
     * escala atual), preservando extremidades sem reintroduzir vazamento.
     */
    int padX=cel.sw/18;
    int padY=cel.sh/18;
    if(padX<4)padX=4;
    if(padY<4)padY=4;

    esq-=padX;dir+=padX;cima-=padY;baixo+=padY;
    if(esq<guarda)esq=guarda;
    if(cima<guarda)cima=guarda;
    if(dir>cel.sw-1-guarda)dir=cel.sw-1-guarda;
    if(baixo>cel.sh-1-guarda)baixo=cel.sh-1-guarda;

    SourceRect r={cel.sx+esq,cel.sy+cima,dir-esq+1,baixo-cima+1};

    /* Se a deteccao ficou excessivamente pequena, mantemos a celula segura. */
    if(r.sw<cel.sw/3||r.sh<cel.sh/3)
        r=(SourceRect){cel.sx+guarda,cel.sy+guarda,cel.sw-guarda*2,cel.sh-guarda*2};

    free(linhas);free(colunas);
    return r;
}

static bool validarAnimacao(const Animacao* a,const char* caminho)
{
    if(!a||!a->imagem)return false;

    int w=al_get_bitmap_width(a->imagem);
    int h=al_get_bitmap_height(a->imagem);
    if(w<QTD_FRAMES*16||h<QTD_DIRECOES*16)
    {
        printf("ERRO sheet pequena: %s %dx%d\n",caminho,w,h);
        return false;
    }

    for(int d=0;d<QTD_DIRECOES;d++)
    for(int f=0;f<QTD_FRAMES;f++)
    {
        SourceRect cel=celulaNominal(a,w,h,d,f);
        SourceRect r=a->source[d][f];

        if(r.sw<=0||r.sh<=0||r.sx<cel.sx||r.sy<cel.sy||
           r.sx+r.sw>cel.sx+cel.sw||r.sy+r.sh>cel.sy+cel.sh||
           r.sx<0||r.sy<0||r.sx+r.sw>w||r.sy+r.sh>h)
        {
            printf("ERRO sourceRect %s dir=%d frame=%d [%d,%d,%d,%d] cel=[%d,%d,%d,%d] folha=%dx%d\n",
                   caminho,d,f,r.sx,r.sy,r.sw,r.sh,
                   cel.sx,cel.sy,cel.sw,cel.sh,w,h);
            return false;
        }
    }
    return true;
}

bool carregarAnimacao(Animacao* a,
                      const char* caminho,
                      float tempoFrame,
                      float escalaVisual,
                      float anchorX,
                      float anchorY)
{
    if(!a)return false;
    memset(a,0,sizeof(*a));

    a->imagem=carregarBitmapFlexivel(caminho);
    if(!a->imagem)return false;

    int w=al_get_bitmap_width(a->imagem);
    int h=al_get_bitmap_height(a->imagem);

    a->framesPorLinha=QTD_FRAMES;
    a->linhasDirecao=QTD_DIRECOES;
    a->frameW=w/QTD_FRAMES;
    a->frameH=h/QTD_DIRECOES;
    a->margemX=0;a->margemY=0;a->gapX=0;a->gapY=0;

    a->linhaDirecao[DIRECAO_DOWN]=0;
    a->linhaDirecao[DIRECAO_UP]=1;
    a->linhaDirecao[DIRECAO_LEFT]=2;
    a->linhaDirecao[DIRECAO_RIGHT]=3;

    ALLEGRO_LOCKED_REGION* lock=al_lock_bitmap(a->imagem,
                                               ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,
                                               ALLEGRO_LOCK_READONLY);

    for(int d=0;d<QTD_DIRECOES;d++)
    for(int f=0;f<QTD_FRAMES;f++)
    {
        if(lock)a->source[d][f]=sourcePorConteudo(a,lock,w,h,d,f);
        else
        {
            SourceRect cel=celulaNominal(a,w,h,d,f);
            a->source[d][f]=(SourceRect){cel.sx+1,cel.sy+1,cel.sw-2,cel.sh-2};
        }
    }

    if(lock)al_unlock_bitmap(a->imagem);
    else printf("WARN %s: bitmap nao pode ser bloqueado; usando guard band de 1 px.\n",caminho);

    a->frameAtual=0;
    a->acumulador=0;
    a->tempoFrame=tempoFrame;
    a->escalaVisual=escalaVisual;
    a->anchorNormX=anchorX;
    a->anchorNormY=anchorY;
    a->ultimoFrameSom=-1;

    if(!validarAnimacao(a,caminho))
    {
        al_destroy_bitmap(a->imagem);
        a->imagem=NULL;
        return false;
    }

    printf("Sprite OK: %s folha=%dx%d crop-alpha seguro 4x4 | RIGHT:",caminho,w,h);
    for(int f=0;f<QTD_FRAMES;f++)
    {
        SourceRect r=a->source[DIRECAO_RIGHT][f];
        printf(" f%d=[%d,%d,%d,%d]",f,r.sx,r.sy,r.sw,r.sh);
    }
    printf("\n");
    return true;
}

void reiniciarAnimacao(Animacao* a)
{
    if(!a)return;
    a->frameAtual=0;a->acumulador=0;a->ultimoFrameSom=-1;
}

void atualizarAnimacaoLoop(Animacao* a,float dt)
{
    if(!a||!a->imagem||a->tempoFrame<=0)return;
    a->acumulador+=dt;
    while(a->acumulador>=a->tempoFrame)
    {
        a->acumulador-=a->tempoFrame;
        a->frameAtual=(a->frameAtual+1)%QTD_FRAMES;
    }
}

bool atualizarAnimacaoUmaVez(Animacao* a,float dt)
{
    if(!a||!a->imagem)return true;
    a->acumulador+=dt;
    if(a->acumulador>=a->tempoFrame)
    {
        a->acumulador-=a->tempoFrame;
        a->frameAtual++;
        if(a->frameAtual>=QTD_FRAMES){reiniciarAnimacao(a);return true;}
    }
    return false;
}

void desenharAnimacao(const Animacao* a,Direcao direcao,float x,float y)
{
    if(!a||!a->imagem)return;

    int d=(int)direcao,f=a->frameAtual;
    if(d<0||d>=QTD_DIRECOES)d=DIRECAO_DOWN;
    if(f<0||f>=QTD_FRAMES)f=0;

    int w=al_get_bitmap_width(a->imagem);
    int h=al_get_bitmap_height(a->imagem);
    SourceRect cel=celulaNominal(a,w,h,d,f);
    SourceRect r=a->source[d][f];
    float s=a->escalaVisual;

    /*
     * O anchor permanece relativo a celula completa. Recortar transparencia
     * ou um fragmento vizinho nao desloca o personagem entre frames.
     */
    float origemX=x-cel.sw*s*a->anchorNormX;
    float origemY=y-cel.sh*s*a->anchorNormY;
    float dx=origemX+(r.sx-cel.sx)*s;
    float dy=origemY+(r.sy-cel.sy)*s;
    float dw=r.sw*s,dh=r.sh*s;

    al_draw_scaled_bitmap(a->imagem,r.sx,r.sy,r.sw,r.sh,dx,dy,dw,dh,0);
}

bool carregarSprites(Scooby* s,Maria* m)
{
    if(!s||!m)return false;

    /*
     * Calibracao autoritativa da captura com o retangulo preto.
     * A posicao logica dos sprites possui grande margem transparente abaixo;
     * por isso a caixa fisica precisa subir ate o tronco em vez de ficar
     * presa ao centro da celula da sheet. Medidas em pixels de tela.
     */
    s->corpo.hitboxLargura=62.0f;
    s->corpo.hitboxAltura=30.0f;
    s->corpo.hitboxOffsetX=-6.0f;
    s->corpo.hitboxOffsetY=-61.0f;

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
{
    if(a&&a->imagem){al_destroy_bitmap(a->imagem);a->imagem=NULL;}
}
