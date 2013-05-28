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

#ifndef __MURPHY_TELEPHONY_H__
#define __MURPHY_TELEPHONY_H__

#include <murphy/common/list.h>
#include <murphy/common/mainloop.h>


/******************************************************************************
 * Define Murphy database table schema for telephony calls.
 * Currently works with oFono, but should also work for VoIP (e.g. Telepathy).
 * More fields could be added when needed (e.g. network related).
 */
typedef struct {
    mrp_list_hook_t hook;          /* this is not part of the DB schema */
    char           *call_id;       /* call path */
    char           *service_id;    /* service/modem path owning the call */
    const char     *line_id;       /* e.g. phone number */
    char           *name;          /* name identification by network */
    char           *state;         /* "active","held","dialing","alerting",
                                      "incoming","waiting","disconnected" */
    char           *end_reason;    /* "local","remote","network" */
    const char     *start_time;    /* start time  */
    uint32_t        multiparty;    /* is a conference call */
    uint32_t        emergency;     /* is an emergency call */
} tel_call_t;


/**
 * Types of events notified from telephony middleware, needed in order to
 * synchronize call information with Murphy DB
 */
typedef enum {
    TEL_CALL_PURGE = 0,            /* remove all call data for a modem */
    TEL_CALL_LISTED,               /* call added on resync of call list */
    TEL_CALL_ADDED,                /* new call appeared */
    TEL_CALL_REMOVED,              /* call got removed */
    TEL_CALL_CHANGED               /* call state has changed */
} tel_event_t;


/******************************************************************************
 * Telephony listener, provides telephony events and corresponding data.
 */
typedef void (*tel_watcher_t)(int event, tel_call_t *call, void *data);

int tel_start_listeners(mrp_mainloop_t *ml);

int tel_exit_listeners(void);

void tel_watcher(int event, tel_call_t *call, void *data);


/******************************************************************************
 * Utility macros.
 */

#define FAIL_IF(cond, retval, err, args...) \
    do { if (MRP_UNLIKELY(cond)) { \
            if (MRP_LIKELY(err != NULL)) mrp_log_error(err, ## args); \
            return retval; \
       } } while(0)

#define FAIL_IF_NULL(ptr, retval, err, args...) \
    do { if (MRP_UNLIKELY((ptr) == NULL)) { \
            if (MRP_LIKELY(err != NULL)) mrp_log_error(err, ## args); \
            return retval; \
       } } while(0)


#endif /* __MURPHY_TELEPHONY_H__ */
