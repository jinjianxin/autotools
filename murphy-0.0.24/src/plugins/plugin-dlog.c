#include <stdlib.h>

#include <dlog/dlog.h>

#include <murphy/common.h>
#include <murphy/core.h>


enum {
    ARG_FORCE,                           /* force TIZEN_DEBUG_LEVEL ? */
    ARG_TAG,                             /* tag to pass to dlog */
};

static const char *log_tag;              /* tag to pass to dlog */


static void logger(void *data, mrp_log_level_t level, const char *file,
                   int line, const char *func, const char *format, va_list ap)
{
    va_list cp;

    MRP_UNUSED(data);
    MRP_UNUSED(file);
    MRP_UNUSED(line);
    MRP_UNUSED(func);

    va_copy(cp, ap);
    switch (level) {
    case MRP_LOG_ERROR:   SLOG_VA(LOG_ERROR, log_tag, format, cp); break;
    case MRP_LOG_WARNING: SLOG_VA(LOG_WARN,  log_tag, format, cp); break;
    case MRP_LOG_INFO:    SLOG_VA(LOG_INFO,  log_tag, format, cp); break;
    case MRP_LOG_DEBUG:   SLOG_VA(LOG_DEBUG, log_tag, format, cp); break;
    default:                                                       break;
    }
    va_end(cp);
}


static int dlogger_init(mrp_plugin_t *plugin)
{
    mrp_plugin_arg_t *args = plugin->args;
    int               force;

    force   = args[ARG_FORCE].bln;
    log_tag = args[ARG_TAG].str;

    if (force)                                     /* Use the Force, Luke ! */
        setenv("TIZEN_DEBUG_LEVEL", "31", TRUE);

    if (mrp_log_register_target("dlog", logger, NULL))
        mrp_log_info("dlog: registered logging target.");
    else
        mrp_log_error("dlog: failed to register logging target.");

    return TRUE;
}


static void dlogger_exit(mrp_plugin_t *plugin)
{
    MRP_UNUSED(plugin);

    mrp_log_unregister_target("dlog");

    return;
}

#define DLOGGER_DESCRIPTION "A dlog logger for Murphy."
#define DLOGGER_HELP        "dlog logger support for Murphy."
#define DLOGGER_VERSION     MRP_VERSION_INT(0, 0, 1)
#define DLOGGER_AUTHORS     "Krisztian Litkey <krisztian.litkey@intel.com>"

static mrp_plugin_arg_t plugin_args[] = {
    MRP_PLUGIN_ARGIDX(ARG_FORCE, BOOL  , "force", FALSE   ),
    MRP_PLUGIN_ARGIDX(ARG_TAG  , STRING, "tag"  , "MURPHY"),
};

MURPHY_REGISTER_PLUGIN("dlog",
                       DLOGGER_VERSION, DLOGGER_DESCRIPTION,
                       DLOGGER_AUTHORS, DLOGGER_HELP, MRP_SINGLETON,
                       dlogger_init, dlogger_exit,
                       plugin_args, MRP_ARRAY_SIZE(plugin_args),
                       NULL, 0, NULL, 0, NULL);
