#include "pch.h"
#include "Sn76489.h"

void sn76489_setup_volume(struct Sn76489 *const sn76489, int chl, int value)
{
	int8_t vol = value & 0x0F;
	sn76489->chls[chl].vol = vol;

	/* Вызов колбека */
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

	/* noiseType = noiseType; */
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
	
	/*
	if (val & 0x80)
	{
		sn76489->adr = (val & 0x70) >> 4;
		switch (sn76489->adr)
		{
		case 0:
		case 2:
		case 4:
			sn76489->chls[sn76489->adr >> 1].tone = sn76489->chls[sn76489->adr >> 1].tone | (val & 0x0F);
			//sng->freq[sng->adr >> 1] = (sng->freq[sng->adr >> 1] & 0x3F0) | (val & 0x0F);
			break;

		case 1:
		case 3:
		case 5:
			sn76489->chls[(sn76489->adr - 1) >> 1].vol = val & 0xF;
			//sng->volume[(sng->adr - 1) >> 1] = val & 0xF;
			break;

		case 6:
			//sng->noise_mode = (val & 4) >> 2;
			sn76489->noise_type = (val & 4) >> 2;

			if ((val & 0x03) == 0x03) {
				sn76489->chls[SN_CHL_NOISE].tone = sn76489->chls[2].tone;
				//sng->noise_freq = sng->freq[2];
				//sng->noise_fref = 1;
			}
			else {
				sn76489->chls[SN_CHL_NOISE].tone = 32 << (val & 0x03);
				//sng->noise_freq = 32 << (val & 0x03);
				//sng->noise_fref = 0;
			}

			if (sn76489->chls[SN_CHL_NOISE].tone == 0)
				sn76489->chls[SN_CHL_NOISE].tone = 1;

			//sng->noise_seed = 0x8000;
			break;

		case 7:
			sn76489->chls[SN_CHL_NOISE].vol = val & 0x0f;
			//sng->noise_volume = val & 0x0f;
			break;
		}
		
	}
	else
	{
		sn76489->chls[sn76489->adr >> 1].tone = ((val & 0x3F) << 4) | (sn76489->chls[sn76489->adr >> 1].tone & 0x0F);
		//sng->freq[sng->adr >> 1] = ((val & 0x3F) << 4) | (sng->freq[sng->adr >> 1] & 0x0F);
	}*/
}

void sn76489_reset(struct Sn76489 *const sn76489)
{
	memset(sn76489, 0, sizeof(*sn76489));
	for (int i = 0; i < SN_CHLS; ++i)
		sn76489->chls[i].vol = SN_VOL_DEFAULT;
}