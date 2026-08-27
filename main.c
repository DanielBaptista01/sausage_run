#include "jogo.h"

static void prepararContextoGrafico(ALLEGRO_DISPLAY* display)
{
    if (!display) return;
    al_set_target_backbuffer(display);
    ALLEGRO_TRANSFORM transform;
    al_identity_transform(&transform);
    al_use_transform(&transform);
    al_reset_clipping_rectangle();
}

static float clamp255(float v)
{
    if (v < 0.0f) return 0.0f;
    if (v > 255.0f) return 255.0f;
    return v;
}

static void atualizarTitulo(ALLEGRO_DISPLAY* display, EstadoJogo estado,
                            const Fase* fase, int faseAtual, int vidas,
                            const Bola* bola)
{
    if (!display) return;

    char titulo[256];
    if (estado == JOGO_VITORIA)
        snprintf(titulo, sizeof(titulo), "Sausage Run - VOCE VENCEU!");
    else if (estado == JOGO_GAME_OVER)
        snprintf(titulo, sizeof(titulo), "Sausage Run - GAME OVER");
    else
        snprintf(titulo, sizeof(titulo),
                 "Sausage Run - Fase %d/4: %s | Vidas: %d | Bola %s",
                 faseAtual + 1,
                 fase ? fase->nome : "?",
                 vidas,
                 bola ? NOMES_CORES[bola->cor] : "?");

    al_set_window_title(display, titulo);
}

static void iniciarTransicao(TransicaoFase* t, const Fase* fase)
{
    if (!t || !fase) return;
    t->etapa = TRANSICAO_APROXIMAR;
    t->tempo = 0.0f;
    t->alphaFade = 0.0f;
    t->alvo = fase->alvoTransicao;
    t->faseTrocada = false;
}

static bool inicializarAllegro(ALLEGRO_DISPLAY** display,
                               ALLEGRO_TIMER** timer,
                               ALLEGRO_EVENT_QUEUE** fila,
                               bool* audioInstalado)
{
    if (!al_init()) { printf("Erro ao iniciar Allegro.\n"); return false; }
    if (!al_install_keyboard()) { printf("Erro ao iniciar teclado.\n"); return false; }
    if (!al_init_primitives_addon()) { printf("Erro ao iniciar allegro_primitives.\n"); return false; }
    if (!al_init_image_addon()) { printf("Erro ao iniciar allegro_image.\n"); return false; }

    al_init_font_addon();

    *audioInstalado = al_install_audio();
    if (*audioInstalado && !al_reserve_samples(24))
    {
        printf("Aviso: audio sem vozes disponiveis; jogo continuara sem som.\n");
        al_uninstall_audio();
        *audioInstalado = false;
    }

    *display = al_create_display(LARGURA_TELA, ALTURA_TELA);
    *timer = al_create_timer(1.0 / FPS);
    *fila = al_create_event_queue();

    if (!*display || !*timer || !*fila)
    {
        printf("Erro ao criar display/timer/fila.\n");
        return false;
    }

    prepararContextoGrafico(*display);

    al_register_event_source(*fila, al_get_display_event_source(*display));
    al_register_event_source(*fila, al_get_timer_event_source(*timer));
    al_register_event_source(*fila, al_get_keyboard_event_source());
    return true;
}

int main(void)
{
    srand((unsigned int)time(NULL));

    ALLEGRO_DISPLAY* display = NULL;
    ALLEGRO_TIMER* timer = NULL;
    ALLEGRO_EVENT_QUEUE* fila = NULL;
    bool audioInstalado = false;

    if (!inicializarAllegro(&display, &timer, &fila, &audioInstalado))
        return -1;

    ALLEGRO_FONT* fonteCarregamento = al_create_builtin_font();
    desenharCarregando(display, fonteCarregamento);

    RecursosMapa recursos = { 0 };
    RecursosAudio audio = { 0 };
    Fase fases[QTD_FASES] = { 0 };
    Scooby scooby = { 0 };
    Maria maria = { 0 };
    Bola bola = { 0 };
    EventoSom som = { SOM_NENHUM, 0, 0, 0, 0, false, false };

    configurarFases(fases);

    /* Hitboxes pequenas dos pes/patas; tamanho visual vem da sprite. */
    scooby.corpo.largura = 34.0f;
    scooby.corpo.altura = 16.0f;
    scooby.corpo.velocidade = 135.0f;
    scooby.direcaoSprite = DIRECAO_UP;

    maria.corpo.largura = 26.0f;
    maria.corpo.altura = 20.0f;
    maria.corpo.velocidade = 96.0f;
    maria.direcaoSprite = DIRECAO_LEFT;
    maria.alcanceVisao = 265.0f;
    maria.anguloVisao = 72.0f * PI / 180.0f;
    maria.alcanceAudicao = 430.0f;

    bool recursosOk =
        carregarRecursosMapa(&recursos) &&
        carregarSprites(&scooby, &maria) &&
        carregarRecursosFase(&recursos, fases, 0);

    if (fonteCarregamento)
    {
        al_destroy_font(fonteCarregamento);
        fonteCarregamento = NULL;
    }

    if (!recursosOk)
    {
        printf("Nao foi possivel carregar todos os recursos obrigatorios.\n");
        destruirRecursos(&recursos, fases, &scooby, &maria);
        destruirRecursosAudio(&audio);
        al_destroy_event_queue(fila);
        al_destroy_timer(timer);
        al_destroy_display(display);
        if (audioInstalado) al_uninstall_audio();
        al_shutdown_font_addon();
        al_shutdown_image_addon();
        al_shutdown_primitives_addon();
        return -1;
    }

    /* Valida todos os spawns e waypoints contra as colisoes finais. */
    for (int i = 0; i < QTD_FASES; i++)
        validarConfiguracaoFase(&fases[i], &scooby, &maria, i);

    prepararContextoGrafico(display);
    if (audioInstalado) criarRecursosAudio(&audio);

    int faseAtual = 0;
    int vidas = 3;
    EstadoJogo estado = JOGO_RODANDO;
    EstadoJogo estadoAntesPausa = JOGO_RODANDO;
    bool executando = true;
    bool redesenhar = true;
    bool debug = false;
    float tempoTutorial = 9.0f;
    TransicaoFase transicao = { TRANSICAO_APROXIMAR, 0.0f, 0.0f, {0,0}, false };

    resetarPersonagensNaFase(&scooby, &maria, &bola,
                             &fases[faseAtual], faseAtual, true);
    atualizarTitulo(display, estado, &fases[faseAtual], faseAtual, vidas, &bola);
    al_start_timer(timer);

    while (executando)
    {
        ALLEGRO_EVENT ev;
        al_wait_for_event(fila, &ev);

        if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
            executando = false;

        if (ev.type == ALLEGRO_EVENT_DISPLAY_SWITCH_OUT && estado == JOGO_RODANDO)
        {
            estadoAntesPausa = estado;
            estado = JOGO_PAUSADO;
            atualizarTitulo(display, estado, &fases[faseAtual], faseAtual, vidas, &bola);
            redesenhar = true;
        }

        if (ev.type == ALLEGRO_EVENT_KEY_DOWN)
        {
            if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE)
            {
                if (estado == JOGO_VITORIA || estado == JOGO_GAME_OVER)
                    executando = false;
                else if (estado == JOGO_RODANDO)
                {
                    estadoAntesPausa = estado;
                    estado = JOGO_PAUSADO;
                    redesenhar = true;
                }
                else if (estado == JOGO_PAUSADO)
                {
                    estado = estadoAntesPausa;
                    redesenhar = true;
                }
                atualizarTitulo(display, estado, &fases[faseAtual], faseAtual, vidas, &bola);
            }

            if (ev.keyboard.keycode == ALLEGRO_KEY_F1)
            {
                debug = !debug;
                if (debug && estado == JOGO_RODANDO)
                {
                    srand(SEED_DEBUG);
                    resetarPersonagensNaFase(&scooby, &maria, &bola,
                                             &fases[faseAtual], faseAtual, true);
                    som.ativo = false;
                }
                redesenhar = true;
            }

            if (ev.keyboard.keycode == ALLEGRO_KEY_R &&
                (estado == JOGO_VITORIA || estado == JOGO_GAME_OVER))
            {
                faseAtual = 0;
                vidas = 3;
                estado = JOGO_RODANDO;
                estadoAntesPausa = JOGO_RODANDO;
                tempoTutorial = 9.0f;
                maria.corpo.velocidade = 96.0f;
                maria.alcanceVisao = 265.0f;

                desenharCarregando(display, recursos.fonte);
                if (!carregarRecursosFase(&recursos, fases, faseAtual))
                    estado = JOGO_GAME_OVER;
                else
                {
                    validarConfiguracaoFase(&fases[faseAtual], &scooby, &maria, faseAtual);
                    resetarPersonagensNaFase(&scooby, &maria, &bola,
                                             &fases[faseAtual], faseAtual, true);
                    som.ativo = false;
                }
                atualizarTitulo(display, estado, &fases[faseAtual], faseAtual, vidas, &bola);
            }

            if (estado == JOGO_RODANDO && maria.estado != MARIA_CAPTURAR)
            {
                if (ev.keyboard.keycode == ALLEGRO_KEY_SPACE)
                    iniciarLatido(&scooby, &som, &audio);
                if (ev.keyboard.keycode == ALLEGRO_KEY_E)
                    iniciarMordida(&scooby, &bola, &audio);
            }
        }

        if (ev.type == ALLEGRO_EVENT_TIMER)
        {
            const float dt = 1.0f / FPS;

            if (estado == JOGO_RODANDO)
            {
                Fase* fase = &fases[faseAtual];
                ALLEGRO_KEYBOARD_STATE teclado;
                al_get_keyboard_state(&teclado);

                atualizarSom(&som, dt);

                if (maria.estado != MARIA_CAPTURAR)
                    atualizarScooby(&scooby, &teclado, fase, &bola, &som, &audio, dt);
                else
                {
                    scooby.movendo = false;
                    scooby.correndo = false;
                    scooby.latindo = false;
                    scooby.mordendo = false;
                }

                atualizarMaria(&maria, &scooby, &som, fase, &audio, dt);

                /* Prioridade obrigatoria: captura antes da saida. */
                if (maria.capturaConcluida)
                {
                    vidas--;
                    som.ativo = false;

                    if (vidas <= 0)
                    {
                        estado = JOGO_GAME_OVER;
                        tocarEfeito(audio.gameOver, 0.82f);
                    }
                    else
                        resetarPersonagensNaFase(&scooby, &maria, &bola,
                                                 fase, faseAtual, false);

                    atualizarTitulo(display, estado, fase, faseAtual, vidas, &bola);
                }
                else if (maria.estado != MARIA_CAPTURAR &&
                         chegouNaSaidaComBola(&scooby, fase))
                {
                    estado = JOGO_TRANSICAO_FASE;
                    som.ativo = false;
                    iniciarTransicao(&transicao, fase);
                    tocarEfeito(audio.faseCompleta, 0.72f);
                    atualizarTitulo(display, estado, fase, faseAtual, vidas, &bola);
                }

                if (tempoTutorial > 0.0f) tempoTutorial -= dt;
            }
            else if (estado == JOGO_TRANSICAO_FASE)
            {
                Fase* fase = &fases[faseAtual];

                if (transicao.etapa != TRANSICAO_FADE_IN)
                {
                    transicao.tempo += dt;

                    if (transicao.tempo <= 0.62f)
                        atualizarScoobyTransicao(&scooby, fase, &bola, transicao.alvo, dt);
                    else
                        scooby.movendo = false;

                    if (transicao.tempo > 0.28f)
                    {
                        transicao.etapa = TRANSICAO_FADE_OUT;
                        float t = (transicao.tempo - 0.28f) / 0.92f;
                        transicao.alphaFade = clamp255(t * 255.0f);
                    }

                    if (transicao.tempo >= 1.20f && !transicao.faseTrocada)
                    {
                        transicao.alphaFade = 255.0f;
                        transicao.faseTrocada = true;

                        if (faseAtual + 1 >= QTD_FASES)
                        {
                            estado = JOGO_VITORIA;
                            tocarEfeito(audio.vitoria, 0.88f);
                        }
                        else
                        {
                            faseAtual++;
                            desenharCarregando(display, recursos.fonte);

                            if (!carregarRecursosFase(&recursos, fases, faseAtual))
                                estado = JOGO_GAME_OVER;
                            else
                            {
                                maria.corpo.velocidade = 96.0f + faseAtual * 6.0f;
                                maria.alcanceVisao = 265.0f + faseAtual * 10.0f;

                                validarConfiguracaoFase(&fases[faseAtual], &scooby, &maria, faseAtual);
                                resetarPersonagensNaFase(&scooby, &maria, &bola,
                                                         &fases[faseAtual], faseAtual, true);

                                transicao.etapa = TRANSICAO_FADE_IN;
                                transicao.tempo = 0.0f;
                                transicao.alphaFade = 255.0f;
                            }
                        }
                        atualizarTitulo(display, estado, &fases[faseAtual], faseAtual, vidas, &bola);
                    }
                }
                else
                {
                    transicao.tempo += dt;
                    float t = transicao.tempo / 0.55f;
                    transicao.alphaFade = clamp255(255.0f * (1.0f - t));

                    if (transicao.tempo >= 0.55f)
                    {
                        transicao.alphaFade = 0.0f;
                        transicao.faseTrocada = false;
                        estado = JOGO_RODANDO;
                        atualizarTitulo(display, estado, &fases[faseAtual], faseAtual, vidas, &bola);
                    }
                }
            }

            redesenhar = true;
        }

        if (redesenhar && al_is_event_queue_empty(fila))
        {
            redesenhar = false;
            prepararContextoGrafico(display);

            if (estado == JOGO_RODANDO || estado == JOGO_PAUSADO || estado == JOGO_TRANSICAO_FASE)
                desenharCena(&fases[faseAtual], &recursos, &scooby, &maria, &bola,
                             &som, debug, vidas, faseAtual, estado,
                             transicao.alphaFade, tempoTutorial);
            else
                desenharTelaFinal(estado, &recursos);
        }
    }

    prepararContextoGrafico(display);
    destruirRecursosAudio(&audio);
    destruirRecursos(&recursos, fases, &scooby, &maria);
    al_destroy_event_queue(fila);
    al_destroy_timer(timer);
    al_destroy_display(display);

    if (audioInstalado) al_uninstall_audio();
    al_shutdown_font_addon();
    al_shutdown_image_addon();
    al_shutdown_primitives_addon();
    return 0;
}
