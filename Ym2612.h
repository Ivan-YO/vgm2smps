#ifndef _YM2612_H
#define _YM2612_H
#include "pch.h"

#define YM_CHLS 6
#define YM_CHL_DAC (YM_CHLS - 1)
#define YM_OPS 4

enum
{
	YM_PAN_NONE,
	YM_PAN_RIGHT,
	YM_PAN_LEFT,
	YM_PAN_CENTRE
};

#define YM_PAN_DEFAULT YM_PAN_CENTRE
#define YM_VOL_DEFAULT 0
#define YM_FREQ_DEFAULT 0
#define YM_FREQ_BLOCK_DEFAULT 0
#define YM_KEY_STATE_DEFAULT false
#define YM_VOL_MIN 0x7F

struct Ym2612Op
{
  uint8_t   dt;    /* detune */
  uint8_t   mul;   /* multiple */
  int8_t   tl;     /* total level */
  uint8_t   rs;    /* rate scaling */
  uint8_t   ar;    /* attack rate */
  uint8_t   d1r;   /* first decay rate */
  bool am;         /* amplitude modulation */
  uint8_t   d2r;   /* secondary decay rate */
  uint8_t   sl;    /* secondary amplitude/sustain level */
  uint8_t   rr;    /* release rate */
  uint8_t   ssgeg; /* SSGEG */
};

struct Ym2612Ins
{
  uint8_t feedback;
  uint8_t algorithm;
  struct Ym2612Op op[YM_OPS];
};

struct Ym2612Chl
{
  struct Ym2612Ins ins;
  int8_t           pan;
  int8_t           ams;
  int8_t           fms;
  bool             op_enabled[YM_OPS];
  int16_t          freq[YM_OPS];
  uint8_t          freq_block[YM_OPS];
};

struct Ym2612
{
  struct Ym2612Chl chls[YM_CHLS];
  uint32_t         clock;
  bool             dac_enabled;
  bool             lfo_enabled;
  uint8_t          lfo_freq;
  bool             ch3_spec;
  /* callbacks */
  void (*on_set_freq)(const struct Ym2612 *const ym2612, int chl);
  void (*on_set_ins)(const struct Ym2612 *const ym2612, int chl, int op);
  void (*on_set_key_state)(const struct Ym2612 *const ym2612, int chl);
  void (*on_key_attack)(const struct Ym2612 *const ym2612, int chl);
  void (*on_set_pan)(const struct Ym2612 *const ym2612, int chl);
  void (*on_pass_dac_data)(const struct Ym2612 *const ym2612, int data);
  void (*on_set_dac_enable)(const struct Ym2612 *const ym2612);
  void (*on_set_lfo)(const struct Ym2612 *const ym2612);
  void (*on_set_ch3_spec)(const struct Ym2612 *const ym2612);
  void (*on_set_lfo_sens)(const struct Ym2612 *const ym2612, int chl);
};

int ins_find_loudest_slot(const struct Ym2612Ins *const ins);
bool ins_compare_novol(const struct Ym2612Ins *const ins1, 
	const struct Ym2612Ins *const ins2);
int ins_detect_vol(const struct Ym2612Ins *const ins);
void ins_make_louder(struct Ym2612Ins *const ins);
void ym2612_write_port0(struct Ym2612 *const ym2612, int reg, int value);
void ym2612_write_port1(struct Ym2612 *const ym2612, int reg, int value);
int ym2612_convert_freq_to_hz(int freq, int freq_block);
void ym2612_reset(struct Ym2612 *const ym2612);
#endif
