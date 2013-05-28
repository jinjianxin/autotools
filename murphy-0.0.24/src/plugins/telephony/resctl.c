#include <stdbool.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/resource/client-api.h>

#include "resctl.h"

struct resctl_s {
    mrp_resource_client_t *client;
    mrp_resource_set_t    *set;
    uint32_t               seqno;
    int                    requested;
    int                    granted;
};

static void event_cb(uint32_t reqid, mrp_resource_set_t *rset, void *user_data);

resctl_t *resctl_init(void)
{
    resctl_t              *ctl;
    mrp_resource_client_t *client;
    mrp_resource_set_t    *set;
    const char            *zone = "driver";
    const char            *play = "audio_playback";
    const char            *rec  = "audio_recording";
    const char            *cls  = "phone";
    bool                   ar   = false;
    uint32_t               prio = 1;               /* what is this ? */

    ctl    = NULL;
    client = NULL;
    set    = NULL;

    ctl = mrp_allocz(sizeof(*ctl));

    if (ctl == NULL) {
        mrp_log_error("Failed to initialize telephony resource control.");
        goto fail;
    }

    client = mrp_resource_client_create("telephony", ctl);

    if (client == NULL) {
        mrp_log_error("Failed to create telephony resource client.");
        goto fail;
    }

    set = mrp_resource_set_create(client, ar, prio, event_cb, ctl);

    if (set == NULL) {
        mrp_log_error("Failed to create telephony resource set.");
        goto fail;
    }

    if (mrp_resource_set_add_resource(set, play, false, NULL, true) < 0 ||
        mrp_resource_set_add_resource(set, rec , false, NULL, true) < 0) {
        mrp_log_error("Failed to initialize telephony resource set.");
        goto fail;
    }

    if (mrp_application_class_add_resource_set(cls, zone, set, 0) != 0) {
        mrp_log_error("Failed to assign telephony resource set with class.");
        goto fail;
    }

    ctl->client = client;
    ctl->set    = set;
    ctl->seqno  = 1;

    return ctl;

 fail:
    if (set != NULL)
        mrp_resource_set_destroy(set);

    if (client != NULL)
        mrp_resource_client_destroy(client);

    mrp_free(ctl);

    return NULL;
}


void resctl_exit(resctl_t *ctl)
{
    if (ctl != NULL) {
        if (ctl->set != NULL)
            mrp_resource_set_destroy(ctl->set);

        if (ctl->client != NULL)
            mrp_resource_client_destroy(ctl->client);

        mrp_free(ctl);
    }
}


void resctl_acquire(resctl_t *ctl)
{
    if (ctl != NULL && !ctl->granted && !ctl->requested) {
        mrp_log_info("acquiring telephony resources");
        mrp_resource_set_acquire(ctl->set, ctl->seqno++);
        ctl->requested = TRUE;
    }
}


void resctl_release(resctl_t *ctl)
{
    if (ctl != NULL && (ctl->granted || ctl->requested)) {
        mrp_log_info("releasing telephony resources");
        mrp_resource_set_release(ctl->set, ctl->seqno++);
        ctl->requested = FALSE;
    }
}


static void event_cb(uint32_t reqid, mrp_resource_set_t *rset, void *user_data)
{
    resctl_t   *ctl = (resctl_t *)user_data;
    const char *state;

    if (mrp_get_resource_set_state(rset) == mrp_resource_acquire) {
        state = (reqid != 0 ? "acquired" : "got");
        ctl->granted = TRUE;
    }
    else {
        state = (reqid != 0 ? "released" : "lost");
        ctl->granted = FALSE;
    }

    mrp_log_info("telephony has %s audio resources.", state);
}
