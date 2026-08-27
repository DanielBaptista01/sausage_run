#ifdef _WIN32
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#endif

#include "jogo.h"
#include "quarto_objetos_data.h"

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
    FILE* teste = fopen(caminho, "rb");
    if (teste) { fclose(teste); return true; }

    FILE* arquivo = fopen(caminho, "wb");
    if (!arquivo) return false;

    int valores[4];
    int quantidade = 0;

    for (const char* p = texto; *p; p++)
    {
        if (*p == '=') valores[quantidade++] = -2;
        else
        {
            int v = valorBase64(*p);
            if (v < 0) continue;
            valores[quantidade++] = v;
        }

        if (quantidade == 4)
        {
            unsigned char b1 = (unsigned char)((valores[0] << 2) | (valores[1] >> 4));
            fwrite(&b1, 1, 1, arquivo);

            if (valores[2] != -2)
            {
                unsigned char b2 = (unsigned char)(((valores[1] & 15) << 4) | (valores[2] >> 2));
                fwrite(&b2, 1, 1, arquivo);
            }

            if (valores[3] != -2 && valores[2] != -2)
            {
                unsigned char b3 = (unsigned char)(((valores[2] & 3) << 6) | valores[3]);
                fwrite(&b3, 1, 1, arquivo);
            }

            quantidade = 0;
        }
    }

    fclose(arquivo);
    return true;
}

/*
 * O Visual Studio normalmente executa o jogo em x64/Debug.
 * Os assets ficam na raiz do repositorio. Localizamos essa raiz antes
 * de carregar qualquer imagem.
 */
static bool prepararDiretorioRecursos(void)
{
    if (al_filename_exists("mapa/cozinha.png"))
        return true;

    const char* candidatos[] = { "..", "../..", "../../..", "../../../.." };
    char teste[768];

    for (int i = 0; i < 4; i++)
    {
        snprintf(teste, sizeof(teste), "%s/mapa/cozinha.png", candidatos[i]);

        if (al_filename_exists(teste) && al_change_directory(candidatos[i]))
        {
            printf("Diretorio de recursos localizado automaticamente em: %s\n", candidatos[i]);
            return true;
        }
    }

    char* atual = al_get_current_directory();
    if (atual)
    {
        printf("Erro: nao encontrei a pasta de recursos. Diretorio atual: %s\n", atual);
        al_free(atual);
    }
    else
    {
        printf("Erro: nao encontrei a pasta de recursos do jogo.\n");
    }

    printf("Esperado encontrar mapa/cozinha.png na raiz do repositorio.\n");
    return false;
}

#ifdef _WIN32
/*
 * Fallback nativo do Windows para PNG/JPG usando Windows Imaging Component.
 * Isso evita depender do codec PNG da instalacao do allegro_image.
 * A saida e copiada para um ALLEGRO_BITMAP em RGBA premultiplicado.
 */
static ALLEGRO_BITMAP* carregarBitmapWIC(const char* caminho)
{
    wchar_t caminhoWide[1024];
    int convertido = MultiByteToWideChar(CP_UTF8, 0, caminho, -1,
                                          caminhoWide,
                                          (int)(sizeof(caminhoWide) / sizeof(caminhoWide[0])));
    if (convertido <= 0)
        return NULL;

    HRESULT hrCom = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    bool deveFinalizarCom = SUCCEEDED(hrCom);

    IWICImagingFactory* factory = NULL;
    IWICBitmapDecoder* decoder = NULL;
    IWICBitmapFrameDecode* frame = NULL;
    IWICFormatConverter* converter = NULL;
    unsigned char* pixels = NULL;
    ALLEGRO_BITMAP* bitmap = NULL;
    ALLEGRO_LOCKED_REGION* lock = NULL;

    HRESULT hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL,
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

    hr = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
    if (FAILED(hr) || !frame)
        goto fim;

    hr = IWICImagingFactory_CreateFormatConverter(factory, &converter);
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

    hr = IWICFormatConverter_GetSize(converter, &largura, &altura);
    if (FAILED(hr) || largura == 0 || altura == 0)
        goto fim;

    if (largura > 10000 || altura > 10000)
        goto fim;

    UINT stride = largura * 4;
    UINT tamanho = stride * altura;

    pixels = (unsigned char*)malloc(tamanho);
    if (!pixels)
        goto fim;

    hr = IWICFormatConverter_CopyPixels(converter, NULL, stride, tamanho, pixels);
    if (FAILED(hr))
        goto fim;

    bitmap = al_create_bitmap((int)largura, (int)altura);
    if (!bitmap)
        goto fim;

    lock = al_lock_bitmap(bitmap,
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
        unsigned char* origem = pixels + y * stride;
        unsigned char* destino = (unsigned char*)lock->data + y * lock->pitch;

        for (UINT x = 0; x < largura; x++)
        {
            unsigned char r = origem[x * 4 + 0];
            unsigned char g = origem[x * 4 + 1];
            unsigned char b = origem[x * 4 + 2];
            unsigned char a = origem[x * 4 + 3];

            /* Allegro usa alpha premultiplicado por padrao. */
            destino[x * 4 + 0] = (unsigned char)((r * a + 127) / 255);
            destino[x * 4 + 1] = (unsigned char)((g * a + 127) / 255);
            destino[x * 4 + 2] = (unsigned char)((b * a + 127) / 255);
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
    const char* prefixos[] = { "", "../", "../../", "../../../" };
    char tentativa[768];

    for (int i = 0; i < 4; i++)
    {
        snprintf(tentativa, sizeof(tentativa), "%s%s", prefixos[i], caminho);

        if (!al_filename_exists(tentativa))
            continue;

        /* Primeiro tentamos o carregador normal do Allegro. */
        ALLEGRO_BITMAP* bmp = al_load_bitmap(tentativa);
        if (bmp)
            return bmp;

#ifdef _WIN32
        /* Se o codec PNG do allegro_image estiver indisponivel, usa WIC. */
        bmp = carregarBitmapWIC(tentativa);
        if (bmp)
        {
            printf("Imagem carregada pelo fallback WIC: %s\n", tentativa);
            return bmp;
        }
#endif

        printf("Arquivo existe, mas nao foi possivel decodificar: %s\n", tentativa);
    }

    return NULL;
}

bool carregarAnimacao(Animacao* a, const char* caminho, float tempoFrame)
{
    a->imagem = carregarBitmapFlexivel(caminho);
    a->frameAtual = 0;
    a->acumulador = 0.0f;
    a->tempoFrame = tempoFrame;

    if (!a->imagem)
    {
        printf("Erro ao carregar sprite: %s\n", caminho);
        return false;
    }

    return true;
}

void reiniciarAnimacao(Animacao* a)
{
    a->frameAtual = 0;
    a->acumulador = 0.0f;
}

void atualizarAnimacaoLoop(Animacao* a, float dt)
{
    if (!a->imagem) return;

    a->acumulador += dt;

    while (a->acumulador >= a->tempoFrame)
    {
        a->acumulador -= a->tempoFrame;
        a->frameAtual = (a->frameAtual + 1) % QTD_FRAMES;
    }
}

bool atualizarAnimacaoUmaVez(Animacao* a, float dt)
{
    if (!a->imagem) return true;

    a->acumulador += dt;

    if (a->acumulador >= a->tempoFrame)
    {
        a->acumulador -= a->tempoFrame;
        a->frameAtual++;

        if (a->frameAtual >= QTD_FRAMES)
        {
            a->frameAtual = 0;
            a->acumulador = 0.0f;
            return true;
        }
    }

    return false;
}

void desenharAnimacao(const Animacao* a, Direcao direcao, float x, float y, float escala)
{
    if (!a->imagem) return;

    int sx = a->frameAtual * FRAME_SPRITE;
    int sy = (int)direcao * FRAME_SPRITE;
    float dw = FRAME_SPRITE * escala;
    float dh = FRAME_SPRITE * escala;

    al_draw_scaled_bitmap(
        a->imagem,
        sx, sy,
        FRAME_SPRITE, FRAME_SPRITE,
        x - dw / 2.0f,
        y - dh * 0.78f,
        dw, dh,
        0);
}

static ALLEGRO_BITMAP* obrigatorio(const char* caminho)
{
    ALLEGRO_BITMAP* bmp = carregarBitmapFlexivel(caminho);

    if (!bmp)
        printf("Erro ao carregar imagem: %s\n", caminho);

    return bmp;
}

bool carregarRecursosMapa(RecursosMapa* r)
{
    if (!prepararDiretorioRecursos())
        return false;

    r->fundos[0] = obrigatorio("mapa/cozinha.png");
    r->fundos[1] = obrigatorio("mapa/sala.png");
    r->fundos[2] = obrigatorio("mapa/banheiro.png");
    r->fundos[3] = obrigatorio("mapa/quarto.png");

    r->folhasObjetos[0] = obrigatorio("mapa/cozinha_objetos.png");
    r->folhasObjetos[1] = obrigatorio("mapa/sala_objetos.png");
    r->folhasObjetos[2] = obrigatorio("mapa/banheiro_objetos.png");
    r->folhasObjetos[3] = carregarBitmapFlexivel("mapa/quarto_objetos.png");

    if (!r->folhasObjetos[3])
    {
        if (decodificarBase64ParaArquivo(QUARTO_OBJETOS_BASE64, "quarto_objetos_runtime.png"))
            r->folhasObjetos[3] = carregarBitmapFlexivel("quarto_objetos_runtime.png");
    }

    r->bolas = obrigatorio("ScoobySprites/littleBalls/balls.png");

    for (int i = 0; i < QTD_FASES; i++)
        if (!r->fundos[i]) return false;

    return r->folhasObjetos[0] &&
           r->folhasObjetos[1] &&
           r->folhasObjetos[2] &&
           r->bolas;
}

bool carregarSprites(Scooby* s, Maria* m)
{
    if (!carregarAnimacao(&s->idle, "ScoobySprites/idle.png", 0.20f)) return false;
    if (!carregarAnimacao(&s->walk, "ScoobySprites/walk.png", 0.12f)) return false;
    if (!carregarAnimacao(&s->run, "ScoobySprites/run.png", 0.085f)) return false;
    if (!carregarAnimacao(&s->bark, "ScoobySprites/bark.png", 0.085f)) return false;
    if (!carregarAnimacao(&s->bite, "ScoobySprites/bite.png", 0.080f)) return false;

    /* Ordem: amarelo, verde, roxo, azul, vermelho. */
    static const char* carry[QTD_CORES_BOLA] = {
        "ScoobySprites/littleBalls/yellow_dog.png",
        "ScoobySprites/littleBalls/green_dog.png",
        "ScoobySprites/littleBalls/purple_dog.png",
        "ScoobySprites/littleBalls/blue_dog.png",
        "ScoobySprites/littleBalls/red_dog.png"
    };

    for (int i = 0; i < QTD_CORES_BOLA; i++)
        if (!carregarAnimacao(&s->carregar[i], carry[i], 0.14f)) return false;

    if (!carregarAnimacao(&m->idle, "mariaSprites/idle.png", 0.20f)) return false;
    if (!carregarAnimacao(&m->walk, "mariaSprites/walk.png", 0.13f)) return false;
    if (!carregarAnimacao(&m->run, "mariaSprites/run.png", 0.095f)) return false;
    if (!carregarAnimacao(&m->pick, "mariaSprites/pick.png", 0.11f)) return false;

    return true;
}

static void destruirAnimacao(Animacao* a)
{
    if (a->imagem)
    {
        al_destroy_bitmap(a->imagem);
        a->imagem = NULL;
    }
}

void destruirRecursos(RecursosMapa* r, Scooby* s, Maria* m)
{
    for (int i = 0; i < QTD_FASES; i++)
    {
        if (r->fundos[i])
        {
            al_destroy_bitmap(r->fundos[i]);
            r->fundos[i] = NULL;
        }

        if (r->folhasObjetos[i])
        {
            al_destroy_bitmap(r->folhasObjetos[i]);
            r->folhasObjetos[i] = NULL;
        }
    }

    if (r->bolas)
    {
        al_destroy_bitmap(r->bolas);
        r->bolas = NULL;
    }

    destruirAnimacao(&s->idle);
    destruirAnimacao(&s->walk);
    destruirAnimacao(&s->run);
    destruirAnimacao(&s->bark);
    destruirAnimacao(&s->bite);

    for (int i = 0; i < QTD_CORES_BOLA; i++)
        destruirAnimacao(&s->carregar[i]);

    destruirAnimacao(&m->idle);
    destruirAnimacao(&m->walk);
    destruirAnimacao(&m->run);
    destruirAnimacao(&m->pick);
}
