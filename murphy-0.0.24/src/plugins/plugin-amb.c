/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Intel Corporation nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
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


#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>

#include <murphy/common.h>
#include <murphy/core.h>
#include <murphy/common/dbus.h>
#include <murphy/common/process.h>

#include <murphy-db/mdb.h>
#include <murphy-db/mqi.h>
#include <murphy-db/mql.h>

#include <murphy/core/lua-utils/object.h>
#include <murphy/core/lua-bindings/murphy.h>
#include <murphy/core/lua-utils/funcbridge.h>
#include <murphy/core/lua-decision/mdb.h>
#include <murphy/core/lua-decision/element.h>

enum {
    ARG_AMB_DBUS_ADDRESS,
    ARG_AMB_CONFIG_FILE,
    ARG_AMB_ID,
    ARG_AMB_TPORT_ADDRESS
};

enum amb_type {
    amb_string = 's',
    amb_bool   = 'b',
    amb_int32  = 'i',
    amb_int16  = 'n',
    amb_uint32 = 'u',
    amb_uint16 = 'q',
    amb_byte   = 'y',
    amb_double = 'd',
};

#define AMB_NAME                "name"
#define AMB_HANDLER             "handler"
#define AMB_DBUS_DATA           "dbus_data"
#define AMB_OBJECT              "obj"
#define AMB_INTERFACE           "interface"
#define AMB_MEMBER              "property"
#define AMB_SIGMEMBER           "notification"
#define AMB_SIGNATURE           "signature"
#define AMB_BASIC_TABLE_NAME    "basic_table_name"
#define AMB_OUTPUTS             "outputs"


/*
signal sender=:1.117 -> dest=(null destination) serial=961
path=/org/automotive/runningstatus/vehicleSpeed;
interface=org.automotive.vehicleSpeed;
member=VehicleSpeedChanged
   variant       int32 0


dbus-send --system --print-reply --dest=org.automotive.message.broker \
        /org/automotive/runningstatus/vehicleSpeed \
        org.freedesktop.DBus.Properties.Get \
        string:'org.automotive.vehicleSpeed' string:'VehicleSpeed'

method return sender=:1.69 -> dest=:1.91 reply_serial=2
   variant       int32 0
*/

typedef struct {
    char *name;
    int type;
    union {
        int32_t i;
        uint32_t u;
        double f;
        char *s;
    } value;
    bool initialized;
} dbus_basic_property_t;

typedef void (*property_updated_cb_t)(dbus_basic_property_t *property,
        void *user_data);

typedef struct {
    struct {
        const char *obj;
        const char *iface;
        const char *name;
        const char *signame;
        const char *signature;
    } dbus_data;
    const char *name;
    const char *basic_table_name;
    int handler_ref;
    int outputs_ref;
} lua_amb_property_t;

typedef struct {
    mrp_dbus_t *dbus;
    const char *amb_addr;
    const char *config_file;
    const char *amb_id;
    const char *tport_addr;
    lua_State *L;
    mrp_list_hook_t lua_properties;

    mrp_process_state_t amb_state;

    mrp_transport_t *lt;
    mrp_transport_t *t;
} data_t;

typedef struct {
    mqi_column_def_t defs[4];

    mql_statement_t *update_operation;
    mqi_data_type_t type;
    mqi_handle_t table;
} basic_table_data_t;

typedef struct {
    dbus_basic_property_t prop;

    property_updated_cb_t cb;
    void *user_data;

    lua_amb_property_t *lua_prop;

    /* for basic tables that we manage ourselves */
    basic_table_data_t *tdata;

    mrp_list_hook_t hook;
    data_t *ctx;
} dbus_property_watch_t;

static data_t *global_ctx = NULL;

static basic_table_data_t *create_basic_property_table(const char *table_name,
        const char *member, int type);

static int subscribe_property(data_t *ctx, dbus_property_watch_t *w);

static void basic_property_updated(dbus_basic_property_t *prop, void *userdata);

static int create_transport(mrp_mainloop_t *ml, data_t *ctx);

/* Lua config */

static int amb_constructor(lua_State *L);
static int amb_setfield(lua_State *L);
static int amb_getfield(lua_State *L);
static void lua_amb_destroy(void *data);

#define PROPERTY_CLASS MRP_LUA_CLASS(amb, property)

MRP_LUA_METHOD_LIST_TABLE (
    amb_methods,          /* methodlist name */
    MRP_LUA_METHOD_CONSTRUCTOR  (amb_constructor)
);

MRP_LUA_METHOD_LIST_TABLE (
    amb_overrides,     /* methodlist name */
    MRP_LUA_OVERRIDE_CALL       (amb_constructor)
    MRP_LUA_OVERRIDE_GETFIELD   (amb_getfield)
    MRP_LUA_OVERRIDE_SETFIELD   (amb_setfield)
);

MRP_LUA_CLASS_DEF (
    amb,                /* main class name */
    property,           /* constructor name */
    lua_amb_property_t, /* userdata type */
    lua_amb_destroy,    /* userdata destructor */
    amb_methods,        /* class methods */
    amb_overrides       /* class overrides */
);



static void lua_amb_destroy(void *data)
{
    lua_amb_property_t *prop = (lua_amb_property_t *)data;

    MRP_LUA_ENTER;

    mrp_log_info("> lua_amb_destroy");

    MRP_UNUSED(prop);

    MRP_LUA_LEAVE_NOARG;
}


static void destroy_prop(data_t *ctx, dbus_property_watch_t *w)
{
    /* TODO */

    MRP_UNUSED(ctx);
    MRP_UNUSED(w);
}


static int amb_constructor(lua_State *L)
{
    lua_amb_property_t *prop;
    size_t field_name_len;
    const char *field_name;
    data_t *ctx = global_ctx;
    dbus_property_watch_t *w = NULL;

    MRP_LUA_ENTER;

    mrp_debug("> amb_constructor, stack size: %d", lua_gettop(L));

    prop = (lua_amb_property_t *)
            mrp_lua_create_object(L, PROPERTY_CLASS, NULL, 0);

    prop->handler_ref = LUA_NOREF;
    prop->outputs_ref = LUA_NOREF;

    MRP_LUA_FOREACH_FIELD(L, 2, field_name, field_name_len) {
        char buf[field_name_len+1];

        strncpy(buf, field_name, field_name_len);
        buf[field_name_len] = '\0';

        mrp_log_info("field name: %s", buf);

        if (strncmp(field_name, "dbus_data", field_name_len) == 0) {

            luaL_checktype(L, -1, LUA_TTABLE);

            lua_pushnil(L);

            while (lua_next(L, -2)) {

                const char *key;
                const char *value;

                luaL_checktype(L, -2, LUA_TSTRING);
                luaL_checktype(L, -1, LUA_TSTRING);

                key = lua_tostring(L, -2);
                value = lua_tostring(L, -1);

                mrp_log_info("%s -> %s", key, value);

                if (!key || !value)
                    goto error;

                if (strcmp(key, "signature") == 0) {
                    prop->dbus_data.signature = mrp_strdup(value);
                }
                else if (strcmp(key, "property") == 0) {
                    prop->dbus_data.name = mrp_strdup(value);
                }
                else if (strcmp(key, "notification") == 0) {
                    prop->dbus_data.signame = mrp_strdup(value);
                }
                else if (strcmp(key, "obj") == 0) {
                    prop->dbus_data.obj = mrp_strdup(value);
                }
                else if (strcmp(key, "interface") == 0) {
                    prop->dbus_data.iface = mrp_strdup(value);
                }
                else {
                    goto error;
                }

                lua_pop(L, 1);
            }

            if (prop->dbus_data.signame == NULL)
                prop->dbus_data.signame = mrp_strdup(prop->dbus_data.name);

            /* check that we have all necessary data */
            if (prop->dbus_data.signature == NULL ||
                prop->dbus_data.iface == NULL ||
                prop->dbus_data.obj == NULL ||
                prop->dbus_data.name == NULL ||
                prop->dbus_data.signame == NULL) {
                goto error;
            }
        }
        else if (strncmp(field_name, "handler", field_name_len) == 0) {
            luaL_checktype(L, -1, LUA_TFUNCTION);
            prop->handler_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            lua_pushnil(L); /* need two items on the stack */
        }
        else if (strncmp(field_name, AMB_NAME, field_name_len) == 0) {
            luaL_checktype(L, -1, LUA_TSTRING);
            prop->name = mrp_strdup(lua_tostring(L, -1));
            mrp_lua_set_object_name(L, PROPERTY_CLASS, prop->name);
        }
        else if (strncmp(field_name, "basic_table_name", field_name_len) == 0) {
            luaL_checktype(L, -1, LUA_TSTRING);
            prop->basic_table_name = mrp_strdup(lua_tostring(L, -1));
        }
        else if (strncmp(field_name, AMB_OUTPUTS, field_name_len) == 0) {
            prop->outputs_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            lua_pushnil(L); /* need two items on the stack */
        }
    }

    if (!prop->name)
        goto error;

    if (prop->handler_ref == LUA_NOREF && !prop->basic_table_name)
        goto error;

    w = (dbus_property_watch_t *) mrp_allocz(sizeof(dbus_property_watch_t));

    if (!w)
        goto error;

    w->ctx = ctx;
    w->lua_prop = prop;
    w->prop.initialized = FALSE;
    w->prop.type = DBUS_TYPE_INVALID;
    w->prop.name = mrp_strdup(w->lua_prop->dbus_data.name);

    if (!w->prop.name)
        goto error;

    if (prop->handler_ref == LUA_NOREF) {
        basic_table_data_t *tdata;

        w->prop.type = w->lua_prop->dbus_data.signature[0]; /* FIXME */

        tdata = create_basic_property_table(prop->basic_table_name,
            prop->dbus_data.name, w->prop.type);

        if (!tdata) {
            goto error;
        }

        w->tdata = tdata;

        w->cb = basic_property_updated;
        w->user_data = w;

        /* add_table_data(tdata, ctx); */
        if (subscribe_property(ctx, w)) {
            mrp_log_error("Failed to subscribe to basic property");
            goto error;
        }
    }
    else {
        /* we now have the callback function reference */

        /* TODO: refactor to decouple updating the property (calling the
         * lua handler) from parsing the D-Bus message. Is this possible? */
        if (subscribe_property(ctx, w)) {
            mrp_log_error("Failed to subscribe to basic property");
            goto error;
        }
    }


    mrp_list_init(&w->hook);

    mrp_list_append(&ctx->lua_properties, &w->hook);

    /* TODO: need some mapping? or custom property_watch? */

    /* TODO: put the object to a global table or not? maybe better to just
     * unload them when the plugin is unloaded. */

    mrp_lua_push_object(L, prop);

    MRP_LUA_LEAVE(1);

error:
    /* TODO: delete the allocated data */
    destroy_prop(global_ctx, w);

    mrp_log_error("< amb_constructor ERROR");
    MRP_LUA_LEAVE(0);
}

static int amb_getfield(lua_State *L)
{
    lua_amb_property_t *prop = (lua_amb_property_t *)
            mrp_lua_check_object(L, PROPERTY_CLASS, 1);
    size_t field_name_len;
    const char *field_name = lua_tolstring(L, 2, &field_name_len);

    MRP_LUA_ENTER;

    if (!prop)
        goto error;

    mrp_debug("> amb_getfield");

    if (strncmp(field_name, AMB_NAME, field_name_len) == 0) {
        if (prop->name)
            lua_pushstring(L, prop->name);
        else
            goto error;
    }
    else if (strncmp(field_name, AMB_HANDLER, field_name_len) == 0) {
        if (prop->handler_ref != LUA_NOREF)
            lua_rawgeti(L, LUA_REGISTRYINDEX, prop->handler_ref);
        else
            goto error;
    }
    else if (strncmp(field_name, AMB_DBUS_DATA, field_name_len) == 0) {
        lua_newtable(L);

        lua_pushstring(L, AMB_OBJECT);
        lua_pushstring(L, prop->dbus_data.obj);
        lua_settable(L, -3);

        lua_pushstring(L, AMB_INTERFACE);
        lua_pushstring(L, prop->dbus_data.iface);
        lua_settable(L, -3);

        lua_pushstring(L, AMB_MEMBER);
        lua_pushstring(L, prop->dbus_data.name);
        lua_settable(L, -3);

        lua_pushstring(L, AMB_SIGMEMBER);
        lua_pushstring(L, prop->dbus_data.signame);
        lua_settable(L, -3);

        lua_pushstring(L, "signature");
        lua_pushstring(L, prop->dbus_data.signature);
        lua_settable(L, -3);
    }
    else if (strncmp(field_name, "basic_table_name", field_name_len) == 0) {
        if (prop->basic_table_name)
            lua_pushstring(L, prop->basic_table_name);
        else
            goto error;
    }
    else if (strncmp(field_name, AMB_OUTPUTS, field_name_len) == 0) {
        if (prop->outputs_ref != LUA_NOREF)
            lua_rawgeti(L, LUA_REGISTRYINDEX, prop->outputs_ref);
        else
            goto error;
    }
    else {
        goto error;
    }

    MRP_LUA_LEAVE(1);

error:
    lua_pushnil(L);
    MRP_LUA_LEAVE(1);
}

static int amb_setfield(lua_State *L)
{
    MRP_LUA_ENTER;

    MRP_UNUSED(L);

    mrp_debug("> amb_setfield");

    MRP_LUA_LEAVE(0);
}

/* lua config end */

static bool parse_elementary_value(lua_State *L,
        DBusMessageIter *iter, dbus_property_watch_t *w)
{
    dbus_int32_t i32_val;
    dbus_int32_t i16_val;
    dbus_uint32_t u32_val;
    dbus_uint16_t u16_val;
    uint8_t byte_val;
    dbus_bool_t b_val;
    double d_val;
    char *s_val;

    char sig;

    MRP_UNUSED(w);

    if (!iter)
        goto error;

    sig = dbus_message_iter_get_arg_type(iter);

    switch (sig) {
        case DBUS_TYPE_INT32:
            dbus_message_iter_get_basic(iter, &i32_val);
            lua_pushinteger(L, i32_val);
            break;
        case DBUS_TYPE_INT16:
            dbus_message_iter_get_basic(iter, &i16_val);
            lua_pushinteger(L, i16_val);
            break;
        case DBUS_TYPE_UINT32:
            dbus_message_iter_get_basic(iter, &u32_val);
            lua_pushinteger(L, u32_val);
            break;
        case DBUS_TYPE_UINT16:
            dbus_message_iter_get_basic(iter, &u16_val);
            lua_pushinteger(L, u16_val);
            break;
        case DBUS_TYPE_BOOLEAN:
            dbus_message_iter_get_basic(iter, &b_val);
            lua_pushboolean(L, b_val == TRUE);
            break;
        case DBUS_TYPE_BYTE:
            dbus_message_iter_get_basic(iter, &byte_val);
            lua_pushinteger(L, byte_val);
            break;
        case DBUS_TYPE_DOUBLE:
            dbus_message_iter_get_basic(iter, &d_val);
            lua_pushnumber(L, d_val);
            break;
        case DBUS_TYPE_STRING:
            dbus_message_iter_get_basic(iter, &s_val);
            lua_pushstring(L, s_val);
            break;
        default:
            mrp_log_info("> parse_elementary_value: unknown type");
            goto error;
    }

    return TRUE;

error:
    return FALSE;
}

static bool parse_value(lua_State *L, DBusMessageIter *iter,
        dbus_property_watch_t *w);

static bool parse_struct(lua_State *L,
        DBusMessageIter *iter, dbus_property_watch_t *w)
{
    int i = 1;
    DBusMessageIter new_iter;

    if (!iter)
        return FALSE;

    /* initialize the table */
    lua_newtable(L);

    dbus_message_iter_recurse(iter, &new_iter);

    while (dbus_message_iter_get_arg_type(&new_iter) != DBUS_TYPE_INVALID) {

        /* struct "index" */
        lua_pushinteger(L, i++);

        parse_value(L, &new_iter, w);
        dbus_message_iter_next(&new_iter);

        /* put the values to the table */
        lua_settable(L, -3);
    }

    return TRUE;
}


static bool parse_dict_entry(lua_State *L,
        DBusMessageIter *iter, dbus_property_watch_t *w)
{
    DBusMessageIter new_iter;

    if (!iter)
        return FALSE;

    dbus_message_iter_recurse(iter, &new_iter);

    while (dbus_message_iter_get_arg_type(&new_iter) != DBUS_TYPE_INVALID) {

        /* key must be elementary, value can be anything */

        parse_elementary_value(L, &new_iter, w);
        dbus_message_iter_next(&new_iter);

        parse_value(L, &new_iter, w);
        dbus_message_iter_next(&new_iter);

        /* put the values to the table */
        lua_settable(L, -3);
    }

    return TRUE;
}

static bool parse_array(lua_State *L,
        DBusMessageIter *iter, dbus_property_watch_t *w)
{
    DBusMessageIter new_iter;
    int element_type;

    if (!iter)
        return FALSE;

    /* the lua array */
    lua_newtable(L);

    element_type = dbus_message_iter_get_element_type(iter);

    dbus_message_iter_recurse(iter, &new_iter);

    /* the problem: if the value inside array is a dict entry, the
     * indexing of elements need to be done with dict keys instead
     * of numbers. */

    if (element_type == DBUS_TYPE_DICT_ENTRY) {
        while (dbus_message_iter_get_arg_type(&new_iter)
            != DBUS_TYPE_INVALID) {

            parse_dict_entry(L, &new_iter, w);
            dbus_message_iter_next(&new_iter);
        }
    }

    else {
        int i = 1;

        while (dbus_message_iter_get_arg_type(&new_iter)
            != DBUS_TYPE_INVALID) {

            /* array index */
            lua_pushinteger(L, i++);

            parse_value(L, &new_iter, w);
            dbus_message_iter_next(&new_iter);

            /* put the values to the table */
            lua_settable(L, -3);
        }
    }

    return TRUE;
}

static bool parse_value(lua_State *L, DBusMessageIter *iter,
        dbus_property_watch_t *w)
{
    char curr;

    if (!iter)
        return FALSE;

    curr = dbus_message_iter_get_arg_type(iter);

    switch (curr) {
        case DBUS_TYPE_BYTE:
        case DBUS_TYPE_BOOLEAN:
        case DBUS_TYPE_INT16:
        case DBUS_TYPE_INT32:
        case DBUS_TYPE_UINT16:
        case DBUS_TYPE_UINT32:
        case DBUS_TYPE_DOUBLE:
        case DBUS_TYPE_STRING:
            return parse_elementary_value(L, iter, w);
        case DBUS_TYPE_ARRAY:
            return parse_array(L, iter, w);
        case DBUS_TYPE_STRUCT:
            return parse_struct(L, iter, w);
        case DBUS_TYPE_DICT_ENTRY:
            goto error; /* these are handled from parse_array */
        case DBUS_TYPE_INVALID:
            return TRUE;
        default:
            break;
    }

error:
    mrp_log_error("failed to parse D-Bus property (sig[i] %c)", curr);
    return FALSE;
}

static void lua_property_handler(DBusMessage *msg, dbus_property_watch_t *w)
{
    DBusMessageIter msg_iter;
    DBusMessageIter variant_iter;
    char *variant_sig = NULL;

    if (!w || !msg)
        goto error;

    if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_ERROR)
        goto error;

    dbus_message_iter_init(msg, &msg_iter);

    if (dbus_message_iter_get_arg_type(&msg_iter) != DBUS_TYPE_VARIANT)
        goto error;

    dbus_message_iter_recurse(&msg_iter, &variant_iter);

    variant_sig = dbus_message_iter_get_signature(&variant_iter);

    if (!variant_sig)
        goto error;

    /*
    mrp_log_info("iter sig: %s, expected: %s",
            variant_sig, w->lua_prop->dbus_data.signature);
    */

    /* check if we got what we were expecting */
    if (strcmp(variant_sig, w->lua_prop->dbus_data.signature) != 0)
        goto error;

    if (w->lua_prop->handler_ref == LUA_NOREF)
        goto error;

    /* load the function pointer to the stack */
    lua_rawgeti(w->ctx->L, LUA_REGISTRYINDEX, w->lua_prop->handler_ref);

    /* "self" parameter */
    mrp_lua_push_object(w->ctx->L, w->lua_prop);

    /* parse values to the stack */
    parse_value(w->ctx->L, &variant_iter, w);

    /* call the handler function */
    lua_pcall(w->ctx->L, 2, 0, 0);

    dbus_free(variant_sig);

    return;

error:
    if (variant_sig)
        dbus_free(variant_sig);
    mrp_log_error("amb: failed to process an incoming D-Bus message");
}


static void basic_property_handler(DBusMessage *msg, dbus_property_watch_t *w)
{
    DBusMessageIter msg_iter;
    DBusMessageIter variant_iter;

    dbus_int32_t i32_val;
    dbus_int32_t i16_val;
    dbus_uint32_t u32_val;
    dbus_uint16_t u16_val;
    uint8_t byte_val;
    dbus_bool_t b_val;
    double d_val;
    char *s_val;

    if (!w || !msg)
        goto error;

    dbus_message_iter_init(msg, &msg_iter);

    if (dbus_message_iter_get_arg_type(&msg_iter) != DBUS_TYPE_VARIANT)
        goto error;

    dbus_message_iter_recurse(&msg_iter, &variant_iter);

    if (dbus_message_iter_get_arg_type(&variant_iter)
                        != w->prop.type)
        goto error;

    switch (w->prop.type) {
        case DBUS_TYPE_INT32:
            dbus_message_iter_get_basic(&variant_iter, &i32_val);
            w->prop.value.i = i32_val;
            break;
        case DBUS_TYPE_INT16:
            dbus_message_iter_get_basic(&variant_iter, &i16_val);
            w->prop.value.i = i16_val;
            break;
        case DBUS_TYPE_UINT32:
            dbus_message_iter_get_basic(&variant_iter, &u32_val);
            w->prop.value.u = u32_val;
            break;
        case DBUS_TYPE_UINT16:
            dbus_message_iter_get_basic(&variant_iter, &u16_val);
            w->prop.value.u = u16_val;
            break;
        case DBUS_TYPE_BOOLEAN:
            dbus_message_iter_get_basic(&variant_iter, &b_val);
            w->prop.value.u = b_val;
            break;
        case DBUS_TYPE_BYTE:
            dbus_message_iter_get_basic(&variant_iter, &byte_val);
            w->prop.value.u = byte_val;
            break;
        case DBUS_TYPE_DOUBLE:
            dbus_message_iter_get_basic(&variant_iter, &d_val);
            w->prop.value.f = d_val;
            break;
        case DBUS_TYPE_STRING:
            dbus_message_iter_get_basic(&variant_iter, &s_val);
            w->prop.value.s = mrp_strdup(s_val);
            break;
        default:
            goto error;
    }

    if (w->cb)
        w->cb(&w->prop, w->user_data);

    return;

error:
    mrp_log_error("amb: failed to parse property value");
    return;
}

static int property_signal_handler(mrp_dbus_t *dbus, DBusMessage *msg,
        void *data)
{
    dbus_property_watch_t *w = (dbus_property_watch_t *) data;

    MRP_UNUSED(dbus);

    mrp_debug("amb: received property signal");

    if (w->tdata) {
        basic_property_handler(msg, w);
    }
    else {
        lua_property_handler(msg, w);
    }

    return TRUE;
}

static void property_reply_handler(mrp_dbus_t *dbus, DBusMessage *msg,
        void *data)
{
    dbus_property_watch_t *w = (dbus_property_watch_t *) data;

    MRP_UNUSED(dbus);

    mrp_debug("amb: received property method reply");

    if (w->tdata) {
        basic_property_handler(msg, w);
    }
    else {
        lua_property_handler(msg, w);
    }}


static int subscribe_property(data_t *ctx, dbus_property_watch_t *w)
{
    const char *obj = w->lua_prop->dbus_data.obj;
    const char *iface = w->lua_prop->dbus_data.iface;
    const char *name = w->lua_prop->dbus_data.name;
    const char *signame = w->lua_prop->dbus_data.signame;

    mrp_log_info("subscribing to signal '%s.%s' at '%s'",
            iface, signame, obj);

    mrp_dbus_subscribe_signal(ctx->dbus, property_signal_handler, w, NULL,
            obj, iface, signame, NULL);

    /* Ok, now we are listening to property changes. Let's get the initial
     * value. */

    mrp_dbus_call(ctx->dbus,
            ctx->amb_addr, obj,
            "org.freedesktop.DBus.Properties",
            "Get", 3000, property_reply_handler, w,
            DBUS_TYPE_STRING, &iface,
            DBUS_TYPE_STRING, &name,
            DBUS_TYPE_INVALID);

    return 0;
}


static void print_basic_property(dbus_basic_property_t *prop)
{
    switch (prop->type) {
        case DBUS_TYPE_INT32:
        case DBUS_TYPE_INT16:
            mrp_debug("Property %s : %i", prop->name, prop->value.i);
            break;
        case DBUS_TYPE_UINT32:
        case DBUS_TYPE_UINT16:
        case DBUS_TYPE_BOOLEAN:
        case DBUS_TYPE_BYTE:
            mrp_debug("Property %s : %u", prop->name, prop->value.u);
            break;
        case DBUS_TYPE_DOUBLE:
            mrp_debug("Property %s : %f", prop->name, prop->value.f);
            break;
        case DBUS_TYPE_STRING:
            mrp_debug("Property %s : %s", prop->name, prop->value.s);
            break;
        default:
            mrp_log_error("Unknown value in property");
    }
}

static void basic_property_updated(dbus_basic_property_t *prop, void *userdata)
{
    char buf[512];
    int buflen;
    mql_result_t *r;
    dbus_property_watch_t *w = (dbus_property_watch_t *) userdata;
    basic_table_data_t *tdata = w->tdata;
    mqi_handle_t tx;

    mrp_debug("> basic_property_updated");

    print_basic_property(prop);

    tx = mqi_begin_transaction();

    if (!prop->initialized) {

        switch (tdata->type) {
            case mqi_string:
                buflen = snprintf(buf, 512, "INSERT INTO %s VALUES (1, '%s', %s)",
                    w->lua_prop->basic_table_name, prop->name, prop->value.s);
                break;
            case mqi_integer:
                buflen = snprintf(buf, 512, "INSERT INTO %s VALUES (1, '%s', %d)",
                    w->lua_prop->basic_table_name, prop->name, prop->value.i);
                break;
            case mqi_unsignd:
                buflen = snprintf(buf, 512, "INSERT INTO %s VALUES (1, '%s', %u)",
                    w->lua_prop->basic_table_name, prop->name, prop->value.u);
                break;
            case mqi_floating:
                buflen = snprintf(buf, 512, "INSERT INTO %s VALUES (1, '%s', %f)",
                    w->lua_prop->basic_table_name, prop->name, prop->value.f);
                break;
            default:
                goto end;
        }

        if (buflen <= 0 || buflen == 512) {
            goto end;
        }

        r = mql_exec_string(mql_result_string, buf);

        prop->initialized = TRUE;
    }
    else {
        int ret;

        switch (tdata->type) {
            case mqi_string:
                ret = mql_bind_value(tdata->update_operation, 1, tdata->type,
                        prop->value.s);
                break;
            case mqi_integer:
                ret = mql_bind_value(tdata->update_operation, 1, tdata->type,
                        prop->value.i);
                break;
            case mqi_unsignd:
                ret = mql_bind_value(tdata->update_operation, 1, tdata->type,
                        prop->value.u);
                break;
            case mqi_floating:
                ret = mql_bind_value(tdata->update_operation, 1, tdata->type,
                        prop->value.f);
                break;
            default:
                goto end;
        }

        if (ret < 0) {
            mrp_log_error("failed to bind value to update operation");
            goto end;
        }

        r = mql_exec_statement(mql_result_string, tdata->update_operation);
    }

    mrp_debug("amb: %s", mql_result_is_success(r) ? "updated database" :
              mql_result_error_get_message(r));

    mql_result_free(r);

end:
    mqi_commit_transaction(tx);
}

static void delete_basic_table_data(basic_table_data_t *tdata)
{
    if (!tdata)
        return;

    if (tdata->update_operation)
        mql_statement_free(tdata->update_operation);

    if (tdata->table)
        mqi_drop_table(tdata->table);

    mrp_free(tdata);
}

static basic_table_data_t *create_basic_property_table(const char *table_name,
        const char *member, int type)
{
    char buf[512];
    char *update_format;
    /* char *insert_format; */
    basic_table_data_t *tdata = NULL;
    int ret;

    if (strlen(member) > 64)
        goto error;

    tdata = (basic_table_data_t *) mrp_allocz(sizeof(basic_table_data_t));

    if (!tdata)
        goto error;

    switch (type) {
        case DBUS_TYPE_INT32:
        case DBUS_TYPE_INT16:
            tdata->type = mqi_integer;
            update_format = "%d";
            /* insert_format = "%d"; */
            break;
        case DBUS_TYPE_UINT32:
        case DBUS_TYPE_UINT16:
        case DBUS_TYPE_BOOLEAN:
        case DBUS_TYPE_BYTE:
            tdata->type = mqi_unsignd;
            update_format = "%u";
            /* insert_format = "%u"; */
            break;
        case DBUS_TYPE_DOUBLE:
            tdata->type = mqi_floating;
            update_format = "%f";
            /* insert_format = "%f"; */
            break;
        case DBUS_TYPE_STRING:
            tdata->type = mqi_varchar;
            update_format = "%s";
            /* insert_format = "'%s'"; */
            break;
        default:
            mrp_log_error("unknown type %d", type);
            goto error;
    }

    tdata->defs[0].name = "id";
    tdata->defs[0].type = mqi_unsignd;
    tdata->defs[0].length = 0;
    tdata->defs[0].flags = 0;

    tdata->defs[1].name = "key";
    tdata->defs[1].type = mqi_varchar;
    tdata->defs[1].length = 64;
    tdata->defs[1].flags = 0;

    tdata->defs[2].name = "value";
    tdata->defs[2].type = tdata->type;
    tdata->defs[2].length = (tdata->type == mqi_varchar) ? 128 : 0;
    tdata->defs[2].flags = 0;

    memset(&tdata->defs[3], 0, sizeof(tdata->defs[3]));

    tdata->table = MQI_CREATE_TABLE((char *) table_name, MQI_TEMPORARY,
            tdata->defs, NULL);

    if (!tdata->table) {
        mrp_log_error("creating table '%s' failed", table_name);
        goto error;
    }

    ret = snprintf(buf, 512, "UPDATE %s SET value = %s where id = 1",
            table_name, update_format);

    if (ret <= 0 || ret == 512) {
        goto error;
    }

    tdata->update_operation = mql_precompile(buf);

    if (!tdata->update_operation) {
        mrp_log_error("buggy buf: '%s'", buf);
        goto error;
    }

    mrp_log_info("amb: compiled update statement '%s'", buf);

    return tdata;

error:
    mrp_log_error("amb: failed to create table %s", table_name);
    delete_basic_table_data(tdata);
    return NULL;
}

static int load_config(lua_State *L, const char *path)
{
    if (!luaL_loadfile(L, path) && !lua_pcall(L, 0, 0, 0))
        return TRUE;
    else {
        mrp_log_error("plugin-lua: failed to load config file %s.", path);
        mrp_log_error("%s", lua_tostring(L, -1));
        lua_settop(L, 0);

        return FALSE;
    }
}

static void amb_watch(const char *id, mrp_process_state_t state, void *data)
{
    data_t *ctx = (data_t *) data;

    if (strcmp(id, ctx->amb_id) != 0)
        return;

    mrp_log_info("ambd state changed to %s",
            state == MRP_PROCESS_STATE_READY ? "ready" : "not ready");


    if (state == MRP_PROCESS_STATE_NOT_READY &&
            ctx->amb_state == MRP_PROCESS_STATE_READY) {
        mrp_log_error("lost connection to ambd");
    }

    ctx->amb_state = state;
}

/* functions for handling updating the AMB properties */

static int update_amb_property(char *name, enum amb_type type, void *value,
        data_t *ctx)
{
    int ret = -1;
    mrp_msg_t *msg = mrp_msg_create(
            MRP_MSG_TAG_STRING(1, name),
            MRP_MSG_FIELD_END);

    if (!msg)
        goto end;

    if (!ctx->t)
        goto end;

    switch(type) {
        case amb_string:
            mrp_msg_append(msg, 2, MRP_MSG_FIELD_STRING, value);
            break;
        case amb_bool:
            mrp_msg_append(msg, 2, MRP_MSG_FIELD_BOOL, *(bool *)value);
            break;
        case amb_int32:
            mrp_msg_append(msg, 2, MRP_MSG_FIELD_INT32, *(int32_t *)value);
            break;
        case amb_int16:
            mrp_msg_append(msg, 2, MRP_MSG_FIELD_INT16, *(int16_t *)value);
            break;
        case amb_uint32:
            mrp_msg_append(msg, 2, MRP_MSG_FIELD_UINT32, *(uint32_t *)value);
            break;
        case amb_uint16:
            mrp_msg_append(msg, 2, MRP_MSG_FIELD_UINT16, *(uint16_t *)value);
            break;
        case amb_byte:
            mrp_msg_append(msg, 2, MRP_MSG_FIELD_UINT8, *(uint8_t *)value);
            break;
        case amb_double:
            mrp_msg_append(msg, 2, MRP_MSG_FIELD_DOUBLE, *(double *)value);
            break;
    }

    if (!mrp_transport_send(ctx->t, msg))
        goto end;

    mrp_log_info("Sent message to AMB");

    ret = 0;

end:
    if (msg)
        mrp_msg_unref(msg);

    return ret;
}


static bool initiate_func(lua_State *L, void *data,
                    const char *signature, mrp_funcbridge_value_t *args,
                    char  *ret_type, mrp_funcbridge_value_t *ret_val)
{
    MRP_UNUSED(L);
    MRP_UNUSED(args);
    MRP_UNUSED(data);

    if (!signature || signature[0] != 'o') {
        return false;
    }

    *ret_type = MRP_FUNCBRIDGE_BOOLEAN;
    ret_val->boolean = true;

    return true;
}


static bool update_func(lua_State *L, void *data,
                    const char *signature, mrp_funcbridge_value_t *args,
                    char  *ret_type, mrp_funcbridge_value_t *ret_val)
{
    mrp_lua_sink_t *sink;
    const char *type, *property;

    const char *s_val;
    int32_t i_val;
    uint32_t u_val;
    double d_val;

    int ret = -1;

    MRP_UNUSED(L);

    if (!signature || signature[0] != 'o') {
        return false;
    }

    sink = (mrp_lua_sink_t *) args[0].pointer;

    property = mrp_lua_sink_get_property(sink);

    if (!property || strlen(property) == 0) {
        goto error;
    }

    /* ok, for now we only support updates of basic values */

    type = mrp_lua_sink_get_type(sink);

    if (!type || strlen(type) != 1) {
        goto error;
    }

    switch (type[0]) {
        case amb_double:
            d_val = mrp_lua_sink_get_floating(sink,0,0,0);
            ret = update_amb_property((char *) property,
                    (enum amb_type) type[0], &d_val, (data_t *) data);
            break;
        case amb_int16:
        case amb_int32:
            i_val = mrp_lua_sink_get_integer(sink,0,0,0);
            ret = update_amb_property((char *) property,
                    (enum amb_type) type[0], &i_val, (data_t *) data);
            break;
        case amb_bool:
        case amb_byte:
        case amb_uint16:
        case amb_uint32:
            u_val = mrp_lua_sink_get_unsigned(sink,0,0,0);
            ret = update_amb_property((char *) property,
                    (enum amb_type) type[0], &u_val, (data_t *) data);
            break;
        case amb_string:
            s_val = mrp_lua_sink_get_string(sink,0,0,0,NULL,0);
            ret = update_amb_property((char *) property,
                    (enum amb_type) type[0], (void *) s_val,
                    (data_t *) data);
            break;
    }

    *ret_type = MRP_FUNCBRIDGE_BOOLEAN;
    ret_val->boolean = true;

    return ret == 0;

error:
    mrp_log_error("AMB: error processing the property change!");

    *ret_type = MRP_FUNCBRIDGE_BOOLEAN;
    ret_val->boolean = false;

    return true;
}


static void recvdatafrom_evt(mrp_transport_t *t, void *data, uint16_t tag,
                     mrp_sockaddr_t *addr, socklen_t addrlen, void *user_data)
{
    /* At the moment we are not receiving anything from AMB through this
     * transport, however that might change */

    MRP_UNUSED(t);
    MRP_UNUSED(data);
    MRP_UNUSED(tag);
    MRP_UNUSED(addr);
    MRP_UNUSED(addrlen);
    MRP_UNUSED(user_data);
}


static void recvdata_evt(mrp_transport_t *t, void *data, uint16_t tag, void *user_data)
{
    recvdatafrom_evt(t, data, tag, NULL, 0, user_data);
}


static void closed_evt(mrp_transport_t *t, int error, void *user_data)
{
    data_t *ctx = (data_t *) user_data;

    MRP_UNUSED(t);
    MRP_UNUSED(error);

    ctx->t = NULL;

    /* open the listening socket again */

    if (!ctx->lt) {
        create_transport(t->ml, ctx);
    }
}

static void connection_evt(mrp_transport_t *lt, void *user_data)
{
    data_t *ctx = (data_t *) user_data;

    mrp_log_info("AMB connection!");

    if (ctx->t) {
        mrp_log_error("Already connected");
    }
    else {
        ctx->t = mrp_transport_accept(lt, ctx, 0);
    }

    /* close the listening socket, since we only have one client */

    mrp_transport_destroy(lt);
    ctx->lt = NULL;
}


static int create_transport(mrp_mainloop_t *ml, data_t *ctx)
{
    socklen_t alen;
    mrp_sockaddr_t addr;
    int flags = MRP_TRANSPORT_REUSEADDR | MRP_TRANSPORT_MODE_MSG;
    const char *atype;
    struct stat statbuf;

    static mrp_transport_evt_t evt; /* static members are initialized to zero */

    mrp_log_info("> create_transport");

    evt.closed = closed_evt;
    evt.connection = connection_evt;
    evt.recvdata = recvdata_evt;
    evt.recvdatafrom = recvdatafrom_evt;

    alen = mrp_transport_resolve(NULL, ctx->tport_addr, &addr, sizeof(addr),
            &atype);
    if (alen <= 0) {
        mrp_log_error("failed to resolve address");
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


    ctx->lt = mrp_transport_create(ml, atype, &evt, ctx, flags);
    if (ctx->lt == NULL) {
        mrp_log_error("failed to create transport");
        goto error;
    }

    if (!mrp_transport_bind(ctx->lt, &addr, alen)) {
        mrp_log_error("failed to bind transport to address");
        goto error;
    }

    if (!mrp_transport_listen(ctx->lt, 1)) {
        mrp_log_error("failed to listen on transport");
        goto error;
    }

    return 0;

error:
    if (ctx->lt)
        mrp_transport_destroy(ctx->lt);

    return -1;
}

/* plugin init and deinit */

static int amb_init(mrp_plugin_t *plugin)
{
    data_t *ctx;
    mrp_plugin_arg_t *args = plugin->args;

    ctx = (data_t *) mrp_allocz(sizeof(data_t));

    if (!ctx)
        return FALSE;

    mrp_list_init(&ctx->lua_properties);

    plugin->data = ctx;

    ctx->amb_addr = args[ARG_AMB_DBUS_ADDRESS].str;
    ctx->config_file = args[ARG_AMB_CONFIG_FILE].str;
    ctx->amb_id = args[ARG_AMB_ID].str;
    ctx->tport_addr = args[ARG_AMB_TPORT_ADDRESS].str;

    mrp_log_info("amb dbus address: %s", ctx->amb_addr);
    mrp_log_info("amb config file: %s", ctx->config_file);
    mrp_log_info("amb transport address: %s", ctx->tport_addr);

    ctx->dbus = mrp_dbus_connect(plugin->ctx->ml, "system", NULL);

    if (!ctx->dbus)
        goto error;

    /* initialize transport towards ambd */

    if (create_transport(plugin->ctx->ml, ctx) < 0)
        goto error;

    /* initialize lua support */

    global_ctx = ctx;

    ctx->L = mrp_lua_get_lua_state();

    if (!ctx->L)
        goto error;

    /* functions to handle the direct property updates */

    mrp_funcbridge_create_cfunc(ctx->L, "amb_initiate", "o",
                                initiate_func, (void *)ctx);
    mrp_funcbridge_create_cfunc(ctx->L, "amb_update", "o",
                                update_func, (void *)ctx);

    /* custom class for configuration */

    mrp_lua_create_object_class(ctx->L, MRP_LUA_CLASS(amb, property));

    /* TODO: create here a "manager" lua object and put that to the global
     * lua table? This one then has a pointer to the C context. */

    /* 1. read the configuration file. The configuration must tell
            - target object (/org/automotive/runningstatus/vehicleSpeed)
            - target interface (org.automotive.vehicleSpeed)
            - target member (VehicleSpeed)
            - target type (int32_t)
            - destination table
     */

    if (!load_config(ctx->L, ctx->config_file))
        goto error;

    /* TODO: if loading the config failed, go to error */

    mrp_process_set_state("murphy-amb", MRP_PROCESS_STATE_READY);

    if (mrp_process_set_watch(ctx->amb_id, plugin->ctx->ml, amb_watch, ctx) < 0)
        goto process_watch_failed;

    ctx->amb_state = mrp_process_query_state(ctx->amb_id);

    return TRUE;

process_watch_failed:
    /* let's not quit yet? */
    return TRUE;

error:
    {
        mrp_list_hook_t *p, *n;

        mrp_list_foreach(&ctx->lua_properties, p, n) {
            dbus_property_watch_t *w =
                    mrp_list_entry(p, dbus_property_watch_t, hook);

            destroy_prop(ctx, w);
        }
    }

    if (ctx->dbus) {
        mrp_dbus_unref(ctx->dbus);
        ctx->dbus = NULL;
    }

    if (ctx->t) {
        mrp_transport_destroy(ctx->t);
        ctx->t = NULL;
    }

    mrp_process_remove_watch(ctx->amb_id);

    mrp_free(ctx);

    return FALSE;
}


static void amb_exit(mrp_plugin_t *plugin)
{
    data_t *ctx = (data_t *) plugin->data;
    mrp_list_hook_t *p, *n;

    mrp_process_set_state("murphy-amb", MRP_PROCESS_STATE_NOT_READY);

    /* for all subscribed properties, unsubscribe and free memory */

    mrp_list_foreach(&ctx->lua_properties, p, n) {
        dbus_property_watch_t *w =
                mrp_list_entry(p, dbus_property_watch_t, hook);

        destroy_prop(ctx, w);
    }

    mrp_transport_destroy(ctx->t);
    ctx->t = NULL;

    global_ctx = NULL;

    mrp_free(ctx);
}

#define AMB_DESCRIPTION "A plugin for Automotive Message Broker D-Bus API."
#define AMB_HELP        "Access Automotive Message Broker."
#define AMB_VERSION     MRP_VERSION_INT(0, 0, 1)
#define AMB_AUTHORS     "Ismo Puustinen <ismo.puustinen@intel.com>"

static mrp_plugin_arg_t args[] = {
    MRP_PLUGIN_ARGIDX(ARG_AMB_DBUS_ADDRESS, STRING, "amb_address",
            "org.automotive.message.broker"),
    MRP_PLUGIN_ARGIDX(ARG_AMB_CONFIG_FILE, STRING, "config_file",
            "/etc/murphy/plugins/amb/config.lua"),
    MRP_PLUGIN_ARGIDX(ARG_AMB_ID, STRING, "amb_id", "ambd"),
    MRP_PLUGIN_ARGIDX(ARG_AMB_TPORT_ADDRESS, STRING, "transport_address",
            "unxs:/tmp/murphy/amb"),
};


MURPHY_REGISTER_PLUGIN("amb",
                       AMB_VERSION, AMB_DESCRIPTION,
                       AMB_AUTHORS, AMB_HELP,
                       MRP_SINGLETON, amb_init, amb_exit,
                       args, MRP_ARRAY_SIZE(args),
                       NULL, 0, NULL, 0, NULL);
