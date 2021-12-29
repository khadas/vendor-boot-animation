#include <gst/gst.h>
#include <unistd.h>
#include "media_player.h"

/* Structure to contain all our information, so we can pass it around */
typedef struct _CustomData
{
    gboolean init;        /* media player init flag */
    GstElement *pipeline; /* Our pipeline */
    GMainLoop *main_loop; /* GLib's Main Loop */
    MsgCb msgcb;          /* error message callback */
    void *user_data;      /* msgcd's user data */

    gboolean playing;      /* media player playing flag */
    gboolean seek_enabled; /* Is seeking enabled for this media? */
    gint64 duration;       /* How long does this media last, in nanoseconds */
} CustomData;

/* playbin flags */
typedef enum
{
    GST_PLAY_FLAG_VIDEO = (1 << 0), /* We want video output */
    GST_PLAY_FLAG_AUDIO = (1 << 1), /* We want audio output */
    GST_PLAY_FLAG_TEXT = (1 << 2)   /* We want subtitle output */
} GstPlayFlags;

/* Forward definition for the message and keyboard processing functions */
static gboolean handle_message(GstBus *bus, GstMessage *msg, CustomData *hdl);
static void enable_factory(const gchar *name, gboolean enable);

/***************************************************
 * External Api
 ***************************************************/
int media_player_create(void **phdl)
{
    /* Check input param */
    if (NULL == phdl)
    {
        g_print("invalid param!\n");
        return -1;
    }

    /* Malloc media player data */
    CustomData *data = (CustomData *)malloc(sizeof(CustomData));
    if (NULL == data)
    {
        return -1;
    }
    memset(data, 0, sizeof(CustomData));

    /* Initialize GStreamer */
    gst_init(NULL, NULL);
    enable_factory((const char *)"amladec", FALSE);
    enable_factory((const char *)"amlasink", FALSE);

    /* Build the pipeline and main loop  */
    data->pipeline = gst_parse_launch("playbin", NULL);
    data->main_loop = g_main_loop_new(NULL, FALSE);
    data->init = TRUE;
    data->playing = FALSE;

    /* Retrun handler */
    *phdl = data;

    return 0;
}

void media_player_destroy(void *hdl)
{
    if (NULL == hdl)
    {
        g_print("invalid param, call order error!\n");
        return;
    }
    CustomData *data = (CustomData *)hdl;
    /* Free resources */
    data->init = FALSE;
    data->playing = FALSE;
    g_main_loop_quit(data->main_loop);
    g_main_loop_unref(data->main_loop);
    gst_element_set_state(data->pipeline, GST_STATE_NULL);
    gst_object_unref(data->pipeline);
    free(data);

    return;
}

int media_player_set_msgcb(void *hdl, MsgCb msgcb, void *user_data)
{
    /* Check input param */
    if ((NULL == hdl) || (NULL == msgcb))
    {
        return -1;
    }
    CustomData *data = (CustomData *)hdl;
    data->msgcb = msgcb;
    data->user_data = user_data;

    return 0;
}

int media_player_set_uri(void *hdl, char *uri)
{
    /* Check input param */
    if ((NULL == hdl) || (NULL == uri))
    {
        return -1;
    }
    CustomData *data = (CustomData *)hdl;
    /* Set playbin's uri */
    g_object_set(GST_OBJECT(data->pipeline), "uri", uri, NULL);

    return 0;
}

int media_player_play(void *hdl)
{
    /* Check input param */
    if (NULL == hdl)
    {
        return -1;
    }
    CustomData *data = (CustomData *)hdl;
    /* Start playing */
    gst_element_set_state(data->pipeline, GST_STATE_PLAYING);
    return 0;
}

int media_player_pause(void *hdl)
{
    /* Check input param */
    if (NULL == hdl)
    {
        return -1;
    }
    CustomData *data = (CustomData *)hdl;
    /* Pause playing */
    gst_element_set_state(data->pipeline, GST_STATE_PAUSED);
    return 0;
}

int media_player_resume(void *hdl)
{
    /* Check input param */
    if (NULL == hdl)
    {
        return -1;
    }
    CustomData *data = (CustomData *)hdl;
    /* Resume playing */
    gst_element_set_state(data->pipeline, GST_STATE_PLAYING);
    return 0;
}

int media_player_stop(void *hdl)
{
    /* Check input param */
    if (NULL == hdl)
    {
        return -1;
    }
    CustomData *data = (CustomData *)hdl;
    /* Stop playing */
    gst_element_set_state(data->pipeline, GST_STATE_NULL);
    return 0;
}

int media_player_seek(void *hdl, unsigned int pos)
{
    /* Check input param */
    if (NULL == hdl)
    {
        return -1;
    }
    CustomData *data = (CustomData *)hdl;

    if (TRUE != data->seek_enabled)
    {
        return -1;
    }
    if (TRUE != gst_element_query_duration(data->pipeline, GST_FORMAT_TIME, &data->duration))
    {
        return -1;
    }
    pos = (pos > data->duration / 1000000000) ? (data->duration / 1000000000) : pos;
    gst_element_seek_simple(data->pipeline, GST_FORMAT_TIME,
                            (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), pos * GST_SECOND);
    return 0;
}

void *media_player_workloop(void *hdl)
{
    /* Check input param */
    if (NULL == hdl)
    {
        g_print("invalid param!\n");
        return NULL;
    }
    CustomData *data = (CustomData *)hdl;
    /* Add bus watch */
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(data->pipeline));
    gst_bus_add_watch(bus, (GstBusFunc)handle_message, data);
    gst_object_unref(bus);
    g_main_loop_run(data->main_loop);

    return NULL;
}

/***************************************************
 * Static Function
 ***************************************************/
/* Process messages from GStreamer */
static gboolean handle_message(GstBus *bus, GstMessage *msg, CustomData *data)
{
    GError *err;
    gchar *debug_info;

    bus = bus;
    switch (GST_MESSAGE_TYPE(msg))
    {
    case GST_MESSAGE_ERROR:
        gst_message_parse_error(msg, &err, &debug_info);
        g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
        g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
        g_clear_error(&err);
        g_free(debug_info);
        g_main_loop_quit(data->main_loop);
        if (NULL != data->msgcb)
        {
            data->msgcb(PLAYER_MSG_ERROR, data->user_data);
        }
        break;
    case GST_MESSAGE_EOS:
        g_print("End-Of-Stream reached.\n");
        if (NULL != data->msgcb)
        {
            data->msgcb(PLAYER_MSG_EOS, data->user_data);
        }
        else
        {
            g_main_loop_quit(data->main_loop);
        }
        break;
    case GST_MESSAGE_STATE_CHANGED:
    {
        GstState old_state, new_state, pending_state;
        gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->pipeline))
        {
            data->playing = FALSE;
            if (new_state == GST_STATE_PLAYING)
            {
                GstQuery *query;
                gint64 start, end;
                data->playing = TRUE;
                query = gst_query_new_seeking(GST_FORMAT_TIME);
                if (gst_element_query(data->pipeline, query))
                {
                    gst_query_parse_seeking(query, NULL, &data->seek_enabled, &start, &end);
                    if (data->seek_enabled)
                    {
                        /* g_print("Seeking is ENABLED from %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT "\n",
                                GST_TIME_ARGS(start), GST_TIME_ARGS(end)); */
                    }
                    else
                    {
                        g_print("Seeking is DISABLED for this stream.\n");
                    }
                }
                else
                {
                    g_printerr("Seeking query failed.");
                }
            }
        }
        break;
    }
    default:
        break;
    }

    /* We want to keep receiving messages */
    return TRUE;
}

static void enable_factory(const gchar *name, gboolean enable)
{
    GstRegistry *registry = NULL;
    GstElementFactory *factory = NULL;

    registry = gst_registry_get();
    if (!registry)
    {
        return;
    }

    factory = gst_element_factory_find(name);
    if (!factory)
    {
        return;
    }

    if (enable)
    {
        gst_plugin_feature_set_rank(GST_PLUGIN_FEATURE(factory), GST_RANK_PRIMARY + 1);
    }
    else
    {
        gst_plugin_feature_set_rank(GST_PLUGIN_FEATURE(factory), GST_RANK_NONE);
    }

    gst_registry_add_feature(registry, GST_PLUGIN_FEATURE(factory));
    return;
}
