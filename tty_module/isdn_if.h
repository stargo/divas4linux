
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

# ifndef ISDN_IF___H
# define ISDN_IF___H

/*
 * ISDN machine interface definitions
 */

/* Size limits								*/

# define ISDN_MAX_NUMBER		47		/* 46 + '\0' */
# define ISDN_MAX_SUBADDR		24		/* 23 + '\0' */
# define ISDN_MAX_BC			13		/* 12 + '\0' */
# define ISDN_MAX_LLC			19		/* 18 + '\0' */
# define ISDN_MAX_FRAME		2048
# define ISDN_MAX_V120_FRAME	256
# define ISDN_MAX_PARTY_NAME 64
# define ISDN_MAX_DISPLAY_IE 84


/* Capabilities								*/

# define ISDN_CAP_FAX3		0x0001
# define ISDN_CAP_MODEM		0x0002

/* B-channel protocols							*/

# define ISDN_PROT_DET		0	/* to be detected		*/
# define ISDN_PROT_MIN		1	/* min defined protocol 	*/
# define ISDN_PROT_X75		1	/* B1=hdlc B2=X75SLP B3=XPARENT	*/
# define ISDN_PROT_V110_s	2	/* B1=v110_s B2=B3=XPARENT	*/
# define ISDN_PROT_V110_a	3	/* B1=v110_a B2=B3=XPARENT	*/
# define ISDN_PROT_MDM_s	4	/* analogue modem (sync)	*/
# define ISDN_PROT_MDM_a	5	/* analogue modem (async)	*/
# define ISDN_PROT_V120		6	/* B1=hdlc B2=V120 B3=TA	*/
# define ISDN_PROT_FAX		7	/* fax 				*/
# define ISDN_PROT_VOICE	8	/* voice			*/
# define ISDN_PROT_RAW		9	/* B1=hdlc B2=B3=XPARENT	*/
# define ISDN_PROT_BTX		10	/* B1=hdlc B2=X75SLP B3=BTX	*/
# define ISDN_PROT_EXT_0	11	/* external device 0 		*/
# define ISDN_PROT_X75V42	12	/* B1=hdlc B2=X75V.42 B3=XPARENT */
# define ISDN_PROT_PIAFS_32K      13	/* B1=PIAFS B2=PIAFS B3=XPARENT */
# define ISDN_PROT_PIAFS_64K_FIX	14	/* B1=PIAFS B2=PIAFS B3=XPARENT */
# define ISDN_PROT_PIAFS_64K_VAR	15	/* B1=PIAFS B2=PIAFS B3=XPARENT */
# define ISDN_PROT_MAX		15	/* max defined protocol 	*/

# define ISDN_PROT_DEF		ISDN_PROT_RAW	/* default protocol	*/

/* Causes : Bit 0,1 = 10 -> xlated ISDN Q931 -> TAPI causes		*/
/*	    Bit 0,1 = 11 -> private disconnect causes			*/

# define CAUSE_MAPPED		0x80	/* xlated or private cause	*/

# define CAUSE_NORMAL		0x80	/* normal disconnect from net	*/
# define CAUSE_BUSY		0x81	/* call failed, callee busy	*/
# define CAUSE_REJECT		0x82	/* call rejected by callee	*/
# define CAUSE_OUTOFORDER	0x83	/* remote station out of order	*/

# define CAUSE_NOANSWER		0x84	/* callee not responding	*/
# define CAUSE_UNREACHABLE	0x85	/* callee unreachable		*/
# define CAUSE_CONGESTION	0x86	/* network overloaded		*/
# define CAUSE_INCOMPATIBLE	0x87	/* service not available	*/
# define CAUSE_REMOTE		0x88	/* remote disconnect		*/
# define CAUSE_UNAVAIL		0x89	/* cause not available now	*/
# define CAUSE_UNKNOWN		0x8A	/* ???				*/

# define CAUSE_IGNORE		0xC0	/* call not wanted		*/
# define CAUSE_AGAIN		0xC1	/* temporary out of resources	*/
# define CAUSE_DROP		0xC2	/* drop or close		*/
# define CAUSE_SHUT		0xC3	/* error detected		*/
# define CAUSE_LATE		0xC4	/* caller disappeared		*/
# define CAUSE_STATE		0xC5	/* illegal state		*/
# define CAUSE_FATAL		0xC6	/* fatal internal error		*/

/* Network layer data request types and flags from pc.h 		*/
/*	replicated here to prevent inclusion of pc.h in upper layer	*/
/* 	modules.							*/
/*	consistency of definitions is guaranteed via isdn.h which	*/
/*	first includes pc.h and then isdn_if.h.				*/

#define N_MDATA         1       /* more data to come REQ/IND        */
#define N_CONNECT       2       /* OSI N-CONNECT REQ/IND            */
#define N_CONNECT_ACK   3       /* OSI N-CONNECT CON/RES            */
#define N_DISC          4       /* OSI N-DISC REQ/IND               */
#define N_DISC_ACK      5       /* OSI N-DISC CON/RES               */
#define N_RESET         6       /* OSI N-RESET REQ/IND              */
#define N_RESET_ACK     7       /* OSI N-RESET CON/RES              */
#define N_DATA          8       /* OSI N-DATA REQ/IND               */
#define N_EDATA         9       /* OSI N-EXPEDITED DATA REQ/IND     */
#define N_UDATA         10      /* OSI D-UNIT-DATA REQ/IND          */
#define N_BDATA         11      /* BROADCAST-DATA REQ/IND           */
#define N_DATA_ACK      12      /* data ack ind for D-bit procedure */
#define N_EDATA_ACK     13      /* data ack ind for INTERRUPT       */

#define N_Q_BIT         0x10    /* Q-bit for req/ind                */
#define N_M_BIT         0x20    /* M-bit for req/ind                */
#define N_D_BIT         0x40    /* D-bit for req/ind                */

/* Flags ored to the original request/indication type in the */
/* FrameType argument of the PortRevc() function.	*/

# define ISDN_FRAME_CTL		0x8000

/* The T30 data structure passed via N_EDATA for FAX applications	*/

typedef struct T30_INFO {
	unsigned char	code;
#define			IDI_EDATA_DIS           0x01
#define			IDI_EDATA_FTT           0x02
#define			IDI_EDATA_MCF           0x03
#define			IDI_EDATA_DCS           0x81
#define			IDI_EDATA_TRAIN_OK      0x82
#define			IDI_EDATA_EOP           0x83
#define			IDI_EDATA_MPS           0x84
#define			IDI_EDATA_EOM           0x85
#define			IDI_EDATA_DTC           0x86
	unsigned char	rate;
#define			IDI_BR_24               1
#define			IDI_BR_48               2
#define			IDI_BR_72               3
#define			IDI_BR_96               4
#define			IDI_BR_120              5
#define			IDI_BR_144              6
#define     IDI_BR_168              7
#define     IDI_BR_192              8
#define     IDI_BR_216              9
#define     IDI_BR_240              0x0a
#define     IDI_BR_264              0x0b
#define     IDI_BR_288              0x0c
#define     IDI_BR_312              0x0d
#define     IDI_BR_336              0x0e

	unsigned char	resolution;
#define			IDI_VR_NORMAL           0
#define			IDI_VR_FINE             1
#define			IDI_VR_SUPER_FINE       2
#define			IDI_VR_ULTRA_FINE       4
	unsigned char	format;
#define			IDI_FORMAT_SFF          0
#define			IDI_FORMAT_ASCII        1
	unsigned char	pages_low;
	unsigned char	pages_high;
	unsigned char	atf;
#define			IDI_ATF_CAPI            0
#define			IDI_ATF_MODEM           1
#define			IDI_ATF_CLASS1          2
	unsigned char	control_bits_low;
	unsigned char	control_bits_high;
#define T30_CONTROL_BIT_DISABLE_FINE       0x0001
#define T30_CONTROL_BIT_ENABLE_ECM         0x0002
#define T30_CONTROL_BIT_ECM_64_BYTES       0x0004
#define T30_CONTROL_BIT_ENABLE_2D_CODING   0x0008
#define T30_CONTROL_BIT_ENABLE_T6_CODING   0x0010
#define T30_CONTROL_BIT_ENABLE_UNCOMPR     0x0020
#define T30_CONTROL_BIT_ACCEPT_POLLING     0x0040
#define T30_CONTROL_BIT_REQUEST_POLLING    0x0080
#define T30_CONTROL_BIT_MORE_DOCUMENTS     0x0100
#define T30_CONTROL_BIT_ACCEPT_SUBADDRESS  0x0200
#define T30_CONTROL_BIT_ACCEPT_SEL_POLLING 0x0400
#define T30_CONTROL_BIT_ACCEPT_PASSWORD    0x0800
#define T30_CONTROL_BIT_ENABLE_V34FAX      0x1000
#define T30_CONTROL_BIT_EARLY_CONNECT      0x2000
#define T30_CONTROL_BIT_MINIMUM_BUFFERING  0x4000
#define T30_CONTROL_BIT_ALL_FEATURES      ( T30_CONTROL_BIT_ENABLE_ECM       | \
                                            T30_CONTROL_BIT_ENABLE_2D_CODING | \
                                            T30_CONTROL_BIT_ENABLE_T6_CODING | \
                                            T30_CONTROL_BIT_ENABLE_UNCOMPR   | \
                                            T30_CONTROL_BIT_ENABLE_V34FAX )
	unsigned char	feature_bits_low;
	unsigned char	feature_bits_high;
#define T30_FEATURE_BIT_FINE               0x0001
#define T30_FEATURE_BIT_ECM                0x0002
#define T30_FEATURE_BIT_ECM_64_BYTES       0x0004
#define T30_FEATURE_BIT_2D_CODING          0x0008
#define T30_FEATURE_BIT_T6_CODING          0x0010
#define T30_FEATURE_BIT_UNCOMPR_ENABLED    0x0020
#define T30_FEATURE_BIT_POLLING            0x0040
#define T30_FEATURE_BIT_MORE_DOCUMENTS     0x0100
#define T30_FEATURE_COPY_QUALITY_NOERR     0x0000
#define T30_FEATURE_COPY_QUALITY_OK        0x0200
#define T30_FEATURE_COPY_QUALITY_RTP       0x0400
#define T30_FEATURE_COPY_QUALITY_RTN       0x0600
#define T30_FEATURE_COPY_QUALITY_MASK      0x0600
#define T30_FEATURE_COPY_QUALITY_SHIFT     9
#define T30_FEATURE_BIT_V34FAX             0x1000
	unsigned char	recording_properties;
/*
	Codung of upper 4 bits
  */
#define T30_MIN_SCANLINE_TIME_00_00_00  0
#define T30_MIN_SCANLINE_TIME_05_05_05  1
#define T30_MIN_SCANLINE_TIME_10_05_05  2
#define T30_MIN_SCANLINE_TIME_10_10_10  3
#define T30_MIN_SCANLINE_TIME_20_10_10  4
#define T30_MIN_SCANLINE_TIME_20_20_20  5
#define T30_MIN_SCANLINE_TIME_40_20_20  6
#define T30_MIN_SCANLINE_TIME_40_40_40  7
#define T30_MIN_SCANLINE_TIME_RES_8     8   /* reserved */
#define T30_MIN_SCANLINE_TIME_RES_9     9   /* reserved */
#define T30_MIN_SCANLINE_TIME_RES_10    10  /* reserved */
#define T30_MIN_SCANLINE_TIME_10_10_05  11
#define T30_MIN_SCANLINE_TIME_20_10_05  12
#define T30_MIN_SCANLINE_TIME_20_20_10  13
#define T30_MIN_SCANLINE_TIME_40_20_10  14
#define T30_MIN_SCANLINE_TIME_40_40_20  15

	unsigned char	universal_6;
	unsigned char	universal_7;
	unsigned char	station_id_len;
	unsigned char	head_line_len;
	unsigned char	station_id[20];
	/* byte head_line[];    */
	/* byte sub_sep_length; */
	/* byte sub_sep_field[];*/
	/* byte pwd_length;     */
	/* byte pwd_field[];    */
} T30_INFO;

#define T30_MAX_SUBADDRESS_LENGTH 20
#define T30_MAX_PASSWORD_LENGTH   20

#define SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT 0x01
#define SDLC_L2_OPTION_SINGLE_DATA_PACKETS    0x02

/* Connection parameters coming with IsdnDial()/PortRing() calls */
typedef struct _diva_modem_options {
		byte	 valid;
		byte	 disable_error_control;
		byte	 framing_valid;
		byte	 guard_tone;
		byte   line_taking;
		byte   protocol_options;
		byte	 s7;
		byte	 s10;
		byte	 retrain;
		byte	 bell;
		byte	 fast_connect_mode;
		byte	 sdlc_prot_options;
		byte	 sdlc_address_A;

		word disabled;
		byte enabled;
		byte negotiation;
		word min_rx;
		word max_rx;
		word min_tx;
		word max_tx;
		int  index;
		int  automode;
		byte	modulation_options;
		word	reserved_modulation_options;
		dword	reserved_modulation;
		byte	bell_selected;
		byte	fast_connect_selected;
		byte	fax_line_taking;
		/*
			Framing
			*/
		byte data_bits;
		byte stop_bits;
		byte parity;
		byte framing_cai;
	} diva_modem_options_t;

typedef struct ISDN_CONN_PARMS
{	unsigned short	MaxFrame ;					/* max. frame length		*/
	unsigned char	Line ;						/* line assigned to call	*/
	unsigned char	PlanDest ;					/* destination # plan		*/
	unsigned char	Dest[ISDN_MAX_NUMBER] ;		/* destination number		*/
	unsigned char	DestSub[ISDN_MAX_SUBADDR] ;	/* destination subaddress	*/
	unsigned char	PlanOrig ;					/* origination # plan		*/
	unsigned char	PlanOrig_1;					/* origination # plan for second origination address */
	unsigned char PresentationAndScreening_1; /* presentation and screening for second origination address */
	unsigned char	Orig[ISDN_MAX_NUMBER] ;		/* origination number		*/
	unsigned char	Orig_1[ISDN_MAX_NUMBER] ;		/* second origination number		*/
	unsigned char	OrigSub[ISDN_MAX_SUBADDR] ;	/* origination subaddress	*/
	unsigned char	OrigName[ISDN_MAX_PARTY_NAME] ;		/* origination number		*/
	unsigned char   DisplayIE[ISDN_MAX_DISPLAY_IE] ;/* Display info element          */
	unsigned char	Bc[ISDN_MAX_BC] ;			/* bearer capabilities		*/
	unsigned char	Llc[ISDN_MAX_LLC] ;			/* low layer compatibility	*/
	unsigned char	Service ;					/* 1TR6 Service	Indicator	*/
	unsigned char	ServiceAdd ;				/* 1TR6 Additional Service	*/
	unsigned char	Prot ;						/* ISDN Protocol			*/
	unsigned char	Mode ;						/* connection mode			*/
	unsigned char	Baud ;						/* baud rate				*/
	unsigned char	Cause ;						/* error cause				*/

	diva_modem_options_t	modem_options;
	int           DestSubLength;     /* length need for non-ascii number */
	int           OrigSubLength; /* length need for non-ascii number */

	int						bit_t_detection; /* framing detection will use bit-transparent data */

	int						selected_channel;

	byte Presentation_and_Screening;
} ISDN_CONN_PARMS ;

/* The connection info passed at a portUp() call */

typedef struct ISDN_CONN_INFO
{	unsigned char	Prot ;						/* B-channel protocol		*/
	unsigned char	Mode ;						/* connection mode			*/
#define				ISDN_CONN_MODE_DEFAULT	0x00
#define				ISDN_CONN_MODE_SYNCPPP	0x01
	unsigned short	MaxFrame ;					/* max. frame length		*/
	unsigned long	RxSpeed ;					/* receive Speed			*/
	unsigned long	TxSpeed ;					/* transmit Speed			*/
	unsigned short	ConOpts ;					/* connected options		*/
	unsigned char	ConNorm ;					/* connected norm			*/
	unsigned char	cFiller ;					/* fill up to a long		*/
} ISDN_CONN_INFO ;

/* Port->Isdn requests */

typedef int	(* IsdnXmit)	(void *P, void  *hC,
				 unsigned char  *Data,
				 unsigned short sizeData,
				 unsigned short FrameType);
typedef void *	(* IsdnDial)	(void *P,
				 ISDN_CONN_PARMS *Parms,
				 T30_INFO		 *T30Info);
typedef int	(* IsdnListen)	(int Listen);
typedef int	(* IsdnAccept)	(void *P, void *hC);
typedef int	(* IsdnAnswer)	(void *P, void *hC,
				 unsigned short	MaxFrame,
				 T30_INFO	*T30Info, ISDN_CONN_PARMS*	Parms);
typedef int	(* IsdnDrop)	(void *P, void *hC,
				 unsigned char	Cause,
				 unsigned char	Detach,
				 unsigned char  OptionalCause);
typedef void	(* IsdnCheck)	(void);
typedef void	(* IsdnReset)	(void);
typedef unsigned short
		(* IsdnCaps)	(void);

typedef struct ISDN_REQUESTS 
{
	IsdnXmit	Xmit;
	IsdnDial	Dial;
	IsdnListen	Listen;
	IsdnAccept	Accept;
	IsdnAnswer	Answer;
	IsdnDrop	Drop;
	IsdnCheck	Check;
	IsdnReset	Reset;
	IsdnCaps	Caps;
} ISDN_REQUESTS;

/* Isdn->Port indications */

typedef int	(* PortWant)	(void *C, void *hP, unsigned short sizeData);
typedef int	(* PortRecv)	(void *C, void *hP, unsigned char *Data,
													 unsigned short sizeData, unsigned short FrameType);
typedef void	(* PortStop)	(void *C, void *hP);
typedef void	(* PortGo)	(void *C, void *hP);
typedef void *	(* PortRing)	(void *C, ISDN_CONN_PARMS *Parms);
typedef char *	(* PortName)	(void *C, void *hP) ;
typedef int	(* PortUp)	(void *C, void *hP, ISDN_CONN_INFO *Info);
typedef int	(* PortDown)	(void *C, void *hP,
													 unsigned char Cause, unsigned char q931_cause);
typedef void	(* PortIdle)	(void *C, void *hP);

typedef struct ISDN_INDICATIONS 
{   PortWant	Want;
	PortRecv	Recv;
	PortStop	Stop;
	PortGo		Go;
	PortRing	Ring;
	PortName	Name;
	PortUp		Up;
	PortDown	Down;
	PortIdle	Idle;
} ISDN_INDICATIONS;

/* Port <-> Isdn attach/detach */

ISDN_REQUESTS *	isdnBind (ISDN_INDICATIONS *Indications,
			  void *DescTab, unsigned short sizeDescTab);

/*
	Global Mode Definition
	In case global mode is set then all calls, independent from the call type
	are handled in accordance with this mode.

	Upper 16 bits - mode
	Lowr  16 bits - protocol to map
	*/
#define GLOBAL_MODE_NONE         0x00000000
/*
	Use PIAFS, how it is used in china and without D-channel signalling - i.e.
  map all incomming calls, independent from used BC/LLC to PIAFS, China dialect
	*/
#define GLOBAL_MODE_CHINA_PIAFS  0x10000000

/*
	GLOBAL MODULE OPTIONS
	*/
#define DIVA_FAX_FORCE_ECM           1
#define DIVA_FAX_FORCE_V34           2
#define DIVA_FAX_FORCE_SEP_SUB_PWD   4
#define DIVA_FAX_ALLOW_V34_CODES     8
#define DIVA_FAX_ALLOW_HIRES         0x10
#define DIVA_ISDN_IGNORE_NUMBER_TYPE 0x20
#define DIVA_ISDN_AT_RSP_IF_RINGING  0x40
#define DIVA_ISDN_DIAL_RR            0x80

#define BIT_T_DETECT_ON_S  1 /* Use BT detection only if the owner is not clear */
#define BIT_T_DETECT_ON_D  2 /* Use BT detection only if not modem */
#define BIT_T_DETECT_ON    3 /* Always use BT detection */
#define BIT_T_DETECT_OFF 0   /* BT detection is turned OFF */

/*
	Call filter used to bind the CPN's to the specified protocols, independent from the
	real detected protocol and independent from the signalled protocol
	*/
#define MAX_CALL_FILTER_ENTRIES 200

typedef struct diva_call_filter_entry_ {
	char number[ISDN_MAX_NUMBER];
	byte prot;
} diva_call_filter_entry_t;

typedef struct diva_call_filter_ {
	int n_entries;
	diva_call_filter_entry_t entry[MAX_CALL_FILTER_ENTRIES];
} diva_call_filter_t;

extern const char* diva_tty_isdn_get_protocol_name (int protocol_nr);
const diva_call_filter_entry_t* diva_tty_isdn_get_call_filter_entry (int filter_nr);
int diva_tty_isdn_set_call_filter_entry (const char* cmd);

#define T30_RECPROP_BIT_PAGE_WIDTH_A4      0x00
#define T30_RECPROP_BIT_PAGE_WIDTH_B4      0x01
#define T30_RECPROP_BIT_PAGE_WIDTH_A3      0x02
#define T30_RECPROP_BIT_PAGE_WIDTH_MASK    0x03
#define T30_RECPROP_BIT_PAGE_WIDTH_SHIFT   0

# endif /* ISDN_IF___H */
