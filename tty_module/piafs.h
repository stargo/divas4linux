
/*
 *
  Copyright (c) Sangoma Technologies, 2018-2024
  Copyright (c) Dialogic(R), 2009-2014.
 *
  This source file is supplied for the use with
  Sangoma (formerly Dialogic) range of DIVA Server Adapters.
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

#ifndef __DIVA_PIAFS_DETECTOR_H__
#define __DIVA_PIAFS_DETECTOR_H__

#define CRC_LENGTH 4
#define FRAME_LENGTH 80
#define BIT_0           0x00000001                      /* bit position 0  */
#define Crc32MsbReset(InitValue, CurrentCRC)     *(CurrentCRC) = InitValue
#define BIT_15          0x00008000                      /* bit position 15 */
#define piafsframing_BIT_STREAM_RX 1

#define Crc32MsbConfigure(InitValue, CurrentCRC) *(CurrentCRC) = InitValue

typedef struct dataqStatusInfo
{
    dword All;    
} dataqStatusInfo_t;

typedef struct PiafsFrmVar_s {
		/*
			Receiver stuff 
			*/
    byte                RxState;
    byte                PreviousRxState;
    dword               syncHistory;
    int                 JustSynchronized;

    byte   PreShiftCount;
    word   PreShiftRegister;
    byte   RateAdaptationEnable;
    byte   IsThereABytePending;
    byte   BytePending;
    byte   BitsToLeftShift;
    byte   CurrentRxShiftRegister;
    byte   huntPhaseSyncLag[7];
    byte   HuntPhaseSyncLagIndex;

    byte                *DQRxBuffer;
    word                DQRxBufLen;
    word                DQRxBufIdx;
    dword  CurrentSynchroCandidate;

    dword                   RX_CRC;
    dword               ReceivedCRC;
    byte                NumberOfReceivedCRCBytes;
    word                IdxInRxFrame;

    // FramingStatusInfo_t *DQRxStatusInfo;

		byte rx_frame[FRAME_LENGTH+4];

} PiafsFrm_t;

typedef enum _diva_piafs_speed {
	DivaPiafs64K = 0,
	DivaPiafs32K = 1
} diva_piafs_speed_t;
#define DIVA_NR_OF_PIAFS_DETECTORS 2

typedef struct {
	PiafsFrm_t fr2;
	void* C;
	void* P;
	const char* name;
} L2;

void init_piafs_detectors (L2* this, void* C, void* P);
int piafs_detector_process(L2* this, byte* pData, word length,
													 dword* /* OUT */ speed, dword* /* OUT */ rate);

#endif
