#ifndef _UNICODE_H
#define _UNICODE_H

#define CHINA_SET_MAX 8

/* convert unicode to UTF-8 */
unsigned char *UnicodetoUTF8(int unicode, unsigned char *p);

int UTF8toUnicode(unsigned char *ch, int *unicode);

#endif
