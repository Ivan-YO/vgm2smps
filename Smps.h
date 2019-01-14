#ifndef _SMPS_H
#define _SMPS_H

#include "pch.h"
#include "Chips.h"
#include "main.h"
#include "VgmParser.h"

#define SMPS_VOICE_SIZE 25
#define SMPS_MAX_VOICE_COUNT 255
#define SMPS_NOTE_C1 0x81
#define SMPS_NOTE_A_8 0xDF
#define SMPS_NOTE_OFF 0x80
#define SMPS_NOTE_LEN_MAX 127
#define SMPS_PTR_MAX 0x10000

// Coordination flags
#define SMPS_PAN_RIGHT 0x40
#define SMPS_PAN_LEFT 0x80
#define SMPS_PAN_CENTRE 0xC0

#define FSMPSPAN 0xE0
#define FSMPSALTERNOTE 0xE1
#define FSMPSRETURN 0xE3
#define FSMPSCHANTEMPODIV 0xE5
#define FSMPSALTERVOL 0xE6
#define FSMPSNOATTACK 0xE7
#define FSMPSNOTEFILL 0xE8
#define FSMPSALTERPITCH 0xE9
#define FSMPSSETTEMPOMOD 0xEA
#define FSMPSSETTEMPODIV 0xEB
#define FSMPSSETVOL 0xEC
#define FSMPSFMVOICE 0xEF
#define FSMPSMODSET 0xF0
#define FSMPSMODON 0xF1
#define FSMPSSTOP 0xF2
#define FSMPSPSGFORM 0xF3
#define FSMPSMODOFF 0xF4
#define FSMPSPSGVOICE 0xF5
#define FSMPSJUMP 0xF6
#define FSMPSLOOP 0xF7
#define FSMPSCALL 0xF8

void smps_create_voice(uint8_t (*const voice)[SMPS_VOICE_SIZE], const struct Ym2612Ins *const ins);
void smps_export_to_file(const char *const file_path);

#endif