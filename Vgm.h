#ifndef _VGM_H
#define _VGM_H

#include "pch.h"

#define VGM_IDENT 0x206D6756
#define GD3_IDENT 0x20336447

struct VgmHeader
{
	uint32_t vgm_ident;
	uint32_t eof_offset;
	uint32_t vgm_version;
	uint32_t sn76489_clock;
	uint32_t ym2413_clock;
	uint32_t gd3_offset;
	uint32_t total_samples;
	uint32_t loop_offset;
	uint32_t loop_samples;
	uint32_t rate;
	uint16_t sn_fb;
	uint16_t snw;
	uint32_t ym2612_clock;
	uint32_t ym2151_clock;
	uint32_t vgm_data_offset;
};

struct Vgm
{
	uint8_t        *buffer;
	size_t     bufsize;
	struct VgmHeader *vgm_header;
	uint8_t        *eof;
	uint8_t        *gd3;
	uint8_t        *loop;
	uint8_t        *vgm_data;
	wchar_t   *track_name_eng;
	wchar_t   *track_name_jap;
	wchar_t   *game_name_eng;
	wchar_t   *game_name_jap;
	wchar_t	  *system_name_eng;
	wchar_t   *system_name_jap;
	wchar_t	  *track_autor_eng;
	wchar_t   *track_autor_jap;
	wchar_t	  *game_release_date;
	wchar_t	  *vgm_creator;
	wchar_t	  *notes;
	uint32_t        ym2612_clock;
	uint32_t        sn76489_clock;
	bool       sn76489_enabled;
	bool       ym2612_enabled;
};

/*
Функция:
	void vgm_create_from_file(const char *const file_path, struct Vgm *const vgm)
Действие:
	Открывает файл по пути file_path, проверяет, является ли он файлом vgm,
	затем копироет содержимое файла в память, указатель на которую
	помещает в поле buffer, и в bufsize записывает
	размер файла.
Параметры:
	file_path -- путь к VGM файлу.
	vgm -- структура Vgm для заполнения.
Ошибки:
	Возникающие при открытии файлов, ENOTVGM, EBADALLOC
*/
void vgm_create_from_file(const char *const file_path, struct Vgm *const vgm);

/*
Функция:
	void vgm_free(const struct Vgm *const vgm)
Действие:
	Освобождает память выделенную для поля buffer.
Параметры:
	vgm -- структура Vgm, содержащая выделенную память в поле buffer.
Ошибки:
*/
void vgm_free(const struct Vgm *const vgm);

/*
Функция:
	void vgm_read_info(struct Vgm *const vgm)
Действие:
	Считывает содержимое поля buffer и заполняет поля найденной информацией о открытом файле.
Параметры:
	vgm -- структура Vgm, содержащая выделенную память в поле buffer.
Ошибки:
*/
void vgm_read_info(struct Vgm *const vgm);

#endif