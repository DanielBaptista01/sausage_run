#include "jogo.h"

static ALLEGRO_SAMPLE* carregarSampleFlexivel(const char* caminho)
{
    const char* prefixos[] = { "", "../", "../../", "../../../" };
    char tentativa[768];

    for (int i = 0; i < 4; i++)
    {
        snprintf(tentativa, sizeof(tentativa), "%s%s", prefixos[i], caminho);
        ALLEGRO_SAMPLE* sample = al_load_sample(tentativa);
        if (sample) return sample;
    }

    printf("Aviso: nao foi possivel carregar o efeito sonoro: %s\n", caminho);
    return NULL;
}

bool carregarRecursosAudio(RecursosAudio* a)
{
    if (!a) return false;

    a->scoobyPasso = carregarSampleFlexivel("audio/scooby_walk.wav");
    a->scoobyCorrida = carregarSampleFlexivel("audio/scooby_run.wav");
    a->scoobyLatido = carregarSampleFlexivel("audio/scooby_bark.wav");
    a->scoobyMordida = carregarSampleFlexivel("audio/scooby_bite.wav");
    a->coletaBola = carregarSampleFlexivel("audio/ball_pickup.wav");

    a->mariaPasso = carregarSampleFlexivel("audio/maria_walk.wav");
    a->mariaCorrida = carregarSampleFlexivel("audio/maria_run.wav");
    a->mariaAlerta = carregarSampleFlexivel("audio/maria_alert.wav");
    a->captura = carregarSampleFlexivel("audio/capture.wav");

    a->faseCompleta = carregarSampleFlexivel("audio/phase_complete.wav");
    a->vitoria = carregarSampleFlexivel("audio/victory.wav");
    a->gameOver = carregarSampleFlexivel("audio/game_over.wav");

    a->disponivel =
        a->scoobyPasso && a->scoobyCorrida && a->scoobyLatido &&
        a->scoobyMordida && a->coletaBola && a->mariaPasso &&
        a->mariaCorrida && a->mariaAlerta && a->captura &&
        a->faseCompleta && a->vitoria && a->gameOver;

    if (!a->disponivel)
        printf("Aviso: alguns efeitos sonoros nao foram carregados. O jogo continuara funcionando.\n");

    return a->disponivel;
}

void tocarEfeito(ALLEGRO_SAMPLE* sample, float ganho)
{
    if (!sample) return;
    if (ganho < 0.0f) ganho = 0.0f;
    if (ganho > 1.0f) ganho = 1.0f;
    al_play_sample(sample, ganho, 0.0f, 1.0f, ALLEGRO_PLAYMODE_ONCE, NULL);
}

void tocarEfeitoPosicional(ALLEGRO_SAMPLE* sample, float x, float ganho)
{
    if (!sample) return;

    float pan = (x - LARGURA_TELA / 2.0f) / (LARGURA_TELA / 2.0f);
    if (pan < -0.75f) pan = -0.75f;
    if (pan > 0.75f) pan = 0.75f;

    if (ganho < 0.0f) ganho = 0.0f;
    if (ganho > 1.0f) ganho = 1.0f;

    al_play_sample(sample, ganho, pan, 1.0f, ALLEGRO_PLAYMODE_ONCE, NULL);
}

static void destruirSample(ALLEGRO_SAMPLE** sample)
{
    if (sample && *sample)
    {
        al_destroy_sample(*sample);
        *sample = NULL;
    }
}

void destruirRecursosAudio(RecursosAudio* a)
{
    if (!a) return;

    destruirSample(&a->scoobyPasso);
    destruirSample(&a->scoobyCorrida);
    destruirSample(&a->scoobyLatido);
    destruirSample(&a->scoobyMordida);
    destruirSample(&a->coletaBola);
    destruirSample(&a->mariaPasso);
    destruirSample(&a->mariaCorrida);
    destruirSample(&a->mariaAlerta);
    destruirSample(&a->captura);
    destruirSample(&a->faseCompleta);
    destruirSample(&a->vitoria);
    destruirSample(&a->gameOver);
    a->disponivel = false;
}
