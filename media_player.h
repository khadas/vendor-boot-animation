#ifndef __MEDIA_PLAYER_H__
#define __MEDIA_PLAYER_H__

#include <stdio.h>
#include <stdlib.h>

/* callback message type */
typedef enum
{
    PLAYER_MSG_OK = 0,
    /* common error code */
    PLAYER_MSG_ERROR = 0x1000,
    /* status code */
    PLAYER_MSG_EOS = 0xA000,
} PlayerMessageCode;

/* error callback */
typedef void (*MsgCb)(PlayerMessageCode msg_code, void *user_data);

/* api */
int media_player_create(void **phdl, int argc, char **argv);

void media_player_destroy(void *hdl);

int media_player_set_msgcb(void *hdl, MsgCb msgcb, void *user_data);

int media_player_set_uri(void *hdl, char *uri);

int media_player_play(void *hdl);

int media_player_pause(void *hdl);

int media_player_resume(void *hdl);

int media_player_stop(void *hdl);

/* pos in seconds */
int media_player_seek(void *hdl, unsigned int pos);

void *media_player_workloop(void *hdl);

#endif
