#pragma once
#include "Chips.h"
extern char *input_file_path;
extern char *output_file_path;
extern bool ym_enabled;
extern bool sn_enabled;
extern bool ym_chls_enabled[YM_CHLS];
extern bool sn_chls_enabled[SN_CHLS];
extern bool ym_enable_keyoff_notes;
extern bool dac_export;
extern uint32_t smps_length;
extern uint32_t smps_start_offset;
void path_remove_extension(char *const path);
void path_remove_file_name(char *const path);