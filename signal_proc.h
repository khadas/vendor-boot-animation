#ifndef __SIGNAL_PROC_H__
#define __SIGNAL_PROC_H__

#include <signal.h>

typedef void (*SIG_HDL)(int sig);

int signal_login(int sig, SIG_HDL sig_hdl);
int signal_send(int sig, char *app_str);

#endif // DEBUG