#include "pch.h"
#include "Ym2612.h"

#define PREV_STATES 4
#define INS_CALC_TL_RATIO(_tl) (127.0f / (127.0f - (float)(_tl)))
#define INS_SCALE_TL(_tl, _ratio) _tl = (127.0f - ((_ratio) * (127.0f - (float)(_tl))))

#define YM_GET_OP(ym2612_, chl_, op_num_) \
  &(ym2612_->chls[chl_].ins.op[op_num_])

int ins_get_slot(const struct Ym2612Ins *const ins)
{
	static const int slot_map[8] = { 0x8, 0x8, 0x8, 0x8, 0xA, 0xE, 0xE, 0xF };
	return slot_map[ins->algorithm & 7];
}

int ins_find_loudest_slot(const struct Ym2612Ins *const ins)
{
	/* slot mask */
	int slot = ins_get_slot(ins);
	int loudest_slot = YM_OPS - 1;
	
	if (slot & 4)
		if (ins->op[2].tl < ins->op[loudest_slot].tl)
			loudest_slot = 2;

	if (slot & 2)
		if (ins->op[1].tl < ins->op[loudest_slot].tl)
			loudest_slot = 1;

	if (slot & 1)
		if (ins->op[0].tl < ins->op[loudest_slot].tl)
			loudest_slot = 0;
	
	return loudest_slot;
}

// Код у с п е ш н о взят из vgm2tfi by ValleyBell...
bool ins_compare_novol(const struct Ym2612Ins *const ins1, 
	const struct Ym2612Ins *const ins2)
{
	int slot = 0;
	int i = 0;

	if (ins1->algorithm != ins2->algorithm) return false;
	if (ins1->feedback != ins2->feedback) return false;

	slot = ins_get_slot(ins1);

	if (!(slot & 1))
		if (ins1->op[0].tl != ins2->op[0].tl)
			return false;

	if (!(slot & 2))
		if (ins1->op[1].tl != ins2->op[1].tl)
			return false;

	if (!(slot & 4))
		if (ins1->op[2].tl != ins2->op[2].tl)
			return false;

	for (i = 0; i < YM_OPS; i++)
	{
		const struct Ym2612Op *const op1 = &(ins1->op[i]);
		const struct Ym2612Op *const op2 = &(ins2->op[i]);

		if (op1->ar != op2->ar) return false;
		if (op1->d1r != op2->d1r) return false;
		if (op1->d2r != op2->d2r) return false;
		if (op1->rr != op2->rr) return false;
		if (op1->sl != op2->sl) return false;
		if (op1->mul != op2->mul) return false;
		if (op1->dt != op2->dt) return false;
		if (op1->ssgeg != op2->ssgeg) return false;
		if (op1->rs != op2->rs) return false;
	}
	return true;
}

int ins_detect_vol(const struct Ym2612Ins *const ins)
{
	int loudest_slot = ins_find_loudest_slot(ins);
	return ins->op[loudest_slot & 3].tl;
}

void ins_make_louder(struct Ym2612Ins *const ins)
{
	int slot = ins_get_slot(ins);
	int loudest_slot = ins_find_loudest_slot(ins) & 3;
	int old_vol = ins->op[loudest_slot].tl & 0xFF;
	float ratio = 1.0f;

	ins->op[loudest_slot].tl = 0;

	ratio = INS_CALC_TL_RATIO(old_vol);

	if (slot == 8)
		return;

	switch (loudest_slot)
	{
	case 0:
		slot &= 14;
		break;

	case 1:
		slot &= 13;
		break;

	case 2:
		slot &= 11;
		break;

	case 3:
		slot &= 7;
		break;
	}

	if (slot & 8)
		INS_SCALE_TL(ins->op[3].tl, ratio);
	if (slot & 4)
		INS_SCALE_TL(ins->op[2].tl, ratio);
	if (slot & 2)
		INS_SCALE_TL(ins->op[1].tl, ratio);
	if (slot & 1)
		INS_SCALE_TL(ins->op[0].tl, ratio);
}

void ym2612_write_op_reg(
	struct Ym2612 *const ym2612, int reg, int value, int chl, int op_num)
{
	struct Ym2612Op *const op = YM_GET_OP(ym2612, chl, op_num);
	bool success = false;

	switch (reg & 0xF0)
	{
	case 0x30:
		/* detune & multiple */
		op->dt = (value >> 4) & 7;
		op->mul = value & 0xF;
		success = true;
		break;

	case 0x40:
		/* total level */
		op->tl = value & 0x7F;
		success = true;
		break;

	case 0x50:
		/* rate scaling; attack rate */
		op->rs = (uint8_t)(value >> 6);
		op->ar = value & 0x1F;
		success = true;
		break;

	case 0x60:
		/* first decay rate; amplitude modulation */
		op->d1r = value & 0x1F;
		op->am = (value >> 7) & 1;
		success = true;
		break;

	case 0x70:
		/* secondary decay rate */
		op->d2r = value & 0x1F;
		success = true;
		break;

	case 0x80:
		/* secondary amplitude; release rate */
		op->sl = (uint8_t)(value >> 4);
		op->rr = value & 0xF;
		success = true;
		break;

	case 0x90:
		/* proprietary (SSG-EG) */
		op->ssgeg = value & 0xF;
		success = true;
		break;
	} /* switch (reg_hi) */

	if (success)
		if (ym2612->on_set_ins)
			ym2612->on_set_ins(ym2612, chl, op_num);
} /* void ym2612_write_op_reg */

void ym2612_write_port0(struct Ym2612 *const ym2612, int reg, int value)
{
	static bool freq_chg_prepare[3] = { 0 };
	static uint8_t   freq_hi_byte[3] = { 0 };

	static int16_t  freq[3] = { 0 };
	static uint8_t   freq_block[3] = { 0 };

	static int16_t  freq_prev[3] = { 0 };
	static uint8_t   freq_block_prev[3] = { 0 };

	static bool key_state[YM_CHLS] = { 0 };
	static bool key_state_prev[YM_CHLS] = { 0 };

	static int  reg_prev[PREV_STATES] = { 0 };
	int reg_hi = reg & 0xF0;
	int reg_lo = reg & 0x0F;

	reg &= 0xFF;
	value &= 0xFF;

	switch (reg)
	{
	case 0x22:
		/* LF0 */
		ym2612->lfo_enabled = value >> 3;
		ym2612->lfo_freq = value & 7;
		if (ym2612->on_set_lfo)
			ym2612->on_set_lfo(ym2612);
		break;

	case 0x27:
		/* Timers; Ch3 / 6 mode */
		ym2612->ch3_spec = (value >> 6) & 1;
		if (ym2612->on_set_ch3_spec)
			ym2612->on_set_ch3_spec(ym2612);
		break;

	case 0x28:
		/* Key on/off */
	{
		int chl = value & 7;
		if (chl == 7) chl = 6;
		int ops = value >> 4;
		bool enabled = false;

		// 0,1,2,4,5,6
		chl -= (chl > 3);
		//chl -= (chl == 6);
		
		assert(chl < YM_CHLS);

		for (int i = 0; i < YM_OPS; ++i)
		{
			/* op[0] = ops & 1;
			 * op[1] = (ops & 2) >> 1;
			 * op[2] = (ops & 4) >> 2;
			 * op[3] = (ops & 8) >> 3; */
			ym2612->chls[chl].op_enabled[i] = (ops & (1 << i)) >> i;

			if (!enabled)
				enabled = ym2612->chls[chl].op_enabled[i];
		}

		key_state_prev[chl] = key_state[chl];
		key_state[chl] = enabled;

		if (key_state[chl] && !key_state_prev[chl])
			if (ym2612->on_key_attack)
				ym2612->on_key_attack(ym2612, chl);

		if (ym2612->on_set_key_state)
			ym2612->on_set_key_state(ym2612, chl);
	} /* case 0x28: */
	break;

	case 0x2A:
		/* DAC data */
		if (ym2612->on_pass_dac_data)
			ym2612->on_pass_dac_data(ym2612, value);
		break;

	case 0x2B:
		/* DAC enable */
		ym2612->dac_enabled = value >> 7;
		if (ym2612->on_set_dac_enable)
			ym2612->on_set_dac_enable(ym2612);
		break;
	} /* switch (reg) */

	{
		/* operator registers */
		int chl = 0;
		int op_num = 0;

		chl = reg_lo % 4;
		op_num = reg_lo / 4;
		if (op_num == 1) op_num = 2;
		else if (op_num == 2) op_num = 1;

		ym2612_write_op_reg(ym2612, reg, value, chl, op_num);
	}

	switch (reg_hi)
	{
	case 0xA0:
		if (ym2612->ch3_spec)
		{
			/* TODO: запилить ch3 special mode */
		}
		else
		{
			switch (reg_lo)
			{
			case 0: /* CH1 */
			case 1: /* CH2 */
			case 2: /* CH3 */
			{
				int chl = reg_lo & 7;
				struct Ym2612Chl *ch = NULL;
				int i = 0;

				ch = &(ym2612->chls[chl]);
				freq_chg_prepare[chl] = false;
				freq_prev[chl] = freq[chl];
				freq[chl] = (int16_t)((freq_hi_byte[chl] << 8) | value);

				for (i = 0; i < YM_OPS; ++i)
				{
					ch->freq[i] = freq[chl];
					ch->freq_block[i] = freq_block[chl];
				}

				if (ym2612->on_set_freq)
					ym2612->on_set_freq(ym2612, chl);
			}
			break;

			case 4: /* CH1 */
			case 5: /* CH2 */
			case 6: /* CH3 */
			{
				int chl = reg_lo - 4;
				freq_chg_prepare[chl] = true;
				freq_hi_byte[chl] = value & 7;
				freq_block_prev[chl] = freq_block[chl];
				freq_block[chl] = (value >> 3) & 7;
			}
			break;
			} /* switch (reg_lo)  */
		} /* else (ym2612->ch3_spec) */
		break; /* case 0xA0: */

	case 0xB0:
		switch (reg_lo)
		{
		case 0: /* CH1 */
		case 1: /* CH2 */
		case 2: /* CH3 */
			/* feedback; algorithm */
		{
			int chl = reg_lo;
			struct Ym2612Ins *ins = &(ym2612->chls[chl].ins);
			ins->algorithm = value & 7;
			ins->feedback = (value >> 3) & 7;

			if (ym2612->on_set_ins)
				ym2612->on_set_ins(ym2612, chl, -1);
		}
		break;

		case 4: /* CH1 */
		case 5: /* CH2 */
		case 6: /* CH3 */
			/* stereo; LFO sensitivity */
		{
			int chl = reg_lo - 4;
			struct Ym2612Chl *ch = &(ym2612->chls[chl]);
			ch->pan = (value >> 6) & 3;
			ch->ams = (value >> 3) & 3;
			ch->fms = (value & 7);

			if (ym2612->on_set_pan)
				ym2612->on_set_pan(ym2612, chl);
			if (ym2612->on_set_lfo_sens)
				ym2612->on_set_lfo_sens(ym2612, chl);
		}
		break;
		} /* switch (reg_lo) */
		break; /* case 0xB0: */
	} /* switch (reg_hi) */

	for (int i = PREV_STATES - 1; i > 0; --i)
		reg_prev[i] = reg_prev[i - 1];

	reg_prev[0] = reg;
} /* void ym2612_write_port0(Ym2612 *ym2612, int reg, int value) */

void ym2612_write_port1(struct Ym2612 *const ym2612, int reg, int value)
{
	static bool freq_chg_prepare[3] = { 0 };
	static uint8_t   freq_hi_byte[3] = { 0 };

	static int16_t  freq[3] = { 0 };
	static uint8_t   freq_block[3] = { 0 };

	static int16_t  freq_prev[3] = { 0 };
	static uint8_t   freq_block_prev[3] = { 0 };

	static bool key_state[YM_CHLS] = { 0 };
	static bool key_state_prev[YM_CHLS] = { 0 };

	static int  reg_prev[PREV_STATES] = { 0 };
	int reg_hi = reg & 0xF0;
	int reg_lo = reg & 0x0F;

	reg &= 0xFF;
	value &= 0xFF;

	{
		/* operator registers */
		int chl = 0;
		int op_num = 0;

		chl = (reg_lo % 4) + 3;
		op_num = reg_lo / 4;
		if (op_num == 1) op_num = 2;
		else if (op_num == 2) op_num = 1;
		if (chl == 7) chl = 6;

		chl -= (chl == 6);

		ym2612_write_op_reg(ym2612, reg, value, chl, op_num);
	}

	switch (reg_hi)
	{
	case 0xA0:
		if (ym2612->ch3_spec)
		{

		}
		else
		{
			switch (reg_lo)
			{
			case 0: /* CH4 */
			case 1: /* CH5 */
			case 2: /* CH6 */
			{
				int chl = reg_lo;
				struct Ym2612Chl *const ch = &(ym2612->chls[(chl + 3) & 7]);
				int i;

				freq_chg_prepare[chl] = false;
				freq_prev[chl] = freq[chl];
				freq[chl] = (int16_t)((freq_hi_byte[chl] << 8) | value);

				for (i = 0; i < YM_OPS; ++i)
				{
					ch->freq[i] = freq[chl];
					ch->freq_block[i] = freq_block[chl];
				}

				if (ym2612->on_set_freq != NULL)
					ym2612->on_set_freq(ym2612, (chl + 3) & 7);
			}
			break;

			case 4: /* CH4 */
			case 5: /* CH5 */
			case 6: /* CH6 */
			{
				int chl = reg_lo - 4;
				freq_chg_prepare[chl] = true;
				freq_hi_byte[chl] = value & 7;
				freq_block_prev[chl] = freq_block[chl];
				freq_block[chl] = (value >> 3) & 7;
			}
			break;
			} /* switch (reg_lo) */
		} /* else (ym2612->ch3_spec) */
		break; /* case 0xA0: */

	case 0xB0:
	{
		switch (reg_lo)
		{
		case 0: /* CH4 */
		case 1: /* CH5 */
		case 2: /* CH6 */
			/* feedback; algorithm */
		{
			int chl = reg_lo + 3;
			struct Ym2612Ins *const ins = &(ym2612->chls[chl].ins);
			ins->algorithm = value & 7;
			ins->feedback = (value >> 3) & 7;

			if (ym2612->on_set_ins)
				ym2612->on_set_ins(ym2612, chl, -1);
		}
		break;

		case 4:
		case 5:
		case 6:
			/* stereo; LFO sensitivity */
		{
			int chl = reg_lo - 1;
			struct Ym2612Chl *const ch = &(ym2612->chls[chl]);

			ch->pan = (int8_t)(value >> 6);
			ch->ams = (value >> 3) & 3;
			ch->fms = (value & 7);

			if (ym2612->on_set_pan)
				ym2612->on_set_pan(ym2612, chl);
			if (ym2612->on_set_lfo_sens)
				ym2612->on_set_lfo_sens(ym2612, chl);
		}
		break;
		} /* switch (reg_lo) */
	} /* case 0xB0: */
	break; /* case 0xB0: */
	} /* switch (reg_hi) */

	for (int i = PREV_STATES - 1; i > 0; --i)
		reg_prev[i] = reg_prev[i - 1];

	reg_prev[0] = reg;
} /* void ym2612_write_port1(Ym2612 *ym2612, int reg, int value) */

// Код взят из и н т е р н е т а... ну и vgm2mid
int ym2612_convert_freq_to_hz(int freq, int freq_block)
{
	//static const uint32_t OSC1 = 53693100;
	//static const uint32_t fM_YM2612 = OSC1 / 14;
	//static const uint32_t fsam_YM2612 = fM_YM2612 / 72;

	static const uint32_t fsam_YM2612 = 53693100 / 14 / 72;

	//double hz = (freq * ((clock & 0x3FFFFFFF) / 72.0)) * pow(2, freq_block - 22);
	double hz = ((freq * fsam_YM2612) * pow(2, freq_block)) / 2097152.0;
	return (int)hz;
};

void ym2612_reset(struct Ym2612 *const ym2612)
{
	memset(ym2612, 0, sizeof(*ym2612));
	for (int i = 0; i < YM_CHLS; ++i)
		ym2612->chls[i].pan = YM_PAN_DEFAULT;
}