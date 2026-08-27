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
    if (!fase || fase->quantidadeObstaculos >= MAX_OBSTACULOS || w <= 0 || h <= 0)
        return;

    Obstaculo* o = &fase->obstaculos[fase->quantidadeObstaculos++];
    o->x = x;
    o->y = y;
    o->largura = w;
    o->altura = h;
    o->bloqueiaMovimento = true;
    o->bloqueiaVisao = bloqueiaVisao;
}

void reconstruirColisoesFase(Fase* fase)
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

    /*
     * O retangulo areaJogavel representa somente piso. O corpo visual pode
     * sobrepor parede, mas o anchor/pes nao atravessa este perimetro.
     */
    const float e = 24.0f;
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

    if (sw <= 0 || sh <= 0) return;

    float escalaTela = MAPA_ESCALA * obj->escala;
    float destinoX = dx + inset * escalaTela;
    float destinoY = dy + inset * escalaTela;
    float dw = sw * escalaTela;
    float dh = sh * escalaTela;

    if (fase->folhaObjetos)
    {
        int larguraFolha = al_get_bitmap_width(fase->folhaObjetos);
        int alturaFolha = al_get_bitmap_height(fase->folhaObjetos);

        if (sx >= 0 && sy >= 0 && sx + sw <= larguraFolha && sy + sh <= alturaFolha)
        {
            al_draw_scaled_bitmap(
                fase->folhaObjetos,
                sx, sy, sw, sh,
                destinoX, destinoY, dw, dh, 0);
            return;
        }
    }

    al_draw_filled_rectangle(destinoX, destinoY, destinoX + dw, destinoY + dh,
                             al_map_rgb(132, 76, 38));
    al_draw_rectangle(destinoX, destinoY, destinoX + dw, destinoY + dh,
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
                   i, fase->nome, o->sx, o->sy, o->sw, o->sh, largura, altura);
            ok = false;
        }
    }

    return ok;
}

static void adicionarWaypoint(Fase* f, float x, float y)
{
    if (!f || f->quantidadeWaypoints >= MAX_WAYPOINTS) return;
    f->waypoints[f->quantidadeWaypoints++] =
        (Ponto){ mapaParaTelaX(x), mapaParaTelaY(y) };
}

static void adicionarSpawnBola(Fase* f, float x, float y)
{
    if (!f || f->quantidadeSpawnsBola >= 3) return;
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

    /* Piso real: exclui toda a faixa superior de parede e rodapes laterais. */
    f->areaJogavel = areaFonte(122, 278, 1200, 705);

    /* Parede superior: geladeira, bancada, fogao e armario. Colisao so na base. */
    adicionarObjeto(f, 25, 19, 243, 354, 105, 115, .94f, .12f, .78f, .76f, .18f, true, true, 2);
    adicionarObjeto(f, 326, 71, 486, 269, 360, 112, .93f, .04f, .76f, .92f, .20f, true, true, 2);
    adicionarObjeto(f, 864, 96, 275, 251, 880, 120, .92f, .10f, .76f, .80f, .20f, true, true, 2);
    adicionarObjeto(f, 1164, 14, 234, 363, 1165, 110, .91f, .10f, .78f, .80f, .18f, true, true, 2);

    /* Mesa foi deslocada para a esquerda e ilha para a direita: corredor central largo. */
    adicionarObjeto(f, 55, 401, 341, 380, 255, 410, .88f, .12f, .67f, .76f, .27f, true, false, 2);
    adicionarObjeto(f, 838, 396, 160, 385, 825, 420, .84f, .14f, .72f, .72f, .24f, true, true, 2);

    /* Aparador e decoracoes: bases pequenas; balde/vassoura/planta nao criam gargalo. */
    adicionarObjeto(f, 451, 615, 192, 243, 115, 700, .86f, .15f, .70f, .70f, .23f, true, true, 2);
    adicionarObjeto(f, 700, 708, 105, 124, 1040, 715, .82f, .20f, .68f, .60f, .24f, false, false, 2);
    adicionarObjeto(f, 975, 667, 106, 203, 1130, 690, .82f, .25f, .72f, .50f, .22f, false, false, 4);
    adicionarObjeto(f, 240, 817, 177, 241, 1195, 755, .82f, .22f, .76f, .56f, .18f, false, false, 2);

    f->spawnScooby = (Ponto){ mapaParaTelaX(610), mapaParaTelaY(850) };
    f->spawnMaria = (Ponto){ mapaParaTelaX(1010), mapaParaTelaY(520) };

    /* No mapa atual a passagem inferior central e tratada como a escada da cozinha. */
    f->saida = areaFonte(625, 900, 190, 82);
    f->alvoTransicao = (Ponto){ mapaParaTelaX(720), mapaParaTelaY(975) };
    f->tipoSaida = SAIDA_ESCADA;

    adicionarWaypoint(f, 520, 365);
    adicionarWaypoint(f, 745, 365);
    adicionarWaypoint(f, 1035, 420);
    adicionarWaypoint(f, 1040, 760);
    adicionarWaypoint(f, 710, 810);
    adicionarWaypoint(f, 360, 750);

    adicionarSpawnBola(f, 515, 520);
    adicionarSpawnBola(f, 715, 720);
    adicionarSpawnBola(f, 1010, 740);
}

static void configurarSala(Fase* f)
{
    f->nome = "Sala";
    f->caminhoFundo = "mapa/sala.png";
    f->caminhoObjetos = "mapa/sala_objetos.png";
    f->areaJogavel = areaFonte(120, 270, 1205, 710);

    adicionarObjeto(f, 52, 19, 188, 494, 105, 235, .78f, .10f, .78f, .80f, .17f, true, true, 2);
    adicionarObjeto(f, 353, 66, 277, 245, 235, 145, .86f, .08f, .72f, .84f, .22f, true, true, 2);
    adicionarObjeto(f, 723, 71, 451, 397, 720, 120, .78f, .08f, .75f, .84f, .20f, true, true, 2);
    adicionarObjeto(f, 1213, 23, 202, 363, 1180, 135, .84f, .12f, .76f, .76f, .20f, true, true, 2);

    /* Sofa central com espaco para dar volta pelos dois lados. */
    adicionarObjeto(f, 258, 469, 372, 213, 430, 455, .88f, .08f, .56f, .84f, .37f, true, true, 2);
    adicionarObjeto(f, 695, 500, 244, 184, 475, 690, .82f, .10f, .48f, .80f, .39f, true, false, 2);
    adicionarObjeto(f, 960, 545, 135, 130, 245, 750, .80f, .16f, .48f, .68f, .38f, true, false, 2);
    adicionarObjeto(f, 1112, 461, 154, 213, 1035, 535, .82f, .14f, .58f, .72f, .30f, true, true, 2);

    /* Decoracao/planta nao bloqueia a rota principal. */
    adicionarObjeto(f, 1291, 500, 122, 163, 1190, 610, .78f, .18f, .58f, .64f, .30f, false, false, 2);
    adicionarObjeto(f, 485, 689, 256, 386, 650, 680, .68f, .12f, .70f, .76f, .21f, true, true, 2);

    f->spawnScooby = (Ponto){ mapaParaTelaX(315), mapaParaTelaY(855) };
    f->spawnMaria = (Ponto){ mapaParaTelaX(800), mapaParaTelaY(390) };

    f->saida = areaFonte(1220, 815, 105, 155);
    f->alvoTransicao = (Ponto){ mapaParaTelaX(1300), mapaParaTelaY(900) };
    f->tipoSaida = SAIDA_PORTA;

    adicionarWaypoint(f, 340, 390);
    adicionarWaypoint(f, 660, 365);
    adicionarWaypoint(f, 990, 400);
    adicionarWaypoint(f, 1070, 690);
    adicionarWaypoint(f, 790, 820);
    adicionarWaypoint(f, 370, 770);

    adicionarSpawnBola(f, 855, 540);
    adicionarSpawnBola(f, 995, 760);
    adicionarSpawnBola(f, 340, 675);
}

static void configurarBanheiro(Fase* f)
{
    f->nome = "Banheiro";
    f->caminhoFundo = "mapa/banheiro.png";
    f->caminhoObjetos = "mapa/banheiro_objetos.png";
    f->areaJogavel = areaFonte(125, 275, 1195, 700);

    adicionarObjeto(f, 42, 19, 437, 478, 85, 120, .80f, .08f, .75f, .84f, .18f, true, true, 2);
    adicionarObjeto(f, 539, 92, 167, 359, 965, 145, .84f, .14f, .74f, .72f, .20f, true, true, 2);
    adicionarObjeto(f, 809, 36, 174, 431, 600, 280, .80f, .12f, .72f, .76f, .22f, true, true, 2);
    adicionarObjeto(f, 1063, 58, 279, 193, 1080, 95, .78f, .10f, .62f, .80f, .30f, true, true, 2);
    adicionarObjeto(f, 1022, 275, 176, 240, 1060, 470, .80f, .12f, .58f, .76f, .30f, true, true, 2);
    adicionarObjeto(f, 1230, 300, 198, 200, 1195, 525, .76f, .14f, .56f, .72f, .30f, true, true, 2);
    adicionarObjeto(f, 42, 501, 397, 410, 105, 585, .78f, .08f, .72f, .84f, .22f, true, true, 2);

    /* Itens pequenos de limpeza/decoracao nao fecham o corredor. */
    adicionarObjeto(f, 558, 546, 176, 163, 745, 655, .74f, .14f, .48f, .72f, .36f, false, false, 2);
    adicionarObjeto(f, 805, 551, 133, 154, 890, 760, .72f, .18f, .48f, .64f, .36f, false, false, 2);
    adicionarObjeto(f, 1000, 567, 192, 160, 1030, 760, .72f, .16f, .48f, .68f, .36f, false, false, 2);

    f->spawnScooby = (Ponto){ mapaParaTelaX(650), mapaParaTelaY(850) };
    f->spawnMaria = (Ponto){ mapaParaTelaX(850), mapaParaTelaY(450) };

    f->saida = areaFonte(600, 900, 245, 72);
    f->alvoTransicao = (Ponto){ mapaParaTelaX(725), mapaParaTelaY(970) };
    f->tipoSaida = SAIDA_PORTA;

    adicionarWaypoint(f, 445, 405);
    adicionarWaypoint(f, 760, 405);
    adicionarWaypoint(f, 1080, 420);
    adicionarWaypoint(f, 1040, 705);
    adicionarWaypoint(f, 745, 815);
    adicionarWaypoint(f, 420, 735);

    adicionarSpawnBola(f, 880, 630);
    adicionarSpawnBola(f, 520, 760);
    adicionarSpawnBola(f, 780, 570);
}

static void configurarQuarto(Fase* f)
{
    f->nome = "Quarto";
    f->caminhoFundo = "mapa/quarto.png";
    f->caminhoObjetos = "mapa/quarto_objetos.png";
    f->areaJogavel = areaFonte(120, 270, 1205, 705);

    adicionarObjeto(f, 0, 1, 32, 28, 105, 125, 10.3f, .06f, .72f, .88f, .24f, true, true, 0);
    adicionarObjeto(f, 33, 7, 20, 20, 530, 150, 10.4f, .08f, .72f, .84f, .24f, true, true, 0);
    adicionarObjeto(f, 55, 1, 18, 26, 900, 120, 10.5f, .08f, .74f, .84f, .22f, true, true, 0);
    adicionarObjeto(f, 75, 6, 10, 20, 1160, 150, 10.5f, .12f, .74f, .76f, .22f, true, true, 0);
    adicionarObjeto(f, 87, 9, 11, 18, 1220, 400, 10.0f, .10f, .68f, .80f, .26f, true, true, 0);

    /* Moveis centrais separados para formar corredores largos. */
    adicionarObjeto(f, 2, 29, 29, 20, 430, 500, 10.1f, .08f, .58f, .84f, .35f, true, false, 0);
    adicionarObjeto(f, 34, 28, 23, 20, 125, 555, 10.0f, .08f, .68f, .84f, .27f, true, true, 0);
    adicionarObjeto(f, 60, 32, 20, 16, 945, 555, 9.8f, .10f, .60f, .80f, .32f, true, true, 0);

    adicionarObjeto(f, 82, 30, 17, 17, 1150, 620, 9.7f, .12f, .48f, .76f, .36f, false, false, 0);
    adicionarObjeto(f, 3, 50, 13, 10, 330, 770, 9.8f, .12f, .45f, .76f, .36f, false, false, 0);
    adicionarObjeto(f, 44, 52, 7, 9, 700, 780, 9.5f, .12f, .45f, .76f, .36f, false, false, 0);

    f->spawnScooby = (Ponto){ mapaParaTelaX(300), mapaParaTelaY(850) };
    f->spawnMaria = (Ponto){ mapaParaTelaX(735), mapaParaTelaY(445) };

    f->saida = areaFonte(1115, 845, 205, 125);
    f->alvoTransicao = (Ponto){ mapaParaTelaX(1220), mapaParaTelaY(930) };
    f->tipoSaida = SAIDA_PORTA;

    adicionarWaypoint(f, 350, 430);
    adicionarWaypoint(f, 720, 405);
    adicionarWaypoint(f, 1050, 430);
    adicionarWaypoint(f, 1035, 730);
    adicionarWaypoint(f, 740, 825);
    adicionarWaypoint(f, 350, 760);

    adicionarSpawnBola(f, 850, 650);
    adicionarSpawnBola(f, 1035, 785);
    adicionarSpawnBola(f, 520, 800);
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
        reconstruirColisoesFase(&fases[i]);
}
