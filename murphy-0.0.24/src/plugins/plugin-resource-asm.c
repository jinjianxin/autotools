/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <signal.h>
#include <wait.h>
#include <fcntl.h>

#include <audio-session-manager.h>

#include <murphy/common.h>
#include <murphy/core.h>
#include <murphy/core/plugin.h>

#include <murphy/resource/client-api.h>

#include "resource-asm/asm-bridge.h"

#define DEFAULT_TRANSPORT  "unxs:/tmp/murphy/asm"
#define TYPE_MAP_SIZE      21
#define FILE_WRITING_HACK  1

/*
 * mapping entry of a single ASM event to a resource set
 *
 * Notes:
 *     Currently it is not possible to specify resource-specific
 *     mandatory or shared flags; they will be applied to all resources
 *     within the set. strict/relaxed policy attributes will be applied
 *     only to audio playback/recording resources.
 *
 *     Although for the sake of completeness currently it is possible to
 *     mark a resource in the mapping as optional, probably this never
 *     makes any sense, as there is no notion or mechanism in the ASM API
 *     to request or communicate any optionality to the client.
 */

typedef struct {
    char *asm_event;                           /* ASM event as a string */
    char *rset_class;                          /* mapped application class */
    unsigned rset_mask;                        /* involved resources (A/V,P/R)*/
    bool mandatory;                            /* requires resources ? */
    bool shared;                               /* agrees to share access */
    uint32_t priority;                         /* priority within class */
    bool strict;                               /* strict or relaxed policy ? */
} rset_class_data_t;

typedef struct {
    uint32_t pid;
    mrp_htbl_t *sets;
    uint32_t n_sets;
    uint32_t current_handle;

    bool monitor; /* if the client has set up a monitor resource */

    mrp_htbl_t *classes;
} client_t;


typedef enum {
    request_type_acquire,
    request_type_release,
    request_type_server_event
} request_type_t;


typedef struct {
    /* configuration */
    const char *address;
    const char *binary;
    const char *log;

    /* asm-bridge management */
    pid_t pid;
    mrp_transport_t *mt; /* listening transport */
    mrp_transport_t *t; /* connected transport */
    bool active;

    /* resource management */
    mrp_htbl_t *clients;
    mrp_resource_client_t *resource_client;
    char *zone;

    char *audio_playback;
    char *audio_recording;
    char *video_playback;
    char *video_recording;

    mrp_htbl_t *requests;
    uint32_t current_request;

    /* murphy integration */
    mrp_sighandler_t *sighandler;
    int error_count;
    bool shutting_down;

    mrp_context_t *ctx;
} asm_data_t;


typedef struct {
    const rset_class_data_t *rset_data;
    client_t *client; /* process */
    char *class_name;
    asm_data_t *ctx;

    /* mutable */
    request_type_t rtype;

    mrp_list_hook_t asm_clients; /* registered clients */
    mrp_resource_set_t *rset; /* murphy resource set that the clients share */
    ASM_sound_states_t granted_state;

    bool event_filtering;
} client_class_t;


typedef struct {
    /* immutable */
    uint32_t pid;
    uint32_t handle;
    uint32_t request_id;
    asm_data_t *ctx;
    client_class_t *client_class;

    request_type_t rtype;
    ASM_sound_states_t requested_state;
    ASM_sound_states_t granted_state;

    bool monitor;
    bool earjack;

    mrp_list_hook_t hook;
} resource_set_data_t;


enum {
    ARG_ASM_BRIDGE,
    ARG_ASM_BRIDGE_LOG,
    ARG_ASM_ZONE,
    ARG_ASM_TPORT_ADDRESS,
    ARG_ASM_AUDIO_PLAYBACK,
    ARG_ASM_AUDIO_RECORDING,
    ARG_ASM_VIDEO_PLAYBACK,
    ARG_ASM_VIDEO_RECORDING,
    ARG_ASM_EVENT_CONFIG,
};


#define EVENT_PREFIX "ASM_EVENT_"

static const char *asm_event_name[] = {
    "ASM_EVENT_SHARE_MMPLAYER",
    "ASM_EVENT_SHARE_MMCAMCORDER",
    "ASM_EVENT_SHARE_MMSOUND",
    "ASM_EVENT_SHARE_OPENAL",
    "ASM_EVENT_SHARE_AVSYSTEM",
    "ASM_EVENT_EXCLUSIVE_MMPLAYER",
    "ASM_EVENT_EXCLUSIVE_MMCAMCORDER",
    "ASM_EVENT_EXCLUSIVE_MMSOUND",
    "ASM_EVENT_EXCLUSIVE_OPENAL",
    "ASM_EVENT_EXCLUSIVE_AVSYSTEM",
    "ASM_EVENT_NOTIFY",
    "ASM_EVENT_CALL",
    "ASM_EVENT_SHARE_FMRADIO",
    "ASM_EVENT_EXCLUSIVE_FMRADIO",
    "ASM_EVENT_EARJACK_UNPLUG",
    "ASM_EVENT_ALARM",
    "ASM_EVENT_VIDEOCALL",
    "ASM_EVENT_MONITOR",
    "ASM_EVENT_RICH_CALL",
    "ASM_EVENT_EMERGENCY",
    "ASM_EVENT_EXCLUSIVE_RESOURCE",
    NULL
};


#define MAP_EVENT(_event, _class, _resmask, _shared, _strict)   \
    [ASM_EVENT_##_event] = {                                    \
    asm_event: #_event,                                         \
    rset_class: _class,                                         \
    rset_mask:  _resmask,                                       \
    mandatory:  TRUE,                                           \
    shared:     _shared,                                        \
    strict:     _strict,                                        \
    }

#define MANDATORY TRUE
#define SHARED    TRUE
#define STRICT    TRUE

#define NONE 0x0                               /* no resources */
#define AP   0x1                               /* audio playback */
#define AR   0x2                               /* audio recording */
#define VP   0x4                               /* video playback */
#define VR   0x8                               /* video recording */
#define APR  (AP|AR)                           /* audio playback/recording */
#define VPR  (VP|VR)                           /* video playback/recording */
#define AVP  (AP|VP)                           /* audio/video playback */
#define AVPR (AP|AR|VP|VR)                     /* audio/video playback/record */

static rset_class_data_t type_map[] = {
    MAP_EVENT(SHARE_MMPLAYER       , "player" , AVP ,  SHARED, !STRICT),
    MAP_EVENT(SHARE_MMCAMCORDER    , "camera" , AVPR,  SHARED, !STRICT),
    MAP_EVENT(SHARE_MMSOUND        , "event"  , AP  ,  SHARED, !STRICT),
    MAP_EVENT(SHARE_OPENAL         , "game"   , AVP ,  SHARED, !STRICT),
    MAP_EVENT(SHARE_AVSYSTEM       , "event"  , AP  ,  SHARED, !STRICT),
    MAP_EVENT(EXCLUSIVE_MMPLAYER   , "player" , AVP , !SHARED, !STRICT),
    MAP_EVENT(EXCLUSIVE_MMCAMCORDER, "camera" , AVPR, !SHARED, !STRICT),
    MAP_EVENT(EXCLUSIVE_MMSOUND    , "event"  , AP  , !SHARED, !STRICT),
    MAP_EVENT(EXCLUSIVE_OPENAL     , "game"   , AVP , !SHARED, !STRICT),
    MAP_EVENT(EXCLUSIVE_AVSYSTEM   , "event"  , AP  , !SHARED, !STRICT),
    MAP_EVENT(NOTIFY               , "event"  , AVP , !SHARED, !STRICT),
    MAP_EVENT(CALL                 , "phone"  , APR , !SHARED, !STRICT),
    MAP_EVENT(SHARE_FMRADIO        , "radio"  , AP  ,  SHARED, !STRICT),
    MAP_EVENT(EXCLUSIVE_FMRADIO    , "radio"  , AP  , !SHARED, !STRICT),
    MAP_EVENT(EARJACK_UNPLUG       , "earjack", NONE,  SHARED, !STRICT),
    MAP_EVENT(ALARM                , "alert"  , AP  , !SHARED, !STRICT),
    MAP_EVENT(VIDEOCALL            , "phone"  , AVPR, !SHARED, !STRICT),
    MAP_EVENT(MONITOR              , "monitor", NONE,  SHARED, !STRICT),
    MAP_EVENT(RICH_CALL            , "phone"  , AVPR, !SHARED, !STRICT),
    MAP_EVENT(EMERGENCY            , "phone"  , AVPR, !SHARED, !STRICT),
    MAP_EVENT(EXCLUSIVE_RESOURCE   , "player" , APR,  !SHARED, !STRICT),

    { NULL, NULL, NONE, FALSE, FALSE, FALSE, 0 }
};


static int max_event_name;


static int close_fds()
{
    int maxfd;
    int i;
    int newin, newout, newerr;
    int ret = -1;

    /* Closing all file descriptors in a protable way is tricky, so improve
       this function as we go. */

    maxfd = sysconf(_SC_OPEN_MAX);

    for (i = 0; i < maxfd; i++) {
        if (i != fileno(stdin) && i != fileno(stdout) && i != fileno(stderr))
            close(i);
    }

    /* redirect the streams to /dev/null */

    newin = open("/dev/null", O_RDONLY);
    newout = open("/dev/null", O_WRONLY);
    newerr = open("/dev/null", O_WRONLY);

    if (newin < 0 || newout < 0 || newerr < 0)
        goto end;

    if (dup2(newin, fileno(stdin)) < 0 ||
        dup2(newout, fileno(stdout)) < 0 ||
        dup2(newerr, fileno(stderr)) < 0)
        goto end;

    ret = 0;

end:
    close(newin);
    close(newout);
    close(newerr);

    return ret;
}


static bool launch_asm_bridge(asm_data_t *ctx)
{
    pid_t pid;

    mrp_log_info("going to fork!");

    pid = fork();

    if (pid < 0) {
        mrp_log_error("error launching asm-bridge");
        return FALSE;
    }
    else if (pid == 0) {
        /* child */
        if (close_fds() < 0) {
            mrp_log_error("close_fds() failed");
            exit(1);
        }
        if (ctx->log != NULL)
            setenv(ASM_BRIDGE_LOG_ENVVAR, ctx->log, 1);
        execl(ctx->binary, ctx->binary, ctx->address, NULL);
        exit(1);
    }
    else {
        /* parent */
        ctx->pid = pid;
        mrp_log_info("child pid is %d", pid);
    }

    return TRUE;
}


static int asm_event(const char *name, int len)
{
    const char *event;
    int i;

    if (!len)
        len = strlen(name);

    for (i = 0; asm_event_name[i] != NULL; i++) {
        event = asm_event_name[i];

        if (!strncasecmp(name, event, len) && event[len] == '\0')
            return i;

        event = asm_event_name[i] + sizeof(EVENT_PREFIX) - 1;

        if (!strncasecmp(name, event, len) && event[len] == '\0')
            return i;
    }

    return -1;
}


static int init_event_config(void)
{
    rset_class_data_t *data;
    int i;

    for (i = 0, data = type_map; data->asm_event != NULL; i++, data++) {
        data->asm_event  = mrp_strdup(data->asm_event);
        data->rset_class = mrp_strdup(data->rset_class);

        if (data->asm_event == NULL || data->rset_class == NULL)
            return FALSE;
    }

    return TRUE;
}


static int parse_config(rset_class_data_t *data, char *config)
{
    char *s, *colon, *f, *comma, *end;
    int len;
    unsigned int mask;

    /*
     * Parse a configuration entry of the form: 'class:flag1,...,flagn, where
     * the possible flags are:
     *
     *     mandatory/optional: whether resources are mandatory
     *     shared/exclusive: whether need exclusive resource ownership
     *     strict/reaxed: apply strict or reaxed policies
     *     priority: an integer priority between 0 and 255
     *
     * Eg. player:mandatory,exclusive,5,relaxed configures the ASM event to
     *     use the player application class, take mandatory (audio) resources
     *     in exclusive mode with priority 5, and ask relaxed policy.
     */

    s = config;

    mrp_debug("parsing config entry %s = '%s'", data->asm_event, config);

    colon = strchr(s, ':');

    if (colon == NULL) {
        mrp_log_error("Missing app. class name for ASM event '%s'.",
                      data->asm_event);
        return FALSE;
    }

    len = colon - (config) + 1;
    mrp_free(data->rset_class);
    data->rset_class = mrp_datadup(config, len);
    data->rset_class[len - 1] = '\0';

    mrp_debug("class name: '%s'", data->rset_class);

    f = colon + 1;
    comma = strchr(f, ',');

    mask = 0x80000000;
    while (f != NULL) {
#       define MATCHES(_f, _n, _l) ({                                   \
                mrp_debug("comparing flag '%*.*s' to '%s'...",          \
                          _l, _l, _f, _n);                              \
                (!strncmp(_f, _n, _l) &&                                \
                 (_f[_l] == '\0' || _f[_l] == ','));})

        len = (comma ? comma - f : (int)strlen(f));

        if      (MATCHES(f, "mandatory", len)) data->mandatory = TRUE;
        else if (MATCHES(f, "optional" , len)) data->mandatory = FALSE;
        else if (MATCHES(f, "shared"   , len)) data->shared    = TRUE;
        else if (MATCHES(f, "exclusive", len)) data->shared    = FALSE;
        else if (MATCHES(f, "strict",    len)) data->strict    = TRUE;
        else if (MATCHES(f, "relaxed",   len)) data->strict    = FALSE;
        else if (MATCHES(f, "none"   ,   len)) mask            = NONE;
        else if (MATCHES(f, "AP"     ,   len)) mask           |= AP;
        else if (MATCHES(f, "AR"     ,   len)) mask           |= AR;
        else if (MATCHES(f, "VP"     ,   len)) mask           |= VP;
        else if (MATCHES(f, "VR"     ,   len)) mask           |= VR;
        else if (MATCHES(f, "APR"    ,   len)) mask           |= APR;
        else if (MATCHES(f, "VPR"    ,   len)) mask           |= VPR;
        else if (MATCHES(f, "AVP"    ,   len)) mask           |= AVP;
        else if (MATCHES(f, "AVPR"   ,   len)) mask           |= AVPR;
        else if (MATCHES(f, "audio_playback" , len)) mask     |= AP;
        else if (MATCHES(f, "audio_recording", len)) mask     |= AR;
        else if (MATCHES(f, "video_playback" , len)) mask     |= VP;
        else if (MATCHES(f, "video_recording", len)) mask     |= VR;
        else {
            data->priority = strtoul(f, &end, 10);
            if (end && *end && *end != ',' && *end != ';') {
                mrp_log_error("Invalid flag or priority (%s) for event %s.",
                              f, data->asm_event);
                return FALSE;
            }
            if (data->priority > 256) {
                mrp_log_error("Out of range priority (%d) for event %s.",
                              data->priority, data->asm_event);
            }
        }

        /* if at the end stop, otherwise go to the next flag */
        f = (comma ? comma + 1 : NULL);
        comma = (f ? strchr(f, ',') : NULL);

#       undef IS_FLAG
    }

    if (mask != 0x80000000)
        data->rset_mask = (mask & 0x0fffffff);

    return TRUE;
}


static int parse_event_config(mrp_plugin_arg_t *events)
{
    rset_class_data_t *data;
    mrp_plugin_arg_t  *cfg;
    int                evt;

    if (events == NULL || events->rest.args == NULL)
        return TRUE;

    mrp_plugin_foreach_undecl_arg(events, cfg) {
        if (cfg->type != MRP_PLUGIN_ARG_TYPE_STRING) {
            mrp_log_warning("Ignoring non-string configuration for '%s'.",
                            cfg->key);
            continue;
        }

        evt = asm_event(cfg->key, 0);

        if (evt < 0) {
            mrp_log_error("Ignoring configuration for unknown ASM event '%s'.",
                          cfg->key);
            continue;
        }

        data = type_map + evt;

        if (!parse_config(data, cfg->str)) {
            mrp_log_error("Failed to parse configuration '%s' for ASM event "
                          "'%s'.", cfg->str, cfg->key);
            return FALSE;
        }
    }

    return TRUE;
}


static void dump_event_config(void)
{
    rset_class_data_t *data;
    int i, l;
    char resmask[16], *rmp;

    if (max_event_name <= 0) {
        for (i = 0, data = type_map; i < ASM_EVENT_MAX; i++, data++)
            if (data->asm_event != NULL &&
                (l = strlen(data->asm_event)) > max_event_name)
                max_event_name = l;
    }

    mrp_debug("event mapping:");
    for (i = 0, data = type_map; i < ASM_EVENT_MAX; i++, data++) {
        rmp = resmask;
        *rmp++ = 'A';
        if (!(data->rset_mask & APR)) *rmp++ = '-';
        else {
            if (data->rset_mask & AP) *rmp++ = 'P';
            if (data->rset_mask & AR) *rmp++ = 'R';
        }
        *rmp++ = '/';
        *rmp++ = 'V';
        if (!(data->rset_mask & VPR)) *rmp++ = '-';
        else {
            if (data->rset_mask & VP) *rmp++ = 'P';
            if (data->rset_mask & VR) *rmp++ = 'R';
        }
        *rmp = '\0';
        mrp_debug("%*.*s: %s (%s, prio %d, %s/%s, %s)",
                  max_event_name, max_event_name, data->asm_event,
                  data->rset_class, resmask, data->priority,
                  data->mandatory ? "m" : "o", data->shared ? "s" : "e",
                  data->strict ? "strict" : "relaxed");
    }
}

static int update_buffer(void *key, void *object, void *user_data)
{
    uint32_t *buffer = (uint32_t *) user_data;
    resource_set_data_t *asm_client = (resource_set_data_t *) object;
    client_class_t *client_class = asm_client->client_class;

    MRP_UNUSED(key);

    if (!client_class->rset)
        return MRP_HTBL_ITER_MORE;

    if (asm_client->handle >= 256)
        return MRP_HTBL_ITER_MORE;

    buffer[asm_client->handle] = mrp_get_resource_set_id(client_class->rset);

    return MRP_HTBL_ITER_MORE;
}

#ifdef FILE_WRITING_HACK
/* temporary hack */

static char* get_filename(client_t *client)
{
    static char fname[256];
    snprintf(fname, 256, "/tmp/murphy/mapping-file-%d", client->pid);

    return fname;
}


static void update_client_mapping_file(client_t *client)
{
    uint32_t buf[256];
    int size = 256*sizeof(uint32_t);
    FILE *f;

    memset(buf, 0, size);

    if (!client->sets)
        return;

    /* go through every asm handle and fill the file with the murphy rset ids */
    mrp_htbl_foreach(client->sets, update_buffer, buf);

    /* create a tmp file */
    f = fopen("/tmp/murphy/tmp-mapping-file", "w");

    if (!f)
        return;

    /* write the buffer to the tmp file */
    fwrite(buf, 256, sizeof(uint32_t), f);
    fclose(f);

    /* rename the tmp file to the official file */
    rename("/tmp/murphy/tmp-mapping-file", get_filename(client));
}
#endif


static void *u_to_p(uint32_t u)
{
#ifdef __SIZEOF_POINTER__
#if __SIZEOF_POINTER__ == 8
    uint64_t o = u;
#else
    uint32_t o = u;
#endif
#else
    uint32_t o = o;
#endif
    return (void *) o;
}


static uint32_t p_to_u(const void *p)
{
#ifdef __SIZEOF_POINTER__
#if __SIZEOF_POINTER__ == 8
    uint32_t o = 0;
    uint64_t big = (uint64_t) p;
    o = big & 0xffffffff;
#else
    uint32_t o = (uint32_t) p;
#endif
#else
    uint32_t o = p;
#endif
    return o;
}


static int int_comp(const void *key1, const void *key2)
{
    return key1 != key2;
}


static uint32_t int_hash(const void *key)
{
    return p_to_u(key);
}


static const rset_class_data_t *map_slp_media_type_to_murphy(
        ASM_sound_events_t media_type)
{
    /* check if the event is within the bounds */
    if (media_type <= ASM_EVENT_NONE || media_type >= ASM_EVENT_MAX)
        return NULL;

    /* check that we don't overflow */
    if (media_type > TYPE_MAP_SIZE)
        return NULL;

    return &type_map[media_type];
}


static void dump_incoming_msg(lib_to_asm_t *msg, asm_data_t *ctx)
{
    MRP_UNUSED(ctx);

    mrp_log_info("   --> client id:       %u", msg->instance_id);
    mrp_log_info("   --> data handle:     %d", msg->handle);
    mrp_log_info("   --> request id:      0x%04x", msg->request_id);
    mrp_log_info("   --> sound event:     0x%04x", msg->sound_event);
    mrp_log_info("   --> system resource: 0x%04x", msg->system_resource);
    mrp_log_info("   --> state:           0x%04x", msg->sound_state);
#ifdef USE_SECURITY
    {
        int n_cookie = msg->n_cookie_bytes;
        int i;

        mrp_log_info(" --> cookie: ")
        for (i = 0; i < n_cookie, i++) {
            mrp_log_info("0x%02x ", msg->cookie[i]);
        }
        mrp_log_info("\n");
    }
#endif
}



static void dump_outgoing_msg(asm_to_lib_t *msg, asm_data_t *ctx)
{
    MRP_UNUSED(ctx);

    mrp_log_info(" <--   client id:       %u", msg->instance_id);
    mrp_log_info(" <--   alloc handle:    %d", msg->alloc_handle);
    mrp_log_info(" <--   command handle:  %d", msg->cmd_handle);
    mrp_log_info(" <--   sound command:   0x%04x", msg->result_sound_command);
    mrp_log_info(" <--   state:           0x%04x", msg->result_sound_state);
    mrp_log_info(" <--   former event:    %d", msg->former_sound_event);
    mrp_log_info(" <--   check privilege: %s",
            msg->check_privilege ? "TRUE" : "FALSE");
}


static void dump_outgoing_cb_msg(asm_to_lib_cb_t *msg, asm_data_t *ctx)
{
    MRP_UNUSED(ctx);

    mrp_log_info(" <--   client id:       %u", msg->instance_id);
    mrp_log_info(" <--   handle:          %d", msg->handle);
    mrp_log_info(" <--   expect callback: %d", msg->callback_expected);
    mrp_log_info(" <--   sound command:   0x%04x", msg->sound_command);
    mrp_log_info(" <--   event source:    0x%04x", msg->event_source);
}


#if 0
static uint32_t encode_pid_handle(uint32_t pid, uint32_t handle) {

    uint32_t max_pid = 4194304; /* 2^22 */
    uint32_t max_handle = 1024; /* 2^10 */

    if (pid > max_pid || handle > max_handle) {
        return 0;
    }

    return pid | (handle << 22);
}

static uint32_t get_pid(uint32_t data) {
    uint32_t pid_mask = 0xffffffff >> 10;

    return data & pid_mask;
}


static uint32_t get_handle(uint32_t data) {

    return data >> 22;
}
#endif


static uint32_t get_next_handle(client_t *client)
{
    uint32_t retval = client->current_handle;

    client->current_handle++;

    if (client->current_handle == ASM_SERVER_HANDLE_MAX)
        client->current_handle = 1;

    return retval;
}


static void htbl_free_client_class(void *key, void *object)
{
    client_class_t *client_class = (client_class_t *) object;

    MRP_UNUSED(key);

    if (client_class->rset)
        mrp_resource_set_destroy(client_class->rset);

    mrp_free(client_class);
}


static void htbl_free_set(void *key, void *object)
{
    resource_set_data_t *d = (resource_set_data_t *) object;

    MRP_UNUSED(key);

    mrp_free(d);
}


static client_t *create_client(uint32_t pid) {
    client_t *client = (client_t *) mrp_allocz(sizeof(client_t));
    mrp_htbl_config_t set_conf;

    if (!client)
        return NULL;

    set_conf.comp = int_comp;
    set_conf.hash = int_hash;
    set_conf.free = htbl_free_set;
    set_conf.nbucket = 0;
    set_conf.nentry = 10;

    client->sets = mrp_htbl_create(&set_conf);

    if (!client->sets)
        goto error;

    set_conf.comp = mrp_string_comp;
    set_conf.hash = mrp_string_hash;
    set_conf.free = htbl_free_client_class;

    client->classes = mrp_htbl_create(&set_conf);

    if (!client->classes)
        goto error;

    client->pid = pid;
    client->current_handle = 1;
    client->n_sets = 0;

    return client;

error:
    if (client && client->sets) {
        mrp_htbl_destroy(client->sets, false);
    }
    mrp_free(client);
    return NULL;
}


static void event_cb(uint32_t request_id, mrp_resource_set_t *set, void *data)
{
    client_class_t *client_class = data;
    asm_data_t *ctx = client_class->ctx;

    mrp_log_info("Event CB: id %u, set %p", request_id, set);
    mrp_log_info("Resource class: %s", client_class->class_name);
    mrp_log_info("Advice 0x%08x, Grant 0x%08x",
            mrp_get_resource_set_advice(client_class->rset),
            mrp_get_resource_set_grant(client_class->rset));

    switch(client_class->rtype) {
        case request_type_acquire:
        {
            mrp_list_hook_t *p, *n;

            mrp_log_info("callback for acquire request %u", request_id);

            /* send the replies to everyone who might have requested this
             * between the first request and this event */

            mrp_list_foreach(&client_class->asm_clients, p, n) {

                asm_to_lib_t reply;
                resource_set_data_t *asm_client = mrp_list_entry(p,
                        typeof(*asm_client), hook);

                if (asm_client->rtype != request_type_acquire) {
                    /* didn't request this */
                    continue;
                }

                reply.instance_id = asm_client->pid;
                reply.check_privilege = TRUE;
                reply.alloc_handle = asm_client->handle;
                reply.cmd_handle = asm_client->handle;

                reply.result_sound_state = ASM_STATE_IGNORE;
                reply.former_sound_event = ASM_EVENT_NONE;

                /* ASM doesn't support optional resources. Thus, if we are
                 * granted anything, we are granted the whole set we requested.
                 */
                if (mrp_get_resource_set_grant(client_class->rset)) {
                    reply.result_sound_command = ASM_COMMAND_PLAY;
                    asm_client->granted_state = asm_client->requested_state;
                }
                else {
                    reply.result_sound_command = ASM_COMMAND_STOP;
                }

                /* only send reply when "PLAYING" state was requested ->
                 * this happens when acquire request is done */
                dump_outgoing_msg(&reply, ctx);
                mrp_transport_senddata(ctx->t, &reply, TAG_ASM_TO_LIB);

                asm_client->rtype = request_type_server_event;
            }

            /* expecting next server events */
            client_class->rtype = request_type_server_event;
            client_class->event_filtering = FALSE;

            break;
        }
        case request_type_release:
        {
            mrp_list_hook_t *p, *n;

            mrp_log_info("callback for release request %u", request_id);

            /* TODO: send the replies to everyone who might have requested this
             * between the first request and this event */

            mrp_list_foreach(&client_class->asm_clients, p, n) {
                resource_set_data_t *asm_client = mrp_list_entry(p,
                        typeof(*asm_client), hook);

                asm_client->rtype = request_type_server_event;
            }

            /* expecting next server events */
            client_class->rtype = request_type_server_event;
            client_class->event_filtering = FALSE;

            break;
        }
        case request_type_server_event:
        {
            mrp_log_info("callback for no request %u", request_id);
            uint32_t grant = mrp_get_resource_set_grant(client_class->rset);
            mrp_list_hook_t *p, *n;

            if (client_class->event_filtering) {
                /* We either haven't requested any resources or have
                 * given up the resources. Filter out events. */
                break;
            }

            mrp_list_foreach(&client_class->asm_clients, p, n) {

                asm_to_lib_cb_t reply;
                resource_set_data_t *asm_client = mrp_list_entry(p,
                        typeof(*asm_client), hook);

                /* send the replies to everyone who might have requested this
                 * between the first request and this event */

                reply.instance_id = asm_client->pid;
                reply.handle = asm_client->handle;

                if (grant) {
                    reply.sound_command = ASM_COMMAND_RESUME;
                    /* ASM doesn't send callback to RESUME commands */
                    reply.callback_expected = FALSE;
                }
                else {
                    reply.sound_command = ASM_COMMAND_PAUSE;
                    reply.callback_expected = TRUE;
                }

                /* FIXME: the player-player case needs to be solved here? */
                reply.event_source = ASM_EVENT_SOURCE_OTHER_PLAYER_APP;

                dump_outgoing_cb_msg(&reply, ctx);
                mrp_transport_senddata(ctx->t, &reply, TAG_ASM_TO_LIB_CB);
            }

            /* TODO: get the client and see if there is the monitor
             * resource present. If yes, tell the availability state changes
             * through it. */

#if 0
            /* check if the d->rset state has actually changed -> only
             * process server side notifications in that case */
            if ((grant && client_class->granted_state == ASM_STATE_PLAYING) ||
                 (!grant && client_class->granted_state != ASM_STATE_PLAYING)) {
                mrp_log_info("state didn't change -> ignoring");
                break;
            }
#endif
            break;
        }
    }
}


static void remove_asm_client_from_class(client_class_t *client_class,
        resource_set_data_t *asm_client)
{
    client_t *client = client_class->client;

    mrp_list_delete(&asm_client->hook);

    if (mrp_list_empty(&client_class->asm_clients)) {
        mrp_htbl_remove(client->classes, client_class->class_name, TRUE);
    }

    /* do not free the asm_client -- it's freed when it's removed from the
     * sets map */
}


static void update_asm_client_states(client_class_t *client_class,
        resource_set_data_t *d)
{
    if (!client_class || !d)
        return;

    /* asm_client just made a request -- see if we can fulfill it without
     * querying resource library */

    switch(d->requested_state) {
        case ASM_STATE_PLAYING:
        {
            /* requests done for "PLAYING" state need a reply, others don't */

            d->rtype = request_type_acquire;

            if (client_class->rtype == request_type_acquire) {
                /* acquire already in process */
                return;
            }

            if (client_class->granted_state == ASM_STATE_PLAYING) {
                /* have to reply */
                asm_to_lib_t reply;

                reply.instance_id = d->pid;
                reply.check_privilege = TRUE;
                reply.alloc_handle = d->handle;
                reply.cmd_handle = d->handle;

                reply.result_sound_state = ASM_STATE_IGNORE;
                reply.former_sound_event = ASM_EVENT_NONE;
                reply.result_sound_command = ASM_COMMAND_PLAY;

                dump_outgoing_msg(&reply, d->ctx);
                mrp_transport_senddata(d->ctx->t, &reply, TAG_ASM_TO_LIB);
            }
            else {
                client_class->rtype = request_type_acquire;

                mrp_log_info("requesting acquisition of playback rights"
                    " for set '%u.%u' (id: %u)", d->pid, d->handle,
                    d->request_id);

                mrp_resource_set_acquire(client_class->rset, d->request_id);
            }

            break;
        }
        case ASM_STATE_STOP:
        case ASM_STATE_PAUSE:
        {
            if (client_class->granted_state == ASM_STATE_STOP ||
                client_class->granted_state == ASM_STATE_PAUSE) {
                /* nothing needs to be done */
            }
            else {
                mrp_list_hook_t *p, *n;
                resource_set_data_t *asm_client;
                bool found = FALSE;

                /* Go through all asm_clients belonging to this class. If there
                 * is even one that wants to play, play. Otherwise release
                 * resources. */

                mrp_list_foreach(&client_class->asm_clients, p, n) {
                    asm_client = mrp_list_entry(p, typeof(*asm_client), hook);

                    if (asm_client->requested_state == ASM_STATE_PLAYING) {
                        found = TRUE;
                        break;
                    }
                }

                if (!found) {
                    d->rtype = request_type_release;

                    if (client_class->rtype == request_type_release) {
                        /* release already in process */
                        return;
                    }

                    client_class->rtype = request_type_release;

                    mrp_log_info("requesting release of playback rights for"
                            " set '%u.%u' (id: %u)", d->pid, d->handle,
                            d->request_id);

                    mrp_resource_set_release(client_class->rset, d->request_id);
                }
            }
            break;
        }
        default:
        {
            mrp_log_error("Unknown state: %d", d->requested_state);
        }
    }
    return;
}


static asm_to_lib_t *process_msg(lib_to_asm_t *msg, asm_data_t *ctx)
{
    pid_t pid = msg->instance_id;
    mrp_attr_t attrs[3];
    char pidbuf[32];

    asm_to_lib_t *reply;

    reply = (asm_to_lib_t *) mrp_allocz(sizeof(asm_to_lib_t));

    if (!reply)
        return NULL;

    reply->instance_id = pid;
    reply->check_privilege = TRUE;

    reply->alloc_handle = msg->handle;
    reply->cmd_handle = msg->handle;

    reply->result_sound_command = ASM_COMMAND_NONE;
    reply->result_sound_state = ASM_STATE_IGNORE;
    reply->former_sound_event = ASM_EVENT_NONE;

    switch(msg->request_id) {
        case ASM_REQUEST_REGISTER:
        {
            uint32_t handle;
            resource_set_data_t *d;
            client_t *client;
            const rset_class_data_t *rset_data;
            client_class_t *client_class;
            bool merged;

            mrp_log_info("REQUEST: REGISTER");

            /* see if the process already has a client object */
            client = (client_t *) mrp_htbl_lookup(ctx->clients, u_to_p(pid));

            if (!client) {
                client = create_client(pid);
                if (!client)
                    goto error;

                mrp_htbl_insert(ctx->clients, u_to_p(pid), client);
            }

            rset_data = map_slp_media_type_to_murphy(
                    (ASM_sound_events_t) msg->sound_event);

            if (!rset_data) {
                mrp_log_error("unknown resource type: %d", msg->sound_event);
                goto error;
            }

            handle = get_next_handle(client);
            d = (resource_set_data_t *) mrp_allocz(sizeof(resource_set_data_t));

            if (!d)
                goto error;

            mrp_list_init(&d->hook);

            d->handle = handle;
            d->ctx = ctx;
            d->pid = pid;
            d->request_id = 0;
            d->requested_state = ASM_STATE_WAITING;
            d->granted_state = ASM_STATE_WAITING;
            d->rtype = request_type_server_event;
            d->earjack = FALSE;
            d->monitor = FALSE;

            /* ok, see if we already have a client_class struct for the type */

            client_class = mrp_htbl_lookup(client->classes,
                    rset_data->rset_class);

            if (!client_class) {
                client_class = mrp_allocz(sizeof(client_class_t));
                if (!client_class)
                    goto error;

                mrp_list_init(&client_class->asm_clients);
                client_class->ctx = ctx;
                client_class->class_name = rset_data->rset_class;
                client_class->client = client;
                client_class->rset_data = rset_data;
                client_class->rset = NULL;
                client_class->event_filtering = TRUE;

                client_class->rtype = request_type_server_event;

                mrp_list_append(&client_class->asm_clients, &d->hook);

                d->client_class = client_class;

                mrp_htbl_insert(client->classes, rset_data->rset_class,
                        client_class);

                merged = FALSE;
            }
            else {
                /* do the merging */
                d->client_class = client_class;
                mrp_list_append(&client_class->asm_clients, &d->hook);
                merged = TRUE;
            }

            if (strcmp(rset_data->rset_class, "earjack") == 0) {
                mrp_log_info("earjack status request was received");
                d->earjack = TRUE;
            }
            else if (strcmp(rset_data->rset_class, "monitor") == 0) {
                mrp_log_info("monitor resource was received");
                /* TODO: tell the available state changes to this pid
                 * via the monitor resource? */
                client->monitor = TRUE;
                d->monitor = TRUE;
            }
            else if (!merged) {
                /* a normal resource request */

                client_class->rset = mrp_resource_set_create(
                        ctx->resource_client, 0, rset_data->priority, event_cb,
                        client_class);

                if (!client_class->rset) {
                    mrp_log_error("Failed to create resource set!");
                    remove_asm_client_from_class(client_class, d);
                    goto error;
                }

                snprintf(pidbuf, sizeof(pidbuf), "%u", d->pid);
                attrs[0].type = mqi_string;
                attrs[0].name = "pid";
                attrs[0].value.string = pidbuf;
                attrs[1].type = mqi_string;
                attrs[1].name = "policy";
                attrs[1].value.string = rset_data->strict ? "strict":"relaxed";
                attrs[2].name = NULL;

                if (rset_data->rset_mask & AP) {
                    if (mrp_resource_set_add_resource(client_class->rset,
                                ctx->audio_playback,
                                rset_data->shared,
                                &attrs[0],
                                rset_data->mandatory) < 0) {
                        mrp_log_error("Failed to add audio playback resource!");
                        remove_asm_client_from_class(client_class, d);
                        goto error;
                    }
                }

                if (rset_data->rset_mask & AR) {
                    if (mrp_resource_set_add_resource(client_class->rset,
                                ctx->audio_recording, rset_data->shared,
                                &attrs[0],
                                rset_data->mandatory) < 0) {
                        mrp_log_error("Failed to add audio record resource!");
                        remove_asm_client_from_class(client_class, d);
                        goto error;
                    }
                }

                if (rset_data->rset_mask & VP) {
                    if (mrp_resource_set_add_resource(client_class->rset,
                                ctx->video_playback,
                                rset_data->shared,
                                &attrs[0],
                                rset_data->mandatory) < 0) {
                        mrp_log_error("Failed to add video playback resource!");
                        remove_asm_client_from_class(client_class, d);
                        goto error;
                    }
                }

                if (rset_data->rset_mask & VR) {
                    if (mrp_resource_set_add_resource(client_class->rset,
                                ctx->video_recording, rset_data->shared,
                                &attrs[0],
                                rset_data->mandatory) < 0) {
                        mrp_log_error("Failed to add video record resource!");
                        remove_asm_client_from_class(client_class, d);
                        goto error;
                    }
                }

                if (mrp_application_class_add_resource_set(
                            rset_data->rset_class, ctx->zone,
                            client_class->rset, 0) < 0) {
                    mrp_log_error("Failed to put the rset in a class!");
                    remove_asm_client_from_class(client_class, d);
                    goto error;
                }
            }
            /* else a merged request, no need to create resource set */

            mrp_htbl_insert(client->sets, u_to_p(handle), d);
            client->n_sets++;

#ifdef FILE_WRITING_HACK
            /* temporary hack */
            update_client_mapping_file(client);
#endif

            reply->alloc_handle = handle;
            reply->cmd_handle = reply->alloc_handle;

            reply->result_sound_state = ASM_STATE_WAITING;
            reply->former_sound_event = ASM_EVENT_NONE;
            break;
        }
        case ASM_REQUEST_UNREGISTER:
            {
                client_t *client = (client_t *)
                        mrp_htbl_lookup(ctx->clients, u_to_p(pid));

                mrp_log_info("REQUEST: UNREGISTER");

                if (client) {
                    resource_set_data_t *d;

                    d = (resource_set_data_t *)
                            mrp_htbl_lookup(client->sets, u_to_p(msg->handle));
                    if (!d) {
                        mrp_log_error("set '%u.%u' not found", pid,
                                msg->handle);
                        goto noreply;
                    }

                    if (strcmp(d->client_class->class_name, "earjack") == 0) {
                        mrp_debug("unregistering earjack resource");
                    }
                    else if (strcmp(d->client_class->class_name, "monitor")
                            == 0) {
                        mrp_debug("unregistering monitor resource");
                        client->monitor = FALSE;
                    }

                    remove_asm_client_from_class(d->client_class, d);

                    /* the resource set id destroyed when it's removed from the
                     * table */
                    mrp_htbl_remove(client->sets, u_to_p(msg->handle), TRUE);
                    client->n_sets--;

#ifdef FILE_WRITING_HACK
                    /* temporary hack */
                    update_client_mapping_file(client);
#endif
                    if (client->n_sets <= 0) {
                        mrp_htbl_remove(ctx->clients, u_to_p(pid), TRUE);
                        client = NULL;
                    }
                }

                /* no reply needed! */
                goto noreply;

                break;
            }
        case ASM_REQUEST_SETSTATE:
            {
                client_t *client =
                        (client_t *) mrp_htbl_lookup(ctx->clients, u_to_p(pid));

                resource_set_data_t *d;

                mrp_log_info("REQUEST: SET STATE");

                if (!client) {
                    mrp_log_error("client '%u' not found", pid);
                    goto error;
                }

                d = (resource_set_data_t *)
                        mrp_htbl_lookup(client->sets, u_to_p(msg->handle));
                if (!d) {
                    mrp_log_error("set '%u.%u' not found", pid, msg->handle);
                    goto error;
                }

                d->request_id = ++ctx->current_request;
                d->requested_state = (ASM_sound_states_t) msg->sound_state;

                update_asm_client_states(d->client_class, d);

                goto noreply;
            }
        case ASM_REQUEST_GETSTATE:
            {
                const rset_class_data_t *rset_data;
                client_t *client = (client_t *)
                        mrp_htbl_lookup(ctx->clients, u_to_p(pid));
                client_class_t *client_class;

                rset_data = map_slp_media_type_to_murphy(
                        (ASM_sound_events_t) msg->sound_event);

                mrp_log_info("REQUEST: GET STATE for %s",
                        rset_data ? rset_data->rset_class : "NULL");

                if (!rset_data) {
                    mrp_log_error("no rset data");
                    goto error;
                }

               if (!client) {
                    mrp_log_error("client '%u' not found", pid);
                    goto error;
                }

                client_class = mrp_htbl_lookup(client->classes,
                        rset_data->rset_class);

                if (!client_class) {
                    /* we just don't know */
                    reply->result_sound_state = ASM_STATE_IGNORE;
                    reply->former_sound_event = ASM_EVENT_NONE;
                }
                else {
                    reply->result_sound_state = client_class->granted_state;
                    reply->former_sound_event = ASM_EVENT_NONE;
                }
                break;
            }
        case ASM_REQUEST_GETMYSTATE:
            {
                client_t *client = (client_t *)
                        mrp_htbl_lookup(ctx->clients, u_to_p(pid));
                resource_set_data_t *d;

                mrp_log_info("REQUEST: GET MY STATE");

                if (!client) {
                    mrp_log_error("client '%u' not found", pid);
                    goto error;
                }

                d = (resource_set_data_t *)
                        mrp_htbl_lookup(client->sets, u_to_p(msg->handle));
                if (!d) {
                    mrp_log_error("set '%u.%u' not found", pid, msg->handle);
                    goto error;
                }

                reply->result_sound_state = d->granted_state;
                reply->former_sound_event = ASM_EVENT_NONE;
                break;
            }
        case ASM_REQUEST_EMERGENT_EXIT:
            {
                client_t *client = (client_t *)
                        mrp_htbl_lookup(ctx->clients, u_to_p(pid));

                mrp_log_info("REQUEST: EMERGENCY EXIT");

                if (!client) {
                    mrp_log_error("client '%u' not found", pid);
                    goto noreply;
                }

                mrp_htbl_remove(ctx->clients, u_to_p(pid), TRUE);

                goto noreply;
            }
        case ASM_REQUEST_SET_SUBSESSION:
            mrp_log_info("REQUEST: SET SUBSESSION");
            break;
        case ASM_REQUEST_GET_SUBSESSION:
            mrp_log_info("REQUEST: GET SUBSESSION");
            break;
        default:
            mrp_log_info("REQUEST: UNKNOWN REQUEST");
            break;
    }

    return reply;

error:
    /* write some message back to avoid client locking */
    return reply;

noreply:

    mrp_free(reply);
    return NULL;
}


static void process_cb_msg(lib_to_asm_cb_t *msg, asm_data_t *ctx)
{
    const char *str;

    MRP_UNUSED(ctx);

    /* TODO: this function might tell something to the resource library */

    switch (msg->cb_result) {
        case ASM_CB_RES_IGNORE:
            str = "ignore";
            break;
        case ASM_CB_RES_NONE:
            str = "none";
            break;
        case ASM_CB_RES_PAUSE:
            str = "pause";
            break;
        case ASM_CB_RES_PLAYING:
            str = "playing";
            break;
        case ASM_CB_RES_STOP:
            str = "stop";
            break;
        default:
            mrp_log_error("unknown callback state %d", msg->cb_result);
            return;
    }

    mrp_log_info("client %d.%u ended in state '%s' after callback",
            msg->instance_id, msg->handle, str);
}


static void recvdatafrom_evt(mrp_transport_t *t, void *data, uint16_t tag,
                     mrp_sockaddr_t *addr, socklen_t addrlen, void *user_data)
{
    asm_data_t *ctx = (asm_data_t *) user_data;

    MRP_UNUSED(addr);
    MRP_UNUSED(addrlen);

    switch (tag) {
        case TAG_LIB_TO_ASM:
            {
                lib_to_asm_t *msg = (lib_to_asm_t *) data;
                asm_to_lib_t *reply;

                /* client requests something from us */

                dump_incoming_msg(msg, ctx);

                reply = process_msg(msg, ctx);
                if (reply) {
                    dump_outgoing_msg(reply, ctx);
                    mrp_transport_senddata(t, reply, TAG_ASM_TO_LIB);
                    mrp_free(reply);
                }
                mrp_log_info(" ");
                break;
            }
        case TAG_LIB_TO_ASM_CB:
            {
                lib_to_asm_cb_t *msg = (lib_to_asm_cb_t *) data;

                /* client tells us which state it entered after preemption */

                process_cb_msg(msg, ctx);
                mrp_log_info(" ");
                break;
            }

        default:
            mrp_log_error("Unknown message received!");
            break;
    }

    mrp_data_free(data, tag);
}


static void recvdata_evt(mrp_transport_t *t, void *data, uint16_t tag, void *user_data)
{
    recvdatafrom_evt(t, data, tag, NULL, 0, user_data);
}



static void closed_evt(mrp_transport_t *t, int error, void *user_data)
{
#if 1
    asm_data_t *ctx = (asm_data_t *) user_data;

    MRP_UNUSED(error);

    mrp_log_info("closed!");

    mrp_transport_disconnect(t);
    mrp_transport_destroy(t);

    ctx->t = NULL;

    /* TODO: start the listening socket again and relaunch the binary? */
#endif
}


static void connection_evt(mrp_transport_t *lt, void *user_data)
{
    asm_data_t *ctx = (asm_data_t *) user_data;

    mrp_log_info("connection!");

    if (ctx->t) {
        mrp_log_error("Already connected");
    }
    else {
        ctx->t = mrp_transport_accept(lt, ctx, 0);
    }

    /* close the listening socket, since we only have one client */

    mrp_transport_destroy(lt);
    ctx->mt = NULL;
}


static int tport_setup(const char *address, asm_data_t *ctx)
{
    const char *atype;
    ssize_t alen;
    mrp_sockaddr_t addr;

    struct stat statbuf;

    static mrp_transport_evt_t evt = {
        { .recvdata = recvdata_evt },
        { .recvdatafrom = recvdatafrom_evt },
        .closed = closed_evt,
        .connection = connection_evt
    };

    alen = mrp_transport_resolve(NULL, address, &addr, sizeof(addr), &atype);

    if (alen <= 0) {
        mrp_log_error("Error resolving transport address");
        goto error;
    }

    /* remove the old socket if present */

    if (strcmp(atype, "unxs") == 0) {
        char *path = addr.unx.sun_path;
        if (path[0] == '/') {
            /* if local socket and file exists, remove it */
            if (stat(path, &statbuf) == 0) {
                if (S_ISSOCK(statbuf.st_mode)) {
                    if (unlink(path) < 0) {
                        mrp_log_error("error removing the socket");
                        goto error;
                    }
                }
                else {
                    mrp_log_error("a file where the socket should be created");
                    goto error;
                }
            }
        }
    }

    ctx->mt = mrp_transport_create(ctx->ctx->ml, atype, &evt, ctx,
            MRP_TRANSPORT_MODE_DATA | MRP_TRANSPORT_NONBLOCK);

    if (ctx->mt == NULL) {
        mrp_log_error("Failed to create the transport");
        goto error;
    }

    if (!mrp_transport_bind(ctx->mt, &addr, alen)) {
        mrp_log_error("Failed to bind the transport to address '%s'", address);
        goto error;
    }


    if (!mrp_transport_listen(ctx->mt, 5)) {
        mrp_log_error("Failed to listen to transport");
        goto error;
    }

    return 0;

error:
    if (ctx->mt) {
        mrp_transport_destroy(ctx->mt);
        ctx->mt = NULL;
    }

    return -1;
}

static void signal_handler(mrp_sighandler_t *h, int signum, void *user_data)
{
    asm_data_t *ctx = (asm_data_t *) user_data;

    MRP_UNUSED(h);

    if (signum == SIGCHLD) {
        /* wait for the child */
        if (ctx->pid > 0) {
            int ret;

            mrp_log_info("Received SIGCHLD, waiting for asm-bridge");

            ret = waitpid(ctx->pid, NULL, WNOHANG);

            if (ret == ctx->pid) {
                mrp_log_warning("asm-bridge process died");
                ctx->pid = 0;
            }

            /* restart the process if the plugin isn't in process of being shut
             * down and the error counter isn't too high */

            if (!ctx->shutting_down && ctx->error_count < 5) {

                ctx->error_count++;

                if (!launch_asm_bridge(ctx)) {
                    mrp_log_error("Error launching asm_bridge");
                }
            }
        }
    }
}

static void htbl_free_client(void *key, void *object)
{
    MRP_UNUSED(key);

    client_t *client = (client_t *) object;

    mrp_htbl_destroy(client->sets, TRUE);
#ifdef FILE_WRITING_HACK
    /* temporary hack*/
    unlink(get_filename(client));
#endif
    /* TODO: free memory when resource API allows that */
    mrp_free(client);
}


static int asm_init(mrp_plugin_t *plugin)
{
    mrp_plugin_arg_t *args = plugin->args;
    asm_data_t *ctx = (asm_data_t *) mrp_allocz(sizeof(asm_data_t));
    mrp_htbl_config_t client_conf;

    if (!ctx) {
        goto error;
    }

    ctx->ctx = plugin->ctx;
    ctx->address = args[ARG_ASM_TPORT_ADDRESS].str;
    ctx->binary = args[ARG_ASM_BRIDGE].str;
    ctx->zone = args[ARG_ASM_ZONE].str;
    ctx->log = args[ARG_ASM_BRIDGE_LOG].str;

    ctx->audio_playback = args[ARG_ASM_AUDIO_PLAYBACK].str;
    ctx->audio_recording = args[ARG_ASM_AUDIO_RECORDING].str;
    ctx->video_playback = args[ARG_ASM_VIDEO_PLAYBACK].str;
    ctx->video_recording = args[ARG_ASM_VIDEO_RECORDING].str;

    if (!init_event_config()) {
        mrp_log_error("Failed to initialize event/class mapping.");
        return FALSE;
    }

    if (!parse_event_config(&args[ARG_ASM_EVENT_CONFIG])) {
        mrp_log_error("Failed to parse event mapping configuration.");
        return FALSE;
    }
    else
        dump_event_config();

    /* create the transport and put it to listen mode */

    if (!mrp_msg_register_type(&asm_to_lib_descr)) {
        mrp_log_error("Failed to register message type asm_to_lib");
        goto error;
    }

    if (!mrp_msg_register_type(&lib_to_asm_descr)) {
        mrp_log_error("Failed to register message type lib_to_asm");
        goto error;
    }

    if (!mrp_msg_register_type(&asm_to_lib_cb_descr)) {
        mrp_log_error("Failed to register message type asm_to_lib_cb");
        goto error;
    }

    if (!mrp_msg_register_type(&lib_to_asm_cb_descr)) {
        mrp_log_error("Failed to register message type lib_to_asm_cb");
        goto error;
    }

    if (tport_setup(ctx->address, ctx) < 0) {
        goto error;
    }

    /* listen to SIGCHLD signal */

    ctx->error_count = 0;
    ctx->shutting_down = FALSE;
    ctx->sighandler = mrp_add_sighandler(plugin->ctx->ml, SIGCHLD,
            signal_handler, ctx);

    if (!ctx->sighandler) {
        mrp_log_error("Failed to register signal handling");
        goto error;
    }

    client_conf.comp = int_comp;
    client_conf.hash = int_hash;
    client_conf.free = htbl_free_client;
    client_conf.nbucket = 0;
    client_conf.nentry = 10;

    ctx->clients = mrp_htbl_create(&client_conf);

    if (!ctx->clients) {
        mrp_log_error("Error creating resource set hash table");
        goto error;
    }

    /* create the client structure towards Murphy */

    ctx->resource_client = mrp_resource_client_create("ASM", ctx);

    if (!ctx->resource_client) {
        mrp_log_error("Failed to get a resource client");
        goto error;
    }

    /* fork-exec the asm bridge binary */

    if (!launch_asm_bridge(ctx)) {
        mrp_log_error("Error launching asm_bridge");
        goto error;
    }

    plugin->data = ctx;

    return TRUE;

error:
    if (!ctx)
        return FALSE;

    ctx->shutting_down = TRUE;

    if (ctx->pid) {
        kill(ctx->pid, SIGTERM);
        ctx->pid = 0;
    }

    if (ctx->clients) {
        mrp_htbl_destroy(ctx->clients, TRUE);
        ctx->clients = NULL;
    }

    if (ctx->sighandler) {
        mrp_del_sighandler(ctx->sighandler);
        ctx->sighandler = NULL;
    }

    if (ctx->resource_client) {
        mrp_resource_client_destroy(ctx->resource_client);
    }

    mrp_free(ctx);

    return FALSE;
}


static void asm_exit(mrp_plugin_t *plugin)
{
    asm_data_t *ctx = (asm_data_t *) plugin->data;

    ctx->shutting_down = TRUE;

    if (ctx->pid) {
        kill(ctx->pid, SIGTERM);
        ctx->pid = 0;
    }

    if (ctx->mt) {
        mrp_transport_disconnect(ctx->mt);
        mrp_transport_destroy(ctx->mt);
        ctx->mt = NULL;
    }

    if (ctx->t) {
        mrp_transport_disconnect(ctx->t);
        mrp_transport_destroy(ctx->t);
        ctx->t = NULL;
    }

    if (ctx->sighandler) {
        mrp_del_sighandler(ctx->sighandler);
        ctx->sighandler = NULL;
    }

    if (ctx->clients) {
        mrp_htbl_destroy(ctx->clients, TRUE);
        ctx->clients = NULL;
    }

    mrp_resource_client_destroy(ctx->resource_client);

    mrp_free(ctx);
    plugin->data = NULL;
}


#define ASM_DESCRIPTION "A plugin to handle SLP Audio Session Manager client requests."
#define ASM_HELP        "Audio Session Manager backend"
#define ASM_VERSION     MRP_VERSION_INT(0, 0, 1)
#define ASM_AUTHORS     "Ismo Puustinen <ismo.puustinen@intel.com>"

static mrp_plugin_arg_t args[] = {
    MRP_PLUGIN_ARGIDX(ARG_ASM_BRIDGE, STRING, "asm_bridge", "/usr/sbin/asm-bridge"),
    MRP_PLUGIN_ARGIDX(ARG_ASM_BRIDGE_LOG, STRING, "asm_bridge_log", NULL),
    MRP_PLUGIN_ARGIDX(ARG_ASM_ZONE, STRING, "zone", "default"),
    MRP_PLUGIN_ARGIDX(ARG_ASM_TPORT_ADDRESS, STRING, "tport_address", DEFAULT_TRANSPORT),
    MRP_PLUGIN_ARGIDX(ARG_ASM_AUDIO_PLAYBACK, STRING, "audio_playback", "audio_playback"),
    MRP_PLUGIN_ARGIDX(ARG_ASM_AUDIO_RECORDING, STRING, "audio_recording", "audio_recording"),
    MRP_PLUGIN_ARGIDX(ARG_ASM_VIDEO_PLAYBACK, STRING, "video_playback", "video_playback"),
    MRP_PLUGIN_ARGIDX(ARG_ASM_VIDEO_RECORDING, STRING, "video_recording", "video_recording"),
    MRP_PLUGIN_ARGIDX(ARG_ASM_EVENT_CONFIG, UNDECL, NULL, NULL)
};


MURPHY_REGISTER_PLUGIN("resource-asm",
                       ASM_VERSION, ASM_DESCRIPTION, ASM_AUTHORS, ASM_HELP,
                       MRP_SINGLETON, asm_init, asm_exit,
                       args, MRP_ARRAY_SIZE(args),
                       NULL, 0, NULL, 0, NULL);
