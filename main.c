#include "jogo.h"

int main(void)
{
    srand((unsigned int)time(NULL));

    if(!al_init()){printf("Erro ao iniciar Allegro.\n");return -1;}
    if(!al_install_keyboard()){printf("Erro ao iniciar teclado.\n");return -1;}
    al_init_primitives_addon();
    if(!al_init_image_addon()){printf("Erro ao iniciar allegro_image.\n");return -1;}

    bool audioInstalado=al_install_audio();
    if(audioInstalado)
    {
        if(!al_reserve_samples(24))
        {
            printf("Aviso: nao foi possivel reservar vozes de audio. O jogo continuara sem som.\n");
            al_uninstall_audio();
            audioInstalado=false;
        }
    }
    else printf("Aviso: audio nao disponivel. O jogo continuara sem som.\n");

    ALLEGRO_DISPLAY* display=al_create_display(LARGURA_TELA,ALTURA_TELA);
    ALLEGRO_TIMER* timer=al_create_timer(1.0/FPS);
    ALLEGRO_EVENT_QUEUE* fila=al_create_event_queue();
    if(!display||!timer||!fila)
    {
        printf("Erro ao criar display/timer/fila.\n");
        if(fila)al_destroy_event_queue(fila);
        if(timer)al_destroy_timer(timer);
        if(display)al_destroy_display(display);
        if(audioInstalado)al_uninstall_audio();
        al_shutdown_image_addon();
        al_shutdown_primitives_addon();
        return -1;
    }

    al_set_window_title(display,"Sausage Run");
    al_register_event_source(fila,al_get_display_event_source(display));
    al_register_event_source(fila,al_get_timer_event_source(timer));
    al_register_event_source(fila,al_get_keyboard_event_source());

    RecursosMapa recursos={0};
    RecursosAudio audio={0};
    Fase fases[QTD_FASES]={0};
    Scooby scooby={0};
    Maria maria={0};
    Bola bola={0};
    EventoSom som={SOM_NENHUM,0,0,0,0,false,false};

    scooby.corpo.largura=46;scooby.corpo.altura=30;scooby.corpo.velocidade=135;scooby.direcaoSprite=DIRECAO_UP;
    maria.corpo.largura=34;maria.corpo.altura=42;maria.corpo.velocidade=96;maria.direcaoSprite=DIRECAO_LEFT;
    maria.alcanceVisao=265;maria.anguloVisao=72.0f*PI/180.0f;maria.alcanceAudicao=430;

    if(!carregarRecursosMapa(&recursos)||!carregarSprites(&scooby,&maria))
    {
        printf("Nao foi possivel carregar todos os recursos obrigatorios.\n");
        destruirRecursos(&recursos,&scooby,&maria);
        destruirRecursosAudio(&audio);
        al_destroy_event_queue(fila);
        al_destroy_timer(timer);
        al_destroy_display(display);
        if(audioInstalado)al_uninstall_audio();
        al_shutdown_image_addon();
        al_shutdown_primitives_addon();
        return -1;
    }

    if(audioInstalado)criarRecursosAudio(&audio);
    configurarFases(fases,&recursos);

    int faseAtual=0,vidas=3;
    EstadoJogo estado=JOGO_RODANDO;
    bool executando=true,redesenhar=true,debug=false;

    resetarPersonagensNaFase(&scooby,&maria,&bola,&fases[faseAtual],faseAtual,true);
    al_start_timer(timer);

    while(executando)
    {
        ALLEGRO_EVENT ev;
        al_wait_for_event(fila,&ev);

        if(ev.type==ALLEGRO_EVENT_DISPLAY_CLOSE)executando=false;

        if(ev.type==ALLEGRO_EVENT_KEY_DOWN)
        {
            if(ev.keyboard.keycode==ALLEGRO_KEY_ESCAPE)executando=false;
            if(ev.keyboard.keycode==ALLEGRO_KEY_F1)debug=!debug;

            if(ev.keyboard.keycode==ALLEGRO_KEY_R&&estado!=JOGO_RODANDO)
            {
                faseAtual=0;vidas=3;estado=JOGO_RODANDO;
                maria.corpo.velocidade=96;maria.alcanceVisao=265;
                resetarPersonagensNaFase(&scooby,&maria,&bola,&fases[0],0,true);
            }

            if(estado==JOGO_RODANDO)
            {
                if(ev.keyboard.keycode==ALLEGRO_KEY_SPACE)iniciarLatido(&scooby,&som,&audio);
                if(ev.keyboard.keycode==ALLEGRO_KEY_E)iniciarMordida(&scooby,&bola,&audio);
            }
        }

        if(ev.type==ALLEGRO_EVENT_TIMER)
        {
            const float dt=1.0f/FPS;

            if(estado==JOGO_RODANDO)
            {
                Fase* fase=&fases[faseAtual];
                ALLEGRO_KEYBOARD_STATE teclado;
                al_get_keyboard_state(&teclado);

                atualizarSom(&som,dt);
                atualizarScooby(&scooby,&teclado,fase,&bola,&som,&audio,dt);
                atualizarMaria(&maria,&scooby,&som,fase,&audio,dt);

                if(maria.capturaConcluida)
                {
                    vidas--;
                    som.ativo=false;

                    if(vidas<=0)
                    {
                        estado=JOGO_GAME_OVER;
                        tocarEfeito(audio.gameOver,0.82f);
                        al_set_window_title(display,"Sausage Run - GAME OVER - pressione R para recomecar");
                    }
                    else
                    {
                        resetarPersonagensNaFase(&scooby,&maria,&bola,fase,faseAtual,false);
                    }
                }

                if(estado==JOGO_RODANDO&&chegouNaSaidaComBola(&scooby,fase))
                {
                    faseAtual++;
                    som.ativo=false;

                    if(faseAtual>=QTD_FASES)
                    {
                        estado=JOGO_VITORIA;
                        tocarEfeito(audio.vitoria,0.88f);
                        al_set_window_title(display,"Sausage Run - VOCE VENCEU! - pressione R para jogar novamente");
                    }
                    else
                    {
                        tocarEfeito(audio.faseCompleta,0.78f);
                        maria.corpo.velocidade=96.0f+faseAtual*6.0f;
                        maria.alcanceVisao=265.0f+faseAtual*10.0f;
                        resetarPersonagensNaFase(&scooby,&maria,&bola,&fases[faseAtual],faseAtual,true);
                    }
                }

                if(estado==JOGO_RODANDO)
                {
                    char titulo[256];
                    snprintf(titulo,sizeof(titulo),
                        "Sausage Run - Fase %d/4: %s | Vidas: %d | Bola %s | WASD mover | Shift correr | Espaco latir | E morder/pegar | F1 debug",
                        faseAtual+1,fases[faseAtual].nome,vidas,NOMES_CORES[bola.cor]);
                    al_set_window_title(display,titulo);
                }
            }

            redesenhar=true;
        }

        if(redesenhar&&al_is_event_queue_empty(fila))
        {
            redesenhar=false;
            if(estado==JOGO_RODANDO)
                desenharCena(&fases[faseAtual],&recursos,&scooby,&maria,&bola,&som,debug,vidas,faseAtual);
            else
                desenharTelaFinal(estado);
        }
    }

    destruirRecursosAudio(&audio);
    destruirRecursos(&recursos,&scooby,&maria);
    al_destroy_event_queue(fila);
    al_destroy_timer(timer);
    al_destroy_display(display);

    if(audioInstalado)al_uninstall_audio();
    al_shutdown_image_addon();
    al_shutdown_primitives_addon();
    return 0;
}
