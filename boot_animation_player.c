#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

#include "media_player.h"
#include "signal_proc.h"

#define LOGI(fmt, ...) printf("[INFO] <%s>:<%d> " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGE(fmt, ...) printf("[ERROR] <%s>:<%d> " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define BOOT_ANIMATION_PLAY_ONCE 0   /* only once, default*/
#define BOOT_ANIMATION_PLAY_REPEAT 1 /* repeat play */

#define BOOT_ANIMATION_STOP_NOW 0   /* stop now, default */
#define BOOT_ANIMARION_STOP_DELAY 1 /* delay stop until the end of this playback */

typedef struct _BootAnimationPlayer
{
    sem_t sem_run; /* the run flag of boot animation */
    bool recv_sig; /* the flag of recevied signal from other process */
    int play_mode; /* boot animation player play mode */
    int stop_mode; /* boot animation player stop mode */
    void *player;  /* media player hdl */
    bool end_play; /* end play flag */
} BootAnimationPlayer;

BootAnimationPlayer g_player_ctrl =
    {
        .sem_run = {0},
        .recv_sig = false,
        .play_mode = BOOT_ANIMATION_PLAY_ONCE,
        .stop_mode = BOOT_ANIMATION_STOP_NOW,
        .player = NULL,
        .end_play = false,

};

static void signal_handler(int sig)
{
    if ((SIGUSR1 == sig) && (false == g_player_ctrl.recv_sig))
    {
        LOGI("received SIGUSR1, ready to exit the process!\n");
        g_player_ctrl.recv_sig = true;
        sem_post(&g_player_ctrl.sem_run);
    }

    return;
}

static void print_usage(void)
{
    printf("Usage: boot-animation-player <file> <play_mode> <stop_mode>\n");
    printf("\t<file>\t\tabsolete path must be used,like:\"/media/boot-animation.mp4\"\n");
    printf("\t<play_mode>\t0:play only once; 1:repeat play\n");
    printf("\t<stop_mode>\t0:stop now; 1:delay stop\n\n");

    return;
}

static void msg_cb(PlayerMessageCode msg_code, void *user_data)
{
    BootAnimationPlayer *ctrl = (BootAnimationPlayer *)user_data;
    switch (msg_code)
    {
    case PLAYER_MSG_ERROR:
        LOGE("media player proc error, end play!\n");
        ctrl->end_play = true;
        sem_post(&ctrl->sem_run);
        break;
    case PLAYER_MSG_EOS:
        if (BOOT_ANIMATION_PLAY_REPEAT == ctrl->play_mode)
        {
            if (true == ctrl->recv_sig)
            {
                LOGI("end play!\n");
                ctrl->end_play = true;
                sem_post(&ctrl->sem_run);
            }
            else
            {
                media_player_seek(ctrl->player, 0);
                media_player_play(ctrl->player);
            }
        }
        else /* play once */
        {
            ctrl->end_play = true;
            sem_post(&ctrl->sem_run);
        }
        break;
    default:
        break;
    }

    return;
}

int main(int argc, char **argv)
{
    int ret = 0;
    pthread_t tid = 0;
    char uri[512] = {0};

    /* sem init */
    ret = sem_init(&g_player_ctrl.sem_run, 0, 0);
    if (0 != ret)
    {
        LOGE("sem_init fail!\n");
        return -1;
    }

    /* Login signal, for IPC */
    ret = signal_login(SIGUSR1, signal_handler);
    if (0 != ret)
    {
        sem_destroy(&g_player_ctrl.sem_run);
        LOGE("signal_login fail!\n");
        return -1;
    }

    /* Check input param */
    if (argc < 4)
    {
        goto INV_PARAM;
    }
    if (((sizeof(uri) - 8) < strlen(argv[1])) || ('/' != argv[1][0]))
    {
        goto INV_PARAM;
    }
    snprintf(uri, sizeof(uri), "file://%s", argv[1]);
    g_player_ctrl.play_mode = atoi(argv[2]);
    g_player_ctrl.stop_mode = atoi(argv[3]);
    if ((0 != g_player_ctrl.play_mode) && (1 != g_player_ctrl.play_mode))
    {
        goto INV_PARAM;
    }
    if ((0 != g_player_ctrl.stop_mode) && (1 != g_player_ctrl.stop_mode))
    {
        goto INV_PARAM;
    }

    /* Start media player */
    media_player_create(&g_player_ctrl.player, argc, argv);
    media_player_set_uri(g_player_ctrl.player, (char *)uri);
    media_player_set_msgcb(g_player_ctrl.player, msg_cb, (void *)&g_player_ctrl);
    pthread_create(&tid, NULL, media_player_workloop, g_player_ctrl.player);
    media_player_play(g_player_ctrl.player);

    /* Wait for the end of playback */
    do
    {
        sem_wait(&g_player_ctrl.sem_run);
        if ((BOOT_ANIMATION_STOP_NOW == g_player_ctrl.stop_mode) || (true == g_player_ctrl.end_play))
        {
            break;
        }
        sem_wait(&g_player_ctrl.sem_run);
    } while (0);

    /* Release */
    media_player_destroy(g_player_ctrl.player);
    pthread_join(tid, NULL);
    if (true == g_player_ctrl.recv_sig)
    {
        /* Send sigusr2 to boot-animation */
        signal_send(SIGUSR2, (char *)"boot-animation");
    }
    sem_destroy(&g_player_ctrl.sem_run);
    return 0;

INV_PARAM:
    LOGE("invalid param!\n");
    print_usage();
    sem_destroy(&g_player_ctrl.sem_run);
    return -1;
}
