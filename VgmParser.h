#ifndef VGM_PARSER_H
#define VGM_PARSER_H

#include "pch.h"
#include "Vgm.h"
#include "Chips.h"
#include "main.h"

#define SAMPLE_RATE 44100

// Максимальное количество состояний
#define STATES_MAX_COUNT 20000 
// Максимальное количество инструментов YM2612
#define INS_MAX_COUNT 256
// Максимальное количество найденных уникальных частот в YM2612
#define UNIQUE_FREQS_BUFFER_LENGTH 256

// Количество нот в октаве
#define NOTES_IN_OCT 12
// Ширина таблицы нот
#define NOTE_TABLE_WIDTH 5
// Октава без смещения в таблице нот
#define NOTE_TABLE_MIDDLE ((NOTE_TABLE_WIDTH - 1) / 2)

#define STATES_YM_SIZE YM_CHLS
#define STATES_SN_SIZE (SN_CHLS - 1)
#define DAC_STREAM_MAX_COUNT 254
#define DATA_BLOCK_MAX_COUNT 255

struct VgmDacStream
{
	bool enabled;
	uint16_t block_id;
	uint32_t sample_rate;
};

struct DataBlock
{
	uint8_t *begin;
	uint32_t size;
};

struct VgmParser
{
	const struct Vgm *vgm;
	uint32_t sample_count;
	uint32_t frame_count;
	struct VgmDacStream dac_stream;
	struct DataBlock data_block[DATA_BLOCK_MAX_COUNT];
	uint8_t data_block_count;
	uint32_t pcm_data_bank_seek;
	struct Ym2612 ym2612;
	struct Sn76489 sn76489;
};

// Структуры, содержащие текущее состояние чипов...
struct Ym2612State
{
	int16_t freq;
	int8_t  freq_block;
	int8_t  key_state;
	int8_t  key_noattack;
	int8_t  pan;
	int8_t	ams;
	int8_t	fms;
	int16_t ins_index;
	int8_t  vol;
	uint32_t frames;
};

struct Sn76489State
{
	int8_t  vol;
	int16_t tone;
	uint32_t frames;
};

struct Sn76489NoiseState
{
	int8_t  vol;
	int16_t tone;
	int8_t  ch4_mode;
	int8_t  noise_type;
	uint32_t frames;
};

struct VgmLogger
{
	uint32_t fps;
	uint32_t samples_per_frame;
	struct Ym2612State ym2612_state[STATES_YM_SIZE][STATES_MAX_COUNT];
	struct Sn76489State sn76489_state[STATES_SN_SIZE][STATES_MAX_COUNT];
	struct Ym2612Ins ins[INS_MAX_COUNT];
	uint8_t ins_count;
	uint32_t ym2612_state_count[STATES_YM_SIZE];
	uint32_t sn76489_state_count[STATES_SN_SIZE];
	uint32_t sn76489_noise_state_count;
	struct Sn76489NoiseState sn76489_noise_state[STATES_MAX_COUNT];
	int16_t note_table[NOTE_TABLE_WIDTH][NOTES_IN_OCT];
	int16_t unique_freqs[UNIQUE_FREQS_BUFFER_LENGTH];
	uint32_t unique_freqs_counts[UNIQUE_FREQS_BUFFER_LENGTH];
	uint16_t unique_freqs_count;
};

/*
Функция:
	void vgm_parse(const struct Vgm *const vgm)
Действие:
	Парсит vgm файл, которых нахоится в поле buffer структуры Vgm
	Вызывает функции логирования, указатели на которые будут в VgmLogger,
	когда проходит время минимум одного кадра
Параметры:
	vgm -- заполненная структура Vgm
Ошибки:
	E2BIG -- слишком много событий, больше максимально разрешенного.
*/
void vgm_parse(const struct Vgm *const vgm);

/*
Функция:
	void vgm_parser_init(const struct Vgm *const vgm)
Действие:
	Возводит в начальное состояние структуру VgmParser и
	записывает информацию о VGM файле, считываемую со структуры Vgm
Параметры:
	vgm -- заполненная структура Vgm
Ошибки:
*/
void vgm_parser_init(const struct Vgm *const vgm);

/*
Функция:
	void vgm_logger_init(const struct Vgm *const vgm, int fps);
Действие:
	Возводит в начальное состояние структуру VgmLogger и
	записывает информацию о VGM файле, считываемую со структуры Vgm
	Запоминет указатели на функции для логирования, которые будут вызываться
	при скачке во времени минимум в один кадр.
Параметры:
	vgm -- заполненная структура Vgm.
	fps -- количиество кадров в секунду.
Ошибки:
*/
void vgm_logger_init(const struct Vgm *const vgm, int fps);

/*
Функция:
	void vgm_logger_remove_blank_chls()
Действие:
	Делает счетчик событий в каждом канале равному 0, если
	В этом канале только тишина.
Параметры:
Ошибки:
*/
void vgm_logger_remove_blank_chls();

/*
Функция:
	void vgm_parser_export_dac_samples(char *output_folder)
Действие:
	Экспортирует найденные в VGM DAC сэмплы в заданную папку
Параметры:
	output_folder -- папка для сохранения
Ошибки:
	Возникающие при файловом вводе\выводе
*/
void vgm_parser_export_dac_samples(char *output_folder);

/*
Функция:
	void vgm_find_unique_freqs()
Действие:
	Находит уникальные частоты ym2612 в проанализированном треке
	и записывает их счет в струкруту VgmLogger
Параметры:
Ошибки:
*/
void vgm_find_unique_freqs();

/*
Функция:
	void vgm_build_note_table()
Действие:
	Состовляет таблицу нот трека, основываясь на информации,
	составленной функцией vgm_find_unique_freqs().
Параметры:
Ошибки:
*/
void vgm_build_note_table();

/*
Функция:
	void vgm_logger_apply_external_note_table(int16_t(*const note_table)[NOTE_TABLE_WIDTH][NOTES_IN_OCT])
Действие:
	Копирует содержимое внешней таблицы нот.
Параметры:
	note_table -- указатель на массив частот.
Ошибки:
*/
void vgm_logger_apply_external_note_table(int16_t(*const note_table)[NOTE_TABLE_WIDTH][NOTES_IN_OCT]);

struct VgmLogger *const get_vgm_logger();
struct VgmParser *const get_vgm_parser();

#endif
