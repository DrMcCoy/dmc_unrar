/* dmc_unrar - A dependency-free, single-file FLOSS unrar library
 *
 * Copyright (c) 2017 by Sven Hesse (DrMcCoy) <drmccoy@drmccoy.de>
 *
 * dmc_unrar is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 2 of
 * the License, or (at your option) any later version.
 *
 * dmc_unrar is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * For the full text of the GNU General Public License version 2,
 * see <https://www.gnu.org/licenses/gpl-2.0.html>
 */

/* This is a small example program to show how to use dmc_unrar. */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#include "dmc_unrar.c"

int display_help(const char *name);
int display_error(const char *where, const char *str);

char get_command(const char *param);

const char *get_archive_comment(dmc_unrar_archive *archive);
const char *get_file_comment(dmc_unrar_archive *archive, size_t i);
const char *get_filename(dmc_unrar_archive *archive, size_t i);

const char *get_filename_no_directory(const char *filename);

int main(int argc, char **argv) {
	size_t i;
	char cmd;

	const char *comment = NULL;
	dmc_unrar_archive archive;
	dmc_unrar_return return_code;

	if (argc < 3)
		return display_help(argv[0]);

	cmd = get_command(argv[1]);
	if (cmd == 0)
		return display_help(argv[0]);

	if (!dmc_unrar_is_rar_path(argv[2]))
		return display_help(argv[0]);

	return_code = dmc_unrar_archive_init(&archive);
	if (return_code != DMC_UNRAR_OK)
		return display_error("Clear", dmc_unrar_strerror(return_code));

	return_code = dmc_unrar_archive_open_path(&archive, argv[2]);
	if (return_code != DMC_UNRAR_OK)
		return display_error("Open", dmc_unrar_strerror(return_code));

	comment = get_archive_comment(&archive);
	if (comment) {
		printf(".--- Archive comment:\n");
		printf("%s\n", comment);
		printf("'---\n\n");
		free((char *)comment);
	}

	for (i = 0; i < dmc_unrar_get_file_count(&archive); i++) {
		const char *name = get_filename(&archive, i), *file_comment = NULL;
		const dmc_unrar_file *file = dmc_unrar_get_file_stat(&archive, i);

		printf("%u/%u: \"%s\" - %u bytes\n",
		       (unsigned int)(i+1), (unsigned int)dmc_unrar_get_file_count(&archive),
		       name ? name : "", file ? (unsigned int)file->uncompressed_size : (unsigned int)0);

		file_comment = get_file_comment(&archive, i);
		if (file_comment) {
			printf(".--- File comment:\n");
			printf("%s\n", file_comment);
			printf("'---\n\n");
			free((char *)file_comment);
		}

		if (cmd == 'e') {
			const char *filename = get_filename_no_directory(name);

			if (filename && !dmc_unrar_file_is_directory(&archive, i)) {
				dmc_unrar_return supported = dmc_unrar_file_is_supported(&archive, i);
				if (supported == DMC_UNRAR_OK) {
					dmc_unrar_return extracted = dmc_unrar_extract_file_to_path(&archive, i, filename, NULL, true);
					if (extracted != DMC_UNRAR_OK)
						fprintf(stderr, "Error: %s\n", dmc_unrar_strerror(extracted));

				} else
					fprintf(stderr, "Not supported: %s\n", dmc_unrar_strerror(supported));
			}
		}

		free((char *)name);
	}

	dmc_unrar_archive_close(&archive);
	return 0;
}

int display_help(const char *name) {
	fprintf(stderr, "dmc_unrar - A dependency-free, single-file FLOSS unrar library\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage: %s <command> <rarfile>\n", name);
	fprintf(stderr, "\n");
	fprintf(stderr, "Commands:\n");
	fprintf(stderr, "  l        List RAR archive contents\n");
	fprintf(stderr, "  e        Extract RAR archive to current directory\n");
	fprintf(stderr, "\n");

	return 1;
}

int display_error(const char *where, const char *str) {
	fprintf(stderr, "%s failed: %s\n", where, str);
	return 1;
}

char get_command(const char *param) {
	char cmd;

	if (!param || (param[0] == '\0') || (param[1] != '\0'))
		return 0;

	cmd = param[0];
	if ((cmd != 'l') && (cmd != 'e'))
		return 0;

	return cmd;
}

static const char *convert_comment(uint8_t *data, size_t size) {
	const dmc_unrar_unicode_encoding encoding = dmc_unrar_unicode_detect_encoding(data, size);
	if (encoding == DMC_UNRAR_UNICODE_ENCODING_UTF8) {
		data[size] = '\0';
		return (const char *)data;
	}

	if (encoding == DMC_UNRAR_UNICODE_ENCODING_UTF16LE) {
		char *utf8_data = NULL;

		size_t utf8_size = dmc_unrar_unicode_convert_utf16le_to_utf8(data, size, NULL, 0);
		if (utf8_size) {
			utf8_data = (char *)malloc(utf8_size);
			if (utf8_data) {
				utf8_size = dmc_unrar_unicode_convert_utf16le_to_utf8(data, size, utf8_data, utf8_size);
				if (utf8_size) {
					free(data);
					return utf8_data;
				}
			}
		}

		free(data);
		free(utf8_data);
		return NULL;
	}

	free(data);
	return NULL;
}

const char *get_archive_comment(dmc_unrar_archive *archive) {
	uint8_t *data = NULL;
	size_t size = dmc_unrar_get_archive_comment(archive, NULL, 0);
	if (!size)
		return NULL;

	data = (uint8_t *)malloc(size + 1);
	if (!data)
		return NULL;

	size = dmc_unrar_get_archive_comment(archive, data, size);
	return convert_comment(data, size);
}

const char *get_file_comment(dmc_unrar_archive *archive, size_t i) {
	uint8_t *data = NULL;
	size_t size = dmc_unrar_get_file_comment(archive, i, NULL, 0);
	if (!size)
		return NULL;

	data = (uint8_t *)malloc(size + 1);
	if (!data)
		return NULL;

	size = dmc_unrar_get_file_comment(archive, i, data, size);
	return convert_comment(data, size);
}

const char *get_filename(dmc_unrar_archive *archive, size_t i) {
	char *filename = 0;
	size_t size = dmc_unrar_get_filename(archive, i, NULL, 0);
	if (!size)
		return NULL;

	filename = (char *)malloc(size);
	if (!filename)
		return NULL;

	size = dmc_unrar_get_filename(archive, i, filename, size);
	if (!size) {
		free(filename);
		return NULL;
	}

	dmc_unrar_unicode_make_valid_utf8(filename);
	if (filename[0] == '\0') {
		free(filename);
		return NULL;
	}

	return filename;
}

const char *get_filename_no_directory(const char *filename) {
	char *p = NULL;
	if (!filename)
		return NULL;

	p = (char *) strrchr(filename, '/');
	if (!p)
		return filename;

	if (p[1] == '\0')
		return NULL;

	return p + 1;
}
