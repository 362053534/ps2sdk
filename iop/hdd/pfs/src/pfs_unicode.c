/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
*/

#include <ff.h>

#include "pfs_unicode.h"

int pfsUTF8toGBK(char *dst, unsigned int dstlen, const char *src)
{
    const unsigned char *p = (const unsigned char *)src;
    unsigned int i = 0;

    if (dst == NULL || src == NULL || dstlen == 0)
        return -1;

    while (*p != '\0') {
        unsigned int uni;
        WCHAR oem;

        if (p[0] < 0x80) {
            uni = *p++;
        } else if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
            uni = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
            if (uni < 0x80)
                return -1;
            p += 2;
        } else if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
            uni = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
            if (uni < 0x800 || (uni >= 0xD800 && uni <= 0xDFFF))
                return -1;
            p += 3;
        } else {
            return -1;
        }

        if (uni < 0x80) {
            if (i + 1 >= dstlen)
                return -1;
            dst[i++] = (char)uni;
        } else {
            oem = ff_uni2oem(uni, FF_CODE_PAGE);
            if (oem == 0 || i + (oem > 0xFF ? 2 : 1) >= dstlen)
                return -1;
            if (oem > 0xFF)
                dst[i++] = (char)(oem >> 8);
            dst[i++] = (char)oem;
        }
    }

    dst[i] = '\0';
    return 0;
}

int pfsGBKtoUTF8(char *dst, unsigned int dstlen, const char *src)
{
    const unsigned char *p = (const unsigned char *)src;
    unsigned int i = 0;

    if (dst == NULL || src == NULL || dstlen == 0)
        return -1;

    while (*p != '\0') {
        unsigned int uni;

        if (p[0] < 0x80) {
            uni = *p++;
        } else {
            if (p[1] == '\0' || (uni = ff_oem2uni((p[0] << 8) | p[1], FF_CODE_PAGE)) == 0)
                return -1;
            p += 2;
        }

        if (uni < 0x80) {
            if (i + 1 >= dstlen)
                return -1;
            dst[i++] = (char)uni;
        } else if (uni < 0x800) {
            if (i + 2 >= dstlen)
                return -1;
            dst[i++] = (char)(0xC0 | (uni >> 6));
            dst[i++] = (char)(0x80 | (uni & 0x3F));
        } else {
            if (i + 3 >= dstlen)
                return -1;
            dst[i++] = (char)(0xE0 | (uni >> 12));
            dst[i++] = (char)(0x80 | ((uni >> 6) & 0x3F));
            dst[i++] = (char)(0x80 | (uni & 0x3F));
        }
    }

    dst[i] = '\0';
    return 0;
}
