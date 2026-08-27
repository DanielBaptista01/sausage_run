#include "jogo.h"

#define AUDIO_FREQ 11025

typedef enum
{
    FX_SCOOBY_PASSO,
    FX_SCOOBY_CORRIDA,
    FX_LATIDO,
    FX_MORDIDA,
    FX_COLETA,
    FX_MARIA_PASSO,
    FX_MARIA_CORRIDA,
    FX_ALERTA,
    FX_CAPTURA,
    FX_FASE,
    FX_VITORIA,
    FX_GAME_OVER
} TipoFx;

static float clampf(float v, float a, float b)
{
    if (v < a) return a;
    if (v > b) return b;
    return v;
}

static float ondaTriangular(float fase)
{
    return (2.0f / PI) * asinf(sinf(fase));
}

static float envelope(float t, float duracao)
{
    float ataque = fminf(0.015f, duracao * 0.18f);
    float entrada = ataque > 0.0f ? fminf(1.0f, t / ataque) : 1.0f;
    float saida = 1.0f - t / duracao;
    return clampf(entrada * saida, 0.0f, 1.0f);
}

static float tom(float t, float frequencia, float duracao, float volume)
{
    return sinf(2.0f * PI * frequencia * t) * envelope(t, duracao) * volume;
}

static float triangular(float t, float frequencia, float duracao, float volume)
{
    return ondaTriangular(2.0f * PI * frequencia * t) * envelope(t, duracao) * volume;
}

static float ruido(float t, float duracao, float volume, unsigned int* estado)
{
    *estado = (*estado * 1664525u) + 1013904223u;
    float r = ((float)((*estado >> 8) & 0xffff) / 32767.5f) - 1.0f;
    return r * envelope(t, duracao) * volume;
}

static float sequenciaNotas(float t, const float* notas, int quantidade, float duracao, float volume)
{
    float trecho = duracao / quantidade;
    int indice = (int)(t / trecho);
    if (indice >= quantidade) indice = quantidade - 1;
    float local = t - indice * trecho;
    return triangular(local, notas[indice], trecho, volume);
}

static ALLEGRO_SAMPLE* criarSom(TipoFx tipo)
{
    float duracao = 0.12f;

    switch (tipo)
    {
        case FX_SCOOBY_PASSO: duracao = 0.085f; break;
        case FX_SCOOBY_CORRIDA: duracao = 0.070f; break;
        case FX_LATIDO: duracao = 0.31f; break;
        case FX_MORDIDA: duracao = 0.085f; break;
        case FX_COLETA: duracao = 0.30f; break;
        case FX_MARIA_PASSO: duracao = 0.080f; break;
        case FX_MARIA_CORRIDA: duracao = 0.060f; break;
        case FX_ALERTA: duracao = 0.19f; break;
        case FX_CAPTURA: duracao = 0.35f; break;
        case FX_FASE: duracao = 0.45f; break;
        case FX_VITORIA: duracao = 0.72f; break;
        case FX_GAME_OVER: duracao = 0.62f; break;
    }

    unsigned int quantidade = (unsigned int)(AUDIO_FREQ * duracao);
    unsigned char* dados = (unsigned char*)malloc(quantidade);
    if (!dados) return NULL;

    unsigned int estadoRuido = 1234567u + (unsigned int)tipo * 197u;

    for (unsigned int i = 0; i < quantidade; i++)
    {
        float t = (float)i / AUDIO_FREQ;
        float s = 0.0f;

        switch (tipo)
        {
            case FX_SCOOBY_PASSO:
            {
                float f = 145.0f - 65.0f * (t / duracao);
                s = tom(t, f, duracao, 0.40f) + ruido(t, duracao, 0.08f, &estadoRuido);
                break;
            }
            case FX_SCOOBY_CORRIDA:
            {
                float f = 220.0f - 115.0f * (t / duracao);
                s = tom(t, f, duracao, 0.50f) + ruido(t, duracao, 0.13f, &estadoRuido);
                break;
            }
            case FX_LATIDO:
            {
                if (t < 0.135f)
                {
                    float local = t;
                    float f = 520.0f - 290.0f * (local / 0.135f);
                    s = tom(local, f, 0.135f, 0.68f) + triangular(local, f * 0.48f, 0.135f, 0.25f);
                }
                else if (t > 0.165f)
                {
                    float local = t - 0.165f;
                    float f = 560.0f - 330.0f * (local / 0.145f);
                    s = tom(local, f, 0.145f, 0.58f) + triangular(local, f * 0.45f, 0.145f, 0.23f);
                }
                break;
            }
            case FX_MORDIDA:
                s = ruido(t, duracao, 0.48f, &estadoRuido) + tom(t, 135.0f, duracao, 0.30f);
                break;
            case FX_COLETA:
            {
                const float n[] = { 523.25f, 659.25f, 783.99f };
                s = sequenciaNotas(t, n, 3, duracao, 0.46f);
                break;
            }
            case FX_MARIA_PASSO:
            {
                float f = 245.0f - 90.0f * (t / duracao);
                s = tom(t, f, duracao, 0.28f) + ruido(t, duracao, 0.05f, &estadoRuido);
                break;
            }
            case FX_MARIA_CORRIDA:
            {
                float f = 310.0f - 135.0f * (t / duracao);
                s = tom(t, f, duracao, 0.34f) + ruido(t, duracao, 0.07f, &estadoRuido);
                break;
            }
            case FX_ALERTA:
            {
                const float n[] = { 740.0f, 880.0f };
                s = sequenciaNotas(t, n, 2, duracao, 0.33f);
                break;
            }
            case FX_CAPTURA:
            {
                const float n[] = { 330.0f, 220.0f, 150.0f };
                s = sequenciaNotas(t, n, 3, duracao, 0.40f);
                break;
            }
            case FX_FASE:
            {
                const float n[] = { 523.0f, 659.0f, 784.0f, 1046.0f };
                s = sequenciaNotas(t, n, 4, duracao, 0.38f);
                break;
            }
            case FX_VITORIA:
            {
                const float n[] = { 523.0f, 659.0f, 784.0f, 1046.0f, 1318.0f };
                s = sequenciaNotas(t, n, 5, duracao, 0.40f);
                break;
            }
            case FX_GAME_OVER:
            {
                const float n[] = { 440.0f, 330.0f, 247.0f, 165.0f };
                s = sequenciaNotas(t, n, 4, duracao, 0.38f);
                break;
            }
        }

        s = clampf(s, -1.0f, 1.0f);
        dados[i] = (unsigned char)((s + 1.0f) * 127.5f);
    }

    return al_create_sample(dados, quantidade, AUDIO_FREQ,
                            ALLEGRO_AUDIO_DEPTH_UINT8,
                            ALLEGRO_CHANNEL_CONF_1, true);
}

bool criarRecursosAudio(RecursosAudio* a)
{
    if (!a) return false;

    a->scoobyPasso = criarSom(FX_SCOOBY_PASSO);
    a->scoobyCorrida = criarSom(FX_SCOOBY_CORRIDA);
    a->scoobyLatido = criarSom(FX_LATIDO);
    a->scoobyMordida = criarSom(FX_MORDIDA);
    a->coletaBola = criarSom(FX_COLETA);
    a->mariaPasso = criarSom(FX_MARIA_PASSO);
    a->mariaCorrida = criarSom(FX_MARIA_CORRIDA);
    a->mariaAlerta = criarSom(FX_ALERTA);
    a->captura = criarSom(FX_CAPTURA);
    a->faseCompleta = criarSom(FX_FASE);
    a->vitoria = criarSom(FX_VITORIA);
    a->gameOver = criarSom(FX_GAME_OVER);

    a->disponivel = a->scoobyPasso && a->scoobyCorrida && a->scoobyLatido &&
                    a->scoobyMordida && a->coletaBola && a->mariaPasso &&
                    a->mariaCorrida && a->mariaAlerta && a->captura &&
                    a->faseCompleta && a->vitoria && a->gameOver;

    if (!a->disponivel)
        printf("Aviso: alguns efeitos sonoros nao puderam ser criados.\n");

    return a->disponivel;
}

void tocarEfeito(ALLEGRO_SAMPLE* sample, float ganho)
{
    if (!sample) return;
    ganho = clampf(ganho, 0.0f, 1.0f);
    al_play_sample(sample, ganho, 0.0f, 1.0f, ALLEGRO_PLAYMODE_ONCE, NULL);
}

void tocarEfeitoPosicional(ALLEGRO_SAMPLE* sample, float x, float ganho)
{
    if (!sample) return;
    float pan = (x - LARGURA_TELA / 2.0f) / (LARGURA_TELA / 2.0f);
    pan = clampf(pan, -0.75f, 0.75f);
    ganho = clampf(ganho, 0.0f, 1.0f);
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
