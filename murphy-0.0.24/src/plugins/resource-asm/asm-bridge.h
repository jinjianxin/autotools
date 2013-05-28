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

#ifndef MURPHY_ASM_BRIDGE_H
#define MURPHY_ASM_BRIDGE_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include <murphy/common.h>

#define ASM_BRIDGE_LOG_ENVVAR "ASM_BRIDGE_LOGFILE"

#define TAG_LIB_TO_ASM ((uint16_t) 0x100)
#define TAG_ASM_TO_LIB ((uint16_t) 0x101)
#define TAG_ASM_TO_LIB_CB ((uint16_t) 0x102)
#define TAG_LIB_TO_ASM_CB ((uint16_t) 0x103)

/* library requests something */

typedef struct {
    uint32_t   instance_id;
    int32_t    handle;
    uint32_t   request_id;
    uint32_t   sound_event;
    uint32_t   sound_state;
    uint32_t   system_resource;
    uint32_t   n_cookie_bytes;
    uint8_t   *cookie;
} lib_to_asm_t;

/* server reply to library */

typedef struct {
    uint32_t   instance_id;
    int32_t    alloc_handle;
    int32_t    cmd_handle;
    uint32_t   result_sound_command;
    uint32_t   result_sound_state; /* ASM_sound_states_t */
    int32_t    former_sound_event; /* ASM_sound_events_t */
    bool       check_privilege;
} asm_to_lib_t;

/* server sends a preemption command */

typedef struct {
    uint32_t   instance_id;
    int32_t    handle;
    bool       callback_expected;
    uint32_t   sound_command; /* ASM_sound_commands_t */
    uint32_t   event_source; /* ASM_event_sources_t */
} asm_to_lib_cb_t;

/* library tells the state it landed in after preemption */

typedef struct {
    uint32_t   instance_id;
    int32_t    handle;
    uint32_t   cb_result; /* ASM_cb_result_t */
} lib_to_asm_cb_t;



MRP_DATA_DESCRIPTOR(lib_to_asm_descr, TAG_LIB_TO_ASM, lib_to_asm_t,
        MRP_DATA_MEMBER(lib_to_asm_t, instance_id, MRP_MSG_FIELD_UINT32),
        MRP_DATA_MEMBER(lib_to_asm_t, handle, MRP_MSG_FIELD_INT32),
        MRP_DATA_MEMBER(lib_to_asm_t, request_id, MRP_MSG_FIELD_UINT32),
        MRP_DATA_MEMBER(lib_to_asm_t, sound_event, MRP_MSG_FIELD_UINT32),
        MRP_DATA_MEMBER(lib_to_asm_t, sound_state, MRP_MSG_FIELD_UINT32),
        MRP_DATA_MEMBER(lib_to_asm_t, system_resource, MRP_MSG_FIELD_UINT32),
        MRP_DATA_MEMBER(lib_to_asm_t, n_cookie_bytes, MRP_MSG_FIELD_UINT32),
        MRP_DATA_ARRAY_COUNT(lib_to_asm_t, cookie, n_cookie_bytes,
                                 MRP_MSG_FIELD_UINT8));

MRP_DATA_DESCRIPTOR(asm_to_lib_descr, TAG_ASM_TO_LIB, asm_to_lib_t,
        MRP_DATA_MEMBER(asm_to_lib_t, instance_id, MRP_MSG_FIELD_UINT32),
        MRP_DATA_MEMBER(asm_to_lib_t, alloc_handle, MRP_MSG_FIELD_INT32),
        MRP_DATA_MEMBER(asm_to_lib_t, cmd_handle, MRP_MSG_FIELD_INT32),
        MRP_DATA_MEMBER(asm_to_lib_t, result_sound_command, MRP_MSG_FIELD_UINT32),
        MRP_DATA_MEMBER(asm_to_lib_t, result_sound_state, MRP_MSG_FIELD_UINT32),
        MRP_DATA_MEMBER(asm_to_lib_t, former_sound_event, MRP_MSG_FIELD_INT32),
        MRP_DATA_MEMBER(asm_to_lib_t, check_privilege, MRP_MSG_FIELD_BOOL));

MRP_DATA_DESCRIPTOR(asm_to_lib_cb_descr, TAG_ASM_TO_LIB_CB, asm_to_lib_cb_t,
        MRP_DATA_MEMBER(asm_to_lib_cb_t, instance_id, MRP_MSG_FIELD_UINT32),
        MRP_DATA_MEMBER(asm_to_lib_cb_t, handle, MRP_MSG_FIELD_INT32),
        MRP_DATA_MEMBER(asm_to_lib_cb_t, callback_expected, MRP_MSG_FIELD_BOOL),
        MRP_DATA_MEMBER(asm_to_lib_cb_t, sound_command, MRP_MSG_FIELD_UINT32),
        MRP_DATA_MEMBER(asm_to_lib_cb_t, event_source, MRP_MSG_FIELD_UINT32));

MRP_DATA_DESCRIPTOR(lib_to_asm_cb_descr, TAG_LIB_TO_ASM_CB, lib_to_asm_cb_t,
        MRP_DATA_MEMBER(lib_to_asm_cb_t, instance_id, MRP_MSG_FIELD_UINT32),
        MRP_DATA_MEMBER(lib_to_asm_cb_t, handle, MRP_MSG_FIELD_INT32),
        MRP_DATA_MEMBER(lib_to_asm_cb_t, cb_result, MRP_MSG_FIELD_UINT32));


#endif
