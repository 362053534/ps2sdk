# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.

SUBDIRS = \
	ahx \
	audsrv \
	clearspu \
	libsd \
	libsnd2 \
	libspu2 \
	ps2snd \
	rspu2drv

# rspu2drv 必须等待两个静态库完成，避免并行构建重复进入同一目录。
all-rspu2drv: all-libsnd2 all-libspu2

include $(PS2SDKSRC)/Defs.make
include $(PS2SDKSRC)/Rules.make
