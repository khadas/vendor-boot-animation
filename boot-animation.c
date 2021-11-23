#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

/* gstreamer */
#include <gst/gst.h>


typedef struct ST_BOOT_ANIMATION
{
    GstBus      *p_bus;
    GstMessage  *p_msg;
    GstStateChangeReturn ret;

    /* pipeline and elements */
    GstElement  *p_pipeline;
    GstElement  *p_source;
    GstElement  *p_demux;
    GstElement  *p_vqueue;
    GstElement  *p_vparse;
    GstElement  *p_vdec;
    GstElement  *p_vsink;
    GstElement  *p_aqueue;
    GstElement  *p_adec;
    GstElement  *p_aconv;
    GstElement  *p_aresample;
    GstElement  *p_asink;

    int         resv[9];
}BOOT_ANIMATION;


static volatile gboolean gb_run = FALSE;


static void print_usage(void) 
{
    g_print("Usage: boot-animation [OPTION]\n\n");
    g_print("-start\t\tstart boot animation\n");
    g_print("-stop\t\tstop boot animation\n");
    
    return;
}


static void signal_handler(int sig) 
{
    g_print("==> enter %s!", __FUNCTION__);

    if(TRUE == gb_run)
    {
        gb_run = FALSE;
    }

    return;
}


static void pad_added_handler(GstElement *src, GstPad *new_pad, void *user_data) 
{
	GstPadLinkReturn    ret = GST_PAD_LINK_OK;
	GstCaps             *new_pad_caps = NULL;
	GstStructure        *new_pad_struct = NULL;
	const gchar         *new_pad_type = NULL;
	guint               caps_size = 0;
    BOOT_ANIMATION      *p_data = (BOOT_ANIMATION*)user_data;
    GstPad              *tmp_sink_pad = NULL;
    
    if(NULL == user_data)
    {
        g_printerr("==> user_data is null!!!\n");
        gb_run = FALSE;
        goto exit;
    }

	new_pad_caps = gst_pad_query_caps(new_pad, NULL);
	new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
	caps_size = gst_caps_get_size(new_pad_caps);
	new_pad_type = gst_structure_get_name(new_pad_struct);
	if(g_str_has_prefix(new_pad_type, "video/x-h264"))
    {
        /* Get vqueue element sink pad. */
        tmp_sink_pad = gst_element_get_static_pad(p_data->p_vqueue, "sink");
	} 
    else if(g_str_has_prefix(new_pad_type, "audio/mpeg")) 
    {
        /* Get adec element sink pad. */
        tmp_sink_pad = gst_element_get_static_pad(p_data->p_adec, "sink");
    } 
    else
    {
		g_print("==> It has type '%s' which is not need. Ignoring.\n", new_pad_type);
        goto exit;
    }

    /* Check linked. */
    if(gst_pad_is_linked(tmp_sink_pad)) 
    {
        g_print("==> We are already linked. Ignoring.\n");
	    gst_object_unref(tmp_sink_pad);
        goto exit;
    }
    /* Link "demux" to "video queue". */
    ret = gst_pad_link(new_pad, tmp_sink_pad);
    if(GST_PAD_LINK_FAILED(ret)) 
    {
        g_printerr("==> Error: link pad '%s' to '%s' failed.\n", 
                   GST_PAD_NAME(new_pad), GST_PAD_NAME(tmp_sink_pad));
        gb_run = FALSE;
    }
    else 
    {
        g_print("==> Type: %s\n", new_pad_type);
        g_print("==> Size: %d\n", caps_size);
    }

	return ;

exit:
	if(new_pad_caps != NULL)
    {
		gst_caps_unref(new_pad_caps);
    }
    return;
}


int main(int argc, char *argv[]) 
{
    if(2 != argc) 
    {
        print_usage();
        exit(1);
    }

    if(0 == strcmp(argv[1], "-stop")) 
    {
        FILE *fp = popen("ps -e | grep \'boot-animation\' | awk \'{print $1}\'", "r");
        char buffer[8] = { 0 };
        int curr_pid = getpid();
        while(NULL != fgets(buffer, sizeof(buffer), fp))
        {
            int tmp_pid = atoi(buffer);
            if((0 < tmp_pid) && (curr_pid > tmp_pid))
            {
                g_print("==> send message to %s!\n", tmp_pid);
                kill(tmp_pid, SIGUSR1);
            }
        }
        pclose(fp);
    }
    else if(0 == strcmp(argv[1], "-start"))
    {
        int                     n_ret = 0;
        struct sigaction        st_act = { 0 };
        BOOT_ANIMATION          st_bootanimation = { 0 };
  
        st_act.sa_handler = signal_handler;
        st_act.sa_flags = 0;
        n_ret = sigaction(SIGUSR1, &st_act, NULL); 
        if(0 > n_ret)
        {
            g_print("==> login sigaction fsignal_handlerailed!\n");
            exit(1);
        }

        gb_run = TRUE;

        /* Initialize GStreamer */
        gst_init(&argc, &argv);

        /* Create the elements */
        st_bootanimation.p_source = gst_element_factory_make("filesrc", "source");
        st_bootanimation.p_demux = gst_element_factory_make("qtdemux", "demux");
        st_bootanimation.p_vqueue = gst_element_factory_make("queue", "vqueue");
        st_bootanimation.p_vparse = gst_element_factory_make("h264parse", "vparse");
        st_bootanimation.p_vdec = gst_element_factory_make("amlvdec", "vdec");
        st_bootanimation.p_vsink = gst_element_factory_make("amlvsink", "vsink");
        st_bootanimation.p_aqueue = gst_element_factory_make("queue", "aqueue");
        st_bootanimation.p_adec = gst_element_factory_make("avdec_aac", "adec");
        st_bootanimation.p_aconv = gst_element_factory_make("audioconvert", "aconv");
        st_bootanimation.p_aresample = gst_element_factory_make("audioresample", "aresample");
        st_bootanimation.p_asink = gst_element_factory_make("alsasink", "asink");

        /* Create the empty pipeline */
        st_bootanimation.p_pipeline = gst_pipeline_new("bootanimation-pipeline");

        if (!st_bootanimation.p_pipeline || !st_bootanimation.p_source || !st_bootanimation.p_demux 
            || !st_bootanimation.p_vqueue || !st_bootanimation.p_vparse || !st_bootanimation.p_vdec || !st_bootanimation.p_vsink 
            || !st_bootanimation.p_aqueue || !st_bootanimation.p_adec || !st_bootanimation.p_aconv || !st_bootanimation.p_aresample 
            || !st_bootanimation.p_asink) 
        {
            g_printerr("==> Not all elements could be created.\n");
            exit(1);
        }

        /* Build the pipeline */
        gst_bin_add_many(GST_BIN (st_bootanimation.p_pipeline), st_bootanimation.p_source, st_bootanimation.p_demux, 
                         st_bootanimation.p_vqueue, st_bootanimation.p_vparse, st_bootanimation.p_vdec, st_bootanimation.p_vsink, 
                         st_bootanimation.p_adec, st_bootanimation.p_aconv, st_bootanimation.p_aresample, st_bootanimation.p_asink, NULL);

        /* Link the element */
        if ((TRUE != gst_element_link(st_bootanimation.p_source, st_bootanimation.p_demux)) ||
            (TRUE != gst_element_link_many(st_bootanimation.p_vqueue, st_bootanimation.p_vparse, \
             st_bootanimation.p_vdec, st_bootanimation.p_vsink, NULL))||
            (TRUE != gst_element_link_many(st_bootanimation.p_adec, st_bootanimation.p_aconv, \
             st_bootanimation.p_aresample, st_bootanimation.p_asink, NULL))) 
        {
            g_printerr("==> Elements could not be linked.\n");
            gst_object_unref (st_bootanimation.p_pipeline);
            exit(1);
        }

        /* Modify the properties */
        g_object_set(st_bootanimation.p_source, "location", "/media/boot-animation.mp4", NULL);
        g_object_set(st_bootanimation.p_asink, "device", "hw:0,0", NULL);

        /* Handler event of "pad-added". */
        g_signal_connect(st_bootanimation.p_demux, "pad-added", G_CALLBACK(pad_added_handler), (void *)&st_bootanimation);

        /* Start playing */
        st_bootanimation.ret = gst_element_set_state(st_bootanimation.p_pipeline, GST_STATE_PLAYING);
        if (GST_STATE_CHANGE_FAILURE == st_bootanimation.ret) 
        {
            g_printerr("==> Unable to set the pipeline to the playing state.\n");
            gst_object_unref(st_bootanimation.p_pipeline);
            exit(1);
        }

        /* Wait until error or EOS */
        st_bootanimation.p_bus = gst_element_get_bus(st_bootanimation.p_pipeline);
        gst_bus_add_signal_watch(st_bootanimation.p_bus);

        /* Parse message */
        while(TRUE == gb_run)
        {
            st_bootanimation.p_msg = gst_bus_timed_pop_filtered(st_bootanimation.p_bus, 100*GST_MSECOND, GST_MESSAGE_ERROR | GST_MESSAGE_EOS); 
            if (NULL != st_bootanimation.p_msg) 
            {
                GError  *err = NULL;
                gchar   *debug_info = NULL;

                switch(GST_MESSAGE_TYPE (st_bootanimation.p_msg)) 
                {
                case GST_MESSAGE_ERROR:
                    gst_message_parse_error(st_bootanimation.p_msg, &err, &debug_info);
                    g_printerr("==> Error received from element %s: %s\n", GST_OBJECT_NAME (st_bootanimation.p_msg->src), err->message);
                    g_printerr("==> Debugging information: %s\n", debug_info ? debug_info : "none");
                    g_clear_error(&err);
                    g_free(debug_info);
                    gb_run = FALSE;
                    break;
                case GST_MESSAGE_EOS:
                    g_print("==> End-Of-Stream reached.\n");
                    gb_run = FALSE;
                    break;
                default:
                    /* We should not reach here because we only asked for ERRORs and EOS */
                    g_printerr("==> Unexpected message received.\n");
                    break;
                }
                gst_message_unref(st_bootanimation.p_msg);
            }
        }
        g_print("==> gb_run:%d!\n", gb_run);

        /* Free resources */
        gst_object_unref(st_bootanimation.p_bus);
        gst_element_set_state(st_bootanimation.p_pipeline, GST_STATE_NULL);
        gst_object_unref(st_bootanimation.p_pipeline); 
    
    }
    else
    {
        g_print("==> invalid argument!\n");
        print_usage();
    }

    g_print("==> exit main!\n");
    return 0;
}

