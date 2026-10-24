/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2005, ps2dev - http://www.ps2dev.org
# Licenced under GNU Library General Public License version 2
*/

/**
 * @file
 * audsrv IOP-side upsamplers
 */

#ifndef __UPSAMPLERS_INCLUDED__
#define __UPSAMPLERS_INCLUDED__

/** 插值方式开关(编译时选择)：UP_INTERP_LINEAR 为线性插值(IOP开销小)，UP_INTERP_CUBIC 为三次插值(音质更好，开销约为线性的两倍)。
 *  可在编译参数里用 -DUP_INTERP_MODE=1 覆盖，默认使用线性插值。
 */
#define UP_INTERP_LINEAR 0
#define UP_INTERP_CUBIC  1
#ifndef UP_INTERP_MODE
#define UP_INTERP_MODE UP_INTERP_LINEAR
#endif

/** 插值需要的历史采样数(上一块末尾的几个采样，保证块与块之间连续；按三次插值的需求保存，线性只用最后一个) */
#define UP_HISTORY_SAMPLES 3

typedef struct upsample_t
{
	short *left;
	short *right;
	const unsigned char *src;
	/** 上一块末尾的采样(转成16位后的值)，由插值升采样器维护，使用前须调用 upsampler_reset */
	short history_left[UP_HISTORY_SAMPLES];
	short history_right[UP_HISTORY_SAMPLES];
} upsample_t;

typedef int (*upsampler_t)(struct upsample_t *);

extern upsampler_t find_upsampler(int freq, int bits, int channels);

/** CD播放专用的44100Hz 16位立体声升采样器(不做插值，不使用历史采样) */
extern upsampler_t find_cdda_upsampler(void);

/** 清空升采样器的历史采样，格式改变或重新开始播放时调用 */
extern void upsampler_reset(struct upsample_t *up);

#endif
