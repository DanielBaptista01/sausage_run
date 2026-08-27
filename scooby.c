#include "jogo.h"

bool bolaNaFrenteDoScooby(const Scooby* scooby, const Bola* bola)
{
    if (!scooby || !bola || bola->coletada) return false;

    float d = distancia(
        scooby->corpo.x, scooby->corpo.y,
        bola->x, bola->y);

    if (d > 88.0f)
        return false;

    float angulo = atan2f(
        bola->y - scooby->corpo.y,
        bola->x - scooby->corpo.x);

    return fabsf(normalizarAngulo(
               angulo - scooby->corpo.direcao)) <=
           80.0f * PI / 180.0f;
}

void iniciarLatido(Scooby* s, EventoSom* som, RecursosAudio* audio)
{
    if (!s || s->latindo || s->mordendo || s->carregandoBola)
        return;

    s->latindo = true;
    s->movendo = false;
    s->correndo = false;
    reiniciarAnimacao(&s->bark);

    emitirSom(
        som,
        SOM_LATIDO,
        s->corpo.x,
        s->corpo.y,
        420.0f);

    if (audio)
        tocarEfeitoPosicional(
            audio->scoobyLatido,
            s->corpo.x,
            0.95f);
}

void iniciarMordida(Scooby* s, const Bola* bola, RecursosAudio* audio)
{
    if (!s || !bola ||
        s->latindo || s->mordendo || s->carregandoBola)
    {
        return;
    }

    s->mordendo = true;
    s->movendo = false;
    s->correndo = false;
    s->coletaPendente = bolaNaFrenteDoScooby(s, bola);
    reiniciarAnimacao(&s->bite);

    if (audio)
        tocarEfeitoPosicional(
            audio->scoobyMordida,
            s->corpo.x,
            0.72f);
}

static void tocarPassoScoobySeNecessario(
    Scooby* s,
    RecursosAudio* audio,
    int frameAnterior,
    bool correndo,
    Animacao* animacao)
{
    if (!s || !animacao) return;

    if (animacao->frameAtual == frameAnterior)
        return;

    if (animacao->frameAtual != 1 &&
        animacao->frameAtual != 3)
    {
        return;
    }

    if (!audio) return;

    if (correndo)
        tocarEfeitoPosicional(
            audio->scoobyCorrida,
            s->corpo.x,
            0.40f);
    else
        tocarEfeitoPosicional(
            audio->scoobyPasso,
            s->corpo.x,
            0.28f);
}

void atualizarScooby(Scooby* s,
                     const ALLEGRO_KEYBOARD_STATE* teclado,
                     const Fase* fase,
                     Bola* bola,
                     EventoSom* som,
                     RecursosAudio* audio,
                     float dt)
{
    if (!s || !teclado || !fase || !bola)
        return;

    s->movendo = false;
    s->correndo = false;

    if (s->latindo)
    {
        if (atualizarAnimacaoUmaVez(&s->bark, dt))
            s->latindo = false;
        return;
    }

    if (s->mordendo)
    {
        if (atualizarAnimacaoUmaVez(&s->bite, dt))
        {
            s->mordendo = false;

            if (s->coletaPendente &&
                bolaNaFrenteDoScooby(s, bola))
            {
                bola->coletada = true;
                s->carregandoBola = true;

                emitirSom(
                    som,
                    SOM_INTERACAO,
                    s->corpo.x,
                    s->corpo.y,
                    135.0f);

                reiniciarAnimacao(
                    &s->carregar[bola->cor]);

                if (audio)
                    tocarEfeitoPosicional(
                        audio->coletaBola,
                        s->corpo.x,
                        0.82f);
            }

            s->coletaPendente = false;
        }

        return;
    }

    float dx = 0.0f;
    float dy = 0.0f;

    if (al_key_down(teclado, ALLEGRO_KEY_W)) dy -= 1.0f;
    if (al_key_down(teclado, ALLEGRO_KEY_S)) dy += 1.0f;
    if (al_key_down(teclado, ALLEGRO_KEY_A)) dx -= 1.0f;
    if (al_key_down(teclado, ALLEGRO_KEY_D)) dx += 1.0f;

    s->movendo =
        fabsf(dx) > 0.01f ||
        fabsf(dy) > 0.01f;

    s->correndo =
        s->movendo &&
        al_key_down(teclado, ALLEGRO_KEY_LSHIFT);

    if (s->movendo)
    {
        float tamanho = sqrtf(dx * dx + dy * dy);
        dx /= tamanho;
        dy /= tamanho;

        s->corpo.direcao = atan2f(dy, dx);
        s->direcaoSprite =
            direcaoSpritePorMovimento(
                dx, dy, s->direcaoSprite);

        float velocidade =
            s->correndo ? 235.0f : 135.0f;

        moverPersonagem(
            &s->corpo,
            dx * velocidade * dt,
            dy * velocidade * dt,
            fase);
    }

    if (s->cooldownSomCorrida > 0.0f)
        s->cooldownSomCorrida -= dt;

    if (s->correndo &&
        s->cooldownSomCorrida <= 0.0f)
    {
        emitirSom(
            som,
            SOM_CORRIDA,
            s->corpo.x,
            s->corpo.y,
            220.0f);

        s->cooldownSomCorrida = 0.30f;
    }

    Animacao* atual = &s->idle;

    if (s->carregandoBola)
        atual = &s->carregar[bola->cor];
    else if (s->correndo)
        atual = &s->run;
    else if (s->movendo)
        atual = &s->walk;

    int frameAnterior = atual->frameAtual;
    atualizarAnimacaoLoop(atual, dt);

    if (s->movendo)
        tocarPassoScoobySeNecessario(
            s, audio, frameAnterior,
            s->correndo, atual);
}

void atualizarScoobyTransicao(
    Scooby* s,
    const Fase* fase,
    const Bola* bola,
    Ponto alvo,
    float dt)
{
    if (!s || !fase || !bola) return;

    s->latindo = false;
    s->mordendo = false;
    s->coletaPendente = false;
    s->correndo = false;
    s->movendo = true;

    float dx = alvo.x - s->corpo.x;
    float dy = alvo.y - s->corpo.y;
    float d = sqrtf(dx * dx + dy * dy);

    if (d > 3.0f)
    {
        dx /= d;
        dy /= d;

        s->corpo.direcao = atan2f(dy, dx);
        s->direcaoSprite =
            direcaoSpritePorMovimento(
                dx, dy, s->direcaoSprite);

        moverPersonagem(
            &s->corpo,
            dx * 62.0f * dt,
            dy * 62.0f * dt,
            fase);
    }
    else
    {
        s->movendo = false;
    }

    if (s->carregandoBola)
        atualizarAnimacaoLoop(
            &s->carregar[bola->cor], dt);
    else
        atualizarAnimacaoLoop(
            &s->walk, dt);
}
