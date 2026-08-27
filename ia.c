#include "jogo.h"

void emitirSom(EventoSom* som, TipoSom tipo, float x, float y, float alcance)
{
    if (!som) return;
    som->tipo = tipo;
    som->x = x;
    som->y = y;
    som->alcance = alcance;
    som->tempoRestante = 0.35f;
    som->ativo = true;
    som->processado = false;
}

void atualizarSom(EventoSom* som, float dt)
{
    if (!som || !som->ativo) return;
    som->tempoRestante -= dt;
    if (som->tempoRestante <= 0.0f)
    {
        som->ativo = false;
        som->tipo = SOM_NENHUM;
        som->processado = false;
    }
}

bool mariaOuveSom(const Maria* m, const EventoSom* s)
{
    if (!m || !s || !s->ativo) return false;
    float alcance = s->alcance;
    if (alcance > m->alcanceAudicao) alcance = m->alcanceAudicao;
    return distancia(m->corpo.x, m->corpo.y, s->x, s->y) <= alcance;
}

static bool segmentoRetangulo(float x1, float y1, float x2, float y2,
                              float rx, float ry, float rw, float rh)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    float t0 = 0.0f, t1 = 1.0f;
    float p[4] = { -dx, dx, -dy, dy };
    float q[4] = { x1-rx, rx+rw-x1, y1-ry, ry+rh-y1 };

    for (int i = 0; i < 4; i++)
    {
        if (fabsf(p[i]) < 0.00001f)
        {
            if (q[i] < 0.0f) return false;
        }
        else
        {
            float r = q[i] / p[i];
            if (p[i] < 0.0f)
            {
                if (r > t1) return false;
                if (r > t0) t0 = r;
            }
            else
            {
                if (r < t0) return false;
                if (r < t1) t1 = r;
            }
        }
    }
    return true;
}

bool linhaVisaoLivre(const Fase* f, float x1, float y1, float x2, float y2)
{
    if (!f) return false;

    for (int i = 0; i < f->quantidadeObstaculos; i++)
    {
        const Obstaculo* o = &f->obstaculos[i];
        if (!o->bloqueiaVisao) continue;

        if (segmentoRetangulo(x1, y1, x2, y2,
                              o->x, o->y, o->largura, o->altura))
            return false;
    }
    return true;
}

bool mariaVeScooby(const Maria* m, const Scooby* s, const Fase* f)
{
    if (!m || !s || !f) return false;

    if (distancia(m->corpo.x, m->corpo.y, s->corpo.x, s->corpo.y) > m->alcanceVisao)
        return false;

    float angulo = atan2f(s->corpo.y - m->corpo.y,
                          s->corpo.x - m->corpo.x);

    if (fabsf(normalizarAngulo(angulo - m->corpo.direcao)) > m->anguloVisao * 0.5f)
        return false;

    return linhaVisaoLivre(f,
                           m->corpo.x, m->corpo.y,
                           s->corpo.x, s->corpo.y);
}

bool mariaPodeCapturar(const Maria* m, const Scooby* s, const Fase* f)
{
    if (!m || !s || !f) return false;

    Retangulo hm = hitboxPersonagem(&m->corpo, m->corpo.x, m->corpo.y);
    Retangulo hs = hitboxPersonagem(&s->corpo, s->corpo.x, s->corpo.y);

    float mx = hm.x + hm.largura * 0.5f;
    float my = hm.y + hm.altura * 0.5f;
    float sx = hs.x + hs.largura * 0.5f;
    float sy = hs.y + hs.altura * 0.5f;

    float dx = fabsf(mx - sx) - (hm.largura + hs.largura) * 0.5f;
    float dy = fabsf(my - sy) - (hm.altura + hs.altura) * 0.5f;
    if (dx < 0.0f) dx = 0.0f;
    if (dy < 0.0f) dy = 0.0f;

    if (sqrtf(dx*dx + dy*dy) > 12.0f) return false;

    return linhaVisaoLivre(f, m->corpo.x, m->corpo.y,
                           s->corpo.x, s->corpo.y);
}

static int gridCol(float x)
{
    int c = (int)((x - MAPA_X) / TAM_CELULA);
    if (c < 0) c = 0;
    if (c >= GRID_COLS) c = GRID_COLS - 1;
    return c;
}

static int gridRow(float y)
{
    int r = (int)((y - MAPA_Y) / TAM_CELULA);
    if (r < 0) r = 0;
    if (r >= GRID_ROWS) r = GRID_ROWS - 1;
    return r;
}

static Ponto centroCelula(int c, int r)
{
    return (Ponto){
        MAPA_X + c * TAM_CELULA + TAM_CELULA * 0.5f,
        MAPA_Y + r * TAM_CELULA + TAM_CELULA * 0.5f
    };
}

/*
 * A propria colisao da Maria decide se a celula oferece clearance suficiente.
 * Assim grid e movimento usam exatamente a mesma hitbox dos pes.
 */
static bool celulaBloqueada(const Maria* m, const Fase* f, int c, int r)
{
    if (!m || !f) return true;
    if (c < 0 || c >= GRID_COLS || r < 0 || r >= GRID_ROWS) return true;

    Ponto p = centroCelula(c, r);
    return personagemColide(&m->corpo, p.x, p.y, f);
}

static bool acharCelulaLivreProxima(const Maria* m, const Fase* f,
                                    int origemC, int origemR, int* saidaC, int* saidaR)
{
    if (!m || !f || !saidaC || !saidaR) return false;

    if (!celulaBloqueada(m, f, origemC, origemR))
    {
        *saidaC = origemC;
        *saidaR = origemR;
        return true;
    }

    for (int raio = 1; raio <= 6; raio++)
    {
        for (int dr = -raio; dr <= raio; dr++)
        {
            for (int dc = -raio; dc <= raio; dc++)
            {
                if (abs(dc) != raio && abs(dr) != raio) continue;
                int c = origemC + dc;
                int r = origemR + dr;
                if (!celulaBloqueada(m, f, c, r))
                {
                    *saidaC = c;
                    *saidaR = r;
                    return true;
                }
            }
        }
    }
    return false;
}

static bool calcularCaminho(Maria* m, const Fase* f, float destinoX, float destinoY)
{
    int sc = gridCol(m->corpo.x);
    int sr = gridRow(m->corpo.y);
    int tcOriginal = gridCol(destinoX);
    int trOriginal = gridRow(destinoY);
    int tc = tcOriginal, tr = trOriginal;

    bool destinoOriginalLivre = !celulaBloqueada(m, f, tc, tr);
    if (!acharCelulaLivreProxima(m, f, tc, tr, &tc, &tr))
        return false;

    if (celulaBloqueada(m, f, sc, sr))
    {
        int nsc = sc, nsr = sr;
        if (acharCelulaLivreProxima(m, f, sc, sr, &nsc, &nsr))
        {
            sc = nsc;
            sr = nsr;
        }
    }

    m->quantidadeCaminho = 0;
    m->indiceCaminho = 0;

    if (sc == tc && sr == tr)
    {
        Ponto alvo = destinoOriginalLivre ? (Ponto){ destinoX, destinoY }
                                          : centroCelula(tc, tr);
        m->caminho[0] = alvo;
        m->quantidadeCaminho = 1;
        m->alvoNavegavelX = alvo.x;
        m->alvoNavegavelY = alvo.y;
        m->ultimoAlvoCaminhoX = destinoX;
        m->ultimoAlvoCaminhoY = destinoY;
        m->tempoRecalcularCaminho = 0.24f;
        return true;
    }

    bool visitado[GRID_ROWS][GRID_COLS] = { false };
    short anteriorC[GRID_ROWS][GRID_COLS];
    short anteriorR[GRID_ROWS][GRID_COLS];
    int filaC[GRID_TOTAL], filaR[GRID_TOTAL];

    for (int r = 0; r < GRID_ROWS; r++)
        for (int c = 0; c < GRID_COLS; c++)
        {
            anteriorC[r][c] = -1;
            anteriorR[r][c] = -1;
        }

    int inicio = 0, fim = 1;
    filaC[0] = sc;
    filaR[0] = sr;
    visitado[sr][sc] = true;

    const int dc[4] = { 1, -1, 0, 0 };
    const int dr[4] = { 0, 0, 1, -1 };
    bool achou = false;

    while (inicio < fim)
    {
        int c = filaC[inicio];
        int r = filaR[inicio++];

        if (c == tc && r == tr)
        {
            achou = true;
            break;
        }

        for (int i = 0; i < 4; i++)
        {
            int nc = c + dc[i];
            int nr = r + dr[i];
            if (nc < 0 || nc >= GRID_COLS || nr < 0 || nr >= GRID_ROWS || visitado[nr][nc])
                continue;
            if (celulaBloqueada(m, f, nc, nr)) continue;

            visitado[nr][nc] = true;
            anteriorC[nr][nc] = (short)c;
            anteriorR[nr][nc] = (short)r;
            filaC[fim] = nc;
            filaR[fim] = nr;
            fim++;
        }
    }

    if (!achou)
    {
        m->alvoNavegavelX = m->corpo.x;
        m->alvoNavegavelY = m->corpo.y;
        m->tempoRecalcularCaminho = 0.10f;
        return false;
    }

    Ponto inverso[MAX_CAMINHO];
    int qtd = 0, c = tc, r = tr;

    while (!(c == sc && r == sr) && qtd < MAX_CAMINHO)
    {
        inverso[qtd++] = centroCelula(c, r);
        int pc = anteriorC[r][c];
        int pr = anteriorR[r][c];
        if (pc < 0 || pr < 0) break;
        c = pc;
        r = pr;
    }

    for (int i = qtd - 1; i >= 0 && m->quantidadeCaminho < MAX_CAMINHO; i--)
        m->caminho[m->quantidadeCaminho++] = inverso[i];

    if (destinoOriginalLivre && m->quantidadeCaminho < MAX_CAMINHO)
    {
        m->caminho[m->quantidadeCaminho++] = (Ponto){ destinoX, destinoY };
        m->alvoNavegavelX = destinoX;
        m->alvoNavegavelY = destinoY;
    }
    else
    {
        Ponto efetivo = centroCelula(tc, tr);
        m->alvoNavegavelX = efetivo.x;
        m->alvoNavegavelY = efetivo.y;
    }

    m->ultimoAlvoCaminhoX = destinoX;
    m->ultimoAlvoCaminhoY = destinoY;
    m->tempoRecalcularCaminho = 0.24f;
    return m->quantidadeCaminho > 0;
}

static void tocarPassoMariaSeNecessario(Maria* m, RecursosAudio* audio,
                                        int frameAnterior, bool correndo)
{
    Animacao* a = correndo ? &m->run : &m->walk;
    if (a->frameAtual == frameAnterior) return;
    if (a->frameAtual != 1 && a->frameAtual != 3) return;
    if (!audio) return;

    if (correndo) tocarEfeitoPosicional(audio->mariaCorrida, m->corpo.x, 0.28f);
    else tocarEfeitoPosicional(audio->mariaPasso, m->corpo.x, 0.20f);
}

static void verificarAntiStuck(Maria* m, const Fase* f, float dt)
{
    if (!m || !f) return;

    float desloc = distancia(m->corpo.x, m->corpo.y,
                             m->antiStuckUltimoX, m->antiStuckUltimoY);

    if (m->movendo && desloc < 1.3f)
        m->antiStuckTempo += dt;
    else
    {
        m->antiStuckTempo = 0.0f;
        m->antiStuckUltimoX = m->corpo.x;
        m->antiStuckUltimoY = m->corpo.y;
    }

    if (m->antiStuckTempo >= 0.65f)
    {
        m->tempoRecalcularCaminho = 0.0f;
        m->quantidadeCaminho = 0;
        m->indiceCaminho = 0;
        m->ultimoAlvoCaminhoX = -9999.0f;
        m->ultimoAlvoCaminhoY = -9999.0f;

        int c = gridCol(m->corpo.x);
        int r = gridRow(m->corpo.y);
        int livreC = c, livreR = r;

        if (acharCelulaLivreProxima(m, f, c, r, &livreC, &livreR))
        {
            Ponto livre = centroCelula(livreC, livreR);
            m->alvoNavegavelX = livre.x;
            m->alvoNavegavelY = livre.y;
        }

        m->antiStuckTempo = 0.0f;
        m->antiStuckUltimoX = m->corpo.x;
        m->antiStuckUltimoY = m->corpo.y;
    }
}

static void moverMaria(Maria* m, const Fase* f,
                       float x, float y, float dt, float multiplicador)
{
    m->movendo = false;
    m->tempoRecalcularCaminho -= dt;

    bool alvoMudou = distancia(x, y,
                               m->ultimoAlvoCaminhoX,
                               m->ultimoAlvoCaminhoY) > 45.0f;

    if (m->tempoRecalcularCaminho <= 0.0f ||
        m->indiceCaminho >= m->quantidadeCaminho || alvoMudou)
        calcularCaminho(m, f, x, y);

    if (m->indiceCaminho >= m->quantidadeCaminho) return;

    Ponto alvo = m->caminho[m->indiceCaminho];
    float dx = alvo.x - m->corpo.x;
    float dy = alvo.y - m->corpo.y;
    float d = sqrtf(dx*dx + dy*dy);

    if (d < 7.0f)
    {
        m->indiceCaminho++;
        return;
    }

    dx /= d;
    dy /= d;
    m->corpo.direcao = atan2f(dy, dx);
    m->direcaoSprite = direcaoSpritePorMovimento(dx, dy, m->direcaoSprite);
    m->movendo = true;

    float antesX = m->corpo.x, antesY = m->corpo.y;
    moverPersonagem(&m->corpo,
                    dx * m->corpo.velocidade * multiplicador * dt,
                    dy * m->corpo.velocidade * multiplicador * dt,
                    f);

    if (distancia(antesX, antesY, m->corpo.x, m->corpo.y) < 0.05f)
        m->tempoRecalcularCaminho = 0.0f;

    verificarAntiStuck(m, f, dt);
}

static bool chegouAlvoNavegavel(const Maria* m)
{
    return distancia(m->corpo.x, m->corpo.y,
                      m->alvoNavegavelX, m->alvoNavegavelY) < 28.0f;
}

static void escolherAlvoBusca(Maria* m, const Fase* f)
{
    for (int tentativa = 0; tentativa < 16; tentativa++)
    {
        float angulo = (float)(rand() % 628) / 100.0f;
        float raio = 55.0f + (float)(rand() % 140);
        float x = m->ultimaPosicaoVistaX + cosf(angulo) * raio;
        float y = m->ultimaPosicaoVistaY + sinf(angulo) * raio;

        if (!personagemColide(&m->corpo, x, y, f))
        {
            m->alvoX = x;
            m->alvoY = y;
            m->tempoRecalcularCaminho = 0.0f;
            return;
        }
    }

    m->alvoX = m->ultimaPosicaoVistaX;
    m->alvoY = m->ultimaPosicaoVistaY;
}

void atualizarMaria(Maria* m, const Scooby* s, EventoSom* som,
                    const Fase* f, RecursosAudio* audio, float dt)
{
    if (!m || !s || !f) return;

    EstadoMaria estadoAntes = m->estado;
    m->movendo = false;
    m->capturaConcluida = false;

    if (m->estado == MARIA_CAPTURAR)
    {
        if (atualizarAnimacaoUmaVez(&m->pick, dt)) m->capturaConcluida = true;
        return;
    }

    bool viuScooby = mariaVeScooby(m, s, f);

    if (viuScooby)
    {
        m->estado = MARIA_PERSEGUIR;
        m->alvoX = s->corpo.x;
        m->alvoY = s->corpo.y;
        m->ultimaPosicaoVistaX = s->corpo.x;
        m->ultimaPosicaoVistaY = s->corpo.y;
    }
    else
    {
        if (m->estado == MARIA_PERSEGUIR)
        {
            m->estado = MARIA_PROCURAR;
            m->alvoX = m->ultimaPosicaoVistaX;
            m->alvoY = m->ultimaPosicaoVistaY;
            m->tempoBusca = 4.0f;
            m->tempoNovoAlvoBusca = 0.8f;
            m->tempoRecalcularCaminho = 0.0f;
        }

        if (som && som->ativo && !som->processado && mariaOuveSom(m, som))
        {
            m->estado = MARIA_INVESTIGAR;
            m->alvoX = som->x;
            m->alvoY = som->y;
            m->ultimaPosicaoVistaX = som->x;
            m->ultimaPosicaoVistaY = som->y;
            m->tempoRecalcularCaminho = 0.0f;
            som->processado = true;
        }
    }

    if (audio && m->estado != estadoAntes &&
        (m->estado == MARIA_INVESTIGAR || m->estado == MARIA_PERSEGUIR))
        tocarEfeitoPosicional(audio->mariaAlerta, m->corpo.x, 0.52f);

    switch (m->estado)
    {
        case MARIA_PATRULHA:
        {
            if (f->quantidadeWaypoints <= 0) break;
            Ponto alvo = f->waypoints[m->waypointAtual];
            moverMaria(m, f, alvo.x, alvo.y, dt, 1.0f);
            if (distancia(m->corpo.x, m->corpo.y, alvo.x, alvo.y) < 30.0f)
            {
                m->waypointAtual = (m->waypointAtual + 1) % f->quantidadeWaypoints;
                m->tempoRecalcularCaminho = 0.0f;
            }
            break;
        }

        case MARIA_INVESTIGAR:
            moverMaria(m, f, m->alvoX, m->alvoY, dt, 1.08f);
            if (chegouAlvoNavegavel(m))
            {
                m->estado = MARIA_PROCURAR;
                m->ultimaPosicaoVistaX = m->alvoNavegavelX;
                m->ultimaPosicaoVistaY = m->alvoNavegavelY;
                m->tempoBusca = 3.2f;
                m->tempoNovoAlvoBusca = 0.0f;
            }
            break;

        case MARIA_PERSEGUIR:
            moverMaria(m, f, s->corpo.x, s->corpo.y, dt, 1.28f);
            break;

        case MARIA_PROCURAR:
            m->tempoBusca -= dt;
            m->tempoNovoAlvoBusca -= dt;
            if (m->tempoBusca <= 0.0f)
            {
                m->estado = MARIA_PATRULHA;
                m->tempoRecalcularCaminho = 0.0f;
            }
            else
            {
                if (m->tempoNovoAlvoBusca <= 0.0f || chegouAlvoNavegavel(m))
                {
                    escolherAlvoBusca(m, f);
                    m->tempoNovoAlvoBusca = 0.9f;
                }
                moverMaria(m, f, m->alvoX, m->alvoY, dt, 0.95f);
            }
            break;

        case MARIA_CAPTURAR:
            break;
    }

    if (mariaPodeCapturar(m, s, f))
    {
        m->estado = MARIA_CAPTURAR;
        m->movendo = false;
        reiniciarAnimacao(&m->pick);
        if (audio) tocarEfeitoPosicional(audio->captura, m->corpo.x, 0.78f);
        return;
    }

    if (m->estado == MARIA_PERSEGUIR)
    {
        int frameAnterior = m->run.frameAtual;
        atualizarAnimacaoLoop(&m->run, dt);
        if (m->movendo) tocarPassoMariaSeNecessario(m, audio, frameAnterior, true);
    }
    else if (m->movendo)
    {
        int frameAnterior = m->walk.frameAtual;
        atualizarAnimacaoLoop(&m->walk, dt);
        tocarPassoMariaSeNecessario(m, audio, frameAnterior, false);
    }
    else
        atualizarAnimacaoLoop(&m->idle, dt);
}
