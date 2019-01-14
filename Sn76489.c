#include "pch.h"
#include "Sn76489.h"

void sn76489_setup_volume(struct Sn76489 *const sn76489, int chl, int value)
{
	int8_t vol = value & 0x0F;
	sn76489->chls[chl].vol = vol;

	// Вызов колбека
	if (sn76489->on_set_volume)
		sn76489->on_set_volume(sn76489, chl);
}

void sn76489_setup_noise(struct Sn76489 *const sn76489, int chl, int value)
{
	int8_t noise_tone = value & 3;
	int8_t *const ch4_mode = &(sn76489->ch4_mode);
	static uint8_t ch4_mode_prev = SN_CH4_MODE_CH3COMPATIBLE;

	*ch4_mode = noise_tone == 3;
	sn76489->noise_type = (value >> 2) & 1;

	if (*ch4_mode != ch4_mode_prev)
	{
		if (sn76489->on_set_ch4_mode)
			sn76489->on_set_ch4_mode(sn76489);
	}

	if (*ch4_mode != SN_CH4_MODE_CH3NOCOMPATIBLE)
	{
		sn76489->chls[chl].tone = noise_tone;
		if (sn76489->on_set_noise_tone)
			sn76489->on_set_noise_tone(sn76489);
	}

	if (sn76489->on_set_noise_type)
		sn76489->on_set_noise_type(sn76489);

	ch4_mode_prev = *ch4_mode;
}

void sn76489_write(struct Sn76489 *const sn76489, int value)
{
	static int chl = 0;
	int value_type = SN_VALUE_DATA;
	static int value_type_prev = SN_VALUE_DATA;
	int latch_type = SN_LATCH_TONENOISE;
	static int latch_type_prev = SN_LATCH_TONENOISE;
	static int value_prev = 0;
	static bool tone_changing = false;
	static int16_t tone = 0;

	value_type = (value >> 7) & 1;

	switch (value_type)
	{
	case SN_VALUE_DATA:
		if (tone_changing)
		{
			tone |= (((value & 0x3F) << 4) & 0x3F0);
			if (sn76489->ch4_mode == SN_CH4_MODE_CH3NOCOMPATIBLE && chl == 2)
				chl = 3;
			sn76489->chls[chl].tone = tone;

			if (chl == 3)
			{
				if (sn76489->on_set_noise_tone)
					sn76489->on_set_noise_tone(sn76489);
			}
			else
			{
				if (sn76489->on_set_tone)
					sn76489->on_set_tone(sn76489, chl);
			}

			tone_changing = false;
			tone = 0;
		} // if (tone_changing) 
		else
		{
			(latch_type_prev == SN_LATCH_TONENOISE ?
				sn76489_setup_noise :
				sn76489_setup_volume)(sn76489, chl, value);
		} // else (tone_changing) 
		break;

	case SN_VALUE_LATCH:
		latch_type = (value >> 4) & 1;
		chl = (value >> 5) & 3;

		switch (latch_type)
		{
		case SN_LATCH_TONENOISE:
			if (chl != 3)
			{
				if (!tone_changing)
				{
					tone_changing = true;
					tone = value & 0xF;
				}
			}
			else
				sn76489_setup_noise(sn76489, chl, value);
			break;

		case SN_LATCH_VOLUME:
			sn76489_setup_volume(sn76489, chl, value);
			break;
		} // switch (latch_type) 
		break;
	} // switch (value_type) 

	value_type_prev = value_type;
	latch_type_prev = latch_type;
	value_prev = value;
}

void sn76489_reset(struct Sn76489 *const sn76489)
{
	memset(sn76489, 0, sizeof(*sn76489));
	for (int i = 0; i < SN_CHLS; ++i)
		sn76489->chls[i].vol = SN_VOL_DEFAULT;
}