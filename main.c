#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>

#include <stdbool.h>
#include <math.h>
#include <stdio.h>

#define LARGURA_TELA 1280
#define ALTURA_TELA 720
#define FPS 60.0

#define PI 3.14159265358979323846f

/* =========================================================
   ESTADOS DA MARIA
   ========================================================= */

typedef enum
{
    MARIA_PATRULHA,
    MARIA_INVESTIGAR,
    MARIA_PERSEGUIR,
    MARIA_PROCURAR

} EstadoMaria;


/* =========================================================
   TIPOS DE SOM
   ========================================================= */

typedef enum
{
    SOM_NENHUM,
    SOM_CORRIDA,
    SOM_LATIDO,
    SOM_INTERACAO

} TipoSom;


/* =========================================================
   PERSONAGEM BASE
   ========================================================= */

typedef struct
{
    float x;
    float y;

    float largura;
    float altura;

    float velocidade;

    /* Direção em radianos */
    float direcao;

} Personagem;


/* =========================================================
   SCOOBY
   ========================================================= */

typedef struct
{
    Personagem corpo;

    bool correndo;
    bool movendo;

    float cooldownSomCorrida;

} Scooby;


/* =========================================================
   MARIA
   ========================================================= */

typedef struct
{
    Personagem corpo;

    EstadoMaria estado;

    float alcanceVisao;
    float anguloVisao;

    float alcanceAudicao;

    float alvoX;
    float alvoY;

    float ultimaPosicaoVistaX;
    float ultimaPosicaoVistaY;

    float tempoBusca;

    int waypointAtual;

} Maria;


/* =========================================================
   OBSTÁCULO
   ========================================================= */

typedef struct
{
    float x;
    float y;

    float largura;
    float altura;

    bool bloqueiaMovimento;
    bool bloqueiaVisao;

} Obstaculo;


/* =========================================================
   EVENTO DE SOM
   ========================================================= */

typedef struct
{
    TipoSom tipo;

    float x;
    float y;

    float alcance;

    float tempoRestante;

    bool ativo;

    /*
        Evita Maria processar o mesmo som
        repetidamente durante vários frames.
    */
    bool processado;

} EventoSom;


/* =========================================================
   DISTÂNCIA
   ========================================================= */

float distancia(float x1, float y1, float x2, float y2)
{
    float dx = x2 - x1;
    float dy = y2 - y1;

    return sqrtf(dx * dx + dy * dy);
}


/* =========================================================
   NORMALIZAR ÂNGULO
   ========================================================= */

float normalizarAngulo(float angulo)
{
    while (angulo > PI)
        angulo -= 2.0f * PI;

    while (angulo < -PI)
        angulo += 2.0f * PI;

    return angulo;
}


/* =========================================================
   TESTE DE PONTO DENTRO DE OBSTÁCULO
   ========================================================= */

bool pontoDentroObstaculo(
    float x,
    float y,
    Obstaculo obstaculo
)
{
    return
        x >= obstaculo.x &&
        x <= obstaculo.x + obstaculo.largura &&
        y >= obstaculo.y &&
        y <= obstaculo.y + obstaculo.altura;
}


/* =========================================================
   COLISÃO PERSONAGEM x OBSTÁCULO
   ========================================================= */

bool personagemColide(
    Personagem* personagem,
    float novoX,
    float novoY,
    Obstaculo obstaculos[],
    int quantidade
)
{
    float esquerda =
        novoX - personagem->largura / 2.0f;

    float direita =
        novoX + personagem->largura / 2.0f;

    float topo =
        novoY - personagem->altura / 2.0f;

    float baixo =
        novoY + personagem->altura / 2.0f;

    for (int i = 0; i < quantidade; i++)
    {
        if (!obstaculos[i].bloqueiaMovimento)
            continue;

        float obsEsquerda = obstaculos[i].x;
        float obsDireita =
            obstaculos[i].x + obstaculos[i].largura;

        float obsTopo = obstaculos[i].y;
        float obsBaixo =
            obstaculos[i].y + obstaculos[i].altura;

        if (
            direita > obsEsquerda &&
            esquerda < obsDireita &&
            baixo > obsTopo &&
            topo < obsBaixo
            )
        {
            return true;
        }
    }

    return false;
}


/* =========================================================
   MOVIMENTAÇÃO COM COLISÃO
   ========================================================= */

void moverPersonagem(
    Personagem* personagem,
    float dx,
    float dy,
    Obstaculo obstaculos[],
    int quantidade
)
{
    float novoX = personagem->x + dx;

    if (!personagemColide(
        personagem,
        novoX,
        personagem->y,
        obstaculos,
        quantidade))
    {
        personagem->x = novoX;
    }


    float novoY = personagem->y + dy;

    if (!personagemColide(
        personagem,
        personagem->x,
        novoY,
        obstaculos,
        quantidade))
    {
        personagem->y = novoY;
    }
}


/* =========================================================
   VERIFICA SE UM OBSTÁCULO BLOQUEIA A VISÃO
   ========================================================= */

bool linhaVisaoBloqueada(
    float origemX,
    float origemY,
    float destinoX,
    float destinoY,
    Obstaculo obstaculos[],
    int quantidade
)
{
    /*
        Protótipo simples.

        Pegamos vários pontos entre Maria e Scooby
        e verificamos se algum atravessa um obstáculo.

        Depois podemos substituir por raycasting.
    */

    const int passos = 40;

    for (int passo = 1; passo < passos; passo++)
    {
        float t = (float)passo / passos;

        float x =
            origemX + (destinoX - origemX) * t;

        float y =
            origemY + (destinoY - origemY) * t;

        for (int i = 0; i < quantidade; i++)
        {
            if (!obstaculos[i].bloqueiaVisao)
                continue;

            if (pontoDentroObstaculo(
                x,
                y,
                obstaculos[i]))
            {
                return true;
            }
        }
    }

    return false;
}


/* =========================================================
   CONE DE VISÃO DA MARIA
   ========================================================= */

bool mariaVeScooby(
    Maria* maria,
    Scooby* scooby,
    Obstaculo obstaculos[],
    int quantidade
)
{
    float dist = distancia(
        maria->corpo.x,
        maria->corpo.y,
        scooby->corpo.x,
        scooby->corpo.y
    );

    if (dist > maria->alcanceVisao)
        return false;


    float anguloParaScooby = atan2f(
        scooby->corpo.y - maria->corpo.y,
        scooby->corpo.x - maria->corpo.x
    );


    float diferenca = normalizarAngulo(
        anguloParaScooby -
        maria->corpo.direcao
    );


    if (fabsf(diferenca) >
        maria->anguloVisao / 2.0f)
    {
        return false;
    }


    /*
        Scooby está dentro do cone.

        Agora verificamos se algum móvel
        bloqueia a visão.
    */

    if (linhaVisaoBloqueada(
        maria->corpo.x,
        maria->corpo.y,
        scooby->corpo.x,
        scooby->corpo.y,
        obstaculos,
        quantidade))
    {
        return false;
    }


    return true;
}


/* =========================================================
   CRIAR UM SOM
   ========================================================= */

void emitirSom(
    EventoSom* som,
    TipoSom tipo,
    float x,
    float y,
    float alcance
)
{
    som->tipo = tipo;

    som->x = x;
    som->y = y;

    som->alcance = alcance;

    som->tempoRestante = 0.30f;

    som->ativo = true;
    som->processado = false;
}


/* =========================================================
   MARIA ESCUTA?
   ========================================================= */

bool mariaOuveSom(
    Maria* maria,
    EventoSom* som
)
{
    if (!som->ativo)
        return false;

    float dist = distancia(
        maria->corpo.x,
        maria->corpo.y,
        som->x,
        som->y
    );

    return dist <= som->alcance;
}


/* =========================================================
   ATUALIZA EVENTO DE SOM
   ========================================================= */

void atualizarSom(
    EventoSom* som,
    float dt
)
{
    if (!som->ativo)
        return;

    som->tempoRestante -= dt;

    if (som->tempoRestante <= 0)
    {
        som->ativo = false;
        som->tipo = SOM_NENHUM;
    }
}


/* =========================================================
   SCOOBY
   ========================================================= */

void atualizarScooby(
    Scooby* scooby,
    ALLEGRO_KEYBOARD_STATE* teclado,
    Obstaculo obstaculos[],
    int quantidade,
    EventoSom* som,
    float dt
)
{
    float dx = 0;
    float dy = 0;


    if (al_key_down(teclado, ALLEGRO_KEY_W))
        dy -= 1;

    if (al_key_down(teclado, ALLEGRO_KEY_S))
        dy += 1;

    if (al_key_down(teclado, ALLEGRO_KEY_A))
        dx -= 1;

    if (al_key_down(teclado, ALLEGRO_KEY_D))
        dx += 1;


    scooby->correndo =
        al_key_down(teclado, ALLEGRO_KEY_LSHIFT);


    scooby->movendo =
        (dx != 0 || dy != 0);


    float velocidade = 130.0f;


    if (scooby->correndo)
        velocidade = 240.0f;


    if (scooby->movendo)
    {
        float tamanho =
            sqrtf(dx * dx + dy * dy);

        dx /= tamanho;
        dy /= tamanho;


        scooby->corpo.direcao =
            atan2f(dy, dx);


        moverPersonagem(
            &scooby->corpo,
            dx * velocidade * dt,
            dy * velocidade * dt,
            obstaculos,
            quantidade
        );
    }


    /*
        Som periódico da corrida.
    */

    if (scooby->cooldownSomCorrida > 0)
        scooby->cooldownSomCorrida -= dt;


    if (
        scooby->correndo &&
        scooby->movendo &&
        scooby->cooldownSomCorrida <= 0
        )
    {
        emitirSom(
            som,
            SOM_CORRIDA,
            scooby->corpo.x,
            scooby->corpo.y,
            220.0f
        );

        scooby->cooldownSomCorrida = 0.35f;
    }
}


/* =========================================================
   MARIA MOVE EM DIREÇÃO AO ALVO
   ========================================================= */

void moverMariaPara(
    Maria* maria,
    float alvoX,
    float alvoY,
    Obstaculo obstaculos[],
    int quantidade,
    float dt
)
{
    float dx = alvoX - maria->corpo.x;
    float dy = alvoY - maria->corpo.y;


    float dist = sqrtf(dx * dx + dy * dy);


    if (dist < 2.0f)
        return;


    dx /= dist;
    dy /= dist;


    maria->corpo.direcao =
        atan2f(dy, dx);


    moverPersonagem(
        &maria->corpo,
        dx * maria->corpo.velocidade * dt,
        dy * maria->corpo.velocidade * dt,
        obstaculos,
        quantidade
    );
}


/* =========================================================
   IA DA MARIA
   ========================================================= */

void atualizarMaria(
    Maria* maria,
    Scooby* scooby,
    EventoSom* som,
    Obstaculo obstaculos[],
    int quantidade,
    float dt
)
{
    bool viuScooby =
        mariaVeScooby(
            maria,
            scooby,
            obstaculos,
            quantidade
        );


    /*
        VISÃO tem prioridade sobre SOM.
    */

    if (viuScooby)
    {
        maria->estado =
            MARIA_PERSEGUIR;

        maria->alvoX =
            scooby->corpo.x;

        maria->alvoY =
            scooby->corpo.y;

        maria->ultimaPosicaoVistaX =
            scooby->corpo.x;

        maria->ultimaPosicaoVistaY =
            scooby->corpo.y;
    }
    else
    {
        /*
            Maria ouviu alguma coisa.
        */

        if (
            som->ativo &&
            !som->processado &&
            mariaOuveSom(maria, som)
            )
        {
            maria->estado =
                MARIA_INVESTIGAR;

            maria->alvoX =
                som->x;

            maria->alvoY =
                som->y;

            som->processado = true;
        }


        /*
            Perdeu Scooby durante perseguição.
        */

        if (maria->estado == MARIA_PERSEGUIR)
        {
            maria->estado =
                MARIA_PROCURAR;

            maria->alvoX =
                maria->ultimaPosicaoVistaX;

            maria->alvoY =
                maria->ultimaPosicaoVistaY;

            maria->tempoBusca = 3.0f;
        }
    }


    /* =====================================================
       EXECUTAR ESTADO
       ===================================================== */

    switch (maria->estado)
    {

        /* ------------------------------------------------- */
    case MARIA_PATRULHA:
    {
        /*
            Patrulha simples.

            Depois vamos transformar isso
            em waypoints por fase.
        */

        static float pontos[4][2] =
        {
            { 250, 200 },
            { 950, 180 },
            { 950, 550 },
            { 280, 540 }
        };


        float alvoX =
            pontos[maria->waypointAtual][0];

        float alvoY =
            pontos[maria->waypointAtual][1];


        moverMariaPara(
            maria,
            alvoX,
            alvoY,
            obstaculos,
            quantidade,
            dt
        );


        if (distancia(
            maria->corpo.x,
            maria->corpo.y,
            alvoX,
            alvoY) < 30)
        {
            maria->waypointAtual++;

            if (maria->waypointAtual >= 4)
                maria->waypointAtual = 0;
        }

        break;
    }


    /* ------------------------------------------------- */
    case MARIA_INVESTIGAR:

        moverMariaPara(
            maria,
            maria->alvoX,
            maria->alvoY,
            obstaculos,
            quantidade,
            dt
        );


        if (distancia(
            maria->corpo.x,
            maria->corpo.y,
            maria->alvoX,
            maria->alvoY) < 30)
        {
            maria->estado =
                MARIA_PROCURAR;

            maria->tempoBusca = 2.5f;
        }

        break;


        /* ------------------------------------------------- */
    case MARIA_PERSEGUIR:

        moverMariaPara(
            maria,
            scooby->corpo.x,
            scooby->corpo.y,
            obstaculos,
            quantidade,
            dt
        );

        break;


        /* ------------------------------------------------- */
    case MARIA_PROCURAR:

        moverMariaPara(
            maria,
            maria->alvoX,
            maria->alvoY,
            obstaculos,
            quantidade,
            dt
        );


        maria->tempoBusca -= dt;


        if (maria->tempoBusca <= 0)
        {
            maria->estado =
                MARIA_PATRULHA;
        }

        break;
    }
}


/* =========================================================
   MAIN
   ========================================================= */

int main(void)
{
    if (!al_init())
    {
        printf("Erro ao iniciar Allegro.\n");
        return -1;
    }


    al_install_keyboard();
    al_init_primitives_addon();


    ALLEGRO_DISPLAY* display =
        al_create_display(
            LARGURA_TELA,
            ALTURA_TELA
        );


    ALLEGRO_TIMER* timer =
        al_create_timer(
            1.0 / FPS
        );


    ALLEGRO_EVENT_QUEUE* fila =
        al_create_event_queue();


    if (!display || !timer || !fila)
    {
        printf("Erro ao criar Allegro.\n");
        return -1;
    }


    al_register_event_source(
        fila,
        al_get_display_event_source(display)
    );


    al_register_event_source(
        fila,
        al_get_timer_event_source(timer)
    );


    al_register_event_source(
        fila,
        al_get_keyboard_event_source()
    );


    /* =====================================================
       SCOOBY
       ===================================================== */

    Scooby scooby =
    {
        .corpo =
        {
            .x = 250,
            .y = 350,
            .largura = 42,
            .altura = 28,
            .velocidade = 130,
            .direcao = 0
        },

        .correndo = false,
        .movendo = false,
        .cooldownSomCorrida = 0
    };


    /* =====================================================
       MARIA
       ===================================================== */

    Maria maria =
    {
        .corpo =
        {
            .x = 900,
            .y = 350,
            .largura = 35,
            .altura = 45,
            .velocidade = 100,
            .direcao = PI
        },

        .estado = MARIA_PATRULHA,

        .alcanceVisao = 260,

        /*
            70 graus
        */
        .anguloVisao =
            70.0f * PI / 180.0f,

        .alcanceAudicao = 400,

        .tempoBusca = 0,

        .waypointAtual = 0
    };


    EventoSom som =
    {
        .tipo = SOM_NENHUM,
        .ativo = false
    };


    /* =====================================================
       OBSTÁCULOS DE TESTE
       ===================================================== */

    Obstaculo obstaculos[] =
    {
        /*
            Sofá
        */
        {
            480, 280,
            250, 100,
            true,
            true
        },

        /*
            Mesa
        */
        {
            400, 500,
            150, 80,
            true,
            true
        },

        /*
            Estante
        */
        {
            900, 100,
            140, 100,
            true,
            true
        },

        /*
            Planta
        */
        {
            750, 470,
            80, 80,
            true,
            true
        }
    };


    int quantidadeObstaculos =
        sizeof(obstaculos) /
        sizeof(obstaculos[0]);


    bool executando = true;
    bool redesenhar = true;


    al_start_timer(timer);


    /* =====================================================
       GAME LOOP
       ===================================================== */

    while (executando)
    {
        ALLEGRO_EVENT evento;

        al_wait_for_event(
            fila,
            &evento
        );


        /* =================================================
           FECHAR
           ================================================= */

        if (evento.type ==
            ALLEGRO_EVENT_DISPLAY_CLOSE)
        {
            executando = false;
        }


        /* =================================================
           TECLADO
           ================================================= */

        if (evento.type ==
            ALLEGRO_EVENT_KEY_DOWN)
        {
            if (evento.keyboard.keycode ==
                ALLEGRO_KEY_ESCAPE)
            {
                executando = false;
            }


            /*
                ESPAÇO = LATIR

                Alcance maior que corrida.
            */

            if (evento.keyboard.keycode ==
                ALLEGRO_KEY_SPACE)
            {
                emitirSom(
                    &som,
                    SOM_LATIDO,
                    scooby.corpo.x,
                    scooby.corpo.y,
                    420.0f
                );
            }
        }


        /* =================================================
           UPDATE
           ================================================= */

        if (evento.type ==
            ALLEGRO_EVENT_TIMER)
        {
            const float dt =
                1.0f / FPS;


            ALLEGRO_KEYBOARD_STATE teclado;

            al_get_keyboard_state(
                &teclado
            );


            atualizarSom(
                &som,
                dt
            );


            atualizarScooby(
                &scooby,
                &teclado,
                obstaculos,
                quantidadeObstaculos,
                &som,
                dt
            );


            atualizarMaria(
                &maria,
                &scooby,
                &som,
                obstaculos,
                quantidadeObstaculos,
                dt
            );


            redesenhar = true;
        }


        /* =================================================
           DRAW
           ================================================= */

        if (
            redesenhar &&
            al_is_event_queue_empty(fila)
            )
        {
            redesenhar = false;


            al_clear_to_color(
                al_map_rgb(
                    220,
                    200,
                    170
                )
            );


            /* =============================================
               OBSTÁCULOS
               ============================================= */

            for (
                int i = 0;
                i < quantidadeObstaculos;
                i++
                )
            {
                al_draw_filled_rectangle(
                    obstaculos[i].x,
                    obstaculos[i].y,

                    obstaculos[i].x +
                    obstaculos[i].largura,

                    obstaculos[i].y +
                    obstaculos[i].altura,

                    al_map_rgb(
                        110,
                        70,
                        40
                    )
                );
            }


            /* =============================================
               CAMPO DE AUDIÇÃO
               Debug
               ============================================= */

            al_draw_circle(
                maria.corpo.x,
                maria.corpo.y,

                maria.alcanceAudicao,

                al_map_rgb(
                    80,
                    130,
                    255
                ),

                2
            );


            /* =============================================
               CONE DE VISÃO
               ============================================= */

            float angulo1 =
                maria.corpo.direcao -
                maria.anguloVisao / 2;


            float angulo2 =
                maria.corpo.direcao +
                maria.anguloVisao / 2;


            al_draw_line(
                maria.corpo.x,
                maria.corpo.y,

                maria.corpo.x +
                cosf(angulo1) *
                maria.alcanceVisao,

                maria.corpo.y +
                sinf(angulo1) *
                maria.alcanceVisao,

                al_map_rgb(
                    255,
                    220,
                    70
                ),

                2
            );


            al_draw_line(
                maria.corpo.x,
                maria.corpo.y,

                maria.corpo.x +
                cosf(angulo2) *
                maria.alcanceVisao,

                maria.corpo.y +
                sinf(angulo2) *
                maria.alcanceVisao,

                al_map_rgb(
                    255,
                    220,
                    70
                ),

                2
            );


            al_draw_arc(
                maria.corpo.x,
                maria.corpo.y,

                maria.alcanceVisao,

                angulo1,
                maria.anguloVisao,

                al_map_rgb(
                    255,
                    220,
                    70
                ),

                2
            );


            /* =============================================
               SCOOBY
               ============================================= */

            al_draw_filled_rectangle(
                scooby.corpo.x -
                scooby.corpo.largura / 2,

                scooby.corpo.y -
                scooby.corpo.altura / 2,

                scooby.corpo.x +
                scooby.corpo.largura / 2,

                scooby.corpo.y +
                scooby.corpo.altura / 2,

                al_map_rgb(
                    130,
                    70,
                    35
                )
            );


            /* =============================================
               MARIA
               ============================================= */

            al_draw_filled_circle(
                maria.corpo.x,
                maria.corpo.y,
                20,

                al_map_rgb(
                    255,
                    120,
                    170
                )
            );


            /* =============================================
               PONTO DO SOM
               Debug
               ============================================= */

            if (som.ativo)
            {
                al_draw_circle(
                    som.x,
                    som.y,
                    12,

                    al_map_rgb(
                        255,
                        50,
                        50
                    ),

                    3
                );
            }


            al_flip_display();
        }
    }


    /* =====================================================
       LIMPEZA
       ===================================================== */

    al_destroy_event_queue(fila);
    al_destroy_timer(timer);
    al_destroy_display(display);

    al_shutdown_primitives_addon();

    return 0;
}