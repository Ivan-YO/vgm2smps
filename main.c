#include "pch.h"
#include "Chips.h"
#include "Vgm.h"
#include "VgmParser.h"
#include "Smps.h"

// TODO:
// Stereophonic DAC
// Decent error handling
#define CHARTOI(c) ((c) - '0')
#define FPS_DEFAULT 60
#define FPS_MAX 999
#define FPS_MIN 1
#define DIRECTORY_SEPARATOR_CHAR '\\'

char* input_file_path = NULL;
char* output_file_path = NULL;
bool ym_enabled = true;
bool sn_enabled = true;
bool ym_chls_enabled[YM_CHLS] =
	{ true, true, true, true, true, true };
bool sn_chls_enabled[SN_CHLS] =
	{ true, true, true, true };
uint16_t fps = 60;
uint32_t smps_length = 0;
uint32_t smps_start_offset = 0;
bool ym_enable_keyoff_notes = false;
bool dac_export = false;
bool alt_ins = false;

void path_remove_extension(char *const path);
void path_remove_file_name(char *const path);
int commandline_parse(int argc, char **argv);
void free_all(void);

struct Vgm vgm;

int main(int argc, char *argv[])
{
	bool success = false;
	if (commandline_parse(argc, argv))
		return 0;

	vgm_create_from_file(input_file_path, &vgm);

	if (errno != 0)
	{
		switch (errno)
		{
		case EVGMNOTSUPPORTED:
			printf("Error! This is not a VGM file!\n");
			break;
		case EBADALLOC:
			printf("Error! Can not memory alloc!\n");
			break;
		case EFAULT:
			break;
		default:
			printf("VGM file opening error!\n");
			break;
		}

		return 0;
	}

	atexit(free_all);
	vgm_read_info(&vgm);
	if (errno == EVGMNOTSUPPORTED)
	{
		printf("Error! VGM has neither YM2612 nor SN76489!\n");
		return 0;
	}
	vgm_parser_init(&vgm);
	vgm_logger_init(&vgm, fps);
	vgm_parse(&vgm);

	vgm_logger_remove_blank_chls();
	vgm_find_unique_freqs();
	vgm_build_note_table();

	bool output_file_path_was_setuped = !!output_file_path;

	if (!output_file_path)
	{
		// File extension to be concatenated with original path
		static const char ext[] = ".bin";

		// What if I'll need UTF-16?
		const int char_size = sizeof(ext[0]);
		const int ext_len = ARRLENGTH(ext) - 1; // without '\0'
		const int old_len = strlen(input_file_path);

		// Length of path with extension
		int new_len = 0;
		output_file_path = malloc((old_len + ext_len) * char_size);
		memcpy(output_file_path,
			   input_file_path,
			   old_len + char_size); // with '\0'

		path_remove_extension(output_file_path);
		new_len = strlen(output_file_path);
		strcat(output_file_path, ext);
	}

	smps_export_to_file(output_file_path);
	if (errno != 0)
	{
		switch (errno)
		{
		case E2BIG:
			printf("SMPS file saving error: too big!\n");
			break;
		default:
			printf("SMPS file saving error!\n");
			break;
		}
	}
	else
		success = true;

	if (dac_export)
		vgm_parser_export_dac_samples(output_file_path);

	if (!output_file_path_was_setuped)
		free(output_file_path);

	if (success) printf("Done!\n");
	return 0;
}

void free_all()
{
	vgm_free(&vgm);
}

int commandline_parse(int argc, char **argv)
{
#define FLAG_COUNT 10
	static const int argbase = 1;
	int found_flag_index = 0;
	int str_pos = 0;
	bool flags_used[FLAG_COUNT] = { false };
	static const char *const flag_strings[FLAG_COUNT] =
	{
		"-fm_enable=",
		"-fm_chls=",
		"-psg_enable=",
		"-psg_chls=",
		"-fm_enable_keyoff_notes=",
		"-dac_export=",
		"-fps=",
		"-length=",
		"-start=",
		"-altins="
	};

	int flag_string_lengths[FLAG_COUNT];
	for (int i = 0; i < FLAG_COUNT; ++i)
		flag_string_lengths[i] = strlen(flag_strings[i]);

	enum // for switch-case
	{
		F_FM_ENABLE,
		F_FM_CHLS,
		F_PSG_ENABLE,
		F_PSG_CHLS,
		F_FM_ENABLE_KEYOFF_NOTES,
		F_DAC_EXPORT,
		F_FPS,
		F_LENGTH,
		F_START,
		F_ALTINS
	};

	if (argc == argbase)
	{
		printf(
			"vgm2smps by NBAH (aka Ivan YO)\n"
			"Usage:\n"
			"  vgm2smps <input_file_name> [<output_file_name>] [-fm_enable=1]\n"
			"    [-fm_chls=111111] [-psg_enable=1]\n"
			"    [psg_chls=1111] [-fps=60] [-length=0] [-start=0] [-fm_enable_keyoff_notes=0] [-dac_export=0]\n"
			"Parameters:\n"
			"  [<output_file_name>]\n"
			"    Output SMPS file name.\n"
			"    Default: <input_file_name>, but with.bin extension.\n"
			"  [-fm_enable=1]\n"
			"    Enable FM. Can be either enabled (1) or disabled (0).\n"
			"    Default: 1\n"
			"  [-fm_chls=111111]\n"
			"    Enable FM channels.Can be either enabled(1) or disabled(0).\n"
			"    If you only want to disable first three channels, you may write - fm_chls=000\n"
			"    Default: 111111\n"
			"  [-psg_enable=1]\n"
			"    Same as FM, but with a PSG.\n"
			"  [-psg_chls=1111]\n"
			"    Same as FM, but with a PSG.\n"
			"  [-fps=60]\n"
			"    Set the parsing speed.\n"
			"  [-length=0]\n"
			"    Set the output SMPS length in frames.\n"
			"    Default: 0 -- source's length.\n"
			"  [-start=0]\n"
			"    Set the start song offset in frames.\n"
			"    Default: 0 -- without an offset.\n"
			"  [-fm_enable_keyoff_notes=0]\n"
			"    Enable support of notes even if key is off.\n"
			"    SMPS does not support frequency changing after key off.\n"
			"    Warning! Result may be corrupt.\n"
			"    Default: 0\n"
			"  [-dac_export=0]\n"
			"    Enable found DAC samples from the VGM file. Saves into input file folder.\n"
			"  [-altins=0]\n"
			"    Enable altarnate \"smart\" instrument and note detecting."
			"    If real frequency (Hz) of note is lower then 0 octave, \n"
			"    instrument makes lower and notes makes upper in octave.\n"
		);
		return 1;
	}

	input_file_path = argv[argbase];

	for (int n = argbase + 1; n < argc; ++n)
	{
		int f = 0;
		int flag_len = 0;
		char *arg = argv[n];
		for (; f < FLAG_COUNT; ++f)
		{
			str_pos = flag_string_lengths[f]; // after '='
			int eq = memcmp(flag_strings[f], arg, str_pos);
			if (eq == 0) break; // flag detected
		}

		if (f == FLAG_COUNT) // flag not found
		{
			// this is output file name
			if (!output_file_path)
			{
				// setup output file name
				//path_remove_extension(arg);
				output_file_path = arg;
			}
			continue;
		}

		// this is a flag
		if (flags_used[f]) continue;

		switch (f)
		{
		case F_FM_ENABLE:
		{
			int c = arg[str_pos];
			ym_enabled = !!CHARTOI(c);
		}
		break;

		case F_FM_CHLS:
			for (int c = 0, i = 0; c = arg[str_pos]; i++, str_pos++)
				ym_chls_enabled[i] = !!CHARTOI(c);
			break;

		case F_PSG_ENABLE:
		{
			int c = arg[str_pos];
			sn_enabled = !!CHARTOI(c);
		}
		break;

		case F_PSG_CHLS:
			for (int c = 0, i = 0; c = arg[str_pos]; i++, str_pos++)
				sn_chls_enabled[i] = !!CHARTOI(c);
			break;

		case F_FM_ENABLE_KEYOFF_NOTES:
		{
			int c = arg[str_pos];
			ym_enable_keyoff_notes = !!CHARTOI(c);
		}
		break;

		case F_DAC_EXPORT:
		{
			int c = arg[str_pos];
			dac_export = !!CHARTOI(c);
		}
		break;

		case F_FPS:
		{
			int num;
			char *num_str = &(arg[str_pos]);
			num = atoi(num_str);

			if (num > FPS_MAX)
				num = FPS_MAX;
			if (num < FPS_MIN)
				num = FPS_MIN;

			fps = num;
		}
		break;

		case F_LENGTH:
		{
			char *num_str = &(arg[str_pos]);
			smps_length = atoi(num_str);
		}
			break;

		case F_START:
		{
			char *num_str = &(arg[str_pos]);
			smps_start_offset = atoi(num_str);
		}
		break;

		case F_ALTINS:
		{
            int c = arg[str_pos];
			alt_ins = !!CHARTOI(c);
		}
		break;
		} // switch (f)

		flags_used[f] = true;
	}
	return 0;
#undef FLAG_COUNT
}

void path_remove_extension(char *const path)
{
	for (int i = strlen(path); i > 0; --i)
	{
		switch (path[i])
		{
		case '.':
			path[i] = '\0';
		case DIRECTORY_SEPARATOR_CHAR:
			return;
		}
	}
}

void path_remove_file_name(char *const path)
{
	for (int i = strlen(path); i > 0; --i)
	{
		char *c = &(path[i]);
		if (*c == DIRECTORY_SEPARATOR_CHAR)
		{
			*(c + 1) = '\0';
			return;
		}
	}
}
