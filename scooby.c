#include "jogo.h"

bool bolaNaFrenteDoScooby(const Scooby* scooby,const Bola* bola)
{
    if(bola->coletada)return false;
    float d=distancia(scooby->corpo.x,scooby->corpo.y,bola->x,bola->y);
    if(d>88.0f)return false;
    float a=atan2f(bola->y-scooby->corpo.y,bola->x-scooby->corpo.x);
    return fabsf(normalizarAngulo(a-scooby->corpo.direcao))<=80.0f*PI/180.0f;
}

void iniciarLatido(Scooby* s,EventoSom* som)
{
    if(s->latindo||s->mordendo||s->carregandoBola)return;
    s->latindo=true; reiniciarAnimacao(&s->bark);
    emitirSom(som,SOM_LATIDO,s->corpo.x,s->corpo.y,420.0f);
}

void iniciarMordida(Scooby* s,const Bola* bola)
{
    if(s->latindo||s->mordendo||s->carregandoBola)return;
    s->mordendo=true; s->coletaPendente=bolaNaFrenteDoScooby(s,bola); reiniciarAnimacao(&s->bite);
}

void atualizarScooby(Scooby* s,const ALLEGRO_KEYBOARD_STATE* teclado,const Fase* fase,Bola* bola,EventoSom* som,float dt)
{
    s->movendo=false; s->correndo=false;
    if(s->latindo){if(atualizarAnimacaoUmaVez(&s->bark,dt))s->latindo=false;return;}
    if(s->mordendo)
    {
        if(atualizarAnimacaoUmaVez(&s->bite,dt))
        {
            s->mordendo=false;
            if(s->coletaPendente&&bolaNaFrenteDoScooby(s,bola))
            {
                bola->coletada=true; s->carregandoBola=true;
                emitirSom(som,SOM_INTERACAO,s->corpo.x,s->corpo.y,135.0f);
                reiniciarAnimacao(&s->carregar[bola->cor]);
            }
            s->coletaPendente=false;
        }
        return;
    }
    float dx=0,dy=0;
    if(al_key_down(teclado,ALLEGRO_KEY_W))dy-=1;
    if(al_key_down(teclado,ALLEGRO_KEY_S))dy+=1;
    if(al_key_down(teclado,ALLEGRO_KEY_A))dx-=1;
    if(al_key_down(teclado,ALLEGRO_KEY_D))dx+=1;
    s->movendo=fabsf(dx)>.01f||fabsf(dy)>.01f;
    s->correndo=s->movendo&&al_key_down(teclado,ALLEGRO_KEY_LSHIFT);
    if(s->movendo)
    {
        float t=sqrtf(dx*dx+dy*dy);dx/=t;dy/=t;
        s->corpo.direcao=atan2f(dy,dx);s->direcaoSprite=direcaoSpritePorMovimento(dx,dy,s->direcaoSprite);
        float v=s->correndo?235.0f:135.0f;
        moverPersonagem(&s->corpo,dx*v*dt,dy*v*dt,fase);
    }
    if(s->cooldownSomCorrida>0)s->cooldownSomCorrida-=dt;
    if(s->correndo&&s->cooldownSomCorrida<=0)
    {
        emitirSom(som,SOM_CORRIDA,s->corpo.x,s->corpo.y,220.0f);s->cooldownSomCorrida=.30f;
    }
    if(s->carregandoBola)atualizarAnimacaoLoop(&s->carregar[bola->cor],dt);
    else if(s->correndo)atualizarAnimacaoLoop(&s->run,dt);
    else if(s->movendo)atualizarAnimacaoLoop(&s->walk,dt);
    else atualizarAnimacaoLoop(&s->idle,dt);
}
