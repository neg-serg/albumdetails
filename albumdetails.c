#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <tag_c.h>
#include <sys/stat.h>

#include "arg.h"

#define LEN(x) (sizeof (x) / sizeof *(x))

char *argv0;

bool siunits = false;
bool utf8flag = true;

void eprintf(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

size_t strlcpy(char *dest, const char *src, size_t size) {
	size_t len;

	len = strlen(src);

	if (size) {
		if (len >= size)
			size -= 1;
		else
			size = len;
		strncpy(dest, src, size);
		dest[size] = '\0';
	}

	return size;
}

int filesize(char *filename) {
	struct stat st;

	stat(filename, &st);
	return st.st_size;
}

char *bytestostr(double bytes) {
	int i;
	int cols;
	static char str[32];
	static const char iec[][4] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB" };
	static const char si[][3] = { "B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB" };
	const char *unit;
	char *fmt;
	double prefix;

	if (siunits) {
		prefix = 1000.0;
		cols = LEN(si);
	} else {
		prefix = 1024.0;
		cols = LEN(iec);
	}

	for (i = 0; bytes >= prefix && i < cols; i++)
		bytes /= prefix;

	fmt = i ? "%.2f %s" : "%.0f %s";
	unit = siunits ? si[i] : iec[i];
	snprintf(str, sizeof(str), fmt, bytes, unit);

	return str;
}

void usage(void) {
	eprintf("Usage: %s [options] <audiofiles>\n"
			"\n"
			"-h    Help\n"
			"-i    Use ISO-8859-1 instead of UTF-8\n"
			"-s    Use SI units instead of IEC\n"
			, argv0);
}

int main(int argc, char **argv) {
	int i;
	int j;
	int totlength = 0;
	int totsize = 0;
	int avgbitrate = 0;
	TagLib_File *file;
	TagLib_Tag *tag;
	const TagLib_AudioProperties *properties;

	struct tracklist {
		char artist[BUFSIZ];
		char album[BUFSIZ];
		char genre[BUFSIZ];
		int year;
		struct quality {
			int bitrate;
			int samplerate;
			int channels;
		} q;

		int track;
		char title[BUFSIZ];
		int length;
		int size;
	} tl[64];

	memset(&tl, 0, sizeof tl);

	ARGBEGIN {
	case 'i':
		utf8flag = false;
		break;
	case 's':
		siunits = true;
		break;
	default:
		usage();
	} ARGEND;

	if (argc < 2)
		usage();

	taglib_set_strings_unicode(utf8flag);

	for (i = 0; i < argc; i++) {
		file = taglib_file_new(argv[i]);
		if (!file) {
			fprintf(stderr, "non-audio file: %s\n", argv[i]);
			argv++;
			argc--;
			i--;
			continue;
		}

		tag = taglib_file_tag(file);
		properties = taglib_file_audioproperties(file);
		if (!tag || !properties) {
			fprintf(stderr, "Can't read meta-data for %s\n", argv[i]);
		} else {
			strlcpy(tl[i].artist, taglib_tag_artist(tag), sizeof tl[i].artist);
			strlcpy(tl[i].album, taglib_tag_album(tag), sizeof tl[i].album);
			strlcpy(tl[i].genre, taglib_tag_genre(tag), sizeof tl[i].genre);
			tl[i].year = taglib_tag_year(tag);
			tl[i].q.bitrate = taglib_audioproperties_bitrate(properties);
			tl[i].q.samplerate = taglib_audioproperties_samplerate(properties);
			tl[i].q.channels = taglib_audioproperties_channels(properties);

			tl[i].track = taglib_tag_track(tag);
			strlcpy(tl[i].title, taglib_tag_title(tag), sizeof tl[i].title);
			tl[i].length = taglib_audioproperties_length(properties);
			tl[i].size = filesize(argv[i]);

			avgbitrate += taglib_audioproperties_bitrate(properties);
		}

		taglib_tag_free_strings();
		taglib_file_free(file);
	}

	avgbitrate /= i;

	printf("Artist ....: %s\n", tl[0].artist);
	printf("Album .....: %s\n", tl[0].album);
	printf("Genre .....: %s\n", tl[0].genre);
	printf("Year ......: %d\n", tl[0].year);
	printf("Quality ...: %dkbps / %.1fkHz / %d channels\n\n",
			avgbitrate, tl[0].q.samplerate / 1000.0f, tl[0].q.channels);

	for (i = 0; tl[i].title[0]; i++) {
		totlength += tl[i].length;
		totsize += tl[i].size;
		printf("%2d. %-56s\t(%d:%02d)\n",
				tl[i].track, tl[i].title,
				tl[i].length / 60, tl[i].length % 60);
	}

	puts("");
	printf("Playing time ...: %02d:%02d:%02d\n",
			totlength / 60 / 60, totlength / 60, totlength % 60);
	printf("Total size .....: %s\n", bytestostr(totsize));

	return EXIT_SUCCESS;
}
