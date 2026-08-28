#include "jogo.h"

/*
 * As sheets dos personagens foram geradas como uma grade 4x4. Algumas delas
 * possuem, por exemplo, 1254x1254 pixels. O resto de 2 pixels NAO representa
 * meia celula maior: ele e uma borda externa da folha.
 *
 * Distribuir esse resto entre as celulas (313/314/313/314) deslocava as linhas
 * a partir da segunda celula e fazia aparecer cabeca/pata/cabelo da linha de
 * cima ou de baixo. A grade correta e:
 *
 *   margem externa + 4 celulas inteiras + margem externa restante
 *
 * Exemplo 1254: margem=1, frame=313, margem final=1.
 */
static void configurarGrade(Animacao* a, int w, int h)
{
    int restoX = w % QTD_FRAMES;
    int restoY = h % QTD_DIRECOES;

    a->margemX = restoX / 2;
    a->margemY = restoY / 2;
    a->frameW = (w - restoX) / QTD_FRAMES;
    a->frameH = (h - restoY) / QTD_DIRECOES;
    a->gapX = 0;
    a->gapY = 0;

    a->linhaDirecao[DIRECAO_DOWN]  = 0;
    a->linhaDirecao[DIRECAO_UP]    = 1;
    a->linhaDirecao[DIRECAO_LEFT]  = 2;
    a->linhaDirecao[DIRECAO_RIGHT] = 3;
}

static SourceRect sourceGrade(const Animacao* a, int direcao, int frame)
{
    int linha = a->linhaDirecao[direcao];
    int sx = a->margemX + frame * (a->frameW + a->gapX);
    int sy = a->margemY + linha * (a->frameH + a->gapY);
    return (SourceRect){sx, sy, a->frameW, a->frameH};
}

static bool validarAnimacao(const Animacao* a, const char* caminho)
{
    if (!a || !a->imagem) return false;

    int w = al_get_bitmap_width(a->imagem);
    int h = al_get_bitmap_height(a->imagem);

    if (a->frameW < 16 || a->frameH < 16)
    {
        printf("ERRO sheet pequena: %s %dx%d\n", caminho, w, h);
        return false;
    }

    for (int d = 0; d < QTD_DIRECOES; d++)
    for (int f = 0; f < QTD_FRAMES; f++)
    {
        SourceRect r = a->source[d][f];
        if (r.sw <= 0 || r.sh <= 0 || r.sx < 0 || r.sy < 0 ||
            r.sx + r.sw > w || r.sy + r.sh > h)
        {
            printf("ERRO sourceRect %s dir=%d frame=%d [%d,%d,%d,%d] folha=%dx%d\n",
                   caminho,d,f,r.sx,r.sy,r.sw,r.sh,w,h);
            return false;
        }

        if (f > 0)
        {
            SourceRect ant = a->source[d][f-1];
            if (ant.sx + ant.sw > r.sx)
            {
                printf("ERRO sobreposicao horizontal em %s dir=%d frame=%d\n", caminho,d,f);
                return false;
            }
        }

        if (d > 0)
        {
            SourceRect cima = a->source[d-1][f];
            if (cima.sy + cima.sh > r.sy)
            {
                printf("ERRO sobreposicao vertical em %s dir=%d frame=%d\n", caminho,d,f);
                return false;
            }
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
    if (!a) return false;
    memset(a, 0, sizeof(*a));

    a->imagem = carregarBitmapFlexivel(caminho);
    if (!a->imagem) return false;

    int w = al_get_bitmap_width(a->imagem);
    int h = al_get_bitmap_height(a->imagem);

    a->framesPorLinha = QTD_FRAMES;
    a->linhasDirecao = QTD_DIRECOES;
    configurarGrade(a,w,h);

    for (int d=0; d<QTD_DIRECOES; d++)
        for (int f=0; f<QTD_FRAMES; f++)
            a->source[d][f] = sourceGrade(a,d,f);

    a->frameAtual = 0;
    a->acumulador = 0;
    a->tempoFrame = tempoFrame;
    a->escalaVisual = escalaVisual;
    a->anchorNormX = anchorX;
    a->anchorNormY = anchorY;
    a->ultimoFrameSom = -1;

    if (!validarAnimacao(a,caminho))
    {
        al_destroy_bitmap(a->imagem);
        a->imagem=NULL;
        return false;
    }

    printf("Sprite OK: %s folha=%dx%d frame=%dx%d margem=(%d,%d) | RIGHT:",
           caminho,w,h,a->frameW,a->frameH,a->margemX,a->margemY);
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

    SourceRect r=a->source[d][f];
    float dw=r.sw*a->escalaVisual,dh=r.sh*a->escalaVisual;
    float dx=x-dw*a->anchorNormX;
    float dy=y-dh*a->anchorNormY;

    al_draw_scaled_bitmap(a->imagem,r.sx,r.sy,r.sw,r.sh,dx,dy,dw,dh,0);
}

bool carregarSprites(Scooby* s,Maria* m)
{
    if(!s||!m)return false;

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
