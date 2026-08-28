#include "jogo.h"

static const char* OPCOES_MENU_PRINCIPAL[QTD_ITENS_MENU_PRINCIPAL] = {
    "JOGAR",
    "TECLAS",
    "HIST\xC3\x93RIA",
    "DESENVOLVEDOR",
    "SAIR"
};

static const char* OPCOES_MENU_PAUSA[QTD_ITENS_MENU_PAUSA] = {
    "CONTINUE",
    "TECLAS",
    "REINICIAR",
    "RETORNAR AO MENU"
};

static float clamp01UI(float v)
{
    if(v<0.0f)return 0.0f;
    if(v>1.0f)return 1.0f;
    return v;
}

/*
 * A fonte builtin permanece como recurso global do projeto. Para dar peso
 * visual aos menus sem introduzir uma nova dependencia de TTF, o texto e
 * desenhado com uma transformacao temporaria. O backbuffer volta sempre para
 * a transformacao que estava ativa antes da chamada.
 */
static void textoEscalado(ALLEGRO_FONT* fonte,ALLEGRO_COLOR cor,float x,float y,
                          float escala,int flags,const char* texto)
{
    if(!fonte||!texto||escala<=0.0f)return;
    ALLEGRO_TRANSFORM anterior=*al_get_current_transform();
    ALLEGRO_TRANSFORM t;
    al_identity_transform(&t);
    al_scale_transform(&t,escala,escala);
    al_use_transform(&t);
    al_draw_text(fonte,cor,x/escala,y/escala,flags,texto);
    al_use_transform(&anterior);
}

static void painel(float x1,float y1,float x2,float y2,float raio,ALLEGRO_COLOR fundo,ALLEGRO_COLOR borda)
{
    al_draw_filled_rounded_rectangle(x1,y1,x2,y2,raio,raio,fundo);
    al_draw_rounded_rectangle(x1,y1,x2,y2,raio,raio,borda,2.0f);
}

static void desenharFundoPerseguicao(const Fase* fase,const Scooby* scooby,const Maria* maria,float tempo)
{
    al_clear_to_color(al_map_rgb(27,18,22));

    if(fase&&fase->fundo)
    {
        al_draw_scaled_bitmap(fase->fundo,0,0,
            al_get_bitmap_width(fase->fundo),al_get_bitmap_height(fase->fundo),
            MAPA_X,MAPA_Y,MAPA_TELA_W,MAPA_TELA_H,0);

        /* Os objetos deixam a arte com a mesma identidade das fases reais. */
        for(int i=0;i<fase->quantidadeObjetos;i++)
            desenharObjeto(fase,&fase->objetos[i]);
    }

    /* Escurece o cenario, preservando legibilidade dos elementos de UI. */
    al_draw_filled_rectangle(0,0,LARGURA_TELA,ALTURA_TELA,al_map_rgba(18,10,20,128));

    /* Linhas de velocidade fazem a composicao comunicar perseguicao/movimento. */
    for(int i=0;i<15;i++)
    {
        float faixa=(float)(i*91);
        float x=fmodf(faixa+tempo*235.0f,(float)LARGURA_TELA+180.0f)-120.0f;
        float y=105.0f+(float)((i*47)%500);
        float tam=42.0f+(float)((i*19)%72);
        al_draw_line(x,y,x+tam,y,al_map_rgba(255,235,210,72),2.0f);
    }

    /* Sombras ancoram os personagens no piso da composicao. */
    al_draw_filled_ellipse(790,566,78,16,al_map_rgba(0,0,0,95));
    al_draw_filled_ellipse(510,566,48,14,al_map_rgba(0,0,0,95));

    /* Maria vem atras e Scooby foge para a direita. */
    if(maria&&maria->run.imagem)
        desenharAnimacao(&maria->run,DIRECAO_RIGHT,510,572);
    if(scooby&&scooby->run.imagem)
        desenharAnimacao(&scooby->run,DIRECAO_RIGHT,790,572);

    /* Pequenas marcas de poeira reforcam a sensacao de corrida. */
    for(int i=0;i<5;i++)
    {
        float osc=sinf(tempo*5.0f+i)*4.0f;
        al_draw_filled_circle(690-i*22.0f,566+osc,4.0f+i*.6f,al_map_rgba(245,224,190,115));
    }
}

static void desenharTituloPrincipal(ALLEGRO_FONT* fonte)
{
    textoEscalado(fonte,al_map_rgb(255,224,92),640,55,5.0f,ALLEGRO_ALIGN_CENTRE,"SAUSAGE RUN");
    textoEscalado(fonte,al_map_rgb(255,245,228),640,108,1.75f,ALLEGRO_ALIGN_CENTRE,
                  "Pegue a bolinha. Engane a Maria. Corra para a saida!");
}

void desenharSplashUI(const RecursosMapa* recursos,const Fase* fase,
                      const Scooby* scooby,const Maria* maria,float tempo)
{
    ALLEGRO_FONT* fonte=recursos?recursos->fonte:NULL;
    desenharFundoPerseguicao(fase,scooby,maria,tempo);

    al_draw_filled_rectangle(0,0,LARGURA_TELA,158,al_map_rgba(14,8,16,185));
    desenharTituloPrincipal(fonte);

    painel(365,615,915,686,14,
           al_map_rgba(20,13,23,218),al_map_rgba(255,226,112,175));
    textoEscalado(fonte,al_map_rgb(255,244,223),640,626,1.65f,ALLEGRO_ALIGN_CENTRE,"CARREGANDO...");

    float progresso=clamp01UI(tempo/2.35f);
    al_draw_filled_rounded_rectangle(410,660,870,674,7,7,al_map_rgba(255,255,255,45));
    al_draw_filled_rounded_rectangle(410,660,410+460.0f*progresso,674,7,7,al_map_rgb(255,203,70));

    /* Fade curto de entrada e saida; a troca real de estado e controlada no main. */
    float alpha=0.0f;
    if(tempo<.38f)alpha=255.0f*(1.0f-tempo/.38f);
    else if(tempo>1.95f)alpha=255.0f*clamp01UI((tempo-1.95f)/.40f);
    if(alpha>0.0f)
        al_draw_filled_rectangle(0,0,LARGURA_TELA,ALTURA_TELA,
                                 al_map_rgba(0,0,0,(unsigned char)alpha));
    al_flip_display();
}

static void itemMenu(ALLEGRO_FONT* fonte,const char* texto,float x,float y,bool selecionado)
{
    if(selecionado)
    {
        float pulso=.55f+.45f*sinf((float)al_get_time()*5.0f);
        ALLEGRO_COLOR borda=al_map_rgba(255,226,94,(unsigned char)(155+90*pulso));
        al_draw_filled_rounded_rectangle(x-185,y-13,x+185,y+31,11,11,al_map_rgba(255,196,68,48));
        al_draw_rounded_rectangle(x-185,y-13,x+185,y+31,11,11,borda,3.0f);
        al_draw_filled_triangle(x-205,y+9,x-191,y-1,x-191,y+19,al_map_rgb(255,226,94));
    }
    textoEscalado(fonte,selecionado?al_map_rgb(255,235,124):al_map_rgb(235,228,230),
                  x,y,2.05f,ALLEGRO_ALIGN_CENTRE,texto);
}

void desenharMenuPrincipalUI(const RecursosMapa* recursos,const Fase* fase,
                             const Scooby* scooby,const Maria* maria,int selecionado,float tempo)
{
    ALLEGRO_FONT* fonte=recursos?recursos->fonte:NULL;
    desenharFundoPerseguicao(fase,scooby,maria,tempo);

    al_draw_filled_rectangle(0,0,LARGURA_TELA,146,al_map_rgba(14,8,16,178));
    desenharTituloPrincipal(fonte);

    painel(54,166,520,645,18,al_map_rgba(18,12,21,218),al_map_rgba(255,226,112,110));
    textoEscalado(fonte,al_map_rgb(255,242,220),287,188,1.55f,ALLEGRO_ALIGN_CENTRE,"MENU PRINCIPAL");

    for(int i=0;i<QTD_ITENS_MENU_PRINCIPAL;i++)
        itemMenu(fonte,OPCOES_MENU_PRINCIPAL[i],287,253+i*70.0f,i==selecionado);

    textoEscalado(fonte,al_map_rgba(235,229,235,210),287,606,1.25f,ALLEGRO_ALIGN_CENTRE,
                  "Setas: navegar   ENTER: confirmar");
    al_flip_display();
}

static void cabecalhoSubmenu(ALLEGRO_FONT* fonte,const char* titulo,const char* subtitulo)
{
    textoEscalado(fonte,al_map_rgb(255,222,91),640,86,3.6f,ALLEGRO_ALIGN_CENTRE,titulo);
    textoEscalado(fonte,al_map_rgb(235,228,235),640,132,1.45f,ALLEGRO_ALIGN_CENTRE,subtitulo);
}

static void prepararSubmenu(const RecursosMapa* recursos,const Fase* fase,
                            const Scooby* scooby,const Maria* maria,float tempo)
{
    desenharFundoPerseguicao(fase,scooby,maria,tempo);
    al_draw_filled_rectangle(0,0,LARGURA_TELA,ALTURA_TELA,al_map_rgba(10,8,14,138));
    painel(150,58,1130,654,20,al_map_rgba(20,14,24,231),al_map_rgba(255,225,112,135));
    (void)recursos;
}

void desenharTelaTeclasUI(const RecursosMapa* recursos,const Fase* fase,
                          const Scooby* scooby,const Maria* maria,float tempo)
{
    ALLEGRO_FONT* fonte=recursos?recursos->fonte:NULL;
    prepararSubmenu(recursos,fase,scooby,maria,tempo);
    cabecalhoSubmenu(fonte,"TECLAS","Controles principais do Sausage Run");

    const char* esquerda[]={"W A S D","SHIFT + W A S D","ESPACO","E"};
    const char* direita[]={"Mover Scooby","Correr e produzir ruido","Latir / criar distracao sonora","Morder / pegar a bolinha"};
    for(int i=0;i<4;i++)
    {
        float y=235+i*68.0f;

        /* Keycap de alto contraste. A versao anterior usava texto amarelo
           sobre uma caixa amarelo-clara; na fonte builtin, especialmente em
           Debug/Windows, as letras praticamente desapareciam. */
        painel(260,y-14,510,y+30,9,
               al_map_rgba(38,24,18,245),
               al_map_rgb(255,220,92));

        /* Sombra + texto branco garantem leitura independente do blender,
           gamma ou do fundo da fase. */
        textoEscalado(fonte,al_map_rgb(0,0,0),387,y+2,1.6f,ALLEGRO_ALIGN_CENTRE,esquerda[i]);
        textoEscalado(fonte,al_map_rgb(255,248,224),385,y,1.6f,ALLEGRO_ALIGN_CENTRE,esquerda[i]);
        textoEscalado(fonte,al_map_rgb(239,235,238),555,y,1.55f,0,direita[i]);
    }

    textoEscalado(fonte,al_map_rgb(218,226,241),640,526,1.45f,ALLEGRO_ALIGN_CENTRE,
                  "F1: debug de colisao e IA     ESC: abrir/fechar pausa durante a fase");
    textoEscalado(fonte,al_map_rgb(255,223,112),640,605,1.45f,ALLEGRO_ALIGN_CENTRE,
                  "ESC ou ENTER para voltar");
    al_flip_display();
}

void desenharTelaHistoriaUI(const RecursosMapa* recursos,const Fase* fase,
                            const Scooby* scooby,const Maria* maria,float tempo)
{
    ALLEGRO_FONT* fonte=recursos?recursos->fonte:NULL;
    prepararSubmenu(recursos,fase,scooby,maria,tempo);
    cabecalhoSubmenu(fonte,"HIST\xC3\x93RIA","Uma pequena perseguicao dentro de casa");

    textoEscalado(fonte,al_map_rgb(244,236,230),640,245,1.7f,ALLEGRO_ALIGN_CENTRE,
                  "Scooby e um dachshund cheio de energia que vive atras de suas bolinhas.");
    textoEscalado(fonte,al_map_rgb(244,236,230),640,292,1.7f,ALLEGRO_ALIGN_CENTRE,
                  "Em cada comodo, Maria procura o cachorro usando visao e audicao.");
    textoEscalado(fonte,al_map_rgb(244,236,230),640,339,1.7f,ALLEGRO_ALIGN_CENTRE,
                  "Encontre a bolinha, esconda-se atras dos moveis e escolha quando correr.");
    textoEscalado(fonte,al_map_rgb(244,236,230),640,386,1.7f,ALLEGRO_ALIGN_CENTRE,
                  "Um latido pode denunciar Scooby... ou atrair Maria para o lugar errado.");
    textoEscalado(fonte,al_map_rgb(255,224,105),640,463,1.85f,ALLEGRO_ALIGN_CENTRE,
                  "Pegue a bolinha e alcance a saida antes que Maria o encontre!");

    textoEscalado(fonte,al_map_rgb(255,223,112),640,605,1.45f,ALLEGRO_ALIGN_CENTRE,
                  "ESC ou ENTER para voltar");
    al_flip_display();
}

void desenharTelaDesenvolvedorUI(const RecursosMapa* recursos,const Fase* fase,
                                 const Scooby* scooby,const Maria* maria,float tempo)
{
    ALLEGRO_FONT* fonte=recursos?recursos->fonte:NULL;
    prepararSubmenu(recursos,fase,scooby,maria,tempo);
    cabecalhoSubmenu(fonte,"DESENVOLVEDOR","Informacoes do projeto");

    textoEscalado(fonte,al_map_rgb(255,226,112),640,252,2.05f,ALLEGRO_ALIGN_CENTRE,"Daniel Baptista");
    textoEscalado(fonte,al_map_rgb(238,234,240),640,315,1.65f,ALLEGRO_ALIGN_CENTRE,
                  "Sausage Run - jogo 2D top-down desenvolvido em C com Allegro 5");
    textoEscalado(fonte,al_map_rgb(238,234,240),640,360,1.65f,ALLEGRO_ALIGN_CENTRE,
                  "Projeto academico de Ciencia da Computacao");
    textoEscalado(fonte,al_map_rgb(218,226,241),640,430,1.45f,ALLEGRO_ALIGN_CENTRE,
                  "Gameplay, IA, pathfinding, colisao, sprites, audio e interface integrados no mesmo projeto.");

    textoEscalado(fonte,al_map_rgb(255,223,112),640,605,1.45f,ALLEGRO_ALIGN_CENTRE,
                  "ESC ou ENTER para voltar");
    al_flip_display();
}

void desenharMenuPausaOverlay(const RecursosMapa* recursos,int selecionado)
{
    ALLEGRO_FONT* fonte=recursos?recursos->fonte:NULL;
    al_draw_filled_rectangle(0,0,LARGURA_TELA,ALTURA_TELA,al_map_rgba(5,5,10,188));
    painel(390,86,890,650,22,al_map_rgba(21,16,27,239),al_map_rgba(255,225,105,175));

    textoEscalado(fonte,al_map_rgb(255,226,101),640,125,4.0f,ALLEGRO_ALIGN_CENTRE,"PAUSADO");
    textoEscalado(fonte,al_map_rgb(226,220,231),640,175,1.45f,ALLEGRO_ALIGN_CENTRE,
                  "A partida esta congelada enquanto este menu estiver aberto.");

    for(int i=0;i<QTD_ITENS_MENU_PAUSA;i++)
        itemMenu(fonte,OPCOES_MENU_PAUSA[i],640,276+i*72.0f,i==selecionado);

    textoEscalado(fonte,al_map_rgb(223,218,228),640,590,1.3f,ALLEGRO_ALIGN_CENTRE,
                  "Setas: navegar   ENTER: confirmar   ESC: continuar");
}