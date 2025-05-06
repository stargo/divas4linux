
/*
 *
  Copyright (c) Sangoma Technologies, 2018-2024
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

#ifndef __DIVA_HSI_INTERFACE_H__
#define __DIVA_HSI_INTERFACE_H__

#define HSI_MAX_BRIDGES             ( 32 )
#define HSI_MAX_BUFFERS             ( 32 )

#define HSI_IOCTL_PACK_CODE( Dev, Code )       ( _IO( Dev, Code ))

#define HSI_FILE_DEVICE						0x53
#define HSI_BRIDGE_INIT_IOCTL				HSI_IOCTL_PACK_CODE( HSI_FILE_DEVICE, 0x1 )
#define HSI_BRIDGE_STOP_IOCTL				HSI_IOCTL_PACK_CODE( HSI_FILE_DEVICE, 0x2 )
#define HSI_INTR_RATE_ENABLE_IOCTL		HSI_IOCTL_PACK_CODE( HSI_FILE_DEVICE, 0x4 )
#define HSI_INTR_RATE_DISABLE_IOCTL		HSI_IOCTL_PACK_CODE( HSI_FILE_DEVICE, 0x5 )

typedef void (*HSI_ISR_CALLBACK)( int bridgeId, int hsiBufferIndex );
typedef void (*HSI_SYNC_CALLBACK)( int bridgeId );

typedef enum _tagHSI_START_TYPE
{
	HSI_START_ON_INTR_ENABLE = 0,
	HSI_START_ON_SYNC        = 1
}
HSI_START_TYPE, *PHSI_START_TYPE;


typedef enum _tagHSI_BUS_TYPE
{
	HSI_BUS_TYPE_TRANSPARENT = 0,
	HSI_BUS_TYPE_ALAW        = 1,
	HSI_BUS_TYPE_MULAW       = 2
}
HSI_BUS_TYPE, *PHSI_BUS_TYPE;

typedef enum _tagHSI_TICK_RATE
{
	HSI_RATE_2MS = 2,
	HSI_RATE_4MS = 4
}
HSI_TICK_RATE, *PHSI_TICK_RATE;

typedef enum _tagHSI_RESPONSE_TYPE
{
	HSI_BRIDGE_INIT_COMPLETION = 0,
	HSI_BRIDGE_ROUTE_COMPLETION,
	HSI_BRIDGE_STOP_COMPLETION,
	HSI_BRIDGE_INTR_ENABLE_COMPLETION,
	HSI_BRIDGE_INTR_DISABLE_COMPLETION
}
HSI_RESPONSE_TYPE, *PHSI_RESPONSE_TYPE;

typedef enum _tagHSI_PORT_ALLOCATION_METHOD
{
	HSI_ALLOCATION_METHOD_0 = 0,
	HSI_ALLOCATION_METHOD_1,
	HSI_ALLOCATION_METHOD_DYNAMIC
}
HSI_PORT_ALLOCATION_METHOD, *PHSI_PORT_ALLOCATION_METHOD;

typedef struct _tagHSI_BRIDGE_INIT_INPUT
{
	unsigned int               logicalId;
	unsigned int               bridgeId;
	unsigned int               numTotalPorts;
	unsigned int               numVoicePorts;
	unsigned int               numDataPorts;
	HSI_PORT_ALLOCATION_METHOD portAllocCode;
	unsigned int               numHsiBuffers;
	HSI_TICK_RATE              tickRate;
	HSI_START_TYPE             startType;
	HSI_BUS_TYPE               busType;
	unsigned int               lawIdlePattern;
	unsigned int               syncTimeslot;
	char                       syncPattern[4];
	HSI_ISR_CALLBACK           hmpIsrFunc;
	HSI_SYNC_CALLBACK          hmpSyncComplete;
	void *                     hostToBoardBuffers[HSI_MAX_BUFFERS];
	void *                     boardToHostBuffers[HSI_MAX_BUFFERS];
}
HSI_BRIDGE_INIT_INPUT, *PHSI_BRIDGE_INIT_INPUT;

typedef struct _tagHSI_BRIDGE_INIT_OUTPUT
{
	HSI_RESPONSE_TYPE type;
	unsigned int      bridgeId;
	unsigned int      txDelay;
	unsigned int      rxDelay;
	unsigned int      txSyncDelay;
	unsigned int      rxSyncDelay;
}
HSI_BRIDGE_INIT_OUTPUT, *PHSI_BRIDGE_INIT_OUTPUT;

typedef struct _tagHSI_BRIDGE_STOP_OUTPUT
{
	HSI_RESPONSE_TYPE type;
	unsigned int      bridgeId;
}
HSI_BRIDGE_STOP_OUTPUT, *PHSI_BRIDGE_STOP_OUTPUT;

typedef struct _tagHSI_BRIDGE_INTR_ENABLE_OUTPUT
{
	HSI_RESPONSE_TYPE type;
	unsigned int      bridgeId;
}
HSI_BRIDGE_INTR_ENABLE_OUTPUT, *PHSI_BRIDGE_INTR_ENABLE_OUTPUT;

typedef struct _tagHSI_BRIDGE_INTR_DISABLE_OUTPUT
{
	HSI_RESPONSE_TYPE type;
	unsigned int      bridgeId;
}
HSI_BRIDGE_INTR_DISABLE_OUTPUT, *PHSI_BRIDGE_INTR_DISABLE_OUTPUT;


#endif

