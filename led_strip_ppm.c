/*
	License:
	led_strip_ppm.c - RaspberryPi POV light painting in C.
	Copyright (C) 2013-11-07 Phill W.J. Rogers
	PhillRogers_at_JerseyMail.co.uk
	
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
	
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 
	https://github.com/TechColab/led-strip.git
*/
/*
Common parts for all chip types:
	double buffering because the write function may also read back the prev value into the same buffer
	gamma correction
	R,G,B values of 0-255
Separation of IC specific parts - LPD8806
	change 'RGB' order to 'GRB'
	reduction to 7-bit
	set hi-bit
	add latch code
*/
/*
 SoFar:
		2014-02-24
		works OK
		2014-02-27
		malloc, auto seqlen(y), delay as arg, reps as arg
		2014-02-28
		auto numofleds(x) assuming image has already been scaled for my led-strip
		timings taken from LightPainting example
		2014-03-01
		shared array
 ToDo:
		invert alternate reps if reps < 0
		optargs
		auto-invoke imagemagick-convert?
*/

#include "wiringPi.h"
#include "wiringPiSPI.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <math.h>

void msleep(unsigned long milisec)
{
	struct timespec req = { 0 };
	time_t sec = (int) (milisec / 1000);
	milisec = milisec - (sec * 1000);
	req.tv_sec = sec;
	req.tv_nsec = milisec * 1000000L;
	nanosleep(&req, (struct timespec *) NULL);
}

void update_LPD8806(int channel, uint8_t * ptr, int buflen)
{
	int loop = 0;
	uint8_t val = 0, r = 0, g = 0, b = 0;
	uint8_t eo_buf[] = { 0, 0, 0 };
	for (loop = 0; loop < buflen; loop += 3) {
		g = ptr[loop + 0];
		r = ptr[loop + 1];
		b = ptr[loop + 2];
		ptr[loop + 0] = r;
		ptr[loop + 1] = g;
		ptr[loop + 2] = b;
	}
	for (loop = 0; loop < buflen; loop++) {
		val = (ptr[loop] >> 1) | 0x80;
		ptr[loop] = val;
	}
	wiringPiSPIDataRW(channel, ptr, buflen);
	wiringPiSPIDataRW(channel, eo_buf, sizeof(eo_buf));
	/* consider converting read prev values back to RGB,0-255 */
}

uint8_t byte_gamma(uint8_t val)
{
	return (uint8_t) (floor((255.0 * pow(1.0 * val, 2.5)) + 0.5));
	// return val ;
}

int main(int argc, char *argv[])
{
	int channel = 0;
	int reps = 1;
	int bufsize = 0;
	uint8_t *buf = NULL;
	uint8_t **ary = NULL;
	int x = 0, y = 0;
	int optind = 0;
	char buff[16];
	char *filename = NULL;
	FILE *fp;
	int c, rgb_comp_color;
	int seqlen_y = 0;
	int numofleds_x = 0;

	if (argc < 3) {
		fprintf(stderr, "Usage: %s Reps image.ppm (raw binary mode P6)\n",
			argv[0]);
		exit(EXIT_FAILURE);
	}
	reps = atoi(argv[++optind]);
	filename = argv[++optind];

	memset(buff, 0, sizeof(buff));
	fp = fopen(filename, "rb");	// open PPM file for reading
	if (!fp) {
		fprintf(stderr, "Unable to open file %s \n", filename);
		exit(1);
	}
	if (!fgets(buff, sizeof(buff), fp)) {	// read image format magic from first line
		perror(filename);
		exit(1);
	}
	if (buff[0] != 'P' || buff[1] != '6') {	// check the image format
		fprintf(stderr, "Invalid image format (must be 'P6')\n");
		exit(1);
	}
	c = getc(fp);			// check for comments
	while (c == '#') {
		while (getc(fp) != '\n');
		c = getc(fp);
	}
	ungetc(c, fp);

	if (fscanf(fp, "%d %d", &x, &y) != 2) {	// read image size info
		fprintf(stderr, "Invalid image size (error loading '%s')\n", filename);
		exit(1);
	}
	if (fscanf(fp, "%d", &rgb_comp_color) != 1) {	// read rgb component
		fprintf(stderr, "Invalid rgb component (error loading '%s')\n",
			filename);
		exit(1);
	}
	if (rgb_comp_color != 255) {	// check rgb component depth
		fprintf(stderr, "'%s' does not have 8-bits components\n", filename);
		exit(1);
	}
	while (fgetc(fp) != '\n');
	numofleds_x = x;
	seqlen_y = y;
	bufsize = (3 * numofleds_x);

	ary = (uint8_t **) malloc(seqlen_y * sizeof(uint8_t *));
	if (!ary) {
		fprintf(stderr, "Unable to allocate memory\n");
		exit(1);
	}
	for (y = 0; y < seqlen_y; y++) {
		ary[y] = (uint8_t *) malloc(bufsize * sizeof(uint8_t));
		if (!ary[y]) {
			fprintf(stderr, "Unable to allocate memory\n");
			exit(1);
		}
		memset(ary[y], 0, (bufsize * sizeof(uint8_t)));
	}

	// create the pattern
	for (y = 0; y < seqlen_y; y++) {
		for (x = 0; x < numofleds_x; x++) {
			ary[y][(3 * x) + 0] = getc(fp);
			ary[y][(3 * x) + 1] = getc(fp);
			ary[y][(3 * x) + 2] = getc(fp);
		}
	}

	fclose(fp);

	buf = (uint8_t *) malloc(bufsize * sizeof(uint8_t));	// alloc memory form image
	if (!buf) {
		fprintf(stderr, "Unable to allocate memory\n");
		exit(1);
	}
	memset(buf, 0, bufsize);

	// set output SPI channel to 0 and speed to 8MHz
	if (wiringPiSPISetup(channel, 8000000) < 0) {
		fprintf(stderr, "Unable to open SPI device 0: %s\n", strerror(errno));
		exit(1);
	}
	wiringPiSetupSys();

	// animate the sequence
	for (; reps > 0; reps--) {
		for (y = 0; y < seqlen_y; y++) {
			memcpy(buf, &ary[y][0], bufsize);
			update_LPD8806(channel, buf, bufsize);
			msleep(1);
			// getchar(); // step-by-step
		}
		memset(buf, 0, bufsize);
		update_LPD8806(channel, buf, bufsize);
		msleep(500);
	}

	free(buf);
	for (y = seqlen_y - 1; y >= 0; y--)
		free(ary[y]);
	free(ary);
	return (EXIT_SUCCESS);
}
