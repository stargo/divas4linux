/*****************************************************************************
 *
 * (c) COPYRIGHT 2005-2008       Dialogic Corporation
 *
 * ALL RIGHTS RESERVED
 *
 * This software is the property of Dialogic Corporation and/or its
 * subsidiaries ("Dialogic"). This copyright notice may not be removed,
 * modified or obliterated without the prior written permission of
 * Dialogic.
 *
 * This software is a Trade Secret of Dialogic and may not be copied,
 * transmitted, provided to or otherwise made available to any company,
 * corporation or other person or entity without written permission of
 * Dialogic.
 *
 * No right, title, ownership or other interest in the software is hereby
 * granted or transferred. The information contained herein is subject
 * to change without notice and should not be construed as a commitment of
 * Dialogic.
 *
 *----------------------------------------------------------------------------*/
/*****************************************************************************
 *
 * File:           capi_tbl.c
 * Component:      Maint
 * Description:    CAPI 2.0 analyser (info string tables)
 *
 *****************************************************************************/
static struct
{
    byte code;
    const char * name;
    const char * format[4];
    void (* analyse) (byte, API_PARSE *, FILE *);
} capi20_command[] =
{
    { 0x01, "ALERT",                  {"ds",          "dw",   "",           ""       }, alert },
    { 0x02, "CONNECT",                {"dwsssssssss", "dw",   "dwssssssss", "dwsssss"}, connect },
    { 0x03, "CONNECT_ACTIVE",         {"",            "",     "dsss",       "d"      }, connect_active },
    { 0x04, "DISCONNECT",             {"ds",          "dw",   "dw",         "d"      }, disconnect },
    { 0x05, "LISTEN",                 {"ddddss",      "dw",   "",           ""       }, listen },
    { 0x08, "INFO",                   {"dss",         "dw",   "dws",        "d"      }, info },
    { 0x41, "SELECT_B_PROTOCOL",      {"ds",          "dw",   "",           ""       }, select_b },
    { 0x80, "FACILITY",               {"dws",         "dwws", "dws",        "dws"    }, facility },
    { 0x82, "CONNECT_B3",             {"ds",          "dw",   "ds",         "dws"    }, connect_b3 },
    { 0x83, "CONNECT_B3_ACTIVE",      {"",            "",     "ds",         "d"      }, connect_b3_active },
    { 0x84, "DISCONNECT_B3",          {"ds",          "dw",   "dws",        "d"      }, disconnect_b3 },
    { 0x86, "DATA_B3",                {"ddwwwr",      "dww",  "ddwwwr",     "dw"     }, data_b3 },
    { 0x87, "RESET_B3",               {"ds",          "dw",   "ds",         "d"      }, reset_b3 },
    { 0x88, "CONNECT_B3_T90_ACTIVE",  {"",            "",     "ds",         "d"      }, connect_b3_t90_active },
    { 0xd4, "PORTABILITY",            {"dws",         "dw",   "dww",        "d"      }, portability },
    { 0xff, "MANUFACTURER",           {"ddr",         "ddr",  "ddr",        "ddr"    }, manufacturer },
    { 0x00, "",                       {"",            "",     "",           ""       }, ((void*)0)  }
};
static struct
{
    byte code;
    char * name;
} capi20_subcommand[] =
{
    { 0x80, "REQ" },
    { 0x81, "CONF" },
    { 0x82, "IND" },
    { 0x83, "RESP" },
    { 0x00, "" }
};
/******************************************************************************/
typedef struct info_table
{
    word value;
    char * name;
} INFO_TABLE;
typedef struct info_table32
{
    dword value;
    char * name;
} INFO_TABLE32;
INFO_TABLE Info[] =
{
    { 0x0000, "initiated" },
    { 0x0001, "NCPI not supported by current protocol" },
    { 0x0002, "flags not supported by current protocol" },
    { 0x0003, "alert already sent by another application" },
    { 0x1001, "too many applications" },
    { 0x1002, "logical block size too small" },
    { 0x1003, "buffer exceeds 64 kByte" },
    { 0x1004, "message buffer size too small" },
    { 0x1005, "max. number of logical connections not supported" },
    { 0x1006, "reserved" },
    { 0x1007, "not accepted because of internal busy situation" },
    { 0x1008, "OS resource error" },
    { 0x1009, "Common ISDN API not installed" },
    { 0x100a, "controller does not support external equipment" },
    { 0x100b, "controller does only support external equipment" },
    { 0x1101, "illegal application number" },
    { 0x1102, "illegal command" },
    { 0x1103, "queue full" },
    { 0x1104, "queue is empty" },
    { 0x1105, "queue overflow, message lost" },
    { 0x1106, "unknown notification parameter" },
    { 0x1107, "not accepted because of internal busy situation" },
    { 0x1108, "OS resource error" },
    { 0x1109, "Common ISDN API not installed" },
    { 0x110a, "controller does not support external equipment" },
    { 0x110b, "controller does only support external equipment" },
    { 0x2001, "message not supported in current state" },
    { 0x2002, "illegal Controller/PLCI/NCCI" },
    { 0x2003, "out of PLCI" },
    { 0x2004, "out of NCCI" },
    { 0x2005, "out of LISTEN" },
    { 0x2006, "out of FAX resources" },
    { 0x2007, "illegal message parameter coding" },
    { 0x2008, "no interconnection resources available" },
    { 0x3001, "B1 protocol not supported" },
    { 0x3002, "B2 protocol not supported" },
    { 0x3003, "B3 protocol not supported" },
    { 0x3004, "B1 protocol parameter not supported" },
    { 0x3005, "B2 protocol parameter not supported" },
    { 0x3006, "B3 protocol parameter not supported" },
    { 0x3007, "B protocol combination not supported" },
    { 0x3008, "NCPI not supported" },
    { 0x3009, "CIP value unknown" },
    { 0x300a, "flags not supported" },
    { 0x300b, "facility not supported" },
    { 0x300c, "data length not supported by current protocol" },
    { 0x300d, "reset procedure not supported by current protocol" },
    { 0x300e, "supplementary service not supported / TEI assignment failed / overlapping channel masks" },
    { 0x300f, "unsupported interoperability" },
    { 0x3010, "request not allowed in this state" },
    { 0x3011, "facility specific function not supported" },
    { 0x3305, "rejected by the ISDN-Guard security option" },
    { 0xffff, "unknown value" }
};
INFO_TABLE BChannelInfo[] =
{
    { 0x0000, "use B channel" },
    { 0x0001, "use D channel" },
    { 0x0002, "use neither B channel nor D channel" },
    { 0x0003, "use channel allocation" },
    { 0x0004, "use channel identification info element" },
    { 0xffff, "unknown value" }
};
INFO_TABLE B1Prot[] =
{
    { 0x0000, "64 kBit/s HDLC" },
    { 0x0001, "64 kBit/s transparent" },
    { 0x0002, "V.110 async start/stop" },
    { 0x0003, "V.110 sync HDLC" },
    { 0x0004, "T.30 modem for fax group 3 and real-time FAX over IP" },
    { 0x0005, "64 kBit/s inverted HDLC" },
    { 0x0006, "56 kBit/s transparent" },
    { 0x0007, "Modem with full negotiation" },
    { 0x0008, "Modem async start/stop" },
    { 0x0009, "Modem sync HDLC" },
    { 0x001f, "Voice codec for packetized voice transfer" },
    { 0xffff, "unknown value" }
};
INFO_TABLE B2Prot[] =
{
    { 0x0000, "X.75 SLP (ISO 7776)" },
    { 0x0001, "Transparent" },
    { 0x0002, "SDLC" },
    { 0x0003, "LAPD for D channel X.25" },
    { 0x0004, "T.30" },
    { 0x0005, "PPP" },
    { 0x0006, "Transparent" },
    { 0x0007, "Modem with full negotiation" },
    { 0x0008, "X.75 SLP (ISO 7776) with V.42bis" },
    { 0x0009, "V.120 async" },
    { 0x000a, "V.120 async with V.42bis" },
    { 0x000b, "V.120 bit transparent" },
    { 0x000c, "LAPD Q.921 free SAPI selection" },
    { 0x001e, "T.38 for real-time fax communication" },
    { 0x001f, "Packetized voice transfer for voice over IP" },
    { 0xffff, "unknown value" }
};
INFO_TABLE B3Prot[] =
{
    { 0x0000, "Transparent" },
    { 0x0001, "T.90NL" },
    { 0x0002, "ISO 8208 (X.25 DTE-DTE)" },
    { 0x0003, "X.25 DCE" },
    { 0x0004, "T.30" },
    { 0x0005, "T.30 with extensions" },
    { 0x0006, "reserved" },
    { 0x0007, "Modem" },
    { 0x001e, "Transparent protocol for communication over IP" },
    { 0x001f, "RTP (Real-time Transport Protocol) for voice and FAX" },
    { 0xffff, "unknown value" }
};
INFO_TABLE CipValue[] =
{
    { 0, "no predefined profile" },
    { 1, "speech" },
    { 2, "unrestricted digital information" },
    { 3, "restricted digital information" },
    { 4, "3.1 kHz audio" },
    { 5, "7 kHz audio" },
    { 6, "video" },
    { 7, "packet mode" },
    { 8, "56 kBit/s rate adaption" },
    { 9, "unrestricted digital information with tones" },
    { 10, "reserved" },
    { 11, "reserved" },
    { 12, "reserved" },
    { 13, "reserved" },
    { 14, "reserved" },
    { 15, "reserved" },
    { 16, "telephony" },
    { 17, "fax group 2/3" },
    { 18, "fax group 4 class 1" },
    { 19, "teletex basic/mixed and fax group 4 class 2/3" },
    { 20, "teletex basic/processable" },
    { 21, "teletex basic" },
    { 22, "interworking for videotex" },
    { 23, "telex" },
    { 24, "message handling systems X.400" },
    { 25, "OSI application X.200" },
    { 26, "7 kHz telephony" },
    { 27, "video telephony, first connection" },
    { 28, "video telephony, second connection" },
    { 0xffff, "unknown value" }
};
INFO_TABLE Reject[] =
{
    { 0x0000, "accept call" },
    { 0x0001, "ignore call" },
    { 0x0002, "reject call, normal call clearing" },
    { 0x0003, "reject call, user busy" },
    { 0x0004, "reject call, requested channel not available" },
    { 0x0005, "reject call, facility rejected" },
    { 0x0006, "reject call, channel unacceptable" },
    { 0x0007, "reject call, incompatible destination" },
    { 0x0008, "reject call, destination out of order" },
    { 0xffff, "unknown value" }
};
INFO_TABLE ReasonB3[] =
{
    { 0x0000, "clearing according to protocol" },
    { 0x3301, "protocol error layer 1" },
    { 0x3302, "protocol error layer 2" },
    { 0x3303, "protocol error layer 3" },
    { 0x3305, "Call rejected by the ISDN-Guard security option" },
    { 0x3311, "remote station is no fax G3 machine" },
    { 0x3312, "training error" },
    { 0x3313, "remote station does not support transfer mode" },
    { 0x3314, "remote abort" },
    { 0x3315, "remote procedure error" },
    { 0x3316, "local tx data underrun" },
    { 0x3317, "local rx data overflow" },
    { 0x3318, "local abort" },
    { 0x3319, "illegal parameter coding" },
    { 0x3500, "normal end of connection" },
    { 0x3501, "carrier lost" },
    { 0x3502, "no modem with error correction" },
    { 0x3503, "no answer to protocol request" },
    { 0x3504, "modem only works in sync mode" },
    { 0x3505, "framing fails" },
    { 0x3506, "protocol negotiation fails" },
    { 0x3507, "modem sends wrong protocol request" },
    { 0x3508, "sync information missing" },
    { 0x3509, "normal end of connection" },
    { 0x350a, "no answer" },
    { 0x350b, "protocol error" },
    { 0x350c, "error on compression" },
    { 0x350d, "no connect" },
    { 0x350e, "no fallback allowed" },
    { 0x350f, "no modem on requested number" },
    { 0x3510, "handshake error" },
    { 0xffff, "unknown value" }
};
INFO_TABLE Reason[] =
{
    { 0x0000, "no cause available" },
    { 0x3301, "protocol error layer 1" },
    { 0x3302, "protocol error layer 2" },
    { 0x3303, "protocol error layer 3" },
    { 0x3304, "another application got that call" },
    { 0x3305, "Call rejected by the ISDN-Guard security option" },
    { 0xffff, "unknown value" }
};
INFO_TABLE DataReqFlags[] =
{
    { 0x0001, "qualifier bit" },
    { 0x0002, "more data bit" },
    { 0x0004, "delivery confirmation bit" },
    { 0x0008, "expedited data" },
    { 0x0010, "UI frame" },
    { 0xffff, "reserved" }
};
INFO_TABLE DataIndFlags[] =
{
    { 0x0001, "qualifier bit" },
    { 0x0002, "more data bit" },
    { 0x0004, "delivery confirmation bit" },
    { 0x0008, "expedited data" },
    { 0x0010, "break" },
    { 0x8000, "framing error bit" },
    { 0xffff, "reserved" }
};
INFO_TABLE Format[] =
{
    { 0x0000, "SFF" },
    { 0x0001, "Plain FAX format" },
    { 0x0002, "PCX" },
    { 0x0003, "DCX" },
    { 0x0004, "TIFF" },
    { 0x0005, "ASCII" },
    { 0x0006, "Extended ANSI" },
    { 0x0007, "Binary-File transfer" },
    { 0x0008, "Native (JPEG/JBIG)" },
    { 0xffff, "unknown value" }
};
INFO_TABLE Facility[] =
{
    { 0x0000, "Handset support" },
    { 0x0001, "DTMF" },
    { 0x0002, "V.42 bis" },
    { 0x0003, "Supplementary Services" },
    { 0x0004, "Power Management Wakeup" },
    { 0x0005, "Line Interconnect" },
    { 0x0006, "Broadband Extensions/Echo Cancellation" },
    { 0x0007, "Controller Events" },
    { 0x0008, "Echo Cancellation" },
    { 0x00FB, "Resource reservation" },
    { 0x00FC, "Connect support" },
    { 0x00FD, "FAX over IP" },
    { 0x00FE, "Voice over IP" },
    { 0xffff, "reserved" }
};
INFO_TABLE FacilityFunction[] =
{
    { 0x0000, "Get compression info" },
    { 0x0001, "Start DTMF listen" },
    { 0x0002, "Stop DTMF listen" },
    { 0x0003, "Send DTMF digits" },
    { 0x0004, "Start DTMF listen, detect edges" },
    { 0xffff, "reserved" }
};
INFO_TABLE SupServiceFunction[] =
{
    { 0x0000, "get supported services" },
    { 0x0001, "listen" },
    { 0x0002, "hold" },
    { 0x0003, "retrieve" },
    { 0x0004, "suspend" },
    { 0x0005, "resume" },
    { 0x0006, "ECT" },
    { 0x0007, "3PTY begin" },
    { 0x0008, "3PTY end" },
    { 0x0009, "CF activate" },
    { 0x000a, "CF deactivate" },
    { 0x000b, "CF interrogate parameters" },
    { 0x000c, "CF interrogate numbers" },
    { 0x000d, "CD" },
    { 0x000e, "MCID" },
    { 0x000f, "CCBS request" },
    { 0x0010, "CCBS deactivate" },
    { 0x0011, "CCBS interrogate" },
    { 0x0012, "CCBS call" },
    { 0x0013, "MWI activate" },
    { 0x0014, "MWI deactivate" },
    { 0x0015, "CCNR request" },
    { 0x0016, "CCNR interrogate" },
    { 0x0017, "CONF Begin" },
    { 0x0018, "CONF add" },
    { 0x0019, "CONF split" },
    { 0x001A, "CONF drop" },
    { 0x001B, "CONF isolate" },
    { 0x001C, "CONF reattach" },
    { 0x8000, "hold notification" },
    { 0x8001, "retrieve notification" },
    { 0x8002, "suspend notification" },
    { 0x8003, "resume notification" },
    { 0x8004, "call is diverting notification" },
    { 0x8005, "diversion activated notification" },
    { 0x8006, "CF activate notification" },
    { 0x8007, "CF deactivate notification" },
    { 0x8008, "diversion information" },
    { 0x8009, "call transfer alerted notification" },
    { 0x800a, "call transfer active notification" },
    { 0x800b, "conference established notification" },
    { 0x800c, "conference disconnect notification" },
    { 0x800d, "CCBS erase call linkage ID notification" },
    { 0x800e, "CCBS status notification" },
    { 0x800f, "CCBS remote user free notification" },
    { 0x8010, "CCBS B-free notification" },
    { 0x8011, "CCBS erase notification" },
    { 0x8012, "CCBS stop alerting notification" },
    { 0x8013, "CCBS info retain notification" },
    { 0x8014, "MWI indication" },
    { 0x8015, "CCNR info retain notification" },
    { 0x8016, "CONF partyDISC" },
    { 0x8017, "CONF notification" },
    { 0xffff, "reserved" }
};
INFO_TABLE FacilityInfo[] =
{
    { 0x0000, "DTMF command initiated" },
    { 0x0001, "incorrect DTMF digit" },
    { 0x0002, "unknown  DTMF request" },
    { 0xffff, "unknown value" }
};
INFO_TABLE32 InfoMask[] =
{
    { 0x00000001L, "cause" },
    { 0x00000002L, "date/time" },
    { 0x00000004L, "display" },
    { 0x00000008L, "user-user info" },
    { 0x00000010L, "call progression" },
    { 0x00000020L, "facility" },
    { 0x00000040L, "charging" },
    { 0x00000080L, "called party number" },
    { 0x00000100L, "channel identification" },
    { 0x00000200L, "early B3 connect" },
    { 0x00000400L, "redirected/redirecting number" },
    { 0x00001000L, "sending complete" },
    { 0xffffffffL, "reserved" }
};
INFO_TABLE32 CipMask[] =
{
    { 0x00000001L, "no predefined profile" },
    { 0x00000002L, "speech" },
    { 0x00000004L, "unrestricted digital information" },
    { 0x00000008L, "restricted digital information" },
    { 0x00000010L, "3.1 kHz audio" },
    { 0x00000020L, "7 kHz audio" },
    { 0x00000040L, "video" },
    { 0x00000080L, "packet mode" },
    { 0x00000100L, "56 kBit/s rate adaption" },
    { 0x00000200L, "unrestricted digital information with tones" },
    { 0x00010000L, "telephony" },
    { 0x00020000L, "fax group 2/3" },
    { 0x00040000L, "fax group 4 class 1" },
    { 0x00080000L, "teletex basic/mixed and fax group 4 class 2/3" },
    { 0x00100000L, "teletex basic/processable" },
    { 0x00200000L, "teletex basic" },
    { 0x00400000L, "interworking for videotex" },
    { 0x00800000L, "telex" },
    { 0x01000000L, "message handling systems X.400" },
    { 0x02000000L, "OSI application X.200" },
    { 0x04000000L, "7 kHz telephony" },
    { 0x08000000L, "video telephony F.721, first connection" },
    { 0x10000000L, "video telephony F.721, second connection" },
    { 0xffffffffL, "reserved" }
};
INFO_TABLE32 CipMask2[] =
{
    { 0xffffffffL, "reserved" }
};
INFO_TABLE Event[] =
{
    { 0x0001, "alerting" },
    { 0x0002, "call proceeding" },
    { 0x0007, "connect" },
    { 0x000f, "connect acknowledge" },
    { 0x0003, "progress" },
    { 0x0005, "setup" },
    { 0x000d, "setup acknowledge" },
    { 0x0026, "resume" },
    { 0x002e, "resume acknowledge" },
    { 0x0022, "resume reject" },
    { 0x0025, "suspend" },
    { 0x002d, "suspend acknowledge" },
    { 0x0021, "suspend reject" },
    { 0x0020, "user information" },
    { 0x0045, "disconnect" },
    { 0x004d, "release" },
    { 0x005a, "release complete" },
    { 0x0046, "restart" },
    { 0x004e, "restart acknowledge" },
    { 0x0060, "segment" },
    { 0x0079, "congestion control" },
    { 0x007b, "information" },
    { 0x0062, "facility" },
    { 0x006e, "notify" },
    { 0x007d, "status" },
    { 0x0075, "status enquiry" },
    { 0xffff, "unknown value" }
};
INFO_TABLE InfoElement[] =
{
    { 0x00a1, "Sending complete" },
    { 0x00a0, "MORE" },
    { 0x00b0, "Congestion level" },
    { 0x00d0, "Repeat indicator" },
    { 0x0000, "SMSG" },
    { 0x0004, "Bearer Capability" },
    { 0x0008, "Cause" },
    { 0x000c, "Connected Address" },
    { 0x0010, "Call Identity" },
    { 0x0014, "Call State" },
    { 0x0018, "Channel Id" },
    { 0x001a, "Advice of Charge" },
    { 0x001c, "Facility" },
    { 0x001e, "Progress Indicator" },
    { 0x0020, "NSF" },
    { 0x0027, "Notify Indicator" },
    { 0x0028, "Display" },
    { 0x0029, "Date" },
    { 0x002c, "Key " },
    { 0x0034, "Signal" },
    { 0x003a, "SPID" },
    { 0x003b, "Endpoint Id"  },
    { 0x004c, "Connected Number" },
    { 0x006c, "Calling Party Number" },
    { 0x006d, "Calling Party Subaddress" },
    { 0x0070, "Called Party Number" },
    { 0x0071, "Called Party Subaddress" },
    { 0x0074, "Redirecting Number" },
    { 0x0076, "Redirection Number" },
    { 0x0078, "TNET" },
    { 0x0079, "Restart Indicator" },
    { 0x007c, "LLC" },
    { 0x007d, "HLC" },
    { 0x007e, "User User Info" },
    { 0x007f, "ESC" },
    { 0xffff, "" }
};
INFO_TABLE SendingCompleteMode[] =
{
    { 0x0000, "do not add sending complete" },
    { 0x0001, "add sending complete" },
    { 0xffff, "reserved" }
};
INFO_TABLE SpeedNegotiation[] =
{
    { 0x0000, "none" },
    { 0x0001, "within class of modulation" },
    { 0x0002, "V.100" },
    { 0x0003, "V.8" },
    { 0xffff, "reserved" }
};
INFO_TABLE ModemOptions[] =
{
    { 0x0001, "disable V.42/V.42bis" },
    { 0x0002, "disable MNP4/MNP5" },
    { 0x0004, "disable transparent mode" },
    { 0x0008, "disable V.42 negotiation" },
    { 0x0010, "disable compression" },
    { 0xffff, "reserved" }
};
INFO_TABLE FaxOptions[] =
{
    { 0x0001, "enable high resolution" },
    { 0x0002, "accept faxpolling requests" },
    { 0x0400, "enable JPEG negotiation" },
    { 0x0800, "enable JBIG (T.43)" },
    { 0x1000, "do not use JBIG progr." },
    { 0x2000, "do not use MR" },
    { 0x4000, "do not use MMR" },
    { 0x8000, "do not use ECM" },
    { 0xffff, "reserved" }
};
INFO_TABLE ModemNcpiOptions[] =
{
    { 0x0001, "V.42/V.42bis" },
    { 0x0002, "MNP4/MNP5" },
    { 0x0004, "transparent mode" },
    { 0x0010, "compression" },
    { 0xffff, "reserved" }
};
INFO_TABLE FaxNcpiOptions[] =
{
    { 0x0001, "enable high resolution" },
    { 0x0002, "faxpolling request/indication" },
    { 0x0004, "poll one more document" },
    { 0x0400, "JPEG connection" },
    { 0x0800, "JBIG connection" },
    { 0x1000, "JBIG progr. used" },
    { 0x2000, "MR compression used" },
    { 0x4000, "MMR compression used" },
    { 0x8000, "not an ECM connection" },
    { 0xffff, "reserved" }
};
INFO_TABLE PortSelect[] =
{
    { 0x0000, "suspend" },
    { 0x0001, "resume" },
    { 0xffff, "reserved" }
};
INFO_TABLE Parity[] =
{
    { 0x0000, "none" },
    { 0x0001, "odd" },
    { 0x0002, "even" },
    { 0xffff, "reserved" }
};
INFO_TABLE32 NotificationMask[] =
{
    { 0x00000001, "Hold/Retrieve" },
    { 0x00000002, "Terminal portability" },
    { 0x00000004, "ECT/CT" },
    { 0x00000008, "3PTY" },
    { 0x00000010, "Call Forwarding/Call Deflection" },
    { 0x00000020, "to be ignored" },
    { 0x00000080, "CCBS" },
    { 0x00000100, "MWI Message Waiting Indication" },
    { 0x00000200, "CCNR" },
    { 0x00000400, "CONF" },
    { 0xffffffff, "unknown value" }
};
INFO_TABLE ForwardingType[] =
{
    { 0x0000, "CFU" },
    { 0x0001, "CFB" },
    { 0x0002, "CFNR" },
    { 0xffff, "reserved" }
};
INFO_TABLE PresentationType[] =
{
    { 0x0000, "Display of own address not allowed" },
    { 0x0001, "Display of own address allowed" },
    { 0xffff, "reserved" }
};
INFO_TABLE32 SupportedServices[] =
{
    { 0x00000001, "Hold/Retrieve" },
    { 0x00000002, "Terminal portability" },
    { 0x00000004, "ECT/CT" },
    { 0x00000008, "3PTY" },
    { 0x00000010, "Call Forwarding" },
    { 0x00000020, "Call Deflection" },
    { 0x00000040, "MCID" },
    { 0x00000080, "CCBS" },
    { 0x00000100, "MWI Message Waiting Indication" },
    { 0x00000200, "CCNR" },
    { 0x00000400, "CONF" },
    { 0xffffffff, "unknown value" }
};
INFO_TABLE DiversionReason[] =
{
    { 0x0000, "unknown" },
    { 0x0001, "CFU" },
    { 0x0002, "CFB" },
    { 0x0003, "CFNR" },
    { 0x0004, "CD alerting" },
    { 0x0005, "CD immediate" },
    { 0xffff, "reserved" }
};
INFO_TABLE GlobalConfig[] =
{
    { 0x0000, "default" },
    { 0x0001, "DTE, originate" },
    { 0x0002, "DCE, answer" },
    { 0xffff, "unknown value" }
};
INFO_TABLE EchoCancellerRequestFunction[] =
{
    { 0x0000, "Get supported services" },
    { 0x0001, "Enable line echo canceller operation" },
    { 0x0002, "Disable line echo canceller operation" },
    { 0xffff, "reserved" }
};
INFO_TABLE EchoCancellerIndicationFunction[] =
{
    { 0x0000, "reserved" },
    { 0x0001, "Bypass indication" },
    { 0xffff, "reserved" }
};
INFO_TABLE32 EcSupportedOptionsMask[] =
{
    { 0x00000001, "NLP" },
    { 0x00000002, "Bypass on any 2100 Hz" },
    { 0x00000004, "Bypass on phase reversed 2100 Hz" },
    { 0x00000008, "Adaptive pre-delay" },
    { 0xffffffff, "unknown value" }
};
INFO_TABLE32 EcOptionsMask[] =
{
    { 0x00000001, "Enable NLP" },
    { 0x00000002, "Bypass on continuous 2100 Hz" },
    { 0x00000004, "Bypass on phase reversed 2100 Hz" },
    { 0x00000008, "Adaptive pre-delay" },
    { 0xffffffff, "unknown value" }
};
INFO_TABLE EcBypassEvent[] =
{
    { 0x0000, "Bypass due to continuous 2100 Hz" },
    { 0x0001, "Bypass due to phase-reversed 2100 Hz" },
    { 0x0002, "Bypass released" },
    { 0xffff, "reserved" }
};
INFO_TABLE LineInterconnectFunction[] =
{
    { 0x0000, "Get Supported Services" },
    { 0x0001, "Connect" },
    { 0x0002, "Disconnect" },
    { 0xffff, "reserved" }
};
INFO_TABLE LiOldServiceReason[] =
{
    { 0x0000, "User initiated" },
    { 0x3805, "Line no longer available" },
    { 0x3806, "Interconnect not established" },
    { 0x3807, "Lines not compatible" },
    { 0xffff, "unknown value" }
};
INFO_TABLE32 LiOldSupportedServices[] =
{
    { 0x00000001, "Interconnect" },
    { 0x00000002, "Monitoring" },
    { 0x00000004, "Announcements" },
    { 0x00000008, "Mixing" },
    { 0x00000010, "Cross controller" },
    { 0xffffffff, "unknown value" }
};
INFO_TABLE32 LiOldInterconnectMask[] =
{
    { 0x00000001, "Conf. A->B" },
    { 0x00000002, "Conf. B->A" },
    { 0x00000004, "Monitor A" },
    { 0x00000008, "Monitor B" },
    { 0x00000010, "Announcement A" },
    { 0x00000020, "Announcement B" },
    { 0x00000040, "Mix A" },
    { 0x00000080, "Mix B" },
    { 0xffffffff, "unknown value" }
};
INFO_TABLE LiServiceReason[] =
{
    { 0x0000, "User initiated" },
    { 0x3800, "PLCI has no B-channel" },
    { 0x3801, "Lines not compatible" },
    { 0x3802, "PLCI's are not in any or same interconnection" },
    { 0xffff, "unknown value" }
};
INFO_TABLE32 LiMainInterconnectMask[] =
{
    { 0x00000001, "-" },
    { 0x00000002, "-" },
    { 0x00000004, "Monitor A" },
    { 0x00000008, "Mix A" },
    { 0x00000010, "Monitor X" },
    { 0x00000020, "Mix X" },
    { 0x00000040, "Loop A" },
    { 0x00000080, "Loop data" },
    { 0x00000100, "Loop X" },
    { 0xffffffff, "unknown value" }
};
INFO_TABLE32 LiPartInterconnectMask[] =
{
    { 0x00000001, "Conn. A->B" },
    { 0x00000002, "Conn. B->A" },
    { 0x00000004, "Monitor B" },
    { 0x00000008, "Mix B" },
    { 0x00000010, "Monitor X" },
    { 0x00000020, "Mix X" },
    { 0x00000040, "Loop B" },
    { 0x00000080, "Loop data" },
    { 0x00000100, "Loop X" },
    { 0xffffffff, "unknown value" }
};
INFO_TABLE32 LiSupportedServices[] =
{
    { 0x00000001, "Cross controller" },
    { 0x00000002, "Asymetric connects" },
    { 0x00000004, "Monitoring" },
    { 0x00000008, "Mixing" },
    { 0x00000010, "Remote monitoring" },
    { 0x00000020, "Remote mixing" },
    { 0x00000040, "Line loop" },
    { 0x00000080, "Data loop" },
    { 0x00000100, "X loop" },
    { 0xffffffff, "unknown value" }
};
INFO_TABLE ManufacturerCommand[] =
{
    { 0x0001, "Assign PLCI" },
    { 0x0002, "Advanced Codec control" },
    { 0x0003, "DSP control" },
    { 0x0004, "Signaling control" },
    { 0x0005, "RXT control" },
    { 0x0006, "IDI control" },
    { 0x0007, "Configuration control" },
    { 0x0008, "Remove Codec" },
    { 0x0009, "Options request" },
    { 0x000a, "reserved" },
    { 0x000b, "Negotiate B3" },
    { 0x000c, "Info B3" },
    { 0x0012, "_DI_MANAGEMENT_INFO" },
    { 0xffff, "unknown value" }
};
INFO_TABLE ManufacturerAssignPLCIHardwareResource[] =
{
    { 0x0000, "B channel layer 1" },
    { 0x0001, "Codec" },
    { 0x0002, "Advanced codec" },
    { 0x0003, "Line side" },
    { 0xffff, "unknown value" }
};
INFO_TABLE ManufacturerAssignPLCIUsage[] =
{
    { 0x0000, "listen" },
    { 0x0001, "connect" },
    { 0x0002, "no connect" },
    { 0xffff, "unknown value" }
};
INFO_TABLE B1_VoIP_Payloads[] =
{
    {  0, "PCMU  G.711, u-law coded PCM" },
    {  1, "1016  CELP according to FED-STD 1016" },
    {  2, "G726  G.726, 32 kBit/s" },
    {  3, "GSM   GSM 06.10 / ETS 300 961 full rate" },
    {  4, "G723  G.723.1" },
    {  5, "DVI4" },
    {  6, "DVI4  16000 samples/s" },
    {  7, "LPC" },
    {  8, "PCMA  G.711, A-law coded PCM" },
    {  9, "G722  G.722, 16000 samples/s" },
    { 12, "QCELP TIA IS-733" },
    { 15, "G728  G.728" },
    { 18, "G729  G.729" },
    { 28, "AMR-NR, AMR NB dyn" },
    { 29, "T38	dyn, reserved" },
    { 30, "GSM-HR dyn, GSM 06.20 / ETS 300 969 half rate" },
    { 31, "GSM-EFR dyn, GSM 06.60 / ETS 300 969 en. full rate" },
    { 32, "Additional, RED dyn, redundancy scheme (RFC 2198)" },
    { 33, "Additional, CN, PT 19, Comfort noise" },
    { 34, "Additional, DTMF dyn, DTMF as redundant data (RFC 2833)" },
    { 35, "Additional, FEC dyn, forward error correction (RFC 2733)" },
    { 0xffff, "reserved" }
};
INFO_TABLE B1_VoIP_PayloadOptionsG723[] =
{
    { 0, "Disable voice activity detection" },
    { 1, "Disable comfort noise generator" },
    { 2, "Disable DTMF detection" },
    { 3, "Disable DTMF generation" },
    { 4, "Disable adaptive post-filter" },
    { 8, "G.723.1 low coding rate" },
    { 0xffff, "reserved" }
};
INFO_TABLE B1_VoIP_PayloadOptionsG729[] =
{
    { 0, "Disable voice activity detection" },
    { 1, "Disable comfort noise generator" },
    { 2, "Disable DTMF detection" },
    { 3, "Disable DTMF generation" },
    { 0xffff, "reserved" }
};
INFO_TABLE B1_VoIP_PayloadOptionsAMRNB[] =
{
    { 0, "Disable voice activity detection" },
    { 2, "Disable DTMF detection" },
    { 3, "Disable DTMF generation" },
    { 0xffff, "reserved" }
};
INFO_TABLE B1_VoIP_PayloadOptionsAMRNB_amrOptions_Initial_mode_for_receive_direction[] =
{
 { 0, "4.75 kBit/s" },
 { 1, "5.15 kBit/s" },
 { 2, "5.9 kBit/s" },
 { 3, "6.7 kBit/s" },
 { 4, "7.4 kBit/s" },
 { 5, "7.95 kBit/s" },
 { 6, "10.2 kBit/s" },
 { 7, "12.2 kBit/s" },
 { 0xffff, "reserved" }
};
INFO_TABLE B1_VoIP_PayloadOptionsAMRNB_amrOptions[] =
{
  { 0x0100, "Octet alignment for transmit packets" },
  { 0x0200, "Octet alignment for receive packets" },
  { 0x0400, "CRC for transmit packets" },
  { 0x0800, "CRC for receive packets" },
  { 0x1000, "Robust sorting for transmit packets" },
  { 0x2000, "Robust sorting for receive packets" },
  { 0x4000, "Interleaving for transmit packets" },
  { 0x8000, "Storage format for transmit and receive packets" },
	{ 0xffff, "reserved" }
};
INFO_TABLE B1_VoIP_PayloadOptionsAMRNB_amrModeSet[] =
{
  { 0x0001, "4.75 kBit/s" },
  { 0x0002, "5.15 kBit/s" },
  { 0x0004, "5.9 kBit/s" },
  { 0x0008, "6.7 kBit/s" },
  { 0x0010, "7.4 kBit/s" },
  { 0x0020, "7.95 kBit/s" },
  { 0x0040, "10.2 kBit/s" },
  { 0x0080, "12.2 kBit/s" },
  { 0x8000, "mode change" },
	{ 0xffff, "reserved" }
};
INFO_TABLE B1_VoIP_PayloadOptions[] =
{
    { 0, "Disable voice activity detection (if applicable)" },
    { 1, "Disable comfort noise generator (if applicable)" },
    { 2, "Disable DTMF detection" },
    { 3, "Disable DTMF generation" },
    { 0xffff, "reserved" }
};
INFO_TABLE B3_VoIP_Encapsulation[] =
{
    { 0, "none (UDP)" },
    { 1, "UDP" },
    { 2, "IP+UDP" },
    { 0xffff, "unknown" }
};
INFO_TABLE32 VoIP_NCPI_ConnectOptions[] =
{
    {  0, "Disconnect B3 on SSRC change" },
    {  1, "Disconnect B3 on payload type change" },
    {  2, "Disconnect B3 on unknown payload type" },
    { 16, "Generate silence information packets" },
    { 0xffffffff, "unknown" }
};
INFO_TABLE B1_VoIP_PayloadOptionsCN[] =
{
    { 0, "Disable voice activity detection" },
    { 1, "Disable comfort noise generator (if applicable)" },
    { 0xffff, "reserved" }
};
INFO_TABLE B1_VoIP_PayloadOptionsDTMF[] =
{
    { 2, "Disable DTMF detection" },
    { 3, "Disable DTMF generation" },
    { 8, "Suppress DTMF in audio signal" },
    { 0xffff, "reserved" }
};
INFO_TABLE _DI_MANAGEMENT_INFO_cmd [] =
{
    { 1, "_DI_MANAGEMENT_INFO_IDENTIFY" },
    { 2, "_DI_MANAGEMENT_INFO_READ" },
    { 0xffff, "unknown" }
};
INFO_TABLE _DI_MANAGEMENT_INFO_read_ie [] =
{
    { 1, "_DI_MANAGEMENT_INFO_READ_LOGICAL_ADAPTER_NUMBER_IE"},
    { 2, "_DI_MANAGEMENT_INFO_READ_VSCAPI_IE                "},
    { 3, "_DI_MANAGEMENT_INFO_READ_ADAPTER_SERIAL_NUMBER_IE "},
    { 4, "_DI_MANAGEMENT_INFO_READ_CARD_TYPE_IE             "},
    { 5, "_DI_MANAGEMENT_INFO_READ_CARD_ID_IE               "},
    { 0xffff, "unknown" }
};
INFO_TABLE facilityVoIP_function [] = {
  { 0x0001, "Get supported parameters" },
  { 0x0002, "Get supported payload protocols" },
  { 0x0003, "Get supported payload options" },
  { 0x0004, "Query RTCP report" },
  { 0x0005, "RTP control" },
  { 0xffff, "unknown" }
};
INFO_TABLE facilityVoIP_RTP_CONTROL_function [] = {
  { 0x0000, "AMR mode request" },
  { 0x0001, "AMR mode indication" },
  { 0xffff, "unknown" }
};
INFO_TABLE facilityVoIP_RTP_CONTROL_function_AMR_mode_request [] = {
	{ 0,  "4.75 kBit/s" },
	{ 1,  "5.15 kBit/s" },
	{ 2,  "5.9 kBit/s" },
	{ 3,  "6.7 kBit/s" },
	{ 4,  "7.4 kBit/s" },
	{ 5,  "7.95 kBit/s" },
	{ 6,  "10.2 kBit/s" },
	{ 7,  "12.2 kBit/s" },
	{ 8,  "reserved" },
	{ 9,  "reserved" },
	{ 10, "reserved" },
	{ 11, "reserved" },
	{ 12, "reserved" },
	{ 13, "reserved" },
	{ 14, "reserved" },
	{ 15, "reserved" },
  { 0xffff, "unknown" }
};
INFO_TABLE facilityVoIP_RTP_CONTROL_function_AMR_mode_indication [] = {
  { 0,  "4.75 kBit/s" },
  { 1,  "5.15 kBit/s" },
  { 2,  "5.9 kBit/s" },
  { 3,  "6.7 kBit/s" },
  { 4,  "7.4 kBit/s" },
  { 5,  "7.95 kBit/s" },
  { 6,  "10.2 kBit/s" },
  { 7,  "12.2 kBit/s" },
  { 8,  "reserved" },
  { 9,  "reserved" },
  { 10, "reserved" },
  { 11, "reserved" },
  { 12, "reserved" },
  { 13, "reserved" },
  { 14, "reserved" },
  { 15, "no mode change" },
  { 0xffff, "unknown" }
};

