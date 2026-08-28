#include "jogo.h"

static bool pixelOpaco(const ALLEGRO_LOCKED_REGION* lock, int x, int y)
{
    const unsigned char* p =
        (const unsigned char*)lock->data + y * lock->pitch + x * 4;
    return p[3] > 20;
}

/*
 * Retorna o bounding box da UNIAO dos componentes significativos da celula.
 *
 * A versao anterior mantinha somente o maior componente conectado. Isso
 * funcionava para remover pequenos pixels vindos da celula vizinha, mas era
 * incorreto para sprites legitimamente compostas por partes desconectadas.
 * Nas folhas carry, especialmente DIRECAO_RIGHT, cabeca/orelha/bola podem
 * formar componentes separados e a parte superior acabava descartada.
 *
 * Agora:
 * 1) medimos todos os componentes alfa da celula;
 * 2) descobrimos a area do maior;
 * 3) preservamos TODOS os componentes relevantes;
 * 4) descartamos apenas fragmentos muito pequenos (bleed de frame vizinho).
 */
static SourceRect componentesSignificativos(const ALLEGRO_LOCKED_REGION* lock,
                                             int cellX, int cellY,
                                             int cellW, int cellH)
{
    SourceRect fallback = {cellX + 1, cellY + 1, cellW - 2, cellH - 2};
    const int total = cellW * cellH;

    unsigned char* visitado = (unsigned char*)calloc((size_t)total, 1);
    int* fila = (int*)malloc((size_t)total * sizeof(int));
    int* areas = (int*)malloc((size_t)total * sizeof(int));
    int* minXs = (int*)malloc((size_t)total * sizeof(int));
    int* minYs = (int*)malloc((size_t)total * sizeof(int));
    int* maxXs = (int*)malloc((size_t)total * sizeof(int));
    int* maxYs = (int*)malloc((size_t)total * sizeof(int));

    if (!visitado || !fila || !areas || !minXs || !minYs || !maxXs || !maxYs)
    {
        free(visitado); free(fila); free(areas);
        free(minXs); free(minYs); free(maxXs); free(maxYs);
        return fallback;
    }

    const int dx[8] = {1,-1,0,0,1,1,-1,-1};
    const int dy[8] = {0,0,1,-1,1,-1,1,-1};

    int qtdComponentes = 0;
    int maiorArea = 0;

    for (int ly = 0; ly < cellH; ly++)
    for (int lx = 0; lx < cellW; lx++)
    {
        int idx = ly * cellW + lx;

        if (visitado[idx] || !pixelOpaco(lock, cellX + lx, cellY + ly))
            continue;

        int ini = 0, fim = 0;
        fila[fim++] = idx;
        visitado[idx] = 1;

        int area = 0;
        int minX = lx, minY = ly, maxX = lx, maxY = ly;

        while (ini < fim)
        {
            int atual = fila[ini++];
            int x = atual % cellW;
            int y = atual / cellW;
            area++;

            if (x < minX) minX = x;
            if (x > maxX) maxX = x;
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;

            for (int k = 0; k < 8; k++)
            {
                int nx = x + dx[k];
                int ny = y + dy[k];

                if (nx < 0 || nx >= cellW || ny < 0 || ny >= cellH)
                    continue;

                int ni = ny * cellW + nx;

                if (visitado[ni] || !pixelOpaco(lock, cellX + nx, cellY + ny))
                    continue;

                visitado[ni] = 1;
                fila[fim++] = ni;
            }
        }

        if (qtdComponentes < total)
        {
            areas[qtdComponentes] = area;
            minXs[qtdComponentes] = minX;
            minYs[qtdComponentes] = minY;
            maxXs[qtdComponentes] = maxX;
            maxYs[qtdComponentes] = maxY;
            qtdComponentes++;
        }

        if (area > maiorArea)
            maiorArea = area;
    }

    if (maiorArea < 20 || qtdComponentes <= 0)
    {
        free(visitado); free(fila); free(areas);
        free(minXs); free(minYs); free(maxXs); free(maxYs);
        return fallback;
    }

    /*
     * Um componente legitimo precisa ter pelo menos 2% da area do maior,
     * com piso minimo de 18 pixels. Isso preserva bola/orelha/cabeca e
     * rejeita pontinhos isolados de sheets vizinhas.
     */
    int limiteArea = (int)(maiorArea * 0.02f);
    if (limiteArea < 18) limiteArea = 18;

    int unionMinX = cellW, unionMinY = cellH;
    int unionMaxX = -1, unionMaxY = -1;
    int mantidos = 0;

    for (int i = 0; i < qtdComponentes; i++)
    {
        if (areas[i] < limiteArea)
            continue;

        if (minXs[i] < unionMinX) unionMinX = minXs[i];
        if (minYs[i] < unionMinY) unionMinY = minYs[i];
        if (maxXs[i] > unionMaxX) unionMaxX = maxXs[i];
        if (maxYs[i] > unionMaxY) unionMaxY = maxYs[i];
        mantidos++;
    }

    free(visitado); free(fila); free(areas);
    free(minXs); free(minYs); free(maxXs); free(maxYs);

    if (mantidos == 0 || unionMaxX < unionMinX || unionMaxY < unionMinY)
        return fallback;

    const int margem = 2;
    int x0 = unionMinX - margem;
    int y0 = unionMinY - margem;
    int x1 = unionMaxX + margem;
    int y1 = unionMaxY + margem;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > cellW - 1) x1 = cellW - 1;
    if (y1 > cellH - 1) y1 = cellH - 1;

    return (SourceRect){
        cellX + x0,
        cellY + y0,
        x1 - x0 + 1,
        y1 - y0 + 1
    };
}

static bool validarAnimacao(const Animacao* a, const char* caminho)
{
    if (!a || !a->imagem) return false;

    int w = al_get_bitmap_width(a->imagem);
    int h = al_get_bitmap_height(a->imagem);

    for (int d = 0; d < QTD_DIRECOES; d++)
    for (int f = 0; f < QTD_FRAMES; f++)
    {
        SourceRect r = a->source[d][f];

        if (r.sw <= 0 || r.sh <= 0 ||
            r.sx < 0 || r.sy < 0 ||
            r.sx + r.sw > w || r.sy + r.sh > h)
        {
            printf("ERRO sourceRect %s dir=%d frame=%d [%d,%d,%d,%d] folha=%dx%d\n",
                   caminho,d,f,r.sx,r.sy,r.sw,r.sh,w,h);
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
    if (!a) return false;
    memset(a,0,sizeof(*a));

    a->imagem = carregarBitmapFlexivel(caminho);
    if (!a->imagem) return false;

    int w = al_get_bitmap_width(a->imagem);
    int h = al_get_bitmap_height(a->imagem);

    a->framesPorLinha = QTD_FRAMES;
    a->linhasDirecao = QTD_DIRECOES;

    /*
     * frameW/frameH sao calculados POR BITMAP. Assim cada uma das cinco
     * folhas carry pode possuir dimensoes diferentes de idle/walk/run sem
     * depender de FRAME_SPRITE global.
     */
    a->frameW = w / QTD_FRAMES;
    a->frameH = h / QTD_DIRECOES;
    a->margemX = 0;
    a->margemY = 0;
    a->gapX = 0;
    a->gapY = 0;

    a->linhaDirecao[DIRECAO_DOWN]  = 0;
    a->linhaDirecao[DIRECAO_UP]    = 1;
    a->linhaDirecao[DIRECAO_LEFT]  = 2;
    a->linhaDirecao[DIRECAO_RIGHT] = 3;

    if (a->frameW < 16 || a->frameH < 16)
    {
        printf("ERRO sheet pequena: %s %dx%d\n", caminho,w,h);
        al_destroy_bitmap(a->imagem);
        a->imagem=NULL;
        return false;
    }

    ALLEGRO_LOCKED_REGION* lock =
        al_lock_bitmap(a->imagem,
                       ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,
                       ALLEGRO_LOCK_READONLY);

    for (int d=0; d<QTD_DIRECOES; d++)
    for (int f=0; f<QTD_FRAMES; f++)
    {
        int cellX = a->margemX + f * (a->frameW + a->gapX);
        int cellY = a->margemY + a->linhaDirecao[d] * (a->frameH + a->gapY);

        if (lock)
            a->source[d][f] = componentesSignificativos(lock,cellX,cellY,a->frameW,a->frameH);
        else
            a->source[d][f] = (SourceRect){cellX,cellY,a->frameW,a->frameH};
    }

    if (lock)
        al_unlock_bitmap(a->imagem);

    a->frameAtual=0;
    a->acumulador=0;
    a->tempoFrame=tempoFrame;
    a->escalaVisual=escalaVisual;
    a->anchorNormX=anchorX;
    a->anchorNormY=anchorY;
    a->ultimoFrameSom=-1;

    if (!validarAnimacao(a,caminho))
    {
        al_destroy_bitmap(a->imagem);
        a->imagem=NULL;
        return false;
    }

    printf("Sprite OK: %s folha=%dx%d celula=%dx%d resto=%d,%d | RIGHT:",
           caminho,w,h,a->frameW,a->frameH,
           w-a->frameW*QTD_FRAMES,
           h-a->frameH*QTD_DIRECOES);

    for (int f=0; f<QTD_FRAMES; f++)
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
    a->frameAtual=0;
    a->acumulador=0;
    a->ultimoFrameSom=-1;
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

        if(a->frameAtual>=QTD_FRAMES)
        {
            reiniciarAnimacao(a);
            return true;
        }
    }

    return false;
}

void desenharAnimacao(const Animacao* a,Direcao direcao,float x,float y)
{
    if(!a||!a->imagem)return;

    int d=(int)direcao;
    int f=a->frameAtual;
    if(d<0||d>=QTD_DIRECOES)d=0;
    if(f<0||f>=QTD_FRAMES)f=0;

    SourceRect r=a->source[d][f];
    int cellX=a->margemX+f*(a->frameW+a->gapX);
    int cellY=a->margemY+a->linhaDirecao[d]*(a->frameH+a->gapY);

    float s=a->escalaVisual;
    float origemVisualX=x-a->frameW*s*a->anchorNormX;
    float origemVisualY=y-a->frameH*s*a->anchorNormY;
    float dx=origemVisualX+(r.sx-cellX)*s;
    float dy=origemVisualY+(r.sy-cellY)*s;

    al_draw_scaled_bitmap(a->imagem,
                          r.sx,r.sy,r.sw,r.sh,
                          dx,dy,r.sw*s,r.sh*s,0);
}

bool carregarSprites(Scooby* s,Maria* m)
{
    if(!s||!m)return false;

    if(!carregarAnimacao(&s->idle,"ScoobySprites/idle.png",.20f,.37f,.50f,.90f))return false;
    if(!carregarAnimacao(&s->walk,"ScoobySprites/walk.png",.12f,.37f,.50f,.90f))return false;
    if(!carregarAnimacao(&s->run, "ScoobySprites/run.png",.085f,.37f,.50f,.90f))return false;
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
    {
        /*
         * Cada cor e medida individualmente. O anchor permanece nos pes,
         * mas o source rect inclui todos os componentes significativos,
         * evitando o corte superior observado quando olha para a direita.
         */
        if(!carregarAnimacao(&s->carregar[i],carry[i],.14f,.37f,.50f,.90f))
            return false;
    }

    if(!carregarAnimacao(&m->idle,"mariaSprites/idle.png",.20f,.31f,.50f,.93f))return false;
    if(!carregarAnimacao(&m->walk,"mariaSprites/walk.png",.13f,.31f,.50f,.93f))return false;
    if(!carregarAnimacao(&m->run, "mariaSprites/run.png",.095f,.31f,.50f,.93f))return false;
    if(!carregarAnimacao(&m->pick,"mariaSprites/pick.png",.11f,.31f,.50f,.93f))return false;

    return true;
}

void destruirAnimacaoInterna(Animacao* a)
{
    if(a&&a->imagem)
    {
        al_destroy_bitmap(a->imagem);
        a->imagem=NULL;
    }
}
