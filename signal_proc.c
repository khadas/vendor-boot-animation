#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "signal_proc.h"

int signal_login(int sig, SIG_HDL sig_hdl)
{
    int ret = 0;
    struct sigaction st_act;
    memset(&st_act, 0, sizeof(struct sigaction));

    st_act.sa_handler = sig_hdl;
    st_act.sa_flags |= SA_RESTART; /* Ensure that interrupted system calls can be restarted */
    ret = sigaction(sig, &st_act, NULL);
    if (0 > ret)
    {
        printf("login sigaction failed!\n");
        return -1;
    }

    return 0;
}

int signal_send(int sig, char *app_str)
{
    int ret = -1;
    FILE *fp = NULL;
    char cmd_buf[256] = {0};
    char buffer[8] = {0};
    int curr_pid = 0;

    snprintf(cmd_buf, sizeof(cmd_buf), "ps -e | grep \'%s\' | grep -v \'grep\' | awk \'{print $1}\'", app_str);
    fp = popen((const char *)cmd_buf, "r");
    if (NULL == fp)
    {
        printf("failed to query PID!\n");
        return ret;
    }
    curr_pid = getpid();
    while (NULL != fgets(buffer, sizeof(buffer), fp))
    {
        int tmp_pid = atoi(buffer);
        if ((0 < tmp_pid) && (curr_pid != tmp_pid))
        {
            kill(tmp_pid, sig);
            ret = 0;
        }
    }
    pclose(fp);

    return ret;
}