#ifndef _SN76489_H
#define _SN76489_H
#include "pch.h"

#define SN_CHLS 4
#define SN_CHL_NOISE (SN_CHLS - 1)
#define SN_VOL_DEFAULT 0xF
#define SN_VOL_MIN 0xF
#define SN_TONE_DEFAULT 0

enum
{
	SN_CH4_MODE_CH3COMPATIBLE,
	SN_CH4_MODE_CH3NOCOMPATIBLE
};

#define SN_CH4_MODE_DEFAULT SN_CH4_MODE_CH3COMPATIBLE

enum
{
	SN_NOISE_PERIODIC,
	SN_NOISE_WHITE
};

#define SN_NOISE_DEFAULT SN_NOISE_WHITE

enum
{
	SN_LATCH_TONENOISE,
	SN_LATCH_VOLUME
};

enum
{
	SN_VALUE_DATA,
	SN_VALUE_LATCH
};

struct Sn76489Chl
{
	int16_t tone;
	int8_t  vol;
};

struct Sn76489
{
	uint32_t adr;
	struct Sn76489Chl chls[SN_CHLS];
	uint32_t clock;
	int8_t ch4_mode;
	int8_t noise_type;
	/* callbacks */
	void(*on_set_tone)(const struct Sn76489 *const sn76489, int chl);
	void(*on_set_volume)(const struct Sn76489 *const sn76489, int chl);
	void(*on_set_noise_type)(const struct Sn76489 *const sn76489);
	void(*on_set_noise_tone)(const struct Sn76489 *const sn76489);
	void(*on_set_ch4_mode)(const struct Sn76489 *const sn76489);
};

void sn76489_write(struct Sn76489 *const sn76489, int value);
void sn76489_reset(struct Sn76489 *const sn76489);

#endif
