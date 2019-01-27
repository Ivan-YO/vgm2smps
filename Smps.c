#include "pch.h"
#include "Smps.h"
#include <byteswap.h>

// Magic
#define SMPS_NODAC 0x000900F2

#define errno_t int

////////////////////////////////////////////////////////////////////////////////
////////////////////   Tables and other global const arrays   //////////////////
////////////////////////////////////////////////////////////////////////////////

int16_t sn76489_tone_table[][NOTES_IN_OCT] =
{
    {
        0x356, 0x326, 0x2F9, 0x2CE, 0x2A5, 0x280,
        0x25C, 0x23A, 0x21A, 0x1FB, 0x1DF, 0x1C4
    },
    {
        0x1AB, 0x193, 0x17D, 0x167, 0x153, 0x140,
        0x12E, 0x11D, 0x10D, 0xFE, 0xEF, 0xE2
    },
    {
        0xD6, 0xC9, 0xBE, 0xB4, 0xA9, 0xA0,
        0x97, 0x8F, 0x87, 0x7F, 0x78, 0x71
    },
    {
        0x6B, 0x65, 0x5F, 0x5A, 0x55, 0x50,
        0x4B, 0x47, 0x43, 0x40, 0x3C, 0x39
    },
    {
        0x36, 0x33, 0x30, 0x2D, 0x2B, 0x28,
        0x26, 0x24, 0x22, 0x20, 0x1F, 0x1D
    },
    {
        0x1B, 0x1A, 0x18, 0x17, 0x16, 0x15,
        0x13, 0x12, 0x11, 0x00, 0x00, 0x00,
    }
};

////////////////////////////////////////////////////////////////////////////////
/////////////////////   Structures and types definitions   /////////////////////
////////////////////////////////////////////////////////////////////////////////

struct
{
    uint16_t   voice_ptr;
    uint8_t	   fm_cnt;
    uint8_t    psg_cnt;
    uint8_t    tempo_div;
    uint8_t    tempo_mod;

    union
    {
        uint16_t ptr;
        uint32_t full_val;
    } dac;

    struct
    {
        uint16_t chl_ptr;
        int8_t   pitch;
        uint8_t  vol;
    } fm[YM_CHLS];

    struct
    {
        uint16_t chl_ptr;
        int8_t   pitch;
        uint8_t  vol;
        uint16_t voice;
    } psg[SN_CHLS - 1];
} smps_header;

struct SmpsModulationState
{
    uint8_t wait;
    uint8_t speed;
    int8_t change;
    uint8_t step;
};

static struct
{
    int16_t		ins_index;
    int8_t		vol;
    int8_t		note_len;
    bool		key_state;
    int8_t		pitch;
    int8_t		pitch_prev;
    int16_t		pitch_raw;
    int16_t		pitch_raw_prev;
    bool		key_noattack;
    uint8_t		note;
    uint8_t		note_prev;
    struct SmpsModulationState mod_state;
    bool mod_enabled;
    bool mod_setuped;
    bool mod_manually_set;

    struct
    {
        struct VgmLogger *logger;
        struct VgmParser *parser;
        bool			loop;
        bool			loop_setuped;
        bool			loop_state_prepeared;
        uint16_t		blank_chl_ptr;
        uint32_t		ch_state_count;
        uint32_t		loop_frames;
        uint32_t		frame_count;
        uint16_t		loop_ptr;
        uint16_t		chl_ptr;
        bool			ym_dac_enabled;
        // For -altins flag
        int8_t			ym_state_notes[YM_CHLS][STATES_MAX_COUNT];
		int8_t			ym_state_notes_oct_index[YM_CHLS][STATES_MAX_COUNT];
		int8_t			ym_ins_oct_off[INS_MAX_COUNT];
    } global;

    struct
    {
        uint8_t		voice[SMPS_VOICE_SIZE];
        int8_t		pan;
        int8_t		ams;
        int8_t		fms;
        int16_t		freq;
        int8_t		freq_block;
        struct Ym2612State loop_state;
    } fm;

    struct
    {
        int16_t		tone;
        int8_t		ch4_mode;
        int8_t		noise_type;
        uint8_t		noise_form_val;
        struct Sn76489State loop_state;
        struct Sn76489NoiseState loop_noise_state;
    } psg;
} cd; // conversion data

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////   Local functions   /////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * Function:
 *   void smps_cd_zero_common()
 *
 * Action:
 *   Sets common fields of struct cd to zero.
 *
 * Arguments:
 *
 * Errors:
 *
 */
static inline void smps_cd_zero_common();

/**
 * Function:
 *   int smps_note_get_value(int note_index, int octave)
 *
 * Action:
 *   Converts note index and octave to corresponding SMPS note.
 *
 * Arguments:
 *   note_index -- note index [0-12]
 *   octave     -- octave
 *
 * Errors:
 */
int smps_note_get_value(int note_index, int octave);

/**
 * Function:
 *   void smps_create_voice(
 *       uint8_t(*const voice)[SMPS_VOICE_SIZE],
 *       const struct Ym2612Ins *const ins)
 *
 * Action:
 *   Converts FM instrument data to SMPS voice format and write that instrument
 *   into the array.
 *
 * Arguments:
 *   voice -- array to accomodate the voice
 *   ins   -- YM2612 instrument
 *
 * Errors:
 *
 */
void smps_create_voice(
    uint8_t(*const voice)[SMPS_VOICE_SIZE],
    const struct Ym2612Ins *const ins);

/**
 * Function:
 *   void smps_write_ym_vol(FILE *const f, const struct Ym2612State *const s)
 *
 * Action:
 *   Outputs volume to SMPS file.
 *
 * Arguments:
 *   f -- SMPS file
 *   s -- YM2612 current state
 *
 * Errors:
 *   I/O errors, E2BIG
 */
static void smps_write_ym_vol(FILE *const f, const struct Ym2612State *const s);

/**
 * Function:
 *   void smps_write_ym_pan(FILE *const f, const struct Ym2612State *const s)
 *
 * Action:
 *   Outputs panning into SMPS file.
 *
 * Arguments:
 *   f -- SMPS file
 *   s -- YM2612 current state
 *
 * Errors:
 *   I/O errors, E2BIG
 */
static void smps_write_ym_pan(
    FILE *const f,
    const struct Ym2612State *const s);

// Sorry, I'm too lazy to write the rest.
// Figure out by yourself, okay?

static void smps_write_ym_fms(
    FILE *const f,
    const struct Ym2612State *const s);

static void smps_write_ym_ins_index(
    FILE *const f,
    const struct Ym2612State *const s);

static void smps_write_ym_freq(
    FILE *const f,
    const struct Ym2612State *const s);

static inline void smps_write_ym_note_len_direct(
    FILE *const f,
    uint32_t note_len,
    bool is_dac);

static void smps_write_ym_note_len(
    FILE *const f,
    const int32_t smp,
    bool is_dac);

static void smps_write_sn_vol(
    FILE *const f,
    const int8_t vol);

static void smps_write_sn_tone(
    FILE *const f,
    const struct Sn76489State *const s,
    const struct Sn76489NoiseState *const ns);

static inline void smps_write_sn_note_len_direct(
    FILE *const f,
    int32_t note_len);

static void smps_write_ym_chls(FILE *const f);

static void smps_write_sn_note_len(FILE *const f, int32_t smp);

static void smps_write_sn_chls(FILE *const f);

static void smps_write_sn_noise(FILE *const f);

static void smps_write_ym_voices(FILE *const f);

static void smps_fms_to_mod(
    struct SmpsModulationState *const mod_state,
    int fms);

static inline void smps_write_ym_pitch(FILE *const f);

static inline void smps_get_ptr(FILE *const f, uint16_t *const ptr_var);

static inline void smps_cd_zero_common()
{
    cd.ins_index        = 0;
    cd.vol              = 0;
    cd.note_len         = 0;
    cd.key_state        = 0;
    cd.pitch            = 0;
    cd.pitch_prev       = 0;
    cd.pitch_raw        = 0;
    cd.pitch_raw_prev   = 0;
    cd.key_noattack     = 0;
    cd.note             = 0;
    cd.note_prev        = 0;
    cd.mod_enabled      = false;
    cd.mod_manually_set = false;
    cd.mod_setuped      = false;

    memset(&(cd.mod_state), 0, sizeof(cd.mod_state));
}

static void smps_fms_to_mod(
    struct SmpsModulationState *const mod_state,
    int fms)
{
    if (!mod_state)
    {
        //errno = EINVAL;
        return;
    }

    mod_state->wait = 0;
    mod_state->speed = 2;
    mod_state->step = 3;
    switch (fms &= 7)
    {
    case 0:
        memset(mod_state, 0, sizeof(*mod_state));
        break;
    case 1:
        mod_state->change = 1;
        break;
    case 2:
        mod_state->change = 2;
        break;
    case 3:
        mod_state->change = 3;
        break;
    case 4:
        mod_state->change = 3;
        break;
    case 5:
        mod_state->change = 4;
        break;
    case 6:
        mod_state->change = 7;
        break;
    case 7:
        mod_state->change = 16;
        break;
    }
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////   The rest of functions   //////////////////////////
////////////////////////////////////////////////////////////////////////////////

int smps_note_get_value(int note_index, int octave)
{
    int val = SMPS_NOTE_C1 + note_index + (octave) * NOTES_IN_OCT;
    while (val > SMPS_NOTE_A_8)
        val -= NOTES_IN_OCT;
    return val;
}

void smps_create_voice(
    uint8_t(*const voice)[SMPS_VOICE_SIZE],
    const struct Ym2612Ins *const ins)
{
    (*voice)[0] = (ins->feedback << 3) + ins->algorithm;

    for (int i = 1; i <= YM_OPS; ++i)
    {
        const struct Ym2612Op *op = &(ins->op[i - 1]);
        (*voice)[i]      = (op->dt << 4) + op->mul;
        (*voice)[i + 4]  = (op->rs << 6) + op->ar;
        (*voice)[i + 8]  = (op->am << 7) + op->d1r;
        (*voice)[i + 12] = op->d2r & 31;
        (*voice)[i + 16] = ((op->sl << 4) & 0xF0) + op->rr;
        (*voice)[i + 20] = op->tl;
    }
}

static void smps_write_ym_vol(FILE *const f, const struct Ym2612State *const s)
{
    int8_t dvol = 0;

    if (s->vol != NOCHANGE)
    {
        dvol = s->vol - cd.vol;
        cd.vol = s->vol;

        if (dvol != 0)
        {
            fputc(FSMPSALTERVOL, f);
            fputc(dvol, f);
        }
    }
}

static void smps_write_ym_pan(FILE *const f, const struct Ym2612State *const s)
{
//#define SMPS_GET_AMSFMS(_ams, _fms) ((((_ams) & 3) | (((_fms) & 7) << 3)) & 0xFF)
    int8_t *const pan = &(cd.fm.pan);
    /*//Alas, AMS and FMS are not supported in SMPS
    if (s->pan != NOCHANGE || s->ams != NOCHANGE || s->fms != NOCHANGE)
    {
        if (*pan == s->pan && *ams == s->ams && *fms == s->fms)
            return;
        *pan = s->pan;
        if (s->ams != NOCHANGE) *ams = s->ams;
        if (s->fms != NOCHANGE) *fms = s->fms;
        int amsfms = SMPS_GET_AMSFMS(*ams, *fms);
        fputc(FSMPSPAN, f);
        uint8_t pan_final = ((*pan & 3) << 6) | amsfms;
        fputc(pan_final, f);
    }
    */
    if (s->pan != NOCHANGE)
    {
        uint8_t amsfms = 0;
        *pan = s->pan;
        fputc(FSMPSPAN, f);
        uint8_t pan_final = ((*pan & 3) << 6) | amsfms;
        fputc(pan_final, f);
    }
//#undef SMPS_GET_AMSFMS
}

static void smps_write_ym_fms(FILE *const f, const struct Ym2612State *const s)
{
    int8_t *const fms = &(cd.fm.fms);
    struct SmpsModulationState *const mod_state = &(cd.mod_state);
    bool *const mod_enabled = &(cd.mod_enabled);
    bool *const mod_setuped = &(cd.mod_setuped);
    int8_t fms_prev = *fms;

    if (cd.mod_manually_set) return;
    if (s->fms == NOCHANGE) return;

    *fms = s->fms;

    if (*mod_setuped && s->fms != 0 && s->fms == fms_prev)
    {
        if (*mod_enabled) return;

        *mod_enabled = true;
        fputc(FSMPSMODON, f);
        return;
    }

    if (*mod_setuped && s->fms == 0)
    {
        *mod_enabled = false;
        fputc(FSMPSMODOFF, f);
        return;
    }

    if (!(*mod_setuped) && *fms == 0)
        return;

    smps_fms_to_mod(mod_state, s->fms);
    fputc(FSMPSMODSET, f);
    fputc(mod_state->wait, f);
    fputc(mod_state->speed, f);
    fputc(mod_state->change, f);
    fputc(mod_state->step, f);
    *mod_setuped = true;
}

static void smps_write_ym_ins_index(
    FILE *const f,
    const struct Ym2612State *const s)
{
    static int16_t ins_index_prev = 0;

    if (s->ins_index != NOCHANGE)
    {
        ins_index_prev = cd.ins_index;
        cd.ins_index = s->ins_index;

        if (cd.ins_index != ins_index_prev)
        {
            fputc(FSMPSFMVOICE, f);
            fputc(cd.ins_index, f);
        }
    }
}

static inline void smps_write_ym_pitch(FILE *const f)
{
    if (cd.pitch_raw != cd.pitch_raw_prev)
    {
        int abs_pitch  = abs(cd.pitch_raw);
        int pitch_sign = SIGN(cd.pitch_raw);
        // TODO: overflow handling
        bool pitch_overflow = abs_pitch > YM_VOL_MIN;
        abs_pitch %= 0x80;
        cd.pitch = abs_pitch * pitch_sign;
        fputc(FSMPSALTERNOTE, f);
        fputc(cd.pitch, f);
    }
}

static void smps_write_ym_freq(
    FILE *const f,
    const struct Ym2612State *const s)
{
    const struct VgmLogger *const logger = get_vgm_logger();
    const int16_t(*const note_table)[NOTE_TABLE_WIDTH][NOTES_IN_OCT] =
        &(logger->note_table);

    if (s->freq != NOCHANGE)
    {
        bool note_on = (cd.key_state == true && cd.vol != YM_VOL_MIN);
        if (ym_enable_keyoff_notes)
            note_on |= (cd.key_state == false
                     && s->freq != NOCHANGE
                     && s->key_state != false); // for Thunder Force and stuff

        if (note_on)
        {
            cd.fm.freq = s->freq;
            cd.fm.freq_block = s->freq_block;
            int_fast8_t oct;
            int_fast8_t oct_off;
            int_fast8_t note_index;

            ARRAY_2D_UNSORTED_FIND_NEAREST(
                int16_t,
                logger->note_table,
                cd.fm.freq,
                oct_off,
                note_index);

            if (cd.key_noattack)
                fputc(FSMPSNOATTACK, f);

            cd.pitch_prev = cd.pitch;
            cd.pitch_raw_prev = cd.pitch_raw;

            cd.pitch_raw = cd.fm.freq - (*note_table)[oct_off][note_index];
            smps_write_ym_pitch(f);

            oct_off -= NOTE_TABLE_MIDDLE;
            oct = (s->freq_block + oct_off);

            // TODO: ins octave offset
            if (oct < 0)
                oct = 0;

            oct &= 0xF;
            cd.note_prev = cd.note;
            cd.note = smps_note_get_value(note_index, oct);

            fputc(cd.note, f);
        } // if (note_on)
        else
        {
            fputc(SMPS_NOTE_OFF, f);
        }
    } // if (s->freq != NOCHANGE)
    else
    {
        if (cd.key_noattack)
        {
            fputc(FSMPSNOATTACK, f);
            fputc((cd.key_state ? cd.note : SMPS_NOTE_OFF), f);
        }
    }
}

static void smps_write_ym_freq_alternate(FILE *const f, const struct Ym2612State *const s, int chl, uint32_t state_index)
{
	const struct VgmLogger *const logger = get_vgm_logger();
	const int16_t(*const note_table)[NOTE_TABLE_WIDTH][NOTES_IN_OCT] =
		&(logger->note_table);
	int8_t(*const ym_state_notes)[YM_CHLS][STATES_MAX_COUNT] = &(cd.global.ym_state_notes);
	int8_t(*const ym_state_notes_oct_index)[YM_CHLS][STATES_MAX_COUNT] =
		&(cd.global.ym_state_notes_oct_index);
	int8_t(*const ym_ins_oct_off)[INS_MAX_COUNT] = &(cd.global.ym_ins_oct_off);
	int16_t ins_index = cd.ins_index != NOCHANGE ? cd.ins_index : 0;
	if (s->freq != NOCHANGE)
	{
		bool note_on = (cd.key_state == true && cd.vol != YM_VOL_MIN);
		if (ym_enable_keyoff_notes)
			note_on |= (cd.key_state == false && s->freq != NOCHANGE && s->key_state != false); // для тандерфорсаф

		if (note_on)
		{
			cd.fm.freq = s->freq;
			cd.fm.freq_block = s->freq_block;
			int8_t oct = (*ym_ins_oct_off)[ins_index] + cd.fm.freq_block;
			int8_t oct_off = (*ym_state_notes_oct_index)[chl][state_index];
			int8_t note_index = (*ym_state_notes)[chl][state_index];

			if (cd.key_noattack)
				fputc(FSMPSNOATTACK, f);

			cd.pitch_prev = cd.pitch;
			cd.pitch_raw_prev = cd.pitch_raw;
			cd.pitch_raw = cd.fm.freq - (*note_table)[oct_off][note_index];

			smps_write_ym_pitch(f);

			oct_off -= NOTE_TABLE_MIDDLE;
			oct += oct_off;

			if (oct < 0)
				assert(oct >= 0);

			cd.note_prev = cd.note;
			cd.note = smps_note_get_value(note_index, oct);

			assert(cd.note != 0);

			fputc(cd.note, f);
		}
		else
		{
			fputc(SMPS_NOTE_OFF, f);
		}
	} // if (s->freq != NOCHANGE)
	else
	{
		if (cd.key_noattack)
		{
			//note = SMPS_NOTE_OFF;
			fputc(FSMPSNOATTACK, f);
			fputc((cd.key_state ? cd.note : SMPS_NOTE_OFF), f);
		}
	}
}

static inline void smps_write_ym_note_len_direct(
    FILE *const f,
    uint32_t note_len,
    bool is_dac)
{
    if (note_len == 0) return;

    size_t len_over = (note_len - 1) / SMPS_NOTE_LEN_MAX;

    if (len_over > 0)
        fputc(SMPS_NOTE_LEN_MAX, f);

    for (size_t i = 1; i < len_over; ++i)
    {
        if (!is_dac)
        {
            if (cd.key_state)
                fputc(FSMPSNOATTACK, f);
        }
        else
        {
            if (cd.note != SMPS_NOTE_OFF)
            {
                cd.note = SMPS_NOTE_OFF;
                fputc(SMPS_NOTE_OFF, f);
            }
        }

        fputc(SMPS_NOTE_LEN_MAX, f);
    }

    note_len %= SMPS_NOTE_LEN_MAX;

    if (note_len == 0)
        note_len = SMPS_NOTE_LEN_MAX;

    cd.note_len = note_len;

    if (len_over > 0)
    {
        if (!is_dac)
        {
            if (cd.key_state)
                fputc(FSMPSNOATTACK, f);
        }
        else
        {
            cd.note = SMPS_NOTE_OFF;
            fputc(SMPS_NOTE_OFF, f);
        }
    }

    fputc(note_len, f);
}

static void smps_write_ym_note_len(
    FILE *const f,
    const int32_t smp,
    bool is_dac)
{
    const bool loop                   = cd.global.loop;
    bool *const loop_setuped          = &(cd.global.loop_setuped);
    const uint32_t *const frame_count = &(cd.global.frame_count);
    const uint32_t *const loop_frames = &(cd.global.loop_frames);
    uint16_t *const loop_ptr          = &(cd.global.loop_ptr);

    bool write_len             = false;
    bool write_len_needed      = false;
    bool loop_frame_overlapped = false;

    write_len_needed = cd.note_len != smp
                  || ((cd.note == cd.note_prev) && cd.key_noattack);

    write_len = write_len_needed;

    if (loop && !(*loop_setuped))
    {
        loop_frame_overlapped = *frame_count >= *loop_frames;
        write_len |= loop_frame_overlapped;
    }

    if (!write_len) return;

    int32_t sep_lens[2] =
    {
        (loop && !(*loop_setuped) &&
        loop_frame_overlapped ?
            (*frame_count - *loop_frames) : 0),
        smp - sep_lens[0]
    };

    if (loop && !(*loop_setuped) && loop_frame_overlapped)
    {
        assert(sep_lens[1] >= 0);
        bool has_second_len = sep_lens[0] != 0;
        *loop_setuped = true;

        if (has_second_len && sep_lens[0] != 0)
        {
            smps_write_ym_note_len_direct(f, sep_lens[1], is_dac);
            smps_get_ptr(f, loop_ptr);

            if (cd.key_state && !is_dac)
            {
                fputc(FSMPSNOATTACK, f);
                fputc(cd.note, f);
            }
            else
                fputc(SMPS_NOTE_OFF, f);

            smps_write_ym_note_len_direct(f, sep_lens[0], is_dac);
        }
        else
        {
            if (write_len_needed)
                smps_write_ym_note_len_direct(f, sep_lens[1], is_dac);
            *loop_ptr = ftell(f);

        }
    } // if (loop && !(*loop_setuped) && loop_frame_overlapped)
    else
    {
        smps_write_ym_note_len_direct(f, sep_lens[1], is_dac);
    }
}

static void smps_write_sn_vol(FILE *const f, const int8_t vol)
{
    int8_t dvol = 0;

    if (vol != NOCHANGE)
    {
        if (vol == SN_VOL_MIN) return;
        dvol = vol - cd.vol;
        if (dvol == 0)
            return;
        cd.vol = vol;
        if (vol != SN_VOL_MIN)
        {
            fputc(FSMPSSETVOL, f);
            fputc(dvol, f);
        }
    }
}

static void smps_write_sn_tone(FILE *const f,
    const struct Sn76489State *const s,
    const struct Sn76489NoiseState *const ns)
{
    int16_t	tone = ((!ns) ? s->tone : ns->tone);

    bool full_note_range_mode = true;
    if (ns)
        full_note_range_mode = *((ns->ch4_mode != NOCHANGE) ?
            &(ns->ch4_mode) : &(cd.psg.ch4_mode)) == SN_CH4_MODE_CH3NOCOMPATIBLE;
    //full_note_range_mode = ((ns->ch4_mode != NOCHANGE) ?
    //	(ns->ch4_mode == SN_CH4_MODE_CH3NOCOMPATIBLE) :
    //	(cd.psg.ch4_mode == SN_CH4_MODE_CH3NOCOMPATIBLE));
    uint8_t psg_form_val = 0xE0;
    static bool key_state_prev = false;

    if (ns)
    {
        bool psg_form_changed = false;
        if (ns->ch4_mode != NOCHANGE || ns->noise_type != NOCHANGE)
        {
            if (full_note_range_mode)
                psg_form_val = (ns->noise_type == SN_NOISE_PERIODIC
                    ? 0xE3
                    : 0xE7);
            else
                psg_form_val +=
                    (4 * ((cd.psg.noise_type == SN_NOISE_WHITE) & 1));

            psg_form_changed = true;

            if (ns->noise_type != NOCHANGE)
                cd.psg.noise_type = ns->noise_type;

            if (ns->ch4_mode != NOCHANGE)
                cd.psg.ch4_mode = ns->ch4_mode;
        }

        if (full_note_range_mode && psg_form_changed)
        {
            fputc(FSMPSPSGFORM, f);
            fputc(psg_form_val, f);
        }
    }

    if (tone != NOCHANGE)
    {
        int_fast8_t oct;
        int_fast8_t note_index;
        cd.psg.tone = tone;

        if (full_note_range_mode)
        {
            ARRAY_2D_SORTED_FIND_NEAREST(
                int16_t,
                sn76489_tone_table,
                tone,
                oct,
                note_index);

            cd.pitch_prev = cd.pitch;
            cd.pitch_raw_prev = cd.pitch_raw;

            cd.pitch_raw = tone - sn76489_tone_table[oct][note_index];
            if (cd.pitch_raw != cd.pitch_raw_prev)
            {
                int abs_pitch = abs(cd.pitch_raw);
                int pitch_sign = SIGN(cd.pitch_raw);
                // TODO: overflow handling
                bool pitch_overflow = abs_pitch > 0x7F;
                abs_pitch %= 0x80;
                cd.pitch = abs_pitch * pitch_sign;
            }
        }

        cd.note_prev = cd.note;
        cd.note = full_note_range_mode ?
            smps_note_get_value(note_index, oct) : (int8_t)tone;
    }

    if (cd.key_state)
    {
        if (full_note_range_mode)
        {
            if (cd.pitch_raw != cd.pitch_raw_prev)
            {
                fputc(FSMPSALTERNOTE, f);
                fputc(cd.pitch, f);
            }
            fputc(cd.note, f);
        }
        else
        {
            if (tone != NOCHANGE)
            {
                psg_form_val += tone & 3;
                fputc(FSMPSPSGFORM, f);
                fputc(psg_form_val, f);
                if (!key_state_prev)
                {
                    cd.note = SMPS_NOTE_C1;
                    fputc(SMPS_NOTE_C1, f);
                }
            }
        }
    }
    else
    {
        fputc(SMPS_NOTE_OFF, f);
    }

    if (ns)
        cd.psg.noise_form_val = psg_form_val;

    key_state_prev = cd.key_state;
}

static inline void smps_write_sn_note_len_direct(
    FILE *const f,
    int32_t note_len)
{
    assert(note_len >= 0);

    size_t len_over = (note_len - 1) / SMPS_NOTE_LEN_MAX;

    if (len_over > 0)
        fputc(SMPS_NOTE_LEN_MAX, f);

    for (size_t i = 1; i < len_over; ++i)
        fputc(SMPS_NOTE_LEN_MAX, f);

    note_len %= SMPS_NOTE_LEN_MAX;

    if (note_len == 0)
        note_len = SMPS_NOTE_LEN_MAX;
    cd.note_len = note_len;
    fputc(note_len, f);
}

static inline void smps_sn_set_key_state(
    const struct Sn76489State *const s,
    const struct Sn76489NoiseState *const ns)
{
    int8_t vol   = ((ns == NULL) ? s->vol  : ns->vol);
    int16_t tone = ((ns == NULL) ? s->tone : ns->tone);

    if (vol != NOCHANGE)
        cd.key_state = vol != SN_VOL_MIN;

    if (!ns && cd.key_state)
        cd.key_state = ((tone == NOCHANGE) ? (cd.psg.tone != 0) : (tone != 0));
}

static void smps_write_ym_chls(FILE *const f)
{
    errno_t errno_prev                   = errno;
    const bool loop                      = cd.global.loop;
    bool *const loop_state_prepeared     = &(cd.global.loop_state_prepeared);
    bool *const loop_setuped             = &(cd.global.loop_setuped);
    struct Ym2612State *const loop_state = &(cd.fm.loop_state);
    uint32_t *const frame_count          = &(cd.global.frame_count);
    const uint32_t *const loop_frames    = &(cd.global.loop_frames);
    uint16_t *const loop_ptr             = &(cd.global.loop_ptr);
    uint16_t *const chl_ptr              = &(cd.global.chl_ptr);
    uint16_t *const blank_chl_ptr        = &(cd.global.blank_chl_ptr);
    uint32_t *const ch_state_count       = &(cd.global.ch_state_count);
    const bool *const dac_enabled        = &(cd.global.ym_dac_enabled);

    struct Ym2612State default_state;

    memset(&default_state, 0, sizeof(default_state));
    default_state.pan = YM_PAN_DEFAULT;

    const struct VgmLogger *const logger = cd.global.logger;

    // Подготовить ноты, инструменты, октавные смещения для них
	if (alt_ins)
	{
		int8_t(*const ym_state_notes)[YM_CHLS][STATES_MAX_COUNT] = &(cd.global.ym_state_notes);
		int8_t(*const ym_ins_oct_off)[INS_MAX_COUNT] = &(cd.global.ym_ins_oct_off);
		int8_t(*const ym_state_notes_oct_index)[YM_CHLS][STATES_MAX_COUNT] = &(cd.global.ym_state_notes_oct_index);
		const int16_t(*const note_table)[NOTE_TABLE_WIDTH][NOTES_IN_OCT] =
			&(logger->note_table);

		for (int chl = 0; chl < YM_CHLS; ++chl)
		{
			int16_t ins_index = 0;
			int16_t freq = 0;
			int8_t freq_block = 0;
			const struct Ym2612State(*ch_s)[STATES_MAX_COUNT] =
				&(logger->ym2612_state[chl]);
			int8_t oct = 0;
			int8_t oct_off = 0;
			int8_t note_index = 0;

			for (uint32_t i = 0; i < logger->ym2612_state_count[chl]; ++i)
			{
				const struct Ym2612State *s = &((*ch_s)[i]);
				int8_t *s_note = &((*ym_state_notes)[chl][i]);
				int8_t *s_oct_off = &((*ym_state_notes_oct_index)[chl][i]);


				if (s->freq != NOCHANGE)
				{
					freq = s->freq;
					freq_block = s->freq_block;

					ARRAY_2D_UNSORTED_FIND_NEAREST(
                        int16_t,
                        logger->note_table,
                        freq,
                        *s_oct_off,
                        *s_note);
					oct_off = *s_oct_off;
					oct_off -= NOTE_TABLE_MIDDLE;
					oct = (freq_block + oct_off);
				}

				if (s->ins_index != NOCHANGE)
					ins_index = s->ins_index;

				if (oct >= 0) continue;

				oct = -oct;
				//oct = abs(oct);
				if (oct > (*ym_ins_oct_off)[ins_index])
					(*ym_ins_oct_off)[ins_index] = oct;

			}
		}

		for (int i = 0; i < logger->ins_count; ++i)
		{
			struct Ym2612Ins *const ins = &(logger->ins[i]);
			if ((*ym_ins_oct_off)[i] == 0) continue;
			for (int j = 0; j < YM_OPS; ++j)
				ins->op[j].mul /= (1 << (*ym_ins_oct_off)[i]);
		}
	}

    // Go through all FM channels
    for (int chl = 0; chl < YM_CHLS; ++chl)
    {
        int16_t loop_pitch_raw = 0;

        if (*dac_enabled && (chl == (YM_CHLS - 1)))
            continue;

        *frame_count          = 0;
        *loop_ptr             = 0;
        *chl_ptr              = 0;
        *loop_setuped         = false;
        *loop_state_prepeared = false;

        *ch_state_count = logger->ym2612_state_count[chl];

        const struct Ym2612State(*ch_s)[STATES_MAX_COUNT] =
            &(logger->ym2612_state[chl]);

        if (*ch_state_count != 0 && ym_chls_enabled[chl]
            && ym_enabled)
            smps_get_ptr(f, chl_ptr);
        else
            *chl_ptr = *blank_chl_ptr;

        if (errno != errno_prev)
            return;

        smps_header.fm[chl].chl_ptr = __bswap_16(*chl_ptr);

        if (!(ym_enabled && ym_chls_enabled[chl])) continue;

        memset(&(cd.fm), 0, sizeof(cd.fm));
        smps_cd_zero_common();

        cd.fm.pan    = YM_PAN_DEFAULT;
        cd.ins_index = NOCHANGE;
        cd.note      = SMPS_NOTE_OFF;
        cd.note_prev = SMPS_NOTE_OFF;

        for (uint32_t i = 0; i < *ch_state_count; ++i)
        {
            const struct Ym2612State *s = &((*ch_s)[i]);

            if (s->frames == 0) continue;

            if (s->key_noattack != NOCHANGE)
            {
                /* Workaround for AMS false trigger.

                   Since there is no AMS implementation yet, we should not count
                   AMS change for an instrument change.
                 */
                bool ams_only_changed =
                    s->ams       != NOCHANGE
                 && s->freq      == NOCHANGE
                 && s->pan       == NOCHANGE
                 && s->fms       == NOCHANGE
                 && s->ins_index == NOCHANGE
                 && s->key_state == NOCHANGE
                 && s->vol       == NOCHANGE;

                if (!ams_only_changed)
                    cd.key_noattack = s->key_noattack;
            }

            if (s->key_state != NOCHANGE)
                cd.key_state = s->key_state;

            *frame_count += s->frames;

            smps_write_ym_ins_index(f, s);
            smps_write_ym_vol(f, s);
            smps_write_ym_pan(f, s);
            smps_write_ym_fms(f, s);

            if (!alt_ins)
                smps_write_ym_freq(f, s);
            else
                smps_write_ym_freq_alternate(f, s, chl, i);

            if (loop && !(*loop_state_prepeared) && (*frame_count >= *loop_frames))
            {
                if (*loop_frames == 0)
                {
                    *loop_ptr = *chl_ptr;
                    *loop_setuped = true;
                    *loop_state = default_state;
                }
                else
                {
                    loop_state->vol          = cd.vol;
                    loop_state->ams          = cd.fm.ams;
                    loop_state->fms          = cd.fm.fms;
                    loop_state->freq         = cd.fm.freq;
                    loop_state->freq_block   = cd.fm.freq_block;
                    loop_state->ins_index    = cd.ins_index;
                    loop_state->key_noattack = cd.key_noattack;
                    loop_state->key_state    = cd.key_state;
                    loop_state->pan          = cd.fm.pan;
                    loop_state->frames       = 0;
                    loop_pitch_raw           = cd.pitch_raw;
                }

                *loop_state_prepeared = true;
            }

            smps_write_ym_note_len(f, s->frames, false);

            if (errno != errno_prev)
                return;
        } // for (uint32_t i = 0; i < ch_states_count; ++i)

        if (*chl_ptr != *blank_chl_ptr)
        {
            if (loop)
            {
                smps_write_ym_ins_index(f, loop_state);
                smps_write_ym_vol(f, loop_state);
                smps_write_ym_pan(f, loop_state);
                smps_write_ym_fms(f, loop_state);

                cd.pitch_raw = loop_pitch_raw;
                smps_write_ym_pitch(f);

                fputc(FSMPSJUMP, f);
                *loop_ptr = (*loop_ptr - ftell(f) - 1);
                *loop_ptr = __bswap_16(*loop_ptr);
                fwrite(loop_ptr, sizeof(*loop_ptr), 1, f);
            } // if (loop)
            else
            {
                fputc(FSMPSSTOP, f);
            }
        } // if (*chl_ptr != *blank_chl_ptr)
    } // for (int chl = 0; chl < YM_CHLS; ++chl)
}

static inline void smps_write_sn_note_len(FILE *const f, int32_t smp)
{
    const bool loop                   = cd.global.loop;
    bool *const loop_setuped          = &(cd.global.loop_setuped);
    const uint32_t *const frame_count = &(cd.global.frame_count);
    const uint32_t *const loop_frames = &(cd.global.loop_frames);
    uint16_t *const loop_ptr          = &(cd.global.loop_ptr);

    bool write_len = false;
    bool write_len_needed = false;
    bool loop_frame_overlapped = false;

    write_len_needed = cd.note_len != smp;
    write_len = write_len_needed;

    if (loop && !(*loop_setuped))
    {
        loop_frame_overlapped = *frame_count >= *loop_frames;
        write_len |= loop_frame_overlapped;
    }

    if (!write_len) return;

    int32_t sep_lens[2] = { 0, smp - sep_lens[0] };

    if (loop && !(*loop_setuped) && loop_frame_overlapped)
    {
        /**
         * std282: переместил часть инициализации sep_lens[] сюда. Условие для
         * sep_lens[0] и внутри if одно и то же.
         *
         * Рекомендую избегать длинных конструкций с ?:
         */
        sep_lens[0] = *frame_count - *loop_frames;

        assert(sep_lens[1] >= 0);
        bool has_second_len = sep_lens[0] != 0;
        *loop_setuped = true;

        if (has_second_len && sep_lens[0] != 0)
        {
            smps_write_sn_note_len_direct(f, sep_lens[1]);
            smps_get_ptr(f, loop_ptr);
            fputc(cd.key_state ? cd.note : SMPS_NOTE_OFF, f);
            smps_write_sn_note_len_direct(f, sep_lens[0]);
        }
        else
        {
            if (write_len_needed)
                smps_write_sn_note_len_direct(f, sep_lens[1]);

            if (*loop_ptr == 0)
                *loop_ptr = ftell(f);
        }
    } // if (loop && !(*loop_setuped) && loop_frame_overlapped)
    else
    {
        smps_write_sn_note_len_direct(f, sep_lens[1]);
    }
} // static inline void smps_write_sn_note_len(FILE *const f, int32_t smp)

static void smps_write_sn_chls(FILE *const f)
{
    errno_t errno_prev                    = errno;
    const bool loop                       = cd.global.loop;
    bool *const loop_state_prepeared      = &(cd.global.loop_state_prepeared);
    bool *const loop_setuped              = &(cd.global.loop_setuped);
    struct Sn76489State *const loop_state = &(cd.psg.loop_state);
    uint32_t *const frame_count           = &(cd.global.frame_count);
    const uint32_t *const loop_frames     = &(cd.global.loop_frames);
    uint16_t *const loop_ptr              = &(cd.global.loop_ptr);
    uint16_t *const chl_ptr               = &(cd.global.chl_ptr);
    uint16_t *const blank_chl_ptr         = &(cd.global.blank_chl_ptr);
    //bool(*const chl_enabled)[SN_CHLS] = &(cd.global.psg.chl_enabled);
    //bool *const enabled = &(cd.global.psg.enabled);
    uint32_t *const ch_state_count        = &(cd.global.ch_state_count);
    const struct VgmLogger *const logger  = cd.global.logger;
    struct Sn76489State default_state     = { 0 };

    for (int chl = 0; chl < SN_CHLS - 1; ++chl)
    {
        *frame_count          = 0;
        *loop_ptr             = 0;
        *chl_ptr              = 0;
        *loop_setuped         = false;
        *loop_state_prepeared = false;

        *ch_state_count = logger->sn76489_state_count[chl];

        const struct Sn76489State(*ch_s)[STATES_MAX_COUNT] =
            &(logger->sn76489_state[chl]);

        //*chl_ptr = (*ch_state_count != 0 && sn_chls_enabled[chl]
        //	&& sn_enabled) ? off : *blank_chl_ptr;
        if (*ch_state_count != 0 && sn_chls_enabled[chl]
            && sn_enabled)
            smps_get_ptr(f, chl_ptr);
        else
            *chl_ptr = *blank_chl_ptr;

        if (errno != errno_prev)
            return;

        smps_header.psg[chl].chl_ptr = __bswap_16(*chl_ptr);

        if (!(sn_enabled && sn_chls_enabled[chl])) continue;

        memset(&(cd.psg), 0, sizeof(cd.psg));
        smps_cd_zero_common();

        cd.note      = SMPS_NOTE_OFF;
        cd.note_prev = SMPS_NOTE_OFF;

        for (uint32_t i = 0; i < *ch_state_count; ++i)
        {
            const struct Sn76489State *s = &((*ch_s)[i]);
            if (s->frames == 0) continue;

            *frame_count += s->frames;
            if (loop && !(*loop_state_prepeared) && *frame_count >= *loop_frames)
            {
                if (*loop_frames == 0)
                {
                    *loop_state   = default_state;
                    *loop_ptr     = *chl_ptr;
                    *loop_setuped = true;
                }

                loop_state->vol    = cd.vol;
                loop_state->tone   = cd.psg.tone;
                loop_state->frames = 0;

                *loop_state_prepeared = true;
            }

            smps_sn_set_key_state(s, NULL);
            smps_write_sn_vol(f, s->vol);
            smps_write_sn_tone(f, s, NULL);
            smps_write_sn_note_len(f, s->frames);

            if (errno != errno_prev)
                return;
        } // for (uint32_t i = 0; i < *ch_state_count; ++i)

        if (*chl_ptr != *blank_chl_ptr)
        {
            if (loop)
            {
                smps_sn_set_key_state(loop_state, NULL);
                smps_write_sn_vol(f, loop_state->vol);

                fputc(FSMPSJUMP, f);
                *loop_ptr = (*loop_ptr - ftell(f) - 1);
                *loop_ptr = __bswap_16(*loop_ptr);
                fwrite(loop_ptr, sizeof(*loop_ptr), 1, f);
            } /* if (loop) */
            else
            {
                fputc(FSMPSSTOP, f);
            }
        } // if (*chl_ptr != *blank_chl_ptr)
    } // for (int chl = 0; chl < SN_CHLS - 1; ++chl)
} // static void smps_write_sn_chls(FILE *const f)

static void smps_write_sn_noise(FILE *const f)
{
    errno_t errno_prev                         = errno;
    int chl                                    = SN_CHL_NOISE;
    long off                                   = ftell(f); // см. ниже
    const bool loop                            = cd.global.loop;
    bool *const loop_state_prepeared           = &(cd.global.loop_state_prepeared);
    bool *const loop_setuped                   = &(cd.global.loop_setuped);
    struct Sn76489NoiseState *const loop_state = &(cd.psg.loop_noise_state);
    uint32_t *const frame_count                = &(cd.global.frame_count);
    const uint32_t *const loop_frames          = &(cd.global.loop_frames);
    uint16_t *const loop_ptr                   = &(cd.global.loop_ptr);
    uint16_t *const chl_ptr                    = &(cd.global.chl_ptr);
    uint16_t *const blank_chl_ptr              = &(cd.global.blank_chl_ptr);
    uint32_t *const ch_state_count             = &(cd.global.ch_state_count);
    const struct VgmLogger *const logger       = cd.global.logger;
    struct Sn76489NoiseState default_state     = { 0 };

    /**
     * std282: Было:
     *     long off = 0;
     *     off = ftell(f);
     *
     * Есть ли этому разумное объяснение?
     */

    *frame_count          = 0;
    *loop_ptr             = 0;
    *chl_ptr              = 0;
    *loop_setuped         = false;
    *loop_state_prepeared = false;

    *ch_state_count = logger->sn76489_noise_state_count;
    const struct Sn76489NoiseState(*ch_s)[STATES_MAX_COUNT] =
        &(logger->sn76489_noise_state);

    if (*ch_state_count != 0 && sn_chls_enabled[chl] && sn_enabled)
        smps_get_ptr(f, chl_ptr);
    else
        *chl_ptr = *blank_chl_ptr;

    if (errno != errno_prev)
        return;

    smps_header.psg[2].chl_ptr = __bswap_16(*chl_ptr);

    if (!(sn_enabled && sn_chls_enabled[chl])) return;

    memset(&(cd.psg), 0, sizeof(cd.psg));
    smps_cd_zero_common();

    cd.note      = SMPS_NOTE_OFF;
    cd.note_prev = SMPS_NOTE_OFF;

    for (uint32_t i = 0; i < *ch_state_count; ++i)
    {
        const struct Sn76489NoiseState *s = &((*ch_s)[i]);
        if (s->frames == 0) continue;

        *frame_count += s->frames;

        if (loop && !(*loop_state_prepeared) && (*frame_count >= *loop_frames))
        {
            if (*loop_frames == 0)
            {
                *loop_state   = default_state;
                *loop_ptr     = *chl_ptr;
                *loop_setuped = true;
            }

            loop_state->vol        = cd.vol;
            loop_state->tone       = cd.psg.tone;
            loop_state->ch4_mode   = cd.psg.ch4_mode;
            loop_state->noise_type = cd.psg.noise_type;
            loop_state->frames     = 0;

            *loop_state_prepeared = true;
        }


        smps_sn_set_key_state(NULL, s);
        smps_write_sn_vol(f, s->vol);
        smps_write_sn_tone(f, NULL, s);
        smps_write_sn_note_len(f, s->frames);
    }

    if (errno != errno_prev)
        return;

    if (*chl_ptr != *blank_chl_ptr)
    {
        if (loop)
        {
            smps_sn_set_key_state(NULL, loop_state);
            smps_write_sn_vol(f, loop_state->vol);

            fputc(FSMPSJUMP, f);
            *loop_ptr = (*loop_ptr - ftell(f) - 1);
            *loop_ptr = __bswap_16(*loop_ptr);
            fwrite(loop_ptr, sizeof(*loop_ptr), 1, f);
        }
        else
        {
            fputc(FSMPSSTOP, f);
        }
    }
}

static void smps_write_ym_voices(FILE *const f)
{
    errno_t errno_prev                     = errno;
    uint8_t(*const voice)[SMPS_VOICE_SIZE] = &(cd.fm.voice);
    const struct VgmLogger *const logger   = cd.global.logger;

    if (ym_enabled)
    {
        smps_get_ptr(f, &(smps_header.voice_ptr));

        if (errno != errno_prev)
            return;

        smps_header.voice_ptr = __bswap_16(smps_header.voice_ptr);

        for (int i = 0; i < logger->ins_count; ++i)
        {
            smps_create_voice(voice, &(logger->ins[i]));
            fwrite(*voice, SMPS_VOICE_SIZE, 1, f);
        }
    } // if (ym_enabled)
} // static void smps_write_ym_voices(FILE *const f)

void smps_write_dac(FILE *const f)
{
    errno_t errno_prev                   = errno;
    const bool loop                      = cd.global.loop;
    bool *const loop_state_prepeared     = &(cd.global.loop_state_prepeared);
    bool *const loop_setuped             = &(cd.global.loop_setuped);
    struct Ym2612State *const loop_state = &(cd.fm.loop_state);
    uint32_t *const frame_count          = &(cd.global.frame_count);
    const uint32_t *const loop_frames    = &(cd.global.loop_frames);
    uint16_t *const loop_ptr             = &(cd.global.loop_ptr);
    uint16_t *const chl_ptr              = &(cd.global.chl_ptr);
    uint16_t *const blank_chl_ptr        = &(cd.global.blank_chl_ptr);
    uint32_t *const ch_state_count       = &(cd.global.ch_state_count);
    const bool *const dac_enabled        = &(cd.global.ym_dac_enabled);

    const struct VgmLogger *const logger = cd.global.logger;
    int32_t chl = YM_CHL_DAC;

    if (!ym_enabled) return;
    if (!(*dac_enabled)) return;

    *frame_count          = 0;
    *loop_ptr             = 0;
    *chl_ptr              = 0;
    *loop_setuped         = false;
    *loop_state_prepeared = false;

    *ch_state_count = logger->ym2612_state_count[chl];

    const struct Ym2612State(*ch_s)[STATES_MAX_COUNT] =
        &(logger->ym2612_state[chl]);

    if (*ch_state_count != 0 && ym_chls_enabled[chl] && ym_enabled)
        smps_get_ptr(f, chl_ptr);
    else
        *chl_ptr = *blank_chl_ptr;

    if (errno != errno_prev)
        return;

    smps_header.dac.ptr = __bswap_16(*chl_ptr);

    if (!(ym_enabled && ym_chls_enabled[chl])) return;

    for (uint32_t i = 0; i < *ch_state_count; ++i)
    {
        const struct Ym2612State *s = &((*ch_s)[i]);

        if (s->frames == 0) continue;
        *frame_count += s->frames;

        cd.note_prev = cd.note;
        cd.note      = 0x80 + s->freq;
        fputc(cd.note, f);

        if (loop && !(*loop_state_prepeared) && (*frame_count >= *loop_frames))
        {
            if (*loop_frames == 0)
            {
                *loop_ptr     = *chl_ptr;
                *loop_setuped = true;
            }

            loop_state->vol          = cd.vol;
            loop_state->ams          = cd.fm.ams;
            loop_state->fms          = cd.fm.fms;
            loop_state->freq         = cd.fm.freq;
            loop_state->freq_block   = cd.fm.freq_block;
            loop_state->ins_index    = cd.ins_index;
            loop_state->key_noattack = cd.key_noattack;
            loop_state->key_state    = cd.key_state;
            loop_state->pan          = cd.fm.pan;
            loop_state->frames       = 0;

            *loop_state_prepeared = true;
        } // if (loop && !(*loop_state_prepeared) && (*frame_count >= *loop_frames))

        smps_write_ym_note_len(f, s->frames, true);

        if (errno != errno_prev)
            return;
    } // for (uint32_t i = 0; i < *ch_state_count; ++i)

    if (*chl_ptr != *blank_chl_ptr)
    {
        if (loop)
        {
            fputc(FSMPSJUMP, f);
            *loop_ptr = (*loop_ptr - ftell(f) - 1);
            *loop_ptr = __bswap_16(*loop_ptr);
            fwrite(loop_ptr, sizeof(*loop_ptr), 1, f);
        } // if (loop)
        else
        {
            fputc(FSMPSSTOP, f);
        }
    } // if (*chl_ptr != *blank_chl_ptr)
}

void smps_export_to_file(const char *const file_path)
{
    bool *const loop              = &(cd.global.loop);
    uint32_t *const loop_frames   = &(cd.global.loop_frames);
    uint16_t *const blank_chl_ptr = &(cd.global.blank_chl_ptr);
    bool *const dac_enabled       = &(cd.global.ym_dac_enabled);

    errno_t errno_prev = errno;
    FILE *f = fopen(file_path, "wb");
    if (errno != errno_prev) return;

    memset(&cd, 0, sizeof(cd));

    cd.global.logger = get_vgm_logger();
    cd.global.parser = get_vgm_parser();

    struct VgmLogger *const logger = cd.global.logger;
    struct VgmParser *const parser = cd.global.parser;

    if (sn_enabled)
        sn_chls_enabled[
            (logger->sn76489_noise_state_count != 0) ? 2 : 3] = false;

    if (ym_enabled)
        *dac_enabled = parser->ym2612.dac_enabled;

    *loop = parser->vgm->vgm_header->loop_samples != 0;

    if (*loop)
        *loop_frames =
            (
                parser->vgm->vgm_header->total_samples
              - parser->vgm->vgm_header->loop_samples
            ) / logger->samples_per_frame;

    uint16_t smps_header_size = sizeof(smps_header);

    if (*dac_enabled)
        smps_header_size -= sizeof(smps_header.fm[0]);

    memset(&smps_header, 0, smps_header_size);

    smps_header.fm_cnt    = *dac_enabled ? 6 : 7;
    smps_header.psg_cnt   = 3;
    smps_header.tempo_div = 1;

    if (!(*dac_enabled))
        smps_header.dac.full_val = __bswap_32(SMPS_NODAC);

    fseek(f, smps_header_size, SEEK_SET);

    // Сюда будут ссылаться пустые каналы
    *blank_chl_ptr = ftell(f);
    fputc(FSMPSSTOP, f);

    smps_write_ym_chls(f);

    if (errno != errno_prev)
        goto end;

    smps_write_sn_chls(f);

    if (errno != errno_prev)
        goto end;

    smps_write_sn_noise(f);

    if (errno != errno_prev)
        goto end;

    smps_write_dac(f);

    if (errno != errno_prev)
        goto end;

    smps_write_ym_voices(f);

    if (errno != errno_prev)
        goto end;

    fseek(f, 0, SEEK_SET);


    fwrite(&(smps_header.voice_ptr), sizeof(smps_header.voice_ptr), 1, f);
    fputc(smps_header.fm_cnt, f);
    fputc(smps_header.psg_cnt, f);
    fputc(smps_header.tempo_div, f);
    fputc(smps_header.tempo_mod, f);
    fwrite(&(smps_header.dac), sizeof(smps_header.dac), 1, f);
    fwrite(smps_header.fm, sizeof(smps_header.fm[0]),
        (*dac_enabled) ? YM_CHLS - 1 : YM_CHLS, f);
    fwrite(smps_header.psg, sizeof(smps_header.psg), 1, f);
end:
    fclose(f);
} // void smps_export_to_file(const char *const file_path)

static inline void smps_get_ptr(FILE *const f, uint16_t *const ptr_var)
{
    int32_t ptr = ftell(f);

    if (ptr > SMPS_PTR_MAX)
    {
        errno = E2BIG;
        return;
    }

    *ptr_var = ptr;
}
