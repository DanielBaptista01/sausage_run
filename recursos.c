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

    int valores[4], qtd = 0;
    for (const char* p = texto; *p; ++p)
    {
        if (*p == '=') valores[qtd++] = -2;
        else
        {
            int v = valorBase64(*p);
            if (v < 0) continue;
            valores[qtd++] = v;
        }

        if (qtd == 4)
        {
            unsigned char b1=(unsigned char)((valores[0]<<2)|(valores[1]>>4));
            fwrite(&b1,1,1,arq);
            if(valores[2]!=-2)
            {
                unsigned char b2=(unsigned char)(((valores[1]&15)<<4)|(valores[2]>>2));
                fwrite(&b2,1,1,arq);
            }
            if(valores[2]!=-2&&valores[3]!=-2)
            {
                unsigned char b3=(unsigned char)(((valores[2]&3)<<6)|valores[3]);
                fwrite(&b3,1,1,arq);
            }
            qtd=0;
        }
    }
    fclose(arq);
    return true;
}

bool inicializarRaizRecursos(void)
{
    if(g_raizInicializada)return true;
    const char* candidatos[]={"","../","../../","../../../","../../../../","../../../../../"};
    char teste[1024];

    for(int i=0;i<(int)(sizeof(candidatos)/sizeof(candidatos[0]));i++)
    {
        snprintf(teste,sizeof(teste),"%smapa/cozinha.png",candidatos[i]);
        if(!al_filename_exists(teste))continue;
        snprintf(g_raizRecursos,sizeof(g_raizRecursos),"%s",candidatos[i]);
        g_raizInicializada=true;
        printf("Raiz de recursos: %s\n",g_raizRecursos[0]?g_raizRecursos:"./");
        return true;
    }

    char* atual=al_get_current_directory();
    printf("ERRO: raiz de recursos nao encontrada%s%s\n",atual?": ":".",atual?atual:"");
    if(atual)al_free(atual);
    return false;
}

bool resolverCaminhoRecurso(const char* relativo,char* saida,size_t tamanho)
{
    if(!relativo||!saida||tamanho==0||!inicializarRaizRecursos())return false;
    int n=snprintf(saida,tamanho,"%s%s",g_raizRecursos,relativo);
    return n>0&&(size_t)n<tamanho;
}

/*
 * As sprite sheets dos personagens sao analisadas pixel a pixel em
 * animacao.c para calcular os source rectangles e eliminar vazamento entre
 * celulas. Quando elas eram carregadas como VIDEO_BITMAP, cada al_lock_bitmap
 * forçava leitura/sincronizacao da textura com a GPU/Direct3D. Em Debug isso
 * podia deixar a aplicacao dezenas de segundos parada em "Carregando recursos".
 *
 * Carregamos apenas as folhas de personagens como MEMORY_BITMAP durante essa
 * etapa de preprocessamento. Antes de a primeira fase ser carregada,
 * carregarRecursosFase() chama al_convert_memory_bitmaps(), convertendo-as de
 * volta para o formato otimizado do display. Assim mantemos exatamente o mesmo
 * algoritmo de recorte, sem sacrificar o desempenho durante a gameplay.
 */
static bool recursoEhSpritePersonagem(const char* caminho)
{
    if(!caminho)return false;
    return strncmp(caminho,"ScoobySprites/",14)==0 ||
           strncmp(caminho,"mariaSprites/",13)==0;
}

ALLEGRO_BITMAP* carregarBitmapFlexivel(const char* caminho)
{
    char absoluto[1024];
    if(!resolverCaminhoRecurso(caminho,absoluto,sizeof(absoluto)))return NULL;
    if(!al_filename_exists(absoluto)){printf("ERRO asset ausente: %s\n",caminho);return NULL;}

    bool memoriaTemporaria=recursoEhSpritePersonagem(caminho);
    int flagsAnteriores=al_get_new_bitmap_flags();
    if(memoriaTemporaria)
        al_set_new_bitmap_flags(flagsAnteriores|ALLEGRO_MEMORY_BITMAP);

    ALLEGRO_BITMAP* bmp=al_load_bitmap(absoluto);
    if(bmp)
    {
        if(memoriaTemporaria)al_set_new_bitmap_flags(flagsAnteriores);
        return bmp;
    }

    bmp=carregarBitmapWICSeguro(absoluto);
    if(memoriaTemporaria)al_set_new_bitmap_flags(flagsAnteriores);
    if(bmp){printf("WIC: %s\n",caminho);return bmp;}

    printf("ERRO asset existe mas nao decodifica: %s\n",caminho);
    return NULL;
}

static ALLEGRO_BITMAP* carregarObrigatorio(const char* caminho)
{
    ALLEGRO_BITMAP* bitmap=carregarBitmapFlexivel(caminho);
    if(!bitmap)printf("ERRO imagem obrigatoria: %s\n",caminho);
    return bitmap;
}

static ALLEGRO_BITMAP* carregarObjetosQuarto(void)
{
    /* O PNG real sempre tem prioridade. */
    ALLEGRO_BITMAP* real=carregarBitmapFlexivel("mapa/quarto_objetos.png");
    if(real)
    {
        printf("Quarto: usando mapa/quarto_objetos.png real (%dx%d).\n",
               al_get_bitmap_width(real),al_get_bitmap_height(real));
        return real;
    }

    char runtime[1024];
    if(!resolverCaminhoRecurso("quarto_objetos_runtime.png",runtime,sizeof(runtime)))return NULL;

    printf("WARN Quarto: PNG real indisponivel; usando fallback original apenas por contingencia.\n");
    if(!decodificarBase64ParaArquivo(QUARTO_OBJETOS_BASE64,runtime))return NULL;

    ALLEGRO_BITMAP* fallback=al_load_bitmap(runtime);
    if(!fallback)fallback=carregarBitmapWICSeguro(runtime);
    if(fallback)
        printf("Quarto: fallback carregado (%dx%d).\n",al_get_bitmap_width(fallback),al_get_bitmap_height(fallback));
    return fallback;
}

/*
 * O primeiro atlas do quarto tinha 100x75 e definiu o layout relativo dos
 * sprites. O PNG definitivo preserva esse layout, mas em resolucao maior.
 *
 * fase.c guarda os crops na grade de referencia 100x75. Assim que o PNG real
 * e carregado, convertemos cada source rectangle para a resolucao real e
 * dividimos a escala visual pelo mesmo fator. O tamanho na tela permanece
 * igual, mas os pixels usados passam a ser os sprites HD do PNG definitivo.
 *
 * Isto corrige a causa do bug em que crops 32x28 eram aplicados diretamente
 * sobre uma folha grande e mostravam apenas pequenos fragmentos dos moveis.
 */
static void adaptarAtlasQuarto(Fase* fase)
{
    if(!fase||!fase->folhaObjetos||!fase->caminhoObjetos||
       strcmp(fase->caminhoObjetos,"mapa/quarto_objetos.png")!=0)return;

    const int REF_W=100,REF_H=75;
    int w=al_get_bitmap_width(fase->folhaObjetos);
    int h=al_get_bitmap_height(fase->folhaObjetos);

    if(w<=0||h<=0)return;
    if(w==REF_W&&h==REF_H)
    {
        printf("Quarto: atlas fallback 100x75; crops de referencia usados sem conversao.\n");
        return;
    }

    /* Se algum crop ja ultrapassa a grade 100x75, a fase ja foi adaptada em
       uma carga anterior (por exemplo, ao reiniciar a partida). */
    bool aindaReferencia=true;
    for(int i=0;i<fase->quantidadeObjetos;i++)
    {
        const ObjetoMapa* o=&fase->objetos[i];
        if(o->visualId<0)continue;
        if(o->sx+o->sw>REF_W||o->sy+o->sh>REF_H){aindaReferencia=false;break;}
    }
    if(!aindaReferencia)return;

    float fx=(float)w/(float)REF_W;
    float fy=(float)h/(float)REF_H;
    float fator=(fx+fy)*.5f;

    if(fabsf(fx-fy)>.08f*fator)
        printf("WARN Quarto: atlas mudou proporcao: ref=100x75 real=%dx%d fx=%.3f fy=%.3f\n",w,h,fx,fy);

    for(int i=0;i<fase->quantidadeObjetos;i++)
    {
        ObjetoMapa* o=&fase->objetos[i];
        if(o->visualId<0)continue;

        int x0=(int)lroundf(o->sx*fx);
        int y0=(int)lroundf(o->sy*fy);
        int x1=(int)lroundf((o->sx+o->sw)*fx);
        int y1=(int)lroundf((o->sy+o->sh)*fy);

        o->sx=x0;o->sy=y0;o->sw=x1-x0;o->sh=y1-y0;
        if(fator>0.001f)o->escala/=fator;
    }

    printf("Quarto: crops 100x75 convertidos para atlas real %dx%d (fator %.3f).\n",w,h,fator);
}

bool carregarRecursosMapa(RecursosMapa* recursos)
{
    if(!recursos)return false;
    memset(recursos,0,sizeof(*recursos));recursos->faseCarregada=-1;
    if(!inicializarRaizRecursos())return false;

    recursos->bolas=carregarObrigatorio("ScoobySprites/littleBalls/balls.png");
    recursos->fonte=al_create_builtin_font();
    if(!recursos->fonte)printf("ERRO ao criar fonte interna.\n");
    return recursos->bolas&&recursos->fonte;
}

void descarregarFase(Fase* fase)
{
    if(!fase)return;
    if(fase->fundo){al_destroy_bitmap(fase->fundo);fase->fundo=NULL;}
    if(fase->folhaObjetos){al_destroy_bitmap(fase->folhaObjetos);fase->folhaObjetos=NULL;}
}

bool carregarRecursosFase(RecursosMapa* recursos,Fase fases[QTD_FASES],int indiceFase)
{
    if(!recursos||!fases||indiceFase<0||indiceFase>=QTD_FASES)return false;

    /*
     * Este e o primeiro ponto chamado depois de carregarSprites() na
     * inicializacao. Converte em lote as folhas temporariamente carregadas em
     * RAM para bitmaps do display, sem repetir a leitura pixel-a-pixel.
     */
    al_convert_memory_bitmaps();

    if(recursos->faseCarregada>=0&&recursos->faseCarregada<QTD_FASES&&recursos->faseCarregada!=indiceFase)
        descarregarFase(&fases[recursos->faseCarregada]);

    Fase* fase=&fases[indiceFase];
    if(!fase->fundo)fase->fundo=carregarObrigatorio(fase->caminhoFundo);
    if(!fase->fundo)return false;

    if(!fase->folhaObjetos)
    {
        if(indiceFase==3)fase->folhaObjetos=carregarObjetosQuarto();
        else fase->folhaObjetos=carregarObrigatorio(fase->caminhoObjetos);
    }
    if(!fase->folhaObjetos)return false;

    if(indiceFase==3)adaptarAtlasQuarto(fase);

    recursos->faseCarregada=indiceFase;
    reconstruirColisoesFase(fase);

    if(!validarObjetosFase(fase))
    {
        printf("ERRO: %s possui source rectangles invalidos.\n",fase->nome);
        return false;
    }
    return true;
}

void destruirRecursos(RecursosMapa* recursos,Fase fases[QTD_FASES],Scooby* scooby,Maria* maria)
{
    if(fases)for(int i=0;i<QTD_FASES;i++)descarregarFase(&fases[i]);

    if(recursos)
    {
        if(recursos->bolas){al_destroy_bitmap(recursos->bolas);recursos->bolas=NULL;}
        if(recursos->fonte){al_destroy_font(recursos->fonte);recursos->fonte=NULL;}
        recursos->faseCarregada=-1;
    }

    if(scooby)
    {
        destruirAnimacaoInterna(&scooby->idle);destruirAnimacaoInterna(&scooby->walk);
        destruirAnimacaoInterna(&scooby->run);destruirAnimacaoInterna(&scooby->bark);
        destruirAnimacaoInterna(&scooby->bite);
        for(int i=0;i<QTD_CORES_BOLA;i++)destruirAnimacaoInterna(&scooby->carregar[i]);
    }

    if(maria)
    {
        destruirAnimacaoInterna(&maria->idle);destruirAnimacaoInterna(&maria->walk);
        destruirAnimacaoInterna(&maria->run);destruirAnimacaoInterna(&maria->pick);
    }
}
