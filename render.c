#include "jogo.h"

typedef enum {
    ITEM_OBJETO,
    ITEM_BOLA,
    ITEM_SCOOBY,
    ITEM_MARIA
} TipoItemRender;

typedef struct {
    TipoItemRender tipo;
    float depthY;
    int indice;
} ItemRender;

static void desenharBola(
    const RecursosMapa* r,
    const Bola* b)
{
    if (!r || !r->bolas ||
        !b || b->coletada)
    {
        return;
    }

    static const int sx[QTD_CORES_BOLA] =
        { 117, 536, 955, 1374, 1793 };

    static const int sy[QTD_CORES_BOLA] =
        { 219, 219, 219, 219, 219 };

    static const int sw[QTD_CORES_BOLA] =
        { 262, 262, 262, 262, 263 };

    static const int sh[QTD_CORES_BOLA] =
        { 259, 259, 259, 259, 259 };

    int cor = b->cor;
    if (cor < 0 || cor >= QTD_CORES_BOLA)
        cor = 0;

    float tamanho = 42.0f;

    al_draw_scaled_bitmap(
        r->bolas,
        sx[cor],
        sy[cor],
        sw[cor],
        sh[cor],
        b->x - tamanho * 0.5f,
        b->y - tamanho * 0.65f,
        tamanho,
        tamanho,
        0);
}

static void desenharScooby(
    const Scooby* s,
    const Bola* b)
{
    if (!s || !b) return;

    const Animacao* a = &s->idle;

    if (s->carregandoBola)
        a = &s->carregar[b->cor];
    else if (s->latindo)
        a = &s->bark;
    else if (s->mordendo)
        a = &s->bite;
    else if (s->correndo)
        a = &s->run;
    else if (s->movendo)
        a = &s->walk;

    desenharAnimacao(
        a,
        s->direcaoSprite,
        s->corpo.x,
        s->corpo.y,
        0.37f);
}

static void desenharMaria(const Maria* m)
{
    if (!m) return;

    const Animacao* a = &m->idle;

    if (m->estado == MARIA_CAPTURAR)
        a = &m->pick;
    else if (m->estado == MARIA_PERSEGUIR)
        a = &m->run;
    else if (m->movendo)
        a = &m->walk;

    desenharAnimacao(
        a,
        m->direcaoSprite,
        m->corpo.x,
        m->corpo.y,
        0.31f);
}

static void ordenarItens(
    ItemRender itens[],
    int quantidade)
{
    for (int i = 1; i < quantidade; i++)
    {
        ItemRender chave = itens[i];
        int j = i - 1;

        while (j >= 0 &&
               itens[j].depthY > chave.depthY)
        {
            itens[j + 1] = itens[j];
            j--;
        }

        itens[j + 1] = chave;
    }
}

static void desenharIndicadorSaida(
    const Fase* f,
    float tempo)
{
    if (!f) return;

    float pulso =
        0.55f +
        0.45f * sinf(tempo * 5.0f);

    unsigned char alpha =
        (unsigned char)(115 + pulso * 95);

    ALLEGRO_COLOR brilho =
        al_map_rgba(255, 225, 70, alpha);

    float x =
        f->saida.x + f->saida.largura * 0.5f;

    float y =
        f->saida.y + f->saida.altura * 0.5f;

    al_draw_rectangle(
        f->saida.x,
        f->saida.y,
        f->saida.x + f->saida.largura,
        f->saida.y + f->saida.altura,
        brilho,
        4.0f);

    al_draw_filled_triangle(
        x,
        y - 26.0f,
        x - 15.0f,
        y - 48.0f,
        x + 15.0f,
        y - 48.0f,
        brilho);
}

static void desenharHUD(
    const RecursosMapa* r,
    const Fase* f,
    const Scooby* s,
    const Bola* b,
    int vidas,
    int faseAtual,
    float tempoTutorial,
    EstadoJogo estado)
{
    if (!r || !r->fonte ||
        !f || !s || !b)
    {
        return;
    }

    ALLEGRO_COLOR painel =
        al_map_rgba(15, 15, 20, 190);

    al_draw_filled_rounded_rectangle(
        16, 14, 435, 118,
        10, 10,
        painel);

    char linha[256];

    snprintf(
        linha, sizeof(linha),
        "Fase %d/4 - %s",
        faseAtual + 1,
        f->nome);

    al_draw_text(
        r->fonte,
        al_map_rgb(255, 240, 205),
        30, 25,
        0,
        linha);

    snprintf(
        linha, sizeof(linha),
        "Vidas: %d   Bola: %s",
        vidas,
        NOMES_CORES[b->cor]);

    al_draw_text(
        r->fonte,
        al_map_rgb(245, 205, 215),
        30, 50,
        0,
        linha);

    const char* objetivo =
        s->carregandoBola ?
        "Objetivo: alcance a saida indicada." :
        "Objetivo: encontre e pegue a bolinha.";

    al_draw_text(
        r->fonte,
        al_map_rgb(210, 235, 255),
        30, 76,
        0,
        objetivo);

    if (tempoTutorial > 0.0f &&
        faseAtual == 0 &&
        estado == JOGO_RODANDO)
    {
        float largura = 560.0f;
        float x1 = LARGURA_TELA * 0.5f - largura * 0.5f;
        float x2 = LARGURA_TELA * 0.5f + largura * 0.5f;

        al_draw_filled_rounded_rectangle(
            x1, ALTURA_TELA - 105,
            x2, ALTURA_TELA - 20,
            10, 10,
            al_map_rgba(10, 10, 15, 210));

        al_draw_text(
            r->fonte,
            al_map_rgb(255, 255, 255),
            LARGURA_TELA * 0.5f,
            ALTURA_TELA - 91,
            ALLEGRO_ALIGN_CENTRE,
            "WASD mover | Shift correr | Espaco latir | E pegar bola");

        al_draw_text(
            r->fonte,
            al_map_rgb(255, 225, 120),
            LARGURA_TELA * 0.5f,
            ALTURA_TELA - 62,
            ALLEGRO_ALIGN_CENTRE,
            "Pegue a bolinha e chegue a saida sem Maria capturar Scooby.");

        al_draw_text(
            r->fonte,
            al_map_rgb(210, 210, 220),
            LARGURA_TELA * 0.5f,
            ALTURA_TELA - 36,
            ALLEGRO_ALIGN_CENTRE,
            "Esc pausa | F1 debug");
    }

    if (estado == JOGO_PAUSADO)
    {
        al_draw_filled_rectangle(
            0, 0,
            LARGURA_TELA,
            ALTURA_TELA,
            al_map_rgba(0, 0, 0, 165));

        al_draw_text(
            r->fonte,
            al_map_rgb(255, 255, 255),
            LARGURA_TELA * 0.5f,
            ALTURA_TELA * 0.5f - 20,
            ALLEGRO_ALIGN_CENTRE,
            "PAUSADO");

        al_draw_text(
            r->fonte,
            al_map_rgb(220, 220, 225),
            LARGURA_TELA * 0.5f,
            ALTURA_TELA * 0.5f + 15,
            ALLEGRO_ALIGN_CENTRE,
            "Pressione Esc para continuar");
    }
}

static void desenharDebug(
    const Fase* f,
    const Maria* m,
    const EventoSom* som)
{
    if (!f || !m) return;

    al_draw_circle(
        m->corpo.x,
        m->corpo.y,
        m->alcanceAudicao,
        al_map_rgba(70, 135, 255, 140),
        2.0f);

    float a1 =
        m->corpo.direcao -
        m->anguloVisao * 0.5f;

    float a2 =
        m->corpo.direcao +
        m->anguloVisao * 0.5f;

    ALLEGRO_COLOR amarelo =
        al_map_rgba(255, 220, 70, 190);

    al_draw_line(
        m->corpo.x,
        m->corpo.y,
        m->corpo.x + cosf(a1) * m->alcanceVisao,
        m->corpo.y + sinf(a1) * m->alcanceVisao,
        amarelo,
        2.0f);

    al_draw_line(
        m->corpo.x,
        m->corpo.y,
        m->corpo.x + cosf(a2) * m->alcanceVisao,
        m->corpo.y + sinf(a2) * m->alcanceVisao,
        amarelo,
        2.0f);

    al_draw_arc(
        m->corpo.x,
        m->corpo.y,
        m->alcanceVisao,
        a1,
        m->anguloVisao,
        amarelo,
        2.0f);

    al_draw_rectangle(
        f->areaJogavel.x,
        f->areaJogavel.y,
        f->areaJogavel.x + f->areaJogavel.largura,
        f->areaJogavel.y + f->areaJogavel.altura,
        al_map_rgb(80, 220, 255),
        2.0f);

    for (int i = 0;
         i < f->quantidadeObstaculos;
         i++)
    {
        const Obstaculo* o =
            &f->obstaculos[i];

        ALLEGRO_COLOR c =
            o->bloqueiaVisao ?
            al_map_rgb(255, 80, 80) :
            al_map_rgb(80, 255, 110);

        al_draw_rectangle(
            o->x,
            o->y,
            o->x + o->largura,
            o->y + o->altura,
            c,
            1.0f);
    }

    al_draw_rectangle(
        f->saida.x,
        f->saida.y,
        f->saida.x + f->saida.largura,
        f->saida.y + f->saida.altura,
        al_map_rgb(80, 255, 230),
        3.0f);

    if (som && som->ativo)
    {
        al_draw_circle(
            som->x,
            som->y,
            som->alcance,
            al_map_rgba(255, 80, 80, 100),
            2.0f);
    }

    for (int i = m->indiceCaminho;
         i < m->quantidadeCaminho;
         i++)
    {
        al_draw_filled_circle(
            m->caminho[i].x,
            m->caminho[i].y,
            3.0f,
            al_map_rgb(190, 80, 255));
    }
}

void desenharCena(
    const Fase* f,
    const RecursosMapa* r,
    const Scooby* s,
    const Maria* m,
    const Bola* b,
    const EventoSom* som,
    bool debug,
    int vidas,
    int faseAtual,
    EstadoJogo estado,
    float alphaFade,
    float tempoTutorial)
{
    if (!f || !r || !s || !m || !b)
        return;

    al_clear_to_color(
        al_map_rgb(22, 20, 22));

    if (f->fundo)
    {
        al_draw_scaled_bitmap(
            f->fundo,
            0, 0,
            al_get_bitmap_width(f->fundo),
            al_get_bitmap_height(f->fundo),
            MAPA_X,
            MAPA_Y,
            MAPA_TELA_W,
            MAPA_TELA_H,
            0);
    }

    if (s->carregandoBola &&
        estado != JOGO_TRANSICAO_FASE)
    {
        desenharIndicadorSaida(
            f,
            (float)al_get_time());
    }

    ItemRender itens[MAX_OBJETOS + 3];
    int quantidade = 0;

    for (int i = 0;
         i < f->quantidadeObjetos;
         i++)
    {
        itens[quantidade++] =
            (ItemRender){
                ITEM_OBJETO,
                baseYObjeto(&f->objetos[i]),
                i
            };
    }

    if (!b->coletada)
    {
        itens[quantidade++] =
            (ItemRender){
                ITEM_BOLA,
                b->y + 10.0f,
                0
            };
    }

    itens[quantidade++] =
        (ItemRender){
            ITEM_SCOOBY,
            s->corpo.y,
            0
        };

    itens[quantidade++] =
        (ItemRender){
            ITEM_MARIA,
            m->corpo.y,
            0
        };

    ordenarItens(itens, quantidade);

    for (int i = 0; i < quantidade; i++)
    {
        switch (itens[i].tipo)
        {
            case ITEM_OBJETO:
                desenharObjeto(
                    f,
                    &f->objetos[itens[i].indice]);
                break;

            case ITEM_BOLA:
                desenharBola(r, b);
                break;

            case ITEM_SCOOBY:
                desenharScooby(s, b);
                break;

            case ITEM_MARIA:
                desenharMaria(m);
                break;
        }
    }

    if (debug)
        desenharDebug(f, m, som);

    desenharHUD(
        r, f, s, b,
        vidas, faseAtual,
        tempoTutorial,
        estado);

    if (alphaFade > 0.0f)
    {
        if (alphaFade > 255.0f)
            alphaFade = 255.0f;

        al_draw_filled_rectangle(
            0, 0,
            LARGURA_TELA,
            ALTURA_TELA,
            al_map_rgba(
                0, 0, 0,
                (unsigned char)alphaFade));
    }

    al_flip_display();
}

void desenharTelaFinal(
    EstadoJogo estado,
    const RecursosMapa* r)
{
    ALLEGRO_COLOR fundo =
        estado == JOGO_VITORIA ?
        al_map_rgb(45, 110, 70) :
        al_map_rgb(120, 45, 55);

    al_clear_to_color(fundo);

    float x = LARGURA_TELA * 0.5f;
    float y = ALTURA_TELA * 0.5f;

    if (estado == JOGO_VITORIA)
    {
        al_draw_filled_circle(
            x, y - 45,
            100,
            al_map_rgb(245, 205, 70));

        al_draw_filled_circle(
            x, y - 45,
            68,
            al_map_rgb(255, 230, 120));

        if (r && r->fonte)
        {
            al_draw_text(
                r->fonte,
                al_map_rgb(255, 255, 255),
                x, y + 90,
                ALLEGRO_ALIGN_CENTRE,
                "VOCE VENCEU!");
        }
    }
    else
    {
        al_draw_line(
            x - 70, y - 100,
            x + 70, y + 40,
            al_map_rgb(245, 220, 220),
            15);

        al_draw_line(
            x + 70, y - 100,
            x - 70, y + 40,
            al_map_rgb(245, 220, 220),
            15);

        if (r && r->fonte)
        {
            al_draw_text(
                r->fonte,
                al_map_rgb(255, 255, 255),
                x, y + 90,
                ALLEGRO_ALIGN_CENTRE,
                "GAME OVER");
        }
    }

    if (r && r->fonte)
    {
        al_draw_text(
            r->fonte,
            al_map_rgb(240, 240, 240),
            x, y + 125,
            ALLEGRO_ALIGN_CENTRE,
            "R - jogar novamente");

        al_draw_text(
            r->fonte,
            al_map_rgb(225, 225, 225),
            x, y + 150,
            ALLEGRO_ALIGN_CENTRE,
            "ESC - sair");
    }

    al_flip_display();
}

void desenharCarregando(
    ALLEGRO_DISPLAY* display,
    ALLEGRO_FONT* fonte)
{
    if (!display) return;

    al_set_target_backbuffer(display);
    al_clear_to_color(
        al_map_rgb(25, 22, 28));

    if (fonte)
    {
        al_draw_text(
            fonte,
            al_map_rgb(255, 235, 190),
            LARGURA_TELA * 0.5f,
            ALTURA_TELA * 0.5f,
            ALLEGRO_ALIGN_CENTRE,
            "Carregando...");
    }

    al_flip_display();
}
