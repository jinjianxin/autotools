#ifndef __MURPHY_TELEPHONY_RESCTL_H__
#define __MURPHY_TELEPHONY_RESCTL_H__

struct resctl_s;
typedef struct resctl_s resctl_t;

resctl_t *resctl_init(void);
void resctl_exit(resctl_t *ctl);

void resctl_acquire(resctl_t *ctl);
void resctl_release(resctl_t *ctl);

#endif /* __MURPHY_TELEPHONY_RESCTL_H__ */
