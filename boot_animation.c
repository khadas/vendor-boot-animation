#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

/* media player */
#include "media_player.h"

/* signal process */
#include "signal_proc.h"

#define LOGI(fmt, ...) printf("[INFO] <%s>:<%d> " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGE(fmt, ...) printf("[ERROR] <%s>:<%d> " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)

typedef struct _BootAnimationCtrl
{
    sem_t sem_run; /* the run flag of boot animation */
    bool recv_sig; /* the flag of recevied signal from other process */
} BootAnimationCtrl;

BootAnimationCtrl g_ctrl = {.sem_run = {0},
                            .recv_sig = false};

/***************************************************
 * Static Function
 ***************************************************/
static void signal_handler(int sig)
{
    if ((SIGUSR2 == sig) && (false == g_ctrl.recv_sig))
    {
        /* After receiving the SIGUSR2, post sem notifies the main thread to end the wait */
        LOGI("received SIGUSR2, ready to exit the process!\n");
        g_ctrl.recv_sig = true;
        sem_post(&g_ctrl.sem_run);
    }

    return;
}

static void print_usage(void)
{
    printf("Usage: boot-animation [OPTION] ...\n");
    printf("-start\t\tstart boot-animation-player\n");
    printf("-stop <param>\tstop boot-animation-player\n");
    printf("\teg. boot-animation -stop <timeout>\n");
    printf("\t<timeout>\tblocking timeout,if not set,default:10s\n\n");
    return;
}

int main(int argc, char *argv[])
{
    int ret = 0;

    /* Check input param */
    if (2 > argc)
    {
        goto INV_PARAM;
    }

    if (0 == strcmp(argv[1], "-stop"))
    {
        int timeout = 10; /* default 10s */
        struct timeval now_time;
        struct timespec out_time;

        /* semaphore init */
        ret = sem_init(&g_ctrl.sem_run, 0, 0);
        if (0 != ret)
        {
            goto EXIT;
        }

        /* Login signal. For waiting for trigger signal */
        ret = signal_login(SIGUSR2, signal_handler);
        if (0 != ret)
        {
            goto EXIT;
        }

        /* Send sigusr1 to boot-animaiton-player. Notify boot-animation-player to end playback*/
        ret = signal_send(SIGUSR1, (char *)"boot-animation-player");
        if (0 != ret)
        {
            LOGI("boot animation is not started. just return!\n");
            goto EXIT;
        }

        /* Wait for the semaphore and end the process. */
        /* post sem in signal_handler */
        if ((3 == argc) && (0 < atoi(argv[2])))
        {
            timeout = atoi(argv[2]);
            LOGI("boot-animation stop timeout:%d(s)!\n", timeout);
        }
        gettimeofday(&now_time, NULL);
        out_time.tv_sec = now_time.tv_sec + timeout;
        out_time.tv_nsec = now_time.tv_usec * 1000;
        while ((-1 == (ret = sem_timedwait(&g_ctrl.sem_run, &out_time))) && (EINTR == errno))
            ;
        if (0 != ret)
        {
            LOGE("stop timeout, just exit, ret:%d, errno:%d!\n", ret, errno);
            goto EXIT;
        }
        sem_destroy(&g_ctrl.sem_run);
    }
    else if (0 == strcmp(argv[1], "-start"))
    {
        if (2 != argc)
        {
            goto INV_PARAM;
        }
        /* Start boot-animation-player, default:boot-animation-player /media/boot-animation.mp4 0 0 */
        ret = execl("/usr/bin/boot-animation-player", "boot-animation-player", "/media/boot-animation.mp4", "0", "0", NULL);
        if (-1 == ret)
        {
            LOGE("start boot-animation-player failed!\n");
            return -1;
        }
    }
    else
    {
        goto INV_PARAM;
    }
    LOGI("%s %s success!\n", argv[0], argv[1]);
    return 0;

INV_PARAM:
    LOGE("invalid argument!\n\n");
    print_usage();
EXIT:
    sem_destroy(&g_ctrl.sem_run);
    return -1;
}
