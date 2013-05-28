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

#include <stdarg.h>
#include <string.h>
#include <murphy/common.h>
#include <murphy/core.h>
#include "ofono.h"


/**
 * oFono listener for Murphy
 *
 * For tracking voice calls handled by oFono, first all modems are listed and
 * tracked. Then, for each modem object which has VoiceCallManager interface,
 * VoiceCall objects are tracked and call information is updated to Murphy DB
 * using the provided listener callback (see telephony.c).
 */

#define _NOTIFY_MDB         1

#define OFONO_DBUS          "system"
#define OFONO_DBUS_PATH     "/org/ofono/"
#define OFONO_SERVICE       "org.ofono"
#define OFONO_MODEM_MGR     "org.ofono.Manager"
#define OFONO_MODEM         "org.ofono.Modem"
#define OFONO_CALL_MGR      "org.ofono.VoiceCallManager"
#define OFONO_CALL          "org.ofono.VoiceCall"

#define DUMP_STR(f)         ((f) ? (f)   : "")
#define DUMP_YESNO(f)       ((f) ? "yes" : "no")
#define FREE(p)             if((p) != NULL) mrp_free(p)

typedef void *(*array_cb_t) (DBusMessageIter *it, void *user_data);

/******************************************************************************/

static int install_ofono_handlers(ofono_t *ofono);
static void remove_ofono_handlers(ofono_t *ofono);
static void ofono_init_cb(mrp_dbus_t *dbus, const char *name, int running,
                          const char *owner, void *user_data);

static ofono_modem_t *find_modem(ofono_t *ofono, const char *path);
static int modem_has_interface(ofono_modem_t *modem, const char *interface);
static int modem_has_feature(ofono_modem_t *modem, const char *feature);
static int query_modems(ofono_t *ofono);
static void cancel_modem_query(ofono_t *ofono);
static void modem_query_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *user_data);
static int modem_added_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *user_data);
static int modem_removed_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *user_data);
static int modem_changed_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *user_data);
static int call_endreason_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *user_data);
static void *parse_modem(DBusMessageIter *it, void *user_data);
static void *parse_modem_property(DBusMessageIter *it, void *user_data);
static void purge_modems(ofono_t *ofono);

static ofono_call_t *find_call(ofono_modem_t *modem, const char *path);
static int query_calls(mrp_dbus_t *dbus, ofono_modem_t *modem);
static void call_query_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *user_data);
static int call_added_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *user_data);
static int call_removed_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *user_data);
static int call_changed_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *user_data);
static void *parse_call(DBusMessageIter *it, void *user_data);
static void *parse_call_property(DBusMessageIter *it, void *target);
static void dump_call(ofono_call_t *call);
static void cancel_call_query(ofono_modem_t *modem);
static void purge_calls(ofono_modem_t *modem);

static int parse_dbus_field(DBusMessageIter * iter,
                            const char * token, const char * key,
                            int dbus_type, void * valptr);


/******************************************************************************
 * The public methods of this file: ofono_watch, ofono_unwatch
 */

ofono_t *ofono_watch(mrp_mainloop_t *ml, tel_watcher_t notify)
{
    ofono_t *ofono;
    DBusError dbuserr;
    mrp_dbus_t *dbus;

    mrp_debug("entering ofono_watch");

    dbus = mrp_dbus_connect(ml, OFONO_DBUS, &dbuserr);
    FAIL_IF_NULL(dbus, NULL, "failed to open %s DBUS", OFONO_DBUS);

    ofono = mrp_allocz(sizeof(*ofono));
    FAIL_IF_NULL(ofono, NULL, "failed to allocate ofono proxy");

    mrp_list_init(&ofono->modems);
    ofono->dbus = dbus;

    if (install_ofono_handlers(ofono)) {
        ofono->notify = notify;
        /* query_modems(ofono); */ /* will be done from ofono_init_cb */
        return ofono;
    } else {
        mrp_log_error("failed to set up ofono DBUS handlers");
        ofono_unwatch(ofono);
    }

    return NULL;
}


void ofono_unwatch(ofono_t *ofono)
{
    if (MRP_LIKELY(ofono != NULL)) {
        remove_ofono_handlers(ofono);
        mrp_free(ofono);
    }
}

/******************************************************************************/

static int install_ofono_handlers(ofono_t *ofono)
{
    const char *path;

    if (!mrp_dbus_follow_name(ofono->dbus, OFONO_SERVICE,
                              ofono_init_cb,(void *) ofono))
        goto fail;

    path  = "/";

    /** watch modem change signals */
    if (!mrp_dbus_subscribe_signal(ofono->dbus, modem_added_cb, ofono,
            OFONO_SERVICE, path, OFONO_MODEM_MGR, "ModemAdded", NULL)) {
        mrp_log_error("error watching ModemAdded on %s", OFONO_MODEM_MGR);
        goto fail;
    }

    /* TODO: check if really needed to be handled */
    if (!mrp_dbus_subscribe_signal(ofono->dbus, modem_removed_cb, ofono,
            OFONO_SERVICE, path, OFONO_MODEM_MGR, "ModemRemoved", NULL)) {
        mrp_log_error("error watching ModemRemoved on %s", OFONO_MODEM_MGR);
        goto fail;
    }

    /* TODO: check if really needed to be handled */
    if (!mrp_dbus_subscribe_signal(ofono->dbus, modem_changed_cb, ofono,
            OFONO_SERVICE, NULL, OFONO_MODEM, "PropertyChanged", NULL)) {
        mrp_log_error("error watching PropertyChanged on %s", OFONO_MODEM);
        goto fail;
    }

    /** watch call manager signals from a modem object */
    if (!mrp_dbus_subscribe_signal(ofono->dbus, call_added_cb, ofono,
            OFONO_SERVICE, NULL, OFONO_CALL_MGR, "CallAdded", NULL)) {
        mrp_log_error("error watching CallAdded on %s", OFONO_CALL_MGR);
        goto fail;
    }

    if (!mrp_dbus_subscribe_signal(ofono->dbus, call_removed_cb, ofono,
            OFONO_SERVICE, NULL, OFONO_CALL_MGR, "CallRemoved", NULL)) {
        mrp_log_error("error watching CallRemoved on %s", OFONO_CALL_MGR);
        goto fail;
    }

    /** watch call change signals from a call object */
    if (!mrp_dbus_subscribe_signal(ofono->dbus, call_changed_cb, ofono,
            OFONO_SERVICE, NULL, OFONO_CALL, "PropertyChanged", NULL)) {
        mrp_log_error("error watching PropertyChanged on %s", OFONO_CALL);
        goto fail;
    }

    if (!mrp_dbus_subscribe_signal(ofono->dbus, call_endreason_cb, ofono,
            OFONO_SERVICE, NULL, OFONO_CALL, "DisconnectReason", NULL)) {
        mrp_log_error("error watching DisconnectReason on %s", OFONO_CALL);
        goto fail;
    }

    mrp_debug("installed oFono signal handlers");
    return TRUE;

fail:
    remove_ofono_handlers(ofono);
    mrp_log_error("failed to install oFono signal handlers");
    return FALSE;
}


static void remove_ofono_handlers(ofono_t *ofono)
{
    const char *path;

    FAIL_IF_NULL(ofono,, "trying to remove handlers from NULL ofono");
    FAIL_IF_NULL(ofono->dbus,, "ofono->dbus is NULL");

    mrp_dbus_forget_name(ofono->dbus, OFONO_SERVICE,
                         ofono_init_cb, (void *) ofono);

    path  = "/";
    mrp_debug("removing DBUS signal watchers");

    mrp_dbus_unsubscribe_signal(ofono->dbus, modem_added_cb, ofono,
        OFONO_SERVICE, path, OFONO_MODEM_MGR, "ModemAdded", NULL);

    mrp_dbus_unsubscribe_signal(ofono->dbus, modem_removed_cb, ofono,
        OFONO_SERVICE, path, OFONO_MODEM_MGR, "ModemRemoved", NULL);

    mrp_dbus_unsubscribe_signal(ofono->dbus, modem_changed_cb, ofono,
        OFONO_SERVICE, NULL, OFONO_MODEM, "PropertyChanged", NULL);

    mrp_dbus_unsubscribe_signal(ofono->dbus, call_added_cb, ofono,
        OFONO_SERVICE, NULL, OFONO_CALL_MGR, "CallAdded", NULL);

    mrp_dbus_unsubscribe_signal(ofono->dbus, call_removed_cb, ofono,
        OFONO_SERVICE, NULL, OFONO_CALL_MGR, "CallRemoved", NULL);

    mrp_dbus_unsubscribe_signal(ofono->dbus, call_changed_cb, ofono,
        OFONO_SERVICE, NULL, OFONO_CALL, "PropertyChanged", NULL);

    mrp_dbus_unsubscribe_signal(ofono->dbus, call_changed_cb, ofono,
        OFONO_SERVICE, NULL, OFONO_CALL, "DisconnectReason", NULL);

}


static void ofono_init_cb(mrp_dbus_t *dbus, const char *name, int running,
                         const char *owner, void *user_data)
{
    ofono_t *ofono = (ofono_t *)user_data;
    (void)dbus;

    FAIL_IF_NULL(ofono,, "passed NULL user pointer to ofono proxy");
    mrp_debug("%s is %s with owner ", name, running ? "up" : "down", owner);

    if (!running)
        purge_modems(ofono);
    else
        query_modems(ofono);
}


static int dbus_array_foreach(DBusMessageIter *it, array_cb_t callback,
                         void *user_data)
{
    DBusMessageIter arr;

    if (dbus_message_iter_get_arg_type(it) != DBUS_TYPE_ARRAY)
        return FALSE;

    for (dbus_message_iter_recurse(it, &arr);
         dbus_message_iter_get_arg_type(&arr) != DBUS_TYPE_INVALID;
         dbus_message_iter_next(&arr)) {

        if (callback(&arr, user_data) == NULL)
            return FALSE;
    }

    return TRUE;
}


static void free_strarr(char **strarr)
{
    int n;

    if (strarr != NULL) {
        for (n = 0; strarr[n] != NULL; n++)
            mrp_free(strarr[n]);

        mrp_free(strarr);
    }
}


static int strarr_contains(char **strarr, const char *str)
{
    int n;

    if (strarr != NULL)
        for (n = 0; strarr[n] != NULL; n++)
            if (!strcmp(strarr[n], str))
                return TRUE;

    return FALSE;
}


/******************************************************************************/

static void dump_modem(ofono_modem_t *m)
{
    ofono_call_t     *call;
    mrp_list_hook_t  *p, *n;
    int               i;


    mrp_debug("modem '%s' {", m->modem_id);
    mrp_debug("    name:         '%s'", DUMP_STR(m->name));
    mrp_debug("    manufacturer: '%s'", DUMP_STR(m->manufacturer));
    mrp_debug("    model:        '%s'", DUMP_STR(m->model));
    mrp_debug("    revision:     '%s'", DUMP_STR(m->revision));
    mrp_debug("    serial:       '%s'", DUMP_STR(m->serial));
    mrp_debug("    type:         '%s'", DUMP_STR(m->type));

    if (m->interfaces != NULL) {
        mrp_debug("    supported interfaces:");
        for (i = 0; m->interfaces[i] != NULL; i++)
            mrp_debug("        %s", m->interfaces[i]);
    }

    if (m->features != NULL) {
        mrp_debug("    supported features:");
        for (i = 0; m->features[i] != NULL; i++)
            mrp_debug("        %s", m->features[i]);
    }

    mrp_debug("    is powered %s", m->powered ? "on" : "off");
    mrp_debug("    is %sline" , m->online ? "on" : "off");
    mrp_debug("    is %slocked", m->lockdown ? "" : "un");
    mrp_debug("    has %s emergency call", m->emergency ? "ongoing" : "no");

    mrp_debug("    calls:");
    if (mrp_list_empty(&m->calls))
        mrp_debug("    none");
    else {
        mrp_list_foreach(&m->calls, p, n) {
            call = mrp_list_entry(p, ofono_call_t, hook);
            dump_call(call);
        }
    }

    mrp_debug("}");
}


static int free_modem(ofono_modem_t *modem)
{
    if (modem != NULL) {
        mrp_list_delete(&modem->hook);

        cancel_call_query(modem);
        purge_calls(modem);

        mrp_free(modem->modem_id);
        mrp_free(modem->name);
        mrp_free(modem->manufacturer);
        mrp_free(modem->model);
        mrp_free(modem->revision);
        mrp_free(modem->serial);
        mrp_free(modem->type);

        mrp_free(modem);
        return TRUE;
    }
    return FALSE;
}


static void purge_modems(ofono_t *ofono)
{
    mrp_list_hook_t *p, *n;
    ofono_modem_t  *modem;

    FAIL_IF_NULL(ofono, ,"attempt to purge NULL ofono proxy");
    FAIL_IF_NULL(ofono->dbus, ,"ofono->dbus is NULL");

    if (ofono->modem_qry != 0) {
        mrp_dbus_call_cancel(ofono->dbus, ofono->modem_qry);
        ofono->modem_qry = 0;
    }

    mrp_list_foreach(&ofono->modems, p, n) {
        modem = mrp_list_entry(p, ofono_modem_t, hook);
        free_modem(modem);
    }
}

static void dump_call(ofono_call_t *call)
{

    if (call == NULL) {
        mrp_debug("    none");
        return;
    }

    mrp_debug("call '%s' {", call->call_id);
    mrp_debug("    service_id:           '%s'", DUMP_STR(call->service_id));
    mrp_debug("    line_id:              '%s'", DUMP_STR(call->line_id));
    mrp_debug("    name:                 '%s'", DUMP_STR(call->name));
    mrp_debug("    state:                '%s'", DUMP_STR(call->state));
    mrp_debug("    end_reason:           '%s'", DUMP_STR(call->end_reason));
    mrp_debug("    start_time:           '%s'", DUMP_STR(call->start_time));
    mrp_debug("    is multiparty:        '%s'", DUMP_YESNO(call->multiparty));
    mrp_debug("    is emergency:         '%s'", DUMP_YESNO(call->emergency));
    mrp_debug("    information:          '%s'", DUMP_STR(call->info));
    mrp_debug("    icon_id:              '%u'", call->icon_id);
    mrp_debug("    remote held:          '%s'", DUMP_YESNO(call->remoteheld));
    mrp_debug("}");
}


static void free_call(ofono_call_t *call)
{
    if (call) {
        mrp_list_delete(&call->hook);
        mrp_free(call->call_id);
        mrp_free(call->service_id);
        mrp_free(call->line_id);
        mrp_free(call->name);
        mrp_free(call->state);
        mrp_free(call->end_reason);
        mrp_free(call->start_time);
        mrp_free(call->info);

        mrp_free(call);
    }
}


static void purge_calls(ofono_modem_t *modem)
{
    mrp_list_hook_t *p, *n;
    ofono_call_t  *call;
    ofono_t * ofono;

    if (modem) {
        ofono = modem->ofono;
        if (ofono->dbus) {
            if (modem->call_qry != 0) {
                mrp_dbus_call_cancel(ofono->dbus, modem->call_qry);
                modem->call_qry = 0;
            }

            mrp_list_foreach(&modem->calls, p, n) {
                call = mrp_list_entry(p, ofono_call_t, hook);
                ofono->notify(TEL_CALL_REMOVED, (tel_call_t *)call, modem);
                free_call(call);
            }
        }
    }
}


/*******************************************************************************
 * ofono modem handling
 */

ofono_modem_t *ofono_online_modem(ofono_t *ofono)
{
    ofono_modem_t  *modem;
    mrp_list_hook_t *p, *n;

    mrp_list_foreach(&ofono->modems, p, n) {
        modem = mrp_list_entry(p, ofono_modem_t, hook);

        if (modem->powered && modem->online) {
            return modem;
        }
    }
    return NULL;
}


static int modem_has_interface(ofono_modem_t *modem, const char *interface)
{
    (void) modem_has_feature; /* to suppress unused function warning */

    mrp_debug("checking interface %s on modem %s, with interfaces",
              interface, modem->modem_id, modem->interfaces);

    return strarr_contains(modem->interfaces, interface);
}


static int modem_has_feature(ofono_modem_t *modem, const char *feature)
{
    mrp_debug("checking feature %s on modem %s, with features",
              feature, modem->modem_id, modem->features);

    return strarr_contains(modem->features, feature);
}


static int query_modems(ofono_t *ofono)
{
    int32_t    qry;

    mrp_debug("querying modems on oFono");
    FAIL_IF_NULL(ofono, FALSE, "ofono is NULL");
    FAIL_IF_NULL(ofono->dbus, FALSE, "ofono->dbus is NULL");

    cancel_modem_query(ofono);

    qry = mrp_dbus_call(ofono->dbus, OFONO_SERVICE, "/",
                        OFONO_MODEM_MGR, "GetModems", 5000,
                        modem_query_cb, ofono, DBUS_TYPE_INVALID);

    ofono->modem_qry = qry;

    return qry != 0;
}


static void cancel_modem_query(ofono_t *ofono)
{
    FAIL_IF_NULL(ofono, , "ofono is NULL");

    if (ofono->dbus != NULL && ofono->modem_qry)
        mrp_dbus_call_cancel(ofono->dbus, ofono->modem_qry);

    ofono->modem_qry = 0;
}


static void modem_query_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *user_data)
{
    DBusMessageIter  it;
    ofono_t         *ofono = (ofono_t *)user_data;
    ofono_modem_t   *modem;
    mrp_list_hook_t  *p, *n;

    mrp_debug("modem query response on oFono");

    if (dbus_message_iter_init(msg, &it)) {
        if (dbus_array_foreach(&it, parse_modem, ofono)) {
            /* array_foreach successfully fetched all modems into the list */
            mrp_list_foreach(&ofono->modems, p, n) {
                modem = mrp_list_entry(p, ofono_modem_t, hook);
                query_calls(dbus, modem);
            }
            return;
        }
    }

    mrp_log_error("failed to process modem query response");
}


static ofono_modem_t *find_modem(ofono_t *ofono, const char *path)
{
    mrp_list_hook_t *p, *n;
    ofono_modem_t  *modem;

    FAIL_IF_NULL(ofono, NULL, "ofono is NULL");
    FAIL_IF_NULL(path, NULL, "path is NULL");

    mrp_list_foreach(&ofono->modems, p, n) {
        modem = mrp_list_entry(p, ofono_modem_t, hook);

        if (modem->modem_id != NULL && !strcmp(modem->modem_id, path))
            return modem;
    }

    return NULL;
}


static int modem_added_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *user_data)
{
    ofono_t         *ofono = (ofono_t *)user_data;
    ofono_modem_t   *modem;
    DBusMessageIter  it;

    (void)dbus;

    mrp_debug("new modem signaled to be added on oFono...");
    FAIL_IF_NULL(ofono, FALSE, "ofono is NULL");

    if (dbus_message_iter_init(msg, &it)) {
        modem = (ofono_modem_t*) parse_modem(&it, ofono);
        return query_calls(dbus, modem);
    }

    return FALSE;
}


static int modem_removed_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *user_data)
{
    ofono_t       *ofono = (ofono_t *)user_data;
    ofono_modem_t *modem;
    const char    *path;

    (void)dbus;

    if (dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &path,
                              DBUS_TYPE_INVALID)) {
        mrp_debug("modem '%s' was removed", path);

        modem = find_modem(ofono, path);
        if(modem != NULL)
            return free_modem(modem);
    }

    return FALSE;
}


static int modem_changed_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *user_data)
{
    const char      *path;
    ofono_t         *ofono;
    ofono_modem_t   *modem;
    DBusMessageIter  it;

    (void)dbus;
    path = dbus_message_get_path(msg);
    ofono = (ofono_t *)user_data;
    modem = find_modem(ofono, path);

    if (modem != NULL && dbus_message_iter_init(msg, &it)) {
        mrp_debug("changes in modem '%s'...", modem->modem_id);

        if (parse_modem_property(&it, modem)) {
            dump_modem(modem);
            /* place to handle Online, Powered, Lockdown signals */
            /* since call objects will be signaled separately, do nothing */
        }
    }

    return TRUE;
}


static void *parse_modem(DBusMessageIter *msg, void *user_data)
{
    ofono_t         *ofono;
    ofono_modem_t   *modem;
    char            *path;
    DBusMessageIter  mdm, *it;
    int              type;

    ofono = (ofono_t *)user_data;
    FAIL_IF_NULL(ofono, FALSE, "ofono is NULL");

    modem = NULL;

    mrp_debug("parsing ofono modem");

    /*
     * We can be called from an initial modem query callback
     *    array{object, dict} GetModems(), where dict is array{property, value}
     * on each element of the array,
     * or then from a modem add notification callback:
     *    ModemAdded(object_path, dict)
     * We first take care of the message content differences here
     * (object vs object path)
     */

    if ((type = dbus_message_iter_get_arg_type(msg)) == DBUS_TYPE_STRUCT) {
        dbus_message_iter_recurse(msg, &mdm);
        it = &mdm;
    }
    else {
        if (type == DBUS_TYPE_OBJECT_PATH)
            it = msg;
        else
            goto malformed;
    }

    if (dbus_message_iter_get_arg_type(it) != DBUS_TYPE_OBJECT_PATH)
        goto malformed;

    /* at this point we have an object path and a property-value array (dict) */
    dbus_message_iter_get_basic(it, &path);
    dbus_message_iter_next(it);

    if (dbus_message_iter_get_arg_type(it) != DBUS_TYPE_ARRAY)
        goto malformed;

    if ((modem = mrp_allocz(sizeof(*modem))) == NULL)
        goto fail;

    mrp_list_init(&modem->hook);
    mrp_list_init(&modem->calls);
    modem->ofono = ofono;

    if ((modem->modem_id = mrp_strdup(path)) == NULL)
        goto fail;

    mrp_list_append(&ofono->modems, &modem->hook);

    /* iterate over all properties and try to match to our data */
    if (!dbus_array_foreach(it, parse_modem_property, modem))
        goto malformed;

    mrp_debug("finished parsing modem %s", modem->modem_id);

    return (void *)modem;

malformed:
    mrp_log_error("malformed modem entry");

fail:
    free_modem(modem);

    return NULL;
}



static void *parse_modem_property(DBusMessageIter *it, void *target)
{
    ofono_modem_t   *modem = (ofono_modem_t *)target;
    DBusMessageIter  dict, iter, *prop;
    const char      *key;
    int              t, i;

    /* The properties of interest of the DBUS message to be parsed. */
    struct _field {
        const char *field;
        int         type;
        void       *valptr;
    } spec [] = {
        { "Type",         DBUS_TYPE_STRING,  &modem->type         },
        { "Powered",      DBUS_TYPE_BOOLEAN, &modem->powered      },
        { "Online",       DBUS_TYPE_BOOLEAN, &modem->online       },
        { "Lockdown",     DBUS_TYPE_BOOLEAN, &modem->lockdown     },
        { "Emergency",    DBUS_TYPE_BOOLEAN, &modem->emergency    },
        { "Name",         DBUS_TYPE_STRING,  &modem->name         },
        { "Manufacturer", DBUS_TYPE_STRING,  &modem->manufacturer },
        { "Model",        DBUS_TYPE_STRING,  &modem->model        },
        { "Revision",     DBUS_TYPE_STRING,  &modem->revision     },
        { "Serial",       DBUS_TYPE_STRING,  &modem->serial       },
        { "Interfaces",   DBUS_TYPE_ARRAY,   &modem->interfaces   },
        { "Features",     DBUS_TYPE_ARRAY,   &modem->features     }
        /* other properties are ignored */
    };
    int spec_size = 12;

    key = NULL;
    t   = dbus_message_iter_get_arg_type(it);

    /*
     * We are either called from a modem query response callback, or
     * from a modem property change notification callback. In the former
     * case we get an iterator pointing to a dictionary entry, in the
     * latter case we get an iterator already pointing to the property
     * key. We take care of the differences here...
     */

    if (t == DBUS_TYPE_DICT_ENTRY) {
        dbus_message_iter_recurse(it, &dict);

        FAIL_IF(dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_STRING,
                NULL, "malformed call entry");

        prop = &dict;
    }
    else {
        FAIL_IF(t != DBUS_TYPE_STRING,
                NULL, "malformed call entry, string expected");
        prop = it;
    }

    dbus_message_iter_get_basic(prop, &key);
    dbus_message_iter_next(prop);

    FAIL_IF(dbus_message_iter_get_arg_type(prop) != DBUS_TYPE_VARIANT,
            NULL,"malformed call entry for key %s", key);

    dbus_message_iter_recurse(prop, &iter);

    /*
     * there will be maximum 1 match for the given key
     * error is returned only if a field is matched, but value fetching fails
     */
    for (i = 0, t = -1; i < spec_size && t < 0; i++)
        if ((t = parse_dbus_field(&iter, spec[i].field, key,
                                  spec[i].type, spec[i].valptr)) == 0)
            return NULL;

    return modem;
}


/*******************************************************************************
 * ofono call manager
 */

static ofono_call_t *find_call(ofono_modem_t *modem, const char *path)
{
    mrp_list_hook_t *p, *n;
    ofono_call_t  *call;

    mrp_list_foreach(&modem->calls, p, n) {
        call = mrp_list_entry(p, ofono_call_t, hook);

        if (call->call_id != NULL && !strcmp(call->call_id, path))
            return call;
    }

    return NULL;
}


static int query_calls(mrp_dbus_t *dbus, ofono_modem_t *modem)
{
    FAIL_IF_NULL(modem, FALSE, "modem is NULL");

    if (modem->call_qry != 0)
        return TRUE;    /* already querying */

    if (modem->online && modem_has_interface(modem, OFONO_CALL_MGR)) {
        modem->call_qry  = mrp_dbus_call(dbus, OFONO_SERVICE, modem->modem_id,
                                       OFONO_CALL_MGR, "GetCalls", 5000,
                                       call_query_cb, modem, DBUS_TYPE_INVALID);

        return modem->call_qry != 0;
    } else
        mrp_debug("call query canceled on modem %s: offline or no callmanager",
                  modem->modem_id);


    return FALSE;
}


static void cancel_call_query(ofono_modem_t *modem)
{
    FAIL_IF_NULL(modem, ,"modem is NULL");
    FAIL_IF_NULL(modem->ofono, ,"ofono is NULL in modem %s", modem->modem_id);

    if (modem->ofono->dbus && modem->call_qry != 0)
        mrp_dbus_call_cancel(modem->ofono->dbus, modem->call_qry);

    modem->call_qry = 0;
}


static void call_query_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *user_data)
{
    ofono_modem_t   *modem = (ofono_modem_t *)user_data;
    ofono_t         *ofono = modem->ofono;
    ofono_call_t    *call;
    DBusMessageIter  it;
    mrp_list_hook_t *p, *n;

    (void)dbus;

    FAIL_IF_NULL(ofono, , "ofono is NULL");
    FAIL_IF_NULL(modem, , "modem is NULL");

    mrp_debug("call query callback on modem %s", modem->modem_id);

    modem->call_qry = 0;

    if (dbus_message_iter_init(msg, &it)) {
        if (!dbus_array_foreach(&it, parse_call, modem)) {
            mrp_log_error("failed processing call query response");
            return;
        }
        mrp_debug("calling notify TEL_CALL_PURGE on modem %s", modem->modem_id);
        /*modem->ofono->notify(TEL_CALL_PURGE, NULL, modem->modem_id);*/
        mrp_list_foreach(&modem->calls, p, n) {
            call = mrp_list_entry(p, ofono_call_t, hook);
            mrp_debug("calling notify TEL_CALL_LISTED on call %s, modem %s",
                      (tel_call_t*)call->call_id, modem->modem_id);
#if _NOTIFY_MDB
            ofono->notify(TEL_CALL_LISTED, (tel_call_t*)call, modem->modem_id);
#else
            dump_call((ofono_call_t*)call);
#endif
            mrp_debug("new oFono call listed: %s", call->call_id);
        }
        return;
    }
    mrp_log_error("listing oFono calls failed");
}


static int call_added_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *user_data)
{
    ofono_t         *ofono;
    ofono_modem_t   *modem;
    ofono_call_t    *call;
    DBusMessageIter  it;

    (void)dbus;

    ofono = (ofono_t *)user_data;
    FAIL_IF_NULL(ofono, FALSE, "ofono is NULL");

    modem = find_modem(ofono, dbus_message_get_path (msg));
    FAIL_IF_NULL(modem, FALSE, "modem is NULL");
    FAIL_IF(modem->ofono != ofono, FALSE, "corrupted modem data");

    mrp_debug("new oFono call signaled on modem %s", modem->modem_id);

    if (dbus_message_iter_init(msg, &it)) {
        call = parse_call(&it, modem);
        if (call) {
            FAIL_IF_NULL(modem->ofono->notify, FALSE, "notify is NULL");
            mrp_debug("calling notify TEL_CALL_ADDED on call %s, modem %s",
                      (tel_call_t*)call->call_id, modem->modem_id);
#if _NOTIFY_MDB
            ofono->notify(TEL_CALL_ADDED, (tel_call_t*)call, modem->modem_id);
#else
            dump_call((ofono_call_t*)call);
#endif
            mrp_debug("new oFono call added: %s", call->call_id);
            dump_modem(modem);
            return TRUE;
        }
    }

    mrp_log_error("adding new oFono call failed");
    return FALSE;
}


static int call_removed_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *user_data)
{
    ofono_t         *ofono;
    ofono_modem_t   *modem;
    ofono_call_t    *call;
    const char      *path;

    (void)dbus;

    ofono = (ofono_t *)user_data;
    FAIL_IF_NULL(ofono, FALSE, "ofono is NULL");

    path = dbus_message_get_path (msg);
    modem = find_modem(ofono, path);

    FAIL_IF_NULL(modem, FALSE, "modem is not found based on path %s", path);
    FAIL_IF(modem->ofono != ofono, FALSE, "corrupted modem data");

    if (dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &path,
                              DBUS_TYPE_INVALID)) {
        mrp_debug("call '%s' signaled to be removed", path);
        call = find_call(modem, path);
        if (call) {
            FAIL_IF_NULL(ofono->notify, FALSE, "notify is NULL");
            mrp_debug("calling notify TEL_CALL_REMOVED on call %s, modem %s",
                      (tel_call_t*)call->call_id, modem->modem_id);
#if _NOTIFY_MDB
            ofono->notify(TEL_CALL_REMOVED, (tel_call_t*)call, modem->modem_id);
#else
            dump_call((ofono_call_t*)call);
#endif
            mrp_debug("oFono call removed: %s", call->call_id);
            free_call(call);
            dump_modem(modem);
            return TRUE;
        } else
            mrp_log_error("could not find call based on path %s", path);
    } else
        mrp_log_error("removing oFono call failed: could not get DBUS path");

    return FALSE;
}


/*******************************************************************************
 * ofono call handling
 */

/**
 * find service_id (modem) from call path (call_id)
 * example: path = "/hfp/00DBDF143ADC_44C05C71BAF6/voicecall01",
 * modem_id = "/hfp/00DBDF143ADC_44C05C71BAF6"
 * call_id = path
 */
static void get_modem_from_call_path(char *call_path, char **modem_id)
{
    char * dest = NULL;
    unsigned int i = (call_path == NULL ? 0 : strlen(call_path)-1);

    for(; i > 0 && call_path[i] != '/'; i--);
    if(i > 0) {
        call_path[i] = '\0';
        dest = mrp_strdup(call_path);
        call_path[i] = '/'; /* restore path */
    }
    *modem_id = dest;
}


static int call_changed_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *user_data)
{
    char            *path, *modem_id;
    DBusMessageIter  it;
    ofono_t         *ofono;
    ofono_modem_t   *modem;
    ofono_call_t    *call;

    (void)dbus;

    ofono = (ofono_t *)user_data;
    FAIL_IF_NULL(ofono, FALSE, "ofono is NULL");

    path = (char *) dbus_message_get_path(msg); /* contains call object path */
    get_modem_from_call_path(path, &modem_id);  /* get modem from modem path */
    modem = find_modem(ofono, modem_id);        /* modem path is modem_id    */
    FAIL_IF_NULL(modem, FALSE, "modem is NULL");
    FAIL_IF(modem->ofono != ofono, FALSE, "corrupted modem data");

    call = find_call(modem, path);
    FAIL_IF_NULL(call, FALSE, "call not found based on path %s", path);

    FAIL_IF(!dbus_message_iter_init(msg, &it), FALSE,
            "parsing error in call change callback for %s", call->call_id);

    mrp_debug("changes in call '%s'...", path);

    FAIL_IF(!parse_call_property(&it, call), FALSE,
            "parsing error in call change callback for %s", call->call_id);

    FAIL_IF_NULL(ofono->notify, FALSE, "notify is NULL");

    mrp_debug("calling notify TEL_CALL_CHANGED on call %s, modem %s",
              (tel_call_t*)call->call_id, modem->modem_id);

#if _NOTIFY_MDB
    ofono->notify(TEL_CALL_CHANGED, (tel_call_t*)call, modem->modem_id);
#else
    dump_call((ofono_call_t*)call);
#endif
    mrp_debug("oFono call changed: %s", call->call_id);
    dump_modem(modem);
    return TRUE;
}


static int call_endreason_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *user_data)
{
    char            *path, *modem_id;
    DBusMessageIter  it;
    ofono_t         *ofono;
    ofono_modem_t   *modem;
    ofono_call_t    *call;

    (void)dbus;

    ofono = (ofono_t *)user_data;
    FAIL_IF_NULL(ofono, FALSE, "ofono is NULL");

    path = (char *) dbus_message_get_path(msg); /* contains call object path */
    get_modem_from_call_path(path, &modem_id);  /* get modem from modem path */
    modem = find_modem(ofono, modem_id);        /* modem path is modem_id    */

    FAIL_IF_NULL(modem, FALSE, "modem is NULL");
    FAIL_IF(modem->ofono != ofono, FALSE, "corrupted modem data");

    call = find_call(modem, path);
    FAIL_IF_NULL(call, FALSE, "call not found based on path %s", path);

    if (dbus_message_iter_init(msg, &it)) {
        FAIL_IF(dbus_message_iter_get_arg_type(&it) != DBUS_TYPE_STRING,
                FALSE,
                "wrong dbus argument type in call change callback for %s",
                call->call_id);

        dbus_message_iter_get_basic(&it, &path);
        call->end_reason = mrp_strdup(path);

        FAIL_IF(call->end_reason == NULL, FALSE,
                "failed in mrp_strdup %s", path);

        mrp_debug("disconnect reason in call '%s': %s",
                  call->call_id, call->end_reason);

        FAIL_IF_NULL(ofono->notify, FALSE, "notify is NULL");
        mrp_debug("calling notify TEL_CALL_CHANGED on call %s, modem %s",
                  (tel_call_t*)call->call_id, modem->modem_id);
#if _NOTIFY_MDB
        ofono->notify(TEL_CALL_CHANGED, (tel_call_t*)call, modem->modem_id);
#else
        dump_call((ofono_call_t*)call);
#endif
        mrp_debug("oFono call end reason changed: %s", call->call_id);
        dump_modem(modem);
        return TRUE;
    } else
        mrp_debug("dbus iterator error in call end reason callback for %s",
                  call->call_id);

    return FALSE;
}


static void *parse_call(DBusMessageIter *msg, void *user_data)
{
    ofono_modem_t   *modem;
    ofono_call_t    *call;
    char            *path;
    DBusMessageIter  sub, *it;
    int              type;

    modem = (ofono_modem_t *)user_data;
    FAIL_IF_NULL(modem, FALSE, "modem is NULL");

    call = NULL;

    mrp_debug("parsing call in modem '%s'...", modem->modem_id);

    /*
     * We can be called from an initial call query callback, which returns
     * array of { object, property-dictionary },
     * or from a "call added" notification callback, which provides
     * and object path and a property-dictionary.
     * We first take care of the message content differences here.
     */

    if ((type = dbus_message_iter_get_arg_type(msg)) == DBUS_TYPE_STRUCT) {
        dbus_message_iter_recurse(msg, &sub);
        it = &sub;
    }
    else {
        if (type == DBUS_TYPE_OBJECT_PATH)
            it = msg;
        else
            goto malformed;
    }

    /* in both cases the first should be an object path */
    if (dbus_message_iter_get_arg_type(it) != DBUS_TYPE_OBJECT_PATH)
        goto malformed;

    dbus_message_iter_get_basic(it, &path);
    if(path == NULL)
        goto fail;

    dbus_message_iter_next(it);

    if (dbus_message_iter_get_arg_type(it) != DBUS_TYPE_ARRAY)
        goto malformed;

    if ((call = mrp_allocz(sizeof(*call))) == NULL)
        goto fail;

    mrp_list_init(&call->hook);
    mrp_list_init(&modem->calls);
    call->modem = modem;

    if ((call->call_id = mrp_strdup(path)) == NULL)
        goto fail;

    get_modem_from_call_path(path, &(call->service_id));

    mrp_list_append(&modem->calls, &call->hook);

    /* iterate over all properties and try to match to our data */
    if (!dbus_array_foreach(it, parse_call_property, call))
        goto malformed;

    mrp_debug("finished parsing call %s", call->call_id);\
    return (void *)call;

malformed:
    mrp_log_error("malformed call entry");

fail:
    free_call(call);
    return NULL;
}


static void *parse_call_property(DBusMessageIter *it, void *target)
{
    DBusMessageIter  dict, iter, *prop;
    const char      *key;
    int              i, t;
    ofono_call_t    *call = (ofono_call_t *) target;

    /* The properties of interest of the DBUS message to be parsed. */
    struct _field {
        const char *field;
        int         type;
        void       *valptr;
    } spec [] = {
        { "IncomingLine", DBUS_TYPE_STRING,  &call->incoming_line },
        { "Name",         DBUS_TYPE_STRING,  &call->name          },
        { "Multiparty",   DBUS_TYPE_BOOLEAN, &call->multiparty    },
        { "State",        DBUS_TYPE_STRING,  &call->state         },
        { "StartTime",    DBUS_TYPE_STRING,  &call->start_time    },
        { "Information",  DBUS_TYPE_STRING,  &call->info          },
        { "Icon",         DBUS_TYPE_BYTE,    &call->icon_id       },
        { "Emergency",    DBUS_TYPE_BOOLEAN, &call->emergency     },
        { "RemoteHeld",   DBUS_TYPE_BOOLEAN, &call->remoteheld    }
        /* other properties are ignored */
    };
    int spec_size = sizeof(spec) / sizeof(spec[0]);


    key = NULL;
    t   = dbus_message_iter_get_arg_type(it);

    /*
     * We are either called from a call query response callback, or
     * from a call propery change notification callback. In the former
     * case we get an iterator pointing to a dictionary entry, in the
     * latter case we get an iterator already pointing to the property
     * key. We take care of the differences here...
     */

    if (t == DBUS_TYPE_DICT_ENTRY) {
        dbus_message_iter_recurse(it, &dict);

        FAIL_IF(dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_STRING,
                NULL, "malformed call entry");

        prop = &dict;
    }
    else {
        FAIL_IF(t != DBUS_TYPE_STRING,
                NULL, "malformed call entry, string expected");
        prop = it;
    }

    dbus_message_iter_get_basic(prop, &key);
    dbus_message_iter_next(prop);

    FAIL_IF(dbus_message_iter_get_arg_type(prop) != DBUS_TYPE_VARIANT,
            NULL,"malformed call entry for key %s", key);

    dbus_message_iter_recurse(prop, &iter);

    /*
     * there will be maximum 1 match for the given key
     * error is returned only if a field is matched, but value fetching fails
     */
    for (i = 0, t = -1; i < spec_size && t < 0; i++)
        if ((t = parse_dbus_field(&iter, spec[i].field, key,
                                  spec[i].type, spec[i].valptr)) == 0)
            return NULL;

    return call;
}


/**
 * Match the current field against the given property,
 * and if matched, fetch the value.
 */
static int parse_dbus_field(DBusMessageIter * iter,
                            const char * field, const char * prop,
                            int dbus_type, void * valptr)
{
    int t, n;
    DBusMessageIter arr;
    char ** strarr, *str;

    if (strcmp(field, prop))
        return -1; /* no match, but not an error */

    mrp_debug("matched property %s", field);

    if (dbus_type == DBUS_TYPE_STRING || dbus_type == DBUS_TYPE_OBJECT_PATH)
        mrp_free(*(char **)valptr);

    if (dbus_type == DBUS_TYPE_ARRAY)
    {
        dbus_message_iter_recurse(iter, &arr);
        strarr = NULL;
        n = 1;

        while ((t = dbus_message_iter_get_arg_type(&arr)) == DBUS_TYPE_STRING) {
            dbus_message_iter_get_basic(&arr, &str);

            if (mrp_reallocz(strarr, n, n + 1) != NULL) {
                if ((strarr[n-1] = mrp_strdup(str)) == NULL) {
                    free_strarr(strarr);
                    return 0;
                }
                n++;
            }

            dbus_message_iter_next(&arr);
        }

        if (t == DBUS_TYPE_INVALID) {
            free_strarr(*(char ***)valptr);
            *(char ***)valptr = strarr;
        }
        else {
            mrp_log_error("malformed array entry for property %s", prop);
            free_strarr(strarr);
            return 0;
        }
    } else {

        if (dbus_message_iter_get_arg_type(iter) != dbus_type) {
            mrp_log_error("malformed modem entry for property %s", prop);
            return 0;
        } else
            dbus_message_iter_get_basic(iter, valptr);

        if (dbus_type == DBUS_TYPE_STRING ||
                dbus_type == DBUS_TYPE_OBJECT_PATH) {
            *(char **)valptr = mrp_strdup(*(char **)valptr);

            if (*(char **)valptr == NULL) {
                mrp_log_error("failed to allocate property %s", prop);
                return 0;
            }
        }
    }

    return 1; /* successful match and value fetch */
}
