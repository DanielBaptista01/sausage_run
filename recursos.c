#include "jogo.h"

static char g_raizRecursos[768] = "";
static bool g_raizInicializada = false;

bool inicializarRaizRecursos(void)
{
    if (g_raizInicializada) return true;

    const char* candidatos[] = {
        "", "../", "../../", "../../../",
        "../../../../", "../../../../../"
    };

    char teste[1024];

    for (int i = 0;
         i < (int)(sizeof(candidatos) / sizeof(candidatos[0]));
         i++)
    {
        snprintf(teste, sizeof(teste),
                 "%smapa/cozinha.png", candidatos[i]);

        if (!al_filename_exists(teste))
            continue;

        snprintf(g_raizRecursos,
                 sizeof(g_raizRecursos),
                 "%s", candidatos[i]);

        g_raizInicializada = true;

        printf("Raiz de recursos: %s\n",
               g_raizRecursos[0] ? g_raizRecursos : "./");

        return true;
    }

    char* atual = al_get_current_directory();

    printf("ERRO: raiz de recursos nao encontrada%s%s\n",
           atual ? ": " : ".",
           atual ? atual : "");

    if (atual) al_free(atual);
    return false;
}

bool resolverCaminhoRecurso(const char* relativo,
                            char* saida,
                            size_t tamanho)
{
    if (!relativo || !saida || tamanho == 0 ||
        !inicializarRaizRecursos())
        return false;

    int n = snprintf(saida, tamanho,
                     "%s%s", g_raizRecursos, relativo);

    return n > 0 && (size_t)n < tamanho;
}

ALLEGRO_BITMAP* carregarBitmapFlexivel(const char* caminho)
{
    char absoluto[1024];

    if (!resolverCaminhoRecurso(
            caminho, absoluto, sizeof(absoluto)))
        return NULL;

    if (!al_filename_exists(absoluto))
    {
        printf("ERRO asset ausente: %s\n", caminho);
        return NULL;
    }

    ALLEGRO_BITMAP* bmp = al_load_bitmap(absoluto);
    if (bmp) return bmp;

    /*
     * O ambiente Windows do projeto já demonstrou não decodificar PNG
     * pelo allegro_image em algumas instalações. O WIC é somente um
     * decoder alternativo; caminhos e assets continuam os mesmos.
     */
    bmp = carregarBitmapWICSeguro(absoluto);

    if (bmp)
    {
        printf("WIC: %s\n", caminho);
        return bmp;
    }

    printf("ERRO asset existe mas nao decodifica: %s\n", caminho);
    return NULL;
}

static ALLEGRO_BITMAP* obrigatorio(const char* caminho)
{
    ALLEGRO_BITMAP* b = carregarBitmapFlexivel(caminho);

    if (!b)
        printf("ERRO imagem obrigatoria: %s\n", caminho);

    return b;
}

bool carregarRecursosMapa(RecursosMapa* r)
{
    if (!r) return false;

    memset(r, 0, sizeof(*r));
    r->faseCarregada = -1;

    if (!inicializarRaizRecursos())
        return false;

    r->bolas =
        obrigatorio("ScoobySprites/littleBalls/balls.png");

    r->fonte = al_create_builtin_font();

    if (!r->fonte)
        printf("ERRO ao criar fonte interna.\n");

    return r->bolas && r->fonte;
}

void descarregarFase(Fase* f)
{
    if (!f) return;

    if (f->fundo)
    {
        al_destroy_bitmap(f->fundo);
        f->fundo = NULL;
    }

    if (f->folhaObjetos)
    {
        al_destroy_bitmap(f->folhaObjetos);
        f->folhaObjetos = NULL;
    }
}

bool carregarRecursosFase(RecursosMapa* r,
                          Fase fases[QTD_FASES],
                          int indice)
{
    if (!r || !fases ||
        indice < 0 || indice >= QTD_FASES)
        return false;

    if (r->faseCarregada >= 0 &&
        r->faseCarregada < QTD_FASES &&
        r->faseCarregada != indice)
    {
        descarregarFase(&fases[r->faseCarregada]);
    }

    Fase* f = &fases[indice];

    if (!f->fundo)
        f->fundo = obrigatorio(f->caminhoFundo);

    if (!f->fundo)
        return false;

    if (!f->folhaObjetos)
    {
        if (indice == 3)
        {
            ALLEGRO_BITMAP* existente =
                carregarBitmapFlexivel(f->caminhoObjetos);

            /*
             * O arquivo antigo do quarto tem resolução extremamente baixa
             * e exigia escala ~12x. Não o ampliamos mais.
             */
            if (existente &&
                al_get_bitmap_width(existente) >= 512 &&
                al_get_bitmap_height(existente) >= 400)
            {
                f->folhaObjetos = existente;
            }
            else
            {
                if (existente)
                    al_destroy_bitmap(existente);

                printf(
                    "Quarto: sheet de baixa resolucao descartada; "
                    "usando composicao HD.\n");

                f->folhaObjetos =
                    criarFolhaQuartoProcedural();
            }
        }
        else
        {
            f->folhaObjetos =
                obrigatorio(f->caminhoObjetos);
        }
    }

    if (!f->folhaObjetos)
        return false;

    r->faseCarregada = indice;

    /* As colisões são reconstruídas depois de qualquer layout/asset. */
    reconstruirColisoesFase(f);

    if (!validarObjetosFase(f))
    {
        printf("ERRO: %s possui source rectangles invalidos.\n",
               f->nome);
        return false;
    }

    return true;
}

void destruirRecursos(RecursosMapa* r,
                      Fase fases[QTD_FASES],
                      Scooby* s,
                      Maria* m)
{
    if (fases)
        for (int i = 0; i < QTD_FASES; i++)
            descarregarFase(&fases[i]);

    if (r)
    {
        if (r->bolas)
            al_destroy_bitmap(r->bolas);

        if (r->fonte)
            al_destroy_font(r->fonte);

        r->bolas = NULL;
        r->fonte = NULL;
        r->faseCarregada = -1;
    }

    if (s)
    {
        destruirAnimacaoInterna(&s->idle);
        destruirAnimacaoInterna(&s->walk);
        destruirAnimacaoInterna(&s->run);
        destruirAnimacaoInterna(&s->bark);
        destruirAnimacaoInterna(&s->bite);

        for (int i = 0; i < QTD_CORES_BOLA; i++)
            destruirAnimacaoInterna(&s->carregar[i]);
    }

    if (m)
    {
        destruirAnimacaoInterna(&m->idle);
        destruirAnimacaoInterna(&m->walk);
        destruirAnimacaoInterna(&m->run);
        destruirAnimacaoInterna(&m->pick);
    }
}
