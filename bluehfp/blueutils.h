#ifndef BLUE_UTILS_H
#define BLUE_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* 获得短信中心号码*/
char *blue_utils_get_center_number(char *arg);
/* 获得来电号码*/
char *blue_utils_get_ring_number(char *arg);
/*短信原始内容*/
char *blue_utils_get_sms_msg(const char *arg);

char *blue_utils_number_to16(int size);
char *blue_utils_number_to10(char *str);

/*将号码反转*/
char *blue_utils_switch_number(const char *number);
/*获得正常号码*/
char *blue_utils_get_number(const char *number);
/*获得短信时间*/
char *blue_utils_get_msg_time(const char* str);
/*获得短信内容*/
char *blue_utils_get_msg_content(const char *str);
/*将字符串转为int类型*/
unsigned long blue_utils_unicode_int(const char* str);

char *blue_utils_notify_sms(const char *str);

char *blue_utils_get_signal(const char *str);

#endif
