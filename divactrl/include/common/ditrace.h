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
#define PPP_ANALYSE_BCHANNEL 0x00000001
#define PPP_ANALYSE_DATA     0x00000002

#define VSIG_ANALYSE_DATA    0x00000001

#define RTP_ANALYSE_DATA    0x00000001

#define DBG_MAGIC (0x47114711L)

#define TRACE_COMPRESSION_TAG "TRACE ENTRY COMPRESSION: ARIB62"

typedef struct MSG_QUEUE {
	dword	Size;		/* total size of queue (constant)	*/
	byte	*Base;		/* lowest address (constant)		*/
	byte	*High;		/* Base + Size (constant)		*/
	byte	*Head;		/* first message in queue (if any)	*/
	byte	*Tail;		/* first free position			*/
	byte	*Wrap;		/* current wraparound position 		*/
	dword	Count;		/* current no of bytes in queue		*/
} MSG_QUEUE;

typedef struct MSG_HEAD {
	volatile dword	Size;		/* size of data following MSG_HEAD	*/
#define	MSG_INCOMPLETE	0x8000	/* ored to Size until queueCompleteMsg 	*/
} MSG_HEAD;

#define queueCompleteMsg(p) do{ ((MSG_HEAD *)p - 1)->Size &= ~MSG_INCOMPLETE; }while(0)
extern int diva_read_binary_ring_buffer (char *filename);


typedef struct _diva_cmd_line_parameter {
	const char* ident;
	int found;
	int string;
	int value;
	int length;
	dword min;
	dword max;
	dword  vi;
	char vs[256];
} diva_cmd_line_parameter_t;

#if defined(__DIVA_INCLUDE_DITRACE_PARAMETERS__)

static diva_cmd_line_parameter_t ditrace_parameters[] = {
/*                        found
                            |  string
                            |  |  value
                            |  |  |     length
                            |  |  |     |  min
                            |  |  |     |  |           max
                            |  |  |     |  |           |  vi
                            |  |  |     |  |           |  |  vs */
{ "l",                      0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "p",                      0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "s",                      0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "d",                      0, 1, 1,  255, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "f",                      0, 1, 2,  127, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "m",                      0, 0, 2,    0, 0, 0x7fffffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "events",                 0, 1, 2,  255, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "Silent",                 0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "debug",                  0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"File",                    0, 1, 1,  255, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "w",                      0, 0, 1,    0,64,   512*1024, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"i",                       0, 1, 1,  255, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "-version",               0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "-help",                  0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "h",                      0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "?",                      0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "UseMlogTime",            0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"s",                       0, 1, 1,  255, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"Compress",                0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "no64bit",                0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "64bit",                  0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "ppp",                    0, 1, 2,  255, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "vsig",                   0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "rtp",                    0, 0, 2,    0, 0,          3, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "ssrc",                   0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "pt",                     0, 0, 1,    0, 0,       0x7f, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "savertppayload",         0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "channel",                0, 0, 1,    0, 1,         31, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "nodchannel",             0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "nostrings",              0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "noheaders",              0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "ncpib3protocol",         0, 0, 1,    0, 1,       0xff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "search",                 0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{  0 , 0, 0, 0, 0, 0,    0, 0, {0, 0}}
};


typedef struct {
        const char*                     key;
        const char*     text[64];
} diva_ditrace_options_t;

static diva_ditrace_options_t ditrace_options [] = {
{"-l",           {"list registered adapters/drivers",
                  0}
},
{"-d  [1...]",   {"specify driver for debug/configuration",
                  0}
},
{"-p",           {"poll debug driver continuously",
                  0}
},
{"-m",           {"read driver debug mask",
                  0}
},
{"-m [0...0x7fffffff]",
                 {"set driver debug mask",
                  0}
},
{"-events [EVENTS]",
                 {"set driver debug mask",
                  " EVENTS:",
                  "  Adapter events:",
                  "   CARD_STRINGS          0x00000001 All trace messages from the card",
                  "   CARD_DCHAN            0x00000002 All D-channel related trace messages",
                  "   CARD_MDM_PROGRESS     0x00000004 Modem progress events",
                  "   CARD_FAX_PROGRESS     0x00000008 Fax progress events",
                  "   CARD_IFC_STATISTICS   0x00000010 Interface call statistics",
                  "   CARD_MDM_STATISTICS   0x00000020 Global modem statistics",
                  "   CARD_FAX_STATISTICS   0x00000040 Global call statistics",
                  "   CARD_LINE_EVENTS      0x00000080 Line state events",
                  "   CARD_IFC_EVENTS       0x00000100 Interface/L1/L2 state events",
                  "   CARD_IFC_BCHANNEL     0x00000200 B-Channel trace for all channels",
                  "   CARD_IFC_AUDIO        0x00000400 Audio Tap trace for all channels",
                  "  Driver events:",
                  "   DRV_LOG               0x00000001 Always worth mentioning",
                  "   DRV_FTL               0x00000002 Always sampled error",
                  "   DRV_ERR               0x00000004 Any kind of error",
                  "   DRV_TRC               0x00000008 Verbose information",
                  "   DRV_XLOG              0x00000010 Old xlog info",
                  "   DRV_MXLOG             0x00000020 Maestra xlog info",
                  "   DRV_EVL               0x00000080 Eventlog message",
                  "   DRV_REG               0x00000100 Init/configuration read",
                  "   DRV_MEM               0x00000200 Memory management",
                  "   DRV_SPL               0x00000400 Event/spinlock handling",
                  "   DRV_IRP               0x00000800 OS I/O request handling",
                  "   DRV_TIM               0x00001000 Timer/watchdog handling",
                  "   DRV_BLK               0x00002000 Raw data block contents",
                  "   DRV_TAPI              0x00010000 Debug TAPI interface",
                  "   DRV_NDIS              0x00020000 Debug NDIS interface",
                  "   DRV_CONN              0x00040000 Connection handling",
                  "   DRV_STAT              0x00080000 Trace state machines",
                  "   DRV_SEND              0x00100000 Trace raw xmitted data",
                  "   DRV_RECV              0x00200000 Trace raw received data",
                  "   DRV_PRV0              0x01000000 Trace messages",
                  "   DRV_PRV1              0x02000000 Trace messages",
                  "   DRV_PRV2              0x04000000 Trace messages",
                  "   DRV_PRV3              0x08000000 Trace messages",
                  "  General events:",
                  "   NONE                  0x00000000 Used to deactivate all trace events",
                  "                         (e.g. -events NONE)",
                  " or any combination (e.g. -events CARD_STRINGS+CARD_DCHAN)",
                  0}
},
{"-f",           {"read CPN/CiPN filter",
                  0}
},
{"-f ['string']",
                 {"set CPN/CiPN filter for selective tracing",
                  "'*'   - turns filter off (default)",
                  "'XXX' - turns the B-channel and AudioTap",
                  "        tracing is on only if CPN/CiPN ends with XXX",
                  "maximum filter length: 127 characters",
                  0}
},
{"-w [64...524288]",
                 {"store raw data stream in ring buffer",
                  "with specified size (in Kilobytes)",
                  "suggested value: 256 KBytes",
                  "can be used only with '-File' option",
                  0}
},
{"-i XXX",       {"interpret raw debug data from file 'XXX'",
                  0}
},
{"-h or -?",    {"help", 0}},
{"--help",      {"help", 0}},
{"--version",   {"build version", 0}},
{"-File",        {"write information to file instead of console",
                  "Filename should be provided",
                  0}
},
{"-UseMlogTime", {"show adapter time stamp for adapter trace",
                  "messages instead of system time stamp",
                  0}
},
{"-o",           {"write information to file instead of console",
                  "Filename should be provided",
                  0}
},
{"-Silent",      {"silent operation mode, do not display messages",
                  0}
},
{"-debug",       {"display extra debug messages",
                  0}
},
{"-Compress",    {"compress trace messages before write to binary",
                  "trace buffer.",
                  "Compression allows increase capacity of",
                  "trace buffer by ~ 15 percent.",
                  0}
},
{"-no64bit",    {"Turns off detection of 64bit",
                 "trace buffer.",
                 0}
},
{"-64bit",    {"64bit trace buffer.",
               0}
},
{"-ppp [type]", {"Decode PPP frames",
                 "Optional 'type' parameter:",
                 "  bchannel - decode bchannel (default)",
                 "  data     - decode data",
                 "  detect   - detect PPP frames",
                 "  or any combination (e.g. -ppp bchannel+data)",
                 0}
},
{"-vsig",       {"Decode VSIG messages",
                 0}
},
{"-rtp",        {"Decode RTP packages",
                 0}
},
{"-ssrc SSRC",   {"Filter RTP SSRC",
                  "SSRC - RTP SSRC",
                 0}
},
{"-pt PT",       {"Filter RTP payload",
                  "PT - RTP payload type",
                 0}
},
{"-savertppayload", {"Save RTP payload to file",
                     "Use hex SSRC value as file name",
                      0}
},
{"-channel BCh", {"Process only messages originated by selected channel",
                  "BCh parameter: 1...31",
                  0}
},
{"-nodchannel",  {"Do not decode dchannel frames",
                  0}
},
{"-nostrings",   {"Do not decode adapter trace messages",
                  0}
},
{"-noheaders",   {"Do not decode trace headers",
                  0}
},
{"-ncpib3protocol", {"Use specified CAPI B3 protocol for decoding of",
                     "CAPI NCPI messages if B3 protocol for message",
                     "is not known",
                     0 }
},
{"-search",      {"Search for events and stop trace process upon",
                  "detection of events",
                  0 }
},
{ 0, {0}}
};

#endif

#if !defined(DIVA_BUILD)
#define DIVA_BUILD "local"
#endif

