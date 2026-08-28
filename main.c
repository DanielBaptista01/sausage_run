#include "jogo.h"

static void contexto(ALLEGRO_DISPLAY* d)
{
    if(!d)return;al_set_target_backbuffer(d);ALLEGRO_TRANSFORM t;al_identity_transform(&t);al_use_transform(&t);al_reset_clipping_rectangle();
}
static float clamp255(float v){return v<0?0:v>255?255:v;}

static void titulo(ALLEGRO_DISPLAY* d,EstadoJogo e,const Fase* f,int idx,int vidas,const Bola* b)
{
    char t[256];
    if(e==JOGO_VITORIA)snprintf(t,sizeof(t),"Sausage Run - VOCE VENCEU!");
    else if(e==JOGO_GAME_OVER)snprintf(t,sizeof(t),"Sausage Run - GAME OVER");
    else snprintf(t,sizeof(t),"Sausage Run - Fase %d/4: %s | Vidas: %d | Bola %s",idx+1,f?f->nome:"?",vidas,b?NOMES_CORES[b->cor]:"?");
    al_set_window_title(d,t);
}

static void iniciarTransicao(TransicaoFase* t,const Fase* f)
{
    t->etapa=TRANSICAO_APROXIMAR;t->tempo=0;t->alphaFade=0;t->alvo=f->alvoEntradaSaida;t->faseTrocada=false;
}

static void imprimirValidacao(const Fase fases[QTD_FASES],const Scooby* s,const Maria* m)
{
    printf("--- Validacao estrutural / regressao configuracional ---\n");
    srand(SEED_DEBUG);
    for(int fi=0;fi<QTD_FASES;fi++)
    {
        const Fase* f=&fases[fi];
        Scooby ts=*s; Maria tm=*m; Bola tb={0};
        int falhasSpawn=0, falhasWaypoint=0;

        for(int teste=0;teste<50;teste++)
        {
            resetarPersonagensNaFase(&ts,&tm,&tb,f,fi,true);
            Ponto p={tb.x,tb.y};
            if(tb.coletada || !spawnBolaValido(f,&ts.corpo,&tm.corpo,p) ||
               !pontoAlcancavel(f,&ts.corpo,(Ponto){ts.corpo.x,ts.corpo.y},p))
                falhasSpawn++;
        }

        for(int i=0;i<f->quantidadeWaypoints;i++)
        {
            Ponto a=(i==0)?f->spawnMaria:f->waypoints[i-1];
            Ponto b=f->waypoints[i];
            if(!pontoAlcancavel(f,&m->corpo,a,b)) falhasWaypoint++;
        }

        printf("F%d %-9s | objetos=%d obst=%d | spawn 50x: %s | rotas waypoint: %s\n",
               fi+1,f->nome,f->quantidadeObjetos,f->quantidadeObstaculos,
               falhasSpawn?"FALHA":"OK",falhasWaypoint?"FALHA":"OK");
        if(falhasSpawn) printf("  -> %d/50 resets com spawn invalido\n",falhasSpawn);
        if(falhasWaypoint) printf("  -> %d segmentos de patrulha sem rota\n",falhasWaypoint);
    }
    srand((unsigned int)time(NULL));
}

int main(void)
{
    srand((unsigned int)time(NULL));
    if(!al_init()){printf("ERRO al_init\n");return -1;}
    if(!al_install_keyboard()){printf("ERRO teclado\n");return -1;}
    if(!al_init_primitives_addon()){printf("ERRO primitives\n");return -1;}
    if(!al_init_image_addon()){printf("ERRO image addon\n");al_shutdown_primitives_addon();return -1;}
    al_init_font_addon();

    bool audioInstalado=al_install_audio();
    if(audioInstalado&&!al_reserve_samples(24)){printf("Audio sem vozes; seguindo sem audio.\n");al_uninstall_audio();audioInstalado=false;}

    ALLEGRO_DISPLAY* display=al_create_display(LARGURA_TELA,ALTURA_TELA);
    ALLEGRO_TIMER* timer=al_create_timer(1.0/FPS);
    ALLEGRO_EVENT_QUEUE* fila=al_create_event_queue();
    if(!display||!timer||!fila)
    {
        printf("ERRO display/timer/fila\n");if(fila)al_destroy_event_queue(fila);if(timer)al_destroy_timer(timer);if(display)al_destroy_display(display);
        if(audioInstalado)al_uninstall_audio();al_shutdown_font_addon();al_shutdown_image_addon();al_shutdown_primitives_addon();return -1;
    }
    contexto(display);
    al_register_event_source(fila,al_get_display_event_source(display));
    al_register_event_source(fila,al_get_timer_event_source(timer));
    al_register_event_source(fila,al_get_keyboard_event_source());

    ALLEGRO_FONT* fonteCarga=al_create_builtin_font();desenharCarregando(display,fonteCarga);

    RecursosMapa recursos={0};RecursosAudio audio={0};Fase fases[QTD_FASES]={0};Scooby scooby={0};Maria maria={0};Bola bola={0};
    EventoSom som={SOM_NENHUM,0,0,0,0,false,false};
    configurarFases(fases);

    /*
     * Estes quatro campos permanecem apenas para rotinas genericas de
     * validacao/spawn. A colisao real do Scooby usa HitboxScooby (corpo +
     * cabeca) calculada por obterHitboxScooby() conforme direcaoSprite.
     */
    scooby.corpo.velocidade=135;
    scooby.corpo.hitboxLargura=48;
    scooby.corpo.hitboxAltura=24;
    scooby.corpo.hitboxOffsetX=0;
    scooby.corpo.hitboxOffsetY=-31;
    scooby.direcaoSprite=DIRECAO_UP;

    maria.corpo.velocidade=96;maria.corpo.hitboxLargura=28;maria.corpo.hitboxAltura=20;maria.corpo.hitboxOffsetX=0;maria.corpo.hitboxOffsetY=-10;maria.direcaoSprite=DIRECAO_LEFT;
    maria.alcanceVisao=265;maria.anguloVisao=72.0f*PI/180.0f;maria.alcanceAudicao=430;

    bool ok=carregarRecursosMapa(&recursos)&&carregarSprites(&scooby,&maria)&&carregarRecursosFase(&recursos,fases,0);
    if(fonteCarga)al_destroy_font(fonteCarga);
    if(!ok)
    {
        printf("ERRO: recursos obrigatorios.\n");destruirRecursos(&recursos,fases,&scooby,&maria);destruirRecursosAudio(&audio);
        al_destroy_event_queue(fila);al_destroy_timer(timer);al_destroy_display(display);if(audioInstalado)al_uninstall_audio();
        al_shutdown_font_addon();al_shutdown_image_addon();al_shutdown_primitives_addon();return -1;
    }
    contexto(display);if(audioInstalado)criarRecursosAudio(&audio);

    for(int i=0;i<QTD_FASES;i++)validarConfiguracaoFase(&fases[i],&scooby,&maria,i);
    imprimirValidacao(fases,&scooby,&maria);

    int faseAtual=0,vidas=3;EstadoJogo estado=JOGO_RODANDO,antesPausa=JOGO_RODANDO;
    bool executando=true,redesenhar=true,debug=false;float tutorial=8.0f;
    TransicaoFase trans={TRANSICAO_APROXIMAR,0,0,{0,0},false};
    resetarPersonagensNaFase(&scooby,&maria,&bola,&fases[0],0,true);titulo(display,estado,&fases[0],0,vidas,&bola);
    al_start_timer(timer);

    while(executando)
    {
        ALLEGRO_EVENT ev;al_wait_for_event(fila,&ev);
        if(ev.type==ALLEGRO_EVENT_DISPLAY_CLOSE)executando=false;
        if(ev.type==ALLEGRO_EVENT_DISPLAY_SWITCH_OUT&&estado==JOGO_RODANDO){antesPausa=estado;estado=JOGO_PAUSADO;redesenhar=true;}

        if(ev.type==ALLEGRO_EVENT_KEY_DOWN)
        {
            int k=ev.keyboard.keycode;
            if(k==ALLEGRO_KEY_ESCAPE)
            {
                if(estado==JOGO_VITORIA||estado==JOGO_GAME_OVER)executando=false;
                else if(estado==JOGO_RODANDO){antesPausa=estado;estado=JOGO_PAUSADO;}
                else if(estado==JOGO_PAUSADO)estado=antesPausa;
                redesenhar=true;
            }
            if(k==ALLEGRO_KEY_F1){debug=!debug;redesenhar=true;printf("Debug F1: %s\n",debug?"ON":"OFF");}
            if(k==ALLEGRO_KEY_R&&(estado==JOGO_VITORIA||estado==JOGO_GAME_OVER))
            {
                faseAtual=0;vidas=3;estado=JOGO_RODANDO;antesPausa=JOGO_RODANDO;tutorial=8;
                maria.corpo.velocidade=96;maria.alcanceVisao=265;
                if(!carregarRecursosFase(&recursos,fases,0)){estado=JOGO_GAME_OVER;}
                else{validarConfiguracaoFase(&fases[0],&scooby,&maria,0);resetarPersonagensNaFase(&scooby,&maria,&bola,&fases[0],0,true);som.ativo=false;}
                titulo(display,estado,&fases[faseAtual],faseAtual,vidas,&bola);
            }
            if(estado==JOGO_RODANDO&&maria.estado!=MARIA_CAPTURAR)
            {
                if(k==ALLEGRO_KEY_SPACE)iniciarLatido(&scooby,&som,&audio);
                if(k==ALLEGRO_KEY_E)iniciarMordida(&scooby,&bola,&audio);
            }
        }

        if(ev.type==ALLEGRO_EVENT_TIMER)
        {
            float dt=1.0f/FPS;
            if(estado==JOGO_RODANDO)
            {
                Fase* f=&fases[faseAtual];ALLEGRO_KEYBOARD_STATE teclado;al_get_keyboard_state(&teclado);atualizarSom(&som,dt);
                if(maria.estado!=MARIA_CAPTURAR)atualizarScooby(&scooby,&teclado,f,&bola,&som,&audio,dt);
                else{scooby.movendo=false;scooby.correndo=false;scooby.latindo=false;scooby.mordendo=false;}
                atualizarMaria(&maria,&scooby,&som,f,&audio,dt);

                if(maria.capturaConcluida)
                {
                    vidas--;som.ativo=false;
                    if(vidas<=0){estado=JOGO_GAME_OVER;tocarEfeito(audio.gameOver,.82f);}
                    else resetarPersonagensNaFase(&scooby,&maria,&bola,f,faseAtual,false);
                    titulo(display,estado,f,faseAtual,vidas,&bola);
                }
                else if(maria.estado!=MARIA_CAPTURAR&&chegouNaSaidaComBola(&scooby,f))
                {
                    estado=JOGO_TRANSICAO_FASE;som.ativo=false;iniciarTransicao(&trans,f);tocarEfeito(audio.faseCompleta,.72f);
                }
                if(tutorial>0)tutorial-=dt;
            }
            else if(estado==JOGO_TRANSICAO_FASE)
            {
                Fase* f=&fases[faseAtual];trans.tempo+=dt;
                if(trans.etapa==TRANSICAO_APROXIMAR)
                {
                    atualizarScoobyTransicao(&scooby,f,&bola,trans.alvo,dt);
                    if(trans.tempo>=.48f||distancia(scooby.corpo.x,scooby.corpo.y,trans.alvo.x,trans.alvo.y)<5)
                    {trans.etapa=TRANSICAO_FADE_OUT;trans.tempo=0;}
                }
                else if(trans.etapa==TRANSICAO_FADE_OUT)
                {
                    scooby.movendo=false;trans.alphaFade=clamp255((trans.tempo/.78f)*255.0f);
                    if(trans.tempo>=.78f&&!trans.faseTrocada)
                    {
                        trans.alphaFade=255;trans.faseTrocada=true;
                        if(faseAtual+1>=QTD_FASES){estado=JOGO_VITORIA;tocarEfeito(audio.vitoria,.88f);}
                        else
                        {
                            faseAtual++;
                            if(!carregarRecursosFase(&recursos,fases,faseAtual)){printf("ERRO carregar fase %d\n",faseAtual+1);estado=JOGO_GAME_OVER;}
                            else
                            {
                                maria.corpo.velocidade=96+faseAtual*6;maria.alcanceVisao=265+faseAtual*10;
                                validarConfiguracaoFase(&fases[faseAtual],&scooby,&maria,faseAtual);
                                resetarPersonagensNaFase(&scooby,&maria,&bola,&fases[faseAtual],faseAtual,true);
                                trans.etapa=TRANSICAO_FADE_IN;trans.tempo=0;trans.alphaFade=255;
                            }
                        }
                        titulo(display,estado,&fases[faseAtual],faseAtual,vidas,&bola);
                    }
                }
                else
                {
                    trans.alphaFade=clamp255(255.0f*(1.0f-trans.tempo/.55f));
                    if(trans.tempo>=.55f){trans.alphaFade=0;trans.faseTrocada=false;estado=JOGO_RODANDO;titulo(display,estado,&fases[faseAtual],faseAtual,vidas,&bola);}
                }
            }
            redesenhar=true;
        }

        if(redesenhar&&al_is_event_queue_empty(fila))
        {
            redesenhar=false;contexto(display);
            if(estado==JOGO_RODANDO||estado==JOGO_PAUSADO||estado==JOGO_TRANSICAO_FASE)
                desenharCena(&fases[faseAtual],&recursos,&scooby,&maria,&bola,&som,debug,vidas,faseAtual,estado,trans.alphaFade,tutorial);
            else desenharTelaFinal(estado,&recursos);
        }
    }

    contexto(display);destruirRecursosAudio(&audio);destruirRecursos(&recursos,fases,&scooby,&maria);
    al_destroy_event_queue(fila);al_destroy_timer(timer);al_destroy_display(display);if(audioInstalado)al_uninstall_audio();
    al_shutdown_font_addon();al_shutdown_image_addon();al_shutdown_primitives_addon();return 0;
}
