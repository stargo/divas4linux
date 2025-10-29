
/*
 *
  Copyright (c) Dialogic(R), 2009-2014.
 *
  This source file is supplied for the use with
  Dialogic range of DIVA Server Adapters.
 *
  Dialogic(R) File Revision :    2.1
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

#if defined(LINUX)
#include <malloc.h>
#else
#include <windows.h>
#endif

#include <stdio.h>

#include "platform.h"
#include "debug.h"

#include "dlist.h"
#include "diva_malloc.h"

//#define MEMCHECK            /* Supervise memory (keep track of allocated memory with a separate list) */
//#define MEMBYTECOUNT        /* Calculate memory consumption in bytes */
//#define LATE_DELETION       /* Keep memory for later write check ? */
//#define BOUNDARY_CHECK      /* Check for buffer overflows */
//#define MEMSTATISTICS       /* Track malloc requests (count and size) */
/* To increase the performance on calling free(), the allocated blocks may be put into one of a number of queues.
 * The more queues, the more performance, but the more memory usage.
 * Memory consumption is about 8*(2^MEMCHECK_BITS) bytes, e.g. 16K bytes with a 12 bit significance. */
#define MEMCHECK_BITS            12        /* Number of relevant bits to select the memory queue (may be 0 to have a single queue only) */
#define MEMCHECK_IGNORE_BITS     8         /* Number of least significant bits to ignore */
#define MEMCHECK_OFFSET(addr)    ((((dword)addr)>>(MEMCHECK_IGNORE_BITS)) & ((1<<(MEMCHECK_BITS))-1)) /* Offset into memoryQ array for a given address */


/***********************************************************************************************************/
/* TYPES */
/***********************************************************************************************************/

typedef struct
{
#ifdef MEMCHECK
	diva_entity_link_t link;
	byte  *data;
	BOOL   staticValue;
	dword  mallocID;
#endif
	dword  numOfBytesAllocated;
} malloc_mgt_t;


/***********************************************************************************************************/
/* PRIVATE VARIABLES */
/***********************************************************************************************************/

static dword malloc_bytes_current  = 0;
static dword malloc_bytes_max      = 0;
static dword malloc_bytes_static   = 0;   /* Amount of memory considered as "static", i.e. the memory consumed just after startup which always remains constant */
static dword malloc_blocks_current = 0;
static dword malloc_blocks_max     = 0;
static dword malloc_blocks_static  = 0;   /* dito, as number of blocks */

#ifdef MEMCHECK
static int mallocID = 0;

static diva_entity_queue_t memoryQTable[1<<(MEMCHECK_BITS)];
#endif

#ifdef LATE_DELETION
diva_entity_queue_t deletedQ = {NULL, NULL};
#define DELETION_MAGIC_BYTE      0xCA
#endif

#ifdef BOUNDARY_CHECK
/* "Magic" block positioned at start and end of an allocated memory block to find boundary violations */
/* Must have a length of multiple of 8 to be sure that it does not interfere with memory alignment */
static const char boundaryMagic[] = {0xBC, 0x52, 0xBC, 0x52, 0xBC, 0x52, 0xBC, 0x52,
                                     0xBC, 0x52, 0xBC, 0x52, 0xBC, 0x52, 0xBC, 0x52,
                                     0xBC, 0x52, 0xBC, 0x52, 0xBC, 0x52, 0xBC, 0x52,
                                     0xBC, 0x52, 0xBC, 0x52, 0xBC, 0x52, 0xBC, 0x52};
#endif

#ifdef MEMSTATISTICS
static dword blockSizesSmall[257] = {0};   /* Counts the number of requested blocks between 0 and 256 byte */
static dword blockSizesBig[257] = {0};     /* Counts the number of requested blocks between 257 and 65536 byte */
static dword blockSizesHuge = 0;         /* Counts the number of requested blocks larger 65536 bytes */
#endif

/***************************************************************************************/
/* Internal function to be called by dlist module to compare two memory allocator elements */
#ifdef MEMCHECK /* memory monitoring model*/
static int memoryDataCmp(const void* what, const diva_entity_link_t *cmpLink)
{
	malloc_mgt_t     *mp = DIVAS_CONTAINING_RECORD(cmpLink, malloc_mgt_t, link);

	return(what != mp->data);
}
#endif

/***************************************************************************************/
#ifdef LINUX
static void *platform_malloc(dword flags, dword allocSize)
{
	void *p = malloc(allocSize);

	if (p && ((flags & HEAP_ZERO_MEMORY) || (flags & MALLOC_FLAG_MEMZERO)))
	{
		memset (p, 0x00, allocSize);
	}
	return p;
}

static void platform_free(dword flags, void *data)
{
	free(data);
}
#endif

/***************************************************************************************/
#ifdef WIN32
static void *platform_malloc(dword flags, dword allocSize)
{
	return HeapAlloc(GetProcessHeap(), flags, allocSize);
}

static void platform_free(dword flags, void *data)
{
	(void)flags;
	HeapFree(GetProcessHeap(), 0, data);
}
#endif


/***************************************************************************************/
/* Public functions */
/***************************************************************************************/
void diva_malloc_init(void)
{
#ifdef MEMCHECK
	dword            queue = 0;

	for(queue = 0; queue < (1<<(MEMCHECK_BITS)); queue++)
	{
		diva_q_init(&memoryQTable[queue]);
	}

#ifdef LATE_DELETION
	diva_q_init(&deletedQ);
#endif
#endif

#ifdef MEMSTATISTICS
	CLEAR(blockSizesSmall);
	CLEAR(blockSizesBig);
	blockSizesHuge = 0;
#endif

	malloc_bytes_current  = 0;
	malloc_bytes_max      = 0;
	malloc_bytes_static   = 0;
	malloc_blocks_current = 0;
	malloc_blocks_max     = 0;
	malloc_blocks_static  = 0;
}

/***********************************************************************************************************/
void *diva_os_malloc(dword flags, dword memSize)
{
	void   *p = NULL;
	dword   allocSize = memSize;

#if defined MEMCHECK || defined MEMBYTECOUNT
	malloc_mgt_t *mp = NULL;

	allocSize += DIVA_ALIGN_SIZE(sizeof(malloc_mgt_t));
#ifdef BOUNDARY_CHECK
	allocSize += 2 * sizeof(boundaryMagic);
#endif
#endif

	p = platform_malloc(flags, allocSize);
	if(p)
	{
		malloc_blocks_current++;

		if(malloc_blocks_max < malloc_blocks_current)
		{
			malloc_blocks_max = malloc_blocks_current;
		}

#ifdef MEMSTATISTICS
		if(memSize <= 256)
		{
			blockSizesSmall[memSize]++;
		}
		
		if(memSize <= 65536)
		{
			blockSizesBig[memSize / 256]++;
		}
		else
		{
			blockSizesHuge++;
		}
#endif

#ifdef MEMCHECK
		mp = (malloc_mgt_t *)p;
		memset(mp, 0, sizeof(malloc_mgt_t));
		mp->numOfBytesAllocated = memSize;
		mp->mallocID = mallocID++;
		malloc_bytes_current += memSize;
		if(malloc_bytes_max < malloc_bytes_current)
		{
			malloc_bytes_max = malloc_bytes_current;
		}

		mp->data = (byte*)mp + DIVA_ALIGN_SIZE(sizeof(malloc_mgt_t));
#ifdef BOUNDARY_CHECK
		mp->data += DIVA_ALIGN_SIZE(sizeof(boundaryMagic));
		memcpy((char *)mp + DIVA_ALIGN_SIZE(sizeof(malloc_mgt_t)), boundaryMagic, sizeof(boundaryMagic));
		memcpy((char *)mp->data + mp->numOfBytesAllocated, boundaryMagic, sizeof(boundaryMagic));
#endif
		p = mp->data;
		diva_q_add_tail(&memoryQTable[MEMCHECK_OFFSET(mp->data)], &mp->link);
		dbg_msg(DBG_MALLOC, "diva_os_malloc mp:%p ID:%06d data:%p size:%d(%d) Total:%d TotalMem:%d Bytes", 
			mp, mp->mallocID, mp->data, mp->numOfBytesAllocated, allocSize, malloc_blocks_current, malloc_bytes_current);
#elif defined MEMBYTECOUNT
		malloc_bytes_current += memSize;
		if(malloc_bytes_max < malloc_bytes_current)
		{
			malloc_bytes_max = malloc_bytes_current;
		}

		mp = p;
		p = (byte*)mp + DIVA_ALIGN_SIZE(sizeof(malloc_mgt_t));

		memset(mp, 0, sizeof(malloc_mgt_t));
		mp->numOfBytesAllocated = memSize;
		dbg_msg(DBG_MALLOC, "diva_os_malloc mp:%p ID:%06d data:%p size:%d(%d) Total:%d TotalMem:%d Bytes", 
			mp, 0, p, memSize, allocSize, malloc_blocks_current, malloc_bytes_current);
#else
		dbg_msg(DBG_MALLOC, "diva_os_malloc mp:%p ID:0 size:%d Total:%d", 
			p, allocSize, malloc_blocks_current);
#endif
	}
	else
	{
		dbg_msg(DBG_MALLOC, "diva_os_malloc fail act:0x%x (reqSize=%d)", malloc_blocks_current, memSize);
	}

	return (p);
}

/***************************************************************************************/
void diva_os_free(dword flags, void *p)
{
	if(p)
	{
#ifdef MEMCHECK
		/* Check if this is a valid memory block */
		diva_entity_link_t   *memLink = diva_q_find(&memoryQTable[MEMCHECK_OFFSET(p)], p, memoryDataCmp);
		malloc_mgt_t         *mp = NULL;

		if(NULL == memLink)
		{
			dbg_msg(DBG_ERROR, "**************** FATAL ERROR: Tried to free unoccupied memory (data:%p) !!! *******************", 
				p);
#ifdef _DEBUG
			DebugBreak();
#endif
		}
		else
		{
			mp = DIVAS_CONTAINING_RECORD(memLink, malloc_mgt_t, link);

			dbg_msg(DBG_MALLOC, "diva_os_free   mp:%p ID:%06d data:%p size:%d Total:%d TotalMem:%d Bytes", 
				mp, mp->mallocID, mp->data, mp->numOfBytesAllocated, malloc_blocks_current, malloc_bytes_current);

			malloc_bytes_current -= mp->numOfBytesAllocated;
			if(mp->staticValue)
			{
				malloc_bytes_static -= mp->numOfBytesAllocated;
				malloc_blocks_static--;
			}

			diva_q_remove(&memoryQTable[MEMCHECK_OFFSET(p)], memLink);

#ifdef BOUNDARY_CHECK
			if(0 != memcmp((char*)mp + DIVA_ALIGN_SIZE(sizeof(malloc_mgt_t)), boundaryMagic, sizeof(boundaryMagic)))
			{
				char                  mpCaption[20];

				dbg_msg(DBG_ERROR, "************** ERROR lower bound violation (mp:%p ID:%06d data:%p size:%p) ****************",
					mp, mp->mallocID, mp->data, mp->numOfBytesAllocated);

				sprintf(mpCaption, "[%p/%06d]", mp, mp->mallocID);
				dbg_dump(DBG_ERROR, mp->data, MIN(mp->numOfBytesAllocated, 1024), mpCaption, "", 0);

#ifdef _DEBUG
				DebugBreak();
#endif
			}
			if(0 != memcmp((char*)mp->data + mp->numOfBytesAllocated, boundaryMagic, sizeof(boundaryMagic)))
			{
				char                  mpCaption[20];

				dbg_msg(DBG_ERROR, "************** ERROR upper bound violation (mp:%p ID:%06d data:%p size:%p) ****************", 
					mp, mp->mallocID, mp->data, mp->numOfBytesAllocated);

				sprintf(mpCaption, "[%p/%06d]", mp, mp->mallocID);
				dbg_dump(DBG_ERROR, mp->data, MIN(mp->numOfBytesAllocated, 1024), mpCaption, "", 0);
#ifdef _DEBUG
				DebugBreak();
#endif
			}
#endif

#ifdef LATE_DELETION
			memset(p, DELETION_MAGIC_BYTE, mp->numOfBytesAllocated);  /* Delete memory to find too late memory access */
			diva_q_add_tail(&deletedQ, memLink);
#else
			memset(mp, 0x00, sizeof(malloc_mgt_t));
			platform_free(flags, mp);
#endif
		}

#elif defined MEMBYTECOUNT
		malloc_mgt_t         *mp = (malloc_mgt_t*)((char*)p - DIVA_ALIGN_SIZE(sizeof(malloc_mgt_t)));

		malloc_bytes_current -= mp->numOfBytesAllocated;
		dbg_msg(DBG_MALLOC, "diva_os_free   mp:%p ID:%06d data:%p size:%d Total:%d TotalMem:%d Bytes", 
			mp, 0, p, mp->numOfBytesAllocated, malloc_blocks_current, malloc_bytes_current);
		memset(mp, 0x00, sizeof(malloc_mgt_t));
		platform_free(flags, mp);
#else
		dbg_msg(DBG_MALLOC, "diva_os_free   mp:%p ID:0 Total:%d", 
			p, malloc_blocks_current);
		platform_free(flags, p);
#endif
		malloc_blocks_current--;

	}

}

/***********************************************************************************************************/
void diva_malloc_emergency_free(void)
{ 
#ifdef MEMCHECK
  dword                 queue = 0;
	
	for(queue = 0; queue < (1<<(MEMCHECK_BITS)); queue++)
	{
		diva_entity_link_t   *memLink = NULL;

		while((memLink = diva_q_get_head(&memoryQTable[queue])) != NULL) 
		{
			malloc_mgt_t         *mp = NULL;
			diva_q_remove(&memoryQTable[queue], memLink);
			mp = DIVAS_CONTAINING_RECORD(memLink, malloc_mgt_t, link);
			platform_free(0, mp);
		}
	}
#endif
}

/***************************************************************************************/
void diva_malloc_mem_set_static(void)
{
#ifdef MEMCHECK
  dword                 queue = 0;
	
	for(queue = 0; queue < (1<<(MEMCHECK_BITS)); queue++)
	{
		diva_entity_link_t   *memLink = NULL;

		memLink = diva_q_get_head(&memoryQTable[queue]);
		while(NULL != memLink)
		{
			malloc_mgt_t         *mp = NULL;

			mp = DIVAS_CONTAINING_RECORD(memLink, malloc_mgt_t, link);
			mp->staticValue = TRUE;

			memLink = diva_q_get_next(memLink);
		}
	}
#endif

	malloc_bytes_static = malloc_bytes_current;
	malloc_blocks_static = malloc_blocks_current;
}

/***********************************************************************************************************/
void diva_malloc_memstat_print(void)
{
#ifdef MEMCHECK
	diva_entity_link_t   *memLink = NULL;
	char                  mpCaption[20];
	dword                 queue = 0;

	dbg_msg(DBG_INFO, "******************** Memory usage: %u bytes in %u blocks (non-static: %u bytes in %u blocks) ********************", 
		malloc_bytes_current, malloc_blocks_current, malloc_bytes_current - malloc_bytes_static, malloc_blocks_current - malloc_blocks_static);

	for(queue = 0; queue < (1<<(MEMCHECK_BITS)); queue++)
	{
		memLink = diva_q_get_head(&memoryQTable[queue]);
		while(NULL != memLink)
		{
			malloc_mgt_t         *mp = NULL;

			mp = DIVAS_CONTAINING_RECORD(memLink, malloc_mgt_t, link);
			if(!mp->staticValue)
			{
				dbg_msg(DBG_INFO, "Used block mp:%p ID:%06u data:%p size:%u q:%u", 
					mp, mp->mallocID, mp->data, mp->numOfBytesAllocated, queue);
				sprintf(mpCaption, "[%p/%06u]", mp, mp->mallocID);
				dbg_dump(DBG_MALLOC, mp->data, MIN(mp->numOfBytesAllocated, 1024), mpCaption, "", 0);
				if(mp->numOfBytesAllocated > 1024)
				{
					dbg_msg(DBG_MALLOC, "%s[%u more bytes...]", mpCaption, mp->numOfBytesAllocated - 1024);
				}
			}

			memLink = diva_q_get_next(memLink);
		}
	}

#endif

#ifdef MEMSTATISTICS
	{
		dword                 row = 0;

		dbg_msg(DBG_INFO, "Memory block sizes requested:");
		
		dbg_msg(DBG_INFO, " %-5d:%-8d <%-5d:%-8d >%-5d:%-8d", 0, blockSizesSmall[0], 256, blockSizesBig[0], 65536, blockSizesHuge);
		for(row = 1; row <= 256; row += 8)
		{
			dbg_msg(DBG_INFO, " %-5d:%-8d  %-5d:%-8d  %-5d:%-8d  %-5d:%-8d  %-5d:%-8d  %-5d:%-8d  %-5d:%-8d  %-5d:%-8d",
				row+0, blockSizesSmall[row+0], row+1, blockSizesSmall[row+1], 
				row+2, blockSizesSmall[row+2], row+3, blockSizesSmall[row+3], 
				row+4, blockSizesSmall[row+4], row+5, blockSizesSmall[row+5], 
				row+6, blockSizesSmall[row+6], row+7, blockSizesSmall[row+7]);
		}
		for(row = 1; row <= 256; row += 8)
		{
			dbg_msg(DBG_INFO, "<%-5d:%-8d <%-5d:%-8d <%-5d:%-8d <%-5d:%-8d <%-5d:%-8d <%-5d:%-8d <%-5d:%-8d <%-5d:%-8d",
				(row+1)*256, blockSizesBig[row+0], (row+2)*256, blockSizesBig[row+1], 
				(row+3)*256, blockSizesBig[row+2], (row+4)*256, blockSizesBig[row+3], 
				(row+5)*256, blockSizesBig[row+4], (row+6)*256, blockSizesBig[row+5], 
				(row+7)*256, blockSizesBig[row+6], (row+8)*256, blockSizesBig[row+7]);
		}
	}
#endif
}


/***************************************************************************************/
void diva_malloc_checkdeleted(void)
{
#ifdef MEMCHECK
#ifdef LATE_DELETION
	diva_entity_link_t   *memLink = NULL;
	char                  mpCaption[20];

	memLink = diva_q_get_head(&deletedQ);
	while(NULL != memLink)
	{
		malloc_mgt_t         *mp = NULL;
		dword                 offset = 0;

		mp = DIVAS_CONTAINING_RECORD(memLink, malloc_mgt_t, link);

#ifdef BOUNDARY_CHECK
		if(0 != memcmp((char*)mp + sizeof(malloc_mgt_t), boundaryMagic, sizeof(boundaryMagic)))
		{
			dbg_msg(DBG_ERROR, "************** ERROR lower bound violation in deleted object (mp:%p ID:%06d data:%p size:%p) ****************", 
				mp, mp->mallocID, mp->data, mp->numOfBytesAllocated);
#ifdef _DEBUG
      DebugBreak();
#endif
		}
		if(0 != memcmp((char *)mp->data + mp->numOfBytesAllocated, boundaryMagic, sizeof(boundaryMagic)))
		{
			dbg_msg(DBG_ERROR, "************** ERROR upper bound violation in deleted object (mp:%p ID:%06d data:%p size:%p) ****************",
				mp, mp->mallocID, mp->data, mp->numOfBytesAllocated);
#ifdef _DEBUG
      DebugBreak();
#endif
		}
#endif

		for(offset = 0; offset < mp->numOfBytesAllocated; offset++)
		{
			if(mp->data[offset] != DELETION_MAGIC_BYTE)
			{
				dbg_msg(DBG_ERROR, "**** ERROR write detected in deleted object (mp:%p ID:%06d data:%p size:%p offset:%d value:0x%x) ****",
					mp, mp->mallocID, mp->data, mp->numOfBytesAllocated, offset, mp->data[offset]);
				sprintf(mpCaption, "{%06d}", mp);
				dbg_dump(DBG_MALLOC, mp->data, MIN(mp->numOfBytesAllocated, 1024), mpCaption, "", 0);
				if(mp->numOfBytesAllocated > 1024)
				{
					dbg_msg(DBG_MALLOC, "[%d more bytes...]", mp->numOfBytesAllocated-1024);
				}
#ifdef _DEBUG
				DebugBreak();
#endif
				break;
			}
		}
		memLink = diva_q_get_next(memLink);
		diva_q_remove(&deletedQ, &mp->link);
		memset(mp, 0x00, sizeof(malloc_mgt_t));
		platform_free(0, mp);
	}
#endif
#endif
}


/***************************************************************************************/
void diva_malloc_checkactive(void)
{
#ifdef MEMCHECK
#ifdef LATE_DELETION
	dword                 queue = 0;

	for(queue = 0; queue < (1<<(MEMCHECK_BITS)); queue++)
	{
		diva_entity_link_t   *memLink = NULL;

		memLink = diva_q_get_head(&memoryQTable[queue]);
		while(NULL != memLink)
		{
			malloc_mgt_t         *mp = DIVAS_CONTAINING_RECORD(memLink, malloc_mgt_t, link);

#ifdef BOUNDARY_CHECK
			if(0 != memcmp((char*)mp + sizeof(malloc_mgt_t), boundaryMagic, sizeof(boundaryMagic)))
			{
				dbg_msg(DBG_ERROR, "************** ERROR lower bound violation in active object (Q:%u mp:%p ID:%06d data:%p size:%p) ****************", 
					queue, mp, mp->mallocID, mp->data, mp->numOfBytesAllocated);
#ifdef _DEBUG
				DebugBreak();
#endif
			}
			if(0 != memcmp((char *)mp->data + mp->numOfBytesAllocated, boundaryMagic, sizeof(boundaryMagic)))
			{
				dbg_msg(DBG_ERROR, "************** ERROR upper bound violation in active object (Q:%u mp:%p ID:%06d data:%p size:%p) ****************",
					queue, mp, mp->mallocID, mp->data, mp->numOfBytesAllocated);
#ifdef _DEBUG
				DebugBreak();
#endif
			}
#endif

			memLink = diva_q_get_next(memLink);
		}
	}
#endif
#endif
}


/***********************************************************************************************************/
dword diva_malloc_mem_bytes_current(void)
{
	return malloc_bytes_current;
}

/***********************************************************************************************************/
dword diva_malloc_mem_bytes_max(void)
{
	return malloc_bytes_max;
}

/***********************************************************************************************************/
dword diva_malloc_mem_bytes_static(void)
{
	return malloc_bytes_static;
}

/***********************************************************************************************************/
dword diva_malloc_mem_blocks_current(void)
{
	return malloc_blocks_current;
}

/***********************************************************************************************************/
dword diva_malloc_mem_blocks_max(void)
{
	return malloc_blocks_max;
}

/***********************************************************************************************************/
dword diva_malloc_mem_blocks_static(void)
{
	return malloc_blocks_static;
}

/***********************************************************************************************************/
void diva_malloc_deinit(void)
{
}




