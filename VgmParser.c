#include "pch.h"
#include "VgmParser.h"

// Глобальные переменные 
struct VgmParser parser;
struct VgmLogger logger;

/*
Функция:
	void vgm_logger_frames_to_deltas()
Действие:
	Конвертирует значения поля frames из структур состояний в VgmLogger
	из абсолютных значений в дельты.
Параметры:
Ошибки:
*/
static void vgm_logger_frames_to_deltas();

// Колбэки, которые будут вызываться из vgm_parce()...
static void vgm_ym2612_log();
static void vgm_sn76489_log();

struct VgmLogger *const get_vgm_logger()
{
	return &logger;
}

struct VgmParser *const get_vgm_parser()
{
	return &parser;
}

// Массивы, заполняемые в callback-ах Ym2612
static bool ym2612_key_attacked[YM_CHLS] = { false };
static bool ym2612_ins_changed[YM_CHLS] = { false };
static bool ym2612_lfo_sens_changed[YM_CHLS] = { false };

void ym2612_on_key_attack(const struct Ym2612 *ym2612, int chl)
{
	ym2612_key_attacked[chl] = true;
}

void ym2612_on_set_ins(const struct Ym2612 *const ym2612, int chl, int op)
{
	ym2612_ins_changed[chl] = true;
}

void ym2612_on_set_lfo_sens(const struct Ym2612 *const ym2612, int chl)
{
	ym2612_lfo_sens_changed[chl] = true;
}

void vgm_parser_init(const struct Vgm *const vgm)
{
	memset(&parser, 0, sizeof(parser));

	if (vgm->ym2612_enabled)
	{
		ym2612_reset(&(parser.ym2612));
		parser.ym2612.clock = vgm->ym2612_clock;
		parser.ym2612.on_key_attack = ym2612_on_key_attack;
		parser.ym2612.on_set_ins = ym2612_on_set_ins;
	}

	if (vgm->sn76489_enabled)
	{
		sn76489_reset(&(parser.sn76489));
		parser.sn76489.clock = vgm->sn76489_clock;
	}
	parser.vgm = vgm;
}

void vgm_logger_init(const struct Vgm *const vgm, int fps)
{
	memset(&logger, 0, sizeof(logger));
				
	logger.fps = fps;
	logger.samples_per_frame = SAMPLE_RATE / logger.fps;
}

static void vgm_ym2612_log()
{
	if (!ym_enabled) return;

	uint32_t frame_count = parser.frame_count;
	static uint32_t frame_count_prev = NOCHANGE;
	static bool first_call = true;
	int16_t freq[YM_CHLS] = { 0 };
	static int16_t freq_prev[YM_CHLS] = { 0 };

	int8_t freq_block[YM_CHLS] = { 0 };
	static int8_t freq_block_prev[YM_CHLS] = { 0 };

	bool key_state[YM_CHLS] = { false };
	static bool key_state_prev[YM_CHLS] = { false };

	int8_t pan[YM_CHLS];// = { YM_PAN_NONE };
	static int8_t pan_prev[YM_CHLS] = { YM_PAN_NONE };

	bool key_noattack[YM_CHLS] = { false };
	bool key_pressed[YM_CHLS] = { false };

	int16_t ins_index[YM_CHLS];
	static int16_t ins_index_prev[YM_CHLS];
	memset(ins_index, NOCHANGE, sizeof(ins_index));
	if (first_call)
		memset(ins_index_prev, NOCHANGE, sizeof(ins_index_prev));

	int8_t vol[YM_CHLS];
	static int8_t vol_prev[YM_CHLS];
	memset(vol, NOCHANGE, sizeof(vol));
	if (first_call)
		memset(vol_prev, NOCHANGE, sizeof(vol_prev));

	int8_t ams[YM_CHLS] = { 0 };
	static int8_t ams_prev[YM_CHLS] = { 0 };

	int8_t fms[YM_CHLS] = { 0 };
	static int8_t fms_prev[YM_CHLS] = { 0 };

	// Флаги изменения состояний
	bool freq_changed[YM_CHLS] = { false };
	bool ins_index_changed[YM_CHLS] = { false };
	bool vol_changed[YM_CHLS] = { false };
	bool pan_changed[YM_CHLS] = { false };
	bool key_state_changed[YM_CHLS] = { false };
	bool ams_changed[YM_CHLS] = { false };
	bool fms_changed[YM_CHLS] = { false };

	const struct Ym2612 *ym2612 = &(parser.ym2612);

	for (int chl = 0; chl < YM_CHLS; ++chl)
	{
		if (!ym_chls_enabled[chl])
			continue;

		if (ym2612->dac_enabled && chl == YM_CHL_DAC)
			continue;
		
		// ссылки...
		const struct Ym2612Chl *ch = &(ym2612->chls[chl]);

		int16_t *const ch_freq = &(freq[chl]);
		int16_t *const ch_freq_prev = &(freq_prev[chl]);

		int8_t *const ch_freq_block = &(freq_block[chl]);
		int8_t *const ch_freq_block_prev = &(freq_block_prev[chl]);

		bool *const ch_key_state = &(key_state[chl]);
		bool *const ch_key_state_prev = &(key_state_prev[chl]);

		int8_t *const ch_pan = &(pan[chl]);
		int8_t *const ch_pan_prev = &(pan_prev[chl]);

		bool *const ch_key_noattack = &(key_noattack[chl]);
		bool *const ch_key_pressed = &(key_pressed[chl]);

		int16_t *const ch_ins_index = &(ins_index[chl]);
		int16_t *const ch_ins_index_prev = &(ins_index_prev[chl]);

		int8_t *const ch_vol = &(vol[chl]);
		int8_t *const ch_vol_prev = &(vol_prev[chl]);

		int8_t *const ch_ams = &(ams[chl]);
		int8_t *const ch_ams_prev = &(ams_prev[chl]);

		int8_t *const ch_fms = &(fms[chl]);
		int8_t *const ch_fms_prev = &(fms_prev[chl]);

		bool *const ch_key_attacked = &(ym2612_key_attacked[chl]);
		bool *const ch_ins_changed = &(ym2612_ins_changed[chl]);

		// Флаги изменения состояний
		bool *const ch_freq_changed = &(freq_changed[chl]);
		bool *const ch_ins_index_changed = &(ins_index_changed[chl]);
		bool *const ch_vol_changed = &(vol_changed[chl]);
		bool *const ch_pan_changed = &(pan_changed[chl]);
		bool *const ch_key_state_changed = &(key_state_changed[chl]);
		bool *const ch_ams_changed = &(ams_changed[chl]);
		bool *const ch_fms_changed = &(fms_changed[chl]);

		*ch_freq = ch->freq[0];
		*ch_freq_block = ch->freq_block[0];
		*ch_key_state = ch->op_enabled[0];
		
		if (!ym_enable_keyoff_notes && !(*ch_key_state))
			*ch_key_attacked = false;

		*ch_key_noattack = !(*ch_key_attacked);
		
		*ch_pan = ch->pan;
		*ch_ams = ch->ams;
		*ch_fms = ch->fms;

		// Оптимизация... если нота отпущена и не включена опция,
		// -fm_enable_keyoff_notes
		// то исключить изменение частоты
		if (!ym_enable_keyoff_notes && !(*ch_key_state))
		{
			*ch_freq = *ch_freq_prev;
			*ch_freq_block = *ch_freq_block_prev;
		}

		*ch_freq_changed = ((*ch_freq != *ch_freq_prev) ||
			(*ch_freq_block != *ch_freq_block_prev));
		
		*ch_pan_changed = *ch_pan != *ch_pan_prev;
		*ch_key_state_changed = *ch_key_state != *ch_key_state_prev;
		*ch_key_pressed = *ch_key_attacked;
		*ch_ams_changed = *ch_ams != *ch_ams_prev;
		*ch_fms_changed = *ch_fms != *ch_fms_prev;

		if (*ch_ins_changed && (*ch_pan != YM_PAN_NONE))
		{
			// Делаем копию инструмента
			struct Ym2612Ins ins = ch->ins;

			int16_t index = NOCHANGE;
			// Ищем такой же инмтрумент
			for (int i = 0; i < logger.ins_count; ++i)
			{
				// Если инструмент тот же, но отличается громкость
				if (ins_compare_novol(&(logger.ins[i]), &ins))
				{
					index = i;
					*ch_vol = ins_detect_vol(&ins);
					break;
				}
			}

			// Если инструмент не найден в коллекции...
			if (index == NOCHANGE)
			{
				// Узнать его текущуу громкость и установить как
				// громкость канала
				*ch_vol = ins_detect_vol(&ins);
				// максимизировать инструмент
				ins_make_louder(&ins);
				// Записать в коллекцию
				logger.ins[logger.ins_count] = ins;
				// Установить индекс этого инструмента текущим
				index = logger.ins_count;
				++(logger.ins_count);
			}

			*ch_ins_index = index;

			*ch_ins_index_changed = *ch_ins_index != *ch_ins_index_prev;
			*ch_vol_changed = *ch_vol != *ch_vol_prev;
		} // if (*ch_ins_changed && (*ch_pan != YM_PAN_NONE))

		{
			bool empty = logger.ym2612_state_count[chl] == 0;

			if (logger.ym2612_state_count[chl] >= STATES_MAX_COUNT)
				goto err_range;

			struct Ym2612State *s_back =
				&(logger.ym2612_state[chl]
					[!empty ? logger.ym2612_state_count[chl] - 1 : 0]);
			struct Ym2612State *s_new =
				&(logger.ym2612_state[chl][logger.ym2612_state_count[chl]]);

			if (*ch_freq_changed || *ch_key_state_changed ||
				*ch_ins_index_changed || *ch_vol_changed ||
				*ch_pan_changed || *ch_key_pressed ||
				*ams_changed || *fms_changed)
			{
				struct Ym2612State *s;
				if (frame_count != frame_count_prev)
				{
					s = s_new;
					s->freq =
						(*ch_freq_changed || *ch_key_pressed || *ch_key_state_changed) ?
						*ch_freq : (!empty ? NOCHANGE : YM_FREQ_DEFAULT);
					s->freq_block = (s->freq != NOCHANGE) ? *ch_freq_block :
						(!empty ? NOCHANGE : YM_FREQ_BLOCK_DEFAULT);
					s->key_state = *ch_key_state_changed ?
						*ch_key_state : (!empty ? NOCHANGE : YM_KEY_STATE_DEFAULT);
					s->key_noattack = (*ch_freq_changed || *ch_vol_changed ||
						*ch_pan_changed || *ch_ins_index_changed || *ch_ams_changed || *ch_fms_changed) ?
						*ch_key_noattack : NOCHANGE;
					s->ins_index = *ch_ins_index_changed ? *ch_ins_index : NOCHANGE;
					s->vol = *ch_vol_changed ? *ch_vol : (!empty ? NOCHANGE : YM_VOL_DEFAULT);
					s->pan = *ch_pan_changed ? *ch_pan : (!empty ? NOCHANGE : YM_PAN_DEFAULT);
					s->ams = *ch_ams_changed ? *ch_ams : (!empty ? NOCHANGE : 0);
					s->fms = *ch_fms_changed ? *ch_fms : (!empty ? NOCHANGE : 0);
					s->frames = frame_count;
					++(logger.ym2612_state_count[chl]);
				}
				else
				{
					s = s_back;
					if (*ch_freq_changed || *ch_key_pressed || *ch_key_state_changed)
					{
						s->freq = *ch_freq;
						s->freq_block = *ch_freq_block;
					}

					if (*ch_key_state_changed)
						s->key_state = *ch_key_state;

					if (*ch_freq_changed || *ch_vol_changed ||
						*ch_pan_changed || *ch_ins_index_changed)
						s->key_noattack = *ch_key_noattack;

					if (*ch_ins_index_changed)
						s->ins_index = *ch_ins_index;

					if (*ch_vol_changed)
						s->vol = *ch_vol;

					if (*ch_pan_changed)
						s->pan = *ch_pan;

					if (*ch_ams_changed)
						s->ams = *ch_ams;

					if (*ch_fms_changed)
						s->fms = *ch_fms;
				}

				*ch_ins_changed = false;
				*ch_key_attacked = false;
				*ch_ams_changed = false;
				*ch_fms_changed = false;
			}
			else
			{
				if (empty) ++(logger.ym2612_state_count[chl]);
				s_back->frames = frame_count;
				//s_back->smp = parser.sample_count;
			}
		}

		

		*ch_freq_prev = *ch_freq;
		*ch_freq_block_prev = *ch_freq_block;
		*ch_key_state_prev = *ch_key_state;
		*ch_pan_prev = *ch_pan;
		*ch_ins_index_prev = *ch_ins_index;
		*ch_vol_prev = *ch_vol;
		*ch_ams_prev = *ch_ams;
		*ch_fms_prev = *ch_fms;
	} // for (int chl = 0; chl < YM_CHLS; ++chl)

	frame_count_prev = frame_count;
	first_call = false;
	return;

err_range:
	errno = ERANGE;
	return;
} // void vgm_ym2612_log(const VgmParser *parser, VgmLogger *logger)

static void vgm_sn76489_log()
{
	static bool first_call = true;
	uint32_t frame_count = parser.frame_count;

	static uint32_t frame_count_prev = 0;

	int8_t vol[SN_CHLS];
	static int8_t vol_prev[SN_CHLS];

	int16_t tone[SN_CHLS] = { 0 };
	static int16_t tone_prev[SN_CHLS] = { 0 };

	for (int i = 0; i < SN_CHLS; ++i)
	{
		if (first_call)
			vol_prev[i] = SN_VOL_DEFAULT;

		vol[i] = SN_VOL_DEFAULT;
	}

	const struct Sn76489 *sn76489 = &(parser.sn76489);

	bool tone_changed[SN_CHLS] = { false };
	bool vol_changed[SN_CHLS] = { false };
	bool ch4_mode_changed = false;
	bool noise_type_changed = false;

	const struct Sn76489Chl *ch = NULL;
	int8_t *ch_vol = NULL;
	int8_t *ch_vol_prev = NULL;
	int16_t *ch_tone = NULL;
	int16_t *ch_tone_prev = NULL;
	bool *ch_vol_changed = NULL;
	bool *ch_tone_changed = NULL;

	for (int chl = 0; chl < STATES_SN_SIZE; ++chl)
	{
		ch = &(sn76489->chls[chl]);
		ch_vol = &(vol[chl]);
		ch_vol_prev = &(vol_prev[chl]);
		ch_tone = &(tone[chl]);
		ch_tone_prev = &(tone_prev[chl]);
		ch_vol_changed = &(vol_changed[chl]);
		ch_tone_changed = &(tone_changed[chl]);

		*ch_vol = ch->vol;
		*ch_tone = ch->tone;

		*ch_vol_changed = (*ch_vol != *ch_vol_prev);
		*ch_tone_changed = (*ch_tone != *ch_tone_prev);

		if (*ch_vol == SN_VOL_MIN && *ch_tone_changed)
		{
			*ch_tone = *ch_tone_prev;
			*ch_tone_changed = false;
		}
			
		/*if (*ch_tone == SN_TONE_DEFAULT && *ch_vol_changed)
		{
			*ch_vol = *ch_vol_prev;
			*ch_vol_changed = false;
		}*/

		{
			bool empty = logger.sn76489_state_count[chl] == 0;

			if (logger.sn76489_state_count[chl] >= STATES_MAX_COUNT)
				goto err_range;

			struct Sn76489State *s_back =
				&(logger.sn76489_state[chl]
					[!empty ? logger.sn76489_state_count[chl] - 1 : 0]);
			struct Sn76489State *s_new =
				&(logger.sn76489_state[chl][logger.sn76489_state_count[chl]]);

			if (*ch_tone_changed || *ch_vol_changed)
			{
				struct Sn76489State *s;
				if (frame_count != frame_count_prev)
				{
					s = s_new;
					s->vol = *ch_vol_changed ? *ch_vol :
						(!empty ? NOCHANGE : SN_VOL_DEFAULT);
					s->tone = *ch_tone_changed ? *ch_tone :
						(!empty ? NOCHANGE : SN_TONE_DEFAULT);
					s->frames = frame_count;
					++(logger.sn76489_state_count[chl]);
				}
				else
				{
					s = s_back;
					if (*ch_vol_changed)
						s->vol = *ch_vol;

					if (*ch_tone_changed)
						s->tone = *ch_tone;
				}
			}
			else
			{
				if (empty) ++(logger.sn76489_state_count[chl]);
				s_back->frames = frame_count;
			}
		}

		*ch_vol_prev = *ch_vol;
		*ch_tone_prev = *ch_tone;
	}

	// Noise states
	int chl = SN_CHLS - 1;

	int8_t ch4_mode = NOCHANGE;
	static int8_t ch4_mode_prev = NOCHANGE;

	int8_t noise_type = NOCHANGE;
	static int8_t noise_type_prev = NOCHANGE;

	ch4_mode = sn76489->ch4_mode;
	noise_type = sn76489->noise_type;

	ch = &(sn76489->chls[chl]);
	ch_vol = &(vol[chl]);
	ch_vol_prev = &(vol_prev[chl]);
	ch_tone = &(tone[chl]);
	ch_tone_prev = &(tone_prev[chl]);
	ch_vol_changed = &(vol_changed[chl]);
	ch_tone_changed = &(tone_changed[chl]);

	*ch_vol = ch->vol;
	*ch_tone = ch->tone;

	*ch_vol_changed = *ch_vol != *ch_vol_prev;
	*ch_tone_changed = *ch_tone != *ch_tone_prev;

	if (*ch_vol == SN_VOL_MIN && *ch_tone_changed)
	{
		*ch_tone = *ch_tone_prev;
		*ch_tone_changed = false;
	}
	
	ch4_mode_changed = ch4_mode != ch4_mode_prev;
	noise_type_changed = noise_type != noise_type_prev;

	{
		bool empty = logger.sn76489_noise_state_count == 0;

		if (logger.sn76489_noise_state_count >= STATES_MAX_COUNT)
			goto err_range;

		struct Sn76489NoiseState *s_back =
			&(logger.sn76489_noise_state
				[!empty ? logger.sn76489_noise_state_count - 1 : 0]);
		struct Sn76489NoiseState *s_new =
			&(logger.sn76489_noise_state[logger.sn76489_noise_state_count]);

		if (ch4_mode_changed || noise_type_changed ||
			*ch_vol_changed || *ch_tone_changed)
		{
			struct Sn76489NoiseState *s = NULL;
			if (frame_count != frame_count_prev)
			{
				s = s_new;
				s->vol = *ch_vol_changed ? *ch_vol : (!empty ? NOCHANGE : SN_VOL_DEFAULT);
				s->tone = *ch_tone_changed ? *ch_tone : (!empty ? NOCHANGE : SN_TONE_DEFAULT);
				s->ch4_mode = ch4_mode_changed ? ch4_mode :
					(!empty ? NOCHANGE : SN_CH4_MODE_DEFAULT);
				s->noise_type = noise_type_changed ? noise_type :
					(!empty ? NOCHANGE : SN_NOISE_DEFAULT);
				s->frames = frame_count;
				++(logger.sn76489_noise_state_count);
			}
			else
			{
				s = s_back;
				if (*ch_vol_changed)
					s->vol = *ch_vol;

				if (*ch_tone_changed)
					s->tone = *ch_tone;

				if (ch4_mode_changed)
					s->ch4_mode = ch4_mode;

				if (noise_type_changed)
					s->noise_type = noise_type;
			}
		}
		else
		{
			if (empty) ++(logger.sn76489_noise_state_count);
			s_back->frames = frame_count;
		}
	}

	*ch_vol_prev = *ch_vol;
	*ch_tone_prev = *ch_tone;
	ch4_mode_prev = ch4_mode;
	noise_type_prev = noise_type;

	frame_count_prev = frame_count;
	first_call = false;
	return;

err_range:
	errno = ERANGE;
	return;
}

void vgm_ym2612_dac_log(const struct VgmDacStream *const dac_stream)
{
	uint32_t frame_count = parser.frame_count;
	static uint32_t frame_count_prev = NOCHANGE;
	int32_t chl = YM_CHL_DAC; /* LAST (5) */
	static bool first_call = true;
	uint32_t *const ch_state_count = &(logger.ym2612_state_count[chl]);

	if (first_call && logger.ym2612_state_count[chl] != 0)
	{
		memset(&(logger.ym2612_state[chl]), 0, 
			sizeof(logger.ym2612_state[chl][0]) * logger.ym2612_state_count[chl]);
		logger.ym2612_state_count[chl] = 0;
	}

	bool empty = logger.ym2612_state_count[chl] == 0;

	if (logger.ym2612_state_count[chl] >= STATES_MAX_COUNT)
		goto err_range;

	struct Ym2612State *s_back =
		&(logger.ym2612_state[chl]
			[!empty ? logger.ym2612_state_count[chl] - 1 : 0]);
	struct Ym2612State *s_new =
		&(logger.ym2612_state[chl][logger.ym2612_state_count[chl]]);

	if (frame_count != frame_count_prev)
	{
		s_new->freq = dac_stream && dac_stream->enabled ? dac_stream->block_id + 1 : 0;
		s_new->frames = frame_count;

		++(logger.ym2612_state_count[chl]);
	}
	else
	{
		if (dac_stream && dac_stream->enabled)
			s_back->freq = dac_stream->block_id + 1;

	}
	
	//printf("DAC played! frame = %d stream ID = %d, block ID = %d, smp rate = %d\n",
	//	frame_count, dac_stream_id, dac_stream->block_id, dac_stream->sample_rate);
	first_call = false;
	frame_count_prev = frame_count;
	return;

err_range:
	errno = ERANGE;
	return;
}

void vgm_ym2612_dac_final_log(const struct VgmDacStream *const dac_stream)
{
	struct Ym2612 *const ym2612 = &(parser.ym2612);
	bool dac_enabled = ym_enabled && ym_chls_enabled[YM_CHL_DAC];
	const uint32_t frame_count = parser.frame_count;

	if (dac_enabled && ym2612->dac_enabled)
	{
		struct Ym2612State *s_back =
			&(logger.ym2612_state[YM_CHL_DAC]
				[logger.ym2612_state_count[YM_CHL_DAC] - 1]);

		s_back->freq = dac_stream->block_id + 1;
		s_back->frames = frame_count;
	}

}

void vgm_build_note_table()
{
#define NOTE_TABLE_DEFAULT_OCTAVE 5
	// Таблица реальных частот нот
	// которые будет возвращать функция ym2612_convert_freq_to_hz()
	static const int16_t ref_hz_table[NOTE_TABLE_WIDTH][NOTES_IN_OCT] =
	{
		/*{
			65, 69, 73, 77, 82, 87,
			92, 98, 103, 110, 116, 123
		},*/
		{
			130, 138, 147, 155, 164, 174, // oct -2 (3)
			185, 196, 207, 220, 233, 247
		},
		{ 
			261, 277, 293, 311, 329, 349,	// oct -1 (4)
			369, 392, 415, 440, 466, 493 
		},
		{ 
			523, 554, 587, 622, 659, 698,	// oct 0 (5)
			739, 784, 830, 880, 932, 987 
		},
		{ 
			1046, 1108, 1174, 1244, 1318, 1396,	// oct +1 (6)
			1480, 1568, 1661, 1720, 1864, 1975 
		},
		{
			2093, 2217, 2349, 2489, 2637, 2793,	// oct +2 (7)
			2960, 3136, 3332, 3440, 3729, 3951
		}
	};

	// Количества использований найденных частот в треке
	// или их популярность в треке
	uint32_t note_popularity[NOTE_TABLE_WIDTH][NOTES_IN_OCT] = { 0 };
	// Отклонение найденной ноты от идеальной
	int16_t note_hz_offset[NOTE_TABLE_WIDTH][NOTES_IN_OCT] = { 0 };
	// Ссылки...
	int16_t(*const note_table)[NOTE_TABLE_WIDTH][NOTES_IN_OCT] =
		&(logger.note_table);
	const int16_t(*const unique_freqs)[UNIQUE_FREQS_BUFFER_LENGTH] =
		&(logger.unique_freqs);
	const uint32_t(*const unique_freqs_counts)[UNIQUE_FREQS_BUFFER_LENGTH] =
		&(logger.unique_freqs_counts);

	// Проход по всем найденным уникальным частотам...
	for (int i = 0; i < logger.unique_freqs_count; ++i)
	{
		int16_t freq = (*unique_freqs)[i];
		uint32_t freq_count = (*unique_freqs_counts)[i];
		int16_t hz = ym2612_convert_freq_to_hz(freq, NOTE_TABLE_DEFAULT_OCTAVE);
//#ifndef NDEBUG
//		if (hz < ref_hz_table[0][0] || hz > ref_hz_table[NOTE_TABLE_WIDTH - 1][NOTES_IN_OCT - 1])
//			printf("Warning! Detected frequency out of range. Hz = %d\n", (int)hz);
//#endif
		int16_t found_ref_note;
		int x, y;
		// Найти ближайшую идеальную частоту...
		ARRAY_2D_SORTED_FIND_NEAREST(int16_t, ref_hz_table, hz, y, x);

		// Ссылки...
		const int16_t *const xy_ref_hz_table = &(ref_hz_table[y][x]);
		int16_t *const xy_note_hz_offset = &(note_hz_offset[y][x]);
		uint32_t *const xy_note_popularity = &(note_popularity[y][x]);
		int16_t *const xy_note_table = &((*note_table)[y][x]);
		found_ref_note = *xy_ref_hz_table;
		const int16_t cur_note_hz_offset = hz - found_ref_note;
		const int16_t cur_note_hz_diff = abs(cur_note_hz_offset);

		// Записывать или нет
		bool write = *xy_note_table == 0;
		write = !write ? 
			(/**xy_note_popularity < freq_count ||*/
				abs(*xy_note_hz_offset) > cur_note_hz_diff) : write;

		if (write)
		{
			*xy_note_table = freq;
			*xy_note_popularity = freq_count;
			*xy_note_hz_offset = cur_note_hz_offset;
		}
	} // for (int i = 0; i < logger.unique_freqs_count; ++i)
#undef NOTE_TABLE_DEFAULT_OCTAVE
} // void vgm_build_note_table(struct VgmLogger *logger)

void vgm_find_unique_freqs()
{
	struct Ym2612State (*ch_s)[STATES_MAX_COUNT];
	struct Ym2612State *s;
	int16_t(*unique_freqs)[UNIQUE_FREQS_BUFFER_LENGTH] = 
		&(logger.unique_freqs);
	uint32_t(*unique_freqs_counts)[UNIQUE_FREQS_BUFFER_LENGTH] = 
		&(logger.unique_freqs_counts);

	for (uint32_t chl = 0; chl < STATES_YM_SIZE; ++chl)
	{
		ch_s = &(logger.ym2612_state[chl]);
		for (size_t i = 0; i < logger.ym2612_state_count[chl]; ++i)
		{
			s = &((*ch_s)[i]);
			if (s->freq == NOCHANGE || s->freq == 0) continue;

			int freq = s->freq;

			int index;
			for (index = 0;
				index < logger.unique_freqs_count &&
				(*unique_freqs)[index] != freq; ++index);

			if ((index == logger.unique_freqs_count ||
				logger.unique_freqs_count == 0) &&
				logger.unique_freqs_count <= UNIQUE_FREQS_BUFFER_LENGTH)
			{
				int new_index = logger.unique_freqs_count;
				(*unique_freqs)[new_index] = freq;
				(*unique_freqs_counts)[new_index] = 1;
				++(logger.unique_freqs_count);
			}
			else
			{
				++((*unique_freqs_counts)[index]);
			}
		} // for (uint32_t i = 0; i < logger.ym2612_state_count[chl]; ++i)
	} // for (uint32_t chl = 0; chl < STATES_YM_SIZE; ++chl)
} // void vgm_find_unique_freqs(struct VgmLogger *logger)

void vgm_parse(const struct Vgm *const vgm)
{
	bool gd3_parsing = false;
	bool data_block_parsing = false;
	const uint32_t samples_per_frame = logger.samples_per_frame;
	uint8_t ym2612_reg = 0;
	uint8_t ym2612_val = 0;
	uint8_t sn76489_val = 0;
	uint8_t vgm_command = 0;
	uint8_t vgm_command_prev = 0;
	uint8_t *vgm_data = vgm->vgm_data;
	uint32_t off = 0;
	uint32_t *const sample_count = &(parser.sample_count);
	uint32_t *const frame_count = &(parser.frame_count);
	uint32_t frame_count_prev = 0;
	bool vgm_command_is_unknown = false;
	struct Ym2612 *const ym2612 = &(parser.ym2612);
	struct Sn76489 *const sn76489 = &(parser.sn76489);
	bool dac_enabled = ym_enabled && ym_chls_enabled[YM_CHL_DAC] && ym2612->dac_enabled;
	struct VgmDacStream dac_stream_prev = { 0 };
	const uint32_t smps_end = smps_length + smps_start_offset;
#define FRAME_COUNT_SETUP() \
do \
{ \
	frame_count_prev = *frame_count; \
	*frame_count = *sample_count / samples_per_frame; \
	if (*frame_count < smps_start_offset) \
		*frame_count = 0; \
	else \
		*frame_count -= smps_start_offset; \
} while (false)
// Сокращение...
#define RUN_LOG_FUNCS(_ADD_SAMPLES) \
do \
{ \
	*sample_count += (_ADD_SAMPLES); \
	FRAME_COUNT_SETUP(); \
	if (*frame_count != frame_count_prev) \
	{ \
		if (smps_length != 0 && *frame_count > smps_end) \
			break; \
		if (vgm->ym2612_enabled && ym_enabled) \
			vgm_ym2612_log(); \
		if (vgm->sn76489_enabled && sn_enabled) \
			vgm_sn76489_log(); \
	} \
} while (false)

	/* Парсинг всего до GD3 тэгов */
	while ((vgm_data + off) != vgm->gd3)
	{
		vgm_command = *(vgm_data + off); ++off;
		vgm_command &= 0xFF;

		switch (vgm_command)
		{
		case 0x4F:
			/* Game Gear PSG stereo, write dd to port 0x06 */
		{
			/* TODO: PSG STEREO SUPPORT */
			uint8_t dd = 0;
			dd = *(vgm_data + off); ++off;
		}
		break;

		case 0x50:
			/* PSG(SN76489 / SN76496) write value dd */
			sn76489_val = *(vgm_data + off); ++off;
			if (sn76489)
				sn76489_write(sn76489, sn76489_val);
			break;

		case 0x52:
			/* aa dd : YM2612 port 0, write value dd to register aa */
			ym2612_reg = *(vgm_data + off); ++off;
			ym2612_val = *(vgm_data + off); ++off;

			if (ym2612)
				ym2612_write_port0(ym2612, ym2612_reg, ym2612_val);
			break;

		case 0x53:
			ym2612_reg = *(vgm_data + off); ++off;
			ym2612_val = *(vgm_data + off); ++off;

			if (ym2612)
				ym2612_write_port1(ym2612, ym2612_reg, ym2612_val);
			break;
			/* case 0x54: YM2151, write value dd to register aa*/
		case 0x61:
			/* 0x61 nn nn : Wait n samples, n can range from 0 to 65535 (approx 1.49
			 * seconds).Longer pauses than this are represented by multiple
			 * wait commands.*/
		{
			uint32_t seek = off;
			uint16_t n = 0;
			n = *((uint16_t *)(vgm_data + off)); off += 2;

			if (n > 0)
				RUN_LOG_FUNCS(n);
		} /* case 0x61: */
		break; /* case 0x61: */

		case 0x62:
			/* wait 735 samples(60th of a second), a shortcut for
			 * 0x61 0xdf 0x02 */
			RUN_LOG_FUNCS(735);
		break; /* case 0x62: */

		case 0x63:
			/* wait 882 samples(50th of a second), a shortcut for
			 * 0x61 0x72 0x03 */
			RUN_LOG_FUNCS(882);
		break; /* case 0x63: */

		case 0x66:
			/* end of sound data */
			gd3_parsing = true;
			break;

		case 0x67:
			// data block
		{
			// compatibility command to make older players stop parsing the stream
			++off;

			struct DataBlock *b = &(parser.data_block[parser.data_block_count++]);

			uint8_t data_type = 0;
			uint32_t data_size = 0;
			data_type = *(vgm_data + off); ++off;
			data_size = *((uint32_t *)(vgm_data + off)); off += 4;
			b->begin = vgm_data + off;
			b->size = data_size;
			off += data_size;

			data_block_parsing = false;
		}
		break;

		case 0x90:
			// Setup Stream Control :
			// 0x90 ss tt pp cc
			//  ss = Stream ID
			//  tt = Chip Type(see clock - order at header, e.g.YM2612 = 0x02)
			//  bit 7 is used to select the 2nd chip
			//  pp cc = write command / register cc at port pp
			off += 4; // skip dis shit
			break;

		case 0x91:
			// Set Stream Data:
			// 0x91 ss dd ll bb
			//  ss = Stream ID
			//  dd = Data Bank ID(see data block types 0x00..0x3f)
			//  ll = Step Size(how many data is skipped after every write, usually 1)
			//  Set to 2, if you're using an interleaved stream (e.g. for
			//  left / right channel).
			//  bb = Step Base(data offset added to the Start Offset when starting
			//    stream playback, usually 0)
			//  If you're using an interleaved stream, set it to 0 in one stream
			//  and to 1 in the other one.
			//  Note: Step Size / Step Step are given in command - data - size
			//      (i.e. 1 for YM2612, 2 for PWM), not uint8_ts
			off += 4;
			break;

		case 0x92:
			// Set Stream Frequency
			// 0x92 ss ff ff ff ff
			// ss = Stream ID
			//  ff = Frequency(or Sample Rate, in Hz) at which the writes are done
		{
			//sizeof(struct VgmParser)
			uint8_t s_id = *(vgm_data + off); ++off;
			parser.dac_stream.sample_rate =
				*((uint32_t *)(vgm_data + off)); off += 4;
			break;
		}

		case 0x93:
			// Start Stream :
			// 0x93 ss aa aa aa aa mm ll ll ll ll
			//  ss = Stream ID
			//  aa = Data Start offset in data bank(uint8_t offset in data bank)
			//  Note : if set to - 1, the Data Start offset is ignored
			//  mm = Length Mode(how the Data Length is calculated)
			//  00 - ignore(just change current data position)
			//  01 - length = number of commands
			//  02 - length in msec
			//  03 - play until end of data
			//  8 ? -(bit 7) Loop(automatically restarts when finished)
			//  ll = Data Length
		{
			uint8_t s_id = *(vgm_data + off); ++off;
			parser.dac_stream.enabled = true;
			off += 9;
			break;
		}
		case 0x94:
		{
			//Stop Stream:
			//  0x94 ss
			//    ss = Stream ID
			uint8_t s_id = *(vgm_data + off); ++off;
			parser.dac_stream.enabled = false;
			break;
		}
		case 0x95:
			// Start Stream (fast call):
			// 0x95 ss bb bb ff
			//  ss = Stream ID
			//  bb = Block ID(number of the data block that's part of the data bank set
			//  with command 0x91)
			//  ff = Flags
			//  bit 0 - Loop(see command 0x93)
		{
			dac_enabled = ym_enabled && ym_chls_enabled[YM_CHL_DAC] && ym2612->dac_enabled;
			uint8_t s_id = 0;
			uint16_t block_id = 0;
			uint8_t flags = 0;
			struct VgmDacStream *s = &(parser.dac_stream);

			FRAME_COUNT_SETUP();
			if (smps_length != 0 && *frame_count > smps_end)
				break;

			s_id = *(vgm_data + off); ++off;
			block_id = *((uint16_t *)(vgm_data + off)); off += 2;
			flags = *(vgm_data + off); ++off;

			s->block_id = block_id;
			s->enabled = true;

			if (dac_enabled)
				vgm_ym2612_dac_log(&dac_stream_prev);

			dac_stream_prev = *s;
			break;
		}
		case 0xE0:
			//seek to offset dddddddd(Intel uint8_t order) in PCM data bank
			parser.pcm_data_bank_seek = *((uint32_t *)(vgm_data + off)); off += 4;
			break;

		default:
		{
			//printf("%02X, frames = %010d\n", (int)vgm_command, frame_count);
		}
		break;
		} /* switch (vgm_command) */

		switch (vgm_command & 0xF0)
		{
		case 0x70:
			// wait n+1 samples, n can range from 0 to 15.
		{
			uint8_t smp = (vgm_command & 0xF) + 1;
			RUN_LOG_FUNCS(smp);
		}
		break;

		case 0x80:
			// YM2612 port 0 address 2A write from the data bank, then wait
			// n samples; n can range from 0 to 15. Note that the wait is n,
			// NOT n + 1.
		{
			uint8_t smp = vgm_command & 0xF;
			if (smp == 0) break;
			RUN_LOG_FUNCS(smp);
		}
		break;
		}
	} /* while ((vgm_data + off) != vgm->gd3) */
	
	// DAC final update
	FRAME_COUNT_SETUP();
	if (dac_enabled)
		vgm_ym2612_dac_log(&(parser.dac_stream));

	vgm_logger_frames_to_deltas();
#undef RUN_LOG_FUNCS
#undef FRAME_COUNT_SETUP
} // void vgm_parse(const struct Vgm *const vgm)

void vgm_logger_remove_blank_chls()
{
	struct VgmLogger *const logger = get_vgm_logger();
	for (int chl = 0; chl < STATES_YM_SIZE; ++chl)
	{
		bool blank = false;
		struct Ym2612State(*const ch_s)[STATES_MAX_COUNT] = &(logger->ym2612_state[chl]);
		struct Ym2612State *s;

		uint32_t *ch_state_count = &(logger->ym2612_state_count[chl]);
		{
			uint32_t blank_states = 0;
			for (; blank_states < *ch_state_count; ++blank_states)
			{
				s = &((*ch_s)[blank_states]);
				blank = s->freq == 0;
				if (!blank)
					blank = s->vol == YM_VOL_MIN;

				if (!blank) break;
			}

			if (blank_states == *ch_state_count)
				*ch_state_count = 0;
		}
	}

	for (int chl = 0; chl < STATES_SN_SIZE; ++chl)
	{
		bool blank = false;
		struct Sn76489State(*const ch_s)[STATES_MAX_COUNT] = &(logger->sn76489_state[chl]);
		struct Sn76489State *s;

		uint32_t *ch_state_count = &(logger->sn76489_state_count[chl]);
		{
			uint32_t blank_states = 0;
			for (; blank_states < *ch_state_count; ++blank_states)
			{
				s = &((*ch_s)[blank_states]);
				blank = s->vol == SN_VOL_MIN;
				if (!blank) blank = s->tone == 0;

				if (!blank) break;
			}

			if (blank_states == *ch_state_count)
				*ch_state_count = 0;
		}
	}

	{
		bool blank = false;
		struct Sn76489NoiseState(*const ch_s)[STATES_MAX_COUNT] = &(logger->sn76489_noise_state);
		struct Sn76489NoiseState *s;

		uint32_t *ch_state_count = &(logger->sn76489_noise_state_count);
		{
			uint32_t blank_states = 0;
			for (; blank_states < *ch_state_count; ++blank_states)
			{
				s = &((*ch_s)[blank_states]);
				blank = s->vol == SN_VOL_MIN;

				if (!blank) break;
			}

			if (blank_states == *ch_state_count)
				*ch_state_count = 0;
		}
	}
} // void vgm_logger_remove_blank_chls()

static void vgm_logger_frames_to_deltas()
{
	uint32_t *lengths = malloc(sizeof(uint32_t) * (STATES_MAX_COUNT - 1));

	// YM2612 states
	for (uint32_t chl = 0; chl < STATES_YM_SIZE; ++chl)
	{
		struct Ym2612State(*ch_s)[STATES_MAX_COUNT] =
			&(logger.ym2612_state[chl]);

		for (uint32_t i = 1; i < logger.ym2612_state_count[chl]; ++i)
		{
			const struct Ym2612State *s = &((*ch_s)[i]);
			lengths[i - 1] = s->frames - ((s - 1)->frames);
		}

		for (uint32_t i = 1; i < logger.ym2612_state_count[chl]; ++i)
		{
			struct Ym2612State *s = &((*ch_s)[i]);
			s->frames = lengths[i - 1];
		}
	}

	// SN76489 general channel states
	for (uint32_t chl = 0; chl < STATES_SN_SIZE; ++chl)
	{
		struct Sn76489State(*ch_s)[STATES_MAX_COUNT] =
			&(logger.sn76489_state[chl]);

		for (uint32_t i = 1; i < logger.sn76489_state_count[chl]; ++i)
		{
			const struct Sn76489State *s = &((*ch_s)[i]);
			lengths[i - 1] = s->frames - ((s - 1)->frames);
		}

		for (uint32_t i = 1; i < logger.sn76489_state_count[chl]; ++i)
		{
			struct Sn76489State *s = &((*ch_s)[i]);
			s->frames = lengths[i - 1];
		}
	}

	//SN76489's noise states
	struct Sn76489NoiseState(*ch_s)[STATES_MAX_COUNT] =
		&(logger.sn76489_noise_state);

	for (uint32_t i = 1; i < logger.sn76489_noise_state_count; ++i)
	{
		const struct Sn76489NoiseState *s = &((*ch_s)[i]);
		lengths[i - 1] = s->frames - ((s - 1)->frames);
	}

	for (uint32_t i = 1; i < logger.sn76489_noise_state_count; ++i)
	{
		struct Sn76489NoiseState *s = &((*ch_s)[i]);
		s->frames = lengths[i - 1];
	}

	free(lengths);
}

void vgm_logger_apply_external_note_table(int16_t(*const note_table)[NOTE_TABLE_WIDTH][NOTES_IN_OCT])
{
	for (int_fast16_t y = 0; y < NOTE_TABLE_WIDTH; ++y)
		for (int_fast16_t x = 0; x < NOTES_IN_OCT; ++x)
			logger.note_table[y][x] = (*note_table)[y][x];
}

void vgm_parser_export_dac_samples(char *output_folder)
{
	bool dac_enabled = ym_enabled && ym_chls_enabled[YM_CHL_DAC];
	if (!dac_enabled) return;
	// Шаблон имени
	char dac_file_template[] = "dac_0000.snd";
	// На всякий случай...
	const size_t char_size = sizeof(dac_file_template[0]);
	// Количество символов в шаблоне, включая нулевой
	const int dac_file_template_len = ARRLENGTH(dac_file_template);
	// Длина входной строки...
	int old_len = strlen(output_folder);
	// Выделим память для новой строки, где будет полный путь
	char *output_file_name = 
		malloc((old_len + dac_file_template_len) * char_size);
	// Скопируем входную строку в выделенную, которую будет модифицировать
	memcpy(output_file_name, output_folder, old_len);
	// Удалим имя файла в конце строки
	path_remove_file_name(output_file_name);
	// Узнаем длину строки, содержащую только путь без имени файла
	size_t path_folder_len = strlen(output_file_name);
	// Устанавливаем указатель на начало имени файла
	char *dac_number_str = &(output_file_name[path_folder_len]);
	// Проходим по всем найденным сэмплам...
	for (size_t i = 0; i < parser.data_block_count; ++i)
	{
		struct DataBlock *const b = &(parser.data_block[i]);
		sprintf(dac_number_str, "dac_%04d.snd", i);
		FILE *f = fopen(output_file_name, "wb");
		if (!f)
		{
			printf("Error! DAC sample %04d did not saved!\n", i);
			continue;
		}
		fwrite(b->begin, b->size, 1, f);
		fclose(f);
	}
	// Освобождаем выделенную память
	free(output_file_name);
}