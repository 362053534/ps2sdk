/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
*/

#ifndef _PFS_UNICODE_H
#define _PFS_UNICODE_H

extern int pfsUTF8toGBK(char *dst, unsigned int dstlen, const char *src);
extern int pfsGBKtoUTF8(char *dst, unsigned int dstlen, const char *src);

#endif /* _PFS_UNICODE_H */
