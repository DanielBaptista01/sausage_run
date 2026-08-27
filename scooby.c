#include "jogo.h"

bool bolaNaFrenteDoScooby(const Scooby* s,const Bola* b)
{
    if(!s||!b||b->coletada)return false;
    float d=distancia(s->corpo.x,s->corpo.y,b->x,b->y);if(d>82)return false;
    float a=atan2f(b->y-s->corpo.y,b->x-s->corpo.x);
    return fabsf(normalizarAngulo(a-s->corpo.direcao))<=85.0f*PI/180.0f;
}

void iniciarLatido(Scooby* s,EventoSom* som,RecursosAudio* audio)
{
    if(!s||s->latindo||s->mordendo||s->carregandoBola)return;
    s->latindo=true;s->movendo=false;s->correndo=false;reiniciarAnimacao(&s->bark);
    emitirSom(som,SOM_LATIDO,s->corpo.x,s->corpo.y,420);
    if(audio)tocarEfeitoPosicional(audio->scoobyLatido,s->corpo.x,.95f);
}

void iniciarMordida(Scooby* s,const Bola* b,RecursosAudio* audio)
{
    if(!s||!b||s->latindo||s->mordendo||s->carregandoBola)return;
    s->mordendo=true;s->movendo=false;s->correndo=false;s->coletaPendente=bolaNaFrenteDoScooby(s,b);reiniciarAnimacao(&s->bite);
    if(audio)tocarEfeitoPosicional(audio->scoobyMordida,s->corpo.x,.72f);
}

static void passo(Scooby* s,RecursosAudio* audio,int anterior,bool corrida,Animacao* a)
{
    if(!s||!audio||!a||a->frameAtual==anterior||(a->frameAtual!=1&&a->frameAtual!=3))return;
    tocarEfeitoPosicional(corrida?audio->scoobyCorrida:audio->scoobyPasso,s->corpo.x,corrida?.40f:.28f);
}

void atualizarScooby(Scooby* s,const ALLEGRO_KEYBOARD_STATE* teclado,const Fase* f,
                     Bola* b,EventoSom* som,RecursosAudio* audio,float dt)
{
    if(!s||!teclado||!f||!b)return;
    s->movendo=false;s->correndo=false;

    if(s->latindo){if(atualizarAnimacaoUmaVez(&s->bark,dt))s->latindo=false;return;}
    if(s->mordendo)
    {
        if(atualizarAnimacaoUmaVez(&s->bite,dt))
        {
            s->mordendo=false;
            if(s->coletaPendente&&bolaNaFrenteDoScooby(s,b))
            {
                b->coletada=true;s->carregandoBola=true;emitirSom(som,SOM_INTERACAO,s->corpo.x,s->corpo.y,135);
                reiniciarAnimacao(&s->carregar[b->cor]);if(audio)tocarEfeitoPosicional(audio->coletaBola,s->corpo.x,.82f);
            }
            s->coletaPendente=false;
        }
        return;
    }

    float dx=0,dy=0;if(al_key_down(teclado,ALLEGRO_KEY_W))dy-=1;if(al_key_down(teclado,ALLEGRO_KEY_S))dy+=1;
    if(al_key_down(teclado,ALLEGRO_KEY_A))dx-=1;if(al_key_down(teclado,ALLEGRO_KEY_D))dx+=1;
    s->movendo=fabsf(dx)>.01f||fabsf(dy)>.01f;
    s->correndo=s->movendo&&al_key_down(teclado,ALLEGRO_KEY_LSHIFT);

    if(s->movendo)
    {
        float n=sqrtf(dx*dx+dy*dy);dx/=n;dy/=n;s->corpo.direcao=atan2f(dy,dx);s->direcaoSprite=direcaoSpritePorMovimento(dx,dy,s->direcaoSprite);
        float v=s->correndo?235.0f:135.0f;moverPersonagem(&s->corpo,dx*v*dt,dy*v*dt,f);
    }

    if(s->cooldownSomCorrida>0)s->cooldownSomCorrida-=dt;
    if(s->correndo&&s->cooldownSomCorrida<=0){emitirSom(som,SOM_CORRIDA,s->corpo.x,s->corpo.y,220);s->cooldownSomCorrida=.30f;}

    Animacao* a=&s->idle;if(s->carregandoBola)a=&s->carregar[b->cor];else if(s->correndo)a=&s->run;else if(s->movendo)a=&s->walk;
    int fr=a->frameAtual;atualizarAnimacaoLoop(a,dt);if(s->movendo)passo(s,audio,fr,s->correndo,a);
}

void atualizarScoobyTransicao(Scooby* s,const Fase* f,const Bola* b,Ponto alvo,float dt)
{
    if(!s||!f||!b)return;
    s->latindo=false;s->mordendo=false;s->coletaPendente=false;s->correndo=false;
    float dx=alvo.x-s->corpo.x,dy=alvo.y-s->corpo.y,d=sqrtf(dx*dx+dy*dy);
    if(d>2.5f)
    {
        dx/=d;dy/=d;s->movendo=true;s->corpo.direcao=atan2f(dy,dx);s->direcaoSprite=direcaoSpritePorMovimento(dx,dy,s->direcaoSprite);
        moverPersonagem(&s->corpo,dx*55.0f*dt,dy*55.0f*dt,f);
    }
    else s->movendo=false;
    atualizarAnimacaoLoop(s->carregandoBola?&s->carregar[b->cor]:&s->walk,dt);
}
