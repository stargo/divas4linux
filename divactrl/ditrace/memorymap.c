/*------------------------------------------------------------------------------
 *
 * (c) COPYRIGHT 1999-2007       Dialogic Corporation
 *
 * ALL RIGHTS RESERVED
 *
 * This software is the property of Dialogic Corporation and/or its
 * subsidiaries ("Dialogic"). This copyright notice may not be removed,
 * modified or obliterated without the prior written permission of
 * Dialogic.
 *
 * This software is a Trade Secret of Dialogic and may not be
 * copied, transmitted, provided to or otherwise made available to any company,
 * corporation or other person or entity without written permission of
 * Dialogic.
 *
 * No right, title, ownership or other interest in the software is hereby
 * granted or transferred. The information contained herein is subject
 * to change without notice and should not be construed as a commitment of
 * Dialogic.
 *
 *------------------------------------------------------------------------------*/
#include <stdio.h>
#include <sys/mman.h>
#include "platform.h"

extern int silent_operation;
extern FILE* VisualStream;

static FILE *InStream = 0;
dword buffer_size;

byte *memorymap(char *filename) {
	int rb_fd;
	byte *rb;

	FILE* InStream = fopen (filename, "r+");

	if (!InStream) {
		if (!silent_operation)
			perror ("Input File");
		return ((void*)-1);
	}

	rb_fd = fileno (InStream);
	if (rb_fd < 0) {
		if (!silent_operation)
			perror ("Input  File");
		fclose (InStream);
		InStream = 0;
		return ((void*)-1);
	}

	buffer_size = (dword)lseek(rb_fd, 0, SEEK_END);
	lseek (rb_fd, 0, SEEK_SET);

	if (buffer_size < 4096) {
		fprintf (VisualStream, "ERROR: Invalid File Format\n");
		fclose (InStream);
		InStream = 0;
		return ((void*)-1);
	}

	if (!(rb = (byte*)mmap (0, buffer_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, rb_fd, 0))) {
		if (!silent_operation)
			perror ("Input Buffer");
		fclose (InStream);
		InStream = 0;
		return ((void*)-1);
	}

  return (rb);
}

void unmemorymap(byte *map_base) {
	if (InStream != 0)
		fclose (InStream);
	InStream = 0;
	munmap(map_base, buffer_size);
}
