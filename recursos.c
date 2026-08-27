#ifdef _WIN32
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#endif

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

static bool escreverBase64ParaArquivo(const char* texto, const char* caminho)
{
    if (!texto || !caminho) return false;

    remove(caminho);

    FILE* arquivo = fopen(caminho, "wb");
    if (!arquivo) return false;

    int valores[4];
    int quantidade = 0;

    for (const char* p = texto; *p; p++)
    {
        if (*p == '=')
            valores[quantidade++] = -2;
        else
        {
            int v = valorBase64(*p);
            if (v < 0) continue;
            valores[quantidade++] = v;
        }

        if (quantidade == 4)
        {
            unsigned char b1 =
                (unsigned char)((valores[0] << 2) | (valores[1] >> 4));
            fwrite(&b1, 1, 1, arquivo);

            if (valores[2] != -2)
            {
                unsigned char b2 =
                    (unsigned char)(((valores[1] & 15) << 4) |
                                    (valores[2] >> 2));
                fwrite(&b2, 1, 1, arquivo);
            }

            if (valores[3] != -2 && valores[2] != -2)
            {
                unsigned char b3 =
                    (unsigned char)(((valores[2] & 3) << 6) |
                                    valores[3]);
                fwrite(&b3, 1, 1, arquivo);
            }

            quantidade = 0;
        }
    }

    fclose(arquivo);
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
        "../../../../../",
        "../../../../../../"
    };

    char teste[1024];

    for (int i = 0; i < (int)(sizeof(candidatos) / sizeof(candidatos[0])); i++)
    {
        snprintf(
            teste, sizeof(teste),
            "%smapa/cozinha.png",
            candidatos[i]);

        if (al_filename_exists(teste))
        {
            snprintf(
                g_raizRecursos,
                sizeof(g_raizRecursos),
                "%s",
                candidatos[i]);

            g_raizInicializada = true;

            printf(
                "Raiz de recursos: %s\n",
                g_raizRecursos[0] ? g_raizRecursos : "./");

            return true;
        }
    }

    char* atual = al_get_current_directory();
    if (atual)
    {
        printf(
            "Erro: nao foi possivel localizar a raiz de recursos. Diretorio atual: %s\n",
            atual);
        al_free(atual);
    }

    return false;
}

bool resolverCaminhoRecurso(
    const char* relativo,
    char* saida,
    size_t tamanho)
{
    if (!relativo || !saida || tamanho == 0)
        return false;

    if (!inicializarRaizRecursos())
        return false;

    int escritos = snprintf(
        saida,
        tamanho,
        "%s%s",
        g_raizRecursos,
        relativo);

    return escritos > 0 &&
           (size_t)escritos < tamanho;
}

#ifdef _WIN32
static ALLEGRO_BITMAP* carregarBitmapWIC(const char* caminho)
{
    if (!caminho) return NULL;

    wchar_t caminhoWide[1024];

    int convertido = MultiByteToWideChar(
        CP_UTF8, 0, caminho, -1,
        caminhoWide,
        (int)(sizeof(caminhoWide) / sizeof(caminhoWide[0])));

    if (convertido <= 0)
        return NULL;

    HRESULT hrCom =
        CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    bool deveFinalizarCom =
        SUCCEEDED(hrCom);

    IWICImagingFactory* factory = NULL;
    IWICBitmapDecoder* decoder = NULL;
    IWICBitmapFrameDecode* frame = NULL;
    IWICFormatConverter* converter = NULL;

    unsigned char* pixels = NULL;
    ALLEGRO_BITMAP* bitmap = NULL;
    ALLEGRO_LOCKED_REGION* lock = NULL;

    HRESULT hr = CoCreateInstance(
        &CLSID_WICImagingFactory,
        NULL,
        CLSCTX_INPROC_SERVER,
        &IID_IWICImagingFactory,
        (LPVOID*)&factory);

    if (FAILED(hr) || !factory)
        goto fim;

    hr = IWICImagingFactory_CreateDecoderFromFilename(
        factory,
        caminhoWide,
        NULL,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder);

    if (FAILED(hr) || !decoder)
        goto fim;

    hr = IWICBitmapDecoder_GetFrame(
        decoder, 0, &frame);

    if (FAILED(hr) || !frame)
        goto fim;

    hr = IWICImagingFactory_CreateFormatConverter(
        factory, &converter);

    if (FAILED(hr) || !converter)
        goto fim;

    hr = IWICFormatConverter_Initialize(
        converter,
        (IWICBitmapSource*)frame,
        &GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        NULL,
        0.0,
        WICBitmapPaletteTypeCustom);

    if (FAILED(hr))
        goto fim;

    UINT largura = 0;
    UINT altura = 0;

    hr = IWICFormatConverter_GetSize(
        converter,
        &largura,
        &altura);

    if (FAILED(hr) ||
        largura == 0 ||
        altura == 0 ||
        largura > 10000 ||
        altura > 10000)
    {
        goto fim;
    }

    UINT stride = largura * 4;
    UINT tamanhoPixels = stride * altura;

    pixels =
        (unsigned char*)malloc(tamanhoPixels);

    if (!pixels)
        goto fim;

    hr = IWICFormatConverter_CopyPixels(
        converter,
        NULL,
        stride,
        tamanhoPixels,
        pixels);

    if (FAILED(hr))
        goto fim;

    int flagsAntigas = al_get_new_bitmap_flags();
    al_set_new_bitmap_flags(ALLEGRO_MEMORY_BITMAP);

    bitmap = al_create_bitmap(
        (int)largura,
        (int)altura);

    al_set_new_bitmap_flags(flagsAntigas);

    if (!bitmap)
        goto fim;

    lock = al_lock_bitmap(
        bitmap,
        ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,
        ALLEGRO_LOCK_WRITEONLY);

    if (!lock)
    {
        al_destroy_bitmap(bitmap);
        bitmap = NULL;
        goto fim;
    }

    for (UINT y = 0; y < altura; y++)
    {
        unsigned char* origem =
            pixels + y * stride;

        unsigned char* destino =
            (unsigned char*)lock->data +
            y * lock->pitch;

        for (UINT x = 0; x < largura; x++)
        {
            unsigned char r = origem[x * 4 + 0];
            unsigned char g = origem[x * 4 + 1];
            unsigned char b = origem[x * 4 + 2];
            unsigned char a = origem[x * 4 + 3];

            destino[x * 4 + 0] =
                (unsigned char)((r * a + 127) / 255);
            destino[x * 4 + 1] =
                (unsigned char)((g * a + 127) / 255);
            destino[x * 4 + 2] =
                (unsigned char)((b * a + 127) / 255);
            destino[x * 4 + 3] = a;
        }
    }

    al_unlock_bitmap(bitmap);
    lock = NULL;

fim:
    if (lock && bitmap)
        al_unlock_bitmap(bitmap);

    free(pixels);

    if (converter) IWICFormatConverter_Release(converter);
    if (frame) IWICBitmapFrameDecode_Release(frame);
    if (decoder) IWICBitmapDecoder_Release(decoder);
    if (factory) IWICImagingFactory_Release(factory);

    if (deveFinalizarCom)
        CoUninitialize();

    return bitmap;
}
#endif

ALLEGRO_BITMAP* carregarBitmapFlexivel(const char* caminho)
{
    char absoluto[1024];

    if (!resolverCaminhoRecurso(
            caminho,
            absoluto,
            sizeof(absoluto)))
    {
        return NULL;
    }

    if (!al_filename_exists(absoluto))
    {
        printf(
            "Arquivo de imagem nao encontrado: %s\n",
            absoluto);
        return NULL;
    }

    ALLEGRO_BITMAP* bmp =
        al_load_bitmap(absoluto);

    if (bmp)
        return bmp;

#ifdef _WIN32
    bmp = carregarBitmapWIC(absoluto);

    if (bmp)
    {
        printf(
            "Imagem carregada pelo fallback WIC: %s\n",
            caminho);
        return bmp;
    }
#endif

    printf(
        "Arquivo existe, mas nao foi possivel decodificar: %s\n",
        caminho);

    return NULL;
}

bool carregarAnimacao(
    Animacao* a,
    const char* caminho,
    float tempoFrame)
{
    if (!a) return false;

    memset(a, 0, sizeof(*a));

    a->imagem =
        carregarBitmapFlexivel(caminho);

    if (!a->imagem)
    {
        printf(
            "Erro ao carregar sprite: %s\n",
            caminho);
        return false;
    }

    int largura =
        al_get_bitmap_width(a->imagem);

    int altura =
        al_get_bitmap_height(a->imagem);

    if (largura <= 0 ||
        altura <= 0 ||
        largura % QTD_FRAMES != 0 ||
        altura % QTD_DIRECOES != 0)
    {
        printf(
            "Sprite sheet invalida: %s -> %dx%d. Esperado dimensoes divisiveis por %d frames e %d direcoes.\n",
            caminho,
            largura,
            altura,
            QTD_FRAMES,
            QTD_DIRECOES);

        al_destroy_bitmap(a->imagem);
        a->imagem = NULL;
        return false;
    }

    a->qtdFrames = QTD_FRAMES;
    a->qtdDirecoes = QTD_DIRECOES;
    a->frameW = largura / a->qtdFrames;
    a->frameH = altura / a->qtdDirecoes;

    if (a->frameW <= 0 || a->frameH <= 0)
    {
        printf(
            "Frames invalidos em %s: %dx%d\n",
            caminho,
            a->frameW,
            a->frameH);

        al_destroy_bitmap(a->imagem);
        a->imagem = NULL;
        return false;
    }

    a->frameAtual = 0;
    a->acumulador = 0.0f;
    a->tempoFrame = tempoFrame;
    a->anchorX = 0.50f;
    a->anchorY = 0.80f;
    a->ultimoFrameSom = -1;

    printf(
        "Sprite validada: %s | folha=%dx%d | frame=%dx%d\n",
        caminho,
        largura,
        altura,
        a->frameW,
        a->frameH);

    return true;
}

void reiniciarAnimacao(Animacao* a)
{
    if (!a) return;

    a->frameAtual = 0;
    a->acumulador = 0.0f;
    a->ultimoFrameSom = -1;
}

void atualizarAnimacaoLoop(
    Animacao* a,
    float dt)
{
    if (!a || !a->imagem ||
        a->qtdFrames <= 0 ||
        a->tempoFrame <= 0.0f)
    {
        return;
    }

    a->acumulador += dt;

    while (a->acumulador >= a->tempoFrame)
    {
        a->acumulador -= a->tempoFrame;
        a->frameAtual =
            (a->frameAtual + 1) %
            a->qtdFrames;
    }
}

bool atualizarAnimacaoUmaVez(
    Animacao* a,
    float dt)
{
    if (!a || !a->imagem)
        return true;

    a->acumulador += dt;

    if (a->acumulador >= a->tempoFrame)
    {
        a->acumulador -= a->tempoFrame;
        a->frameAtual++;

        if (a->frameAtual >= a->qtdFrames)
        {
            a->frameAtual = 0;
            a->acumulador = 0.0f;
            return true;
        }
    }

    return false;
}

void desenharAnimacao(
    const Animacao* a,
    Direcao direcao,
    float x,
    float y,
    float escala)
{
    if (!a || !a->imagem ||
        a->frameW <= 0 ||
        a->frameH <= 0)
    {
        return;
    }

    int frame = a->frameAtual;

    if (frame < 0) frame = 0;
    if (frame >= a->qtdFrames)
        frame = a->qtdFrames - 1;

    int linha = (int)direcao;

    if (linha < 0) linha = 0;
    if (linha >= a->qtdDirecoes)
        linha = a->qtdDirecoes - 1;

    int sx = frame * a->frameW;
    int sy = linha * a->frameH;

    float dw = a->frameW * escala;
    float dh = a->frameH * escala;

    al_draw_scaled_bitmap(
        a->imagem,
        sx, sy,
        a->frameW, a->frameH,
        x - dw * a->anchorX,
        y - dh * a->anchorY,
        dw, dh,
        0);
}

static ALLEGRO_BITMAP* carregarObrigatorio(
    const char* caminho)
{
    ALLEGRO_BITMAP* bmp =
        carregarBitmapFlexivel(caminho);

    if (!bmp)
        printf(
            "Erro ao carregar imagem obrigatoria: %s\n",
            caminho);

    return bmp;
}

bool carregarRecursosMapa(RecursosMapa* r)
{
    if (!r) return false;

    memset(r, 0, sizeof(*r));
    r->faseCarregada = -1;

    if (!inicializarRaizRecursos())
        return false;

    r->bolas =
        carregarObrigatorio(
            "ScoobySprites/littleBalls/balls.png");

    r->fonte =
        al_create_builtin_font();

    if (!r->fonte)
        printf(
            "Erro ao criar fonte interna do Allegro.\n");

    return r->bolas && r->fonte;
}

void descarregarFase(Fase* fase)
{
    if (!fase) return;

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

static ALLEGRO_BITMAP* carregarObjetosQuartoComFallback(
    const char* caminhoOriginal)
{
    ALLEGRO_BITMAP* bmp =
        carregarBitmapFlexivel(caminhoOriginal);

    if (bmp)
        return bmp;

    char runtime[1024];

    if (!resolverCaminhoRecurso(
            "quarto_objetos_runtime.png",
            runtime,
            sizeof(runtime)))
    {
        return NULL;
    }

    remove(runtime);

    if (!escreverBase64ParaArquivo(
            QUARTO_OBJETOS_BASE64,
            runtime))
    {
        printf(
            "Falha ao recriar fallback do quarto.\n");
        return NULL;
    }

#ifdef _WIN32
    bmp = carregarBitmapWIC(runtime);
#else
    bmp = al_load_bitmap(runtime);
#endif

    if (!bmp)
        printf(
            "Fallback do quarto foi recriado, mas ainda nao pode ser decodificado.\n");

    return bmp;
}

bool carregarRecursosFase(
    RecursosMapa* r,
    Fase fases[QTD_FASES],
    int indiceFase)
{
    if (!r || !fases ||
        indiceFase < 0 ||
        indiceFase >= QTD_FASES)
    {
        return false;
    }

    if (r->faseCarregada >= 0 &&
        r->faseCarregada < QTD_FASES &&
        r->faseCarregada != indiceFase)
    {
        descarregarFase(
            &fases[r->faseCarregada]);
    }

    Fase* f = &fases[indiceFase];

    if (!f->fundo)
        f->fundo =
            carregarObrigatorio(
                f->caminhoFundo);

    if (!f->folhaObjetos)
    {
        if (indiceFase == 3)
            f->folhaObjetos =
                carregarObjetosQuartoComFallback(
                    f->caminhoObjetos);
        else
            f->folhaObjetos =
                carregarObrigatorio(
                    f->caminhoObjetos);
    }

    if (!f->fundo ||
        !f->folhaObjetos)
    {
        return false;
    }

    r->faseCarregada = indiceFase;

    if (!validarObjetosFase(f))
        printf(
            "Aviso: %s possui recortes de objetos invalidos.\n",
            f->nome);

    return true;
}

bool carregarSprites(Scooby* s, Maria* m)
{
    if (!s || !m) return false;

    if (!carregarAnimacao(
            &s->idle,
            "ScoobySprites/idle.png",
            0.20f)) return false;

    if (!carregarAnimacao(
            &s->walk,
            "ScoobySprites/walk.png",
            0.12f)) return false;

    if (!carregarAnimacao(
            &s->run,
            "ScoobySprites/run.png",
            0.085f)) return false;

    if (!carregarAnimacao(
            &s->bark,
            "ScoobySprites/bark.png",
            0.085f)) return false;

    if (!carregarAnimacao(
            &s->bite,
            "ScoobySprites/bite.png",
            0.080f)) return false;

    static const char* carry[QTD_CORES_BOLA] = {
        "ScoobySprites/littleBalls/yellow_dog.png",
        "ScoobySprites/littleBalls/green_dog.png",
        "ScoobySprites/littleBalls/purple_dog.png",
        "ScoobySprites/littleBalls/blue_dog.png",
        "ScoobySprites/littleBalls/red_dog.png"
    };

    for (int i = 0; i < QTD_CORES_BOLA; i++)
    {
        if (!carregarAnimacao(
                &s->carregar[i],
                carry[i],
                0.14f))
        {
            return false;
        }
    }

    if (!carregarAnimacao(
            &m->idle,
            "mariaSprites/idle.png",
            0.20f)) return false;

    if (!carregarAnimacao(
            &m->walk,
            "mariaSprites/walk.png",
            0.13f)) return false;

    if (!carregarAnimacao(
            &m->run,
            "mariaSprites/run.png",
            0.095f)) return false;

    if (!carregarAnimacao(
            &m->pick,
            "mariaSprites/pick.png",
            0.11f)) return false;

    return true;
}

static void destruirAnimacao(Animacao* a)
{
    if (!a) return;

    if (a->imagem)
    {
        al_destroy_bitmap(a->imagem);
        a->imagem = NULL;
    }
}

void destruirRecursos(
    RecursosMapa* r,
    Fase fases[QTD_FASES],
    Scooby* s,
    Maria* m)
{
    if (fases)
    {
        for (int i = 0; i < QTD_FASES; i++)
            descarregarFase(&fases[i]);
    }

    if (r)
    {
        if (r->bolas)
        {
            al_destroy_bitmap(r->bolas);
            r->bolas = NULL;
        }

        if (r->fonte)
        {
            al_destroy_font(r->fonte);
            r->fonte = NULL;
        }

        r->faseCarregada = -1;
    }

    if (s)
    {
        destruirAnimacao(&s->idle);
        destruirAnimacao(&s->walk);
        destruirAnimacao(&s->run);
        destruirAnimacao(&s->bark);
        destruirAnimacao(&s->bite);

        for (int i = 0; i < QTD_CORES_BOLA; i++)
            destruirAnimacao(&s->carregar[i]);
    }

    if (m)
    {
        destruirAnimacao(&m->idle);
        destruirAnimacao(&m->walk);
        destruirAnimacao(&m->run);
        destruirAnimacao(&m->pick);
    }
}
