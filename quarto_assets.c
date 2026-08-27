#include "jogo.h"

static void contorno(float x1, float y1, float x2, float y2,
                     ALLEGRO_COLOR fill)
{
    al_draw_filled_rounded_rectangle(x1, y1, x2, y2, 12, 12, fill);
    al_draw_rounded_rectangle(x1, y1, x2, y2, 12, 12,
                              al_map_rgb(76, 43, 28), 5);
}

/*
 * A sheet original do quarto possui cerca de 100x64 px e estava sendo
 * ampliada em ~12x. Esta composição 1024x720 substitui esse fallback
 * defeituoso sem alterar os caminhos de assets já corretos das outras fases.
 */
ALLEGRO_BITMAP* criarFolhaQuartoProcedural(void)
{
    ALLEGRO_BITMAP* alvoAnterior = al_get_target_bitmap();
    ALLEGRO_TRANSFORM transformAnterior;
    const ALLEGRO_TRANSFORM* atual = al_get_current_transform();

    if (atual) transformAnterior = *atual;
    else al_identity_transform(&transformAnterior);

    int flags = al_get_new_bitmap_flags();
    al_set_new_bitmap_flags(0);
    ALLEGRO_BITMAP* folha = al_create_bitmap(1024, 720);
    al_set_new_bitmap_flags(flags);

    if (!folha) return NULL;

    al_set_target_bitmap(folha);
    ALLEGRO_TRANSFORM id;
    al_identity_transform(&id);
    al_use_transform(&id);
    al_clear_to_color(al_map_rgba(0, 0, 0, 0));

    /* 0 - cama do casal: 20,20,300,210 */
    contorno(20,20,320,230, al_map_rgb(139,78,45));
    al_draw_filled_rounded_rectangle(35,45,305,215,12,12,
                                     al_map_rgb(244,218,199));
    al_draw_filled_rectangle(45,105,295,215,al_map_rgb(227,102,130));
    al_draw_filled_rounded_rectangle(50,55,155,105,8,8,
                                     al_map_rgb(255,244,236));
    al_draw_filled_rounded_rectangle(170,55,275,105,8,8,
                                     al_map_rgb(255,244,236));

    /* 1 - berço aberto: 350,20,200,210 */
    contorno(350,20,550,230, al_map_rgb(167,100,56));
    al_draw_filled_rectangle(375,65,525,210,al_map_rgb(250,206,221));
    for (int x=365; x<=535; x+=28)
        al_draw_line((float)x,45,(float)x,220,al_map_rgb(103,61,41),5);
    al_draw_line(360,45,540,45,al_map_rgb(103,61,41),6);

    /* 2 - guarda-roupa: 580,20,180,230 */
    contorno(580,20,760,250,al_map_rgb(151,83,42));
    al_draw_line(670,35,670,235,al_map_rgb(93,52,32),4);
    al_draw_filled_circle(655,135,5,al_map_rgb(235,190,85));
    al_draw_filled_circle(685,135,5,al_map_rgb(235,190,85));

    /* 3 - espelho: 800,25,120,220 */
    contorno(800,25,920,245,al_map_rgb(123,72,44));
    al_draw_filled_rounded_rectangle(817,43,903,215,30,30,
                                     al_map_rgb(173,220,232));
    al_draw_line(842,55,890,120,al_map_rgba(255,255,255,150),5);

    /* 4 - estante de brinquedos: 20,275,220,180 */
    contorno(20,275,240,455,al_map_rgb(156,88,46));
    al_draw_line(30,335,230,335,al_map_rgb(91,51,31),5);
    al_draw_line(30,395,230,395,al_map_rgb(91,51,31),5);
    for (int i=0; i<6; i++)
        al_draw_filled_circle(50+i*30,315+(i%2)*10,12,
                              al_map_rgb(235-20*i,100+20*i,130+10*i));

    /* 5 - cômoda/trocador: 270,280,220,170 */
    contorno(270,280,490,450,al_map_rgb(159,91,51));
    al_draw_filled_rounded_rectangle(290,292,470,335,8,8,
                                     al_map_rgb(252,214,226));
    al_draw_line(290,365,470,365,al_map_rgb(92,52,31),4);
    al_draw_line(290,405,470,405,al_map_rgb(92,52,31),4);

    /* 6 - baú: 525,300,220,145 */
    contorno(525,300,745,445,al_map_rgb(133,76,42));
    al_draw_filled_rounded_rectangle(545,320,725,375,10,10,
                                     al_map_rgb(218,111,139));
    al_draw_filled_circle(575,350,18,al_map_rgb(78,151,220));
    al_draw_filled_circle(620,342,15,al_map_rgb(238,194,65));
    al_draw_filled_circle(670,352,20,al_map_rgb(104,188,106));

    /* 7 - puff: 790,300,150,145 */
    al_draw_filled_ellipse(865,375,72,65,al_map_rgb(235,87,145));
    al_draw_ellipse(865,375,72,65,al_map_rgb(119,47,74),5);
    al_draw_filled_ellipse(865,365,34,28,al_map_rgb(247,131,177));

    /* 8 - mesa infantil: 25,505,180,145 */
    contorno(25,505,205,620,al_map_rgb(176,104,58));
    al_draw_filled_circle(75,545,15,al_map_rgb(238,190,63));
    al_draw_filled_rectangle(110,530,145,565,al_map_rgb(89,158,220));
    al_draw_filled_circle(165,548,14,al_map_rgb(113,187,103));
    al_draw_filled_rectangle(45,615,65,650,al_map_rgb(119,67,39));
    al_draw_filled_rectangle(165,615,185,650,al_map_rgb(119,67,39));

    /* 9 - tapete: 240,510,280,135 */
    al_draw_filled_rounded_rectangle(240,510,520,645,45,45,
                                     al_map_rgb(246,183,205));
    al_draw_rounded_rectangle(240,510,520,645,45,45,
                              al_map_rgb(187,91,129),5);
    al_draw_filled_circle(380,575,32,al_map_rgb(255,220,126));

    /* 10 - criado: 560,510,145,135 */
    contorno(560,510,705,625,al_map_rgb(149,83,45));
    al_draw_filled_circle(632,540,17,al_map_rgb(245,203,92));
    al_draw_line(575,580,690,580,al_map_rgb(89,49,30),4);

    if (alvoAnterior) al_set_target_bitmap(alvoAnterior);
    al_use_transform(&transformAnterior);
    return folha;
}
