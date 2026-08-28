#include "jogo.h"

static char g_raizRecursos[768] = "";
static bool g_raizInicializada = false;

bool inicializarRaizRecursos(void)
{
    if (g_raizInicializada)
        return true;

    const char* candidatos[] = {
        "",
        "../",
        "../../",
        "../../../",
        "../../../../",
        "../../../../../"
    };

    char teste[1024];

    for (int i = 0;
         i < (int)(sizeof(candidatos) / sizeof(candidatos[0]));
         i++)
    {
        snprintf(teste,
                 sizeof(teste),
                 "%smapa/cozinha.png",
                 candidatos[i]);

        if (!al_filename_exists(teste))
            continue;

        snprintf(g_raizRecursos,
                 sizeof(g_raizRecursos),
                 "%s",
                 candidatos[i]);

        g_raizInicializada = true;

        printf("Raiz de recursos: %s\n",
               g_raizRecursos[0] ? g_raizRecursos : "./");

        return true;
    }

    char* atual = al_get_current_directory();

    printf("ERRO: raiz de recursos nao encontrada%s%s\n",
           atual ? ": " : ".",
           atual ? atual : "");

    if (atual)
        al_free(atual);

    return false;
}

bool resolverCaminhoRecurso(const char* relativo,
                            char* saida,
                            size_t tamanho)
{
    if (!relativo ||
        !saida ||
        tamanho == 0 ||
        !inicializarRaizRecursos())
    {
        return false;
    }

    int n = snprintf(saida,
                     tamanho,
                     "%s%s",
                     g_raizRecursos,
                     relativo);

    return n > 0 && (size_t)n < tamanho;
}

ALLEGRO_BITMAP* carregarBitmapFlexivel(const char* caminho)
{
    char absoluto[1024];

    if (!resolverCaminhoRecurso(caminho,
                                absoluto,
                                sizeof(absoluto)))
    {
        return NULL;
    }

    if (!al_filename_exists(absoluto))
    {
        printf("ERRO asset ausente: %s\n", caminho);
        return NULL;
    }

    ALLEGRO_BITMAP* bmp = al_load_bitmap(absoluto);

    if (bmp)
        return bmp;

    /*
     * Algumas instalacoes Windows do projeto nao estao conseguindo
     * decodificar PNG pelo allegro_image. Nesse caso usamos WIC apenas
     * como decoder alternativo, sem mudar caminhos nem arquivos.
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

static ALLEGRO_BITMAP* carregarObrigatorio(const char* caminho)
{
    ALLEGRO_BITMAP* bitmap = carregarBitmapFlexivel(caminho);

    if (!bitmap)
        printf("ERRO imagem obrigatoria: %s\n", caminho);

    return bitmap;
}

bool carregarRecursosMapa(RecursosMapa* recursos)
{
    if (!recursos)
        return false;

    memset(recursos, 0, sizeof(*recursos));
    recursos->faseCarregada = -1;

    if (!inicializarRaizRecursos())
        return false;

    recursos->bolas =
        carregarObrigatorio("ScoobySprites/littleBalls/balls.png");

    recursos->fonte = al_create_builtin_font();

    if (!recursos->fonte)
        printf("ERRO ao criar fonte interna.\n");

    return recursos->bolas && recursos->fonte;
}

void descarregarFase(Fase* fase)
{
    if (!fase)
        return;

    if (fase->fundo)
    {
        al_destroy_bitmap(fase->fundo);
        fase->fundo = NULL;
    }

    if (fase->folhaObjetos)
    {
        al_destroy_bitmap(fase->folhaObjetos);
        fase->folhaObjetos = NULL;
    }
}

bool carregarRecursosFase(RecursosMapa* recursos,
                          Fase fases[QTD_FASES],
                          int indiceFase)
{
    if (!recursos ||
        !fases ||
        indiceFase < 0 ||
        indiceFase >= QTD_FASES)
    {
        return false;
    }

    if (recursos->faseCarregada >= 0 &&
        recursos->faseCarregada < QTD_FASES &&
        recursos->faseCarregada != indiceFase)
    {
        descarregarFase(&fases[recursos->faseCarregada]);
    }

    Fase* fase = &fases[indiceFase];

    if (!fase->fundo)
        fase->fundo = carregarObrigatorio(fase->caminhoFundo);

    if (!fase->fundo)
        return false;

    if (!fase->folhaObjetos)
    {
        if (indiceFase == 3)
        {
            /*
             * IMPORTANTE:
             *
             * O layout fisico/visual atual da Fase 4 usa os source rectangles
             * definidos para a folha HD criada em quarto_assets.c.
             *
             * mapa/quarto_objetos.png possui outro atlas/layout. Carregar esse
             * PNG e aplicar os source rectangles da folha procedural fazia
             * exatamente o bug visto no jogo: fatias de cama, berco e armario
             * desenhadas em blocos separados.
             *
             * Portanto a Fase 4 usa UMA unica fonte visual coerente. Se no
             * futuro o PNG for adotado novamente, seus crops devem ser medidos
             * e configurados especificamente; nunca misturados com outro atlas.
             */
            fase->folhaObjetos = criarFolhaQuartoProcedural();

            if (fase->folhaObjetos)
            {
                printf("Quarto: atlas HD interno carregado (%dx%d).\n",
                       al_get_bitmap_width(fase->folhaObjetos),
                       al_get_bitmap_height(fase->folhaObjetos));
            }
        }
        else
        {
            fase->folhaObjetos =
                carregarObrigatorio(fase->caminhoObjetos);
        }
    }

    if (!fase->folhaObjetos)
        return false;

    recursos->faseCarregada = indiceFase;

    /*
     * Qualquer alteracao de layout ou de asset exige que a geometria
     * fisica seja reconstruida para permanecer sincronizada com a arte.
     */
    reconstruirColisoesFase(fase);

    if (!validarObjetosFase(fase))
    {
        printf("ERRO: %s possui source rectangles invalidos.\n",
               fase->nome);
        return false;
    }

    return true;
}

void destruirRecursos(RecursosMapa* recursos,
                      Fase fases[QTD_FASES],
                      Scooby* scooby,
                      Maria* maria)
{
    if (fases)
    {
        for (int i = 0; i < QTD_FASES; i++)
            descarregarFase(&fases[i]);
    }

    if (recursos)
    {
        if (recursos->bolas)
        {
            al_destroy_bitmap(recursos->bolas);
            recursos->bolas = NULL;
        }

        if (recursos->fonte)
        {
            al_destroy_font(recursos->fonte);
            recursos->fonte = NULL;
        }

        recursos->faseCarregada = -1;
    }

    if (scooby)
    {
        destruirAnimacaoInterna(&scooby->idle);
        destruirAnimacaoInterna(&scooby->walk);
        destruirAnimacaoInterna(&scooby->run);
        destruirAnimacaoInterna(&scooby->bark);
        destruirAnimacaoInterna(&scooby->bite);

        for (int i = 0; i < QTD_CORES_BOLA; i++)
            destruirAnimacaoInterna(&scooby->carregar[i]);
    }

    if (maria)
    {
        destruirAnimacaoInterna(&maria->idle);
        destruirAnimacaoInterna(&maria->walk);
        destruirAnimacaoInterna(&maria->run);
        destruirAnimacaoInterna(&maria->pick);
    }
}
