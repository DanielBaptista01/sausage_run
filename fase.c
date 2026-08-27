#include "jogo.h"

static float clamp01(float v)
{
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static void adicionarObjeto(Fase* fase, int sx, int sy, int sw, int sh,
                            float mapaX, float mapaY, float escala,
                            float colX, float colY, float colW, float colH,
                            bool colide, bool bloqueiaVisao, int insetFonte)
{
    if (!fase || fase->quantidadeObjetos >= MAX_OBJETOS) return;

    ObjetoMapa* o = &fase->objetos[fase->quantidadeObjetos++];
    o->sx = sx;
    o->sy = sy;
    o->sw = sw;
    o->sh = sh;
    o->insetFonte = insetFonte < 0 ? 0 : insetFonte;
    o->mapaX = mapaX;
    o->mapaY = mapaY;
    o->escala = escala;
    o->colX = clamp01(colX);
    o->colY = clamp01(colY);
    o->colW = clamp01(colW);
    o->colH = clamp01(colH);
    o->anchorY = clamp01(o->colY + o->colH);
    o->colide = colide;
    o->bloqueiaVisao = bloqueiaVisao;
}

static void adicionarObstaculo(Fase* fase, float x, float y, float w, float h,
                               bool bloqueiaVisao)
{
    if (!fase || fase->quantidadeObstaculos >= MAX_OBSTACULOS) return;

    Obstaculo* o = &fase->obstaculos[fase->quantidadeObstaculos++];
    o->x = x;
    o->y = y;
    o->largura = w;
    o->altura = h;
    o->bloqueiaMovimento = true;
    o->bloqueiaVisao = bloqueiaVisao;
}

static void montarColisoes(Fase* fase)
{
    if (!fase) return;

    fase->quantidadeObstaculos = 0;

    for (int i = 0; i < fase->quantidadeObjetos; i++)
    {
        const ObjetoMapa* obj = &fase->objetos[i];
        if (!obj->colide) continue;

        float dx = mapaParaTelaX(obj->mapaX);
        float dy = mapaParaTelaY(obj->mapaY);
        float dw = obj->sw * MAPA_ESCALA * obj->escala;
        float dh = obj->sh * MAPA_ESCALA * obj->escala;

        adicionarObstaculo(
            fase,
            dx + dw * obj->colX,
            dy + dh * obj->colY,
            dw * obj->colW,
            dh * obj->colH,
            obj->bloqueiaVisao);
    }

    const float e = 18.0f;
    Retangulo a = fase->areaJogavel;

    adicionarObstaculo(fase, a.x - e, a.y - e, a.largura + 2.0f * e, e, true);
    adicionarObstaculo(fase, a.x - e, a.y + a.altura, a.largura + 2.0f * e, e, true);
    adicionarObstaculo(fase, a.x - e, a.y, e, a.altura, true);
    adicionarObstaculo(fase, a.x + a.largura, a.y, e, a.altura, true);
}

void desenharObjeto(const Fase* fase, const ObjetoMapa* obj)
{
    if (!fase || !obj) return;

    float dx = mapaParaTelaX(obj->mapaX);
    float dy = mapaParaTelaY(obj->mapaY);

    int inset = obj->insetFonte;
    int sx = obj->sx + inset;
    int sy = obj->sy + inset;
    int sw = obj->sw - inset * 2;
    int sh = obj->sh - inset * 2;

    if (sw <= 0 || sh <= 0)
        return;

    float escalaTela = MAPA_ESCALA * obj->escala;
    float destinoX = dx + inset * escalaTela;
    float destinoY = dy + inset * escalaTela;
    float dw = sw * escalaTela;
    float dh = sh * escalaTela;

    if (fase->folhaObjetos)
    {
        int larguraFolha = al_get_bitmap_width(fase->folhaObjetos);
        int alturaFolha = al_get_bitmap_height(fase->folhaObjetos);

        if (sx >= 0 && sy >= 0 &&
            sx + sw <= larguraFolha &&
            sy + sh <= alturaFolha)
        {
            al_draw_scaled_bitmap(
                fase->folhaObjetos,
                sx, sy, sw, sh,
                destinoX, destinoY, dw, dh, 0);
            return;
        }
    }

    al_draw_filled_rectangle(
        destinoX, destinoY, destinoX + dw, destinoY + dh,
        al_map_rgb(132, 76, 38));
    al_draw_rectangle(
        destinoX, destinoY, destinoX + dw, destinoY + dh,
        al_map_rgb(65, 35, 24), 2.0f);
}

float baseYObjeto(const ObjetoMapa* obj)
{
    if (!obj) return -9999.0f;

    return mapaParaTelaY(obj->mapaY) +
           obj->sh * MAPA_ESCALA * obj->escala * obj->anchorY;
}

bool validarObjetosFase(const Fase* fase)
{
    if (!fase || !fase->folhaObjetos) return false;

    int largura = al_get_bitmap_width(fase->folhaObjetos);
    int altura = al_get_bitmap_height(fase->folhaObjetos);
    bool ok = true;

    for (int i = 0; i < fase->quantidadeObjetos; i++)
    {
        const ObjetoMapa* o = &fase->objetos[i];

        if (o->sx < 0 || o->sy < 0 || o->sw <= 0 || o->sh <= 0 ||
            o->sx + o->sw > largura || o->sy + o->sh > altura)
        {
            printf("Objeto %d de %s possui recorte invalido: sx=%d sy=%d sw=%d sh=%d; folha=%dx%d\n",
                   i, fase->nome, o->sx, o->sy, o->sw, o->sh,
                   largura, altura);
            ok = false;
        }
    }

    return ok;
}

static void adicionarWaypoint(Fase* f, float x, float y)
{
    if (f->quantidadeWaypoints >= MAX_WAYPOINTS) return;
    f->waypoints[f->quantidadeWaypoints++] =
        (Ponto){ mapaParaTelaX(x), mapaParaTelaY(y) };
}

static void adicionarSpawnBola(Fase* f, float x, float y)
{
    if (f->quantidadeSpawnsBola >= 3) return;
    f->spawnsBola[f->quantidadeSpawnsBola++] =
        (Ponto){ mapaParaTelaX(x), mapaParaTelaY(y) };
}

static Retangulo areaFonte(float x, float y, float w, float h)
{
    return (Retangulo){
        mapaParaTelaX(x), mapaParaTelaY(y),
        w * MAPA_ESCALA, h * MAPA_ESCALA
    };
}

static void configurarCozinha(Fase* f)
{
    f->nome = "Cozinha";
    f->caminhoFundo = "mapa/cozinha.png";
    f->caminhoObjetos = "mapa/cozinha_objetos.png";
    f->areaJogavel = areaFonte(72, 118, 1302, 875);

    adicionarObjeto(f, 25, 19, 243, 354, 85, 138, 1.0f, .10f, .68f, .80f, .28f, true, true, 1);
    adicionarObjeto(f, 326, 71, 486, 269, 345, 145, 1.0f, .02f, .66f, .96f, .31f, true, true, 1);
    adicionarObjeto(f, 864, 96, 275, 251, 875, 155, 1.0f, .08f, .64f, .84f, .33f, true, true, 1);
    adicionarObjeto(f, 1164, 14, 234, 363, 1180, 118, 1.0f, .08f, .69f, .84f, .27f, true, true, 1);
    adicionarObjeto(f, 55, 401, 341, 380, 320, 405, .95f, .08f, .56f, .84f, .38f, true, false, 1);
    adicionarObjeto(f, 838, 396, 160, 385, 860, 400, .95f, .08f, .67f, .84f, .28f, true, true, 1);
    adicionarObjeto(f, 451, 615, 192, 243, 95, 700, .95f, .08f, .62f, .84f, .30f, true, true, 1);
    adicionarObjeto(f, 700, 708, 105, 124, 1080, 720, .90f, .12f, .58f, .76f, .35f, true, false, 1);
    adicionarObjeto(f, 975, 667, 106, 203, 1200, 680, .90f, .20f, .66f, .60f, .29f, true, false, 3);
    adicionarObjeto(f, 240, 817, 177, 241, 1190, 785, .90f, .15f, .68f, .70f, .25f, true, true, 1);

    f->spawnScooby = (Ponto){ mapaParaTelaX(665), mapaParaTelaY(930) };
    f->spawnMaria = (Ponto){ mapaParaTelaX(1030), mapaParaTelaY(565) };

    f->saida = areaFonte(1240, 760, 120, 210);
    f->alvoTransicao = (Ponto){ mapaParaTelaX(1330), mapaParaTelaY(865) };
    f->tipoSaida = SAIDA_PORTA;

    adicionarWaypoint(f, 332, 332);
    adicionarWaypoint(f, 694, 452);
    adicionarWaypoint(f, 1056, 452);
    adicionarWaypoint(f, 1056, 814);
    adicionarWaypoint(f, 634, 814);
    adicionarWaypoint(f, 272, 754);

    adicionarSpawnBola(f, 450, 450);
    adicionarSpawnBola(f, 750, 650);
    adicionarSpawnBola(f, 1050, 750);
}

static void configurarSala(Fase* f)
{
    f->nome = "Sala";
    f->caminhoFundo = "mapa/sala.png";
    f->caminhoObjetos = "mapa/sala_objetos.png";
    f->areaJogavel = areaFonte(68, 115, 1300, 875);

    adicionarObjeto(f, 52, 19, 188, 494, 70, 285, .85f, .08f, .70f, .84f, .24f, true, true, 1);
    adicionarObjeto(f, 353, 66, 277, 245, 220, 135, .95f, .06f, .66f, .88f, .31f, true, true, 1);
    adicionarObjeto(f, 723, 71, 451, 397, 735, 105, .86f, .07f, .66f, .86f, .30f, true, true, 1);
    adicionarObjeto(f, 1213, 23, 202, 363, 1205, 115, .95f, .08f, .69f, .84f, .27f, true, true, 1);
    adicionarObjeto(f, 258, 469, 372, 213, 400, 435, 1.05f, .04f, .52f, .92f, .43f, true, true, 1);
    adicionarObjeto(f, 695, 500, 244, 184, 470, 675, .92f, .06f, .48f, .88f, .47f, true, false, 1);
    adicionarObjeto(f, 960, 545, 135, 130, 225, 770, .90f, .10f, .46f, .80f, .49f, true, false, 1);
    adicionarObjeto(f, 1112, 461, 154, 213, 1025, 560, .95f, .10f, .60f, .80f, .35f, true, true, 1);
    adicionarObjeto(f, 1291, 500, 122, 163, 1190, 585, .90f, .10f, .58f, .80f, .38f, true, false, 1);
    adicionarObjeto(f, 485, 689, 256, 386, 610, 690, .75f, .08f, .67f, .84f, .28f, true, true, 1);

    f->spawnScooby = (Ponto){ mapaParaTelaX(265), mapaParaTelaY(900) };
    f->spawnMaria = (Ponto){ mapaParaTelaX(650), mapaParaTelaY(300) };
    f->saida = areaFonte(1240, 760, 120, 205);
    f->alvoTransicao = (Ponto){ mapaParaTelaX(1325), mapaParaTelaY(860) };
    f->tipoSaida = SAIDA_PORTA;

    adicionarWaypoint(f, 272, 392);
    adicionarWaypoint(f, 634, 332);
    adicionarWaypoint(f, 996, 452);
    adicionarWaypoint(f, 996, 694);
    adicionarWaypoint(f, 814, 814);
    adicionarWaypoint(f, 332, 754);

    adicionarSpawnBola(f, 820, 545);
    adicionarSpawnBola(f, 980, 760);
    adicionarSpawnBola(f, 320, 690);
}

static void configurarBanheiro(Fase* f)
{
    f->nome = "Banheiro";
    f->caminhoFundo = "mapa/banheiro.png";
    f->caminhoObjetos = "mapa/banheiro_objetos.png";
    f->areaJogavel = areaFonte(78, 120, 1285, 865);

    adicionarObjeto(f, 42, 19, 437, 478, 55, 110, .90f, .04f, .68f, .92f, .27f, true, true, 1);
    adicionarObjeto(f, 539, 92, 167, 359, 950, 140, .95f, .12f, .69f, .76f, .26f, true, true, 1);
    adicionarObjeto(f, 809, 36, 174, 431, 595, 300, .92f, .08f, .66f, .84f, .30f, true, true, 1);
    adicionarObjeto(f, 1063, 58, 279, 193, 1090, 85, .88f, .06f, .60f, .88f, .36f, true, true, 1);
    adicionarObjeto(f, 1022, 275, 176, 240, 1060, 455, .95f, .08f, .61f, .84f, .34f, true, true, 1);
    adicionarObjeto(f, 1230, 300, 198, 200, 1215, 500, .88f, .08f, .58f, .84f, .37f, true, true, 1);
    adicionarObjeto(f, 42, 501, 397, 410, 80, 570, .88f, .05f, .68f, .90f, .27f, true, true, 1);
    adicionarObjeto(f, 558, 546, 176, 163, 735, 650, .82f, .08f, .54f, .84f, .41f, true, false, 1);
    adicionarObjeto(f, 805, 551, 133, 154, 900, 760, .82f, .10f, .53f, .80f, .42f, true, false, 1);
    adicionarObjeto(f, 1000, 567, 192, 160, 1040, 760, .82f, .08f, .52f, .84f, .43f, true, false, 1);

    f->spawnScooby = (Ponto){ mapaParaTelaX(720), mapaParaTelaY(930) };
    f->spawnMaria = (Ponto){ mapaParaTelaX(820), mapaParaTelaY(390) };
    f->saida = areaFonte(585, 900, 280, 80);
    f->alvoTransicao = (Ponto){ mapaParaTelaX(725), mapaParaTelaY(970) };
    f->tipoSaida = SAIDA_PORTA;

    adicionarWaypoint(f, 452, 392);
    adicionarWaypoint(f, 814, 392);
    adicionarWaypoint(f, 1116, 392);
    adicionarWaypoint(f, 1056, 754);
    adicionarWaypoint(f, 754, 814);
    adicionarWaypoint(f, 452, 754);

    adicionarSpawnBola(f, 900, 650);
    adicionarSpawnBola(f, 520, 790);
    adicionarSpawnBola(f, 830, 570);
}

static void configurarQuarto(Fase* f)
{
    f->nome = "Quarto";
    f->caminhoFundo = "mapa/quarto.png";
    f->caminhoObjetos = "mapa/quarto_objetos.png";
    f->areaJogavel = areaFonte(72, 115, 1295, 875);

    adicionarObjeto(f, 0, 1, 32, 28, 90, 120, 11.6f, .04f, .66f, .92f, .30f, true, true, 0);
    adicionarObjeto(f, 33, 7, 20, 20, 520, 145, 11.7f, .06f, .66f, .88f, .31f, true, true, 0);
    adicionarObjeto(f, 55, 1, 18, 26, 900, 110, 11.8f, .06f, .68f, .88f, .29f, true, true, 0);
    adicionarObjeto(f, 75, 6, 10, 20, 1160, 145, 12.0f, .10f, .70f, .80f, .26f, true, true, 0);
    adicionarObjeto(f, 87, 9, 11, 18, 1230, 385, 11.2f, .08f, .64f, .84f, .31f, true, true, 0);
    adicionarObjeto(f, 2, 29, 29, 20, 455, 470, 11.5f, .05f, .53f, .90f, .42f, true, false, 0);
    adicionarObjeto(f, 34, 28, 23, 20, 90, 525, 11.3f, .05f, .63f, .90f, .32f, true, true, 0);
    adicionarObjeto(f, 60, 32, 20, 16, 930, 525, 11.0f, .06f, .57f, .88f, .38f, true, true, 0);
    adicionarObjeto(f, 82, 30, 17, 17, 1150, 590, 11.1f, .08f, .54f, .84f, .41f, true, false, 0);
    adicionarObjeto(f, 3, 50, 13, 10, 300, 760, 11.2f, .08f, .50f, .84f, .44f, true, false, 0);
    adicionarObjeto(f, 44, 52, 7, 9, 690, 770, 10.8f, .08f, .50f, .84f, .44f, true, false, 0);

    f->spawnScooby = (Ponto){ mapaParaTelaX(260), mapaParaTelaY(900) };
    f->spawnMaria = (Ponto){ mapaParaTelaX(700), mapaParaTelaY(430) };
    f->saida = areaFonte(1110, 835, 220, 145);
    f->alvoTransicao = (Ponto){ mapaParaTelaX(1220), mapaParaTelaY(925) };
    f->tipoSaida = SAIDA_ESCADA;

    adicionarWaypoint(f, 332, 513);
    adicionarWaypoint(f, 754, 452);
    adicionarWaypoint(f, 1056, 452);
    adicionarWaypoint(f, 1056, 754);
    adicionarWaypoint(f, 754, 935);
    adicionarWaypoint(f, 272, 814);

    adicionarSpawnBola(f, 850, 680);
    adicionarSpawnBola(f, 1050, 790);
    adicionarSpawnBola(f, 500, 820);
}

void configurarFases(Fase fases[QTD_FASES])
{
    if (!fases) return;

    for (int i = 0; i < QTD_FASES; i++)
        memset(&fases[i], 0, sizeof(Fase));

    configurarCozinha(&fases[0]);
    configurarSala(&fases[1]);
    configurarBanheiro(&fases[2]);
    configurarQuarto(&fases[3]);

    for (int i = 0; i < QTD_FASES; i++)
        montarColisoes(&fases[i]);
}
