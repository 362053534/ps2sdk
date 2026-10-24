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

#include <stdio.h>
#include "upsamplers.h"

/** lut for converting 44100 -> 48000，仅CD播放(cdrom.c)使用，不做插值，避免在关中断期间占用过长时间 */
static const short up_44100_lut[512] =
{
	0x0000, 0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006,
	0x0007, 0x0008, 0x0009, 0x000a, 0x000b, 0x000b, 0x000c, 0x000d,
	0x000e, 0x000f, 0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015,
	0x0016, 0x0016, 0x0017, 0x0018, 0x0019, 0x001a, 0x001b, 0x001c,
	0x001d, 0x001e, 0x001f, 0x0020, 0x0021, 0x0021, 0x0022, 0x0023,
	0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b,
	0x002c, 0x002d, 0x002d, 0x002e, 0x002f, 0x0030, 0x0031, 0x0032,
	0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0038, 0x0039,
	0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f, 0x0040, 0x0041,
	0x0042, 0x0043, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048,
	0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f, 0x004f,
	0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
	0x0058, 0x0059, 0x005a, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e,
	0x005f, 0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0065,
	0x0066, 0x0067, 0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d,
	0x006e, 0x006f, 0x0070, 0x0071, 0x0071, 0x0072, 0x0073, 0x0074,
	0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007a, 0x007b, 0x007c,
	0x007c, 0x007d, 0x007e, 0x007f, 0x0080, 0x0081, 0x0082, 0x0083,
	0x0084, 0x0085, 0x0086, 0x0087, 0x0087, 0x0088, 0x0089, 0x008a,
	0x008b, 0x008c, 0x008d, 0x008e, 0x008f, 0x0090, 0x0091, 0x0092,
	0x0093, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097, 0x0098, 0x0099,
	0x009a, 0x009b, 0x009c, 0x009d, 0x009e, 0x009e, 0x009f, 0x00a0,
	0x00a1, 0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7, 0x00a8,
	0x00a9, 0x00a9, 0x00aa, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x00af,
	0x00b0, 0x00b1, 0x00b2, 0x00b3, 0x00b4, 0x00b4, 0x00b5, 0x00b6,
	0x00b7, 0x00b8, 0x00b9, 0x00ba, 0x00bb, 0x00bc, 0x00bd, 0x00be,
	0x00bf, 0x00c0, 0x00c0, 0x00c1, 0x00c2, 0x00c3, 0x00c4, 0x00c5,
	0x00c6, 0x00c7, 0x00c8, 0x00c9, 0x00ca, 0x00cb, 0x00cb, 0x00cc,
	0x00cd, 0x00ce, 0x00cf, 0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4,
	0x00d5, 0x00d6, 0x00d6, 0x00d7, 0x00d8, 0x00d9, 0x00da, 0x00db,
	0x00dc, 0x00dd, 0x00de, 0x00df, 0x00e0, 0x00e1, 0x00e2, 0x00e2,
	0x00e3, 0x00e4, 0x00e5, 0x00e6, 0x00e7, 0x00e8, 0x00e9, 0x00ea,
	0x00eb, 0x00ec, 0x00ed, 0x00ed, 0x00ee, 0x00ef, 0x00f0, 0x00f1,
	0x00f2, 0x00f3, 0x00f4, 0x00f5, 0x00f6, 0x00f7, 0x00f8, 0x00f8,
	0x00f9, 0x00fa, 0x00fb, 0x00fc, 0x00fd, 0x00fe, 0x00ff, 0x0100,
	0x0101, 0x0102, 0x0103, 0x0104, 0x0104, 0x0105, 0x0106, 0x0107,
	0x0108, 0x0109, 0x010a, 0x010b, 0x010c, 0x010d, 0x010e, 0x010f,
	0x010f, 0x0110, 0x0111, 0x0112, 0x0113, 0x0114, 0x0115, 0x0116,
	0x0117, 0x0118, 0x0119, 0x011a, 0x011a, 0x011b, 0x011c, 0x011d,
	0x011e, 0x011f, 0x0120, 0x0121, 0x0122, 0x0123, 0x0124, 0x0125,
	0x0126, 0x0126, 0x0127, 0x0128, 0x0129, 0x012a, 0x012b, 0x012c,
	0x012d, 0x012e, 0x012f, 0x0130, 0x0131, 0x0131, 0x0132, 0x0133,
	0x0134, 0x0135, 0x0136, 0x0137, 0x0138, 0x0139, 0x013a, 0x013b,
	0x013c, 0x013c, 0x013d, 0x013e, 0x013f, 0x0140, 0x0141, 0x0142,
	0x0143, 0x0144, 0x0145, 0x0146, 0x0147, 0x0147, 0x0148, 0x0149,
	0x014a, 0x014b, 0x014c, 0x014d, 0x014e, 0x014f, 0x0150, 0x0151,
	0x0152, 0x0153, 0x0153, 0x0154, 0x0155, 0x0156, 0x0157, 0x0158,
	0x0159, 0x015a, 0x015b, 0x015c, 0x015d, 0x015e, 0x015e, 0x015f,
	0x0160, 0x0161, 0x0162, 0x0163, 0x0164, 0x0165, 0x0166, 0x0167,
	0x0168, 0x0169, 0x0169, 0x016a, 0x016b, 0x016c, 0x016d, 0x016e,
	0x016f, 0x0170, 0x0171, 0x0172, 0x0173, 0x0174, 0x0175, 0x0175,
	0x0176, 0x0177, 0x0178, 0x0179, 0x017a, 0x017b, 0x017c, 0x017d,
	0x017e, 0x017f, 0x0180, 0x0180, 0x0181, 0x0182, 0x0183, 0x0184,
	0x0185, 0x0186, 0x0187, 0x0188, 0x0189, 0x018a, 0x018b, 0x018b,
	0x018c, 0x018d, 0x018e, 0x018f, 0x0190, 0x0191, 0x0192, 0x0193,
	0x0194, 0x0195, 0x0196, 0x0197, 0x0197, 0x0198, 0x0199, 0x019a,
	0x019b, 0x019c, 0x019d, 0x019e, 0x019f, 0x01a0, 0x01a1, 0x01a2,
	0x01a2, 0x01a3, 0x01a4, 0x01a5, 0x01a6, 0x01a7, 0x01a8, 0x01a9,
	0x01aa, 0x01ab, 0x01ac, 0x01ad, 0x01ad, 0x01ae, 0x01af, 0x01b0,
	0x01b1, 0x01b2, 0x01b3, 0x01b4, 0x01b5, 0x01b6, 0x01b7, 0x01b8,
	0x01b9, 0x01b9, 0x01ba, 0x01bb, 0x01bc, 0x01bd, 0x01be, 0x01bf,
	0x01c0, 0x01c1, 0x01c2, 0x01c3, 0x01c4, 0x01c4, 0x01c5, 0x01c6,
	0x01c7, 0x01c8, 0x01c9, 0x01ca, 0x01cb, 0x01cc, 0x01cd, 0x01ce,
	0x01cf, 0x01cf, 0x01d0, 0x01d1, 0x01d2, 0x01d3, 0x01d4, 0x01d5,
};

/** Demux, split interleaved audio to left and right
 * @param mux   source buffer
 * @param left  left channel destination buffer
 * @param right right channel destination buffer
 * @param len   number of samples to copy
*/
static void demux(short *mux, short *left, short *right, int len)
{
	while (len > 0)
	{
		*left++ = *mux++;
		*right++ = *mux++;
		len--;
	}
}

static void up_generic_16_stereo(struct upsample_t *up, const short *lut)
{
	int p;
	short *left = up->left;
	short *right = up->right;
	short *src = (short *)up->src;

	for (p=0; p<512; p++)
	{
		int q;

		q = lut[p] << 1;
		*left++ = src[q + 0];
		*right++ = src[q + 1];
	}
}

/** CD播放专用的44100Hz立体声升采样(取最近点)
 * CD播放线程在关中断期间调用升采样，三次插值耗时较长，所以这里保持原来的做法
 */
static int up_44100_16_stereo_cd(struct upsample_t *up)
{
	up_generic_16_stereo(up, up_44100_lut);
	return 1880; /* (44100 / 48000) * 512 * stereo * 16bit */
}

/** 三次插值升采样
 *
 * 原来的升采样是"取最近点"(查表)，会产生明显的失真。这里改为三次(Catmull-Rom)插值：
 * 每个输出点用相邻4个输入采样计算，权重只与输出点在块内的序号有关，每个采样率只需算一次表。
 *
 * 每块固定消耗 step 个输入帧、产生 512 个输出点(与原来一致，调用方的缓冲区逻辑不变)。
 * 插值需要当前点前1个、后2个采样，所以输出整体延后 UP_CUBIC_DELAY 个输入采样，
 * 并在 upsample_t 里保存上一块末尾的 UP_CUBIC_HISTORY 个采样，块与块之间连续，不会越过输入缓冲区读取。
 */
#define UP_OUTPUT_COUNT   512 /* 每块输出的采样点数 */
#define UP_CUBIC_HISTORY  UP_HISTORY_SAMPLES /* 插值窗口比当前块多需要的历史采样数，定义在头文件 */
#define UP_CUBIC_DELAY    2   /* 输出相对输入的延后(输入采样数)，保证窗口最远只读到本块最后一个采样 */
#define UP_POS_SHIFT      7   /* 位置的16.16定点小数每个输出点的步进 = step << 7 (65536 / 512 = 128) */
#define UP_FRAC_BITS      15  /* 插值小数部分的位数 */
#define UP_FRAC_ONE       (1 << UP_FRAC_BITS)
#define UP_WEIGHT_BITS    14  /* 权重的定点位数(16位采样 x 权重 的和不会超过32位) */
#define UP_STEP_MAX       512 /* 每块最多消耗的输入帧数，大于它的采样率不走插值 */

typedef struct up_cubic_table_t
{
	/** 该表对应的每块消耗帧数，0表示尚未生成 */
	int step;
	/** 每个输出点对应的窗口起点(相对 work 数组) */
	short base[UP_OUTPUT_COUNT];
	/** 每个输出点的4个权重 */
	short weight[UP_OUTPUT_COUNT][4];
} up_cubic_table_t;

static up_cubic_table_t up_tab_8000;  /* 8000Hz   每块85帧  */
static up_cubic_table_t up_tab_11025; /* 11025Hz  每块117帧 */
static up_cubic_table_t up_tab_12000; /* 12000Hz  每块128帧 */
static up_cubic_table_t up_tab_22050; /* 22050Hz  每块235帧 */
static up_cubic_table_t up_tab_24000; /* 24000Hz  每块256帧 */
static up_cubic_table_t up_tab_32000; /* 32000Hz  每块341帧 */
static up_cubic_table_t up_tab_44100; /* 44100Hz  每块470帧 */

/** 插值用的工作缓冲：历史采样 + 本块全部输入帧，左右声道各一份 */
static short up_work_left[UP_CUBIC_HISTORY + UP_STEP_MAX];
static short up_work_right[UP_CUBIC_HISTORY + UP_STEP_MAX];

/** 清空升采样器的历史采样，格式改变或重新开始播放时调用 */
void upsampler_reset(struct upsample_t *up)
{
	int i;

	for (i = 0; i < UP_CUBIC_HISTORY; i++)
	{
		up->history_left[i] = 0;
		up->history_right[i] = 0;
	}
}

/** 生成三次插值的位置表和权重表
 * @param t     要生成的表
 * @param step  每块消耗的输入帧数
 */
static void up_cubic_build(up_cubic_table_t *t, int step)
{
	int i;

	for (i = 0; i < UP_OUTPUT_COUNT; i++)
	{
		int pos, j, f, f2, f3, w0, w1, w2, w3;

		/* 输出点i在输入中的位置(16.16定点)，减去延后量 */
		pos = ((i * step) << UP_POS_SHIFT) - (UP_CUBIC_DELAY << 16);
		j = pos >> 16;
		f = (pos & 0xffff) >> (16 - UP_FRAC_BITS);
		f2 = (f * f) >> UP_FRAC_BITS;
		f3 = (f2 * f) >> UP_FRAC_BITS;

		/* Catmull-Rom 权重(15位定点)，4个权重之和为 1.0 */
		w0 = (-f3 + 2 * f2 - f) >> 1;
		w1 = (3 * f3 - 5 * f2 + 2 * UP_FRAC_ONE) >> 1;
		w2 = (-3 * f3 + 4 * f2 + f) >> 1;
		w3 = (f3 - f2) >> 1;

		/* 窗口起点：j-1，再加上工作缓冲里历史采样占的位置 */
		t->base[i] = (short)(j - 1 + UP_CUBIC_HISTORY);

		/* 降到14位存储(15位的1.0放不进short) */
		t->weight[i][0] = (short)((w0 + 1) >> (UP_FRAC_BITS - UP_WEIGHT_BITS));
		t->weight[i][1] = (short)((w1 + 1) >> (UP_FRAC_BITS - UP_WEIGHT_BITS));
		t->weight[i][2] = (short)((w2 + 1) >> (UP_FRAC_BITS - UP_WEIGHT_BITS));
		t->weight[i][3] = (short)((w3 + 1) >> (UP_FRAC_BITS - UP_WEIGHT_BITS));
	}

	t->step = step;
}

/** 对一个声道做三次插值，输出512个采样点，并保存历史采样
 * @param t        位置和权重表
 * @param step     每块消耗的输入帧数
 * @param work     工作缓冲(前 UP_CUBIC_HISTORY 个是历史采样，后面是本块输入)
 * @param history  保存下一块需要的历史采样
 * @param out      输出缓冲(512个采样点)
 */
static void up_cubic_render(const up_cubic_table_t *t, int step, const short *work, short *history, short *out)
{
	int i;

	for (i = 0; i < UP_OUTPUT_COUNT; i++)
	{
		const short *p = work + t->base[i];
		const short *w = t->weight[i];
		int v;

		v = p[0] * w[0] + p[1] * w[1] + p[2] * w[2] + p[3] * w[3];
		v = (v + (1 << (UP_WEIGHT_BITS - 1))) >> UP_WEIGHT_BITS;

		/* 三次插值会略微过冲，限幅到16位范围 */
		if (v > 32767)
		{
			v = 32767;
		}
		else if (v < -32768)
		{
			v = -32768;
		}

		out[i] = (short)v;
	}

	for (i = 0; i < UP_CUBIC_HISTORY; i++)
	{
		history[i] = work[step + i];
	}
}

/** 把8位采样转成16位，与原来的转换方式一致 */
static short up_convert_8(signed char b)
{
	return (short)((b + b * 256) - 32768);
}

/** 通用三次插值升采样
 * @param up        升采样器状态
 * @param t         该采样率的表
 * @param step      每块消耗的输入帧数
 * @param bits      输入采样位数(8或16)
 * @param channels  输入声道数(1或2)
 * @returns 本块消耗的输入字节数
 */
static int up_cubic(struct upsample_t *up, up_cubic_table_t *t, int step, int bits, int channels)
{
	int i, k;

	if (t->step != step)
	{
		up_cubic_build(t, step);
	}

	for (i = 0; i < UP_CUBIC_HISTORY; i++)
	{
		up_work_left[i] = up->history_left[i];
		up_work_right[i] = up->history_right[i];
	}

	if (bits == 16)
	{
		const short *src = (const short *)up->src;

		for (k = 0; k < step; k++)
		{
			if (channels == 2)
			{
				up_work_left[UP_CUBIC_HISTORY + k] = src[2 * k + 0];
				up_work_right[UP_CUBIC_HISTORY + k] = src[2 * k + 1];
			}
			else
			{
				up_work_left[UP_CUBIC_HISTORY + k] = src[k];
				up_work_right[UP_CUBIC_HISTORY + k] = src[k];
			}
		}
	}
	else
	{
		const signed char *src = (const signed char *)up->src;

		for (k = 0; k < step; k++)
		{
			if (channels == 2)
			{
				up_work_left[UP_CUBIC_HISTORY + k] = up_convert_8(src[2 * k + 0]);
				up_work_right[UP_CUBIC_HISTORY + k] = up_convert_8(src[2 * k + 1]);
			}
			else
			{
				up_work_left[UP_CUBIC_HISTORY + k] = up_convert_8(src[k]);
				up_work_right[UP_CUBIC_HISTORY + k] = up_work_left[UP_CUBIC_HISTORY + k];
			}
		}
	}

	up_cubic_render(t, step, up_work_left, up->history_left, up->left);
	if (channels == 2)
	{
		up_cubic_render(t, step, up_work_right, up->history_right, up->right);
	}
	else
	{
		/* 单声道：右声道与左声道相同 */
		for (i = 0; i < UP_OUTPUT_COUNT; i++)
		{
			up->right[i] = up->left[i];
		}
	}

	return step * (bits / 8) * channels;
}

static int up_8000_16_mono(struct upsample_t *up)
{
	return up_cubic(up, &up_tab_8000, 85, 16, 1); /* (8000 / 48000) * 512 = 85帧，共170字节 */
}

static int up_8000_16_stereo(struct upsample_t *up)
{
	return up_cubic(up, &up_tab_8000, 85, 16, 2); /* 340字节 */
}

static int up_11025_8_mono(struct upsample_t *up)
{
	return up_cubic(up, &up_tab_11025, 117, 8, 1); /* (11025 / 48000) * 512 = 117帧 */
}

static int up_11025_8_stereo(struct upsample_t *up)
{
	return up_cubic(up, &up_tab_11025, 117, 8, 2); /* 234字节 */
}

static int up_11025_16_mono(struct upsample_t *up)
{
	return up_cubic(up, &up_tab_11025, 117, 16, 1); /* 234字节 */
}

static int up_11025_16_stereo(struct upsample_t *up)
{
	return up_cubic(up, &up_tab_11025, 117, 16, 2); /* 468字节，必须是4的倍数，否则左右声道错位 */
}

static int up_12000_16_stereo(struct upsample_t *up)
{
	return up_cubic(up, &up_tab_12000, 128, 16, 2); /* 512字节 */
}

static int up_22050_8_mono(struct upsample_t *up)
{
	return up_cubic(up, &up_tab_22050, 235, 8, 1); /* (22050 / 48000) * 512 = 235帧 */
}

static int up_22050_16_mono(struct upsample_t *up)
{
	return up_cubic(up, &up_tab_22050, 235, 16, 1); /* 470字节 */
}

static int up_22050_16_stereo(struct upsample_t *up)
{
	return up_cubic(up, &up_tab_22050, 235, 16, 2); /* 940字节 */
}

static int up_24000_16_stereo(struct upsample_t *up)
{
	return up_cubic(up, &up_tab_24000, 256, 16, 2); /* 1024字节 */
}

static int up_32000_8_mono(struct upsample_t *up)
{
	return up_cubic(up, &up_tab_32000, 341, 8, 1); /* (32000 / 48000) * 512 = 341帧 */
}

static int up_32000_16_mono(struct upsample_t *up)
{
	return up_cubic(up, &up_tab_32000, 341, 16, 1); /* 682字节 */
}

static int up_32000_16_stereo(struct upsample_t *up)
{
	return up_cubic(up, &up_tab_32000, 341, 16, 2); /* 1364字节 */
}

static int up_44100_8_mono(struct upsample_t *up)
{
	return up_cubic(up, &up_tab_44100, 470, 8, 1); /* (44100 / 48000) * 512 = 470帧 */
}

static int up_44100_16_mono(struct upsample_t *up)
{
	return up_cubic(up, &up_tab_44100, 470, 16, 1); /* 940字节 */
}

static int up_44100_16_stereo(struct upsample_t *up)
{
	return up_cubic(up, &up_tab_44100, 470, 16, 2); /* 1880字节 */
}

static int up_48000_16_mono(struct upsample_t *up)
{
	int i;
	short *left = (short *)up->left;
	short *right = (short *)up->right;
	short *src = (short *)up->src;

	for (i=0; i<512; i++)
	{
		*left++ = *right++ = *src++;
	}

	return 1024; /* 512 * 16bit */
}

static int up_48000_16_stereo(struct upsample_t *up)
{
	/* simple demux */
	demux((short *)up->src, up->left, up->right, 512);
	return 2048; /* 512 * stereo * 16bit */
}

typedef struct entry_t
{
	/** source frequency */
	int freq;
	/** source sample bit count */
	int bits;
	/** source number of channels */
	int channels;
	/** upsamplers to convert to native */
	upsampler_t func;
} entry_t;

/** supported upsamplers */
static entry_t upsamplers[] =
{
	{8000,  16, 1, up_8000_16_mono},
	{8000,  16, 2, up_8000_16_stereo},
	{11025,  8, 1, up_11025_8_mono},
	{11025,  8, 2, up_11025_8_stereo},
	{11025, 16, 1, up_11025_16_mono},
	{11025, 16, 2, up_11025_16_stereo},
	{12000, 16, 2, up_12000_16_stereo},
	{22050,  8, 1, up_22050_8_mono},
	{22050, 16, 1, up_22050_16_mono},
	{22050, 16, 2, up_22050_16_stereo},
	{24000, 16, 2, up_24000_16_stereo},
	{32000,  8, 1, up_32000_8_mono},
	{32000, 16, 1, up_32000_16_mono},
	{32000, 16, 2, up_32000_16_stereo},
	{44100,  8, 1, up_44100_8_mono},
	{44100, 16, 1, up_44100_16_mono},
	{44100, 16, 2, up_44100_16_stereo},
	{48000, 16, 1, up_48000_16_mono},
	{48000, 16, 2, up_48000_16_stereo},
	{0, 0, 0, 0}
};

/** Returns an upsampler from a specified format to SPU2's native
 * @param freq      frequency used
 * @param bits      bits per sample
 * @param channels  number of audio channels
 * @returns a function to be used for upsampling, null if not found
 *
 * Will return a function to convert source audio to Playstation's native.
 * Note a very limited selection of upsamplers are available, only the most
 * commonly used.
 */
upsampler_t find_upsampler(int freq, int bits, int channels)
{
	struct entry_t *p = upsamplers;
	while (p->func != NULL)
	{
		if (p->freq == freq && p->bits == bits && p->channels == channels)
		{
			/* found us an upsampler */
			return p->func;
		}

		p++;
	}

	/* no more upsamplers */
	return NULL;
}

/** Returns the upsampler used by CD audio playback
 * @returns 44100Hz 16bit stereo upsampler without interpolation
 */
upsampler_t find_cdda_upsampler(void)
{
	return up_44100_16_stereo_cd;
}
