
/*
 *
  Copyright (c) Dialogic(R), 2009.
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

#include "platform.h"
#include "debuglib.h"
#include "piafs.h"
#include "isdn_if.h"

/*
	Imports
	*/
extern char* get_port_name (void* hP);
#define port_name(__x__)  ((__x__) ? get_port_name(__x__) : "?")
extern void* mem_set (void* s1, int c, unsigned long len);


#define HUNT_STATE      0
#define DATA_STATE      1
#define CRC_STATE       2
#define PRE_HUNT_STATE	3


#define SYNC_FOUND             10

/*
	LOCALS
	*/
typedef struct _piafs_detector_setup  {
	diva_piafs_speed_t piafs_speed;
	dword              idi_speed;
	int                idi_baud;
	int								 idi_prot;
} piafs_detector_setup_t;

static piafs_detector_setup_t setup[DIVA_NR_OF_PIAFS_DETECTORS] = {
 { DivaPiafs64K, 64000, 14, ISDN_PROT_PIAFS_64K_VAR },
 { DivaPiafs32K, 32000, 57, ISDN_PROT_PIAFS_32K     }
};

static byte PiafsPreShiftBitRx(PiafsFrm_t * internal, byte x, byte y);
static byte PiafsAcquirePreSync(PiafsFrm_t * internal, byte x, byte y);
static byte PiafsShiftBitRx(PiafsFrm_t * internal, byte x);
static int PiafsProcessIncomingData(L2 * this, byte x);
static int GenericDQOpenWriteRxQueue (PiafsFrm_t* context,
                              byte ** BufferPtr,
                              word * Length);
static void Crc32MsbFunc(dword *Crc32Msb);
static void init_piafs_process (L2* this, void* C, void* P, diva_piafs_speed_t speed);

static byte invert_byte_table [256] = {
0x00,0x80,0x40,0xC0,0x20,0xA0,0x60,0xE0,0x10,0x90,0x50,0xD0,0x30,0xB0,0x70,0xF0,
0x08,0x88,0x48,0xC8,0x28,0xA8,0x68,0xE8,0x18,0x98,0x58,0xD8,0x38,0xB8,0x78,0xF8,
0x04,0x84,0x44,0xC4,0x24,0xA4,0x64,0xE4,0x14,0x94,0x54,0xD4,0x34,0xB4,0x74,0xF4,
0x0C,0x8C,0x4C,0xCC,0x2C,0xAC,0x6C,0xEC,0x1C,0x9C,0x5C,0xDC,0x3C,0xBC,0x7C,0xFC,
0x02,0x82,0x42,0xC2,0x22,0xA2,0x62,0xE2,0x12,0x92,0x52,0xD2,0x32,0xB2,0x72,0xF2,
0x0A,0x8A,0x4A,0xCA,0x2A,0xAA,0x6A,0xEA,0x1A,0x9A,0x5A,0xDA,0x3A,0xBA,0x7A,0xFA,
0x06,0x86,0x46,0xC6,0x26,0xA6,0x66,0xE6,0x16,0x96,0x56,0xD6,0x36,0xB6,0x76,0xF6,
0x0E,0x8E,0x4E,0xCE,0x2E,0xAE,0x6E,0xEE,0x1E,0x9E,0x5E,0xDE,0x3E,0xBE,0x7E,0xFE,
0x01,0x81,0x41,0xC1,0x21,0xA1,0x61,0xE1,0x11,0x91,0x51,0xD1,0x31,0xB1,0x71,0xF1,
0x09,0x89,0x49,0xC9,0x29,0xA9,0x69,0xE9,0x19,0x99,0x59,0xD9,0x39,0xB9,0x79,0xF9,
0x05,0x85,0x45,0xC5,0x25,0xA5,0x65,0xE5,0x15,0x95,0x55,0xD5,0x35,0xB5,0x75,0xF5,
0x0D,0x8D,0x4D,0xCD,0x2D,0xAD,0x6D,0xED,0x1D,0x9D,0x5D,0xDD,0x3D,0xBD,0x7D,0xFD,
0x03,0x83,0x43,0xC3,0x23,0xA3,0x63,0xE3,0x13,0x93,0x53,0xD3,0x33,0xB3,0x73,0xF3,
0x0B,0x8B,0x4B,0xCB,0x2B,0xAB,0x6B,0xEB,0x1B,0x9B,0x5B,0xDB,0x3B,0xBB,0x7B,0xFB,
0x07,0x87,0x47,0xC7,0x27,0xA7,0x67,0xE7,0x17,0x97,0x57,0xD7,0x37,0xB7,0x77,0xF7,
0x0F,0x8F,0x4F,0xCF,0x2F,0xAF,0x6F,0xEF,0x1F,0x9F,0x5F,0xDF,0x3F,0xBF,0x7F,0xFF};


// Internal routine

static dword Crc32MsbCrcTable[256] = {
0x0,0x4C11DB7,0x9823B6E,0xD4326D9,0x130476DC,0x17C56B6B,
0x1A864DB2,0x1E475005,0x2608EDB8,0x22C9F00F,0x2F8AD6D6,
0x2B4BCB61,0x350C9B64,0x31CD86D3,0x3C8EA00A,0x384FBDBD,
0x4C11DB70,0x48D0C6C7,0x4593E01E,0x4152FDA9,0x5F15ADAC,
0x5BD4B01B,0x569796C2,0x52568B75,0x6A1936C8,0x6ED82B7F,
0x639B0DA6,0x675A1011,0x791D4014,0x7DDC5DA3,0x709F7B7A,
0x745E66CD,0x9823B6E0,0x9CE2AB57,0x91A18D8E,0x95609039,
0x8B27C03C,0x8FE6DD8B,0x82A5FB52,0x8664E6E5,0xBE2B5B58,
0xBAEA46EF,0xB7A96036,0xB3687D81,0xAD2F2D84,0xA9EE3033,
0xA4AD16EA,0xA06C0B5D,0xD4326D90,0xD0F37027,0xDDB056FE,
0xD9714B49,0xC7361B4C,0xC3F706FB,0xCEB42022,0xCA753D95,
0xF23A8028,0xF6FB9D9F,0xFBB8BB46,0xFF79A6F1,0xE13EF6F4,
0xE5FFEB43,0xE8BCCD9A,0xEC7DD02D,0x34867077,0x30476DC0,
0x3D044B19,0x39C556AE,0x278206AB,0x23431B1C,0x2E003DC5,
0x2AC12072,0x128E9DCF,0x164F8078,0x1B0CA6A1,0x1FCDBB16,
0x18AEB13,0x54BF6A4,0x808D07D,0xCC9CDCA,0x7897AB07,
0x7C56B6B0,0x71159069,0x75D48DDE,0x6B93DDDB,0x6F52C06C,
0x6211E6B5,0x66D0FB02,0x5E9F46BF,0x5A5E5B08,0x571D7DD1,
0x53DC6066,0x4D9B3063,0x495A2DD4,0x44190B0D,0x40D816BA,
0xACA5C697,0xA864DB20,0xA527FDF9,0xA1E6E04E,0xBFA1B04B,
0xBB60ADFC,0xB6238B25,0xB2E29692,0x8AAD2B2F,0x8E6C3698,
0x832F1041,0x87EE0DF6,0x99A95DF3,0x9D684044,0x902B669D,
0x94EA7B2A,0xE0B41DE7,0xE4750050,0xE9362689,0xEDF73B3E,
0xF3B06B3B,0xF771768C,0xFA325055,0xFEF34DE2,0xC6BCF05F,
0xC27DEDE8,0xCF3ECB31,0xCBFFD686,0xD5B88683,0xD1799B34,
0xDC3ABDED,0xD8FBA05A,0x690CE0EE,0x6DCDFD59,0x608EDB80,
0x644FC637,0x7A089632,0x7EC98B85,0x738AAD5C,0x774BB0EB,
0x4F040D56,0x4BC510E1,0x46863638,0x42472B8F,0x5C007B8A,
0x58C1663D,0x558240E4,0x51435D53,0x251D3B9E,0x21DC2629,
0x2C9F00F0,0x285E1D47,0x36194D42,0x32D850F5,0x3F9B762C,
0x3B5A6B9B,0x315D626,0x7D4CB91,0xA97ED48,0xE56F0FF,
0x1011A0FA,0x14D0BD4D,0x19939B94,0x1D528623,0xF12F560E,
0xF5EE4BB9,0xF8AD6D60,0xFC6C70D7,0xE22B20D2,0xE6EA3D65,
0xEBA91BBC,0xEF68060B,0xD727BBB6,0xD3E6A601,0xDEA580D8,
0xDA649D6F,0xC423CD6A,0xC0E2D0DD,0xCDA1F604,0xC960EBB3,
0xBD3E8D7E,0xB9FF90C9,0xB4BCB610,0xB07DABA7,0xAE3AFBA2,
0xAAFBE615,0xA7B8C0CC,0xA379DD7B,0x9B3660C6,0x9FF77D71,
0x92B45BA8,0x9675461F,0x8832161A,0x8CF30BAD,0x81B02D74,
0x857130C3,0x5D8A9099,0x594B8D2E,0x5408ABF7,0x50C9B640,
0x4E8EE645,0x4A4FFBF2,0x470CDD2B,0x43CDC09C,0x7B827D21,
0x7F436096,0x7200464F,0x76C15BF8,0x68860BFD,0x6C47164A,
0x61043093,0x65C52D24,0x119B4BE9,0x155A565E,0x18197087,
0x1CD86D30,0x29F3D35,0x65E2082,0xB1D065B,0xFDC1BEC,
0x3793A651,0x3352BBE6,0x3E119D3F,0x3AD08088,0x2497D08D,
0x2056CD3A,0x2D15EBE3,0x29D4F654,0xC5A92679,0xC1683BCE,
0xCC2B1D17,0xC8EA00A0,0xD6AD50A5,0xD26C4D12,0xDF2F6BCB,
0xDBEE767C,0xE3A1CBC1,0xE760D676,0xEA23F0AF,0xEEE2ED18,
0xF0A5BD1D,0xF464A0AA,0xF9278673,0xFDE69BC4,0x89B8FD09,
0x8D79E0BE,0x803AC667,0x84FBDBD0,0x9ABC8BD5,0x9E7D9662,
0x933EB0BB,0x97FFAD0C,0xAFB010B1,0xAB710D06,0xA6322BDF,
0xA2F33668,0xBCB4666D,0xB8757BDA,0xB5365D03,0xB1F740B4};

static byte shift_table[0x100] =
{
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 3, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 5, 8, 8, 8,
  8, 8, 8, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 2, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 4, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 6, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 1, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8
};


static word Fr2PiafsFramingDecode(L2 *this, byte *DataToDecode, word NbBytes, int* detected)
{
  PiafsFrm_t * internal =  &this->fr2;

  word ReadIndex = 0; /* index where we must read next byte in users's buffer */
  word PreSyncIndex = 0;
  word NbBytesSaved = NbBytes;
  byte ShiftedByte;

  while (NbBytes) 
  {
    if( internal->RateAdaptationEnable == TRUE )
    {
      if( NbBytes > 1 )
      {
        if( internal->IsThereABytePending )
        {
          internal->IsThereABytePending = FALSE;
          ShiftedByte = PiafsPreShiftBitRx(internal, internal->BytePending, DataToDecode[ReadIndex] );
          ReadIndex++;
          NbBytes--;
        }
        else
        {
          ShiftedByte = PiafsPreShiftBitRx(internal, DataToDecode[ReadIndex],DataToDecode[ReadIndex+1] );
          NbBytes-=2;
          ReadIndex+=2;
        }
      }
      else
      {
        internal->BytePending = DataToDecode[ReadIndex];
        internal->IsThereABytePending = TRUE;
        NbBytes--;
        ReadIndex++;
        break;
      }
    }
    else
    {
      ShiftedByte = DataToDecode[ReadIndex];
      NbBytes--;
      ReadIndex++;
    }


    switch(internal->RxState) 
    {
      case PRE_HUNT_STATE:
        if( PiafsAcquirePreSync(internal, DataToDecode[PreSyncIndex], DataToDecode[PreSyncIndex+1] ) )
        {
          internal->PreviousRxState = PRE_HUNT_STATE;
          internal->RxState = HUNT_STATE;
          if(ReadIndex <= 10) {
            ReadIndex = 0;
            NbBytes = NbBytesSaved;
          }
          else {
            ReadIndex -= 10;
            NbBytes   += 10;
          }            
          internal->DQRxBufIdx = 0;

					DBG_TRC(("[%p:%s:%s] PIAFS - PreSYNC found %d",
									this->C, port_name(this->P), this->name, ReadIndex))
        }
        else
        {
          PreSyncIndex += 2;
        }
        break;

      case HUNT_STATE:
        {
          byte current_byte  = ShiftedByte;
          byte current_shift = shift_table[internal->syncHistory & 0xff];
          if (((internal->syncHistory << current_shift) | (current_byte >> (8 - current_shift))) == 0x50ef2993)
          {
            byte i;

            //LLDirectMessage(this, SYNC_FOUND,(void *)internal->DQTxCtrlInfo, (void *)NULL);

            /* Synchro found */
            internal->RxState                = DATA_STATE;
            internal->JustSynchronized       = TRUE;
            internal->BitsToLeftShift	     = current_shift;
            internal->CurrentRxShiftRegister = 0;
						DBG_TRC(("[%p:%s:%s] PIAFS - Got SYNC with %d bits to left shift: %d",
										this->C, port_name(this->P), this->name, internal->BitsToLeftShift, ReadIndex))

            /* Initialize Shift register */
            PiafsShiftBitRx(internal, internal->huntPhaseSyncLag[internal->HuntPhaseSyncLagIndex]);                                            

            /* Process Initial Bytes 3 leading bytes + 4 sync bytes */
            for(i=0; i < 6; i++) {
              internal->HuntPhaseSyncLagIndex = (internal->HuntPhaseSyncLagIndex + 1)%7;
              PiafsProcessIncomingData(this, PiafsShiftBitRx(internal, internal->huntPhaseSyncLag[internal->HuntPhaseSyncLagIndex]));                                            
            }
            PiafsProcessIncomingData(this, PiafsShiftBitRx(internal, ShiftedByte));                                            

            break;
          }

          internal->huntPhaseSyncLag[internal->HuntPhaseSyncLagIndex] = ShiftedByte;
          internal->HuntPhaseSyncLagIndex = (internal->HuntPhaseSyncLagIndex + 1)%7;

          internal->syncHistory = (internal->syncHistory << 8) | current_byte;
        }
        break;
		 
      case DATA_STATE:
        {
          byte result = internal->CurrentRxShiftRegister | 
                                  (ShiftedByte >> (8 - internal->BitsToLeftShift));

          internal->CurrentRxShiftRegister = 
                        ShiftedByte << internal->BitsToLeftShift;

          if (internal->DQRxBuffer == NULL)
          {
            internal->DQRxBufIdx = 0;
            GenericDQOpenWriteRxQueue(internal, &(internal->DQRxBuffer), &(internal->DQRxBufLen));
          }                        

          if(internal->DQRxBuffer != NULL) {
            /* At this point we have a Rx DQ Buffer properly opened */

            /* Copy Data */
  					if (internal->DQRxBufIdx >= 80) {
							DBG_ERR(("[%p:%s:%s] frame buffer overflow at %d",
											 this->C, port_name(this->P), this->name, __LINE__))
							internal->DQRxBufIdx = 0;
						}
            internal->DQRxBuffer[internal->DQRxBufIdx] = result;

            /* Compute CRC */
            internal->RX_CRC = (internal->RX_CRC<<8) ^ Crc32MsbCrcTable[(internal->RX_CRC>>24) ^result];

            internal->DQRxBufIdx++;
            internal->IdxInRxFrame++;
                                                        
            /* Are we at the end of the frame ? */
            if (internal->IdxInRxFrame == (FRAME_LENGTH - CRC_LENGTH))
            {
              /* Yes, we are */

              /* Let the H/W driver know if we just synchronized, so that data transfer is optimized */
              if (TRUE == internal->JustSynchronized)
              { 
                internal->JustSynchronized = FALSE;
                {
                  //dword frm_len = FRAME_LENGTH;
                  //LLDirectMessage(this, CHANGE_RXBLOCK_LEN, (void *) &frm_len, (void *)NULL); 
                }
              }                                                            
              if(internal->RxState == DATA_STATE) internal->RxState = CRC_STATE;                   
            }
            internal->ReceivedCRC = 0;
            internal->NumberOfReceivedCRCBytes = 0;
          }
          else
          {
            return (NbBytesSaved - NbBytes);
          }
        }
        break;
  /*----------------------------*/
      case CRC_STATE:
         {  
           byte result = internal->CurrentRxShiftRegister | (ShiftedByte >> (8 - internal->BitsToLeftShift));
 
           internal->CurrentRxShiftRegister = ShiftedByte << internal->BitsToLeftShift;
           internal->ReceivedCRC <<= 8;
           internal->ReceivedCRC |= result;
           internal->NumberOfReceivedCRCBytes++;
                
           if (internal->NumberOfReceivedCRCBytes == CRC_LENGTH)
           {                                                
						 int CrcError = 0;
             if (internal->RX_CRC != internal->ReceivedCRC )
             {
							 DBG_TRC(("[%p:%s:%s] PIAFS - Bad CRC: %d 0x%02x 0x%02x",
											 this->C, port_name(this->P), this->name,
											 ReadIndex, internal->PreviousRxState, internal->RxState))
							 CrcError = 1;
             }
             else
             {
               /* CRC OK */
               if(internal->PreviousRxState != DATA_STATE) { /* first good frame received */
							 	DBG_TRC(("[%p:%s:%s] PIAFS - first good CRC",
												this->C, port_name(this->P), this->name))
               }
							 *detected = 1;
							 return (0);
             }                
 
             if(internal->PreviousRxState == PRE_HUNT_STATE) {
               if(CrcError) {
								 DBG_TRC(("[%p:%s:%s] PIAFS - back into prehunt state",
													this->C, port_name(this->P), this->name))
                 internal->RxState = PRE_HUNT_STATE;
               }
               else {
                 if(internal->RxState == CRC_STATE) internal->PreviousRxState = DATA_STATE;
               }
             }
            
             internal->DQRxBuffer = NULL;
             internal->IdxInRxFrame = 0;
             if(internal->RxState == CRC_STATE) internal->RxState = DATA_STATE;                   
             Crc32MsbReset(0xFFFFFFFF, &(internal->RX_CRC));
             /*
								Sync gained and valid frame is received
								*/
           }
         }
         break;
       default:
				 DBG_TRC(("[%p:%s:%s] PIAFS - Invalid RX state",
									this->C, port_name(this->P), this->name))
         return (NbBytesSaved - NbBytes);
    }          
  }
  if(   (!((internal->RxState == DATA_STATE)||(internal->RxState == CRC_STATE))) 
     && (internal->PreviousRxState == PRE_HUNT_STATE)) {
		DBG_TRC(("[%p:%s:%s] 0Back into pre hunt", this->C, port_name(this->P), this->name))
    internal->RxState = PRE_HUNT_STATE;
  }
 
  return (NbBytesSaved - NbBytes);
}

/*=========================================================================
  Function:    PreShiftBitRx(byte x)


  Purpose:      
        Left shift incoming word, internal->PreShiftCount times.
        Assemble new byte from the incoming word.
  Input:	
        x: the first byte of the word to left shift
        y: the second byte of the word to left shift
  Output:	
    None
  Return: 
    Assembled byte
  =========================================================================*/
static byte PiafsPreShiftBitRx(PiafsFrm_t * internal, byte x, byte y)
{
  byte result = 0,i;
  word z;

  z = x<<8;
  z += y;
  if( internal->PreShiftCount > 4 )
  {
    internal->PreShiftRegister <<= (internal->PreShiftCount%4);
    internal->PreShiftRegister += z >> (20-internal->PreShiftCount); /* 16-(internal->PreShiftCount-4) */

    result = (byte)internal->PreShiftRegister;

    internal->PreShiftRegister = (z >> (12-internal->PreShiftCount)) & 0x0f;

    for( i=0; i<(8-internal->PreShiftCount); i++ )
    {
      internal->PreShiftRegister <<= 1;
      if( (z>>(7-i-internal->PreShiftCount)) & BIT_0 )
      internal->PreShiftRegister |= BIT_0;
    }
  }
  else
  {
    result = (z >> (8 - internal->PreShiftCount)) & 0xf0;
    result |= (z >> (4 - internal->PreShiftCount)) & 0x0f;
  }
    
  return result;
}

/*=========================================================================
  Function:    AcquirePreSync(byte x)


  Purpose:      
            Attempt to lock on the Piafs first 4 bits synchro string and 
            determine the bit shift to apply to subsquent bytes read
  Input:	
        x: the first byte to analyze
        y: the second byte to analyze
  Output:	
    None
  Return: 
    0 if no synchro occured
    1 otherwise
  =========================================================================*/
static byte PiafsAcquirePreSync(PiafsFrm_t * internal, byte x, byte y)
{
  word z;
  byte i;
 
  z = x<<8;
  z += y;

  for (i=1; i <= 16; i++)
  {
    /* Acquire pre-shift according to mask */
    internal->PreShiftRegister<<=1;
    if (z & BIT_15)
    {
      internal->PreShiftRegister |= BIT_0;
    }
    
    z<<=1;            

    if ((internal->PreShiftRegister|0x0f0f) == 0x5f0f)
    {
      internal->PreShiftCount = i%8;
      internal->PreShiftRegister = 0;
      return(TRUE);
    }    
  }
  return( FALSE );    
}

/*=========================================================================
  Function:    ShiftBitRx(byte x)


  Purpose:      
        Left shift incoming byte, internal->BitsToLeftShift times.
        Assemble new byte from the incoming one and the previous one.
  Input:	
        x: the byte to left shift/add to history
  Output:	
    None
  Return: 
    Assembled byte
  =========================================================================*/
static byte PiafsShiftBitRx(PiafsFrm_t * internal, byte x)
{
  byte result = 0;

  result = internal->CurrentRxShiftRegister | (x >> (8 - internal->BitsToLeftShift));

  internal->CurrentRxShiftRegister = x << internal->BitsToLeftShift;
    
  return result;
}       

/*=========================================================================
  Function:    ProcessIncomingData(byte x)


  Purpose:      
            Process Incoming Data from H/W when in data phase.  This does 
            not handle synchronization
  Input:	
        x: the byte to process
  Output:	
    None
  Return: 
    TRUE, if successful,
    FALSE, otherwise
  =========================================================================*/
static int PiafsProcessIncomingData(L2 * this, byte x)
{
  PiafsFrm_t * internal =  &this->fr2;

  if (internal->DQRxBuffer == NULL)
  {
    internal->DQRxBufIdx = 0;
    GenericDQOpenWriteRxQueue(internal, &(internal->DQRxBuffer), &(internal->DQRxBufLen));
  }                        

  /* At this point we have a Rx DQ Buffer properly opened */

	if (internal->DQRxBufIdx >= 80) {
		DBG_ERR(("[%p:%s:%s] frame buffer overflow at %d",
						this->C, port_name(this->P), this->name, __LINE__))
		internal->DQRxBufIdx = 0;
	}

  /* Copy Data */
  internal->DQRxBuffer[internal->DQRxBufIdx] = x;

  /* Compute CRC if necessary */
  /* Crc32MsbAddByte(internal->DQRxBuffer[internal->DQRxBufIdx], &(internal->RX_CRC));                */
  internal->RX_CRC = (internal->RX_CRC<<8) ^ Crc32MsbCrcTable[(internal->RX_CRC>>24) ^internal->DQRxBuffer[internal->DQRxBufIdx]];

  internal->DQRxBufIdx++;
  internal->IdxInRxFrame++;
                                                        
  /* Are we at the end of the frame ? */
  if (internal->IdxInRxFrame == (FRAME_LENGTH - CRC_LENGTH))
  {
    /* Yes, we are */
    
    /* Let the H/W driver know if we just synchronized, 
       so that data transfer is optimized */
    if (TRUE == internal->JustSynchronized)
    {
      internal->JustSynchronized = FALSE;
        
    }     

    /* Do we have to verify CRC */
    //if(internal->RxState == CRC_STATE) internal->RxState = CRC_STATE;                   
    if(internal->RxState == DATA_STATE) internal->RxState = CRC_STATE;                   
    internal->ReceivedCRC = 0;
    internal->NumberOfReceivedCRCBytes = 0;
  }

  return TRUE;
}

/* =====================================================================
Function: GenericDQOpenWriteRxQueue

Verifies if there is an available and valid BD in the receive queue. If
there is one, it is reserved and the returned value is TRUE. Resulting
from this call, the lower layer will get the data buffer starting
address, its length in bytes and the address of the related status
information structure. 

When the lower layer is done with this BD, it will close it by calling
CloseWriteRxQueue.

Input: n/a

Output: ** BufferPtr    Starting address of the first available buffer.
         * Length        Buffer length.
        ** StatusPtr    Address of the status information structure.

Returns:    TRUE    There is an available buffer.
            FALSE   There is no buffer free or buffer is NULL
======================================================================== */
static int GenericDQOpenWriteRxQueue (PiafsFrm_t* context,
                              byte ** BufferPtr,
                              word * Length) {
  /* Initialize output information */
  *BufferPtr = &context->rx_frame[0];
  *Length    = 80;

	return TRUE;
}

void init_piafs_detectors (L2* this, void* C, void* P) {
	int i;

	for (i = 0; i < DIVA_NR_OF_PIAFS_DETECTORS; i++) {
		init_piafs_process (&this[i], C, P, setup[i].piafs_speed);
	}
}

static byte buf[4096];

int piafs_detector_process (L2* this, byte* pData, word length,
														dword* /* OUT */ speed,
														dword* /* OUT */ rate) {
	word consumed;
	word i;
	byte* p = buf;
	int detected = 0;

	for (i = 0; i < length; i++) {
		buf[i] = invert_byte_table[pData[i]];
	}

	while (length) {
		for (i = 0; i < DIVA_NR_OF_PIAFS_DETECTORS; i++) {
			consumed = Fr2PiafsFramingDecode(&this[i], p, length, &detected);
			if (detected) {
				*speed = setup[i].idi_speed;
				*rate  = setup[i].idi_baud;
				return (setup[i].idi_prot);
			}
		}
		length -= MIN(length,consumed);
		p  += consumed;
	}

	return (0);
}


static void init_piafs_process (L2* this, void* C, void* P, diva_piafs_speed_t piafs_cfg_speed) {
  PiafsFrm_t * internal =	&this->fr2;

	mem_set (this, 0x00, sizeof(*this));

	this->C = C;
	this->P = P;
	this->name = piafs_cfg_speed ? "32K" : "64K";

  DBG_TRC(("[%p:%s:%s] PIAFS - Configure", this->C, port_name(this->P), this->name))

  Crc32MsbFunc(&(internal->RX_CRC));
        
  /* Rx */
  internal->DQRxBuffer = NULL;
  internal->IdxInRxFrame = 0;
  internal->RxState = HUNT_STATE;
  internal->JustSynchronized = FALSE;

  internal->CurrentSynchroCandidate = 0;
  internal->CurrentRxShiftRegister = 0;   
  internal->RateAdaptationEnable = piafs_cfg_speed;
  internal->IsThereABytePending = FALSE;
  internal->BytePending = 0;
  internal->PreShiftCount = 0;
 
  /* Change the starting state if the rate adaptation function is ON. */
  if( internal->RateAdaptationEnable == TRUE )
  {
    internal->RxState = PRE_HUNT_STATE;
  }
}

static void Crc32MsbFunc(dword *Crc32Msb)   /* initialise FrmFunc */
{
  Crc32MsbConfigure(0xFFFFFFFF, Crc32Msb);
}


