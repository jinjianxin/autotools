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
#include <sys/types.h>

#include <murphy/common.h>
#include <murphy/core.h>
#include "murphy/plugins/telephony/ofono.h"
#include "murphy/plugins/telephony/telephony.h"


mrp_plugin_t *telephony_plugin;


static int telephony_init(mrp_plugin_t *plugin)
{
    mrp_context_t *ctx = plugin->ctx;

    mrp_log_info("%s() called...", __FUNCTION__);

    if (ctx != NULL) {
        /* use the mainloop from the plugin context */
        return tel_start_listeners(ctx->ml);
    }
    return FALSE;
}


static void telephony_exit(mrp_plugin_t *plugin)
{
    MRP_UNUSED(plugin);

    mrp_log_info("%s() called...", __FUNCTION__);

    tel_exit_listeners();
}


#define TELEPHONY_VERSION MRP_VERSION_INT(0, 0, 1)

#define TELEPHONY_DESCRIPTION "A telephony plugin for Murphy."

#define TELEPHONY_AUTHORS "Zoltan Kis <zoltan.kis@intel.com>"

#define TELEPHONY_HELP \
    "The telephony plugin follows ofono DBUS activity" \
    "and updates Murphy database with telephony calls information"

MRP_REGISTER_CORE_PLUGIN("telephony", \
        TELEPHONY_VERSION, TELEPHONY_DESCRIPTION, \
        TELEPHONY_AUTHORS, TELEPHONY_HELP, MRP_SINGLETON, \
        telephony_init, telephony_exit,
        NULL, 0, /* args     */
        NULL, 0, /* exports  */
        NULL, 0, /* imports  */
        NULL     /* commands */ );
