
/*
 *
  Copyright (c) Sangoma Technologies, 2018-2022
  Copyright (c) Dialogic(R), 2004-2017
  Copyright 2000-2003 by Armin Schindler (mac@melware.de)
  Copyright 2000-2003 Cytronics & Melware (info@melware.de)

 *
  This source file is supplied for the use with
  Sangoma (formerly Dialogic) range of Adapters.
 *
  File Revision :    2.1
 *
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.
 *
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY OF ANY KIND WHATSOEVER INCLUDING ANY
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.
 *
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

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
