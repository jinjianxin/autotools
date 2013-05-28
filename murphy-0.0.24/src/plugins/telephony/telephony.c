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

#include <murphy/common.h>
#include <murphy/core.h>
#include <murphy-db/mqi.h>
#include "murphy/plugins/telephony/telephony.h"
#include "murphy/plugins/telephony/ofono.h"

#include "resctl.h"


#define TEL_CALLS_TABLE   "tel_calls"


MQI_COLUMN_DEFINITION_LIST( tel_calls_columndef,
    MQI_COLUMN_DEFINITION( "call_id"     , MQI_VARCHAR(64) ),
    MQI_COLUMN_DEFINITION( "service_id"  , MQI_VARCHAR(64) ),
    MQI_COLUMN_DEFINITION( "line_id"     , MQI_VARCHAR(64) ),
    MQI_COLUMN_DEFINITION( "name"        , MQI_VARCHAR(64) ),
    MQI_COLUMN_DEFINITION( "state"       , MQI_VARCHAR(64) ),
    MQI_COLUMN_DEFINITION( "end_reason"  , MQI_VARCHAR(64) ),
    MQI_COLUMN_DEFINITION( "start_time"  , MQI_VARCHAR(64) ),
    MQI_COLUMN_DEFINITION( "multiparty"  , MQI_UNSIGNED),
    MQI_COLUMN_DEFINITION( "emergency"   , MQI_UNSIGNED)
);


MQI_COLUMN_SELECTION_LIST( tel_calls_columns,
    MQI_COLUMN_SELECTOR(  0, tel_call_t, call_id      ),
    MQI_COLUMN_SELECTOR(  1, tel_call_t, service_id   ),
    MQI_COLUMN_SELECTOR(  2, tel_call_t, line_id      ),
    MQI_COLUMN_SELECTOR(  3, tel_call_t, name         ),
    MQI_COLUMN_SELECTOR(  4, tel_call_t, state        ),
    MQI_COLUMN_SELECTOR(  5, tel_call_t, end_reason   ),
    MQI_COLUMN_SELECTOR(  6, tel_call_t, start_time   ),
    MQI_COLUMN_SELECTOR(  7, tel_call_t, multiparty   ),
    MQI_COLUMN_SELECTOR(  8, tel_call_t, emergency    )
 );


MQI_INDEX_DEFINITION(tel_calls_indexdef,
    MQI_INDEX_COLUMN("call_id")
);


static ofono_t     *ofono     = NULL;

static mqi_handle_t tel_calls = MQI_HANDLE_INVALID;

static resctl_t    *ctl       = NULL;

static void check_resources(void);

/**
 *
 */
int tel_start_listeners(mrp_mainloop_t *ml)
{
    int ret = TRUE;

    mrp_debug("starting listeners");

    if (ml == NULL) {
        mrp_log_error("NULL mainloop");
        return FALSE;
    }
    if (mqi_open() != 0)  {
        mrp_log_error("could not open Murphy database");
        return FALSE;
    }

    /* create if table does not exists */
    tel_calls = mqi_create_table(TEL_CALLS_TABLE, MQI_TEMPORARY,
                                 tel_calls_indexdef, tel_calls_columndef);

    if (tel_calls == MQI_HANDLE_INVALID) {
        mrp_log_error("could not create table in Murphy database");
        return FALSE;
    }

    if (mqi_create_index(tel_calls, tel_calls_indexdef) != 0)
        mrp_log_error("could not create index in Murphy database");

    ofono = ofono_watch(ml, tel_watcher);
    if (ofono == NULL)
        ret = FALSE;
    else
        ctl = resctl_init();

    mrp_debug(ret? "started listeners" : "failed starting listeners");
    return ret;
}


int tel_exit_listeners(void)
{
    int ret;

    mrp_debug("exiting listeners");

    ofono_unwatch(ofono);
    ofono = NULL;
    /* leave the tables in MDB */
    ret = (mqi_close() == 0 ? TRUE : FALSE);
    tel_calls = MQI_HANDLE_INVALID;

    resctl_exit(ctl);
    ctl = NULL;

    mrp_debug(ret? "closed listeners" : "failed closing MDB");
    return ret;
}


/**
 * Track telephony calls and keep Murphy database upodated.
 */

void tel_watcher(int event, tel_call_t *call, void *data)
{
    int n = 0;
    static tel_call_t *call_ptr_list[] = { NULL, NULL };
    char *service = (char*)data;
    mqi_handle_t trh;

    if(call == NULL) {
        mrp_log_error("NULL telephony entry");
        return;
    }

    trh = mqi_begin_transaction();

    mrp_debug("entering telephony watcher");
    switch (event) {

      case TEL_CALL_PURGE: {
        /* remove all calls belonging to the given service (modem) */
        mrp_debug("query for deleting all calls from service %s", service);

        mqi_cond_entry_t where[] = {
            MQI_EQUAL( MQI_COLUMN(1), /* service_id */
                       MQI_STRING_VAR(service) )
            MQI_OPERATOR(end)
        };

        n = MQI_DELETE(tel_calls, where);
        mrp_debug("%d call on service/modem %s removed from Murphy DB",
                  service, n);
        break;
      }

      case TEL_CALL_LISTED:
        /* call was present when connected to the service/modem */
        mrp_debug("query for listing all calls on service %s", service);

      case TEL_CALL_ADDED:
        /* add the call to Murphy DB */
        mrp_debug("query for adding call %s on service %s",
                  call->call_id, service);

        call_ptr_list[0] = call;

        n = MQI_INSERT_INTO(tel_calls, tel_calls_columns, call_ptr_list);

        mrp_debug("%d call inserted into Murphy DB", n);
        break;

      case TEL_CALL_REMOVED: {
        /* remove the call from Murphy DB */
        mrp_debug("query for removing call %s from service %s",
                  call->call_id, service);

        mqi_cond_entry_t where[] = {
            MQI_EQUAL( MQI_COLUMN(0), /* call_id */
                       MQI_STRING_VAR(call->call_id) )
            MQI_OPERATOR(end)
        };

        MQI_DELETE(tel_calls, where);

        mrp_debug("call %s removed from Murphy DB", call->call_id);
        break;
      }

      case TEL_CALL_CHANGED:
        /* update the call in Murphy DB */
        mrp_debug("query for changing call %s on service %s",
                  call->call_id, service);

        call_ptr_list[0] = call;

        MQI_REPLACE(tel_calls, tel_calls_columns, call_ptr_list);

        mrp_debug("call changed in Murphy DB");
        break;

      default:
        mrp_log_error("corrupted telephony event");
    }

    check_resources();

    mqi_commit_transaction(trh);
}


static void check_resources(void)
{
#if 0
    static const char *disconn  = "disconnected";
    static const char *incoming = "incoming";

    MQI_WHERE_CLAUSE(where,
                     MQI_NOT(MQI_EQUAL(MQI_COLUMN(5),
                                       MQI_STRING_VAR(disconn )))  MQI_AND
                     MQI_NOT(MQI_EQUAL(MQI_COLUMN(5),
                                       MQI_STRING_VAR(incoming))) );
#endif

    MQI_COLUMN_SELECTION_LIST(select_columns,
                              MQI_COLUMN_SELECTOR(1, tel_call_t, call_id),
                              MQI_COLUMN_SELECTOR(5, tel_call_t, state  ));


    tel_call_t calls[32];
    int        n, need_audio, i;

    n = MQI_SELECT(select_columns, tel_calls, NULL, calls);

    need_audio = false;
    for (i = 0; i < n; i++) {
        if (strcmp(calls[i].state, "disconnected") ||
            strcmp(calls[i].state, "incoming")) {
            need_audio = true;
            break;
        }
    }

    if (need_audio) {
        if (ctl == NULL)
            ctl = resctl_init();
        resctl_acquire(ctl);
    }
    else
        resctl_release(ctl);
}
