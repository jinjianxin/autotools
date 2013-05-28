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

#ifndef __MURPHY_TELEPHONY_OFONO_H__
#define __MURPHY_TELEPHONY_OFONO_H__

#include <murphy/common/dbus.h>
#include "telephony.h"


/** oFono object */
typedef struct {
    mrp_dbus_t        *dbus;         /* connection, mainloop, signal handlers */
    mrp_list_hook_t    modems;       /* list of modems */
    int32_t            modem_qry;    /* id of modem query if active */
    tel_watcher_t      notify;       /* the notification callback for Murphy */
}ofono_t;


/** oFono modem object */
typedef struct {
    mrp_list_hook_t    hook;         /* hook to peers in a list of modems */
    ofono_t           *ofono;        /* back pointer to ofono context */
    char              *modem_id;     /* modem object path */
    char              *name;         /* modem name */
    char              *manufacturer; /* modem manufacturer */
    char              *model;        /*       model */
    char              *revision;     /*       revision */
    char              *serial;       /*       serial number */
    char              *type;         /*       type: "hfp", "sap", "hardware" */
    char             **features;     /* list of supported features */
    char             **interfaces;   /* list of supported interfaces */
    int                powered;      /* whether is powered on */
    int                online;       /* whether is online */
    int                lockdown;     /* whether is locked */
    int                emergency;    /* has active emergency call */
    uint32_t           timer;        /* change/query timer */
    mrp_list_hook_t    calls;        /* head of phone call list */
    int32_t            call_qry;     /* call query id, if active */
} ofono_modem_t;


/** cellular calls, extends mrp_telephony call with the rest of the fields  */
typedef struct _call_s {
    mrp_list_hook_t    hook;         /* hook to peers in a list of calls */
    char              *call_id;      /* call path */
    char              *service_id;   /* service/modem path owning the call */
    char              *line_id;      /* phone number */
    char              *name;         /* name identification by network */
    char              *state;        /* "active","held","dialing","alerting",
                                        "incoming","waiting","disconnected" */
    char              *end_reason;   /* "local","remote","network" */
    char              *start_time;   /* start time  */
    uint32_t           multiparty;   /* is a conference call */
    uint32_t           emergency;    /* is an emergency call */

    char              *incoming_line;/* the number dialed by remote */
    char              *info;         /* for calls initiated by SIM Toolkit */
    uint8_t            icon_id;      /* icon identifier */
    bool               remoteheld;   /* is remote holding the call */
    ofono_modem_t     *modem;        /* pointer back to modem */
} ofono_call_t;


/** Create an ofono listener. */
ofono_t *ofono_watch(mrp_mainloop_t *ml, tel_watcher_t notify);

/** Remove an ofono listener. */
void ofono_unwatch(ofono_t *ofono);

#endif /* __MURPHY_TELEPHONY_OFONO_H__ */
