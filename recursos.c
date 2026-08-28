#include "jogo.h"
#include "quarto_objetos_data.h"

static char g_raizRecursos[768] = "";
static bool g_raizInicializada = false;

static int valorBase64(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static bool decodificarBase64ParaArquivo(const char* texto, const char* caminho)
{
    if (!texto || !caminho) return false;

    FILE* arq = fopen(caminho, "wb");
    if (!arq) return false;

    int valores[4];
    int qtd = 0;

    for (const char* p = texto; *p; ++p)
    {
        if (*p == '=')
            valores[qtd++] = -2;
        else
        {
            int v = valorBase64(*p);
            if (v < 0) continue;
            valores[qtd++] = v;
        }

        if (qtd == 4)
        {
            unsigned char b1 = (unsigned char)((valores[0] << 2) | (valores[1] >> 4));
            fwrite(&b1, 1, 1, arq);

            if (valores[2] != -2)
            {
                unsigned char b2 = (unsigned char)(((valores[1] & 15) << 4) | (valores[2] >> 2));
                fwrite(&b2, 1, 1, arq);
            }

            if (valores[2] != -2 && valores[3] != -2)
            {
                unsigned char b3 = (unsigned char)(((valores[2] & 3) << 6) | valores[3]);
                fwrite(&b3, 1, 1, arq);
            }

            qtd = 0;
        }
    }

    fclose(arq);
    return true;
}

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

    for (int i = 0; i < (int)(sizeof(candidatos) / sizeof(candidatos[0])); i++)
    {
        snprintf(teste, sizeof(teste), "%smapa/cozinha.png", candidatos[i]);

        if (!al_filename_exists(teste))
            continue;

        snprintf(g_raizRecursos, sizeof(g_raizRecursos), "%s", candidatos[i]);
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

bool resolverCaminhoRecurso(const char* relativo, char* saida, size_t tamanho)
{
    if (!relativo || !saida || tamanho == 0 || !inicializarRaizRecursos())
        return false;

    int n = snprintf(saida, tamanho, "%s%s", g_raizRecursos, relativo);
    return n > 0 && (size_t)n < tamanho;
}

ALLEGRO_BITMAP* carregarBitmapFlexivel(const char* caminho)
{
    char absoluto[1024];

    if (!resolverCaminhoRecurso(caminho, absoluto, sizeof(absoluto)))
        return NULL;

    if (!al_filename_exists(absoluto))
    {
        printf("ERRO asset ausente: %s\n", caminho);
        return NULL;
    }

    ALLEGRO_BITMAP* bmp = al_load_bitmap(absoluto);
    if (bmp)
        return bmp;

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

static ALLEGRO_BITMAP* carregarObjetosQuarto(void)
{
    /*
     * Regra do adendo:
     * 1) o PNG real SEMPRE tem prioridade;
     * 2) o fallback so existe se o PNG estiver ausente ou invalido;
     * 3) o fallback reproduz o MESMO atlas de quarto_objetos.png, portanto
     *    usa os mesmos source rectangles de fase.c e nunca formas simplificadas.
     */
    ALLEGRO_BITMAP* real = carregarBitmapFlexivel("mapa/quarto_objetos.png");

    if (real)
    {
        int w = al_get_bitmap_width(real);
        int h = al_get_bitmap_height(real);
        printf("Quarto: usando mapa/quarto_objetos.png real (%dx%d).\n", w, h);
        return real;
    }

    char runtime[1024];
    if (!resolverCaminhoRecurso("quarto_objetos_runtime.png", runtime, sizeof(runtime)))
        return NULL;

    printf("WARN Quarto: PNG real indisponivel; gerando fallback equivalente do atlas original.\n");

    if (!decodificarBase64ParaArquivo(QUARTO_OBJETOS_BASE64, runtime))
    {
        printf("ERRO Quarto: nao foi possivel gerar fallback base64.\n");
        return NULL;
    }

    ALLEGRO_BITMAP* fallback = al_load_bitmap(runtime);
    if (!fallback)
        fallback = carregarBitmapWICSeguro(runtime);

    if (fallback)
    {
        printf("Quarto: fallback original carregado (%dx%d).\n",
               al_get_bitmap_width(fallback),
               al_get_bitmap_height(fallback));
    }

    return fallback;
}

bool carregarRecursosMapa(RecursosMapa* recursos)
{
    if (!recursos)
        return false;

    memset(recursos, 0, sizeof(*recursos));
    recursos->faseCarregada = -1;

    if (!inicializarRaizRecursos())
        return false;

    recursos->bolas = carregarObrigatorio("ScoobySprites/littleBalls/balls.png");
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
    if (!recursos || !fases || indiceFase < 0 || indiceFase >= QTD_FASES)
        return false;

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
            fase->folhaObjetos = carregarObjetosQuarto();
        else
            fase->folhaObjetos = carregarObrigatorio(fase->caminhoObjetos);
    }

    if (!fase->folhaObjetos)
        return false;

    recursos->faseCarregada = indiceFase;

    reconstruirColisoesFase(fase);

    if (!validarObjetosFase(fase))
    {
        printf("ERRO: %s possui source rectangles invalidos.\n", fase->nome);
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
