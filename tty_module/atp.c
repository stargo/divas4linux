
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

#if defined(UNIX)
# include "platform.h"
# include <linux/types.h>
# include <asm/string.h>
# include "port.h"
# include "fax.h"
#else
# include "typedefs.h"
# include "port.h"
#endif
//# include "serial.h"

# include "atp_if.h"
# include "rna_if.h"
# include "btx_if.h"
# include "fax_if.h"
# include "mdm_msg.h"
# include "debuglib.h"

/*
	PIAFS SPECIFIC DEFINITIONS
	*/
#define MAX_PIAFS_FRAME			(2048)
#define MAX_PIAFS_FRAME_LO	(MAX_PIAFS_FRAME%256)
#define MAX_PIAFS_FRAME_HI	(MAX_PIAFS_FRAME/256)

byte piafs_32K_BC[] = { 0x04, 0x88, 0x90, 0x21, 0x8c };
byte piafs_64K_BC[] = { 0x02, 0x88, 0x90 };
byte piafs_64K_FIX_OrigSa[32]={0x07,0xa0,0x00,0x81,0x01,0x02,0x31,0x30};
byte piafs_64K_VAR_OrigSa[32]={0x07,0xa0,0x00,0x81,0x01,0x02,0x34,0x30};
byte piafs_32K_DLC[] = { 0x12,
	MAX_PIAFS_FRAME_LO, MAX_PIAFS_FRAME_HI, 0x00, 0x00, 0x00, 0x00,
	0x0a, 0x00, /* XID length */
	0x00, /* protocol */
	0x03, 0x00, 0x00, 0x00, 0x00, 0x00, /* V.42bis */
	0x02, /* extension length */
	0x80, 0x01 /* udata extension */
};

byte piafs_32K_DLC_china [] = { 0x12,
	MAX_PIAFS_FRAME_LO, MAX_PIAFS_FRAME_HI, 0x00, 0x00, 0x00, 0x00,
	0x0a, 0x00, /* XID length */
	0x04, /* protocol */
	0x03, 0x00, 0x00, 0x00, 0x00, 0x00, /* V.42bis */
	0x02, /* extension length */
	0x80, 0x01 /* udata extension */
};

byte piafs_64K_FIX_DLC[] = { 0x12,
	MAX_PIAFS_FRAME_LO, MAX_PIAFS_FRAME_HI, 0x00, 0x00, 0x00, 0x00,
	0x0a, 0x00, /* XID length */
  0x01, /* protocol */
	0x03, 0x00, 0x00, 0x00, 0x00, 0x00, /* V.42bis */
	0x02, /* extension length */
	0x80, 0x01 /* udata extension */
};

byte piafs_64K_FIX_DLC_china[] = { 0x12,
	MAX_PIAFS_FRAME_LO, MAX_PIAFS_FRAME_HI, 0x00, 0x00, 0x00, 0x00,
	0x0a, 0x00, /* XID length */
  0x05, /* protocol */
	0x03, 0x00, 0x00, 0x00, 0x00, 0x00, /* V.42bis */
	0x02, /* extension length */
	0x80, 0x01 /* udata extension */
};

byte piafs_64K_VAR_DLC[] = { 0x12,
	MAX_PIAFS_FRAME_LO, MAX_PIAFS_FRAME_HI, 0x00, 0x00, 0x00, 0x00,
	0x0a, 0x00, /* XID length */
	0x03, /* protocol */
	0x03, 0x00, 0x00, 0x00, 0x00, 0x00, /* V.42bis  */
	0x02, /* extension length */
	0x80, 0x01 /* udata extension */
};

byte piafs_64K_VAR_DLC_china[] = { 0x12,
	MAX_PIAFS_FRAME_LO, MAX_PIAFS_FRAME_HI, 0x00, 0x00, 0x00, 0x00,
	0x0a, 0x00, /* XID length */
	0x07, /* protocol */
	0x03, 0x00, 0x00, 0x00, 0x00, 0x00, /* V.42bis  */
	0x02, /* extension length */
	0x80, 0x01 /* udata extension */
};


#if defined(LINUX)
#include <linux/kernel.h>
#endif   
/* static byte *atIsdnProtName (byte Prot); */

extern ISDN_PORT_DRIVER PortDriver;
extern int divas_tty_wait_sig_disc;
extern int isdn_get_num_adapters (void);
extern int isdn_get_adapter_channel_count (int adapter_nr);
extern void faxInit (ISDN_PORT *P);
T30_INFO *faxConnect (ISDN_PORT *P);
extern int faxHangup (ISDN_PORT *P);
extern int faxDelay (ISDN_PORT *P,
										 byte *Response, word sizeResponse, dword *RspDelay);
int faxPlusF (ISDN_PORT *P, byte **Arg);
int faxUp (ISDN_PORT *P);
int rnaUp (ISDN_PORT *P);
int btxUp (ISDN_PORT *P);
extern unsigned long sysGetSec(void);
extern int global_options;
unsigned long global_mode = GLOBAL_MODE_NONE;
static char global_mode_name [24] = "none     ";
static void atRing (ISDN_PORT *P);
static int atPrintCID (ISDN_PORT* P, char* Str, int in_ring, int type, const char* rsp, int limit);

extern void* diva_mem_malloc (dword length);
extern void diva_mem_free    (void* ptr);

/* Unimodem response codes and associated modem response strings (not	*/
/* completely exact, unimodem knows only the CONNECT response code 2).	*/
static struct	{
	byte *String[2];
} Responses [] = {
	/* Symbol (Unimodem ID)		AT-shorthand	AT-verbose		*/
	/* R_OK			   0 */		{{"0"	,			(byte *)"OK"}},
	/* R_CONNECT_300   2 */		{{"1"	,			(byte *)"CONNECT"}}		,
	/* R_RING          8 */		{{"2"	,			(byte *)"RING"}}			,
	/* R_NO_CARRIER    4 */		{{"3"	,			(byte *)"NO CARRIER"}}	,
	/* R_ERROR		   3 */		{{"4"	,			(byte *)"ERROR"}}			,
	/* R_CONNECT_1200  2 */		{{"5"	,			(byte *)"CONNECT"}}		,
	/* R_NO_DIALTONE   5 */		{{"6"	,			(byte *)"NO DIALTONE"}}	,
	/* R_BUSY          6 */		{{"7"	,			(byte *)"BUSY"}}			,
	/* R_NO_ANSWER	   7 */		{{"8"	,			(byte *)"NO ANSWER"}}		,
	/* R_CONNECT	   2 */		{{"9"	,			(byte *)"CONNECT"}}		,

	/* for speed dependent connect responses see below		*/
};

/*
 * Connect speed to response code mapping,
 * starting with response code 9 for 14400 baud.
 * The table is created by feeding the response.awk
 * script with speeds.txt and pasting the resulting
 * response.tab file in here.
 * Don't wonder if a speed is listed more than once.
 * This saves us error prone editing of tables and
 * is not harmful anyway.
 */

static dword v110SpeedMap[] = {
	300,  // 0
	600,  // 1
	1200, // 2
	2400, // 3
	4800, // 4
	9600, // 5
	19200,// 6
	38400,// 7
	48000,// 8
	56000,// 9
	7200, // 10
	14400,// 11
	28800,// 12
	12000,// 13
	75,   // 14, 1200_75
	75,   // 15, 75_1200
};

static word SpeedMap[] =
{
        /*  9 */    14400,
        /* 10 */     1200,
        /* 11 */     2400,
        /* 12 */     4800,
        /* 13 */     9600,
        /* 14 */    19200,
        /* 15 */    38400,
        /* 16 */    48000,
        /* 17 */    56000,
        /* 18 */    64000,
        /* 19 */    11111,
        /* 20 */       75,
        /* 21 */      110,
        /* 22 */      150,
        /* 23 */      300,
        /* 24 */      600,
        /* 25 */     1200,
        /* 26 */     2400,
        /* 27 */     4800,
        /* 28 */     7200,
        /* 29 */     9600,
        /* 30 */    12000,
        /* 31 */    14400,
        /* 32 */    16800,
        /* 33 */    19200,
        /* 34 */    21600,
        /* 35 */    24000,
        /* 36 */    26400,
        /* 37 */    28800,
        /* 38 */    31200,
        /* 39 */    33600,
        /* 40 */    36000,
        /* 41 */    38400,
        /* 42 */    40800,
        /* 43 */    43200,
        /* 44 */    45600,
        /* 45 */    48000,
        /* 46 */    50400,
        /* 47 */    52800,
        /* 48 */    55200,
        /* 49 */    56000,
        /* 50 */    57600,
        /* 51 */    60000,
        /* 52 */    62400,
        /* 53 */    64000,
        /* 54 */    28000,
        /* 55 */    29333,
        /* 56 */    30666,
        /* 57 */    32000,
        /* 58 */    33333,
        /* 59 */    34666,
        /* 60 */    37333,
        /* 61 */    38666,
        /* 62 */    40000,
        /* 63 */    41333,
        /* 64 */    42666,
        /* 65 */    44000,
        /* 66 */    45333,
        /* 67 */    46666,
        /* 68 */    49333,
        /* 69 */    50666,
        /* 70 */    52000,
        /* 71 */    53333,
        /* 72 */    54666,
} ;

/* Predefined profiles as used whith &Fn (n == number of profile).  	*/
/* To hold the documentation simple the profile number is mostly the	*/
/* same number as the number of the ISDN protocol used whith this	*/
/* profile, but the code does not depend on this fact.			*/
static AT_CONFIG Profiles [] = {

/* Id   Mode			Prot			  Svc Svc+  PlanD PlanO	Baud    Data
	E   Q   V   X   &D  &K  &Q  \V  _1  _2  _3  +iD   RnaFraming    BitTDetect, S0   */

{  0,   P_MODE_HOOK,	ISDN_PROT_EXT_0,  1,  2,    255,  255,	255,    1,
	1,  0,  1,  4,  2,  3,  0,  0,  0,  0,  0,  0,    P_FRAME_NONE, BIT_T_DETECT_OFF,
	/* S-Registers */
	{0,0,'+'},
	/* Modem Options */
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC,  /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
},
{  1,   P_MODE_MODEM,	ISDN_PROT_X75,    7,  0,    255,  255,	255,    1,
	1,  0,  1,  4,  2,  3,  0,  0,  0,  0,  0,  0,    P_FRAME_NONE, BIT_T_DETECT_OFF,
	/* S-Registers */
	{255,0,'+'},
	/* Modem Options */
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC, /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
},
{  2,   P_MODE_MODEM,	ISDN_PROT_V110_s, 7,  0,    255,  255,	255,    1,
	1,  0,  1,  4,  2,  3,  0,  0,  0,  0,  0,  0,    P_FRAME_NONE, BIT_T_DETECT_OFF,
	/* S-Registers */
	{255,0,'+'},
	/* Modem Options */
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC,  /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
},
{  3,   P_MODE_MODEM,	ISDN_PROT_V110_a, 7,  0,    255,  255,	7,      1,
	1,  0,  1,  4,  2,  3,  0,  0,  0,  0,  0,  0,    P_FRAME_NONE, BIT_T_DETECT_OFF,
	/* S-Registers */
	{255,0,'+'},
	/* Modem Options */
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC,  /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
},
{  4,   P_MODE_MODEM,	ISDN_PROT_MDM_s,  1,  2,    255,  255,	255,    1,
	1,  0,  1,  4,  2,  3,  0,  0,  0,  0,  0,  0,    P_FRAME_NONE, BIT_T_DETECT_OFF,
	/* S-Registers */
	{255,0,'+'},
	/* Modem Options */
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC,  /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
},
{  5,   P_MODE_MODEM,	ISDN_PROT_MDM_a,  1,  2,    255,  255,	255,    1,
	1,  0,  1,  4,  2,  3,  0,  0,  0,  0,  0,  0,    P_FRAME_NONE, BIT_T_DETECT_OFF,
	/* S-Registers */
	{255,0,'+'},
	/* Modem Options */
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC,  /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
},
{  6,   P_MODE_MODEM,	ISDN_PROT_V120,   7,  0,    255,  255,	255,    1,
	1,  0,  1,  4,  2,  3,  0,  0,  0,  0,  0,  0,    P_FRAME_NONE, BIT_T_DETECT_OFF,
	/* S-Registers */
	{255,0,'+'},
	/* Modem Options */
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC,  /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
},
{  7,   P_MODE_MODEM,	ISDN_PROT_V120,   7,  170,  255,  255,	9,      1,
	1,  0,  1,  4,  2,  3,  0,  0,  0,  0,  0,  0,    P_FRAME_NONE, BIT_T_DETECT_OFF,
	/* S-Registers */
	{255,0,'+'} ,
	/* Modem Options */
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC,  /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
},
{  8,   P_MODE_VOICE,	ISDN_PROT_VOICE,  1,  0,    255,  255,	255,    0,
	1,  0,  1,  4,  2,  3,  0,  0,  0,  0,  0,  0,    P_FRAME_NONE, BIT_T_DETECT_OFF,
	/* S-Registers */
	{255,0,'+'},
	/* Modem Options */
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC,  /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
},
#if 0
{  9,   P_MODE_FRAME,	ISDN_PROT_RAW,    7,  0,    255,  255,	255,    1,
	1,  0,  1,  4,  2,  3,  0,  0,  0,  0,  0,  0,  P_FRAME_DET, BIT_T_DETECT_OFF,
	/* S-Registers */
	{255,0,'+'},
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC,  /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
},
#else
{  9,   P_MODE_RNA  ,	ISDN_PROT_RAW,    7,  0,    255,  255,	255,    1,
	0,  0,  0,  4,  2,  3,  0,  0,  0,  0,  0,  0,  P_FRAME_DET,  BIT_T_DETECT_OFF,
	/* S-Registers */
	{255,0,'+'},
	/* Modem Options */
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC,  /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
},
#endif
{  10,  P_MODE_RNA,		ISDN_PROT_RAW,    7,  170,  255,  255,  9,      1,
	0,  0,  0,  4,  2,  3,  0,  0,  0,  0,  0,  0,    P_FRAME_DET,  BIT_T_DETECT_OFF,
	/* S-Registers */
	{255,0,'+'},
	/* Modem Options */
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC,  /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
},
{  11,  P_MODE_BTX,		ISDN_PROT_BTX,    7,  0,    255,  255,	255,    1,
	1,  0,  1,  4,  2,  3,  0,  0,  0,  0,  0,  0,    P_FRAME_NONE, BIT_T_DETECT_OFF,
	/* S-Registers */
	{255,0,'+'},
	/* Modem Options */
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC,  /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
},
{  12,  P_MODE_FRAME,	ISDN_PROT_BTX,    7,  0,    255,  255,	255,    1,
	1,  0,  1,  4,  2,  3,  0,  0,  0,  0,  0,  0,    P_FRAME_NONE, BIT_T_DETECT_OFF,
	/* S-Registers */
	{255,0,'+'},
	/* Modem Options */
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC,  /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
},
{  13,  P_MODE_FAX,  	ISDN_PROT_FAX,    1,  2,    255,  255,	255,    0,
#if defined(WINNT) || defined(UNIX)	/* use default response delay 25 */
	1,  0,  1,  4,  2,  6,  0,  0,  0,  0,  0,  25,   P_FRAME_NONE, BIT_T_DETECT_OFF,
	/* S-Registers */
	{255,0,'+'},
	/* Modem Options */
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC,  /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
},
#endif /* WINNT */
{  14,  P_MODE_MODEM,  	ISDN_PROT_DET,    255,255,  255,  255,	255,    1,
	1,  0,  1,  4,  2,  3,  0,  0,  0,  0,  0,  0,    P_FRAME_NONE, BIT_T_DETECT_OFF,
	/* S-Registers */
	{255,0,'+'},
	/* Modem Options */
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC,  /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
},
{  15,   P_MODE_MODEM,	ISDN_PROT_X75V42,    7,  0,    255,  255,	255,    1,
	1,  0,  1,  4,  2,  3,  0,  0,  0,  0,  0,  0,    P_FRAME_NONE, BIT_T_DETECT_OFF,
	/* S-Registers */
	{255,0,'+'},
	/* Modem Options */
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC,  /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
},
{  16,   P_MODE_MODEM,	ISDN_PROT_PIAFS_32K,   7,  0,    255,  255,	255,    1,
	1,  0,  1,  4,  2,  3,  0,  0,  0,  0,  0,  0,    P_FRAME_NONE, BIT_T_DETECT_OFF,
	/* S-Registers */
	{255,0,'+'},
	/* Modem Options */
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC,  /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
},
{  17,   P_MODE_MODEM,	ISDN_PROT_PIAFS_64K_FIX,7,  0,    255,  255,	255,    1,
	1,  0,  1,  4,  2,  3,  0,  0,  0,  0,  0,  0,    P_FRAME_NONE, BIT_T_DETECT_OFF,
	/* S-Registers */
	{255,0,'+'},
	/* Modem Options */
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC,  /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
},
{  18,   P_MODE_MODEM,	ISDN_PROT_PIAFS_64K_VAR,7,  0,    255,  255,	255,    1,
	1,  0,  1,  4,  2,  3,  0,  0,  0,  0,  0,  0,    P_FRAME_NONE, BIT_T_DETECT_OFF,
	/* S-Registers */
	{255,0,'+'},
	/* Modem Options */
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC,  /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
},
{  19,  P_MODE_MODEM,  	ISDN_PROT_DET,    255,255,  255,  255,	255,    1,
	1,  0,  1,  4,  2,  3,  0,  0,  0,  0,  0,  0,    P_FRAME_NONE, BIT_T_DETECT_ON,
	/* S-Registers */
	{255,0,'+'},
	/* Modem Options */
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC,  /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
},
{  20,  P_MODE_MODEM,  	ISDN_PROT_DET,    255,255,  255,  255,	255,    1,
	1,  0,  1,  4,  2,  3,  0,  0,  0,  0,  0,  0,    P_FRAME_NONE, BIT_T_DETECT_ON_S,
	/* S-Registers */
	{255,0,'+'},
	/* Modem Options */
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC,  /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
},
{  21,  P_MODE_MODEM,  	ISDN_PROT_DET,    255,255,  255,  255,	255,    1,
	1,  0,  1,  4,  2,  3,  0,  0,  0,  0,  0,  0,    P_FRAME_NONE, BIT_T_DETECT_ON_D,
	/* S-Registers */
	{255,0,'+'},
	/* Modem Options */
	{0,  /* valid AT+MS   */
	 DLC_MODEMPROT_DISABLE_SDLC,  /* error control */
	 0,  /* valid framing */
	 0,  /* guard_tone */
	 0,  /* line taking */
	 0,  /* protocol options */
	 0,  /* s7  - wait_carrier_sec */
	 0,  /* s10 - wait_carrier_lost_tenthsec */
	 0,  /* retrain options */
	 0,  /* bell mode */
	 0,  /* fast connect mode */
	 (SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT | SDLC_L2_OPTION_SINGLE_DATA_PACKETS), /* sdlc protocol options */
	 0x30,  /* sdlc address A */
	},
	0, /* S27 */
	0, /* S51 */
	0, /* Presentation_and_Screening */
	0, /* S253 */
	0, /* S254 */
}
};


# define MAX_PROFILE		(sizeof (Profiles) / sizeof (Profiles[0]) - 1)
# define FRAME_MODE_PROFILE	12
# define INBOUND_ONLY_PROFILE	14
# define GENERIC_SERVER_PROFILE	14

static int string2bin (byte* dst, const byte* src, int dst_limit, int src_limit);

static int compare_struct (const byte* ref, const byte* src);

static byte *str_tag_find (byte *Str, byte *endStr, byte Tag)
{
	while ( Str < endStr )
	{
		if ( Str[0] == Tag ) return ( Str + 1 ) ;

		Str = str_chr (Str + 1, '\0') + 1 ;
	}
	return ( 0 ) ;
}

#if 0
static byte *atPortModeName (byte Mode)
{
	static byte Modes[] =
	/* P_MODE_MODEM			*/	"\001MODEM\0"
	/* P_MODE_FAX			*/	"\002FAX\0"
	/* P_MODE_VOICE			*/	"\003VOICE\0"
	/* P_MODE_RNA			*/	"\004RNA\0"
	/* P_MODE_BTX			*/	"\005BTX\0"
	/* P_MODE_FRAME			*/	"\006FRAME\0"
	/* P_MODE_HOOK			*/	"\007SERIAL\0"
	;
	byte *Name = str_tag_find (&Modes[0], &Modes[sizeof(Modes) - 2], Mode) ;
	return ((byte*)( Name? Name : (byte*)"<mode-undef>" )) ;
}
#endif

#if 0
static byte *atIsdnProtName (byte Prot)
{
	static byte Prots[] =
	/* ISDN_PROT_DET		*/	"\000DETECT\0"
	/* ISDN_PROT_X75		*/	"\001X75\0"
	/* ISDN_PROT_V110_s		*/	"\002V110S\0"
	/* ISDN_PROT_V110_a		*/	"\003V110\0"
	/* ISDN_PROT_MDM_s		*/	"\004MODEMS\0"
	/* ISDN_PROT_MDM_a		*/	"\005MODEM\0"
	/* ISDN_PROT_V120		*/	"\006V120\0"
	/* ISDN_PROT_FAX		*/	"\007FAX\0"
	/* ISDN_PROT_VOICE		*/	"\010VOICE\0"
	/* ISDN_PROT_RAW		*/	"\011HDLC\0"
	/* ISDN_PROT_BTX		*/	"\012BTX\0"
	/* ISDN_PROT_EXT_0		*/	"\013EXT0\0"
	/* ISDN_PROT_X75V42		*/	"\014X75B\0"
	;
	byte *Name = str_tag_find (&Prots[0], &Prots[sizeof(Prots) - 2], Prot) ;
	return ( Name? Name : (byte*)"<prot-undef>" ) ;
}
#endif

static byte *atModemConNormName (byte Norm)
{
	static byte Norms[] =
	/* DSP_CONNECTED_NORM_UNSPECIFIED		*/	/*	0	*/
	/* DSP_CONNECTED_NORM_V21				*/	"\x01""V21\0"
	/* DSP_CONNECTED_NORM_V23				*/	"\x02""V23\0"
	/* DSP_CONNECTED_NORM_V22				*/	"\x03""V22\0"
	/* DSP_CONNECTED_NORM_V22_BIS			*/	"\x04""V22BIS\0"
	/* DSP_CONNECTED_NORM_V32_BIS			*/	"\x05""V32BIS\0"
	/* DSP_CONNECTED_NORM_V34				*/	"\x06""V34\0"
	/* DSP_CONNECTED_NORM_V8				*/	/*	7	*/
	/* DSP_CONNECTED_NORM_BELL_212A			*/	"\x08""BELL212A\0"
	/* DSP_CONNECTED_NORM_BELL_103			*/	"\x09""BELL103\0"
	/* DSP_CONNECTED_NORM_V29_LEASED_LINE	*/	"\x0a""V29LL\0"
	/* DSP_CONNECTED_NORM_V33_LEASED_LINE	*/	"\x0b""V33LL\0"
	/* DSP_CONNECTED_NORM_V90				*/	"\x0c""V90\0"
	/* DSP_CONNECTED_NORM_V21_CH2			*/	/*	13	*/
	/* DSP_CONNECTED_NORM_V27_TER			*/	/*	14	*/
	/* DSP_CONNECTED_NORM_V29				*/	/*	15	*/
	/* DSP_CONNECTED_NORM_V33				*/	/*	16	*/
	/* DSP_CONNECTED_NORM_V17				*/	/*	17	*/
	/* DSP_CONNECTED_NORM_V32				*/	"\x12""V32\0"
	/* DSP_CONNECTED_NORM_K56_FLEX			*/	"\x13""K56FLEX\0"
	/* DSP_CONNECTED_NORM_X2				*/	"\x14""X2\0"
	/* DSP_CONNECTED_NORM_VOWN			*/	"\x26""V23S\0"
	/* DSP_CONNECTED_NORM_V23_OFF_HOOK		*/	"\x27""V23HDX\0"
	/* DSP_CONNECTED_NORM_V23_ON_HOOK		*/	"\x28""V23HDXON\0"
	/* DSP_CONNECTED_NORM_V23_AUTO_FRAMER	*/	"\x29""V23S\0"
	/* DSP_CONNECTED_NORM_BELL202_CID		*/	"\x2a""B202CID\0"
	/* DSP_CONNECTED_NORM_TELENOT			*/	"\x2b""TELENOT\0"
	/* DSP_CONNECTED_NORM_BELL202_POS		*/	"\x2c""B202POS\0"
	/* DSP_CONNECTED_NORM_BELL103_SIA		*/	"\x2d""B103SIA\0"
	/* DSP_CONNECTED_NORM_V23_REVERSE		*/	"\x2e""V23R\0"
	/* DSP_CONNECTED_NORM_VOWN_RESERVED_16	*/	"\x36""USER16\0"
	/* DSP_CONNECTED_NORM_VOWN_RESERVED_17	*/	"\x37""USER17\0"
	/* DSP_CONNECTED_NORM_VOWN_RESERVED_18	*/	"\x38""USER18\0"
	/* DSP_CONNECTED_NORM_VOWN_RESERVED_19	*/	"\x39""USER19\0"
	/* DSP_CONNECTED_NORM_VOWN_RESERVED_20	*/	"\x3a""USER20\0"
	/* DSP_CONNECTED_NORM_VOWN_RESERVED_21	*/	"\x3b""USER21\0"
	/* DSP_CONNECTED_NORM_VOWN_RESERVED_22	*/	"\x3c""USER22\0"
	/* DSP_CONNECTED_NORM_VOWN_RESERVED_23	*/	"\x3d""USER23\0"
	/* DSP_CONNECTED_NORM_V22_FC			*/	"\x48""V22FC\0"
	/* DSP_CONNECTED_NORM_V22BIS_FC			*/	"\x49""V22BISFC\0"
	/* DSP_CONNECTED_NORM_V29_FC			*/	"\x4a""V29FC\0"
	;
	byte *Name = str_tag_find (&Norms[0], &Norms[sizeof(Norms) - 2], Norm) ;
	return ( Name? Name : (byte*)"<norm-undef>" ) ;
}

static byte *atModemConOptsName (word ConOpts)
{
	if ( ConOpts & DSP_CONNECTED_OPTION_V42BIS )
		return("LAPM/V42BIS") ;
	if ( ConOpts & DSP_CONNECTED_OPTION_MASK_V42)
		return("LAPM") ;
	if ( ConOpts & DSP_CONNECTED_OPTION_MNP5)
		return("MNP5") ;
	if ( ConOpts & DSP_CONNECTED_OPTION_MASK_MNP)
		return("MNP") ;
	if ( ConOpts & DSP_CONNECTED_OPTION_MASK_SDLC )
		return("SDLC") ;

	return ("NONE") ;
}

static byte* atModemConProtocolName (word conOpts) {
	if (conOpts & DSP_CONNECTED_OPTION_V42_TRANS)
		return("NONE");
	if (conOpts & DSP_CONNECTED_OPTION_V42_LAPM)
		return("V42/LAPM");
	if (conOpts & DSP_CONNECTED_OPTION_MNP2)
		return("MNP2") ;
	if (conOpts & DSP_CONNECTED_OPTION_MNP3)
		return("MNP3") ;
	if (conOpts & DSP_CONNECTED_OPTION_MNP4)
		return("MNP4") ;
	if (conOpts & DSP_CONNECTED_OPTION_MNP10)
		return("MNP10") ;
	if (conOpts & DSP_CONNECTED_OPTION_MASK_SDLC)
		return("SDLC") ;

	return ("NONE");
}

static byte* atModemConCompressionName (word conOpts) {
	if (conOpts & DSP_CONNECTED_OPTION_V42BIS)
		return("V42BIS");
	if (conOpts & DSP_CONNECTED_OPTION_MNP5)
		return("MNP5") ;

	return ("NONE");
}

static byte* atPiafsConNormName (byte Norm) {
	static byte Norms[] = 
	/* No Protocol									*/	/* 0 */
	/* Data Application Protocol		*/	"\001DAP\0"
	/* Real time protocol						*/	"\002RTP\0"
	/* Variable rate type 1					*/	"\003VR1\0"
	/* Variable rate type 2					*/	"\004VR2\0"
	/* Future Protocol							*/	"\005FTR\0"
	;

	byte *Name = (byte*)"<undef>";
	if (!Norm) {
		Name = (byte*)"STD";
	} else {
		Name = str_tag_find (&Norms[0], &Norms[sizeof(Norms) - 2], Norm) ;
	}
	return (Name);
}


void atProfile (AT_DATA *At, int Profile)
{
	

	At->f = Profiles[Profile];
	if (PortDriver.RspDelay > At->f.RspDelay)
		At->f.RspDelay = PortDriver.RspDelay;

	switch (At->f.Prot) {
		case ISDN_PROT_PIAFS_32K:
			mem_cpy (At->f.user_bc, piafs_32K_BC, sizeof(piafs_32K_BC));
			At->f.Baud = 7;
			break;

		case ISDN_PROT_PIAFS_64K_FIX:
			mem_cpy (At->f.user_bc, piafs_64K_BC, sizeof(piafs_64K_BC));
			mem_cpy (&At->f.user_orig_subadr[0],
							&piafs_64K_FIX_OrigSa[0],
							piafs_64K_FIX_OrigSa[0]+1);
			break;

		case ISDN_PROT_PIAFS_64K_VAR:
			mem_cpy (At->f.user_bc, piafs_64K_BC, sizeof(piafs_64K_BC));
			mem_cpy (&At->f.user_orig_subadr[0],
							&piafs_64K_VAR_OrigSa[0],
							piafs_64K_VAR_OrigSa[0]+1);
			break;
	}
}

void atSetup (ISDN_PORT *P, int Profile)
{
/*
 * Initialize AT processor specific data according to 'Profile'.
 */
	mem_zero (&P->At, sizeof (P->At));
	mem_zero (&P->CallbackParms, sizeof (P->CallbackParms));
	P->At.Orig_1[0]  = 0;
	P->At.PlanOrig_1 = 0;
	P->At.PresentationAndScreening_1 = 0;
	atProfile (&P->At, Profile);

	P->State	= P_COMMAND;
	P->StateTimeout	= P_TO_NEVER;
	P->Direction	= P_CONN_NONE;
	P->Mode		= P->At.f.Mode;
	P->Class	= P_CLASS_DATA;
	P->Wait		= 0;
	P->Block	= 0;
	P->Defer	= 0;
	P->Split	= 0;
	P->Channel	= 0;
	P->FlowOpts     = P->At.f.FlowOpts;

	queueReset (&P->RspQueue);

	faxInit (P);
}

void atNewMode (ISDN_PORT *P, AT_DATA *At)
{
	switch (P->State) {
	case P_COMMAND	:
	case P_DIALING	:
	case P_OFFERED	:
	case P_ANSWERED	:
		P->Mode = At->f.Mode;
		break;
	case P_CONNECTED:
	case P_DATA	:
	case P_ESCAPE	:
	case P_IDLE	:
	default		:
		break;
	}
}

static void atUnEscape (ISDN_PORT *P)
{
	P->At.stateLine = AT_INIT ;
	P->EscCount     = 0;
	sysCancelTimer (&P->EscTimer);
}

void atRingOff (ISDN_PORT *P)
{
    	*P->pMsrShadow &= ~(MS_RING_ON | (MS_RING_ON >> 4));
	*P->pDetectedEvents &= ~EV_RingTe;
	P->ModemStateEvents &= ~EV_RingTe;
	sysCancelTimer (&P->RingTimer);
}

static void atRspSend (ISDN_PORT *P, byte *Response, word sizeResponse)
{
	if (sizeResponse >= (sizeof(P->RspBuffer)-2)) {
		return;
	}
	portRxEnqueue (P, Response, sizeResponse, 0);
}

int atRspPop (ISDN_PORT *P)
{
	byte *Response; word sizeResponse;

	if ((Response = queuePeekMsg (&P->RspQueue, &sizeResponse))) {
		if (sizeResponse < (sizeof(P->RspBuffer)-2)) {
			atRspSend (P, Response, sizeResponse);
		} else {
			DBG_TRC(("A: TOO LONG RESPONSE DISCARDED\n"))
		}
		queueFreeMsg (&P->RspQueue);
		return (1);
	} else {
		return (0);
	}
}

void atRspFlush (ISDN_PORT *P)
{
	int i = 0;
	sysCancelTimer (&P->RspTimer);
	while (atRspPop (P)) {
		if (i++ > 16) {
			DBG_TRC(("A: RSP TIMEOUT"))
			queueReset (&P->RspQueue);
			break;
		}
	}
}

void atRspPurge (ISDN_PORT *P)
{
	sysCancelTimer (&P->RspTimer);
	queueReset (&P->RspQueue);
}

static void atUnTimeout (ISDN_PORT *P)
{
	atRspFlush (P);

	atUnEscape (P);

	atRingOff (P);

	portUnTimeout (P);
}

static void _cdecl atRspTimer (ISDN_PORT *P)
{
	byte *Response; word sizeResponse;

	if ((Response = queuePeekMsg (&P->RspQueue, &sizeResponse))) {
		if (!P->PortData.QInCount) {
			atRspSend (P, Response, sizeResponse);
			queueFreeMsg (&P->RspQueue);
		}
		if (queueCount (&P->RspQueue)) {
			sysStartTimer (&P->RspTimer,
				       (dword) (P->At.f.RspDelay ?
						P->At.f.RspDelay : 10) );
		}
	}
}

static void atRspQueue (ISDN_PORT *P, byte *Response, word sizeResponse)
{
	dword RspDelay; byte *Pending; word sizePending;

	RspDelay = P->At.f.RspDelay;

	if (P->Mode == P_MODE_FAX &&
	    faxDelay (P, Response, sizeResponse, &RspDelay)) {
		/* unconditional delay this response */
		atRspFlush (P);
		(void) queueWriteMsg (&P->RspQueue, Response, sizeResponse);
		sysStartTimer (&P->RspTimer, RspDelay);
		return;
	}

	if (!RspDelay) {
		if (queueCount (&P->RspQueue)) atRspFlush (P);
		atRspSend (P, Response, sizeResponse);
		return;
	}

	if (!queueWriteMsg (&P->RspQueue, Response, sizeResponse)) {
		atRspFlush (P);
		(void) queueWriteMsg (&P->RspQueue, Response, sizeResponse);
	}

	if (sysTimerPending(&P->RspTimer)) {
		return;	/* post when timer fires */
	}

	if (!P->PortData.QInCount) {
		Pending = queuePeekMsg (&P->RspQueue, &sizePending);
		atRspSend (P, Pending, sizePending);
		queueFreeMsg (&P->RspQueue);
	}

	if (queueCount (&P->RspQueue)) {
		sysStartTimer (&P->RspTimer, RspDelay);
	}
}

void atRspStr (ISDN_PORT *P, byte *str)
{
	atRspQueue (P, str, (word) str_len ((char*)str));
}

static int is_printable (char c) {
	return (((c >= 'a') && (c <= 'z')) ||
          ((c >= 'A') && (c <= 'Z')) ||
          ((c >= '0') && (c <= '9')));
}

static int write_ascii_address (char* dst, int limit, const char* src, int length) {
	if (limit > 1) {
		int written = 0;

		while ((limit > 1) && length--) {
			if (is_printable (*src)) {
				dst[written++] = *src++;
				limit--;
			} else {
				int i = diva_snprintf (&dst[written], limit, "\\x%02x", (byte)*src++);
				if (i == 0) {
					break;
				}
				written += i;
				limit   -= i;
			}
		}

		dst[written] = 0;

		return (written);
	} else {
		return (0);
	}
}

void atRsp (ISDN_PORT *P, int Rc)
{
	int orgRc = Rc;
/*      X0            X1            X2             X3            X4	     */
static struct {
	byte Rc[6];
} XResponses [] = {
{{R_NO_DIALTONE, \
		R_NO_CARRIER, R_NO_CARRIER, R_NO_DIALTONE, R_NO_CARRIER, R_NO_DIALTONE}},
{{R_BUSY, \
        R_NO_CARRIER, R_NO_CARRIER, R_NO_CARRIER,  R_BUSY,       R_BUSY}},
{{R_NO_ANSWER, \
        R_NO_CARRIER, R_NO_CARRIER, R_NO_CARRIER,  R_NO_CARRIER, R_NO_ANSWER}}
};
	int i ; byte Str[256] ;

	if ( (Rc < 0) || (P->control_operation_mode)
	  || (Rc >= sizeof(Responses)/sizeof(Responses[0]))
	  || (P->At.f.Quiet != 0) )
		return ;

	/* map specific responses according to call progress options */
for ( i = 0 ; i < sizeof(XResponses)/sizeof(XResponses[0]) ; i++ )
	{
		if ( XResponses[i].Rc[0] == Rc )
		{
			Rc = XResponses[i].Rc[P->At.f.Progress + 1] ;
			break ;
		}
	}


	if (orgRc == R_RING && P->At.f.Verbose && P->At.f.cid_setting) {
    i = atPrintCID (P, Str, 1, P->At.f.cid_setting, Responses[Rc].String[1], sizeof(Str));
	} else {
		i = sprintf (Str, P->At.f.Verbose ? "\r\n%s\r\n" : "%s\r",
					  Responses[Rc].String[P->At.f.Verbose]  ) ;
	}

	DBG_TRC(("[%p:%s] atRsp(%d)", P->Channel, P->Name, i))
	DBG_BLK((Str, i))

	atRspQueue (P, Str, (word)i) ;

}

static void atRspConn (ISDN_PORT *P, ISDN_CONN_INFO *Info)
{
	byte	Terse[4], Verbose[24], BlownUp[128], *pRsp, *pTxt ;
	byte Str[256];
	int		Rc, i ;

	if ( P->At.f.Quiet ) return ;

	/* prepare a verbose and a terse response */

	if ( (Info && P->At.f.Verbose && P->At.f.Progress && P->At.f.ConFormat)
	  && (Info->Prot == ISDN_PROT_MDM_s	|| Info->Prot == ISDN_PROT_MDM_a) )
	{	/* respond with a full blown message */
		if (P->At.f.ConFormat == 2) {
			sprintf (BlownUp,
			/*     DCE speed      Carrier       ErrorCor          Compression */
			"CONNECT TX/RX %ld/%ld\r\nCARRIER %s\r\nPROTOCOL %s\r\nCOMPRESSION %s\n",
			Info->TxSpeed, Info->RxSpeed,
			atModemConNormName (Info->ConNorm),
			atModemConProtocolName (Info->ConOpts),
			atModemConCompressionName (Info->ConOpts));
		} else {
			sprintf (BlownUp,
							 "CONNECT %s/%s/%d:TX/%d:RX",
							 atModemConNormName (Info->ConNorm),
							 atModemConOptsName (Info->ConOpts),
							 (int)Info->TxSpeed, (int)Info->RxSpeed) ;
		}
		pRsp =
		pTxt = BlownUp ;
	}
	else if ((Info && P->At.f.Verbose && P->At.f.Progress && P->At.f.ConFormat) &&
					 (Info->Prot == ISDN_PROT_PIAFS_64K_VAR	||
					  Info->Prot == ISDN_PROT_PIAFS_64K_FIX	||
					  Info->Prot == ISDN_PROT_PIAFS_32K)) {
		sprintf (BlownUp, "CONNECT PIAFS/%s/%d",
						   atPiafsConNormName (Info->ConNorm),
						   (int)Info->TxSpeed);
		pRsp =
		pTxt = BlownUp ;
	}
	else if ( P->At.f.Progress == 0 )
	{	/* respond simply with a plain CONNECT */
		pRsp = Responses[R_CONNECT].String[P->At.f.Verbose] ;
		pTxt = Responses[R_CONNECT].String[1] ;
	}
	else
	{	/* respond with a CONNECT <speed> message */
		for ( Rc = 0 ;  ; Rc++ )
		{
			if ( Rc >= sizeof(SpeedMap)/sizeof(SpeedMap[0]) )
			{
				Rc = 10 ;
				P->At.Speed = 65536 ;
				break ;
			}
			if ( SpeedMap[Rc] == P->At.Speed )
			{
				Rc += R_CONNECT ;
				break ;
			}
		}
		(void) sprintf (Terse, "%d", Rc) ;
		(void) sprintf (Verbose, "CONNECT %d", (int)P->At.Speed) ;
		pRsp = (P->At.f.Verbose)? Verbose : Terse ;
		pTxt = Verbose ;
	}

	/* DBG_TRC(("[%p:%s] atRsp: '%s'\n", P->Channel, P->Name, pTxt)) */
	if (P->At.f.cid_setting  && P->At.f.Verbose && P->CadValid) { /* CID in CONNECT */
		i = atPrintCID (P, Str, 0, P->At.f.cid_setting, pRsp, sizeof(Str));
	} else {
		i = sprintf (Str, P->At.f.Verbose ? "\r\n%s\r\n" : "%s\r", pRsp) ;
	}

	DBG_TRC(("[%p:%s] atRspConn(%d)", P->Channel, P->Name, i))
	DBG_BLK((Str, i))

	atRspQueue (P, Str, (word)i) ;
}


void atClearCon (ISDN_PORT *P)
{
	atUnTimeout (P);

	P->Channel	= 0;
	P->State	= P_COMMAND;
	P->StateTimeout	= P_TO_NEVER;
	P->Direction	= P_CONN_NONE;

	if ( (P->At.f.Id == GENERIC_SERVER_PROFILE)
	  && (PortDriver.Fixes & P_FIX_SRV_SYNCPPP_ENABLED) )
	{
		P->Mode = P_MODE_MODEM ;
	}


	/* Clean up the frame queue and switch back the input stream	*/
	/* if we were in frame mode before.				*/

	if (P->PortData.QInAddr == &P->port_u.Frm.Stream[0]) {
		queueReset (&P->RxQueue);
		portInitQIn (P, P->RxBuffer.Buffer, P->RxBuffer.Size);
	}

	/* Clean up the transmit queue, can't transmit anymore.		*/

	portTxPurge (P);
}

void atShut (ISDN_PORT *P, byte Cause)
{
	if (P->Channel) {
		/* drop and detach channel, don't wait for idle from ISDN */
		(void) PortDriver.Isdn->Drop (P, P->Channel, Cause, 1,
																	(P->At.f.S253 & 0x80) ? (P->At.f.S253 & 0x7f) : 0);
	}
	atClearCon (P);

	/* Switch off RLSD in the modem status shadow and in event masks */

	*P->pMsrShadow &= ~(MS_RLSD_ON | (MS_RLSD_ON >> 4));
	*P->pDetectedEvents &= ~EV_RLSDS;
	P->ModemStateEvents &= ~EV_RLSDS;
}

static void atLinkDown (ISDN_PORT *P, int LostCarrier, int response_code)
{
	atClearCon (P);

	/* Tell our client that something happened!			*/

    	if (*P->pMsrShadow & MS_RLSD_ON) {
		/* Clear the RLSD bit and set the delta bit to	*/
		/* remember the line state change and then send	*/
		/* a notification about the state change.	*/
		/* After some while somebody should query the	*/
		/* state and then we will clear the delta bit	*/
		/* (see PortGetEventMask).			*/

		portSetMsr (P, MS_RLSD_ON, 0/*off*/);
		portEvNotify (P);

		if (LostCarrier) {
			/* remote disconnect, send NO CARRIER now	*/
			atRsp (P, response_code);
		}
	}
}

void atDrop (ISDN_PORT *P, byte Cause)
{
	if (P->Channel) {
		/* drop and detach channel, don't wait for idle from ISDN */
		(void) PortDriver.Isdn->Drop (P, P->Channel, Cause, 1,
																	(P->At.f.S253 & 0x80) ? (P->At.f.S253 & 0x7f) : 0);
	}
	atLinkDown (P, 0 /*!LostCarrier*/, R_NO_CARRIER);
}

void atDtrOff (ISDN_PORT *P)
{
	static byte DtrOffAction[ 4 /*&Q*/ ][ 4 /*&D*/ ] = {
		/*	&D0  &D1  &D2  &D3	*/
	   {/*&Q0*/	'N', 'B', 'C', 'D'},
	   {/*&Q1*/	'A', 'B', 'C', 'D'},
	   {/*&Q2*/	'C', 'C', 'C', 'D'},
	   {/*&Q3*/	'C', 'C', 'C', 'D'} 		   } ;

	switch (DtrOffAction[P->At.f.ComOpts][P->At.f.DtrOpts])
	{
	case 'N':	/* no action */
		break;

	case 'A':	/* if off hook hang up, send OK */
	case 'C':	/* if off hook hang up, send OK, disable autoanswer */
		switch (P->State) {
		case P_DIALING	:
		case P_ANSWERED	:
			/* DBG_TRC(("[%p:%s] SHUT: %s\n", P->Channel, P->Name, atState (P->State))) */
			atDrop (P, CAUSE_SHUT);
			atRsp (P, R_OK);
			break;
		case P_CONNECTED:
		case P_DATA	:
		case P_ESCAPE	:
			/* DBG_TRC(("[%p:%s] SHUT: %s\n", P->Channel, P->Name, atState (P->State))) */
			if (P->Mode == P_MODE_FAX) (void)faxHangup (P);
			atDrop (P, CAUSE_DROP);
			atRsp (P, R_OK);
			break;
		case P_OFFERED	:
		case P_DISCONN	:
		case P_COMMAND	:
		case P_IDLE	:
		default		:
			break;
		}
		break;

	case 'B':	/* switch from online to command state, send OK */
		if (P->State == P_DATA) {
			atUnTimeout (P);
			P->State = P_ESCAPE;
			atRsp (P, R_OK);
		}
		break;

	case 'D':	/* hard reset, restore current profile defaults */
		/* DBG_TRC(("[%p:%s] RESET:\n", P->Channel, P->Name)) */
		atShut (P, CAUSE_DROP);
		atSetup (P, P->At.f.Id);
		break;
	}
}

byte *atState (byte State)
{
	switch (State) {
	case P_IDLE	    : return ((byte *)"IDLE");
	case P_COMMAND	: return ((byte *)"COMMAND");
	case P_DIALING	: return ((byte *)"DIALING");
	case P_OFFERED	: return ((byte *)"OFFERED");
	case P_ANSWERED	: return ((byte *) "ANSWERED");
	case P_CONNECTED: return ((byte *) "CONNECTED");
	case P_DATA	    : return ((byte *) "DATA");
	case P_ESCAPE	: return ((byte *) "ESCAPE");
	case P_DISCONN	: return ((byte *)"DISCONN");
	}
	return ((byte *)"OTHER");
}

static int atDialError (byte Cause)
{
/* translate disconnect cause to response code */

	switch (Cause) {
case CAUSE_FATAL	: return R_NO_DIALTONE;
	case CAUSE_CONGESTION	:
	case CAUSE_AGAIN	: /* no free channel to set up call */
	case CAUSE_BUSY		: return R_BUSY;
	case CAUSE_OUTOFORDER	:
	case CAUSE_UNREACHABLE	:
	case CAUSE_NOANSWER	: return R_NO_ANSWER;
	case CAUSE_UNKNOWN	:
	case CAUSE_UNAVAIL	:
	case CAUSE_REJECT	:
	case CAUSE_INCOMPATIBLE	:
	case CAUSE_NORMAL	:
	case CAUSE_REMOTE	: return R_NO_CARRIER;
	default			: return R_ERROR;
	}
}

int atNumArg (byte **Arg)
{
/* evaluate numeric argument, advance '*Arg' behind end of number */

	byte *p; int i;

	if (!Arg || !(p = *Arg)) return (-1);

	for (i = 0; *p >= '0' && *p <= '9'; p++) {
		i *= 10; i += *p - '0';
	}

	if (p == *Arg) return (-1);

	*Arg = p;
	return (i);
}

byte atDialCharList[] = "ptw!'#*0123456789abcd";
byte atSkipCharList[] = {
		'@',    /* wait for quiet answer	*/
		'$',    /* wait for billing tone (TAPI)	*/
		'&',    /* wait for billing (Mobile)	*/
		',',	/* non TAPI separator		*/
		'-',	/* non TAPI separator		*/
		'(',	/* non TAPI separator		*/
		')',	/* non TAPI separator		*/
		0	/* END OF TABLE			*/
};

static void atMkPhone (byte *To, byte *From, int size, int dialing)
{
/* copy phone number, remove delimiters and dial mode letters */
	while (size--) {
		*To = *From++;
		if (str_chr ((char*)atDialCharList, *To) || (dialing && *To == ',')) To++;
	}
	*To = 0;
}

static int atIsPhone (byte **Arg, int dialing)
{
/* evaluate phone number, advance '*Arg' behind end of number */

	byte *p; int i, j;

	if (!Arg || !(p = *Arg)) return (-1);

	for (i = j = 0; *p; p++) {
		if (str_chr ((char*)atDialCharList, *p) || (dialing && *p == ','))
		    	i++;	/* valid part of number */
		else if (str_chr ((char*)atSkipCharList, *p))
		    	j++;	/* delimiter or dial mode letter, ignored */
		else
			break;	/* invalid in number	*/
	}

	if (!i || i >= ISDN_MAX_NUMBER) return (0);

	*Arg = p;
	return (i + j);
}

static int diva_bind_port_to_channel (AT_DATA* At, char dir, const char* line) {
	int line_nr = 0;
	int mlt = 1;
	const char* p = line;
	int ret = 0;

	if (!line || !*line) {
		return (-1);
	}
	while (*p && *p >= '0' && *p <= '9') {
		p++;
	}
	if (p == line) {
		return (-1);
	}
	while (--p >= line) {
		line_nr += ((*p - '0') * mlt);
		mlt *= 10;
	}

	switch (dir) {
		case 'a':
		case 'A':
			if ((At->f.i_line != 0) && (isdn_get_adapter_channel_count (At->f.i_line) >= line_nr) &&
					(At->f.o_line != 0) && (isdn_get_adapter_channel_count (At->f.o_line) >= line_nr)) {
				At->f.i_channel = line_nr;
				At->f.o_channel = line_nr;
			} else {
				ret = -1;
			}
			break;

		case 'i':
		case 'I':
			if ((At->f.i_line != 0) && (isdn_get_adapter_channel_count (At->f.i_line) >= line_nr)) {
				At->f.i_channel = line_nr;
			} else {
				ret = -1;
			}
			break;

		case 'o':
		case 'O':
			if ((At->f.o_line != 0) && (isdn_get_adapter_channel_count (At->f.o_line) >= line_nr)) {
				At->f.o_channel = line_nr;
			} else {
				ret = -1;
			}
			break;

		default:
			ret = -1;
			break;
	}
	
	return (ret);
}

static int diva_bind_port (AT_DATA* At, char dir, const char* line) {
	int line_nr = 0;
	int mlt = 1;
	const char* p = line;

	if (!line || !*line) {
		return (-1);
	}
	while (*p && *p >= '0' && *p <= '9') {
		p++;
	}
	if (p == line) {
		return (-1);
	}
	while (--p >= line) {
		line_nr += ((*p - '0') * mlt);
		mlt *= 10;
	}

	if (line_nr > isdn_get_num_adapters ()) {
		return (-1);
	}

	switch (dir) {
		case 'a':
		case 'A':
			At->f.i_line = line_nr;
			At->f.o_line = line_nr;
			break;

		case 'i':
		case 'I':
			At->f.i_line = line_nr;
			break;

		case 'o':
		case 'O':
			At->f.o_line = line_nr;
			break;

		default:
			return (-1);
	}

	/*
		Update of adapter binding erases channel binding
		*/
	At->f.o_channel = At->f.i_channel = 0;
	
	return (0);
}

static int diva_check_v90_speed (dword speed) {
	if ((speed >= 28000) && (speed <= 56000) &&
			((speed % 4000 == 0) ||
			 ((speed % 4000 >= 1300) && (speed % 4000 <= 1333)) ||
			 ((speed % 4000 >= 2600) && (speed % 4000 <= 2667)))) {
		return (0);
  }
	return (-1);
}

static int diva_check_speed (diva_mdm_speed_map_t* map,
														 word speed,
					 									 int automode) {
	int ret = -1;
	int i;

	if (!speed) return (0);

	if (map->check_fn) {
		ret = (*(map->check_fn))(speed);
	} else {
		for (i = 0; map->map[i]; i++) {
			if (map->map[i] == speed) {
				ret = 0;
			}
		}
	}
	if (ret && automode && map->lower) {
		ret = diva_check_speed (map->lower, speed, automode);
	}

	return (ret);
}

static int atPlusMF (ISDN_PORT *P, AT_DATA *At, byte **Arg) {
	byte* p = *Arg, *d;
	int databits =  8;
	int parity   = 'N';
	int stopbits =  0;
	byte framing_cai = 0;

	if (*p == '?') {
		*Arg = ++p;
		if (!At->f.modem_options.framing_valid) {
			atRspStr (P, "\r\n8,n,1    ");
		} else {
			byte tmp[2] = {0,0};
			tmp[0] = (byte)At->f.modem_options.parity;
			sprintf (&P->dbg_tmp[0], "\r\n%d,%s,%d    ",
							 At->f.modem_options.data_bits,
							 &tmp[0],
							 At->f.modem_options.stop_bits);
			atRspStr (P, &P->dbg_tmp[0]);
		}
		return (R_OK);
	}
	if (*p++ != '=') {
		return (R_ERROR);
	}
	if (*p == '?') {
		*Arg = ++p;
		atRspStr (P, "\r\n(8,7,6,5),(N,E,O,S,M),(1,2)");
		return (R_OK);
	}

	switch ((databits = atNumArg (&p))) { /* data bits */
		case 8:
			framing_cai |= DSP_CAI_ASYNC_CHAR_LENGTH_8;
			break;

		case 7:
			framing_cai |= DSP_CAI_ASYNC_CHAR_LENGTH_7;
			break;

		case 6:
			framing_cai |= DSP_CAI_ASYNC_CHAR_LENGTH_6;
			break;

		case 5:
			framing_cai |= DSP_CAI_ASYNC_CHAR_LENGTH_5;
			break;

		default:
			return (R_ERROR);
	}
	if ((d = str_chr (p, ','))) { /* parity */
		p = ++d;
		switch ((parity = *p++)) {
			case 'n': /* none */
				break;
			case 's': /* space */
				framing_cai |= (DSP_CAI_ASYNC_PARITY_ENABLE  |
												DSP_CAI_ASYNC_PARITY_SPACE);
				break;
			case 'o': /* odd */
				framing_cai |= (DSP_CAI_ASYNC_PARITY_ENABLE  |
												DSP_CAI_ASYNC_PARITY_ODD);
				break;
			case 'e': /* even */
				framing_cai |= (DSP_CAI_ASYNC_PARITY_ENABLE  |
												DSP_CAI_ASYNC_PARITY_EVEN);
				break;
			case 'm': /* mark */
				framing_cai |= (DSP_CAI_ASYNC_PARITY_ENABLE  |
												DSP_CAI_ASYNC_PARITY_MARK);
				break;
			default:
				return (R_ERROR);
		}
		if (*p++ != ',') { /* stop bits */
			return (R_ERROR);
		}
		switch ((stopbits = atNumArg (&p))) {
			case 1:
				framing_cai |= DSP_CAI_ASYNC_ONE_STOP_BIT;
				break;
			case 2:
				framing_cai |= DSP_CAI_ASYNC_TWO_STOP_BITS;
				break;
			default:
				return (R_ERROR);
		}
	} else {
		return (R_ERROR);
	}

	*Arg = p;

	if ((databits == 8) && (stopbits == 1) && (parity == 'n')) {
		At->f.modem_options.framing_valid = 0;
	} else {
		At->f.modem_options.framing_valid = 1;
		At->f.modem_options.data_bits			= databits;
		At->f.modem_options.stop_bits			= stopbits;
		At->f.modem_options.parity				= parity;
		At->f.modem_options.framing_cai		= framing_cai;
		DBG_TRC(("[%p:%s] framing(%02x)", P->Channel, P->Name, framing_cai))
	}

	return (R_OK);
}


static int atPlusMS (ISDN_PORT *P, AT_DATA *At, byte **Arg) {
	byte* p = *Arg, *d;
	int Val, i;
	int index;
	int automode = 1;
	int min_rx = 0;
	int max_rx = 0;
	int min_tx = 0;
	int max_tx = 0;
	word unused_modulations = 0;

	static word v34_speed_map[] =  {33600,31200,28800,26400,24000,21600,19200,
	                                16800,14400,12000,9600,7200,4800,2400, 0};
	static word v32b_speed_map[] = {14400,12000,9600,7200,4800, 0};
	static word v32_speed_map[]  = {9600,4800,0};
	static word v22b_speed_map[] = {2400, 1200, 0};
	static word v22_speed_map[]  = {1200, 0};
	static word v23_speed_map[]  = {1200, 0};
	static word v21_speed_map[]  = {300, 0};
	static word b103_speed_map[] = {300, 0};
	static word b212_speed_map[] = {1200, 0};
	static word v22bf_speed_map[]= {2400, 1200, 0};
	static word v22f_speed_map[] = {1200, 0};
	static word v29f_speed_map[] = {9600, 7200, 4800, 2400, 0};
	static word telenot_speed_map[] = {10, 0};
	static word user_speed_map[] = {64000, 0};

	static diva_mdm_speed_map_t b212_map = { 0, b212_speed_map, 0         };
	static diva_mdm_speed_map_t b103_map = { 0, b103_speed_map, 0         };
	static diva_mdm_speed_map_t v21_map  = { 0, v21_speed_map,  0         };
	static diva_mdm_speed_map_t v23_map  = { 0, v23_speed_map,  0         };
	static diva_mdm_speed_map_t v22_map  = { 0, v22_speed_map,  &v21_map  };
	static diva_mdm_speed_map_t v22b_map = { 0, v22b_speed_map, &v22_map  };
	static diva_mdm_speed_map_t v32_map  = { 0, v32_speed_map,  &v22b_map };
	static diva_mdm_speed_map_t v32b_map = { 0, v32b_speed_map, &v32_map  };
	static diva_mdm_speed_map_t v34_map  = { 0, v34_speed_map,  &v32b_map };
	static diva_mdm_speed_map_t v90_map  = { diva_check_v90_speed, 0,
	                                                              &v34_map};
	static diva_mdm_speed_map_t v22f_map = { 0, v22f_speed_map, 0         };
	static diva_mdm_speed_map_t v22bf_map= { 0, v22bf_speed_map,&v22f_map };
	static diva_mdm_speed_map_t v29f_map = { 0, v29f_speed_map, &v22bf_map};

	static diva_mdm_speed_map_t telenot_map = { 0, telenot_speed_map, 0};
	static diva_mdm_speed_map_t user_map = { 0, user_speed_map, 0         };

#define DIVA_MDM_RESERVED_MOD_OPTION_EMPTY_FRAMES     0x0001
#define DIVA_MDM_RESERVED_MOD_OPTION_MULTIMODING      0x0002
#define DIVA_MDM_RESERVED_MOD_OPTION_SHIELD_EMPTY_FRM 0x0004

#define DIVA_MDM_RESERVED_MODULATION_V23_OFF_HOOK     0x00000002
#define DIVA_MDM_RESERVED_MODULATION_V23_ON_HOOK      0x00000004
#define DIVA_MDM_RESERVED_MODULATION_V23_AUTO_FRAMER  0x00000008
#define DIVA_MDM_RESERVED_MODULATION_BELL202_CID      0x00000010
#define DIVA_MDM_RESERVED_MODULATION_TELENOT          0x00000020
#define DIVA_MDM_RESERVED_MODULATION_BELL202_POS      0x00000040
#define DIVA_MDM_RESERVED_MODULATION_BELL103_SIA      0x00000080
#define DIVA_MDM_RESERVED_MODULATION_V23_REVERSE      0x00000100
	static struct {
		int		numeric;
		char* string;
		dword max_tx;
		word	disabled;
		byte	enabled;
		diva_mdm_speed_map_t* tx_map;
		diva_mdm_speed_map_t* rx_map;
		byte  modulation_options;
		dword	reserved_modulation_options;
		dword	reserved_modulation;
		int		group;
	} mod2norm [] = {
		{12, "v90a", 56000, DSP_CAI_MODEM_DISABLE_V90, DSP_CAI_MODEM_ENABLE_V90A,
		                                                            &v34_map,  &v90_map,   0, 0, 0, 0},
		{12, "v90d", 56000, DSP_CAI_MODEM_DISABLE_V90,           0, &v90_map,  &v34_map,   0, 0, 0, 0},
		{12, "v90",  56000, DSP_CAI_MODEM_DISABLE_V90,           0, &v90_map,  &v34_map,   0, 0, 0, 0},
		{11, "v34",  33600, DSP_CAI_MODEM_DISABLE_V34,           0, &v34_map,  &v34_map,   0, 0, 0, 0},
		{10, "v32b", 14400, DSP_CAI_MODEM_DISABLE_V32BIS,        0, &v32b_map, &v32b_map,  0, 0, 0, 0},
		{ 9, "v32",  9600,  DSP_CAI_MODEM_DISABLE_V32,           0, &v32_map,  &v32_map,   0, 0, 0, 0},
		{ 2, "v22b", 2400,  DSP_CAI_MODEM_DISABLE_V22BIS,        0, &v22b_map, &v22b_map,  0, 0, 0, 0},
		{ 1, "v22",  1200,  DSP_CAI_MODEM_DISABLE_V22,           0, &v22_map,  &v22_map,   0, 0, 0, 0},
		{ 3, "v23c", 1200,  DSP_CAI_MODEM_DISABLE_V23,           0, &v23_map,  &v23_map,   0, 0, 0, 0},
		{ 3, "v23",  1200,  DSP_CAI_MODEM_DISABLE_V23,           0, &v23_map,  &v23_map,   0, 0, 0, 0},
		{ 0, "v21",  300,   DSP_CAI_MODEM_DISABLE_V21,           0, &v21_map,  &v21_map,   0, 0, 0, 0},
		{69, "b212a",1200,  DSP_CAI_MODEM_DISABLE_BELL212A << 8, 0, &b212_map, &b212_map,  0, 0, 0, 0},
		{64, "b103", 300,   DSP_CAI_MODEM_DISABLE_BELL103 << 8,  0, &b103_map, &b103_map,  0, 0, 0, 0},
		{ 9, "v29f", 9600,  0, DSP_CAI_MODEM_ENABLE_V29FC,          &v29f_map, &v29f_map,  0, 0, 0, 2},
		{ 2, "v22bf",2400,  0, DSP_CAI_MODEM_ENABLE_V22BISFC,       &v22bf_map,&v22bf_map, 0, 0, 0, 2},
		{ 1, "v22f", 1200,  0, DSP_CAI_MODEM_ENABLE_V22FC,          &v22f_map, &v22f_map,  0, 0, 0, 2},
		{14, "v23hdx", 1200,       0,                            0, &b212_map, &b212_map,
		                           DSP_CAI_MODEM_DISABLE_FLUSH_TIMER,
		                           0, DIVA_MDM_RESERVED_MODULATION_V23_OFF_HOOK, 1},
		{15, "v23hdxon", 1200,     0,                            0, &b212_map, &b212_map,
		                           DSP_CAI_MODEM_DISABLE_FLUSH_TIMER,
		                           0, DIVA_MDM_RESERVED_MODULATION_V23_ON_HOOK, 1},
		{19, "v23s", 1200,         0,                            0, &b212_map, &b212_map,
		                           0,
		                           0, DIVA_MDM_RESERVED_MODULATION_V23_AUTO_FRAMER, 1},
		{204,"b202cid", 1200,      0,                            0, &b212_map, &b212_map,
		                           DSP_CAI_MODEM_DISABLE_FLUSH_TIMER,
		                           0, DIVA_MDM_RESERVED_MODULATION_BELL202_CID, 1},
		{205,"telenot", 1200,      0,                            0, &telenot_map, &telenot_map,
		                           0,
		                           0, DIVA_MDM_RESERVED_MODULATION_TELENOT, 1},
		{206,"b202pos", 1200,      0,                            0, &b212_map, &b212_map,
		                           DSP_CAI_MODEM_DISABLE_FLUSH_TIMER,
		                           0, DIVA_MDM_RESERVED_MODULATION_BELL202_POS, 1},
		{207,"b103sia", 300,       0,                            0, &b103_map, &b103_map,
		                           DSP_CAI_MODEM_DISABLE_FLUSH_TIMER,
		                           DIVA_MDM_RESERVED_MOD_OPTION_MULTIMODING,
		                           DIVA_MDM_RESERVED_MODULATION_BELL103_SIA, 1},
		{208,"v23r", 1200,         0,                            0, &b212_map, &b212_map,
		                           0,
		                           0, DIVA_MDM_RESERVED_MODULATION_V23_REVERSE, 1},
		{216,"user16",64000,       0,                            0, &user_map, &user_map,  0, 0, 0x00010000, 3},
		{217,"user17",64000,       0,                            0, &user_map, &user_map,  0, 0, 0x00020000, 4},
		{218,"user18",64000,       0,                            0, &user_map, &user_map,  0, 0, 0x00040000, 5},
		{219,"user19",64000,       0,                            0, &user_map, &user_map,  0, 0, 0x00080000, 6},
		{220,"user20",64000,       0,                            0, &user_map, &user_map,  0, 0, 0x00100000, 7},
		{221,"user21",64000,       0,                            0, &user_map, &user_map,  0, 0, 0x00200000, 8},
		{222,"user22",64000,       0,                            0, &user_map, &user_map,  0, 0, 0x00400000, 9},
		{223,"user23",64000,       0,                            0, &user_map, &user_map,  0, 0, 0x00800000, 10},
		{ -1, 0}
	};

	/*
		Create mask that contains all unused by us modulations
		*/
	for (i = 0; mod2norm[i].numeric >= 0; i++) {
		unused_modulations |= mod2norm[i].disabled;
	}
	unused_modulations = ~unused_modulations;

	if (*p == '?') {
		if (At->f.modem_options.valid) {
			index = At->f.modem_options.index;
			sprintf (&P->dbg_tmp[0], "\r\n%s,%d,%d,%d,%d,%d",
							 mod2norm [index].string,
							 At->f.modem_options.automode,
							 At->f.modem_options.min_rx,
							 At->f.modem_options.max_rx,
							 At->f.modem_options.min_tx,
							 At->f.modem_options.max_tx);
			atRspStr (P, &P->dbg_tmp[0]);
		} else {
			atRspStr (P, "\r\nV90,1,300,56000,300,56000,300,56000,300,56000");
		}
		*Arg = ++p;
		return R_OK;
	}
	if (*p++ != '=') {
		return R_ERROR;
	}
	if (*p == '?') {
		atRspStr (P, "\r\n(12|V90|V90D|V90A, 11|V34, 10|V32B, 9|V32, 2|V22B, 1|V22, 3|V23, 0|V21, 69|B212A, 64|B103, 14|V23HDX, 15|V23HDXON), (0,1), (300-56000), (300-56000), (300-56000), (300-56000)");
		*Arg = ++p;
		return (R_OK);
	}

	Val  = atNumArg (&p);

	if (Val < 0) {
		/* look by string */
		int last_len   = 0;
		int last_index = -1;
		int len;

		for (index = 0; mod2norm[index].numeric >= 0; index++) {
			len = str_len (mod2norm[index].string);
			if ((!str_n_cmp (p, mod2norm[index].string, len)) && (len > last_len)) {
				last_len   = len;
				last_index = index;
			}
		}

		if (last_index >= 0) {
			index = last_index;
			p += last_len;
		}

	} else { /* look by integer */
		for (index = 0; mod2norm[index].numeric >= 0; index++) {
			if (mod2norm[index].numeric == Val)
				break;
		}
	}

	if (mod2norm[index].numeric < 0) {
		return (R_ERROR);
	}
	if ((d = str_chr (p, ','))) { /* automode */
		p = d;
		p++;
		automode = atNumArg (&p);
		if ((automode != 0) && (automode != 1)) {
			return (R_ERROR);
		}
		if ((d = str_chr (p, ','))) { /* min_rx */
			p = d;
			p++;
			if ((min_rx = atNumArg (&p)) < 0) return R_ERROR;
			if ((d = str_chr (p, ','))) { /* max_rx */
				p = d;
				p++;
				if ((max_rx = atNumArg (&p)) < 0) return R_ERROR;
				if ((d = str_chr (p, ','))) { /* min_tx */
					p = d;
					p++;
					if ((min_tx = atNumArg (&p)) < 0) return R_ERROR;
					if ((d = str_chr (p, ','))) { /* max_tx */
						p = d;
						p++;
						if ((max_tx = atNumArg (&p)) < 0) return R_ERROR;
					}
				}
			}
		}
	}

	if (diva_check_speed (mod2norm[index].rx_map, min_rx, automode) ||
			diva_check_speed (mod2norm[index].rx_map, max_rx, automode) ||
			diva_check_speed (mod2norm[index].tx_map, min_tx, automode) ||
			diva_check_speed (mod2norm[index].tx_map, max_tx, automode)) {
		return (R_ERROR);
	}

	if (((min_rx > max_rx) && min_rx && max_rx) ||
			((min_tx > max_tx) && min_tx && max_tx)) {
		return (R_ERROR);
	}

	At->f.modem_options.valid			= 0;
	At->f.modem_options.disabled 	= 0;
	At->f.modem_options.enabled		= 0;
	At->f.modem_options.min_rx		= \
	At->f.modem_options.max_rx		= \
	At->f.modem_options.min_tx		= \
	At->f.modem_options.max_tx		= 0;
	At->f.modem_options.bell_selected       = 0;
	At->f.modem_options.fast_connect_selected = 0;
	At->f.modem_options.retrain            = (At->f.modem_options.retrain &
		~DSP_CAI_MODEM_DISABLE_FLUSH_TIMER) | mod2norm[index].modulation_options;
	At->f.modem_options.reserved_modulation_options = 0;
	At->f.modem_options.reserved_modulation = 0;

	if (automode) { /* OR all reserved modulations from this group */
		for (i = 0; mod2norm[i].numeric >= 0; i++) {
			if (mod2norm[index].group == mod2norm[i].group) {
				At->f.modem_options.enabled |= mod2norm[i].enabled;
				At->f.modem_options.reserved_modulation_options |= mod2norm[i].reserved_modulation_options;
				At->f.modem_options.reserved_modulation |= mod2norm[i].reserved_modulation;
			}
		}
	} else {
		At->f.modem_options.enabled = mod2norm[index].enabled;
		At->f.modem_options.reserved_modulation_options = mod2norm[index].reserved_modulation_options;
		At->f.modem_options.reserved_modulation = mod2norm[index].reserved_modulation;
	}

	if (automode) { /* disable only modulations above of this one */
		for (i = 0; i < index; i++) {
			if (mod2norm[index].disabled != mod2norm[i].disabled) {
				At->f.modem_options.disabled |= mod2norm[i].disabled;
			}
			if (mod2norm[index].enabled != mod2norm[i].enabled) {
				At->f.modem_options.enabled &= ~mod2norm[i].enabled;
			}
		}
	} else { /* disable all modulations except this one */
		for (i = 0; mod2norm[i].numeric >= 0; i++) {
			if (mod2norm[index].disabled != mod2norm[i].disabled) {
				At->f.modem_options.disabled |= mod2norm[i].disabled;
			}
			if (mod2norm[index].enabled != mod2norm[i].enabled) {
				At->f.modem_options.enabled &= ~mod2norm[i].enabled;
			}
#if 0 /* should not disable V.8 */
			At->f.modem_options.negotiation = DSP_CAI_MODEM_NEGOTIATE_DISABLED;
#endif
		}
	}
	if (At->f.modem_options.disabled) {
		At->f.modem_options.disabled |= unused_modulations;
	}

	At->f.modem_options.min_rx	= (word)min_rx;
	At->f.modem_options.max_rx	= (word)max_rx;
	At->f.modem_options.min_tx	= (word)min_tx;
	At->f.modem_options.max_tx	= (word)max_tx;
	At->f.modem_options.automode= automode;
	At->f.modem_options.index   = index;

	if (At->f.modem_options.negotiation ||
			At->f.modem_options.disabled		||
			At->f.modem_options.enabled		  ||
			At->f.modem_options.min_rx 			||
			At->f.modem_options.max_rx 			||
			At->f.modem_options.min_tx 			||
			At->f.modem_options.max_tx      ||
			At->f.modem_options.modulation_options  ||
			At->f.modem_options.reserved_modulation_options ||
			At->f.modem_options.reserved_modulation) {
		At->f.modem_options.valid = 1;
	}
	*Arg = p;

	if ((mod2norm[index].disabled == (DSP_CAI_MODEM_DISABLE_BELL212A << 8)) ||
	    (mod2norm[index].disabled == (DSP_CAI_MODEM_DISABLE_BELL103  << 8))) {
		At->f.modem_options.bell_selected = 1;
	}
	if ((mod2norm[index].enabled == DSP_CAI_MODEM_ENABLE_V22FC) ||
	    (mod2norm[index].enabled == DSP_CAI_MODEM_ENABLE_V22BISFC) ||
	    (mod2norm[index].enabled == DSP_CAI_MODEM_ENABLE_V29FC)) {
		At->f.modem_options.fast_connect_selected = 1;
	}

	DBG_TRC(("[%p:%s] +MS command", P->Channel, P->Name))
	DBG_TRC(("[%p:%s] automode=%d", P->Channel, P->Name, automode))
	DBG_TRC(("[%p:%s] negotiation=%04x", P->Channel, P->Name,
								At->f.modem_options.negotiation))
	DBG_TRC(("[%p:%s] disabled=%04x", P->Channel, P->Name,
								At->f.modem_options.disabled))
	DBG_TRC(("[%p:%s] enabled =%04x", P->Channel, P->Name,
								At->f.modem_options.enabled))
	DBG_TRC(("[%p:%s] RX(min,max): %d - %d", P->Channel, P->Name,
								min_rx, max_rx))
	DBG_TRC(("[%p:%s] TX(min,max): %d - %d", P->Channel, P->Name,
								min_tx, max_tx))
	DBG_TRC(("[%p:%s] modulation options: %02x", P->Channel, P->Name,
							 At->f.modem_options.modulation_options))
	DBG_TRC(("[%p:%s] reserved modulation options: %04x", P->Channel, P->Name,
							 At->f.modem_options.reserved_modulation_options))
	DBG_TRC(("[%p:%s] reserved modulation: %08x", P->Channel, P->Name,
							 At->f.modem_options.reserved_modulation))
	DBG_TRC(("[%p:%s] ATB%s, BELL: %s, AT$F%d, F: %s", P->Channel, P->Name,
					At->f.modem_options.bell ? "0" : "1",
					At->f.modem_options.bell_selected ? "Y" : "N",
					At->f.modem_options.fast_connect_mode,
					At->f.modem_options.fast_connect_selected ? "Y" : "N"))

	return (R_OK);
}

int atPlusI (ISDN_PORT *P, AT_DATA *At, byte **Arg)
{
/* Evaluate the ISDN specific configuration parameters which may come	*/
/* either as normal AT commands or as part of a called party number,	*/
/* advance '*Arg' behind end of parameter.				*/
/* Call configuration info is set in a temporary config structure not	*/
/* override the permanent configuration.				*/

	byte *p, *q, func;
	int  Val=0, Val_1, Val_2;

	p = *Arg;
	func = *p++;
	Val  = atNumArg (&p);

	switch (func) {
	case 'a' : /* accepted addr */
	case 'o' : /* origination addr */
		p = *Arg + 1;
		if ((Val = atIsPhone (&p, 0)) <= 0) return R_ERROR;
		q = (func == 'a') ? At->Acpt : At->Orig;
		atMkPhone (q, *Arg + 1, Val, 0);
		break;
	case 'b' : /* baud rate for rate adaption protocols */
		if (Val < 0 /* VST: org: 4 */ || Val > 255) return R_ERROR;
		At->f.Baud = (byte) Val;
		break;
	case 'c' : /* stay in command mode after call setup */
		if ((unsigned) Val > 1) return R_ERROR;
		At->f.Data = Val ? 0 : 1;
		break;
	case 'd' : /* delay for AT responses */
		if ((unsigned) Val > 255) return R_ERROR;
		P->At.f.RspDelay = (byte) Val;
		break;
	case 'f' : /* RNA framing */
		if ((unsigned) Val > P_FRAME_MAX) return R_ERROR;
		At->f.RnaFraming = (byte) Val;
		break;
	case 'g' : /* RNA patches */
		if ((unsigned) Val > 255) return R_ERROR;
		At->RnaPatches = (byte) Val;
		break;
	case 'l' : /* maximum length of transparent frames */
		if ((unsigned) Val > ISDN_MAX_FRAME) return R_ERROR;
		At->MaxFrame = (word) Val;
		break;
	case 'm' : /* working mode */
		if (Val < P_MODE_MIN || Val > P_MODE_MAX)
			return R_ERROR;
		At->f.Mode = (byte) Val;
		atNewMode (P, At);
		break;
	case 'n': { /* numbering plan for destination address/origination address/presentation indicator/screening indicator */
		byte Val_3 = 0, Val_4 = 0, octet_3a_present = 0;
		if ((unsigned) Val > 127) return R_ERROR; /* called party number type of number and numbering plan */
		Val_1 = Val; Val_2 = 127;
		if (*p == '/') {
			p++;
			if ((unsigned) (Val_2 = atNumArg(&p)) > 127) /* calling party number type of number and plan */
				return R_ERROR;
			if (*p == '/') {
				p++;
				octet_3a_present = 1;
				if ((Val_3 = (byte)atNumArg(&p)) > 3) /* calling party number presentation indicator */
					return R_ERROR;
				if (*p == '/') {
					p++;
					if ((Val_4 = (byte)atNumArg(&p)) > 3) /* calling party number screening indicator */
						return R_ERROR;
				}
			}
		}

		At->f.PlanDest = (byte)Val_1 | 0x80;
		At->f.PlanOrig = (byte)Val_2;

		if (octet_3a_present == 0) {
			At->f.PlanOrig |= 0x80;
			At->f.Presentation_and_Screening = 0;
			DBG_TRC(("[%08x:%s] CPN[3]:%02x,CiPN[3]:%02x", P->Channel, P->Name, At->f.PlanDest, At->f.PlanOrig))
		} else {
			At->f.Presentation_and_Screening = (Val_3 << 5) | Val_4 | 0x80;
			DBG_TRC(("[%08x:%s] CPN[3]:%02x,CiPN[3]:%02x,CiPN[3a]:%02x",
								P->Channel, P->Name, At->f.PlanDest, At->f.PlanOrig, At->f.Presentation_and_Screening))
		}
	} break;
	case 'p' : /* B-channel protocol */
		if (Val < ISDN_PROT_MIN || Val > ISDN_PROT_MAX) return R_ERROR;
		At->f.Prot = (byte) Val;
		break;
	case 's' : /* Service/ServiceAdd */
		if (p[0] == 'b' || p[0] == 'l') {
			if (p[1] == '"') {
				byte* dst, *src_start = 0;
				int src_length, dst_limit;

				if (p[0] == 'b') {
					dst = &At->f.user_bc[0];
					dst_limit = ISDN_MAX_BC;
				} else {
					dst = &At->f.user_llc[0];
					dst_limit = ISDN_MAX_LLC;
				}
				p += 2;
				src_start = p;

				for (src_length = 0; *p && *p != '"'; p++) {
					src_length++;
				}
				if (*p == 0)
					return R_ERROR;
				if (*p == '"')
					p++;

				*dst = 0;
				if (src_length && string2bin (dst, src_start, dst_limit, src_length))
					return R_ERROR;
			} else {
				return R_ERROR;
			}
		} else {
			if (Val < 1 || Val > 7) return R_ERROR;
			Val_1 = Val; Val_2 = 0;
			if (*p == '/') {
				p++;
				if ((unsigned) (Val_2 = atNumArg(&p)) > 255)
					return R_ERROR;
			}
			At->f.Service	 = (byte) Val_1;
			At->f.ServiceAdd = (byte) Val_2;
		}
		break;
	case 't' : /* trace options */
		if ((unsigned) Val > 255) return R_ERROR;
		At->IsdnTrace = (byte) Val;
		break;

	/* the following heuristics are mainly for the stupid HyperTerminal  */
	case 'h' : /* set HyperTerminal defaults */
		if (Val) {
			At->Wait  = 1;
			At->Block = 81;
			At->Defer = 64000;
			At->Split = 80;
		} else {
			At->Wait  = 0;
			At->Block = 0;
			At->Defer = 0;
			At->Split = 0;
		}
		break;
	case 'w' : /* defer receive notifications to 'Val' bytes/millisecond */
		At->Wait = (word) ((Val < 0) ? 1 : Val);
		break;
	case 'x' : /* respect read blocksize and defer recv notifications */
		At->Block = (word) ((Val < 0) ? 81 : Val);
		break;
	case 'y' : /* defer transmission to 'Val' bytes/millisecond */
		At->Defer = (word) ((((unsigned) Val > 8) ? 8 : Val) * 8000);
		break;
	case 'z' : /* split large frames to 'Val' byte segments */
		At->Split = (word) ((Val < 0) ? 80 : Val);
		break;

	case 'u':
		if (p[0] == '=' && p[1] == '<') {
			const char* bc_start  = 0;
			const char* llc_start = 0;
			byte bc_length = 0, llc_length = 0;
			int i;

			p += 2;
			bc_start = p;

			for (i = 0; *p && *p != '>'; p++) {
				if (llc_start) {
					llc_length++;
				} else {
					if (*p == '/') {
						p++;
						llc_start = p;
						llc_length += *(p+1) != '>';
					} else {
						bc_length++;
					}
				}
			}
			if (*p == 0) {
				return (R_ERROR);
			}
			if (*p == '>')
				p++;

			At->f.user_bc[0]  = 0;
			At->f.user_llc[0] = 0;
			if (bc_length && string2bin (At->f.user_bc, bc_start, ISDN_MAX_BC, bc_length))
				return (R_ERROR);
			if (llc_length && string2bin (At->f.user_llc, llc_start, ISDN_MAX_LLC, llc_length)) {
				At->f.user_bc[0] = 0;
				return (R_ERROR);
			}

			break;
		}
		return (R_ERROR);

	/*
		At escape sequence options
		does define if AT sequence should not be used in
		FRAME mode
	*/
	case 'j':
		if (p[0] == '?') {
			char* p1 = (At->f.at_escape_option) ? "\n1\n" : "\n0\n";
			atRspStr (P, p1);
			p++;
		} else {
			At->f.at_escape_option = (byte) (Val) ? 1 : 0;
		}
    break;

	case 'k':
		if (p[0] == '=') {
			p++;
			switch (*p) {
				case 'a':
				case 'A':
				case 'o':
				case 'O':
				case 'i':
				case 'I':
					if (diva_bind_port_to_channel(At, *p, (p+1))) {
						return R_ERROR;
					}
					p++;
					while (*p && *p >= '0' && *p <= '9') {
						p++;
					}
					if (*p == ';') p++;
					break;

				case '?': {
					char tmp[256];
					int j = 0;

					j += sprintf (&tmp[j],
												At->f.o_line ? "CALL OUT ADAPTER: %02d CHANNELS: %02d\n" : "CALL OUT ADAPTER: any\n",
												At->f.o_line, isdn_get_adapter_channel_count (At->f.o_line));
					j += sprintf (&tmp[j],
												At->f.i_line ? "CALL IN  ADAPTER: %02d CHANNELS: %02d\n" : "CALL IN  ADAPTER: any\n",
												At->f.i_line, isdn_get_adapter_channel_count (At->f.i_line));
					j += sprintf (&tmp[j],
									At->f.o_line && At->f.o_channel ? "CALL OUT CHANNEL: %02d\n" : "CALL OUT CHANNEL: any\n",
												At->f.o_channel);
					j += sprintf (&tmp[j],
									At->f.i_line && At->f.i_channel ? "CALL IN  CHANNEL: %02d\n" : "CALL IN  CHANNEL: any\n",
									At->f.i_channel);

					tmp[j] = 0;
					atRspStr (P, tmp);
					p++;
				} break;

				default:
					return R_ERROR;
			}
		} else {
			return R_ERROR;
		}
		break;

	case 'q':
		if (p[0] == '=') {
			p++;
			switch (*p) {
				case 'a':
				case 'A':
				case 'o':
				case 'O':
				case 'i':
				case 'I':
					if (diva_bind_port(At, *p, (p+1))) {
						return R_ERROR;
					}
					p++;
					while (*p && *p >= '0' && *p <= '9') {
						p++;
					}
					if (*p == ';') p++;
					break;
				case  '1' : {
					extern void print_adapter_status (void);
							print_adapter_status ();
						p++;
					} return (R_ERROR);
				case '?': {
					char tmp[256];
					int j = 0;
					j += sprintf   (&tmp[j], "ADAPTERS        : %02d\n",\
                                                  isdn_get_num_adapters ());
					if (At->f.o_line) {
						j += sprintf (&tmp[j], "CALL OUT ADAPTER: %02d\n", At->f.o_line);
					} else {
						j += sprintf (&tmp[j], "CALL OUT ADAPTER: %s\n", "any");
					}
					if (At->f.i_line) {
						j += sprintf (&tmp[j], "CALL IN  ADAPTER: %02d\n", At->f.i_line);
					} else {
						j += sprintf (&tmp[j], "CALL IN  ADAPTER: %s\n", "any");
					}
					tmp[j] = 0;
					atRspStr (P, tmp);
					p++;
				} break;
				default:
					return R_ERROR;
			}
		} else {
			return R_ERROR;
		}
		break;

	case 'r': /* CPN filter */
		if (p[0] == '?') {
			const char* name;
			int i;

			p++;

			for (i = 0; (name = diva_tty_isdn_get_protocol_name (i)); i++) {
				if (*name) {
					atRspStr (P, (char*)name);
					atRspStr (P, ", ");
				}
			}
			atRspStr (P, "NONE");
		} else if ((p[0] == '=') && (p[1] == '?')) {
			const diva_call_filter_entry_t* entry;
			const char* name;
			int i;

			p += 2;

			atRspStr (P,  "\r\n\r\n");
			for (i = 0; (entry = diva_tty_isdn_get_call_filter_entry (i)); i++) {
				if (entry->number[0] && (name = diva_tty_isdn_get_protocol_name (entry->prot))) {
					atRspStr (P,  (char*)name);
					atRspStr (P, " : ");
					atRspStr (P, (char*)&entry->number[0]);
					atRspStr (P, "\r\n");
				}
			}
			atRspStr (P,  "\r\n");
		} else if (p[0] == '=') {
			/*
				Set call processing filter
				CPN:PROTOCOL
				*/
			int i;

			p++;

			if ((i = diva_tty_isdn_set_call_filter_entry (p)) < 0) {
				return (R_ERROR);
			} else {
				p += i;
			}
		} else {
			return R_ERROR;
		}
		break;

	case 'e': /* Set global call options (type) */
		if (p[0] == '=') {
			p++;
			if        (!str_n_cmp (p, "none",     str_len("none"))) {
				global_mode = 0;
				str_cpy (global_mode_name, "none     ");
				p += str_len("none");
			} else if (!str_n_cmp (p, "piafs32k", str_len("piafs32k"))) {
				global_mode = GLOBAL_MODE_CHINA_PIAFS | ISDN_PROT_PIAFS_32K;
				str_cpy (global_mode_name, "piafs32k");
				p += str_len("piafs32k");
			} else if (!str_n_cmp (p, "piafs64k", str_len("piafs64k"))) {
				global_mode = GLOBAL_MODE_CHINA_PIAFS | ISDN_PROT_PIAFS_64K_FIX;
				str_cpy (global_mode_name, "piafs64k");
				p += str_len("piafs64k");
			} else if (!str_n_cmp (p, "piafs",    str_len("piafs"))) {
				global_mode = GLOBAL_MODE_CHINA_PIAFS | ISDN_PROT_PIAFS_64K_VAR;
				str_cpy (global_mode_name, "piafs    ");
				p += str_len("piafs");
			} else if (!str_n_cmp (p, "?",    str_len("?"))) {
				p++;
				atRspStr (P, "piafs32k, piafs64k, piafs");
			} else {
				return (R_ERROR);
			}
		} else if (p[0] == '?') {
			p++;
			atRspStr (P, global_mode_name);
		} else {
			return (R_ERROR);
		}
		break;

	case 'i' : /* currently reserved for DIVA mobile (serhook.c) */
	default	: /* bad flag */
		return R_ERROR;
	}

	*Arg = p;
	return R_OK;
}

int atSeqBin(byte **Arg, byte *buffer, unsigned int size)
{
// read a sequence of byte values (hex) from given char string into
// the given buffer. Format: aabbccddeeff, two chars for each byte

	byte *p, c, b;
	int  i;

	if (!Arg || !(p = *Arg) || !buffer)
		return (-1);

	i=0;
	do
	{
		// first digit
		c = *p;
		if (c >= 'A' && c <= 'F')
			b = (c - 'A' + 10) << 4;
		else
		if (c >= 'a' && c <= 'f')
			b = (c - 'a' + 10) << 4;
		else
		if (c >= '0' && c <= '9')
			b = (c - '0') << 4;
		else
			break;
		p++;

		// second digit
		c = *p;
		if (c >= 'A' && c <= 'F')
			b |= c - 'A' + 10;
		else
		if (c >= 'a' && c <= 'f')
			b |= c - 'a' + 10;
		else
		if (c >= '0' && c <= '9')
			b |= c - '0';
		else
			return -1;
		p++;

		if (i >= (int)size)
			return -1;
		buffer[i++] = b;
	} while (TRUE);

	*Arg = p;
	return (i);
}

int atBinSeq(byte *data, unsigned int len, byte *buffer, unsigned int size)
{
static byte HexDigitTable[] =
{
	'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'
};
	byte *p;
	unsigned int  u;

	if (!data || !buffer || size < (2*len + 1))
		return -1;

	p = buffer;

	if (len == 0)
	{
		*p++ = '0';
	}
	else
	{
		for (u=0; u < len; u++)
		{
			*p++ = HexDigitTable[data[u] >> 4];
			*p++ = HexDigitTable[data[u] & 0x0f];
		}
	}
	*p   = '\0';

	return (int)(p - buffer);
}

int atStringValues(byte **Arg, word *buffer, unsigned int size)
{
// read a sequence of word values (dec) from given char string into
// the given buffer. Format: "val1,val2,..."

	byte *p;
	word w;
	int  i;

	if (!Arg || !(p = *Arg) || *p++ != '"' || !buffer)
		return (-1);

	i=0;
	while(*p != '"' && *p != '\0' && (*p < '0' || *p > '9'))
		p++;
	while(*p >= '0' && *p <= '9')
	{
		w = 0;
		do
		{
			w = w * 10 + (*p++ - '0');
		} while (*p >= '0' && *p <= '9');

		if (i >= (int)size)
			return -1;
		buffer[i++] = w;
		while(*p != '"' && *p != '\0' && (*p < '0' || *p > '9'))
			p++;
	}
	if (*p == '"')
		p++;

	*Arg = p;
	return (i);
}

int atValuesString(word *data, unsigned int len, byte *buffer, unsigned int size)
{
	byte *p;
	unsigned int  u;

	if (!data || !buffer || (size < 3))
		return -1;

	p = buffer;
	*p++ = '"';

	for (u=0; u < len; u++)
	{
		if (u != 0)
			*p++ = ',';
		if (size < (unsigned int)((p - buffer) + 8))
			return -1;
		p += sprintf (p, "%d", data[u]);
	}

	*p++ = '"';
	*p   = '\0';

	return (int)(p - buffer);
}

int atPlusA(
	ISDN_PORT *             P,
	AT_DATA *               At,
	byte **                 Arg )
{
/* Evaluate the V.8 specific configuration parameters.	*/

	int                     Rc;
	byte *                  p;
	byte                    func;
	int                     i, Val;
	byte                    OrgMode, AnsMode, BisMode;
	byte                    CIBuffer[MAX_V8_CI_LENGTH];
	byte                    CILength;
	word                    CFTable[MAX_V8_CF_VALUES];
	byte                    CFCount;
	word                    ProtTable[MAX_V8_PROT_VALUES];
	byte                    ProtCount;
	byte                    CMBuffer[MAX_V8_CM_LENGTH];
	byte                    CMLength;
	byte                    JMBuffer[MAX_V8_JM_LENGTH];
	byte                    JMLength;
	char                    Str[80];

	p = *Arg;
	if (*p++ != '8')
		return R_ERROR;

	Rc = R_OK;

	func = *p++;
	switch( func )
	{
	case 'e' : /* V.8 and V.8bis operation control */
		if ( *p == '?' )
		{
			p++;

			i = sprintf (Str, "\r\n+A8E: %d,%d,",
			    At->V8.OrgNegotiationMode,
			    At->V8.AnsNegotiationMode);
			i += atBinSeq (At->V8.CIBuffer, At->V8.CILength, &Str[i], sizeof(Str) - i);
			i += sprintf (&Str[i], ",%d,", At->V8.V8bisNegotiationMode);
			i += atValuesString (At->V8.AcceptCallFunctionTable,
			                     At->V8.AcceptCallFunctionCount,
			                     &Str[i], sizeof(Str) - i);
			i += sprintf (&Str[i], ",");
			i += atValuesString (At->V8.AcceptProtocolTable,
			                     At->V8.AcceptProtocolCount,
			                     &Str[i], sizeof(Str) - i);
			i += sprintf (&Str[i], "\r\n");
			atRspFlush (P);
			portRxEnqueue (P, Str, (word)(str_len (Str)), 0);
			break;
		}

		if ( *p++ != '=' )
			return R_ERROR;

		if ( *p == '?' )
		{
			p++;

			i = sprintf (Str, "\r\n+A8E: (0,1,3,6),(0-5),(%d),(0),(4,5),(1)\r\n",
			    MAX_V8_CI_LENGTH);
			atRspFlush (P);
			portRxEnqueue (P, Str, (word)(str_len (Str)), 0);
			break;
		}

		OrgMode = 0;
		AnsMode = 0;
		BisMode = 0;
		CILength = 0;
		CFCount = 0;
		ProtCount = 0;

		/* get origination negotiation mode */
		Val = atNumArg( &p );
		if( Val > 6 )
			return R_ERROR;

		if( Val >= 0 )
			OrgMode = (byte)Val;

		if( *p != ',' )
			return R_ERROR;

		p++;

		/* get answer negotiation mode */
		Val = atNumArg (&p);
		if( Val > 6 )
			return R_ERROR;

		if( Val >= 0 )
			AnsMode = (byte)Val;

		if( *p == ',' )
		{
			p++;
			/* get CI call function */
			Val = atSeqBin(&p, CIBuffer, MAX_V8_CI_LENGTH);
			if (Val < 0)
				return (R_ERROR);
			CILength = (byte) Val;
			if (*p == ',')
			{
				p++;
				/* get V.8 bis negotiation mode */
				Val = atNumArg (&p);
				if (Val > 2)
					return (R_ERROR);
				if (Val >= 0)
					BisMode = (byte) Val;
				if (*p == ',')
				{
					p++;
					/* get accept call function range */
					Val = atStringValues(&p, CFTable, MAX_V8_CF_VALUES);
					if (Val < 0)
						return (R_ERROR);
					CFCount = (byte) Val;
					if (*p == ',')
					{
						p++;
						/* get accept protocol range */
						Val = atStringValues(&p, ProtTable, MAX_V8_PROT_VALUES);
						if (Val < 0)
							return (R_ERROR);
						ProtCount = (byte) Val;
					}
				}
			}
		}
		At->V8.OrgNegotiationMode = OrgMode;
		At->V8.AnsNegotiationMode = AnsMode;
		memcpy (At->V8.CIBuffer, CIBuffer, sizeof(CIBuffer));
		At->V8.CILength = CILength;
		At->V8.V8bisNegotiationMode = BisMode;
		memcpy (At->V8.AcceptCallFunctionTable, CFTable, sizeof(CFTable));
		At->V8.AcceptCallFunctionCount = CFCount;
		memcpy (At->V8.AcceptProtocolTable, ProtTable, sizeof(ProtTable));
		At->V8.AcceptProtocolCount = ProtCount;

		switch( P->Mode )
		{
		case P_MODE_FAX  :
			Rc = fax1V8Setup( P );
		}
		break;

	case 'm' : /* Send V.8 menu signals */
		if( *p++ != '=' )
			return R_ERROR;

		if( P->Direction == P_CONN_OUT )
		{
			/* get call menu */
			Val = atSeqBin( &p, CMBuffer, MAX_V8_CM_LENGTH );
			if( Val < 0 )
				return R_ERROR;

			CMLength = (byte) Val;
			memcpy (At->V8.CMBuffer, CMBuffer, sizeof(CMBuffer));
			At->V8.CMLength = CMLength;
		}
		else
		{
			/* get joint menu */
			Val = atSeqBin( &p, JMBuffer, MAX_V8_JM_LENGTH );
			if( Val < 0 )
				return R_ERROR;

			JMLength = (byte) Val;
			memcpy (At->V8.JMBuffer, JMBuffer, sizeof(JMBuffer));
			At->V8.JMLength = JMLength;
		}

		switch( P->Mode )
		{
		case P_MODE_FAX  :
			Rc = fax1V8Menu( P );
		}
		break;

	default	: /* bad flag */
		return R_ERROR;
	}

	*Arg = p;
	return Rc;
}

static int atPlus (ISDN_PORT *P, byte **Arg)
{
	byte *p;
	int  Rc;

	p = *Arg;

	switch (*p++) {
	case 'a' : /* V.8    */ Rc = atPlusA (P, &P->At, &p); break;
	case 'i' : /* ISDN   */ Rc = atPlusI (P, &P->At, &p); break;
	case 'f' : /* FAX    */ Rc = faxPlusF (P, &p); break;
	case 'm' :
		switch (*p++) {
			case 's': /* modulation */
				Rc = atPlusMS (P, &P->At, &p);
				break;
			case 'f': /* framing */
				Rc = atPlusMF (P, &P->At, &p);
				break;
			default:
				return (R_ERROR);
		}
		break;
	case 'v' : /* VOICE  */
	default  : /* bad id */ return (R_ERROR);
	}

	*Arg = p;
	return (Rc);
}

static int atRegs (ISDN_PORT *P, byte **Arg, byte Cmd)
{
	byte *p, tmp[128]; int Val = 0;
	byte *ptrtemp;
	byte tempstr[30] = "\r\nR%02d=%02x";

	ptrtemp = tempstr;

	p = *Arg;

	switch (Cmd) {
	case 's' : /* set number */
       Val = atNumArg(&p);
		   P->At.curReg = (word) Val;
		   break;

	case '=' : /* set value  */
			if (P->At.curReg != 1001 && (unsigned)(Val = atNumArg(&p)) > 255) return R_ERROR;
			if (P->At.curReg < AT_MAX_REGS) {
				P->At.f.Regs[P->At.curReg] = (byte) Val;
			} else {
				switch (P->At.curReg) {
					case 7:
						P->At.f.modem_options.s7  = (byte)Val;
						break;
					case 10:
						P->At.f.modem_options.s10 = (byte)Val;
						break;
					case 27:
						P->At.f.S27 = (byte) Val;
						P->At.f.modem_options.line_taking &= \
																					~DSP_CAI_MODEM_DISABLE_ANSWER_TONE;
						if (P->At.f.S27 & 0x08) {
							P->At.f.modem_options.line_taking |= \
																					DSP_CAI_MODEM_DISABLE_ANSWER_TONE;
						}
						break;

					case 51:
						P->At.f.S51 = (byte)Val;
						P->At.f.modem_options.protocol_options &= \
                                  ~(DLC_MODEMPROT_NO_PROTOCOL_IF_1200 |
                                    DLC_MODEMPROT_NO_PROTOCOL_IF_V22BIS |
                                    DLC_MODEMPROT_NO_PROTOCOL_IF_V32BIS);
						if (P->At.f.S51 & 0x01) {
							P->At.f.modem_options.protocol_options |= \
																					DLC_MODEMPROT_NO_PROTOCOL_IF_1200;
						}
						if (P->At.f.S51 & 0x02) {
							P->At.f.modem_options.protocol_options |= \
                                          DLC_MODEMPROT_NO_PROTOCOL_IF_V22BIS;
						}
						if (P->At.f.S51 & 0x04) {
							P->At.f.modem_options.protocol_options |= \
                                          DLC_MODEMPROT_NO_PROTOCOL_IF_V32BIS;
						}
						break;

					case 138: /* FAX Line taking options */
						P->At.f.modem_options.fax_line_taking &= ~0x00ffU;
						P->At.f.modem_options.fax_line_taking |= (byte)Val;
						break;

					case 140: /* SDLC L2 protocol options */
						P->At.f.modem_options.sdlc_prot_options = (byte)Val;
						break;
					case 141: /* SDLC L2 protocol address A */
						P->At.f.modem_options.sdlc_address_A    = (byte)Val;
						break;

					case 160: /* Line taking options */
						P->At.f.modem_options.line_taking &= ~0x00ffU;
						P->At.f.modem_options.line_taking |= (byte)Val;
						break;

					case 162: /* Options for modulations */
						P->At.f.modem_options.modulation_options &= ~0x00ffU;
						P->At.f.modem_options.modulation_options |= (byte)Val;
						break;

					case 172: /* Options for reserved modulations */
						P->At.f.modem_options.reserved_modulation_options &= ~0x00ff;
						P->At.f.modem_options.reserved_modulation_options |= (byte)Val;
						break;

					case 253:
						if ((P->At.f.S253 = (byte)Val) != 0) {
							P->At.f.S253 |= 0x80;
						}
						break;
					case 254:
						/*
							Features register
							BIT 0 - Allows "ATH" in RING state
							BIT 1 - Allows TIES
							*/
						P->At.f.S254 = (byte)Val;
						break;

					case 1001: /* Second CiPN */
						if ((Val = atIsPhone (&p, 0)) < 0)
							return R_ERROR;
						atMkPhone (P->At.Orig_1, *Arg, Val, 0);
						break;
					case 1002: /* numbering plan for second CiPN */
						if (((unsigned)Val) > 255)
							return R_ERROR;
						P->At.PlanOrig_1 = (byte)Val;
						break;
					case 1003: /* presentation and screening for second CiPN */
						if (((unsigned)Val) > 127)
							return R_ERROR;
						P->At.PresentationAndScreening_1 = (byte)Val;
						break;

				}
			}
			 
			break;

	case '?' : /* query value */
		   atRspFlush (P);
			 Val = 0;
			 if (P->At.curReg < AT_MAX_REGS) {
				 Val = P->At.f.Regs[P->At.curReg];
			 } else {
				 switch (P->At.curReg) {
					case 7:
						Val = P->At.f.modem_options.s7;
						break;
					case 10:
						Val = P->At.f.modem_options.s10;
						break;
					case 27:
						Val = P->At.f.S27;
						break;
					case 51:
						Val = P->At.f.S51;
						break;
					case 128: /* ask information about peer */
						sprintf (tmp, "\r\n;%s%s%s;%s%s%s",
										 &P->CallbackParms.Orig[0],
										 P->CallbackParms.OrigSub[0] ? "/" : "",
										 &P->CallbackParms.OrigSub[0],
										 &P->CallbackParms.Dest[0],
										 P->CallbackParms.DestSub[0] ? "/" : "",
										 &P->CallbackParms.DestSub[0]);
						atRspFlush (P);
						ptrtemp = 0;
						break;

					case 138: /* FAX Line taking options */
						Val = (byte)(P->At.f.modem_options.fax_line_taking & 0x00ff);
						break;

					case 140:
						Val = P->At.f.modem_options.sdlc_prot_options;
						break;
					case 141:
						Val = P->At.f.modem_options.sdlc_address_A;
						break;

					case 160: /* Line taking options */
						Val = (byte)(P->At.f.modem_options.line_taking & 0x00ff);
						break;

					case 162: /* Options for modulations */
						Val = (byte)(P->At.f.modem_options.modulation_options & 0x00ff);
						break;

					case 172: /* Options for reserved modulations */
						Val = (byte)(P->At.f.modem_options.reserved_modulation_options & 0x00ff);
						break;

					case 253:
						Val = P->At.f.S253 & 0x7f;
						break;
					case 254:
						/*
							Options register
							*/
						Val = P->At.f.S254;
						break;

					case 1001:
						sprintf (tmp, "\r\n%s", P->At.Orig_1);
						atRspFlush (P);
						ptrtemp = 0;
						break;
					case 1002:
						Val = P->At.PlanOrig_1;
						break;
					case 1003:
						Val = P->At.PresentationAndScreening_1;
						break;
				 }
			 }
			 if (ptrtemp) {
				 	sprintf (tmp, ptrtemp, P->At.curReg, Val);
					DBG_TRC(("[%p:%s] ATS%d read %02x",
										P->Channel, P->Name, P->At.curReg, Val))
			 }
		   portRxEnqueue (P, tmp, (word) str_len ((char*)tmp), 0);
		   break;
	default  : /* bad operation */
		   return R_ERROR;
	}

	*Arg = p;
	return R_OK;
}

static int atBack (ISDN_PORT *P, byte **Arg)
{
	byte *p, func; int Val;

	p    = *Arg;
	func = *p++;

	Val = atNumArg (&p) ; if ( Val < 0 ) Val = 0 ;

	switch (func) {

	case 'n':
		switch (Val) {
			case 0:
			case 1:
				DBG_TRC(("[%p:%s] disable error control", P->Channel, P->Name))
				P->At.f.modem_options.disable_error_control |= \
														(DLC_MODEMPROT_DISABLE_V42_V42BIS |
														 DLC_MODEMPROT_DISABLE_MNP_MNP5   |
														 DLC_MODEMPROT_DISABLE_V42_DETECT |
														 DLC_MODEMPROT_DISABLE_SDLC);
				P->At.f.modem_options.disable_error_control &= \
																		~DLC_MODEMPROT_REQUIRE_PROTOCOL;
				break;

			case 2:
			case 8:
				DBG_TRC(("[%p:%s] enable MNP only", P->Channel, P->Name))
				P->At.f.modem_options.disable_error_control &= \
														~(DLC_MODEMPROT_DISABLE_MNP_MNP5   |
														 	DLC_MODEMPROT_DISABLE_V42_DETECT |
															DLC_MODEMPROT_REQUIRE_PROTOCOL);
				P->At.f.modem_options.disable_error_control |= DLC_MODEMPROT_DISABLE_V42_V42BIS;
				P->At.f.modem_options.disable_error_control |= DLC_MODEMPROT_DISABLE_SDLC;
				break;

			case 3: /* default setting */
				DBG_TRC(("[%p:%s] enable V.42 and MNP", P->Channel, P->Name))
				P->At.f.modem_options.disable_error_control &= \
														~(DLC_MODEMPROT_DISABLE_V42_V42BIS |
														 	DLC_MODEMPROT_DISABLE_MNP_MNP5   |
														 	DLC_MODEMPROT_DISABLE_V42_DETECT |
															DLC_MODEMPROT_REQUIRE_PROTOCOL);
				P->At.f.modem_options.disable_error_control |= DLC_MODEMPROT_DISABLE_SDLC;
				break;

			case 4:
				DBG_TRC(("[%p:%s] require V.42 only", P->Channel, P->Name))
				P->At.f.modem_options.disable_error_control &= \
														~(DLC_MODEMPROT_DISABLE_V42_V42BIS |
														 	DLC_MODEMPROT_DISABLE_V42_DETECT);
				P->At.f.modem_options.disable_error_control |= DLC_MODEMPROT_DISABLE_MNP_MNP5;
				P->At.f.modem_options.disable_error_control |= DLC_MODEMPROT_DISABLE_SDLC;
				P->At.f.modem_options.disable_error_control |= DLC_MODEMPROT_REQUIRE_PROTOCOL;
				break;

			case 5:
				DBG_TRC(("[%p:%s] require MNP only", P->Channel, P->Name))
				P->At.f.modem_options.disable_error_control &= \
														~(DLC_MODEMPROT_DISABLE_MNP_MNP5   |
														 	DLC_MODEMPROT_DISABLE_V42_DETECT);
				P->At.f.modem_options.disable_error_control |= DLC_MODEMPROT_DISABLE_V42_V42BIS;
				P->At.f.modem_options.disable_error_control |= DLC_MODEMPROT_DISABLE_SDLC;
				P->At.f.modem_options.disable_error_control |= DLC_MODEMPROT_REQUIRE_PROTOCOL;
				break;

			case 6:
				DBG_TRC(("[%p:%s] require V.42 or MNP", P->Channel, P->Name))
				P->At.f.modem_options.disable_error_control &= \
														~(DLC_MODEMPROT_DISABLE_V42_V42BIS |
                              DLC_MODEMPROT_DISABLE_MNP_MNP5   |
														 	DLC_MODEMPROT_DISABLE_V42_DETECT);
				P->At.f.modem_options.disable_error_control |= DLC_MODEMPROT_DISABLE_SDLC;
				P->At.f.modem_options.disable_error_control |= DLC_MODEMPROT_REQUIRE_PROTOCOL;
				break;

			case 7:
				DBG_TRC(("[%p:%s] enable V.42 only", P->Channel, P->Name))
				P->At.f.modem_options.disable_error_control &= \
														~(DLC_MODEMPROT_DISABLE_V42_V42BIS |
														 	DLC_MODEMPROT_DISABLE_V42_DETECT |
				                      DLC_MODEMPROT_REQUIRE_PROTOCOL);
				P->At.f.modem_options.disable_error_control |= DLC_MODEMPROT_DISABLE_MNP_MNP5;
				P->At.f.modem_options.disable_error_control |= DLC_MODEMPROT_DISABLE_SDLC;
				break;

			case 9:
				break;

			case 10:
				DBG_TRC(("[%p:%s] require SDLC only", P->Channel, P->Name))
				P->At.f.modem_options.disable_error_control &= ~DLC_MODEMPROT_DISABLE_SDLC;
				P->At.f.modem_options.disable_error_control |= DLC_MODEMPROT_DISABLE_MNP_MNP5;
				P->At.f.modem_options.disable_error_control |= DLC_MODEMPROT_DISABLE_V42_V42BIS;
				P->At.f.modem_options.disable_error_control |= DLC_MODEMPROT_DISABLE_V42_DETECT;
				P->At.f.modem_options.disable_error_control |= DLC_MODEMPROT_REQUIRE_PROTOCOL;
				break;

			case 11:
				DBG_TRC(("[%p:%s] enable SDLC only", P->Channel, P->Name))
				P->At.f.modem_options.disable_error_control &= ~(DLC_MODEMPROT_DISABLE_SDLC |
				                                               DLC_MODEMPROT_REQUIRE_PROTOCOL);
				P->At.f.modem_options.disable_error_control |= DLC_MODEMPROT_DISABLE_MNP_MNP5;
				P->At.f.modem_options.disable_error_control |= DLC_MODEMPROT_DISABLE_V42_V42BIS;
				P->At.f.modem_options.disable_error_control |= DLC_MODEMPROT_DISABLE_V42_DETECT;
				break;

			case 12:
				DBG_TRC(("[%p:%s] enable SDLC and MNP", P->Channel, P->Name))
				P->At.f.modem_options.disable_error_control &= ~(DLC_MODEMPROT_DISABLE_SDLC       |
				                                               DLC_MODEMPROT_DISABLE_V42_DETECT |
				                                               DLC_MODEMPROT_DISABLE_MNP_MNP5   |
				                                               DLC_MODEMPROT_REQUIRE_PROTOCOL);
				P->At.f.modem_options.disable_error_control |= DLC_MODEMPROT_DISABLE_V42_V42BIS;
				break;

			case (12+1):
				DBG_TRC(("[%p:%s] enable SDLC and V.42", P->Channel, P->Name))
				P->At.f.modem_options.disable_error_control &= ~(DLC_MODEMPROT_DISABLE_SDLC       |
				                                               DLC_MODEMPROT_DISABLE_V42_DETECT |
				                                               DLC_MODEMPROT_DISABLE_V42_V42BIS |
				                                               DLC_MODEMPROT_REQUIRE_PROTOCOL);
				P->At.f.modem_options.disable_error_control |= DLC_MODEMPROT_DISABLE_MNP_MNP5;
				break;

			case 14:
				DBG_TRC(("[%p:%s] enable all", P->Channel, P->Name))
				P->At.f.modem_options.disable_error_control = 0;
				break;


			default:
				return (R_ERROR);
		}
		break;

	case 'd' : /* debug flag */
#if !defined(UNIX)
		logMask = (byte) Val;
#endif
		break;
	case 't' : /* inactivity timeout */
		if (Val)
			Val = Val/10 ? Val/10 : 1;
		P->At.RwTimeout = (word) Val;
		break;
	case 'v' : /* very verbose modem CONNECT message */
		P->At.f.ConFormat = (byte) Val ;
		break;

	default  : /* bad option */
		return R_ERROR;
	}

	*Arg = p;
	return R_OK;
}

static int atHash (ISDN_PORT *P, byte **Arg)
{
	byte *p;
	char Str[128];
	int i;

	p = *Arg;

	if ( p[0] != 'c' || p[1] != 'i' || p[2] != 'd' )
		return R_ERROR;	/* not a #CID command */

	p += 3;

	switch ( *p++ )
	{
	case '=' :
		if (*p != '?') {
			i = atNumArg (&p);
			if (i > 15) {
				return R_ERROR;
      }

			P->At.f.cid_setting = i;
			break;
		}
		/* current mode query, fall through to send 0 response */
		p++ ;
	case '?' :
		i = sprintf (Str, "\r\n%d\r\n", P->At.f.cid_setting);
		portRxEnqueue (P, (byte*)Str, i, 0);
		break;
	default  :
		return R_ERROR;
	}

	*Arg = p;
	return R_OK;
}

/*
	AT$ commands
	*/
static int atDollar (ISDN_PORT* P, byte** Arg) {
	byte *p, func;
	int Val, i;
	char Str[24];

	p    = *Arg;
	func = *p++;

	switch (func) {
		case 'f':
			if (*p == '?') {
				p++;
				i = sprintf (Str, "\r\n%d\r\n", P->At.f.modem_options.fast_connect_mode);
				portRxEnqueue (P, (byte*)Str, i, 0);
			} else {
      	Val = atNumArg(&p);
 	     if ((Val < 0) || (Val > 4)) {
					return (R_ERROR);
				}
				P->At.f.modem_options.fast_connect_mode = (byte)Val;
			}
			break;

		default:
			return (R_ERROR);
	}

	*Arg = p;
	return R_OK;
}

static int atAmper (ISDN_PORT *P, byte **Arg)
{
	byte *p, *v, func, *tmp;
	int  Val, i; AT_CONFIG *prf;
	
	p    = *Arg;
	func = *p++;
	Val  = atNumArg (&p);

	switch (func) {

	case 'c' : /* data carrier detect options (for compatibility) */
		break;

	case 'd' : /* data terminal ready options */
		if ((unsigned) Val > 3) return R_ERROR;
		P->At.f.DtrOpts = (byte) Val;
		break;

	case 'k' : /* terminal flow control options	*/
		/* 0 - no flow control			*/
		/* 1 - RTS/CTS local+remote (obsolete)	*/
		/* 2 - XON/XOFF (obsolete)		*/
		/* 3 - RTS/CTS local+remote (data modem)*/
		/* 4 - XON/XOFF                         */
		/* 5 - transparent XON/XOFF             */
		/* 6 - both XON/XOFF & RTS/CTS (fax, voice) */
		if (Val > AT_FLOWOPTS_MAX) return R_ERROR;
		P->At.f.FlowOpts = (Val < 0) ? 0 : (byte) Val;
		break;

	case 'f' : /* set factory defaults for all options */
		if (Val > (int) MAX_PROFILE) return R_ERROR;
		P->At.f.user_bc[0] = P->At.f.user_llc[0] = 0;
		P->At.f.user_orig_subadr[0] = 0;
		atProfile (&P->At, (Val <= 0) ? PortDriver.Profile : Val);
		atNewMode (P, &P->At);
		P->At.f.cid_setting = 0;
		P->At.f.at_escape_option = 0;
		P->At.f.o_line           = 0;
		P->At.f.i_line           = 0;
		P->At.f.o_channel        = 0;
		P->At.f.i_channel        = 0;
		break;

	case 'q' : /* communications mode options */
		if ((unsigned) Val > 3) return R_ERROR;
		P->At.f.ComOpts = (byte) Val;
		break;

	case 'v' : /* show current settings and factory profiles */
		i = /* hyperterminal patches */
		((P->At.Wait | P->At.Block | P->At.Defer | P->At.Split) != 0);
		tmp = diva_mem_malloc(2048);
		if (tmp == NULL) {
			return (R_ERROR);
		}
		v = tmp;
			v += sprintf (v,
(byte*)"\r\n\
PROFILE --- E%d V%d Q%d X%d &D%d &K%d &Q%d S0=%d +iM%d +iP%d +iS%d/%d +iB%d\
\r\n\
               +iO'%s' +iA'%s' +iN%d/%d +iF%d +iD%d +iG%d +iH%d",
		/* line 1 */
		P->At.f.Echo, P->At.f.Verbose, P->At.f.Quiet, P->At.f.Progress,
		P->At.f.DtrOpts, P->At.f.FlowOpts, P->At.f.ComOpts,
		P->At.f.Regs[0], P->At.f.Mode, P->At.f.Prot,
		P->At.f.Service, P->At.f.ServiceAdd, P->At.f.Baud,
		/* line 2 */
		P->At.Orig, P->At.Acpt,
		P->At.f.PlanDest, P->At.f.PlanOrig,
		P->At.f.RnaFraming, P->At.f.RspDelay, P->At.RnaPatches, i);

		if (i) {
			v += sprintf (v, (byte*)"\r\n\
           +iW%d +iX%d +iY%d +iZ%d",
		P->At.Wait, P->At.Block, P->At.Defer/8000, P->At.Split);
		}
		if (P->At.Dest[0]) {
			v += sprintf (v, (byte*) "\r\n\
                  last DIAL to '%s'", P->At.Dest);
		}
		if (P->At.Ring[0]) {
			v += sprintf (v, (byte*) "\r\n\
                  last RING from '%s'", P->At.Ring);
		}

		if (Val >= 0) {
			*v++ = '\r'; *v++ = '\n';
		}

		for (i = 1, prf = Profiles + 1;
		     Val >= 0 && prf < Profiles + MAX_PROFILE; i++, prf++) {
			v += sprintf (v, (byte*)"\r\n\
PROFILE %2d: E%d V%d Q%d X%d &D%d &K%d &Q%d S0=%d +iM%d +iP%d +iS%d/%d +iB%d",
			i, prf->Echo, prf->Verbose, prf->Quiet, prf->Progress,
			prf->DtrOpts, prf->FlowOpts, prf->ComOpts,
			prf->Regs[0], prf->Mode, prf->Prot,
			prf->Service, prf->ServiceAdd, prf->Baud);
		}

		if (!P->At.f.Verbose) {	/* don't override last line */
			*v++ = '\r'; *v++ = '\n';
		}

		atRspFlush (P);
		portRxEnqueue (P, tmp, (word) (v - tmp), 0);
		diva_mem_free(tmp);

		break;

	case 'g': /* &G[0...2] - Guard Tone operation */
		if (((byte)Val) > 2) return R_ERROR;

		switch (Val) {
			case 0:
				P->At.f.modem_options.guard_tone = DSP_CAI_MODEM_GUARD_TONE_NONE;
				break;
			case 1:
				P->At.f.modem_options.guard_tone = DSP_CAI_MODEM_GUARD_TONE_550HZ;
				break;
			case 2:
				P->At.f.modem_options.guard_tone = DSP_CAI_MODEM_GUARD_TONE_1800HZ;
				break;
			default:
				return (R_ERROR);
		}


		break;

	default  : /* bad option */
		return R_ERROR;
	}

	*Arg = p;
	return R_OK;
}

static int atPercent (ISDN_PORT *P, byte **Arg) {
	byte *p, func;
	int  Val;
	
	p    = *Arg;
	func = *p++;
	Val  = atNumArg (&p);

	switch (func) {
		case 'e':	/* disable/enable retrain/stepdown */
			if (Val) {
				P->At.f.modem_options.retrain &= ~(DSP_CAI_MODEM_DISABLE_RETRAIN |
				                                   DSP_CAI_MODEM_DISABLE_STEPDOWN |
				                                   DSP_CAI_MODEM_DISABLE_STEPUP);
			} else {
				P->At.f.modem_options.retrain |= (DSP_CAI_MODEM_DISABLE_RETRAIN |
				                                  DSP_CAI_MODEM_DISABLE_STEPDOWN |
				                                  DSP_CAI_MODEM_DISABLE_STEPUP);
			}
			break;


		case 'c': /* disable/enable compression */
			switch (Val) {
				case 0:
					P->At.f.modem_options.disable_error_control |= \
					                         DLC_MODEMPROT_DISABLE_COMPRESSION;
					break;

				case 1:
				case 2:
				case 3:
					P->At.f.modem_options.disable_error_control &= \
					                        ~DLC_MODEMPROT_DISABLE_COMPRESSION;
					break;

				default:
					return (R_ERROR);
			}

			break;

		default:
			return (R_ERROR);
	}

	*Arg = p;
	return R_OK;
}

static void atAdjustCall (ISDN_PORT *P, AT_DATA *At, byte *Prot, byte Baud)
{
	if (*Prot == ISDN_PROT_DET) *Prot = At->f.Prot;

	P->At.Prot  = *Prot;

	P->At.Data  = (/* P->Mode == P_MODE_VOICE || */ P->Mode == P_MODE_FAX) ?
		       0 : At->f.Data;

	/* adjust the speed to report when connected */

	if (*Prot == ISDN_PROT_MDM_s || *Prot == ISDN_PROT_MDM_a) {
		/* currently we don't know the negotiated speed */
		P->At.Speed = 14400;
	} else if ((*Prot == ISDN_PROT_V110_a) &&
              (Baud < (sizeof(v110SpeedMap)/sizeof(v110SpeedMap[0])))) {
		P->At.Speed = v110SpeedMap[Baud];
	} else if (Baud >= 2 && Baud <= 9) {
		P->At.Speed = SpeedMap[Baud - 1] ;
	} else {
		P->At.Speed = 64000;
	}

	/* adjust the special hacks */

	P->Defer = At->Defer;	/* applies to all modes */
	P->Split = P->Wait = P->Block = 0;


	switch (P->Mode) {
	case P_MODE_MODEM :
		if (At->Split)
			P->Split = At->Split;
		else if (*Prot == ISDN_PROT_V120)
 			P->Split = ISDN_MAX_V120_FRAME;
		else
			P->Split = ISDN_MAX_FRAME;
		P->Wait	 = At->Wait;
		P->Block = At->Block;
		break;
	case P_MODE_FAX	  :
	case P_MODE_VOICE :
		P->Split = At->Split;
		P->Wait	 = At->Wait;
		P->Block = At->Block;
		break;
	case P_MODE_RNA:
		P->port_u.Rna.Framing = At->f.RnaFraming;
		P->port_u.Rna.Patches = At->RnaPatches;
		break;
	case P_MODE_BTX	  :
	case P_MODE_FRAME :
	default:
		break;
	}
}

int atAnswer (ISDN_PORT *P)
{
	byte                    Prot;
	unsigned long           u;

	atRingOff (P);

	if (P->State != P_OFFERED) {
		/* DBG_TRC(("[%p:%s] ANSWER: %s ?\n", P->Channel, P->Name, atState (P->State))) */
		return R_NO_CARRIER;
	}

	if (P->Channel) {
		static ISDN_CONN_PARMS	Parms ;			/* all dial parameters	  */

		Parms = P->CallbackParms;

		/*--- set mode and prot depending of current class ---------------------*/
		P->FlowOpts = P->At.f.FlowOpts;
		switch (P->Class) {
		case P_CLASS_FAX1:
		case P_CLASS_FAX10:
		case P_CLASS_FAX2:
		case P_CLASS_FAX20:
		case P_CLASS_FAX21:
			P->Mode	= P_MODE_FAX;
			Prot	= ISDN_PROT_FAX;
			P->FlowOpts = ((P->At.f.FlowOpts == AT_FLOWOPTS_BOTH) ||
			               (P->At.f.FlowOpts == AT_FLOWOPTS_RTSCTS)) ?
		        	        AT_FLOWOPTS_BOTH : AT_FLOWOPTS_XONXOFF;
			break;
		case P_CLASS_VOICE:
			P->Mode	= P_MODE_VOICE;
			Prot	= ISDN_PROT_VOICE;
			break;
		case P_CLASS_DATA:
		default:
			/*--- use configured params ----------------------------------------*/
			Prot = P->At.f.Prot;

			if (!global_mode) {
				if ( (Parms.Service <= 2) && (P->At.f.Service > 2)
				  && (P->At.f.Prot != ISDN_PROT_MDM_a) && (P->At.f.Prot != ISDN_PROT_MDM_s) ) {
					/* analogue call accepted by a digital modem */
					P->Mode = P_MODE_MODEM ;
					Prot = ISDN_PROT_MDM_a ;
				}
			}

			/*--- switch from prot fax to prot detection, since fax is invalid -*/
			if (Prot == ISDN_PROT_FAX)
			{	Prot    = ISDN_PROT_DET;
				P->Mode = P_MODE_MODEM ;
			}
			/*--- switch from mode fax to a mode, which fit with cur prot -----*/
			if (P->Mode == P_MODE_FAX)
			{	P->Mode = P_MODE_MODEM;                   // default mode
				for( u= 0; u < MAX_PROFILE; u++ )
				{	if( Profiles[u].Prot == Prot )
					{	P->Mode = Profiles[u].Mode;       // a better match
						break;
					}
				}
			}

			/*--- use outbound detected prot if prot detection is configered ---*/
			if (Prot == ISDN_PROT_DET)
				Prot = P->CallbackParms.Prot;
			break ;
		}

		Parms.Prot      = Prot;
		Parms.MaxFrame  = P->At.MaxFrame;
		if (Parms.Baud == 255)
			Parms.Baud = P->At.f.Baud;
		Parms.bit_t_detection = P->At.f.BitTDetect;
		Parms.modem_options = P->At.f.modem_options;

		atAdjustCall (P, &P->At, &Parms.Prot, Parms.Baud);

		if (PortDriver.Isdn->Answer (P, P->Channel,
					     P->At.MaxFrame,
					     (P->Mode == P_MODE_FAX) ?
					     (T30_INFO*)faxConnect (P) : (T30_INFO *)0, &Parms)) {
			P->State	= P_ANSWERED;
			P->StateTimeout	= P_TO_ANSWERED(PortDriver.Ticks);
			DBG_TRC(("[%p:%s] ANSWER: D=%s O=%s M=%d P=%d S=%x/%x B=%x pend",
			  P->Channel, P->Name, Parms.Dest, Parms.Orig, P->Mode, Parms.Prot,
			  Parms.Service, Parms.ServiceAdd, Parms.Baud))
			/* DBG_TRC(("[%p:%s] ANSWER: M=%d P=%d pend\n",
				  P->Channel, P->Name, P->Mode, (P->Mode == P_MODE_FAX) ?
					  	    ISDN_PROT_FAX : P->At.Prot)) */
			return R_VOID;
		}
		atShut (P, CAUSE_SHUT);
	}

	DBG_TRC(("A: [%s] ANSWER: fail\n", P->Name))

	P->State	= P_COMMAND;
	P->StateTimeout	= P_TO_NEVER;

	return R_ERROR;
}

static void _cdecl atAutoAnswer (ISDN_PORT *P)
{
	(void) atAnswer (P);
}

int atDial (ISDN_PORT *P, byte **Arg)
{
	static AT_DATA	At;		/* dial out configuration */
	byte	*Number;	/* begin of number	  */
	int	sizeNumber;	/* and its size		  */
	byte			*SubAddr;		/* begin of subaddress	  */
	int				sizeSubAddr;	/* and its size			  */
	byte	*Next;		/* next call parm or cmd  */
	static ISDN_CONN_PARMS	Parms ;			/* all dial parameters	  */

	if (P->State != P_COMMAND) {
		DBG_TRC(("A: [%s] CALL: %s ?\n", P->Name, atState (P->State)))
		return R_ERROR;
	}


	Next	= *Arg;
	At	= P->At;	/* copy configuration for temporary changes */

	if (*Next == ';') {
		/* clear remembered number */
		P->At.Dest[0] = 0;
		return R_OK;
	}

	/* check number, set 'Next' past end of number and	*/
	/* modify initial configuration according to dial parms	*/

	Number = Next;
	sizeSubAddr = 0 ;
	SubAddr = "";


	if ((sizeNumber = atIsPhone (&Next, 1)) <= 0) goto badArgs;

	while (*Next == '|') {
		/* ISDN subaddresses (see TAPI 1.4 spec), skip */
		for (SubAddr = ++Next; *Next && !str_chr ("^|+;", *Next) ; Next++)
			;
		sizeSubAddr = Next - SubAddr ;
		if (sizeSubAddr >= (ISDN_MAX_SUBADDR-1))
			sizeSubAddr = 0 ;	/* forget it */

	}

	while (*Next == '^') {
		/* ISDN name (see TAPI 1.4 spec) */
		Next++;
		if (Next[0] == '5' && Next[1] == '6' && Next[2] == 'k') {
			At.f.Baud = 9;
			Next += 3;
		} else if (Next[0] == 's' && Next[1] == 'p') {
			/* semipermanent connection (ignored) */
			Next += 2;
		}
	}

	while (*Next == '+') {
		if (*++Next == 'i') {
			/* look for our ISDN settings */
			if (*++Next == 0 || atPlusI (P, &At, &Next) != R_OK)
		    		goto badArgs;
		} else {
			/* may be the AMARIS - ACOTEC ISDN BTX protocol hack */
			if (!mem_cmp (Next, "p=btx", sizeof ("p=btx") - 1)) {
				atProfile (&At, FRAME_MODE_PROFILE);
				atNewMode (P, &At);
				Next += sizeof ("p=btx") - 1;
			} else {
				for ( ; *Next && *Next != '+'; Next++)
					; /* skip unknown stuff */
			}
		}
	}

	if (*Next == ';') {
		/* stay in command mode after dialing */
		At.f.Data = 0;
	} else if (*Next) {
		/* no commands permitted after dialstring */
		goto badArgs;
	}

	*Arg = Next;

	if (At.f.Progress == 3) {
		At.Dest[0] = 'x';
		At.Dest[1] =  0;
		atMkPhone (&At.Dest[1], Number, sizeNumber, 1);
	} else {
		atMkPhone (At.Dest, Number, sizeNumber, 1);
	}

	if ( At.f.Id == GENERIC_SERVER_PROFILE )
	{
#if 0 /* WINNT */
		if ( !(PortDriver.Fixes & P_FIX_SRV_CALLBACK_ENABLED) )
		{
			DBG_TRC(("A: [%s] CALL: inbound only !", P->Name))
			return R_ERROR ;
		}

		if ( At.f.Prot == ISDN_PROT_DET )
		{
			LARGE_INTEGER Now ;
 			KeQuerySystemTime (&Now) ;
			if ( (P->CallbackValid == 0)
			  || (Now.QuadPart >
			      P->LastDisc.QuadPart +
			      Int32x32To64 (PortDriver.CallbackTimeout * 1000L, 10000L)) )
			{
				DBG_TRC(("A: [%s] CALL: no matching inbound call !", P->Name))
				P->CallbackValid = 0 ;
				return R_ERROR ;
			}

			/* prepare call with parameters of inbound call */

			Parms = P->CallbackParms ;
			P->Mode  = P_MODE_MODEM ;
			P->Class = P_CLASS_DATA ;

			if ( (PortDriver.Fixes & P_FIX_SRV_SYNCPPP_ENABLED)
			  && (P->CallbackParms.Prot == ISDN_PROT_RAW)
			  && (P->CallbackParms.Mode == ISDN_CONN_MODE_SYNCPPP) )
			{
				P->Mode = P_MODE_RNA ;
				At.f.RnaFraming = P_FRAME_SYNC ;
				At.RnaPatches	= 0 ;
			}

			if (Parms.Baud == 255)
				Parms.Baud = P->At.f.Baud;

			atAdjustCall (P, &At, &Parm.Prot, Parm.Baud) ;

			Parms.PlanDest = At.f.PlanDest ;
			str_cpy (&Parms.Dest[0], &At.Dest[0]) ;

			Parms.DestSub[0] = 0 ;
			if (sizeSubAddr) {
				Parms.DestSub[0] = 0x50;
				mem_cpy (&Parms.DestSub[1], SubAddr, sizeSubAddr) ;
				Parms.DestSub[sizeSubAddr+1] = 0 ;
			}

			Parms.PlanOrig = At.f.PlanOrig ;
			Parms.Presentation_and_Screening = At.f.Presentation_and_Screening;
			str_cpy (&Parms.Orig[0], &At.Orig[0]) ;
			mem_cpy (&Parms.OrigSub[0],
							&At->f.user_orig_subadr[0],
							At->f.user_orig_subadr[0]+1);

			goto Dial ;	/* sorry for the goto, but's the simplest way for now */
		}
#endif /* WINNT */
	}

	P->FlowOpts = P->At.f.FlowOpts;
	switch (P->Class) {
	case P_CLASS_FAX1:
	case P_CLASS_FAX10:
	case P_CLASS_FAX2:
	case P_CLASS_FAX20:
	case P_CLASS_FAX21:
		P->Mode	  = P_MODE_FAX;
		At.f.Prot = ISDN_PROT_FAX;
		if (At.f.Service > 2) {
			At.f.Service = 1; At.f.ServiceAdd = 2;
		}
		P->FlowOpts = ((P->At.f.FlowOpts == AT_FLOWOPTS_BOTH) ||
		               (P->At.f.FlowOpts == AT_FLOWOPTS_RTSCTS)) ?
		                AT_FLOWOPTS_BOTH : AT_FLOWOPTS_XONXOFF;
		break;
	case P_CLASS_VOICE:
		P->Mode	  = P_MODE_VOICE;
		At.f.Prot = ISDN_PROT_VOICE;
		if (At.f.Service > 1) {
			At.f.Service = 1; At.f.ServiceAdd = 0;
		}
		break;
	case P_CLASS_DATA:
	default:
		break;
	}

	atAdjustCall (P, &At, &At.f.Prot, At.f.Baud);

	mem_zero (&Parms, sizeof(Parms)) ;

	Parms.MaxFrame	 = At.MaxFrame ;
	Parms.PlanDest	 = At.f.PlanDest ;
	str_cpy (&Parms.Dest[0], &At.Dest[0]) ;

	Parms.DestSub[0] = 0 ;
	if (sizeSubAddr) {
		Parms.DestSub[0] = 0x50;
		mem_cpy (&Parms.DestSub[1], SubAddr, sizeSubAddr) ;
		Parms.DestSub[sizeSubAddr+1] = 0 ;
	}

	Parms.PlanOrig	 = At.f.PlanOrig ;
	Parms.Presentation_and_Screening = At.f.Presentation_and_Screening;
	str_cpy (&Parms.Orig[0], &At.Orig[0]) ;
	mem_cpy (&Parms.OrigSub[0],
					&At.f.user_orig_subadr[0],
					At.f.user_orig_subadr[0]+1);

	Parms.PlanOrig_1 = At.PlanOrig_1;
	if ((Parms.PlanOrig_1 & 0x80) == 0)
		Parms.PresentationAndScreening_1 = At.PresentationAndScreening_1 | 0x80;
	else
		Parms.PresentationAndScreening_1 = 0;

	str_cpy (&Parms.Orig_1[0], &At.Orig_1[0]);

	Parms.Service	 = At.f.Service ;
	Parms.ServiceAdd = At.f.ServiceAdd ;
	Parms.Prot		 = At.f.Prot ;
	Parms.Baud		 = At.f.Baud ;

	if ((Parms.Line = At.f.o_line)) {
		Parms.selected_channel = At.f.o_channel;
	} else {
		Parms.selected_channel = 0;
	}

	if (At.f.user_bc[0]) {
		mem_cpy  (Parms.Bc, At.f.user_bc, sizeof(Parms.Bc));
		if (At.f.user_llc[0]) {
			mem_cpy  (Parms.Llc, At.f.user_llc, sizeof(Parms.Llc));
		}
	}

	/* set up call via IsdnDial()	*/

/* Dial: */

	P->State	= P_DIALING;
	P->StateTimeout	= P_TO_DIALING(PortDriver.Ticks);
	P->Direction	= P_CONN_OUT;
	P->LastUsed	= ++(PortDriver.Unique) ;

	Parms.modem_options = P->At.f.modem_options;
	memcpy (&P->last_parms, &Parms, sizeof(ISDN_CONN_PARMS));

	P->Channel = PortDriver.Isdn->Dial (P, &Parms,
										(P->Mode == P_MODE_FAX) ?
										 faxConnect (P) : (T30_INFO *)0) ;

	if ( !P->Channel )
	{
		DBG_TRC(("A: [%s] DIAL ERROR, No channel found!", P->Name))
		/* call setup failed */
		P->State		= P_COMMAND;
		P->StateTimeout	= P_TO_NEVER;
		P->Direction	= P_CONN_NONE;
		return (atDialError (Parms.Cause));


	}

	/* remember last dialed destination */

	str_cpy ((char*)P->At.Dest, (char*)At.Dest);
  P->LastAlertTick = 0; /* Reset call establishement timer */

	/* respond when connected or call failed */

	return R_VOID;

badArgs:
	DBG_TRC(("A: [%s] CALL: bad args %s\n", P->Name, Number))
	return R_ERROR;
}

int atReset (ISDN_PORT *P, byte **Arg)
{
	int Val;

	/* DBG_TRC(("[%s] RESET:\n", P->Name)) */

	if ((Val = atNumArg (Arg)) < 0)
		Val = P->At.f.Id;
	else if (Val == 0)
		Val = 1;
	else if (Val > MAX_PROFILE)
		return R_ERROR;

	atShut (P, CAUSE_DROP);

	/* reinitialize whith the requested profile */

	atSetup (P, Val /*Profile Id*/);

	return R_OK;
}

int atHangup (ISDN_PORT *P, byte **Arg) {
	switch (atNumArg (Arg)) {
		case -1: /* no arg = 0 */
		case  0: /* go on hook */
			sysCancelDpc (&P->AutoAnswerDpc);
			sysCancelTimer (&P->EscTimer);
			if ((P->At.f.S254 & S254_ATH_IN_RING) ||
					(P->State != P_OFFERED)) {
				atShut (P, CAUSE_DROP);
			}
		case  1:
		case  2: /* go off hook */
			return R_OK;
	}
	return R_ERROR;
}

static int atOnline (ISDN_PORT *P, byte **Arg)
{
	/* DBG_TRC(("[%s] ONLINE:\n", P->Name)) */

	switch (atNumArg (Arg)) {
	case -1: /* no arg */ case 0: case 1: /* known args */
		if (P->State == P_CONNECTED || P->State == P_ESCAPE)
		{
			P->State = P_DATA;
			switch (P->Mode) {
			case P_MODE_FAX  :
				return (fax1Online (P));
			}
			return R_OK;
		}
	}
	return R_ERROR;
}

static int atCmd (ISDN_PORT *P)
{ /* evaluate the single commands of a complete line saved in P->At.Last */

	byte *p, Cmd; int Rc, Val;
    byte Enq[20] = "\r\n";
    byte *RxVal;

	/* DBG_TRC(("[%s] atCmd: '%s'\n", P->Name, P->At.Line)) */

	for (p = P->At.Last, Rc = R_OK; (Rc == R_OK || Rc == R_VOID) && *p; ) {
		switch (Cmd = *p++) {
		case 'a' : /* answer incoming call */
			   return (atAnswer (P));

		case 'b' : /* B[0|1] - BELL modulation mode */
				P->At.f.modem_options.bell = (atNumArg (&p) <= 0) ? 1 : 0;
				break;

		case 'd' : /* dial */
			   Rc = atDial (P, &p);
			   break;
		case 'e' : /* echo mode */
			   P->At.f.Echo = (atNumArg (&p) <= 0)? 0 : 1;
			   break;
		case 'i' : /* identification */
			   (void) atNumArg (&p);
               RxVal = Enq;
			   atRspFlush (P);
			   portRxEnqueue (P, RxVal, 2, 0);
			   portRxEnqueue (P, PortDriver.ModemID,
			   		 (word) str_len ((char*)PortDriver.ModemID),0);

			   portRxEnqueue (P, RxVal, 2, 0);
			   break;
		case 'h' : /* hangup */
			   Rc = atHangup (P, &p);
			   break;
		case 'o' : /* online */
			   return (atOnline (P, &p));
		case 'p' : /* pulse dialing (ignored) */
			   break;
		case 'q' : /* quiet operation */
			   P->At.f.Quiet = (atNumArg (&p) <= 0)? 0 : 1;
			   break;
		case 's' : /* set register number  */
		case '=' : /* set register value   */
		case '?' : /* query register value */
			   Rc = atRegs (P, &p, Cmd);
			   break;
		case 't' : /* tone dialing (ignored) */
			   break;
		case 'v' : /* response verbosity */
			   P->At.f.Verbose = (atNumArg (&p) <= 0)? 0 : 1;
			   break;
		case 'x' : /* call progress options */
			   if ((unsigned) (Val = atNumArg(&p)) > 4)
			   	Rc = R_ERROR;
			   else
			   	P->At.f.Progress = (byte) Val;
			   break;
		case 'z' : /* soft reset */
			   return (atReset (P, &p));
		case '&' : /* special options */
			   Rc = atAmper (P, &p);
			   break;
		case '%' : /* special options */
			   Rc = atPercent (P, &p);
			   break;
		case '+' : /* extension command sets */
			   Rc = atPlus (P, &p);
			   break;
		case '\\': /* error control */
			   Rc = atBack (P, &p);
			   break;
		case '#' : /* caller ID */
			   Rc = atHash (P, &p);
			   break;
		case ';' : /* command delimiter */
			   break;
		case '$': /* FC commands */
				 Rc = atDollar(P, &p);
				 break;
		/* IGNORED commands */
		case 'l' : /* speaker volume level options	*/
		case 'm' : /* speaker on/off options		*/
		case 'n' : /* negotiation of handshake options	*/
		case 'y' : /* long space disconnect options	*/
			   (void) atNumArg (&p);
			   break;
		default  : /* unknown command */
			   DBG_TRC(("A: [%s] atCmd: bad cmd", P->Name))
			   return R_ERROR;
		}
	}

	return Rc;
}

void atWrite (ISDN_PORT *P, byte *Data, word sizeData)
{ /* gather characters in line buffer, handle conmands if line is complete */

	byte oldReg_0 = 0;
	
	if (!sizeData || sizeData > 64) {
		return;
	}

	DBG_TRC(("[%p:%s] atWrite(%d)", P->Channel, P->Name, sizeData))
	DBG_BLK((Data, sizeData))

	/* seems it was not a good idea to stop ring indications here,	*/
	/* a lot af COM applications send a bulk of commands after the	*/
	/* first RING and expect then another RING before they answer. 	*/
	/* atRingOff (P) ; // better not here				*/

	atRspFlush (P) ;	/* flush pending responses */
	if (!P->control_operation_mode) {
		if ( P->At.f.Echo )
			portRxEnqueue (P, Data, sizeData, 0) ;
	}

	switch ( atScan (P, Data, sizeData) )
	{
	default: /* should better never happen	*/
	case AT_SCAN_ERROR:	/* bad line or line too long	*/
		atRsp (P, R_ERROR) ;
		return ;
	case AT_SCAN_MORE:	/* line incomplete, no trouble */
		return ;
	case AT_SCAN_DOLAST:	/* "A/" -> repeat last line	*/
	case AT_SCAN_DOTHIS:	/* "AT...\r" -> complete line	*/
		break ;
	}

	/* check the commands, if a command changed Register 0	*/
	/* we set or clear the listen mode if necessary		*/

	oldReg_0 = P->At.f.Regs[0];
 
	P->control_response = atCmd(P);
	atRsp (P, P->control_response);
	switch (P->control_response) {
		case R_OK:
		case R_VOID:
			P->control_response = 0;
			break;
		default:
			P->control_response = -1;
	}

# if LISTEN_SELECTIVELY
	if ((oldReg_0 == 0) ^ (P->At.f.Regs[0] == 0))
		PortDriver.Isdn->Listen (!P->At.f.Regs[0]);
# endif /* LISTEN_SELECTIVELY */
}

static void _cdecl atEscTimer (ISDN_PORT *P)
{
	dword curEscCount;

	curEscCount = P->EscCount;
	P->EscCount = 0;

	P->At.stateLine = AT_INIT ;

	if ((P->State == P_DATA) &&
			((P->At.f.S254 & S254_ENABLE_TIES) || (curEscCount == 3))) {
		/* saw the escape sequence but nothing more,	*/
		/* send OK result code and set escape state.	*/
		/* DBG_TRC(("[%p:%s] atEsc: OK\n", P->Channel, P->Name)) */
		P->State = P_ESCAPE;
		atRsp (P, R_OK);
	} else {
		/* escape sequence or command incomplete 	*/
		/* DBG_TRC(("[%p:%s] atEsc: TO\n", P->Channel, P->Name)) */
	}
}

int atEscape (ISDN_PORT *P, byte *Data, word sizeData)
{
	dword lastEscTime = P->EscTime; word Cnt; int Rc; AT_CONFIG f;
	P->EscTime  = sysLastUpdatedTimerTick();

	if (!sizeData) return (0);

	if (P->At.f.Regs[2] == 127) {
		return (0);
	}


	if (P->At.f.S254 & S254_ENABLE_TIES) {
		sysCancelTimer(&P->EscTimer);
		if ((sizeData == 6) && (Data[0] == '+') && (Data[1] == '+') && (Data[2] == '+') &&
				(Data[3] == 'A') && (Data[4] == 'T') && (Data[5] == '\r') &&
				(((dword)(P->EscTime - lastEscTime)) >= sysGetSec()/50 + 1)) {
			sysStartTimer (&P->EscTimer, 50);
			return (0);
		}
		return (0);
	}

	if (!P->EscCount) {
	  if ((*Data != P->At.f.Regs[2]) ||
				(((dword)(P->EscTime - lastEscTime)) < 2*sysGetSec())) {
			return (0);
		}
	}

  sysCancelTimer(&P->EscTimer);

	for (; sizeData; Data++, sizeData--) {
		if (*Data == P->At.f.Regs[2]) {
	    P->EscCount++;
		} else {
			goto reset;
		}
	}
	if (P->EscCount > 3) {
		goto reset;
	}
  sysStartTimer (&P->EscTimer, 2000);
	
	return (0);


	switch ( atScan (P, Data, sizeData) )
	{
	default		  :
	case AT_SCAN_ERROR:	/* bad line or line too long	*/
		goto reset ;
	case AT_SCAN_MORE:	/* wait for more data		*/
		return (0) ;
	case AT_SCAN_DOLAST:	/* "A/" -> repeat last line	*/
		goto reset ;
	case AT_SCAN_DOTHIS:	/* "AT...\r" -> complete line	*/
		/* check if this is a pure command line */
		for ( Cnt = 0; Cnt < sizeData && Data[Cnt] != '\r'; Cnt++ )
			;
		if ( Cnt != (sizeData - 1) )
			goto reset;	/* '\r' not last or dup */
		break ;
	}

	/* Got a nice command line before timer fired,		*/
	/* reset timer and command buffer and save flags and	*/
	/* registers for error backup				*/

	atUnEscape (P);
	f = P->At.f;

	if ( (Rc = atCmd (P)) != R_OK ) {
		/* forget the bad command */
		P->At.f = f;
		return (0);
	}

	if (P->State == P_DATA)
		P->State = P_ESCAPE;
	atRsp (P, Rc);
	return (1);	/* don't put this on the wire */

reset:
	atUnEscape (P);	/* reset timer and command buffer */
	return (0);
}

/*
 * the connection oriented indication functions
 */

int portUp (void *C, void *hP, ISDN_CONN_INFO *Info)
{
/* call completed, enter data mode  */

	ISDN_PORT *P;
	int	  Respond;

	if (!(P = (ISDN_PORT *) hP) || P->Channel != C) {
		DBG_TRC(("A: <<UP>> link ?"))
		if (!P) {
			DBG_ERR(("A: [%p:?] UP NoPORT", C))
		} else {
			DBG_ERR(("A: [%p:%s] UP NoChannel=%p", C, P->Name, P->Channel))
		}
		return (0);
	}
	
	P->rx_queued_in_event_queue  = 0;

	if ( Info )
	{
		mem_cpy (&P->last_conn_info, Info, sizeof(*Info));
		P->last_conn_info_valid			 = 1;

		P->At.Speed = (Info->RxSpeed > Info->TxSpeed) ?
					   Info->RxSpeed : Info->TxSpeed  ;

		if ( Info->MaxFrame && Info->MaxFrame < P->Split )
			P->Split = Info->MaxFrame ;

		if ( Info->Prot == ISDN_PROT_MDM_s || Info->Prot == ISDN_PROT_MDM_a )
		{
			/* DBG_TRC(("[%s] <<UP>> %s --> %s - %s/%s/%d:TX/%d:RX\n",
					  P->Name, atState (P->State), atIsdnProtName (Info->Prot),
					  atModemConNormName (Info->ConNorm),
					  atModemConOptsName (Info->ConOpts),
					  (unsigned int)Info->TxSpeed, (unsigned int)Info->RxSpeed)) */
		}
		else
		{
			/* DBG_TRC(("[%s] <<UP>> %s --> %s - %s%d\n",
					  P->Name, atState (P->State), atIsdnProtName (Info->Prot),
					  (   Info->Prot == ISDN_PROT_RAW
					   && Info->Mode == ISDN_CONN_MODE_SYNCPPP)? "SYNCPPP ":"",
					   (unsigned int)P->At.Speed)) */
		}
	}
	else
	{
		/* DBG_TRC(("[%s] <<UP>> %s --> %s - %d\n",
				  P->Name, atState (P->State), atIsdnProtName (P->At.Prot),
				  (unsigned int)P->At.Speed)) */
		P->last_conn_info.Prot    = P->At.Prot;
		P->last_conn_info.RxSpeed = P->At.Speed;
		P->last_conn_info.TxSpeed = P->At.Speed;
		P->last_conn_info.ConOpts = 0;
		P->last_conn_info.ConNorm = 0;
		P->last_conn_info_valid		= 1;
	}


	switch (P->State) {

	case P_CONNECTED:
	case P_DATA	:
	case P_ESCAPE	:
		return (1); /* ignore 2nd up */

	case P_OFFERED	:
	case P_DISCONN	:
		P->State = P_COMMAND;
	case P_COMMAND	:
	case P_IDLE	:
	default		:	/* what's that ? */
		P->Channel = 0;
		return (0);	/* forget it */

	case P_DIALING:
#if defined(WINNT) || defined(UNIX)
		P->CallbackValid = 0 ;
#endif /* WINNT */
		P->CadValid      = 0 ;
		break;

	case P_ANSWERED:
		P->CadValid      = 1;
	if ( (Info) && (P->At.f.Id == GENERIC_SERVER_PROFILE) )
		{
#if defined(WINNT) || defined(UNIX)
			P->CallbackValid = 1 ;
			P->CallbackParms.MaxFrame = Info->MaxFrame ;
			P->CallbackParms.Prot	  = Info->Prot ;
			P->CallbackParms.Mode	  = Info->Mode ;
#endif /* WINNT */
			if ( (PortDriver.Fixes & P_FIX_SRV_SYNCPPP_ENABLED)
			  && (Info->Prot == ISDN_PROT_RAW)
			  && (Info->Mode == ISDN_CONN_MODE_SYNCPPP) )
			{
				P->Mode = P_MODE_RNA ;
				P->port_u.Rna.Framing = P_FRAME_SYNC ;
				P->port_u.Rna.Patches = 0 ;
			}
		}
	}

	P->State	= P->At.Data ? P_DATA : P_CONNECTED;
	P->StateTimeout	= P_TO_NEVER;
	P->LastRwTick	= PortDriver.Ticks;

	switch (P->Mode) {
	case P_MODE_RNA  :
		Respond = rnaUp (P);
		break;
	case P_MODE_BTX  :
		Respond = btxUp (P);
		break;
	case P_MODE_FAX  :
		Respond = faxUp (P);
		break;
	case P_MODE_FRAME:
		/* clear and switch input queue */
		queueReset (&P->RxQueue);
		portInitQIn (P, P->port_u.Frm.Stream, sizeof(P->port_u.Frm.Stream));
		/* fall through */
	default		 :
		Respond = 1;
		break;
	}

	portSetMsr (P, MS_RLSD_ON, MS_RLSD_ON/*on*/);

	if (Respond) {
		/* atRsp() calls portRxEnq() which in turn calls */
		/* the portRxNotify() and portEvNotify() funcs	 */
		atRspConn (P, Info) ;
	} else {
		portEvNotify (P);
	}

	return (1);
}

int portDown (void *C, void *hP, byte Cause, byte q931_cause)
{
/* call or connection down */

	ISDN_PORT *P;
	int  LostCarrier = 1 ;

	if (!(P = (ISDN_PORT *) hP) || P->Channel != C) {
		DBG_TRC(("A: <<DOWN>> link?"))
		if (!P) {
			DBG_ERR(("A: [%p:%s] DOWN, NoPORT", C, "?"))
		} else {
			DBG_TRC(("A: [%p:%s] DOWN, NoChannel=%p", C, P->Name, P->Channel))
		}
		return 0;	/* forget this channel */
	}
	P->rx_queued_in_event_queue  = 0;

	P->At.f.S253 = q931_cause & 0x7f;

	DBG_TRC(("[%p:%s] <<DOWN>> %s\n", C, P->Name, atState (P->State)))

	switch (P->State) {

	case P_COMMAND	:
	case P_IDLE	:
	default		:
		P->Channel = 0;
		return (0);	/* forget it */

	case P_DIALING	:
	
	/* 
  	Adapter walk extension for e.g. SS7 
  	In case a channel is already busy with something like 
  	a SS7 signalling link, advance to the next controller.
  	This is necessary to support > 30 channels using SS7
	*/

	/* if user selected line/channel don't do anything */
	if ( (P->At.f.o_line == 0) && (P->At.f.o_channel == 0) && (Cause == CAUSE_UNREACHABLE)) 
	{
		if (P->last_parms.Line==0) P->last_parms.Line=1;
		while ( P->last_parms.Line < isdn_get_num_adapters() ) 
		{
			P->last_parms.Line++;
			DBG_TRC(("A: [%s] Adapterwalk: dial next adapter %d \n", P->Name, P->last_parms.Line))
			P->Channel = PortDriver.Isdn->Dial (P, &P->last_parms,
								(P->Mode == P_MODE_FAX) ?
								faxConnect (P) : (T30_INFO *)0);
			if ( P->Channel ) {
				return(0);
			}
		} 
		DBG_TRC(("A: [%s] Adapterwalk: DIAL ERROR, No channel found!", P->Name))
		/* call setup failed */
		P->State                = P_COMMAND;
		P->StateTimeout = P_TO_NEVER;
		P->Direction    = P_CONN_NONE;
	}
	case P_ANSWERED	:
		atRsp (P, atDialError (Cause));
		break;

	case P_DATA	:
	case P_ESCAPE	:
	case P_CONNECTED:
		if (P->Mode == P_MODE_FAX) LostCarrier = faxHangup (P);
		break;

	case P_OFFERED	:
		if ((global_options & DIVA_ISDN_AT_RSP_IF_RINGING) != 0 &&
				(*P->pMsrShadow & MS_RLSD_ON) == 0 && P->Mode  != P_MODE_FAX) {
			atRsp (P, R_NO_CARRIER);
		}
		break;

	case P_DISCONN	:
		break;
	}

	atLinkDown (P,
							LostCarrier,
							((divas_tty_wait_sig_disc == 0) || (atDialError (Cause) == R_ERROR)) ?
																	R_NO_CARRIER : atDialError(Cause));

	return (0);	/* forget channel, don't wait for Idle from ISDN */
}

void portIdle (void *C, void *hP)
{
/*
 * Disconnect complete, connected channel idle now.
 *
 * Currently this function should not be called cause we don't request
 * to be called again after a 'portDown' indication (see above) or when
 * we drop a connection via 'Isdn->Drop'.
 */
}

static void _cdecl atRingTimer (ISDN_PORT *P)
{
	atRing (P);
}

static void atRing (ISDN_PORT *P)
{
	dword delay;

	sysCancelTimer (&P->RingTimer);	/* just to be sure, should not pend */

	if (*P->pMsrShadow & MS_RING_ON) {
		portSetMsr (P, MS_RING_ON, 0/*off*/);
		portEvNotify (P);
		delay = PortDriver.RingOff;
	} else {
		portSetMsr (P, MS_RING_ON, MS_RING_ON/*on*/);
		/* atRsp() calls portRxEnq() which in turn calls */
		/* the portRxNotify() and portEvNotify() funcs	 */
		atRsp (P, R_RING);
		delay = PortDriver.RingOn;
	}

	if (P->State != P_OFFERED) return;

	sysStartTimer (&P->RingTimer, delay);
}

void *portRing (void *C, ISDN_CONN_PARMS *Parms)
{
	ISDN_PORT_DESC	*D;
	ISDN_PORT	*P, *bestP, *anyP;
	int		sizeDest, sizeAcpt, Want;
	int pass;

	/* see if we want this call */

	sizeDest = str_len (Parms->Dest);

	/*
		Two passes detection procedure:
		First pass:  Using BC/LLC
		Second pass: Using Service
		*/
	for (pass = 0, anyP = 0, bestP = 0, Want = 0; pass < 2; pass++) {
		if (anyP != 0 || bestP != 0) {
			break;
		}
		for (Want = 0, bestP = anyP = 0, D = PortDriver.Ports; D < PortDriver.lastPort; D++) {
			if (((P = D->Port) == 0) || P->State == P_IDLE || P->At.f.Regs[0] == 255 /*never answer*/) {
				continue;
			}
			if (P->At.f.i_line && Parms->Line) {
				if (P->At.f.i_line != Parms->Line)
					continue;
				if (P->At.f.i_channel && (P->At.f.i_channel != Parms->selected_channel))
					continue;
			}
			if (pass == 0) {
				if (P->At.f.user_bc[0] == 0 && P->At.f.user_llc[0] == 0)
					continue;
				if (compare_struct (&P->At.f.user_bc[0],  &Parms->Bc[0]) ||
						compare_struct (&P->At.f.user_llc[0], &Parms->Llc[0]))
					continue;
			} else {
				if (P->At.f.user_bc[0] != 0 || P->At.f.user_llc[0] != 0)
					continue;

				if ( P->At.f.Service <= 2 ) {
					/*
						expecting analogue service (modem/fax/voice)
						more compatibility checking needed here.
						*/
					if (Parms->Service > 2)
						continue ;
				} else if (!(PortDriver.Fixes & P_FIX_ACCEPT_ANALOG_CALLS)) {
					/*
						expecting digital service only
						*/
					if (Parms->Service <= 2)
						continue;
				}
			}

			if (sizeDest && (sizeAcpt = str_len ((char*)P->At.Acpt))) {
				if ((sizeDest - sizeAcpt) < 0 ||
						mem_cmp (Parms->Dest + (sizeDest - sizeAcpt), P->At.Acpt, sizeAcpt)) {
					;	/* explicitely not wanted */
				} else if (P->State != P_COMMAND) {
					Want++;	/* wanted but busy */
				} else if (!bestP || sizeAcpt > (int) str_len ((char*)bestP->At.Acpt)) {
					bestP = P; /* this is the best one so far */
				}
				continue;
			}

			if (P->State != P_COMMAND) {
				Want++; /* wanted but busy */
				continue;
			}

			if (bestP != 0)
				continue ;	/* look for an even better one */

			if (anyP == 0) {
				anyP = P;
				continue;
			}
			if (pass != 0) {
				if (Parms->Service <= 2 && P->At.f.Service <= 2 && anyP->At.f.Service > 2) {
					anyP = P;
					continue;
				}
			}
			if ((PortDriver.Fixes & P_FIX_ROUND_ROBIN_RINGS) && (P->LastUsed < anyP->LastUsed)) {
				anyP = P;
				continue ;
			}
		}
	}

	if (!(P = bestP) && !(P = anyP)) {
		/* DBG_TRC(("<<RING>> %s D=%s O=%s S=%x/%x\n",
				  Want ? "target busy" : "no target",
				  Parms->Dest, Parms->Orig, Parms->Service, Parms->ServiceAdd)) */
		Parms->Cause = Want? CAUSE_AGAIN : CAUSE_IGNORE;
		return (0);
	}

	/* wanted and to be serviced */

	P->CallbackParms = *Parms ;

	DBG_TRC(("[%p:%s] <<RING>> D=%s O=%s M=%d P=%d S=%x/%x B=%x",
			  C, P->Name, Parms->Dest, Parms->Orig, P->Mode, Parms->Prot,
			  Parms->Service, Parms->ServiceAdd, Parms->Baud))

	P->Channel	= C;
	P->State	= P_OFFERED;
	P->StateTimeout	= P_TO_OFFERED(PortDriver.Ticks);
	P->Direction	= P_CONN_IN;
	P->LastUsed	= ++(PortDriver.Unique) ;

	/*warning str_cpy (P->At.Ring, Orig);*/
	if (Parms->Orig_1[0] == 0) {
		str_cpy ((char*)P->At.Ring, (char *)Parms->Orig);
	} else {
		sprintf ((char*)P->At.Ring, "%s(%s)", Parms->Orig, Parms->Orig_1);
	}
	str_cpy ((char*)P->At.RingName, (char *)Parms->OrigName);

	if (P->At.f.Regs[0] == 0) {
		/* set MsrShadow, generate RING response and notify */
		atRing (P);
	} else {
#if defined(UNIX)
/* 
 * Should actually provide S0 rings before answering. Lets just provide one
 * at least for now.
 */
		atRsp (P, R_RING);
#endif
		/* schedule automatic answer */
	  sysScheduleDpc (&P->AutoAnswerDpc);
	}

	return ((void *) P);
}

#if defined(UNIX)
void atInit (ISDN_PORT *P, int NewPort, byte *Address, int Profile)
#else
void atInit (ISDN_PORT *P, int NewPort)
#endif
{
	/* adjust default profile (not worth an extra check function)	*/

	if (PortDriver.Profile > MAX_PROFILE) PortDriver.Profile = 1;

	/* initialize the variables mainly used by the AT processor	*/

	P->EscCount = 0;
	P->EscTime  = 0;
	sysInitializeTimer (&P->EscTimer, (sys_CALLBACK)atEscTimer, P);
	sysInitializeTimer (&P->RspTimer, (sys_CALLBACK)atRspTimer, P);
	queueInit (&P->RspQueue, P->RspBuffer, sizeof(P->RspBuffer));
	sysInitializeTimer (&P->RingTimer, (sys_CALLBACK)atRingTimer, P);
	sysInitializeDpc (&P->AutoAnswerDpc, (sys_CALLBACK)atAutoAnswer, P);

	/* for a new port instance initialize the AT processor state	*/

	if (NewPort) atSetup (P, (
				  Profile ) );

#if defined(UNIX)
	{
		int Val = 0;
		byte *p;
	
		p = Address;

		if ((Val = atIsPhone (&p, 0)) <= 0) 
		{
			return;
		}
		else
		{
			atMkPhone (P->At.Acpt, Address, Val, 0);
		}
	}
#endif
}

void atFinit (ISDN_PORT *P, int Detach)
{
	faxFinit (P);
	atRspPurge (P);
	atUnTimeout (P);
	if (Detach && P->Channel)
		(void) PortDriver.Isdn->Drop (P, P->Channel, CAUSE_SHUT, 1,
																	(P->At.f.S253 & 0x80) ? (P->At.f.S253 & 0x7f) : 0);
# if LISTEN_SELECTIVELY
	if (P->At.f.Regs[0] == 0) (void) PortDriver.Isdn->Listen (0);
# endif /* LISTEN_SELECTIVELY */
}

int atScan (ISDN_PORT *P, byte *Data, word sizeData)
{ /* gather characters in line buffer until line is complete	*/

	for ( ; sizeData-- ; Data++ )
	{
		byte Char, inString, *Line, *Last;

		Char = *Data & 0x7f ;

		/*--- lock for Xon and Xoff ----------------------------------------*/
		if( Char == 0x11 )  /* Xon */
		{
			P->RxXoffFlow = FALSE;
			continue ;
		}
		if( Char == 0x13 )  /* Xoff */
		{
			P->RxXoffFlow = TRUE;
			continue ;
		}
		/* Assume that the flow control condition ceased when */
		/* the application sends a command.                   */
		/* This should resolve any inconsistencies when FAX   */
		/* or modem data happens to be sent in command state. */
		P->RxXoffFlow = FALSE;

		switch ( P->At.stateLine )
		{
		case AT_INIT:	/* wait for 'A', skip everything else */
			P->At.sizeLine = 0 ;
			P->At.Line[0]  = 0 ;
			if ( Char == 'A' || Char == 'a' )
				P->At.stateLine = AT_LOOK ;
			continue ;
		case AT_LOOK:	/* 'A' seen, expect 'T' or '/' */
			if ( Char == 'T' || Char == 't' ) {
				P->At.stateLine = AT_SCAN ;
				continue ;
			}
			P->At.stateLine = AT_INIT;
			if ( Char != '/' ) {
				continue ;
			}
			/* "A/" -> repeat last line (if any) */
			if ( P->At.Last[0] ) {
				return (AT_SCAN_DOLAST) ;
			}
			/* DBG_TRC(("[%s] atScan: 'A/' miss cmd\n", P->Name)) */
			return (AT_SCAN_ERROR) ;
		case AT_SCAN:	/* "AT" seen, scan for '\r' */
			if ( Char == '\b' ) {
				if ( P->At.sizeLine )
					P->At.sizeLine-- ;
				continue ;
			}
			if ( Char != '\r' ) {
				/* buffer this character */
				P->At.Line[P->At.sizeLine++] = Char ;
				if ( P->At.sizeLine >= sizeof(P->At.Line) ) {
					P->At.stateLine = AT_INIT ;
					DBG_TRC(("A: [%s] atScan: line too long", P->Name))
					return (AT_SCAN_ERROR) ;
				}
				continue ;
			}
			break ;
		}

		/* "AT...\r" -> normalize this line */

		P->At.stateLine = AT_INIT ;
		P->At.Line[P->At.sizeLine] = '\0' ;

		for ( Line = P->At.Line, Last = P->At.Last, inString = 0 ; ; )
		{
			Char = *Line++ ;
			if ( Char == '\0') {
				if ( inString ) {
					DBG_TRC(("A: [%s] atScan: miss \"", P->Name))
					P->At.Last[0] = '\0' ;
					return (AT_SCAN_ERROR) ;
				}
				*Last = '\0' ;
				break ;
			}
			if ( Char == '"' ) {
				/* start or end of string */
				inString ^= 1 ;
			} else if ( inString ) {
				/* copy non control characters unchanged */
				if ( Char < ' ' )
					continue ;
			} else {
				/* skip control characters and spaces,	*/
				/* convert upper case to lower case	*/
				if ( Char <= ' ' )
					continue ;
				if ( Char >= 'A' && Char <= 'Z' )
					Char |= ' ' ;
			}
			*Last++ = Char ;
		}
		return (AT_SCAN_DOTHIS) ;
	}
	/* line incomplete, wait for more data */
	return (AT_SCAN_MORE) ;
}

static byte a2bin (int c) {
	switch (c) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			return ((byte)(c - '0'));
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
			return ((byte)(0x0a + c - 'a'));
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
			return ((byte)(0x0a + c - 'A'));
	}

	return (0xff);
}

static int string2bin (byte* dst, const byte* src, int dst_limit, int src_limit) {
	byte hi, lo;

	if ((src_limit % 2) || (dst_limit - 1 < src_limit/2)) {
		return (-1);
	}

	*dst++ = src_limit/2;

	while (src_limit) {
		if ((hi = a2bin (*src++)) == 0xff) { dst[0] = 0; return (-1); }
		if ((lo = a2bin (*src++)) == 0xff) { dst[0] = 0; return (-1); }
		*dst++ = ((hi << 4) | lo);
		src_limit -=2;
	}

	return (0);
}

void create_status_message (ISDN_PORT *P, char* out, int limit) {
	ISDN_CONN_INFO *Info = &P->last_conn_info;
	int len, Rc, Speed, Prot;


	len =  diva_snprintf (out, limit, "CID:%s%s",
									&P->At.Ring[0],
									P->CallbackParms.OrigSubLength ? "/" : "");
	len += write_ascii_address (&out[len], (limit - len),
															&P->CallbackParms.OrigSub[0],
															P->CallbackParms.OrigSubLength);
	len += diva_snprintf (&out[len], (limit - len), " DAD:%s%s",
									&P->CallbackParms.Dest[0],
									P->CallbackParms.DestSubLength ? "/" : "");
	len += write_ascii_address (&out[len], (limit - len),
															&P->CallbackParms.DestSub[0],
															P->CallbackParms.DestSubLength);
	out += len;
	limit -= len;

	if (!P->last_conn_info_valid) {
		return;
	}

	if (Info->Prot == ISDN_PROT_MDM_s	|| Info->Prot == ISDN_PROT_MDM_a) {
		diva_snprintf (out, limit, " CONNECT MODEM/%s/%s/%d:TX/%d:RX",
						   atModemConNormName (Info->ConNorm),
						   atModemConOptsName (Info->ConOpts),
						   (int)Info->TxSpeed, (int)Info->RxSpeed) ;
		return;
	}
	if (Info->Prot == ISDN_PROT_PIAFS_64K_VAR	||
			Info->Prot == ISDN_PROT_PIAFS_64K_FIX	||
			Info->Prot == ISDN_PROT_PIAFS_32K) {
		diva_snprintf (out, limit, " CONNECT PIAFS/%s/%d",
						   atPiafsConNormName (Info->ConNorm),
						   (int)Info->TxSpeed);
		return;
	}

	Speed = (int)Info->TxSpeed;
	Speed = (Speed >= (int)Info->RxSpeed) ? Speed : (int)Info->RxSpeed;
	Prot = Info->Prot;

	len = diva_snprintf (out, limit, "%s", " CONNECT ");
	out += len;
	limit -= len;

	switch (Prot) {
		case ISDN_PROT_X75:
			len = diva_snprintf (out, limit, "%s", "X75/");
			break;
		case ISDN_PROT_V110_s:
		case ISDN_PROT_V110_a:
			len = diva_snprintf (out, limit, "%s", "V110/");
			break;
		case ISDN_PROT_V120:
			len = diva_snprintf (out, limit, "%s", "V120/");
			break;
		case ISDN_PROT_FAX:
			len = diva_snprintf (out, limit, "%s", "FAX/");
			break;
		case ISDN_PROT_VOICE:
			len = diva_snprintf (out, limit, "%s", "G.711/");
			break;
		case ISDN_PROT_RAW:
			len = diva_snprintf (out, limit, "%s", "HDLC/");
			break;
		case ISDN_PROT_BTX:
			len = diva_snprintf (out, limit, "%s", "BTX/");
			break;
		case ISDN_PROT_EXT_0:
			len = diva_snprintf (out, limit, "%s", "EXT/");
			break;
		case ISDN_PROT_X75V42:
			len = diva_snprintf (out, limit, "%s", "X75/V42BIS/");
			break;

		default:
			len  = 0;
	}
	out += len;
	limit -= len;

	len  = 0;

	for ( Rc = 0 ;  ; Rc++ ) {
		if ( Rc >= sizeof(SpeedMap)/sizeof(SpeedMap[0]) ) {
			Rc = 10 ;
			Speed = 65536 ;
			break ;
		}
		if (SpeedMap[Rc] == Speed) {
			Rc += R_CONNECT ;
			break ;
		}
	}

	(void)diva_snprintf (out, limit, "%d", Speed) ;
}
 
static int atPrintCID (ISDN_PORT* P, char* Str, int in_ring, int type, const char* rsp, int limit) {
	int i;

	limit -= 10;

	switch (type) {
		case 1: /* RING CID:XXX[/YYYY] */
			if (in_ring) {
				i = diva_snprintf (Str, limit,
                     "\r\n%s CID: %s%s",
                     rsp,
                     &P->At.Ring[0],
                     P->CallbackParms.OrigSubLength ? "/" : "");
				i += write_ascii_address (&Str[i], (limit-i),
																	&P->CallbackParms.OrigSub[0],
																	P->CallbackParms.OrigSubLength);
				limit -= i;
			} else {
				i = diva_snprintf (Str, limit, "\r\n%s", rsp);
				limit -= i;
			}
			break;

		case 2: /* CONNECT ... CID:XXX[/YYYY] */
			if (!in_ring) {
				i = diva_snprintf (Str, limit,
                     "\r\n%s CID: %s%s",
                     rsp,
                     &P->At.Ring[0],
                     P->CallbackParms.OrigSubLength ? "/" : "");
				i += write_ascii_address (&Str[i], (limit-i),
																	&P->CallbackParms.OrigSub[0],
																	P->CallbackParms.OrigSubLength);
				limit -= i;
			} else {
				i = diva_snprintf (Str, limit, "\r\n%s", rsp);
				limit -= i;
			}
			break;

		case 3: /* CID: XXX[/YYY] in RING and in CONNECT messages */
			i = diva_snprintf (Str, limit,
                   "\r\n%s CID: %s%s",
                   rsp,
                   &P->At.Ring[0],
                   P->CallbackParms.OrigSubLength ? "/" : "");
			i += write_ascii_address (&Str[i], (limit-i),
																&P->CallbackParms.OrigSub[0],
																P->CallbackParms.OrigSubLength);
			limit -= i;
			break;

		case 5: /* RING CID: XXX[/YYY] DAD: HHH[/ZZZ] */
			if (in_ring) {
				i = diva_snprintf (Str, limit,
                     "\r\n%s CID: %s%s",
                     rsp,
                     &P->At.Ring[0],
                     P->CallbackParms.OrigSubLength ? "/" : "");
				i += write_ascii_address (&Str[i], (limit-i),
																	&P->CallbackParms.OrigSub[0],
																	P->CallbackParms.OrigSubLength);
				i += diva_snprintf (&Str[i], (limit-i), " DAD: %s%s",
											&P->CallbackParms.Dest[0],
											P->CallbackParms.DestSubLength ? "/" : "");
				i += write_ascii_address (&Str[i], (limit-i),
																	&P->CallbackParms.DestSub[0],
																	P->CallbackParms.DestSubLength);
				limit -= i;
			} else {
				i = diva_snprintf (Str, limit, "\r\n%s", rsp);
				limit -= i;
			}
			break;

		case 6: /* CONNECT ... CID: XXX[/YYY] DAD: HHH[/ZZZ] */
			if (!in_ring) {
				i = diva_snprintf (Str, limit,
                     "\r\n%s CID: %s%s",
                     rsp,
                     &P->At.Ring[0],
                     P->CallbackParms.OrigSubLength ? "/" : "");
				i += write_ascii_address (&Str[i], (limit - i),
																	&P->CallbackParms.OrigSub[0],
																	P->CallbackParms.OrigSubLength);
				i += diva_snprintf (&Str[i], (limit - i), " DAD: %s%s",
											&P->CallbackParms.Dest[0],
											P->CallbackParms.DestSubLength ? "/" : "");
				i += write_ascii_address (&Str[i], (limit - i),
																	&P->CallbackParms.DestSub[0],
																	P->CallbackParms.DestSubLength);
				limit -= i;
			} else {
				i = diva_snprintf (Str, limit, "\r\n%s", rsp);
				limit -= i;
			}
			break;

		case 7: /* CID: XXX[/YYY] DAD: HHH[/ZZZ]  in RING and in CONNECT */
			i = diva_snprintf (Str, limit,
                   "\r\n%s CID: %s%s",
                   rsp,
                   &P->At.Ring[0],
                   P->CallbackParms.OrigSubLength ? "/" : "");
			i += write_ascii_address (&Str[i], (limit - i),
																&P->CallbackParms.OrigSub[0],
																P->CallbackParms.OrigSubLength);
			i += diva_snprintf (&Str[i], (limit - i), " DAD: %s%s",
										&P->CallbackParms.Dest[0],
										P->CallbackParms.DestSubLength ? "/" : "");
			i += write_ascii_address (&Str[i], (limit - i),
																&P->CallbackParms.DestSub[0],
																P->CallbackParms.DestSubLength);
			limit -= i;
			break;

		case 8: /* UNIMODEM friendly format */
			if (in_ring) {
			 /*
				 RING
				 DATE = %02d%02d, Month, Day
				 TIME = %02d%02d, Hour, Minute
				 NMBR = CID or P
				 NAME = RingName
				 RING
				 */
				i = diva_snprintf (Str, limit, "\r\n%s", rsp);

				if (P->At.Ring[0] || P->CallbackParms.OrigSubLength) {
					i += diva_snprintf (&Str[i], (limit-i), "\r\nNMBR = %s%s",
												&P->At.Ring[0],
												P->CallbackParms.OrigSubLength ? "/" : "");
					i += write_ascii_address (&Str[i], (limit - i),
																		&P->CallbackParms.OrigSub[0],
																		P->CallbackParms.OrigSubLength);
				} else {
					i += diva_snprintf (&Str[i], (limit - i), "\r\nNMBR = P");
				}
				if (P->At.RingName[0]) {
					i += diva_snprintf (&Str[i], (limit - i), "\r\nNAME = %s", &P->At.RingName[0]);
				}
				i += diva_snprintf (&Str[i], (limit-i), "\r\n%s", rsp);
				limit -= i;
			} else {
				i = diva_snprintf (Str, limit, "\r\n%s", rsp);
				limit -= i;
			}
			break;

		case 9: /* RING;XXX[/YYYY] - CID */
			if (in_ring) {
				i = diva_snprintf (Str, limit, 
                     "\r\n%s;%s%s",
                     rsp,
                     &P->At.Ring[0],
                     P->CallbackParms.OrigSubLength ? "/" : "");
				i += write_ascii_address (&Str[i], (limit-i),
																	&P->CallbackParms.OrigSub[0],
																	P->CallbackParms.OrigSubLength);
				limit -= i;
			} else {
				i = diva_snprintf (Str, limit, "\r\n%s", rsp);
				limit -= i;
			}
			break;

		case 14: /* HylaFax friendly format */
			if (in_ring) {
			 /*
				 RING
				 CID: XXX[/YYY]
				 DAD: HHH[/ZZZ]
				 RING
				 */

				i = diva_snprintf (Str, limit,
                     "\r\n%s\r\nCID: %s%s",
                     rsp,
                     &P->At.Ring[0],
                     P->CallbackParms.OrigSubLength ? "/" : "");
				i += write_ascii_address (&Str[i], (limit - i),
																	&P->CallbackParms.OrigSub[0],
																	P->CallbackParms.OrigSubLength);
				i += diva_snprintf (&Str[i], (limit-i), "\r\nDAD: %s%s",
											&P->CallbackParms.Dest[0],
											P->CallbackParms.DestSubLength ? "/" : "");
				i += write_ascii_address (&Str[i], (limit - i),
																	&P->CallbackParms.DestSub[0],
																	P->CallbackParms.DestSubLength);
				i += diva_snprintf (&Str[i], (limit-i), "\r\n%s", rsp);
				limit -= i;
			} else {
				i = diva_snprintf (Str, limit, "\r\n%s", rsp);
				limit -= i;
			}
			break;

		case 15: /* RING;XXX[/YYY];HHH[/ZZZ] */
			if (in_ring) {
				i = diva_snprintf (Str, limit,
                     "\r\n%s;%s%s",
                     rsp,
                     &P->At.Ring[0],
                     P->CallbackParms.OrigSubLength ? "/" : "");
				i += write_ascii_address (&Str[i], (limit - i),
																	&P->CallbackParms.OrigSub[0],
																	P->CallbackParms.OrigSubLength);
				i += diva_snprintf (&Str[i], (limit - i),";%s%s",
											&P->CallbackParms.Dest[0],
											P->CallbackParms.DestSubLength ? "/" : "");
				i += write_ascii_address (&Str[i], (limit - i),
																	&P->CallbackParms.DestSub[0],
																	P->CallbackParms.DestSubLength);
				limit -= 1;
			} else {
				i = diva_snprintf (Str, limit, "\r\n%s", rsp);
				limit -= i;
			}
			break;

		default:
			i = diva_snprintf (Str, limit, "\r\n%s", rsp);
			limit -= i;
	}

	i += diva_snprintf (&Str[i], limit+10, "%s", "\r\n");

	return (i);
}

byte atGetProfile (byte Prot) {
	byte i;

	for (i = 0; i < MAX_PROFILE; i++) {
		if (Profiles[i].Prot ==  Prot) {
			return (i);
		}
	}

	return (0xff);
}

static int compare_struct (const byte* ref, const byte* src) {
	if (*ref == *src) {
		if (*ref == 0)
			return (0);
		return (mem_cmp ((byte*)ref+1, (byte*)src+1, *ref));
	}

	return (-1);
}

