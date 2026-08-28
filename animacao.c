#include "jogo.h"

/*
 * As folhas dos personagens sao grades 4x4, mas algumas possuem dimensoes
 * como 1254x1254. Nesses casos 1254/4 nao e inteiro.
 *
 * O erro anterior vinha de duas abordagens incorretas:
 *  - usar frameW/frameH inteiros e repetir 313 pixels, deixando o erro de
 *    arredondamento acumular a partir da segunda/terceira celula;
 *  - tentar recortar cada celula pelo alpha, o que podia interpretar pixels
 *    residuais da linha vizinha como parte legitima do personagem.
 *
 * A grade agora e calculada pelos LIMITES proporcionais da folha inteira:
 *   x0 = floor(frame     * largura / 4)
 *   x1 = floor((frame+1) * largura / 4)
 * e o mesmo para Y.
 *
 * Dessa forma os pixels excedentes sao distribuidos entre as celulas e as
 * 16 regioes cobrem a folha exatamente, sem sobreposicao e sem buracos.
 * Nenhum algoritmo de alpha expande ou reduz o source rectangle.
 */
static SourceRect sourceGrade(const Animacao* a,
                              int larguraFolha,
                              int alturaFolha,
                              int direcao,
                              int frame)
{
    int linha = a->linhaDirecao[direcao];

    int x0 = (frame * larguraFolha) / QTD_FRAMES;
    int x1 = ((frame + 1) * larguraFolha) / QTD_FRAMES;
    int y0 = (linha * alturaFolha) / QTD_DIRECOES;
    int y1 = ((linha + 1) * alturaFolha) / QTD_DIRECOES;

    return (SourceRect){x0, y0, x1 - x0, y1 - y0};
}

static bool validarAnimacao(const Animacao* a, const char* caminho)
{
    if (!a || !a->imagem)
        return false;

    int w = al_get_bitmap_width(a->imagem);
    int h = al_get_bitmap_height(a->imagem);

    for (int d = 0; d < QTD_DIRECOES; d++)
    {
        int ultimoFimX = 0;

        for (int f = 0; f < QTD_FRAMES; f++)
        {
            SourceRect r = a->source[d][f];

            if (r.sw <= 0 || r.sh <= 0 ||
                r.sx < 0 || r.sy < 0 ||
                r.sx + r.sw > w || r.sy + r.sh > h)
            {
                printf("ERRO sourceRect %s dir=%d frame=%d [%d,%d,%d,%d] folha=%dx%d\n",
                       caminho, d, f, r.sx, r.sy, r.sw, r.sh, w, h);
                return false;
            }

            /* As colunas de uma mesma linha precisam ser contiguas. */
            if (r.sx != ultimoFimX)
            {
                printf("ERRO grade horizontal %s dir=%d frame=%d: esperado x=%d recebido x=%d\n",
                       caminho, d, f, ultimoFimX, r.sx);
                return false;
            }

            ultimoFimX = r.sx + r.sw;
        }

        if (ultimoFimX != w)
        {
            printf("ERRO grade horizontal %s dir=%d termina em %d, folha=%d\n",
                   caminho, d, ultimoFimX, w);
            return false;
        }
    }

    /* Verifica tambem que as quatro linhas ocupam exatamente a altura. */
    for (int linha = 0; linha < QTD_DIRECOES; linha++)
    {
        int y0 = (linha * h) / QTD_DIRECOES;
        int y1 = ((linha + 1) * h) / QTD_DIRECOES;
        if (y1 <= y0)
            return false;
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
    if (!a)
        return false;

    memset(a, 0, sizeof(*a));

    a->imagem = carregarBitmapFlexivel(caminho);
    if (!a->imagem)
        return false;

    int w = al_get_bitmap_width(a->imagem);
    int h = al_get_bitmap_height(a->imagem);

    a->framesPorLinha = QTD_FRAMES;
    a->linhasDirecao = QTD_DIRECOES;

    /* Dimensoes nominais apenas para diagnostico/compatibilidade. */
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
        printf("ERRO sheet pequena: %s %dx%d\n", caminho, w, h);
        al_destroy_bitmap(a->imagem);
        a->imagem = NULL;
        return false;
    }

    for (int d = 0; d < QTD_DIRECOES; d++)
    {
        for (int f = 0; f < QTD_FRAMES; f++)
            a->source[d][f] = sourceGrade(a, w, h, d, f);
    }

    a->frameAtual = 0;
    a->acumulador = 0;
    a->tempoFrame = tempoFrame;
    a->escalaVisual = escalaVisual;
    a->anchorNormX = anchorX;
    a->anchorNormY = anchorY;
    a->ultimoFrameSom = -1;

    if (!validarAnimacao(a, caminho))
    {
        al_destroy_bitmap(a->imagem);
        a->imagem = NULL;
        return false;
    }

    printf("Sprite OK: %s folha=%dx%d grade proporcional 4x4 | ", caminho, w, h);
    printf("linhasY=");
    for (int linha = 0; linha < QTD_DIRECOES; linha++)
    {
        int y0 = (linha * h) / QTD_DIRECOES;
        int y1 = ((linha + 1) * h) / QTD_DIRECOES;
        printf(" L%d[%d..%d)", linha, y0, y1);
    }
    printf(" | RIGHT:");

    for (int f = 0; f < QTD_FRAMES; f++)
    {
        SourceRect r = a->source[DIRECAO_RIGHT][f];
        printf(" f%d=[%d,%d,%d,%d]", f, r.sx, r.sy, r.sw, r.sh);
    }
    printf("\n");

    return true;
}

void reiniciarAnimacao(Animacao* a)
{
    if (!a)
        return;

    a->frameAtual = 0;
    a->acumulador = 0;
    a->ultimoFrameSom = -1;
}

void atualizarAnimacaoLoop(Animacao* a, float dt)
{
    if (!a || !a->imagem || a->tempoFrame <= 0)
        return;

    a->acumulador += dt;

    while (a->acumulador >= a->tempoFrame)
    {
        a->acumulador -= a->tempoFrame;
        a->frameAtual = (a->frameAtual + 1) % QTD_FRAMES;
    }
}

bool atualizarAnimacaoUmaVez(Animacao* a, float dt)
{
    if (!a || !a->imagem)
        return true;

    a->acumulador += dt;

    if (a->acumulador >= a->tempoFrame)
    {
        a->acumulador -= a->tempoFrame;
        a->frameAtual++;

        if (a->frameAtual >= QTD_FRAMES)
        {
            reiniciarAnimacao(a);
            return true;
        }
    }

    return false;
}

void desenharAnimacao(const Animacao* a,
                      Direcao direcao,
                      float x,
                      float y)
{
    if (!a || !a->imagem)
        return;

    int d = (int)direcao;
    int f = a->frameAtual;

    if (d < 0 || d >= QTD_DIRECOES) d = DIRECAO_DOWN;
    if (f < 0 || f >= QTD_FRAMES) f = 0;

    SourceRect r = a->source[d][f];
    float s = a->escalaVisual;

    /*
     * O anchor e aplicado sobre a CELULA REAL daquele frame, e nao sobre
     * frameW/frameH truncados. Assim uma celula de 314 px e uma de 313 px
     * continuam apoiadas exatamente no mesmo ponto de chao.
     */
    float dw = r.sw * s;
    float dh = r.sh * s;
    float dx = x - dw * a->anchorNormX;
    float dy = y - dh * a->anchorNormY;

    al_draw_scaled_bitmap(a->imagem,
                          r.sx, r.sy, r.sw, r.sh,
                          dx, dy, dw, dh, 0);
}

bool carregarSprites(Scooby* s, Maria* m)
{
    if (!s || !m)
        return false;

    if (!carregarAnimacao(&s->idle, "ScoobySprites/idle.png", .20f, .37f, .50f, .90f)) return false;
    if (!carregarAnimacao(&s->walk, "ScoobySprites/walk.png", .12f, .37f, .50f, .90f)) return false;
    if (!carregarAnimacao(&s->run,  "ScoobySprites/run.png", .085f, .37f, .50f, .90f)) return false;
    if (!carregarAnimacao(&s->bark, "ScoobySprites/bark.png", .085f, .37f, .50f, .90f)) return false;
    if (!carregarAnimacao(&s->bite, "ScoobySprites/bite.png", .08f, .37f, .50f, .90f)) return false;

    static const char* carry[QTD_CORES_BOLA] = {
        "ScoobySprites/littleBalls/yellow_dog.png",
        "ScoobySprites/littleBalls/green_dog.png",
        "ScoobySprites/littleBalls/purple_dog.png",
        "ScoobySprites/littleBalls/blue_dog.png",
        "ScoobySprites/littleBalls/red_dog.png"
    };

    for (int i = 0; i < QTD_CORES_BOLA; i++)
    {
        /* Cada folha carry e medida de forma independente. */
        if (!carregarAnimacao(&s->carregar[i], carry[i], .14f, .37f, .50f, .90f))
            return false;
    }

    if (!carregarAnimacao(&m->idle, "mariaSprites/idle.png", .20f, .31f, .50f, .93f)) return false;
    if (!carregarAnimacao(&m->walk, "mariaSprites/walk.png", .13f, .31f, .50f, .93f)) return false;
    if (!carregarAnimacao(&m->run,  "mariaSprites/run.png", .095f, .31f, .50f, .93f)) return false;
    if (!carregarAnimacao(&m->pick, "mariaSprites/pick.png", .11f, .31f, .50f, .93f)) return false;

    return true;
}

void destruirAnimacaoInterna(Animacao* a)
{
    if (a && a->imagem)
    {
        al_destroy_bitmap(a->imagem);
        a->imagem = NULL;
    }
}
