#include "pch.h"
#include "Vgm.h"
#include <byteswap.h>
//#define ZLIB_WINAPI
//#include "Zlib/zlib.h"
#include <zlib.h>

static uint32_t get_gz_file_length(FILE* hFile)
{
	uint32_t file_size;
	uint16_t gz_head;
	size_t ret_val;

	ret_val = fread(&gz_head, 0x02, 0x01, hFile);
	if (ret_val >= 1)
	{
		gz_head = __bswap_16(gz_head);
		if (gz_head != 0x1F8B)
		{
			ret_val = 0;	// no .gz signature - treat as normal file
		}
		else
		{
			// .gz File
			fseek(hFile, -4, SEEK_END);
			// Note: In the error case it falls back to fseek/ftell.
			ret_val = fread(&file_size, 0x04, 0x01, hFile);
		}
	}
	if (ret_val <= 0)
	{
		// normal file
		fseek(hFile, 0x00, SEEK_END);
		file_size = ftell(hFile);
	}

	return file_size;
}

void vgm_create_from_file(const char *const file_path, struct Vgm *const vgm)
{
	int errno_prev = errno;
	gzFile gzf = NULL;
	uint32_t vgmf_ident = 0;

	FILE *f = fopen(file_path, "rb");

	if (errno != errno_prev)
		return;

	size_t file_size = get_gz_file_length(f);
	fclose(f);

	gzf = gzopen(file_path, "rb");
	int rd = gzread(gzf, &vgmf_ident, 4);

	if (rd != 4)
	{
		gzclose(gzf);
		goto err_notvgm;
	}

	if (vgmf_ident != VGM_IDENT)
		goto err_notvgm;

	vgm->bufsize = file_size;
	vgm->buffer = malloc(file_size);

	if (!vgm->buffer)
		goto err_badalloc;

	gzrewind(gzf);
	int res = gzread(gzf, vgm->buffer, file_size);

	if (res < 0)
	{
		gzclose(gzf);
		printf("Error with decompression! %s\n", gzerror(gzf, &res));
		errno = EFAULT;
		return;
	}

	gzclose(gzf);
	return;

err_notvgm:
	errno = EVGMNOTSUPPORTED;
	return;

err_badalloc:
	errno = EBADALLOC;
	return;
}

void vgm_free(const struct Vgm *const vgm)
{
	if (!vgm) return;
	if (!(vgm->buffer)) return;
	free(vgm->buffer);
}

void vgm_read_info(struct Vgm *const vgm)
{
	struct VgmHeader *vgm_header = NULL;
	uint8_t *gd3_off = NULL;
	uint8_t *eof_off = NULL;
	uint8_t *loop_off = NULL;
	uint8_t *vgm_data_off = NULL;

	vgm->vgm_header = (struct VgmHeader *)vgm->buffer;
	vgm_header = vgm->vgm_header;
	gd3_off = (uint8_t *)(&(vgm_header->gd3_offset));
	eof_off = (uint8_t *)(&(vgm_header->eof_offset));
	loop_off = (uint8_t *)(&(vgm_header->loop_offset));
	vgm_data_off = (uint8_t *)(&(vgm_header->vgm_data_offset));

	vgm->gd3 = *((uint32_t *)(gd3_off)) + gd3_off;
	vgm->eof = *((uint32_t *)(eof_off)) + eof_off;
	vgm->loop = *((uint32_t *)(loop_off)) + loop_off;

	vgm->sn76489_clock = vgm_header->sn76489_clock;

	if (vgm_header->vgm_version < 0x110)
	{
		vgm->vgm_data = (uint8_t *)vgm_header + 0x40;
		vgm->ym2612_clock = vgm_header->ym2413_clock;
	}
	else
	{
		vgm->ym2612_clock = vgm_header->ym2612_clock;
		vgm->vgm_data = (vgm_header->vgm_data_offset == 0xC) ?
			(uint8_t *)vgm_header + 0x40 :
			*((uint32_t *)(vgm_data_off)) + vgm_data_off;
	}

	vgm->ym2612_enabled = vgm->ym2612_clock != 0;
	vgm->sn76489_enabled = vgm->sn76489_clock != 0;
}
