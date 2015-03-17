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

#include "platform.h"  
/*****************************************************************************
 *
 * File:           dbg_capi.c
 * Component:      Maint
 * Description:    CAPI 2.0 analyser
 *
 *****************************************************************************/
#include <stdio.h>
#include <string.h>
#include "cau_q931.h"
// make "dimaint.h" working
#include "dimaint.h"
#define UNALIGNED
/******************************************************************************/
byte dbg_capi_nspi_use_b3_protocol = 0;
/******************************************************************************/
typedef struct api_parse
{
    long len;
    byte UNALIGNED * info;
} API_PARSE;
/******************************************************************************/
static void alert                 (byte subCommand, API_PARSE *data, FILE *out);
static void connect               (byte subCommand, API_PARSE *data, FILE *out);
static void connect_active        (byte subCommand, API_PARSE *data, FILE *out);
static void disconnect            (byte subCommand, API_PARSE *data, FILE *out);
static void listen                (byte subCommand, API_PARSE *data, FILE *out);
static void info                  (byte subCommand, API_PARSE *data, FILE *out);
static void select_b              (byte subCommand, API_PARSE *data, FILE *out);
static void facility              (byte subCommand, API_PARSE *data, FILE *out);
static void connect_b3            (byte subCommand, API_PARSE *data, FILE *out);
static void connect_b3_active     (byte subCommand, API_PARSE *data, FILE *out);
static void disconnect_b3         (byte subCommand, API_PARSE *data, FILE *out);
static void data_b3               (byte subCommand, API_PARSE *data, FILE *out);
static void reset_b3              (byte subCommand, API_PARSE *data, FILE *out);
static void connect_b3_t90_active (byte subCommand, API_PARSE *data, FILE *out);
static void portability           (byte subCommand, API_PARSE *data, FILE *out);
static void manufacturer          (byte subCommand, API_PARSE *data, FILE *out);
/******************************************************************************/
typedef void (*ParseInfoElementCallbackProc_t)(void* callback_context,
																							 byte Type,
																							 byte *IE,
																							 int  IElen,
																							 int SingleOctett);
static int ParseInfoElement (byte *Buf,
                             int  Buflen,
                             ParseInfoElementCallbackProc_t callback,
                             void* callback_context);
/******************************************************************************/
#include "capi_tbl.c"
#define REQ 0x80
#define CON 0x81
#define IND 0x82
#define RES 0x83
/*****************************************************************************/
static void analyseMessage (byte *data, long len, FILE *out);
static void AdditionalInfo (byte *data, long len, FILE *out, byte flag);
static void CPartyNumber (byte *data, long len, FILE *out, long screen);
static void BProtocol (byte *data, long len, FILE *out, dword plci, byte flag);
static void Ncpi (byte *data, long len, FILE *out, dword plci);
static void ModemB1Options (word Options, FILE *out);
static void SupplementaryServices (byte subCommand, byte *data, long len, FILE *out);
static void PowerManagementWakeup (byte subCommand, byte *data, long len, FILE *out);
static void LineInterconnect (byte subCommand, byte *data, long len, FILE *out);
static void EchoCanceller (byte subCommand, byte *data, long len, FILE *out);
static void facilityVoIP (byte subCommand, byte *data, long len, FILE *out);
static long apiParse (byte *msg, long len, const char *format, API_PARSE *parms);
static char* printString (INFO_TABLE *info, word value);
static char* printString32 (INFO_TABLE32 * info, dword value);
static void printHex (byte *data, long len, FILE *out);
static void printAscii (byte *data, long len, FILE *out);
static void printMask16 (INFO_TABLE *info, word value, FILE *out);
static void printMask32 (INFO_TABLE32 *info, dword value, FILE *out);
static void putAsc (FILE *out, byte *data, long addr, long len);
/*****************************************************************************/
#define PLCI_INFO_ENTRIES  0x100
typedef struct tagPlciInfo
{
    byte Next;
    byte Prev;
    byte Controller;
    byte Plci;
    byte B3Protocol;
    byte Reserved1;
    byte Reserved2;
    byte Reserved3;
} TPlciInfo;
static byte PlciInfoInitialized = FALSE;
static TPlciInfo PlciInfoTable[PLCI_INFO_ENTRIES] = { { 0 } };
static byte PlciInfoIndexTable[0x80][0x100] = { { 0 } };
#define PlciInfoIndexLookup(plci)    (PlciInfoIndexTable[(plci) & 0x7f][((plci) >> 8) & 0xff])
#define PlciInfoIndexSet(c,l,i)    PlciInfoIndexTable[c][l] = (i);
static void PlciInfoMostRecentlyUsed (dword plci)
{
    word i;
    TPlciInfo *p;
    i = PlciInfoIndexLookup (plci);
    if (i != 0)
    {
        p = &(PlciInfoTable[i]);
        PlciInfoTable[p->Next].Prev = p->Prev;
        PlciInfoTable[p->Prev].Next = p->Next;
        PlciInfoTable[PlciInfoTable[0].Next].Prev = (byte) i;
        p->Prev = 0;
        p->Next = PlciInfoTable[0].Next;
        PlciInfoTable[0].Next = (byte) i;
    }
}
static TPlciInfo *PlciInfoNew (dword plci)
{
    word i;
    TPlciInfo *p;
    if (!PlciInfoInitialized)
    {
        for (i = 0; i < PLCI_INFO_ENTRIES; i++)
        {
            p = &(PlciInfoTable[i]);
            p->Next = (byte)((i == PLCI_INFO_ENTRIES - 1) ? 0 : i + 1);
            p->Prev = (byte)((i == 0) ? PLCI_INFO_ENTRIES - 1 : i - 1);
            p->Controller = 0;
            p->Plci = 0;
        }
        PlciInfoInitialized = TRUE;
    }
    i = PlciInfoIndexLookup (plci);
    if (i != 0)
    {
        p = &(PlciInfoTable[i]);
    }
    else
    {
        i = PlciInfoTable[0].Prev;
        p = &(PlciInfoTable[i]);
        PlciInfoIndexSet (p->Controller, p->Plci, 0);
        p->Controller = (byte)(plci & 0x7f);
        p->Plci = (byte)((plci >> 8) & 0xff);
        if (p->Controller == 0)
            return (p);
        if (p->Plci != 0)
            PlciInfoIndexSet (p->Controller, p->Plci, (byte) i);
    }
    PlciInfoTable[p->Next].Prev = p->Prev;
    PlciInfoTable[p->Prev].Next = p->Next;
    PlciInfoTable[PlciInfoTable[0].Next].Prev = (byte) i;
    p->Prev = 0;
    p->Next = PlciInfoTable[0].Next;
    PlciInfoTable[0].Next = (byte) i;
    return (p);
}
static void PlciInfoFree (dword plci)
{
    word i;
    TPlciInfo *p;
    i = PlciInfoIndexLookup (plci);
    if (i != 0)
    {
        p = &(PlciInfoTable[i]);
        PlciInfoIndexSet (p->Controller, p->Plci, 0);
        p->Controller = 0;
        p->Plci = 0;
        PlciInfoTable[p->Next].Prev = p->Prev;
        PlciInfoTable[p->Prev].Next = p->Next;
        PlciInfoTable[PlciInfoTable[0].Prev].Next = (byte) i;
        p->Next = 0;
        p->Prev = PlciInfoTable[0].Prev;
        PlciInfoTable[0].Prev = (byte) i;
    }
}
static void PlciInfoIdentify (dword plci)
{
    word i, j;
    TPlciInfo *p;
    i = PlciInfoIndexLookup (plci);
    if (i != 0)
    {
        PlciInfoFree (plci);
    }
    else if (((plci & 0x7f) != 0) && (((plci >> 8) & 0xff) != 0))
    {
        i = 0;
        j = PlciInfoTable[0].Next;
        p = &(PlciInfoTable[j]);
        while (p->Controller != 0)
        {
            if ((p->Controller == (plci & 0x7f)) && (p->Plci == 0))
                i = j;
            j = PlciInfoTable[j].Next;
            p = &(PlciInfoTable[j]);
        }
        if (i != 0)
        {
            p = &(PlciInfoTable[i]);
            p->Plci = (byte)((plci >> 8) & 0xff);
            PlciInfoIndexSet (p->Controller, p->Plci, (byte) i);
            PlciInfoTable[p->Next].Prev = p->Prev;
            PlciInfoTable[p->Prev].Next = p->Next;
            PlciInfoTable[PlciInfoTable[0].Next].Prev = (byte) i;
            p->Prev = 0;
            p->Next = PlciInfoTable[0].Next;
            PlciInfoTable[0].Next = (byte) i;
        }
    }
}
static TPlciInfo *PlciInfoLookup (dword plci)
{
    word i;
    i = PlciInfoIndexLookup (plci);
    if (i == 0)
        return (((void*)0) );
    return (&(PlciInfoTable[i]));
}
/*****************************************************************************/
void
capiAnalyse (FILE *out, XLOG *msg)
{
    unsigned char *buffer;
    long i, len;
    buffer = &msg->info.l1.i[0];
    len    = msg->info.l1.length;
    // print out raw message first
    if (msg->code == 0x80)
    {
        fprintf (out, "CAPI20_PUT(%03ld)", len);
    }
    else
    {
        fprintf (out, "CAPI20_GET(%03ld)", len);
    }
    len -= 2; // len includes length field!
    for (i = 0; i < len; i += 16)
    {
        putAsc (out, &buffer[i], i, len - i);
    }
    fputs ("\n", out);
    // analyse message
    analyseMessage (buffer, len, out);
}
// --------------------------------------------------------------
static
void
analyseMessage (byte *rawdata, long len, FILE *out)
{
    long i, j;
 byte command;
 byte subcommand;
 dword plci;
 char commando_str[30];
      API_PARSE msgParms[15];
    byte    UNALIGNED   *data = (byte UNALIGNED *) rawdata;
 // check message length
    if (len < 6)
    {
        fprintf (out, "    - Bad Message (length < header)!\n");
  return;
 }
    // get a readable text for command
 command     = data[2];
 subcommand  = data[3];
    for (i = 0; (capi20_command[i].code    && (command    != capi20_command[i].code));    i++);
    for (j = 0; (capi20_subcommand[j].code && (subcommand != capi20_subcommand[j].code)); j++);
    if (capi20_command[i].code && capi20_subcommand[j].code)
    {
  sprintf (commando_str, "%s %s", capi20_command[i].name, capi20_subcommand[j].name);
    }
    else
    {
        sprintf (commando_str, "CMD 0x%02x 0x%02x", command, subcommand);
    }
    // print out the command
    fprintf (out, "  ---\n  %-24s AppID 0x%04x MsgNr 0x%04x",
        commando_str, READ_WORD(&data[0]), READ_WORD(&data[4]));
    if (capi20_command[i].code)
    {
        data += 6;
        len  -= 6;
        if (len > 0)
  {
   // check format and parse message
   memset (msgParms, 0, sizeof(msgParms));
   if (apiParse (data, len, capi20_command[i].format[subcommand & 0x000f], msgParms))
   {
    // print Controller/PLCI/NCCI value
    if (msgParms[0].info)
    {
     plci = READ_DWORD (msgParms[0].info);
     if (plci & 0xffff0000L)
     {
      fprintf (out, " NCCI: 0x%08lx\n", (unsigned long)plci);
     }
     else if (plci & 0x0000ff00L)
     {
      fprintf (out, " PLCI: 0x%08lx\n", (unsigned long)plci);
     }
     else
     {
      fprintf (out, " CNTR: 0x%08lx\n", (unsigned long)plci);
     }
     PlciInfoMostRecentlyUsed (plci);
    }
    // analyse message
    capi20_command[i].analyse (subcommand, msgParms, out);
    fputs("\n", out);
   }
   else
   {
    fprintf (out, "\n    --> wrong message format! (expected %s)\n",
     capi20_command[i].format[subcommand & 0x000f]);
   }
  }
  else
        {
            fprintf (out, "\n    - Parameters missing!\n");
   return;
        }
    }
    else
    {
        fprintf (out, "\n    --> unknown command!\n");
    }
    return;
}
// --------------------------------------------------------------
static
void
alert (byte subCommand, API_PARSE * data, FILE * out)
{
    switch (subCommand)
    {
      case REQ:
        AdditionalInfo (data[1].info, data[1].len, out, 0);
        break;
      case CON:
        fprintf (out, "    Info: %s\n", printString (Info, READ_WORD (data[1].info)));
        break;
    }
}
// --------------------------------------------------------------
static
void
connect (byte subCommand, API_PARSE * data, FILE * out)
{
    dword plci = READ_DWORD (data[0].info);
    switch (subCommand)
    {
      case REQ:
      case IND:
        fprintf (out, "    CIP value: %d (%s)\n", READ_WORD (data[1].info),
                 printString (CipValue, READ_WORD (data[1].info)));
        fprintf (out, "    Called Party Number (0x%lx): ", (unsigned long)data[2].len);
        if (data[2].len)
        {
            CPartyNumber (data[2].info, data[2].len, out, FALSE);
        }
        fprintf (out, "\n    Calling Party Number (0x%lx): ", (unsigned long)data[3].len);
        if (data[3].len)
        {
            CPartyNumber (data[3].info, data[3].len, out, TRUE);
        }
        fprintf (out, "\n    Called Party Subaddress (0x%lx): ", (unsigned long)data[4].len);
        if (data[4].len)
        {
            CPartyNumber (data[4].info, data[4].len, out, FALSE);
        }
        fprintf (out, "\n    Calling Party Subaddress (0x%lx): ", (unsigned long)data[5].len);
        if (data[5].len)
        {
            CPartyNumber (data[5].info, data[5].len, out, FALSE);
        }
        fprintf (out, "\n");
        if (subCommand == REQ)
        {
            BProtocol (data[6].info, data[6].len, out, plci, 1);
            data++;
        }
        fprintf (out, "    BC (0x%lx): ", (unsigned long)data[7].len);
        if (data[6].len)
        {
            printHex (data[6].info, data[6].len, out);
        }
        fprintf (out, "\n    LLC (0x%lx): ", (unsigned long)data[7].len);
        if (data[7].len)
        {
            printHex (data[7].info, data[7].len, out);
        }
        fprintf (out, "\n    HLC (0x%lx): ", (unsigned long)data[8].len);
        if (data[8].len)
        {
            printHex (data[8].info, data[8].len, out);
        }
        fprintf (out, "\n");
        AdditionalInfo (data[9].info, data[9].len, out, 0);
        break;
      case CON:
        fprintf (out, "    Info: %s\n", printString (Info, READ_WORD (data[1].info)));
        PlciInfoIdentify (plci);
        break;
      case RES:
        fprintf (out, "    Reject: %s\n", printString (Reject, READ_WORD (data[1].info)));
        BProtocol (data[2].info, data[2].len, out, plci, 1);
        fprintf (out, "    Connected Number (0x%lx): ", (unsigned long)data[3].len);
        if (data[3].len)
        {
            CPartyNumber (data[3].info, data[3].len, out, TRUE);
        }
        fprintf (out, "\n    Connected Subaddress (0x%lx): ", (unsigned long)data[4].len);
        if (data[4].len)
        {
            CPartyNumber (data[4].info, data[4].len, out, FALSE);
        }
        fprintf (out, "\n    LLC (0x%lx): ", (unsigned long)data[5].len);
        if (data[5].len)
        {
            printHex (data[5].info, data[5].len, out);
        }
        fprintf (out, "\n");
        AdditionalInfo (data[6].info, data[6].len, out, 0);
        break;
    }
}
// --------------------------------------------------------------
static
void
connect_active (byte subCommand, API_PARSE * data, FILE * out)
{
    switch (subCommand)
    {
      case IND:
        fprintf (out, "    Connected Number (0x%lx): ", (unsigned long)data[1].len);
        if (data[1].len)
        {
            CPartyNumber (data[1].info, data[1].len, out, TRUE);
        }
        fprintf (out, "\n    Connected Subaddress (0x%lx): ", (unsigned long)data[2].len);
        if (data[2].len)
        {
            CPartyNumber (data[2].info, data[2].len, out, FALSE);
        }
        fprintf (out, "\n    LLC (0x%lx): ", (unsigned long)data[3].len);
        if (data[3].len)
        {
            printHex (data[3].info, data[3].len, out);
        }
        fprintf (out, "\n");
        break;
      case RES:
        break;
    }
}
// --------------------------------------------------------------
static
void
connect_b3_active (byte subCommand, API_PARSE * data, FILE * out)
{
    dword plci = READ_DWORD (data[0].info);
    switch (subCommand)
    {
      case IND:
        Ncpi (data[1].info, data[1].len, out, plci);
        break;
      case RES:
        break;
    }
}
// --------------------------------------------------------------
static
void
connect_b3 (byte subCommand, API_PARSE * data, FILE * out)
{
    dword plci = READ_DWORD (data[0].info);
    switch (subCommand)
    {
      case REQ:
        Ncpi (data[1].info, data[1].len, out, plci);
        break;
      case CON:
        fprintf (out, "    Info: %s\n", printString (Info, READ_WORD (data[1].info)));
        break;
      case IND:
        Ncpi (data[1].info, data[1].len, out, plci);
        break;
      case RES:
        fprintf (out, "    Reject: %s\n", printString (Reject, READ_WORD (data[1].info)));
        Ncpi (data[2].info, data[2].len, out, plci);
        break;
    }
}
// --------------------------------------------------------------
static
void
connect_b3_t90_active (byte subCommand, API_PARSE * data, FILE * out)
{
    dword plci = READ_DWORD (data[0].info);
    switch (subCommand)
    {
      case IND:
        Ncpi (data[1].info, data[1].len, out, plci);
        break;
      case RES:
        break;
    }
}
// --------------------------------------------------------------
static
void
data_b3 (byte subCommand, API_PARSE * data, FILE * out)
{
    switch (subCommand)
    {
      case REQ:
        fprintf (out, "    Data: 0x%08lx\n", (unsigned long)READ_DWORD (data[1].info));
        fprintf (out, "    Data length: %d\n", READ_WORD (data[2].info));
        fprintf (out, "    Data handle: 0x%04x\n", READ_WORD (data[3].info));
        fprintf (out, "    Flags: 0x%x ", READ_WORD (data[4].info));
        printMask16 (DataReqFlags, READ_WORD (data[4].info), out);
        fprintf (out, "\n");
        if (READ_DWORD (data[1].info) == 0)
        {
            if (data[5].len >= 8)
            {
                fprintf (out, "    Data64: 0x%08lx%08lx\n",
                         (unsigned long)READ_DWORD (data[5].info + sizeof(dword)),
                         (unsigned long)READ_DWORD (data[5].info));
            }
            else
            {
                fprintf (out, "      --> wrong format! (expected ddwwwq)\n");
            }
        }
        break;
      case CON:
        fprintf (out, "    Data handle: 0x%04x\n", READ_WORD (data[1].info));
        fprintf (out, "    Info: %s\n", printString (Info, READ_WORD (data[2].info)));
        break;
      case IND:
        fprintf (out, "    Data: 0x%08lx\n", (unsigned long)READ_DWORD (data[1].info));
        fprintf (out, "    Data length: %d\n", READ_WORD (data[2].info));
        fprintf (out, "    Data handle: 0x%04x\n", READ_WORD (data[3].info));
        fprintf (out, "    Flags: 0x%x ", READ_WORD (data[4].info));
        printMask16 (DataIndFlags, READ_WORD (data[4].info), out);
        fprintf (out, "\n");
        if (READ_DWORD (data[1].info) == 0)
        {
            if (data[5].len >= 8)
            {
                fprintf (out, "    Data64: 0x%08lx%08lx\n",
                         (unsigned long)READ_DWORD (data[5].info + sizeof(dword)),
                         (unsigned long)READ_DWORD (data[5].info));
            }
            else
            {
                fprintf (out, "      --> wrong format! (expected ddwwwq)\n");
            }
        }
        break;
      case RES:
        fprintf (out, "    Data handle: 0x%04x\n", READ_WORD (data[1].info));
        break;
    }
}
// --------------------------------------------------------------
static
void
disconnect_b3 (byte subCommand, API_PARSE * data, FILE * out)
{
    dword plci = READ_DWORD (data[0].info);
    switch (subCommand)
    {
      case REQ:
        Ncpi (data[1].info, data[1].len, out, plci);
        break;
      case CON:
        fprintf (out, "    Info: %s\n", printString (Info, READ_WORD (data[1].info)));
        break;
      case IND:
        fprintf (out, "    Reason_B3: %s\n", printString (ReasonB3, READ_WORD (data[1].info)));
        Ncpi (data[2].info, data[2].len, out, plci);
        break;
      case RES:
        break;
    }
}
// --------------------------------------------------------------
static
void
disconnect (byte subCommand, API_PARSE * data, FILE * out)
{
    dword plci = READ_DWORD (data[0].info);
    word reason;
    switch (subCommand)
    {
      case REQ:
        AdditionalInfo (data[1].info, data[1].len, out, 0);
        break;
      case CON:
        fprintf (out, "    Info: %s\n", printString (Info, READ_WORD (data[1].info)));
        break;
      case IND:
        reason = READ_WORD (data[1].info);
        if ((reason & 0xff00) == 0x3400)
        {
            fprintf (out, "    Reason: %s\n", cau_q931[reason & 0x007f]);
        }
        else
        {
            fprintf (out, "    Reason: %s\n", printString (Reason, reason));
        }
        PlciInfoFree (plci);
        break;
      case RES:
        break;
    }
}
// --------------------------------------------------------------
static
void
facility (byte subCommand, API_PARSE * data, FILE * out)
{
    word facSelect;
      API_PARSE parms[10];
      API_PARSE parms2[4];
    switch (subCommand)
    {
      case REQ:
        facSelect = READ_WORD (data[1].info);
        fprintf (out, "    Facility selector: 0x%x %s\n", facSelect, printString (Facility, facSelect));
        fprintf (out, "    Facility parameter (0x%lx): ", (unsigned long)data[2].len);
        if (data[2].len)
        {
            switch (facSelect)
            {
              case 1:
                if (apiParse (data[2].info, data[2].len, "wwwsr", parms))
                {
                    fprintf (out, "\n      Function: %s\n", printString (FacilityFunction, READ_WORD (parms[0].info)));
                    fprintf (out, "      Tone-Duration: %d ms\n", READ_WORD (parms[1].info));
                    fprintf (out, "      Gap-Duration: %d ms\n", READ_WORD (parms[2].info));
                    fprintf (out, "      DTMF-Digits (0x%lx): ", (unsigned long)parms[3].len);
                    if (parms[3].len)
                    {
                        printAscii (parms[3].info, parms[3].len, out);
                    }
                    fprintf (out, "\n");
                    if (parms[4].len > 0)
                    {
                        if (apiParse (parms[4].info, parms[4].len, "s", parms))
                        {
                            fprintf (out, "      DTMF-Characteristics (0x%lx):\n",(unsigned long)parms[0].len);
                            if (parms[0].len)
                            {
                                if (apiParse (parms[0].info, parms[0].len, "w", parms2))
                                {
                                    fprintf (out, "        DTMF Selectivity: %d\n", READ_WORD (parms2[0].info));
                                }
                                else
                                {
                                    fprintf (out, "      --> wrong format! (expected w)\n");
                                }
                            }
                        }
                        else
                        {
                            fprintf (out, "      --> wrong format! (expected s)\n");
                        }
                    }
                }
                else if (apiParse (data[2].info, data[2].len, "w", parms))
                {
                    fprintf (out, "\n      Function: %s\n", printString (FacilityFunction, READ_WORD (parms[0].info)));
  }
                else
                {
                    fprintf (out, "      --> wrong format! (expected wwws)\n");
                }
                break;
              case 2:
                if (apiParse (data[2].info, data[2].len, "w", parms))
                {
                    fprintf (out, "\n      Function: %s\n", printString (FacilityFunction, READ_WORD (parms[0].info)));
                }
                else
                {
                    fprintf (out, "      --> wrong format! (expected w)\n");
                }
                break;
              case 3:
                SupplementaryServices (subCommand, data[2].info, data[2].len, out);
                break;
              case 4:
                PowerManagementWakeup (subCommand, data[2].info, data[2].len, out);
                break;
              case 5:
                LineInterconnect (subCommand, data[2].info, data[2].len, out);
                break;
              case 6:
                EchoCanceller (subCommand, data[2].info, data[2].len, out);
                break;
              case 8:
                EchoCanceller (subCommand, data[2].info, data[2].len, out);
                break;
              case 0xfe:
                facilityVoIP (subCommand, data[2].info, data[2].len, out);
                break;
              default:
                printHex (data[2].info, data[2].len, out);
                fprintf (out, "\n");
                break;
            }
        }
        else
        {
            fprintf (out, "\n");
        }
        break;
      case CON:
        fprintf (out, "    Info: %s\n", printString (Info, READ_WORD (data[1].info)));
        facSelect = READ_WORD (data[2].info);
        fprintf (out, "    Facility selector: 0x%x %s\n", facSelect, printString (Facility, facSelect));
        fprintf (out, "    Facility parameter (0x%lx): ", (unsigned long)data[3].len);
        if (data[3].len)
        {
            switch (facSelect)
            {
              case 1:
                if (apiParse (data[3].info, data[3].len, "w", parms))
                {
                    fprintf (out, "\n      DTMF information: %s\n", printString (FacilityInfo, READ_WORD (parms[0].info)));
                }
                else
                {
                    fprintf (out, "      --> wrong format! (expected w)\n");
                }
                break;
              case 2:
                if (apiParse (data[3].info, data[3].len, "wwwwdddd", parms))
                {
                    fprintf (out, "\n      V.42bis info: 0x%x\n", READ_WORD (parms[0].info));
                    fprintf (out, "      Compression mode: 0x%x\n", READ_WORD (parms[1].info));
                    fprintf (out, "      Code words: 0x%x\n", READ_WORD (parms[2].info));
                    fprintf (out, "      Max string length: 0x%x\n", READ_WORD (parms[3].info));
                    fprintf (out, "      Tx total: 0x%lx\n", (unsigned long)READ_DWORD (parms[4].info));
                    fprintf (out, "      Tx compressed: 0x%lx\n", (unsigned long)READ_DWORD (parms[5].info));
                    fprintf (out, "      Rx total: 0x%lx\n", (unsigned long)READ_DWORD (parms[6].info));
                    fprintf (out, "      Rx uncompressed: 0x%lx\n", (unsigned long)READ_DWORD (parms[7].info));
                }
                else
                {
                    fprintf (out, "      --> wrong format! (expected wwwwdddd)\n");
                }
                break;
              case 3:
                SupplementaryServices (subCommand, data[3].info, data[3].len, out);
                break;
              case 4:
                PowerManagementWakeup (subCommand, data[3].info, data[3].len, out);
                break;
              case 5:
                LineInterconnect (subCommand, data[3].info, data[3].len, out);
                break;
              case 6:
                EchoCanceller (subCommand, data[3].info, data[3].len, out);
                break;
              case 8:
                EchoCanceller (subCommand, data[3].info, data[3].len, out);
                break;
              case 0xfe:
                facilityVoIP (subCommand, data[3].info, data[3].len, out);
                break;
              default:
                printHex (data[3].info, data[3].len, out);
                fprintf (out, "\n");
                break;
            }
        }
        else
        {
            fprintf (out, "\n");
        }
        break;
      case IND:
        facSelect = READ_WORD (data[1].info);
        fprintf (out, "    Facility selector: 0x%x %s\n", facSelect, printString (Facility, facSelect));
        fprintf (out, "    Facility parameter (0x%lx): ", (unsigned long)data[2].len);
        if (data[2].len)
        {
            switch (facSelect)
            {
              case 0:
                fprintf (out, "\n      Handset digits: ");
                printAscii (data[2].info, data[2].len, out);
                fprintf (out, "\n");
                break;
              case 1:
                fprintf (out, "\n      DTMF digits: ");
                printAscii (data[2].info, data[2].len, out);
                fprintf (out, "\n");
                break;
              case 3:
                SupplementaryServices (subCommand, data[2].info, data[2].len, out);
                break;
              case 5:
                LineInterconnect (subCommand, data[2].info, data[2].len, out);
                break;
              case 6:
                EchoCanceller (subCommand, data[2].info, data[2].len, out);
                break;
              case 8:
                EchoCanceller (subCommand, data[2].info, data[2].len, out);
                break;
              default:
                printHex (data[2].info, data[2].len, out);
                fprintf (out, "\n");
                break;
            }
        }
        else
        {
            fprintf (out, "\n");
        }
        break;
      case RES:
        facSelect = READ_WORD (data[1].info);
        fprintf (out, "    Facility selector: 0x%x %s\n", facSelect, printString (Facility, facSelect));
        fprintf (out, "    Facility parameter (0x%lx): ", (unsigned long)data[2].len);
        if (data[2].len)
        {
            switch (facSelect)
            {
              case 3:
                SupplementaryServices (subCommand, data[2].info, data[2].len, out);
                break;
              default:
                printHex (data[2].info, data[2].len, out);
                fprintf (out, "\n");
                break;
            }
        }
        else
        {
            fprintf (out, "\n");
        }
        break;
    }
}
// --------------------------------------------------------------
static
void
SupplementaryServices (byte subCommand, byte * data, long len, FILE * out)
{
      API_PARSE parms1[5];
      API_PARSE parms2[10];
//    STATIC API_PARSE parms3[5];
      API_PARSE parms3[8];
    word function;
    int i;
    if (apiParse (data, len, "ws", parms1))
    {
        function = READ_WORD (parms1[0].info);
        fprintf (out, "\n      Function: %s\n", printString (SupServiceFunction, function));
        fprintf (out, "      Supplementary Service parameter (0x%lx):\n", parms1[1].len);
        if (parms1[1].len)
        {
            switch (subCommand)
            {
              case REQ:
                switch (function)
                {
                  case 0x0001:
                    if (apiParse (parms1[1].info, parms1[1].len, "d", parms2))
                    {
                        fprintf (out, "        Notification mask: 0x%lx ", (unsigned long)READ_DWORD (parms2[0].info));
                        printMask32 (NotificationMask, READ_DWORD (parms2[0].info), out);
                        fprintf (out, "\n");
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected d)\n");
                    }
                    break;
                  case 0x0004:
                  case 0x0005:
                    if (apiParse (parms1[1].info, parms1[1].len, "s", parms2))
                    {
                        fprintf (out, "        Call Identity: (0x%lx): ", parms2[0].len);
                        if (parms2[0].len)
                        {
                            CPartyNumber (parms2[0].info, parms2[0].len, out, FALSE);
                        }
                        fprintf (out, "\n");
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected s)\n");
                    }
                    break;
                  case 0x0006:
                  case 0x0007:
                  case 0x0008:
                  case 0x0018:
                    if (apiParse (parms1[1].info, parms1[1].len, "d", parms2))
                    {
                        fprintf (out, "        PLCI: 0x%08lx\n", (unsigned long)READ_DWORD (parms2[0].info));
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected d)\n");
                    }
                    break;
                  case 0x0009:
                    if (apiParse (parms1[1].info, parms1[1].len, "dwwsss", parms2))
                    {
                        fprintf (out, "        Handle: 0x%lx\n", (unsigned long)READ_DWORD (parms2[0].info));
                        fprintf (out, "        Call-Forwarding type: 0x%x ", READ_WORD (parms2[1].info));
                        printMask16 (ForwardingType, READ_WORD (parms2[1].info), out);
                        fprintf (out, "\n        Basic Service: 0x%x\n", READ_WORD (parms2[2].info));
                        fprintf (out, "        Served User Number: (0x%lx): ", parms2[3].len);
                        if (parms2[3].len)
                        {
                            CPartyNumber (parms2[3].info, parms2[3].len, out, 2);
                        }
                        fprintf (out, "\n        Forwarded-to-Number: (0x%lx): ", parms2[4].len);
                        if (parms2[4].len)
                        {
                            CPartyNumber (parms2[4].info, parms2[4].len, out, 2);
                        }
                        fprintf (out, "\n        Forwarded-to-Subaddress: (0x%lx): ", parms2[5].len);
                        if (parms2[5].len)
                        {
                            CPartyNumber (parms2[5].info, parms2[5].len, out, FALSE);
                        }
                        fprintf (out, "\n");
                        if (apiParse (parms1[1].info, parms1[1].len, "dwwssss", parms2))
                        {
                            fprintf (out, "        Activating User Number: (0x%lx): ", parms2[6].len);
                            if (parms2[6].len)
                            {
                                CPartyNumber (parms2[6].info, parms2[6].len, out, 2);
                            }
                            fprintf (out, "\n");
                        }
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected dwwsss or dwwssss)\n");
                    }
                    break;
                  case 0x000a:
                  case 0x000b:
                    if (apiParse (parms1[1].info, parms1[1].len, "dwws", parms2))
                    {
                        fprintf (out, "        Handle: 0x%lx\n", (unsigned long)READ_DWORD (parms2[0].info));
                        fprintf (out, "        Call-Forwarding type: 0x%x ", READ_WORD (parms2[1].info));
                        printMask16 (ForwardingType, READ_WORD (parms2[1].info), out);
                        fprintf (out, "\n        Basic Service: 0x%x\n", READ_WORD (parms2[2].info));
                        fprintf (out, "        Served User Number: (0x%lx): ", parms2[3].len);
                        if (parms2[3].len)
                        {
                            CPartyNumber (parms2[3].info, parms2[3].len, out, 2);
                        }
                        fprintf (out, "\n");
                        if (apiParse (parms1[1].info, parms1[1].len, "dwwss", parms2))
                        {
                            if(function==0x000a) fprintf (out, "        Deactivating User Number: (0x%lx): ", parms2[4].len);
                            else fprintf (out, "        Interrogating User Number: (0x%lx): ", parms2[4].len);
                            if (parms2[4].len)
                            {
                                CPartyNumber (parms2[4].info, parms2[4].len, out, 2);
                            }
                            fprintf (out, "\n");
                        }
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected dwws or dwwss)\n");
                    }
                    break;
                  case 0x000c:
                    if (apiParse (parms1[1].info, parms1[1].len, "d", parms2))
                    {
                        fprintf (out, "        Handle: 0x%lx\n",(unsigned long)READ_DWORD (parms2[0].info));
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected d)\n");
                    }
                    break;
                  case 0x000d:
                    if (apiParse (parms1[1].info, parms1[1].len, "wss", parms2))
                    {
                        fprintf (out, "        Presentation Allowed: 0x%x ", READ_WORD (parms2[0].info));
                        printMask16 (PresentationType, READ_WORD (parms2[0].info), out);
                        fprintf (out, "\n        Deflected-to-Number: (0x%lx): ", parms2[1].len);
                        if (parms2[1].len)
                        {
                            CPartyNumber (parms2[1].info, parms2[1].len, out, 2);
                        }
                        fprintf (out, "\n        Deflected-to-Subaddress: (0x%lx): ", parms2[2].len);
                        if (parms2[2].len)
                        {
                            CPartyNumber (parms2[2].info, parms2[2].len, out, FALSE);
                        }
                        fprintf (out, "\n");
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wss)\n");
                    }
                    break;
                  case 0x000f:
                  case 0x0015:
                    if (apiParse (parms1[1].info, parms1[1].len, "dw", parms2))
                    {
                        fprintf (out, "        Handle: 0x%lx\n",(unsigned long)READ_DWORD (parms2[0].info));
                        fprintf (out, "        CCBS Call Linkage ID: 0x%x\n", READ_WORD (parms2[1].info));
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected dw)\n");
                    }
                    break;
                  case 0x0010:
                    if (apiParse (parms1[1].info, parms1[1].len, "dw", parms2))
                    {
                        fprintf (out, "        Handle: 0x%lx\n",(unsigned long)READ_DWORD (parms2[0].info));
                        fprintf (out, "        CCBS Reference: 0x%x\n", READ_WORD (parms2[1].info));
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected dw)\n");
                    }
                    break;
                  case 0x0011:
                  case 0x0016:
                    if (apiParse (parms1[1].info, parms1[1].len, "dws", parms2))
                    {
                        fprintf (out, "        Handle: 0x%lx\n",(unsigned long)READ_DWORD (parms2[0].info));
                        fprintf (out, "        CCBS Reference: 0x%x\n", READ_WORD (parms2[1].info));
                        fprintf (out, "        Served User Number: (0x%lx): ", parms2[2].len);
                        if (parms2[2].len)
                        {
                            CPartyNumber (parms2[2].info, parms2[2].len, out, 2);
                        }
                        fprintf (out, "\n");
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected dw)\n");
                    }
                    break;
                  case 0x0012:
                    if (apiParse (parms1[1].info, parms1[1].len, "wwwsssss", parms2))
                    {
                        fprintf (out, "        CCBS Reference: 0x%x\n", READ_WORD (parms2[0].info));
                        fprintf (out, "        CIP Value: 0x%x\n", READ_WORD (parms2[1].info));
                        fprintf (out, "        Reserved: 0x%x\n", READ_WORD (parms2[2].info));
                        BProtocol (parms2[3].info, parms2[3].len, out, 0, 2);
                        fprintf (out, "        BC (0x%lx): ", parms2[4].len);
                        if (parms2[4].len)
                        {
                            printHex (parms2[4].info, parms2[4].len, out);
                        }
                        fprintf (out, "\n        LLC (0x%lx): ", parms2[5].len);
                        if (parms2[5].len)
                        {
                            printHex (parms2[5].info, parms2[5].len, out);
                        }
                        fprintf (out, "\n        HLC (0x%lx): ", parms2[6].len);
                        if (parms2[6].len)
                        {
                            printHex (parms2[6].info, parms2[6].len, out);
                        }
                        fprintf (out, "\n");
                        AdditionalInfo (parms2[7].info, parms2[7].len, out, 1);
                    }
                    break;
                  case 0x0013:
                    if (apiParse (parms1[1].info, parms1[1].len, "wdwwwssss", parms2))
                    {
                        fprintf (out, "        Basic Service: 0x%x\n", READ_WORD (parms2[0].info));
                        fprintf (out, "        Number of Messages: 0x%lx\n", (unsigned long)READ_DWORD (parms2[1].info));
                        fprintf (out, "        Message Status: 0x%x\n", READ_WORD (parms2[2].info));
                        fprintf (out, "        Message Reference: 0x%x\n", READ_WORD (parms2[3].info));
                        fprintf (out, "        Invocation Mode: 0x%x\n", READ_WORD (parms2[4].info));
                        fprintf (out, "        Receiving User Number: (0x%lx): ", parms2[5].len);
                        if (parms2[5].len)
                        {
                            CPartyNumber (parms2[5].info, parms2[5].len, out, 2);
                        }
                        fprintf (out, "\n        Controlling User Number: (0x%lx): ", parms2[6].len);
                        if (parms2[6].len)
                        {
                            CPartyNumber (parms2[6].info, parms2[6].len, out, 2);
                        }
                        fprintf (out, "\n        Controlling User Provided Number: (0x%lx): ", parms2[7].len);
                        if (parms2[7].len)
                        {
                            CPartyNumber (parms2[7].info, parms2[7].len, out, 2);
                        }
                        fprintf (out, "\n        Time: (0x%lx): ", parms2[8].len);
                        if (parms2[8].len)
                        {
                            fprintf (out, "'");
                            for (i = 0; i < parms2[8].len; i++)
                                fprintf (out, "%c",parms2[8].info[i]);
                            fprintf (out, "'");
                        }
                        fprintf (out, "\n");
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wdwwwssss)\n");
                    }
                    break;
                  case 0x0014:
                    if (apiParse (parms1[1].info, parms1[1].len, "wwss", parms2))
                    {
                        fprintf (out, "        Basic Service: 0x%x\n", READ_WORD (parms2[0].info));
                        fprintf (out, "        Invocation Mode: 0x%x\n", READ_WORD (parms2[1].info));
                        fprintf (out, "        Receiving User Number: (0x%lx): ", parms2[2].len);
                        if (parms2[2].len)
                        {
                            CPartyNumber (parms2[2].info, parms2[2].len, out, 2);
                        }
                        fprintf (out, "\n        Controlling User Number: (0x%lx): ", parms2[3].len);
                        if (parms2[3].len)
                        {
                            CPartyNumber (parms2[3].info, parms2[3].len, out, 2);
                        }
                        fprintf (out, "\n");
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wwss)\n");
                    }
                    break;
                  case 0x0017:
                  case 0x0019:
                  case 0x001a:
                  case 0x001b:
                  case 0x001c:
                    if (apiParse (parms1[1].info, parms1[1].len, "d", parms2))
                    {
                        if(function==0x17) fprintf (out, "        Conference Size: (0x%x): ", READ_DWORD (parms2[0].info));
                        else  fprintf (out, "        Party Identifier: (0x%x): ", READ_DWORD (parms2[0].info));
                        fprintf (out, "\n");
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected b)\n");
                    }
                    break;
                }
                break;
              case CON:
                switch (function)
                {
                  case 0x0000:
                    if (apiParse (parms1[1].info, parms1[1].len, "wd", parms2))
                    {
                        fprintf (out, "        Info: 0x%x\n", READ_WORD (parms2[0].info));
                        fprintf (out, "        Supported Services: 0x%lx ", (unsigned long)READ_DWORD (parms2[1].info));
                        printMask32 (SupportedServices, READ_DWORD (parms2[1].info), out);
                        fprintf (out, "\n");
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wd)\n");
                    }
                    break;
                  case 0x0001:
                  case 0x0002:
                  case 0x0003:
                  case 0x0004:
                  case 0x0005:
                  case 0x0006:
                  case 0x0007:
                  case 0x0008:
                  case 0x0009:
                  case 0x000a:
                  case 0x000b:
                  case 0x000c:
                  case 0x000d:
                  case 0x000e:
                  case 0x000f:
                  case 0x0010:
                  case 0x0011:
                  case 0x0012:
                  case 0x0013:
                  case 0x0014:
                  case 0x0015:
                  case 0x0016:
                  case 0x0017:
                  case 0x0018:
                  case 0x0019:
                  case 0x001a:
                  case 0x001b:
                  case 0x001c:
                    if (apiParse (parms1[1].info, parms1[1].len, "w", parms2))
                    {
                        fprintf (out, "        Info: 0x%x\n", READ_WORD (parms2[0].info));
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected w)\n");
                    }
                    break;
                }
                break;
              case IND:
                switch (function)
                {
                  case 0x0002:
                  case 0x0003:
                  case 0x0004:
                  case 0x0005:
                  case 0x0006:
                  case 0x0007:
                  case 0x0008:
                  case 0x000d:
                  case 0x000e:
                  case 0x0012:
                  case 0x0013:
                  case 0x0014:
                  case 0x001a:
                  case 0x001b:
                  case 0x001c:
                    if (apiParse (parms1[1].info, parms1[1].len, "w", parms2))
                    {
                        fprintf (out, "        Reason: 0x%x\n", READ_WORD (parms2[0].info));
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected w)\n");
                    }
                    break;
                  case 0x0009:
                  case 0x000a:
                  case 0x0010:
                    if (apiParse (parms1[1].info, parms1[1].len, "wd", parms2))
                    {
                        fprintf (out, "        Reason: 0x%x\n", READ_WORD (parms2[0].info));
                        fprintf (out, "        Handle: 0x%lx\n",(unsigned long)READ_DWORD (parms2[1].info));
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wd)\n");
                    }
                    break;
                  case 0x000b:
                    if (apiParse (parms1[1].info, parms1[1].len, "wds", parms2))
                    {
                        fprintf (out, "        Reason: 0x%x\n", READ_WORD (parms2[0].info));
                        fprintf (out, "        Handle: 0x%lx\n",(unsigned long)READ_DWORD (parms2[1].info));
                        fprintf (out, "        Instances: (0x%lx):\n", parms2[2].len);
                        if (parms2[2].len)
                        {
                            byte *p = parms2[2].info;
                            long len = 0;
                            while (p - parms2[2].info < parms2[2].len)
                            {
                                len = (long) (*p + 1);
                                if (apiParse (p+1, len, "wwsss", parms3))
                                {
                                    fprintf (out, "          Call-Forwarding type: 0x%x ", READ_WORD (parms3[0].info));
                                    printMask16 (ForwardingType, READ_WORD (parms3[0].info), out);
                                    fprintf (out, "\n          Basic Service: 0x%x\n", READ_WORD (parms3[1].info));
                                    fprintf (out, "          Served User Number: (0x%lx): ", parms3[2].len);
                                    if (parms3[2].len)
                                    {
                                        CPartyNumber (parms3[2].info, parms3[2].len, out, 2);
                                    }
                                    fprintf (out, "\n          Forwarded-to-Number: (0x%lx): ", parms3[3].len);
                                    if (parms3[3].len)
                                    {
                                        CPartyNumber (parms3[3].info, parms3[3].len, out, 2);
                                    }
                                    fprintf (out, "\n          Forwarded-to-Subaddress: (0x%lx): ", parms3[4].len);
                                    if (parms3[4].len)
                                    {
                                        CPartyNumber (parms3[4].info, parms3[4].len, out, FALSE);
                                    }
                                    fprintf (out, "\n");
                                    if (apiParse (p+1, len, "wwsssw", parms3))
                                        fprintf (out, "          Remote Enabled: 0x%x\n", READ_WORD (parms3[5].info));
                                }
                                else
                                {
                                    fprintf (out, "          --> wrong format! (expected wwsss or wwsssw)\n");
                                }
                                p += len;
                            }
                        }
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wds)\n");
                    }
                    break;
                  case 0x000c:
                    if (apiParse (parms1[1].info, parms1[1].len, "wds", parms2))
                    {
                        fprintf (out, "        Reason: 0x%x\n", READ_WORD (parms2[0].info));
                        fprintf (out, "        Handle: 0x%lx\n", (unsigned long)READ_DWORD (parms2[1].info));
                        fprintf (out, "        Served User Numbers: (0x%lx):\n", parms2[2].len);
                        if (parms2[2].len)
                        {
                            byte *p = parms2[2].info;
                            long len = 0;
                            while (p - parms2[2].info < parms2[2].len)
                            {
                                len = (long) (*p + 1);
                                fprintf (out, "          ");
                                CPartyNumber (p+1, len-1, out, 2);
                                fprintf (out, "\n");
                                p += len;
                            }
                        }
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wds)\n");
                    }
                    break;
                  case 0x0017:
                  case 0x0018:
                    if (apiParse (parms1[1].info, parms1[1].len, "wd", parms2))
                    {
                        fprintf (out, "        Reason: 0x%x\n", READ_WORD (parms2[0].info));
                        fprintf (out, "        Party Identifier: 0x%x\n", READ_DWORD (parms2[1].info));
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wb)\n");
                    }
                    break;
                  case 0x0019:
                    if (apiParse (parms1[1].info, parms1[1].len, "wd", parms2))
                    {
                        fprintf (out, "        Reason: 0x%x\n", READ_WORD (parms2[0].info));
                        fprintf (out, "        PLCI: 0x%08lx\n",(unsigned long)READ_DWORD (parms2[1].info));
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wd)\n");
                    }
                    break;
                  case 0x000f:
                  case 0x0015:
                    if (apiParse (parms1[1].info, parms1[1].len, "wdww", parms2))
                    {
                        fprintf (out, "        Reason: 0x%x\n", READ_WORD (parms2[0].info));
                        fprintf (out, "        Handle: 0x%08lx\n", (unsigned long)READ_DWORD (parms2[1].info));
                        fprintf (out, "        CCBS Recall Mode: 0x%x\n", READ_WORD (parms2[2].info));
                        fprintf (out, "        Reference: 0x%x\n", READ_WORD (parms2[3].info));
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wdww)\n");
                    }
                    break;
                  case 0x0011:
                  case 0x0016:
                    if (apiParse (parms1[1].info, parms1[1].len, "wdws", parms2))
                    {
                        fprintf (out, "        Reason: 0x%x\n", READ_WORD (parms2[0].info));
                        fprintf (out, "        Handle: 0x%08lx\n", (unsigned long)READ_DWORD (parms2[1].info));
                        fprintf (out, "        CCBS Recall Mode: 0x%x\n", READ_WORD (parms2[2].info));
                        fprintf (out, "        CCBS Instances: (0x%lx):\n", parms2[3].len);
                        if (parms2[3].len)
                        {
                            byte *p = parms2[3].info;
                            long len = 0;
                            while (p - parms2[3].info < parms2[3].len)
                            {
                                len = (long) (*p + 1);
                                if (apiParse (p+1, len, "wwssssss", parms3))
                                {
                                    fprintf (out, "          CCBS Reference: 0x%x\n", READ_WORD (parms3[0].info));                                    
                                    fprintf (out, "          CIP Value: 0x%x\n", READ_WORD (parms3[1].info));
                                    fprintf (out, "          BC (0x%lx): ", parms3[2].len);
                                    if (parms3[2].len)
                                    {
                                        printHex (parms3[2].info, parms3[2].len, out);
                                    }
                                    fprintf (out, "\n          LLC (0x%lx): ", parms3[3].len);
                                    if (parms3[3].len)
                                    {
                                        printHex (parms3[3].info, parms3[3].len, out);
                                    }
                                    fprintf (out, "\n          HLC (0x%lx): ", parms3[4].len);
                                    if (parms3[4].len)
                                    {
                                        printHex (parms3[4].info, parms3[4].len, out);
                                    }                                    
                                    fprintf (out, "\n          Address of B-party: (0x%lx): ", parms3[5].len);
                                    if (parms3[5].len)
                                    {
                                        CPartyNumber (parms3[5].info, parms3[5].len, out, 2);
                                    }
                                    fprintf (out, "\n          Subaddress of B-party: (0x%lx): ", parms3[6].len);
                                    if (parms3[6].len)
                                    {
                                        CPartyNumber (parms3[6].info, parms3[6].len, out, 0);
                                    }
                                    fprintf (out, "\n          Subaddress of A-party: (0x%lx): ", parms3[7].len);
                                    if (parms3[7].len)
                                    {
                                        CPartyNumber (parms3[7].info, parms3[7].len, out, 0);
                                    }
                                    fprintf (out, "\n");                                    
                                }
                                else
                                {
                                    fprintf (out, "          --> wrong format! (expected wwssssss)\n");
                                }
                                p += len;
                            }
                        }
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wdws)\n");
                    }
                    break;
                  case 0x8006:
                    if (apiParse (parms1[1].info, parms1[1].len, "wwsss", parms2))
                    {
                        fprintf (out, "        Call-Forwarding type: 0x%x ", READ_WORD (parms2[0].info));
                        printMask16 (ForwardingType, READ_WORD (parms2[0].info), out);
                        fprintf (out, "\n        Basic Service: 0x%x\n", READ_WORD (parms2[1].info));
                        fprintf (out, "        Served User Number: (0x%lx): ", parms2[2].len);
                        if (parms2[2].len)
                        {
                            CPartyNumber (parms2[2].info, parms2[2].len, out, 2);
                        }
                        fprintf (out, "\n        Forwarded-to-Number: (0x%lx): ", parms2[3].len);
                        if (parms2[3].len)
                        {
                            CPartyNumber (parms2[3].info, parms2[3].len, out, 2);
                        }
                        fprintf (out, "\n        Forwarded-to-Subaddress: (0x%lx): ", parms2[4].len);
                        if (parms2[4].len)
                        {
                            CPartyNumber (parms2[4].info, parms2[4].len, out, FALSE);
                        }
                        fprintf (out, "\n");
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wwsss)\n");
                    }
                    break;
                  case 0x8007:
                    if (apiParse (parms1[1].info, parms1[1].len, "wws", parms2))
                    {
                        fprintf (out, "        Call-Forwarding type: 0x%x ", READ_WORD (parms2[0].info));
                        printMask16 (ForwardingType, READ_WORD (parms2[0].info), out);
                        fprintf (out, "\n        Basic Service: 0x%x\n", READ_WORD (parms2[1].info));
                        fprintf (out, "        Served User Number: (0x%lx): ", parms2[2].len);
                        if (parms2[2].len)
                        {
                            CPartyNumber (parms2[2].info, parms2[2].len, out, 2);
                        }
                        fprintf (out, "\n");
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wws)\n");
                    }
                    break;
                  case 0x8008:
                    if (apiParse (parms1[1].info, parms1[1].len, "wwwsssss", parms2))
                    {
                        fprintf (out, "        Basic Service: 0x%x\n", READ_WORD (parms2[0].info));
                        fprintf (out, "        Diversion reason: 0x%x ", READ_WORD (parms2[1].info));
                        printMask16 (DiversionReason, READ_WORD (parms2[1].info), out);
                        fprintf (out, "        Last divertiing reason: 0x%x ", READ_WORD (parms2[2].info));
                        printMask16 (DiversionReason, READ_WORD (parms2[2].info), out);
                        fprintf (out, "        Served User Subaddress: (0x%lx): ", parms2[3].len);
                        if (parms2[3].len)
                        {
                            CPartyNumber (parms2[3].info, parms2[3].len, out, 0);
                        }
                        fprintf (out, "\n        Calling Number: (0x%lx): ", parms2[4].len);
                        if (parms2[4].len)
                        {
                            CPartyNumber (parms2[4].info, parms2[4].len, out, 2);
                        }
                        fprintf (out, "\n        Calling Subaddress: (0x%lx): ", parms2[5].len);
                        if (parms2[5].len)
                        {
                            CPartyNumber (parms2[5].info, parms2[5].len, out, 0);
                        }
                        fprintf (out, "\n        Original called number: (0x%lx): ", parms2[6].len);
                        if (parms2[6].len)
                        {
                            CPartyNumber (parms2[6].info, parms2[6].len, out, 2);
                        }
                        fprintf (out, "\n        Last diverting number: (0x%lx): ", parms2[7].len);
                        if (parms2[7].len)
                        {
                            CPartyNumber (parms2[7].info, parms2[7].len, out, 2);
                        }
                        fprintf (out, "\n");
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wwwsssss)\n");
                    }
                    break;
                  case 0x800d:
                    if (apiParse (parms1[1].info, parms1[1].len, "wss", parms2))
                    {
                        fprintf (out, "        CCBS Call Linkage ID: 0x%x\n", READ_WORD (parms2[0].info));
                        fprintf (out, "        Called Party Number (0x%lx): ", parms2[1].len);
                        if (parms2[1].len)
                        {
                            CPartyNumber (parms2[1].info, parms2[1].len, out, 0);
                        }
                        fprintf (out, "\n        Called Party Subaddress: (0x%lx): ", parms2[2].len);
                        if (parms2[2].len)
                        {
                            CPartyNumber (parms2[2].info, parms2[2].len, out, 0);
                        }
                        fprintf (out, "\n");
                    }
                    break;
                  case 0x800e:
                  case 0x800f:
                  case 0x8010:
                  case 0x8011:
                    if (((function==0x800e) && apiParse (parms1[1].info, parms1[1].len, "wwwsssss", parms2)) ||
                        ((function==0x8011) && apiParse (parms1[1].info, parms1[1].len, "wwwwsssssss", parms2)) ||
                        ((function!=0x8011) && apiParse (parms1[1].info, parms1[1].len, "wwwsssssss", parms2)))
                    {
                        fprintf (out, "        CCBS Recall Mode: 0x%x\n", READ_WORD (parms2[0].info));
                        fprintf (out, "        CCBS Reference: 0x%x\n", READ_WORD (parms2[1].info));
                        i=2;
                        if(function==0x8011)
                        {
                            fprintf (out, "        CCBS Erase Reason: 0x%x\n", READ_WORD (parms2[i].info));
                        i++;
                        }
                        fprintf (out, "        CIP Value: 0x%x\n", READ_WORD (parms2[i].info));
                        i++;
                        fprintf (out, "        BC (0x%lx): ", parms2[i].len);
                        if (parms2[i].len)
                        {
                            printHex (parms2[i].info, parms2[i].len, out);
                        }
                        fprintf (out, "\n        LLC (0x%lx): ", parms2[++i].len);
                        if (parms2[i].len)
                        {
                            printHex (parms2[i].info, parms2[i].len, out);
                        }
                        fprintf (out, "\n        HLC (0x%lx): ", parms2[++i].len);
                        if (parms2[i].len)
                        {
                            printHex (parms2[i].info, parms2[i].len, out);
                        }
                        fprintf (out, "\n        Called Party Number (0x%lx): ", parms2[++i].len);
                        if (parms2[i].len)
                        {
                            CPartyNumber (parms2[i].info, parms2[i].len, out, 0);
                        }
                        fprintf (out, "\n        Called Party Subaddress (0x%lx): ", parms2[++i].len);
                        if (parms2[i].len)
                        {
                            CPartyNumber (parms2[i].info, parms2[i].len, out, 0);
                        }
                        if(function!=0x800e)
                        {
                            fprintf (out, "\n        Address of B-party (0x%lx): ", parms2[++i].len);
                            if (parms2[i].len)
                            {
                                CPartyNumber (parms2[i].info, parms2[i].len, out, 2);
                            }
                            fprintf (out, "\n        Subaddress of B-party (0x%lx): ", parms2[++i].len);
                            if (parms2[i].len)
                            {
                                CPartyNumber (parms2[i].info, parms2[i].len, out, 2);
                            }
                        }
                        fprintf (out, "\n");
                    }
                    break;
                  
                  case 0x8012:
                    if (apiParse (parms1[1].info, parms1[1].len, "w", parms2))
                    {
                        fprintf (out, "        CCBS Reference: 0x%x\n", READ_WORD (parms2[0].info));
                    }
                    break;
                  case 0x8013:
                  case 0x8015:
                    if (apiParse (parms1[1].info, parms1[1].len, "w", parms2))
                    {
                        fprintf (out, "        CCBS Call Linkage ID: 0x%x\n", READ_WORD (parms2[0].info));
                    }
                    break;
                  case 0x8014:
                    if (apiParse (parms1[1].info, parms1[1].len, "wdwwsss", parms2))
                    {
                        fprintf (out, "        Basic Service: 0x%x\n", READ_WORD (parms2[0].info));
                        fprintf (out, "        Number of Messages: 0x%lx\n", (unsigned long)READ_DWORD (parms2[1].info));
                        fprintf (out, "        Message Status: 0x%x\n", READ_WORD (parms2[2].info));
                        fprintf (out, "        Message Reference: 0x%x\n", READ_WORD (parms2[3].info));
                        fprintf (out, "        Controlling User Number: (0x%lx): ", parms2[4].len);
                        if (parms2[4].len)
                        {
                            CPartyNumber (parms2[4].info, parms2[4].len, out, 2);
                        }
                        fprintf (out, "\n        Controlling User Provided Number: (0x%lx): ", parms2[5].len);
                        if (parms2[5].len)
                        {
                            CPartyNumber (parms2[5].info, parms2[5].len, out, 2);
                        }
                        fprintf (out, "\n        Time: (0x%lx): ", parms2[6].len);
                        if (parms2[6].len)
                        {
                            fprintf (out, "'");
                            for (i = 0; i < parms2[6].len; i++)
                                fprintf (out, "%c",parms2[6].info[i]);
                            fprintf (out, "'");
                        }
                        if (apiParse (parms1[1].info, parms1[1].len, "wdwwssss", parms2))
                        {
                            fprintf (out, "\n        Called Party Number (0x%lx): ", parms2[7].len);
                            if (parms2[7].len)
                            {
                                for (i = 0; i < parms2[7].len && !(parms2[7].info[i] & 0x80); i++)
                                    fprintf (out, " %02x",parms2[7].info[i]);
                                fprintf (out, " %02x",parms2[7].info[i++]);
                                fprintf (out, " '");
                                for ( ; i < parms2[7].len; i++) fprintf (out, "%c",parms2[7].info[i]);
                                fprintf (out, "'");
                            }
                        }
                        fprintf (out, "\n");
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wdwwsss)\n");
                    }
                    break;
                  case 0x8016:
                  case 0x8017:
                    if (apiParse (parms1[1].info, parms1[1].len, "d", parms2))
                    {
                        if(function==0x8016) fprintf (out, "        Party Identifier: 0x%x\n", READ_DWORD (parms2[0].info));
                        else fprintf (out, "        Notification Identifier: 0x%x\n", READ_DWORD (parms2[0].info));
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected b)\n");
                    }
                    break;
                }
                break;
              case RES:
                switch (function)
                {
                  case 0x800e:
                    if (apiParse (parms1[1].info, parms1[1].len, "w", parms2))
                    {
                        fprintf (out, "        CCBS Status Report: 0x%x\n", READ_WORD (parms2[0].info));
                    }
                    break;
                }
                break;
            }
        }
    }
    else
    {
        fprintf (out, "      --> wrong format! (expected ws)\n");
    }
}
// --------------------------------------------------------------
static
void
PowerManagementWakeup (byte subCommand, byte * data, long len, FILE * out)
{
      API_PARSE parms1[4];
    long pos;
    switch (subCommand)
    {
      case REQ:
        if (apiParse (data, len, "w", parms1))
        {
            fprintf (out, "\n      Number of awake request parameters: %d\n", READ_WORD (parms1[0].info));
            pos = sizeof(word);
            while (pos < len)
            {
                if (apiParse (data + pos, len - pos, "sd", parms1))
                {
                    fprintf (out, "        Called Party Number (0x%lx): ", parms1[0].len);
                    if (parms1[0].len)
                    {
                        CPartyNumber (parms1[0].info, parms1[0].len, out, FALSE);
                    }
                    fprintf (out, "\n        CIP mask: 0x%lx ", (unsigned long)READ_DWORD (parms1[1].info));
                    if (READ_DWORD (parms1[1].info))
                    {
                        printMask32 (CipMask, READ_DWORD (parms1[1].info), out);
                        fprintf (out, "\n");
                    }
                    else
                    {
                        fprintf (out, "(signalling of incoming calls disabled)\n");
                    }
                    pos = ((long)(parms1[1].info - data)) + parms1[1].len;
                }
                else
                {
                    fprintf (out, "        --> wrong format! (expected sd)\n");
                    pos = len;
                }
            }
        }
        else
        {
            fprintf (out, "        --> wrong format! (expected w)\n");
        }
        break;
      case CON:
        if (apiParse (data, len, "w", parms1))
        {
            fprintf (out, "\n      Accepted awake request parameters: %d\n", READ_WORD (parms1[0].info));
        }
        else
        {
            fprintf (out, "        --> wrong format! (expected w)\n");
        }
        break;
      case IND:
        fprintf (out, "\n");
        break;
      case RES:
        fprintf (out, "\n");
        break;
    }
}
// --------------------------------------------------------------
static
void
LineInterconnect (byte subCommand, byte * data, long len, FILE * out)
{
      API_PARSE parms1[4];
      API_PARSE parms2[8];
      API_PARSE parms3[4];
      API_PARSE parms4[4];
    word function;
    long pos;
    if (apiParse (data, len, "ws", parms1))
    {
        function = READ_WORD (parms1[0].info);
        fprintf (out, "\n      Function: %s\n", printString (LineInterconnectFunction, function));
        fprintf (out, "      Line Interconnect parameter (0x%lx):\n", parms1[1].len);
        if (parms1[1].len)
        {
            switch (subCommand)
            {
              case REQ:
                switch (function)
                {
                  case 0x0001:
                    if (parms1[1].len == 8)
                    {
                        if (apiParse (parms1[1].info, parms1[1].len, "dd", parms2))
                        {
                            fprintf (out, "        PLCI: 0x%08lx\n", (unsigned long)READ_DWORD (parms2[0].info));
                            fprintf (out, "        Mask: 0x%08lx ", (unsigned long)READ_DWORD (parms2[1].info));
                            printMask32 (LiOldInterconnectMask, READ_DWORD (parms2[1].info), out);
                            fprintf (out, "\n");
                        }
                        else
                        {
                            fprintf (out, "        --> wrong format! (expected dd)\n");
                        }
                    }
                    else
                    {
                        if (apiParse (parms1[1].info, parms1[1].len, "ds", parms2))
                        {
                            fprintf (out, "        Mask: 0x%08lx ", (unsigned long)READ_DWORD (parms2[0].info));
                            printMask32 (LiMainInterconnectMask, READ_DWORD (parms2[0].info), out);
                            fprintf (out, "\n");
                            pos = 0;
                            while (pos < parms2[1].len)
                            {
                                if (apiParse (parms2[1].info + pos, parms2[1].len - pos, "s", parms3))
                                {
                                    if (apiParse (parms3[0].info, parms3[0].len, "dd", parms4))
                                    {
                                        fprintf (out, "          PLCI: 0x%08lx\n", (unsigned long)READ_DWORD (parms4[0].info));
                                        fprintf (out, "          Mask: 0x%08lx ", (unsigned long)READ_DWORD (parms4[1].info));
                                        printMask32 (LiPartInterconnectMask, READ_DWORD (parms4[1].info), out);
                                        fprintf (out, "\n");
                                    }
                                    else
                                    {
                                        fprintf (out, "        --> wrong format! (expected dd)\n");
                                    }
                                    pos = ((long)(parms3[0].info - parms2[1].info)) + parms3[0].len;
                                }
                                else
                                {
                                    fprintf (out, "        --> wrong format! (expected s)\n");
                                    pos = parms2[1].len;
                                }
                            }
                        }
                        else
                        {
                            fprintf (out, "        --> wrong format! (expected ds)\n");
                        }
                    }
                    break;
                  case 0x0002:
                    if (parms1[1].len == 4)
                    {
                        if (apiParse (parms1[1].info, parms1[1].len, "d", parms2))
                        {
                            fprintf (out, "        PLCI: 0x%08lx\n", (unsigned long)READ_DWORD (parms2[0].info));
                        }
                        else
                        {
                            fprintf (out, "        --> wrong format! (expected d)\n");
                        }
                    }
                    else
                    {
                        if (apiParse (parms1[1].info, parms1[1].len, "s", parms2))
                        {
                            pos = 0;
                            while (pos < parms2[0].len)
                            {
                                if (apiParse (parms2[0].info + pos, parms2[0].len - pos, "s", parms3))
                                {
                                    if (apiParse (parms3[0].info, parms3[0].len, "d", parms4))
                                    {
                                        fprintf (out, "          PLCI: 0x%08lx\n", (unsigned long)READ_DWORD (parms4[0].info));
                                    }
                                    else
                                    {
                                        fprintf (out, "        --> wrong format! (expected d)\n");
                                    }
                                    pos = ((long)(parms3[0].info - parms2[0].info)) + parms3[0].len;
                                }
                                else
                                {
                                    fprintf (out, "        --> wrong format! (expected s)\n");
                                    pos = parms2[0].len;
                                }
                            }
                        }
                        else
                        {
                            fprintf (out, "        --> wrong format! (expected s)\n");
                        }
                    }
                    break;
                }
                break;
              case CON:
                switch (function)
                {
                  case 0x0000:
                    if (parms1[1].len == 14)
                    {
                        if (apiParse (parms1[1].info, parms1[1].len, "wddd", parms2))
                        {
                            fprintf (out, "        Info: 0x%04x (%s)\n", READ_WORD (parms2[0].info),
                                     printString (Info, READ_WORD (parms2[0].info)));
                            fprintf (out, "        Supported Services: 0x%08lx ", (unsigned long)READ_DWORD (parms2[1].info));
                            printMask32 (LiOldSupportedServices, READ_DWORD (parms2[1].info), out);
                            fprintf (out, "\n        Supported Interconnects: %ld\n", (unsigned long)READ_DWORD (parms2[2].info));
                            fprintf (out, "        Supported Branches: %ld\n", (unsigned long)READ_DWORD (parms2[3].info));
                        }
                        else
                        {
                            fprintf (out, "        --> wrong format! (expected wddd)\n");
                        }
                    }
                    else
                    {
                        if (apiParse (parms1[1].info, parms1[1].len, "wddddd", parms2))
                        {
                            fprintf (out, "        Info: 0x%04x (%s)\n", READ_WORD (parms2[0].info),
                                     printString (Info, READ_WORD (parms2[0].info)));
                            fprintf (out, "        Supported Services: 0x%08lx ", (unsigned long)READ_DWORD (parms2[1].info));
                            printMask32 (LiSupportedServices, READ_DWORD (parms2[1].info), out);
                            fprintf (out, "\n        Supported Interconnects (Ctlr.): %ld\n", (unsigned long)READ_DWORD (parms2[2].info));
                            fprintf (out, "        Supported Branches (Ctlr.): %ld\n", (unsigned long)READ_DWORD (parms2[3].info));
                            fprintf (out, "        Supported Interconnects (Total): %ld\n", (unsigned long)READ_DWORD (parms2[4].info));
                            fprintf (out, "        Supported Branches (Total): %ld\n", (unsigned long)READ_DWORD (parms2[5].info));
                        }
                        else
                        {
                            fprintf (out, "        --> wrong format! (expected wddddd)\n");
                        }
                    }
                    break;
                  case 0x0001:
                    if (parms1[1].len == 6)
                    {
                        if (apiParse (parms1[1].info, parms1[1].len, "dw", parms2))
                        {
                            fprintf (out, "        PLCI: 0x%08lx\n", (unsigned long)READ_DWORD (parms2[0].info));
                            fprintf (out, "        Info: 0x%04x (%s)\n", READ_WORD (parms2[1].info),
                                     printString (Info, READ_WORD (parms2[1].info)));
                        }
                        else
                        {
                            fprintf (out, "        --> wrong format! (expected dw)\n");
                        }
                    }
                    else
                    {
                        if (apiParse (parms1[1].info, parms1[1].len, "ws", parms2))
                        {
                            fprintf (out, "        Info: 0x%04x (%s)\n", READ_WORD (parms2[0].info),
                                     printString (Info, READ_WORD (parms2[0].info)));
                            pos = 0;
                            while (pos < parms2[1].len)
                            {
                                if (apiParse (parms2[1].info + pos, parms2[1].len - pos, "s", parms3))
                                {
                                    if (apiParse (parms3[0].info, parms3[0].len, "dw", parms4))
                                    {
                                        fprintf (out, "          PLCI: 0x%08lx\n", (unsigned long)READ_DWORD (parms4[0].info));
                                        fprintf (out, "          Info: 0x%04x (%s)\n", READ_WORD (parms4[1].info),
                                                 printString (Info, READ_WORD (parms4[1].info)));
                                    }
                                    else
                                    {
                                        fprintf (out, "        --> wrong format! (expected dw)\n");
                                    }
                                    pos = ((long)(parms3[0].info - parms2[1].info)) + parms3[0].len;
                                }
                                else
                                {
                                    fprintf (out, "        --> wrong format! (expected s)\n");
                                    pos = parms2[1].len;
                                }
                            }
                        }
                        else
                        {
                            fprintf (out, "        --> wrong format! (expected ws)\n");
                        }
                    }
                    break;
                  case 0x0002:
                    if (parms1[1].len == 6)
                    {
                        if (apiParse (parms1[1].info, parms1[1].len, "dw", parms2))
                        {
                            fprintf (out, "        PLCI: 0x%08lx\n", (unsigned long)READ_DWORD (parms2[0].info));
                            fprintf (out, "        Info: 0x%04x (%s)\n", READ_WORD (parms2[1].info),
                                     printString (Info, READ_WORD (parms2[1].info)));
                        }
                        else
                        {
                            fprintf (out, "        --> wrong format! (expected dw)\n");
                        }
                    }
                    else
                    {
                        if (apiParse (parms1[1].info, parms1[1].len, "ws", parms2))
                        {
                            fprintf (out, "        Info: 0x%04x (%s)\n", READ_WORD (parms2[0].info),
                                     printString (Info, READ_WORD (parms2[0].info)));
                            pos = 0;
                            while (pos < parms2[1].len)
                            {
                                if (apiParse (parms2[1].info + pos, parms2[1].len - pos, "s", parms3))
                                {
                                    if (apiParse (parms3[0].info, parms3[0].len, "dw", parms4))
                                    {
                                        fprintf (out, "          PLCI: 0x%08lx\n", (unsigned long)READ_DWORD (parms4[0].info));
                                        fprintf (out, "          Info: 0x%04x (%s)\n", READ_WORD (parms4[1].info),
                                                 printString (Info, READ_WORD (parms4[1].info)));
                                    }
                                    else
                                    {
                                        fprintf (out, "        --> wrong format! (expected dw)\n");
                                    }
                                    pos = ((long)(parms3[0].info - parms2[1].info)) + parms3[0].len;
                                }
                                else
                                {
                                    fprintf (out, "        --> wrong format! (expected s)\n");
                                    pos = parms2[1].len;
                                }
                            }
                        }
                        else
                        {
                            fprintf (out, "        --> wrong format! (expected ws)\n");
                        }
                    }
                    break;
                }
                break;
              case IND:
                switch (function)
                {
                  case 0x0001:
                    if (parms1[1].len == 4)
                    {
                        if (apiParse (parms1[1].info, parms1[1].len, "d", parms2))
                        {
                            fprintf (out, "        PLCI: 0x%08lx\n", (unsigned long)READ_DWORD (parms2[0].info));
                        }
                        else
                        {
                            fprintf (out, "        --> wrong format! (expected d)\n");
                        }
                    }
                    else
                    {
                        if (apiParse (parms1[1].info, parms1[1].len, "d", parms2))
                        {
                            fprintf (out, "        PLCI: 0x%08lx\n", (unsigned long)READ_DWORD (parms2[0].info));
                        }
                        else
                        {
                            fprintf (out, "        --> wrong format! (expected d)\n");
                        }
                    }
                    break;
                  case 0x0002:
                    if (parms1[1].len == 2)
                    {
                        if (apiParse (parms1[1].info, parms1[1].len, "w", parms2))
                        {
                            fprintf (out, "        Reason: 0x%04x (%s)\n", READ_WORD (parms2[0].info),
                                     printString (LiOldServiceReason, READ_WORD (parms2[0].info)));
                        }
                        else
                        {
                            fprintf (out, "        --> wrong format! (expected w)\n");
                        }
                    }
                    else
                    {
                        if (apiParse (parms1[1].info, parms1[1].len, "dw", parms2))
                        {
                            fprintf (out, "        PLCI: 0x%08lx\n", (unsigned long)READ_DWORD (parms2[0].info));
                            fprintf (out, "        Reason: 0x%04x (%s)\n", READ_WORD (parms2[1].info),
                                     printString (LiServiceReason, READ_WORD (parms2[1].info)));
                        }
                        else
                        {
                            fprintf (out, "        --> wrong format! (expected dw)\n");
                        }
                    }
                    break;
                }
                break;
              case RES:
                break;
            }
        }
    }
    else
    {
        fprintf (out, "      --> wrong format! (expected ws)\n");
    }
}
// --------------------------------------------------------------
static
void
EchoCanceller (byte subCommand, byte * data, long len, FILE * out)
{
      API_PARSE parms1[4];
      API_PARSE parms2[8];
    word function;
    if (apiParse (data, len, "ws", parms1))
    {
        switch (subCommand)
        {
          case REQ:
            function = READ_WORD (parms1[0].info);
            fprintf (out, "\n      Function: %s\n", printString (EchoCancellerRequestFunction, function));
            fprintf (out, "      Echo Canceller parameter (0x%lx):\n", parms1[1].len);
            if (parms1[1].len)
            {
                switch (function)
                {
                  case 0x0001:
                    if (apiParse (parms1[1].info, parms1[1].len, "www", parms2))
                    {
                        fprintf (out, "        Options: 0x%04x ", READ_WORD (parms2[0].info));
                        printMask32 (EcOptionsMask, READ_WORD (parms2[0].info), out);
                        fprintf (out, "\n        Tail Length: %d\n", READ_WORD (parms2[1].info));
                        fprintf (out, "        Pre-Delay Length: %d\n", READ_WORD (parms2[2].info));
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected www)\n");
                    }
                    break;
                }
            }
            break;
          case CON:
            function = READ_WORD (parms1[0].info);
            fprintf (out, "\n      Function: %s\n", printString (EchoCancellerRequestFunction, function));
            fprintf (out, "      Echo Canceller parameter (0x%lx):\n", parms1[1].len);
            if (parms1[1].len)
            {
                switch (function)
                {
                  case 0x0000:
                    if (apiParse (parms1[1].info, parms1[1].len, "wwww", parms2))
                    {
                        fprintf (out, "        Info: 0x%04x (%s)\n", READ_WORD (parms2[0].info),
                                 printString (Info, READ_WORD (parms2[0].info)));
                        fprintf (out, "        Supported Options: 0x%04x ", READ_WORD (parms2[1].info));
                        printMask32 (EcSupportedOptionsMask, READ_WORD (parms2[1].info), out);
                        fprintf (out, "\n        Supported Tail Length: %d\n", READ_WORD (parms2[2].info));
                        fprintf (out, "        Supported Pre-Delay Length: %d\n", READ_WORD (parms2[3].info));
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wwww)\n");
                    }
                    break;
                  case 0x0001:
                    if (apiParse (parms1[1].info, parms1[1].len, "w", parms2))
                    {
                        fprintf (out, "        Info: 0x%04x (%s)\n", READ_WORD (parms2[0].info),
                                 printString (Info, READ_WORD (parms2[0].info)));
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wwww)\n");
                    }
                    break;
                  case 0x0002:
                    if (apiParse (parms1[1].info, parms1[1].len, "w", parms2))
                    {
                        fprintf (out, "        Info: 0x%04x (%s)\n", READ_WORD (parms2[0].info),
                                 printString (Info, READ_WORD (parms2[0].info)));
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wwww)\n");
                    }
                    break;
                }
            }
            break;
          case IND:
            function = READ_WORD (parms1[0].info);
            fprintf (out, "\n      Function: %s\n", printString (EchoCancellerIndicationFunction, function));
            fprintf (out, "      Echo Canceller parameter (0x%lx):\n", parms1[1].len);
            if (parms1[1].len)
            {
                switch (function)
                {
                  case 0x0001:
                    if (apiParse (parms1[1].info, parms1[1].len, "w", parms2))
                    {
                        fprintf (out, "        Info: 0x%04x (%s)\n", READ_WORD (parms2[0].info),
                                 printString (EcBypassEvent, READ_WORD (parms2[0].info)));
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected w)\n");
                    }
                    break;
                }
            }
            break;
          case RES:
            break;
        }
    }
    else
    {
        fprintf (out, "      --> wrong format! (expected ws)\n");
    }
}
// --------------------------------------------------------------
static void facilityVoIP (byte subCommand, byte *data, long len, FILE *out) {
	API_PARSE parms[4];
	API_PARSE parms1[4];
	word function;

	if (apiParse (data, len, "ws", parms)) {
		switch (subCommand) {
			case REQ:
				function = READ_WORD (parms[0].info);
        fprintf (out, "\n      Function: %s\n", printString (facilityVoIP_function, function));
        fprintf (out, "      VoIP request parameter (0x%lx):\n", parms[1].len);
				switch (function) {
					case 0x0003:
						if (apiParse (parms[1].info, parms[1].len, "b", parms1)) {
							fprintf (out, "        Payload (%u): %s\n",
											 *(byte*)parms1[0].info,
											 printString (B1_VoIP_Payloads, *(byte*)parms1[0].info));
						} else {
							fprintf (out, "        --> wrong format! (expected ws)\n");
						}
						break;

					case 0x0005:
						if (apiParse (parms[1].info, parms[1].len, "bs", parms1)) {
							API_PARSE parms2[4];

							fprintf (out, "        Function (%u): %s\n",
											 *(byte*)parms1[0].info,
											 printString (facilityVoIP_RTP_CONTROL_function, *(byte*)parms1[0].info));

							switch (*(byte*)parms1[0].info) {
								case 0:
									if (apiParse (parms1[1].info, parms1[1].len, "b", parms2)) {
										fprintf (out, "          Bits[3:0] (0x%02x): reserved\n",
														 (*(byte*)parms2[0].info) & 0x0f);
										fprintf (out, "          Bits[7:4] Codec mode (%d): %s\n",
														 ((*(byte*)parms2[0].info) & 0xf0) >> 4,
														 printString (facilityVoIP_RTP_CONTROL_function_AMR_mode_request,
																					((*(byte*)parms2[0].info) & 0xf0) >> 4));
									} else {
										fprintf (out, "          --> wrong format! (expected ws)\n");
									}
									break;

								case 1:
									if (apiParse (parms1[1].info, parms1[1].len, "b", parms2)) {
										fprintf (out, "          Bits[3:0] (0x%02x): reserved\n",
														 (*(byte*)parms2[0].info) & 0x0f);
										fprintf (out, "          Bits[7:4] Codec mode (%d): %s\n",
														 ((*(byte*)parms2[0].info) & 0xf0) >> 4,
														 printString (facilityVoIP_RTP_CONTROL_function_AMR_mode_indication,
																					((*(byte*)parms2[0].info) & 0xf0) >> 4));
									} else {
										fprintf (out, "          --> wrong format! (expected ws)\n");
									}
									break;

								default:
									break;
							}
						} else {
							fprintf (out, "        --> wrong format! (expected ws)\n");
						}
						break;

					default:
						if (parms[1].len != 0)
							printHex (parms[1].info, parms[1].len, out);
						break;
				}
				break;

			case CON:
				function = READ_WORD (parms[0].info);
        fprintf (out, "\n      Function: %s\n", printString (facilityVoIP_function, function));
        fprintf (out, "      VoIP confirmation parameter (0x%lx):\n", parms[1].len);
				switch (function) {
					case 0x0005:
						if (apiParse (parms[1].info, parms[1].len, "wb", parms1)) {
							fprintf (out, "        Voice over IP info (0x%04x): %s\n",
											 *(word*)parms1[0].info,
											 printString (Info, *(word*)parms1[0].info));
							fprintf (out, "        RTP control function (%u): %s\n",
											 *(byte*)parms1[1].info,
											 printString (facilityVoIP_RTP_CONTROL_function, *(byte*)parms1[0].info));
						} else {
							fprintf (out, "        --> wrong format! (expected ws)\n");
						}
						break;

					default:
						if (parms[1].len != 0)
							printHex (parms[1].info, parms[1].len, out);
						break;
				}
				break;

			default:
				printHex (data, len, out);
				fprintf (out, "\n");
				break;
		}
	} else {
		fprintf (out, "      --> wrong format! (expected ws)\n");
		fprintf (out, "        --> wrong format! (expected ws)\n");
	}
}
// --------------------------------------------------------------
static
void
info (byte subCommand, API_PARSE * data, FILE * out)
{
    word infonum;
    long charges = FALSE;
    switch (subCommand)
    {
      case REQ:
        fprintf (out, "    Called Party Number (0x%lx): ", data[1].len);
        if (data[1].len)
        {
            CPartyNumber (data[1].info, data[1].len, out, FALSE);
        }
        fprintf (out, "\n");
        AdditionalInfo (data[2].info, data[2].len, out, 0);
        break;
      case CON:
        fprintf (out, "    Info: %s\n", printString (Info, READ_WORD (data[1].info)));
        break;
      case IND:
        infonum = READ_WORD (data[1].info);
        fprintf (out, "    Info number: 0x%04x ", infonum);
        switch (infonum & 0xc000)
        {
          case 0x0000:
            fprintf (out, "(Q.931 info element)\n");
            fprintf (out, "    Information type: 0x%x (%s)\n", (byte) (infonum & 0x00ff),
                          printString (InfoElement, (word) (infonum & 0x00ff)));
            break;
          case 0x4000:
            if (infonum & 0x00ff)
            {
                fprintf (out, "(charges in national currency)\n");
            }
            else
            {
                fprintf (out, "(charges in charging units)\n");
            }
            charges = TRUE;
            break;
          case 0x8000:
            fprintf (out, "(Q.931 network event)\n");
            fprintf (out, "      Event: 0x%x (%s)\n", (byte) (infonum & 0x00ff),
                          printString (Event, (word) (infonum & 0x00ff)));
            break;
        }
        fprintf (out, "    Info element (0x%lx): ", data[2].len);
        if (data[2].len)
        {
            if (charges)
            {
                fprintf (out, "%ld units", (long)READ_DWORD (data[2].info));
            }
            else
            {
                printHex (data[2].info, data[2].len, out);
            }
        }
        fprintf (out, "\n");
        break;
      case RES:
        break;
    }
}
// --------------------------------------------------------------
static
void
listen (byte subCommand, API_PARSE * data, FILE * out)
{
    switch (subCommand)
    {
      case REQ:
        fprintf (out, "    Info mask: 0x%lx ", (unsigned long)READ_DWORD (data[1].info));
        printMask32 (InfoMask, READ_DWORD (data[1].info), out);
        fprintf (out, "\n    CIP mask: 0x%lx ", (unsigned long)READ_DWORD (data[2].info));
        if (READ_DWORD (data[2].info))
        {
            printMask32 (CipMask, READ_DWORD (data[2].info), out);
        }
        else
        {
            fprintf (out, "(signalling of incoming calls disabled)");
        }
        fprintf (out, "\n    CIP mask 2: 0x%lx ", (unsigned long)READ_DWORD (data[3].info));
        printMask32 (CipMask2, READ_DWORD (data[3].info), out);
        fprintf (out, "\n    Calling Party Number (0x%lx): ", data[4].len);
        if (data[4].len)
        {
            CPartyNumber (data[4].info, data[4].len, out, TRUE);
        }
        fprintf (out, "\n    Calling Party Subaddress (0x%lx): ", data[5].len);
        if (data[5].len)
        {
            CPartyNumber (data[5].info, data[5].len, out, FALSE);
        }
        fprintf (out, "\n");
        break;
      case CON:
        fprintf (out, "    Info: %s\n", printString (Info, READ_WORD (data[1].info)));
        break;
    }
}
// --------------------------------------------------------------
typedef struct _parse_manufacturer_management_read_context {
  const char* info;
  FILE* out;
} parse_manufacturer_management_read_context_t;
static void parse_manufacturer_management_read (void* callback_context,
                                                byte Type,
                                                byte *IE,
                                                int  IElen,
                                                int SingleOctett) {
  parse_manufacturer_management_read_context_t* context = 
                      (parse_manufacturer_management_read_context_t*)callback_context;
  fprintf (context->out, "%s", context->info);
  fprintf (context->out, "%s: ",
           printString (_DI_MANAGEMENT_INFO_read_ie, Type));

  if (IElen <= 2) {
    fprintf (context->out, "%s--> wrong i.e. format!\n", context->info);
    return;
  }

  switch (Type) {
    case 1: /* _DI_MANAGEMENT_INFO_READ_LOGICAL_ADAPTER_NUMBER_IE */
      fprintf (context->out, "%d\n", IE[2]);
      break;
    case 2: /* _DI_MANAGEMENT_INFO_READ_VSCAPI_IE */
      fprintf (context->out, "%s\n", (IE[2] != 0) ? "Yes" : "No");
      break;
    case 3: /* _DI_MANAGEMENT_INFO_READ_ADAPTER_SERIAL_NUMBER_IE */
      fprintf (context->out, "%d\n", READ_DWORD(&IE[2]));
      break;
    case 4: /* _DI_MANAGEMENT_INFO_READ_CARD_TYPE_IE */
      fprintf (context->out, "%d\n", READ_WORD(&IE[2]));
      break;
    case 5: /* _DI_MANAGEMENT_INFO_READ_CARD_ID_IE */
      fprintf (context->out, "0x%08x\n", READ_DWORD(&IE[2]));
      break;

    default:
      printHex (&IE[2], IElen - 2, context->out);
      break;
  }
}
// --------------------------------------------------------------
static
void
manufacturer (byte subCommand, API_PARSE * data, FILE * out)
{
    fprintf (out, "    Manu ID: 0x%08lx\n", (unsigned long)READ_DWORD (data[1].info));

    if (subCommand == CON) {
      PlciInfoIdentify (READ_DWORD (data[0].info));

      if (data[2].len >= 4) {
        byte* p = data[2].info;
        int length = data[2].len;
        word command = READ_WORD(p);

        fprintf (out, "    Info(0x%02x):\n", length);
        fprintf (out, "      Command: 0x%04x %s\n", command, printString (ManufacturerCommand, command));
        p += sizeof(word);
        length -= sizeof(word);
        fprintf (out, "         Info: 0x%04x %s\n", READ_WORD(p), printString (Info, READ_WORD(p)));
        p += sizeof(word);
        length -= sizeof(word);

        if (command == 0x0012) { /* Management info */
          API_PARSE _parms[3];

          if (apiParse (p, length, "bs", _parms)) {
            fprintf (out, "        command: 0x%04x %s\n",
                     _parms[0].info[0],
                     printString (_DI_MANAGEMENT_INFO_cmd,
                     _parms[0].info[0]));
            switch (_parms[0].info[0]) {
              case 1:
                if (_parms[1].len == 1) {
                  fprintf (out, "       Controllers: %d\n", _parms[1].info[0]);
								} else {
                  fprintf (out, "       --> wrong format! (expected b)\n");
                }
                return;
              case 2: {
                API_PARSE _info[3];

                fprintf (out, "        info   :\n");
                if (apiParse (_parms[1].info, _parms[1].len, "ss", _info)) {
                  parse_manufacturer_management_read_context_t context;

                  context.out  = out;
                  context.info = "         ";

                  ParseInfoElement (_info[0].info,
                                    _info[0].len,
                                    parse_manufacturer_management_read,
                                    &context);
                  if (_info[1].len != 0) {
                    API_PARSE __info[3];
                    byte* info = _info[1].info;
                    long len   = _info[1].len;

										do {
											if (apiParse (info, len, "sr", __info)) {

                        context.info = "           ";
                        fprintf (out, "      XDI adapter:\n");

                        info = __info[1].info;
                        len  = __info[1].len;

                        ParseInfoElement (__info[0].info,
                                          __info[0].len,
                                          parse_manufacturer_management_read,
                                          &context);
											} else {
                        fprintf (out, "         --> wrong format! (expected s)\n");
                        __info[1].len = 0;
											}
										} while (__info[1].len != 0);
                  }
                } else {
                  fprintf (out, "      --> wrong format! (expected ss)\n");
                }
              } return;
            }
          }
        }
      }
    } else if (subCommand == 0x80 && data[2].len >= 2) {
      byte* p = data[2].info;
      int length = data[2].len;
      word command = READ_WORD(p);


      fprintf (out, "    Info(0x%02x):\n", length);
      fprintf (out, "      Command: 0x%04x %s\n", command, printString (ManufacturerCommand, command));

      p += sizeof(word);
      length -= sizeof(word);

      if (command == 0x0001) { /* Assign PLCI */
        if (length > 1) {
          API_PARSE _parms[2];
          if (apiParse (p, length, "s", _parms)) {
            API_PARSE parms[4];
            if (apiParse (_parms[0].info, _parms[0].len, "wbbs", parms)) {
              word cmd     = READ_WORD(parms[0].info);
              byte channel = *(byte*)parms[1].info;
              byte usage   = *(byte*)parms[2].info;

              fprintf (out, "      Hardware resource: %d %s\n",
                       cmd, printString (ManufacturerAssignPLCIHardwareResource, cmd));
              fprintf (out, "      B-channel ID: %d\n", channel);
              fprintf (out, "      Usage: %d %s\n",
                       usage, printString (ManufacturerAssignPLCIUsage, usage));

              BProtocol (parms[3].info, parms[3].len, out, READ_DWORD (data[0].info), 3);
            } else {
              fprintf (out, "      --> wrong format! (expected wbbs)\n");
            }
          } else {
            fprintf (out, "      --> wrong format! (expected s)\n");
          }
        }
        return;
      } else if (command == 0x0002) { /* Advanced Codec control */
      } else if (command == 0x0003) { /* DSP control */
      } else if (command == 0x0004) { /* Signaling control */
      } else if (command == 0x0005) { /* RXT control */
      } else if (command == 0x0006) { /* IDI control */
      } else if (command == 0x0007) { /* Configuration control */
      } else if (command == 0x0008) { /* Remove Codec */
      } else if (command == 0x0009) { /* Options request */
      } else if (command == 0x000a) { /* reserved */
      } else if (command == 0x000b) { /* Negotiate B3 */
      } else if (command == 0x000c) { /* Info B3 */
      } else if (command == 0x0012) { /* Management info */
        API_PARSE _parms[3];

        if (apiParse (p+1, length-1, "bs", _parms)) {

          fprintf (out, "        command: 0x%04x %s\n",
                   _parms[0].info[0],
                   printString (_DI_MANAGEMENT_INFO_cmd, _parms[0].info[0]));

          switch (_parms[0].info[0]) {
            case 1:
             fprintf (out, "        info   : ");
             printAscii (_parms[1].info, _parms[1].len, out);
             return;
            case 2:
             fprintf (out, "        info   : ");
             printAscii (_parms[1].info, _parms[1].len, out);
             return;
          }
        } else {
          fprintf (out, "      --> wrong format! (expected bs)\n");
        }
      }

      printHex (p, length, out);
    } else if (data[2].len > 0) {
        fprintf (out, "    Manufacturer specific (0x%lx): ", data[2].len);
        printHex (data[2].info, data[2].len, out);
    }
    fprintf (out, "\n");
}
// --------------------------------------------------------------
static
void
reset_b3 (byte subCommand, API_PARSE * data, FILE * out)
{
    dword plci = READ_DWORD (data[0].info);
    switch (subCommand)
    {
      case REQ:
      case IND:
        Ncpi (data[1].info, data[1].len, out, plci);
        break;
      case CON:
        fprintf (out, "    Info: %s\n", printString (Info, READ_WORD (data[1].info)));
        break;
      case RES:
        break;
    }
}
// --------------------------------------------------------------
static
void
select_b (byte subCommand, API_PARSE * data, FILE * out)
{
    dword plci = READ_DWORD (data[0].info);
    switch (subCommand)
    {
      case REQ:
        BProtocol (data[1].info, data[1].len, out, plci, 1);
        break;
      case CON:
        fprintf (out, "    Info: %s\n", printString (Info, READ_WORD (data[1].info)));
        break;
    }
}
// --------------------------------------------------------------
static
void
portability (byte subCommand, API_PARSE * data, FILE * out)
{
    switch (subCommand)
    {
      case REQ:
        fprintf (out, "    Portability selector: %s\n", printString (PortSelect, READ_WORD (data[1].info)));
        fprintf (out, "    Call Identity (0x%lx): ", data[2].len);
        if (data[2].len)
        {
            printAscii (data[2].info, data[2].len, out);
        }
        fprintf (out, "\n");
      case IND:
        fprintf (out, "    Info: ");
        if (READ_WORD (data[1].info) == 0)
        {
            fprintf (out, "accepted\n");
        }
        else if ((READ_WORD (data[1].info) & 0xff00) == 0x3400)
        {
            fprintf (out, "0x%04x (%s)\n", READ_WORD (data[1].info), cau_q931[READ_WORD (data[1].info) & 0x007f]);
        }
        else
        {
            fprintf (out, "0x%04x\n", READ_WORD (data[1].info));
        }
        fprintf (out, "    Portability selector: %s\n", printString (PortSelect, READ_WORD (data[2].info)));
        break;
      case CON:
        fprintf (out, "    Info: %s\n", printString (Info, READ_WORD (data[1].info)));
        break;
      case RES:
        break;
    }
}
// --------------------------------------------------------------
static
void
AdditionalInfo (byte * data, long len, FILE * out, byte flag)
{
      API_PARSE parms[8];
      API_PARSE parms2[4];
    byte space[5]={' ',' ',' ',' ',0x0};
    if(flag&1) space[0]=' ';
    else space[0]=0x0;
    fprintf (out, "%s    Additional Info (0x%lx):\n", space, len);
    if (len)
    {
        if (apiParse (data, len, "ssssr", parms))
        {
            fprintf (out, "%s      B channel info (0x%lx): ", space, parms[0].len);
            if (parms[0].len)
            {
                if (parms[0].len > 1)
                {
                    fprintf (out, "%s", printString (BChannelInfo, READ_WORD (parms[0].info)));
                    if (READ_WORD (parms[0].info) == 3) // use channel allocation
                    {
                        if ((parms[0].len >= 7) && (parms[0].len <= 36))
                        {
                            fprintf (out, "\n%s        Operation: %s mode", space, READ_WORD (&parms[0].info[2]) ? "DCE" : "DTE");
                            fprintf (out, "\n%s        Channel mask array: ", space);
                            printHex (&parms[0].info[4], parms[0].len - 4, out);
                        }
                        else
                        {
                            fprintf (out, "\n%s        --> wrong format! (expected 7..36 bytes)", space);
                        }
                    }
                    else if (READ_WORD (parms[0].info) == 4) // use channel identification info element
                    {
                        fprintf (out, "\n%s        Channel identification: ", space);
                        printHex (&parms[0].info[2], parms[0].len - 2, out);
                    }
                }
                else
                {
                    fprintf (out, "\n        --> wrong format! (expected w)");
                }
            }
            fprintf (out, "\n%s      Keypad facility (0x%lx): ", space, parms[1].len);
            if (parms[1].len)
            {
                printHex (parms[1].info, parms[1].len, out);
            }
            fprintf (out, "\n%s      User user data (0x%lx): ", space, parms[2].len);
            if (parms[2].len)
            {
                printHex (parms[2].info, parms[2].len, out);
            }
            fprintf (out, "\n%s      Facility data array (0x%lx):\n", space, parms[3].len);
            if (parms[3].len)
            {
                byte *p = parms[3].info;
    long   rest = parms[3].len;
                long len = 0;
                while ( rest )
                {
     if( *p & 0x80 )                  // check for single byte IE
      len = 1;
     else
                    len = (long) (*(p + 1) + 2); // skip info element identifier
     if (len > rest)
      len = rest;
                    fprintf (out, "%s        ", space);
                    printHex (p, len, out);
                    fprintf (out, "\n");
                    p += len;
     rest -= len;
                }
            }
            if (parms[4].len)
            {
                if (apiParse (parms[4].info, parms[4].len, "s", parms))
                {
                    fprintf (out, "%s      Sending complete (0x%lx):\n", space, parms[0].len);
     if (parms[0].len)
     {
                    if (apiParse (parms[0].info, parms[0].len, "w", parms2))
                    {
                        fprintf (out, "%s        Mode: %d (%s)\n", space, READ_WORD (parms2[0].info),
                                 printString (SendingCompleteMode, READ_WORD (parms2[0].info)));
                    }
                    else
                    {
                        fprintf (out, "%s        --> wrong format! (expected w)\n", space);
                    }
                }
                }
                else
                {
                    fprintf (out, "%s        --> wrong format! (expected s)\n", space);
                }
            }
        }
        else
        {
            fprintf (out, "%s      --> wrong format! (expected ssss)\n", space);
        }
    }
}

// --------------------------------------------------------------
static
int
B1_VoIP_payload_specific_option_reserved (INFO_TABLE *info, word i) {
  while (info->value != 0xffff) {
    if (info->value == i)
      return (0);
    info++;
  }

  return (-1);
}

// --------------------------------------------------------------
static
int
B1_VoIP_payload_specific_option_reserved32 (INFO_TABLE32 *info, dword i) {
  while (info->value != 0xffffffff) {
    if (info->value == i)
      return (0);
    info++;
  }

  return (-1);
}

// --------------------------------------------------------------
static
void
BProtocol (byte * data, long len, FILE * out, dword plci, byte flag)
{
      API_PARSE parms1[8];
      API_PARSE parms2[8];
    word b1protocol = 0;
    word b2protocol = 0;
    word b3protocol = 0;
    TPlciInfo *plciInfo;
    byte space[5]={' ',' ',' ',' ',0x0};
    if(flag&2) space[0]=' ';
    else space[0]=0x0;
    fprintf (out, "%s    B protocol (0x%lx):\n", space, len);
    if (len)
    {
        if (apiParse (data, len, "wwwsssr", parms1))
        {
            b1protocol = READ_WORD (parms1[0].info);
            fprintf (out, "%s      B1 protocol: %d (%s)\n", space, b1protocol, printString (B1Prot, b1protocol));
            b2protocol = READ_WORD (parms1[1].info);
            fprintf (out, "%s      B2 protocol: %d (%s)\n", space, b2protocol, printString (B2Prot, b2protocol));
            b3protocol = READ_WORD (parms1[2].info);
            fprintf (out, "%s      B3 protocol: %d (%s)\n", space, b3protocol, printString (B3Prot, b3protocol));
            fprintf (out, "%s      B1 configuration (0x%lx): ", space, parms1[3].len);
            if (parms1[3].len)
            {
                switch (b1protocol)
                {
                  case 2:
                    if (apiParse (parms1[3].info, parms1[3].len, "wwww", parms2))
                    {
                        fprintf (out, "\n%s        Rate: %u\n", space, READ_WORD (parms2[0].info));
                        fprintf (out, "%s        Bits/char: %d\n", space, READ_WORD (parms2[1].info));
                        fprintf (out, "%s        Parity: %s\n", space, printString (Parity, READ_WORD (parms2[2].info)));
                        fprintf (out, "%s        Stop bits: %d\n", space, READ_WORD (parms2[3].info) + 1);
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wwww)\n");
                    }
                    break;
                  case 3:
                    if (apiParse (parms1[3].info, parms1[3].len, "wwww", parms2))
                    {
                        fprintf (out, "\n%s        Rate: %u\n", space, READ_WORD (parms2[0].info));
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wwww)\n");
                    }
                    break;
                  case 4:
                    if (apiParse (parms1[3].info, parms1[3].len, "wwww", parms2))
                    {
                        fprintf (out, "\n%s        Rate: %u\n", space, READ_WORD (parms2[0].info));
                        fprintf (out, "%s        Transmit Level: %d\n", space, READ_WORD (parms2[1].info));
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wwww)\n");
                    }
                    break;
                  case 7:
                  case 8:
                  case 9:
                    if (apiParse (parms1[3].info, parms1[3].len, "wwwwww", parms2))
                    {
                        fprintf (out, "\n%s        Rate: %u\n", space, READ_WORD (parms2[0].info));
                        fprintf (out, "%s        Bits/char: %d\n", space, READ_WORD (parms2[1].info));
                        fprintf (out, "%s        Parity: %s\n", space, printString (Parity, READ_WORD (parms2[2].info)));
                        fprintf (out, "%s        Stop bits: %d\n", space, READ_WORD (parms2[3].info) + 1);
                        fprintf (out, "%s        Options: 0x%x ", space, READ_WORD (parms2[4].info));
                        ModemB1Options (READ_WORD (parms2[4].info), out);
                        fprintf (out, "\n%s        Speed-Negotiation: 0x%x (%s)\n", space, READ_WORD (parms2[5].info),
                                 printString (SpeedNegotiation, READ_WORD (parms2[5].info)));
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wwwwww)\n");
                    }
                    break;

                  case 0x1f: {
                    API_PARSE b1_info[3];

                    if (apiParse (parms1[3].info, parms1[3].len, "bs", b1_info))
                    {
                      fprintf (out, "\n%s        Payload: %d %s\n",
                               space, *(byte*)b1_info[0].info,
                               printString (B1_VoIP_Payloads, *(byte*)b1_info[0].info));
                      fprintf (out, "%s          Payload dependent parameters (0x%02x):\n",
                               space, (int)b1_info[1].len);

                      switch (*(byte*)b1_info[0].info) {
                        case 4: { /* G723 G.723.1 */
                          API_PARSE info[3];

                          if (apiParse (b1_info[1].info, b1_info[1].len, "ww", info)) {
                            word i, v = READ_WORD(info[0].info);

                            fprintf (out, "%s            Options: 0x%04x\n", space, v);
                            for (i = 0; i < sizeof(word)*8; i++) {
                              if ((v & (1 << i)) != 0 ||
                                  B1_VoIP_payload_specific_option_reserved (B1_VoIP_PayloadOptionsG723, i) == 0)
                              fprintf (out, "%s              Bit%d %s: %s\n",
                                     space, i,
                                     printString (B1_VoIP_PayloadOptionsG723, i),
                                     ((v & (1 << i)) != 0) ? "on" : "off");
                            }

                            fprintf (out, "%s            Packetisation interval in sample times: %d\n",
                                     space, READ_WORD(info[1].info));
                          } else {
                            fprintf (out, "            --> wrong format! (expected ww)\n");
                          }
                        } break;

                        case 18: { /* G729 G.729 */
                          API_PARSE info[3];

                          if (apiParse (b1_info[1].info, b1_info[1].len, "ww", info)) {
                            word i, v = READ_WORD(info[0].info);

                            fprintf (out, "%s            Options: 0x%04x\n", space, v);

                            for (i = 0; i < sizeof(word)*8; i++) {
                              if ((v & (1 << i)) != 0 ||
                                  B1_VoIP_payload_specific_option_reserved (B1_VoIP_PayloadOptionsG729, i) == 0)
                              fprintf (out, "%s              Bit%d %s: %s\n",
                                     space, i,
                                     printString (B1_VoIP_PayloadOptionsG729, i),
                                     ((v & (1 << i)) != 0) ? "on" : "off");
                            }

                            fprintf (out, "%s            Packetisation interval in sample times: %d\n",
                                     space, READ_WORD(info[1].info));
                          } else {
                            fprintf (out, "            --> wrong format! (expected ww)\n");
                          }
                        } break;

                        default: {
                          API_PARSE info[3];

                          if (apiParse (b1_info[1].info, b1_info[1].len, "ww", info)) {
                            word i, v = READ_WORD(info[0].info);

                            fprintf (out, "%s            Options: 0x%04x\n", space, v);

                            for (i = 0; i < sizeof(word)*8; i++) {
                              if ((v & (1 << i)) != 0 ||
                                  B1_VoIP_payload_specific_option_reserved (B1_VoIP_PayloadOptions, i) == 0)
                              fprintf (out, "%s              Bit%d %s: %s\n",
                                     space, i,
                                     printString (B1_VoIP_PayloadOptions, i),
                                     ((v & (1 << i)) != 0) ? "on" : "off");
                            }

                            fprintf (out, "%s            Packetisation interval in sample times: %d\n",
                                     space, READ_WORD(info[1].info));
                          } else {
                            fprintf (out, "            --> wrong format! (expected ww)\n");
                          }
                        } break;
                      }
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected bs)\n");
                    }
                  } break;

                  default:
                    printHex (parms1[3].info, parms1[3].len, out);
                    fprintf (out, "\n");
                    break;
                }
            }
            else
            {
                fprintf (out, "\n");
            }
            fprintf (out, "%s      B2 configuration (0x%lx): ", space, parms1[4].len);
            if (parms1[4].len)
            {
                switch (b2protocol)
                {
                  case 0:
                  case 2:
                  case 3:
                  case 9:
                  case 11:
                  case 12:
                    if (apiParse (parms1[4].info, parms1[4].len, "bbbbs", parms2))
                    {
                        if ((b2protocol == 3) || (b2protocol == 12))
                        {
                           fprintf (out, "\n%s        TEI assignment: 0x%x ", space, *parms2[0].info);
                           if (*parms2[0].info & 0x01)
                           {
                              fprintf (out, "(fixed value %d)\n", *parms2[0].info >> 1);
                           }
                           else
                           {
                              fprintf (out, "(automatic)\n");
                           }
                        }
                        else
                        {
                           fprintf (out, "\n%s        Address A: 0x%x\n", space, *parms2[0].info);
                        }
                        fprintf (out, "%s        Address B: 0x%x\n", space, *parms2[1].info);
                        fprintf (out, "%s        Modulo Mode: 0x%x\n", space, *parms2[2].info);
                        fprintf (out, "%s        Window Size: 0x%x\n", space, *parms2[3].info);
                        fprintf (out, "%s        XID (0x%lx): ", space, parms2[4].len);
                        if (parms2[4].len)
                        {
                            printHex (parms2[4].info, parms2[4].len, out);
                        }
                        fprintf (out, "\n");
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected bbbbs)\n");
                    }
                    break;
                  case 8:
                  case 10:
                    if (apiParse (parms1[4].info, parms1[4].len, "bbbbwww", parms2))
                    {
                        fprintf (out, "\n%s        Address A: 0x%x\n", space, *parms2[0].info);
                        fprintf (out, "%s        Address B: 0x%x\n", space, *parms2[1].info);
                        fprintf (out, "%s        Modulo Mode: 0x%x\n", space, *parms2[2].info);
                        fprintf (out, "%s        Window Size: 0x%x\n", space, *parms2[3].info);
                        fprintf (out, "\n%s        Direction: %d\n", space, READ_WORD (parms2[4].info));
                        fprintf (out, "%s        Code words: 0x%x\n", space, READ_WORD (parms2[5].info));
                        fprintf (out, "%s        Max string length: 0x%x\n", space, READ_WORD (parms2[6].info));
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected bbbbwww)\n");
                    }
                    break;
                  case 7:
                    if (apiParse (parms1[4].info, parms1[4].len, "w", parms2))
                    {
                        fprintf (out, "\n%s        Options: 0x%x ", space, READ_WORD (parms2[0].info));
                        printMask16 (ModemOptions, READ_WORD (parms2[0].info), out);
                        fprintf (out, "\n");
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected w)\n");
                    }
                    break;

                  case 0x1f: {
                    API_PARSE info[4];
                    int nr_parameters = 0;

                    if (apiParse (parms1[4].info, parms1[4].len, "www", info)) {
                      nr_parameters = 3;
                    } else if (apiParse (parms1[4].info, parms1[4].len, "ww", info)) {
                      nr_parameters = 2;
                    }
                    if (nr_parameters != 0) {
                      fprintf (out, "\n%s        Maximum late packet rate in 1/1000: %d\n",
                               space, READ_WORD(info[0].info));
                      fprintf (out,   "%s        Maximum dejitter delay in milliseconds: %d\n",
                               space, READ_WORD(info[1].info));
                      if (nr_parameters > 2) {
                        fprintf (out,   "%s        Minimum dejitter delay in milliseconds: %d\n",
                                 space, READ_WORD(info[2].info));
                      }
                    } else {
                      fprintf (out, "        --> wrong format! (expected www or ww)\n");
                    }
                  } break;

                  default:
                    printHex (parms1[4].info, parms1[4].len, out);
                    fprintf (out, "\n");
                    break;
                }
            }
            else
            {
                fprintf (out, "\n");
            }
            fprintf (out, "%s      B3 configuration (0x%lx): ", space, parms1[5].len);
            if (parms1[5].len)
            {
                switch (b3protocol)
                {
                  case 1:
                  case 2:
                  case 3:
                    if (apiParse (parms1[5].info, parms1[5].len, "wwwwwwww", parms2))
                    {
                        fprintf (out, "\n%s        LIC: 0x%x\n", space, READ_WORD (parms2[0].info));
                        fprintf (out, "%s        HIC: 0x%x\n", space, READ_WORD (parms2[1].info));
                        fprintf (out, "%s        LTC: 0x%x\n", space, READ_WORD (parms2[2].info));
                        fprintf (out, "%s        HTC: 0x%x\n", space, READ_WORD (parms2[3].info));
                        fprintf (out, "%s        LOC: 0x%x\n", space, READ_WORD (parms2[4].info));
                        fprintf (out, "%s        HOC: 0x%x\n", space, READ_WORD (parms2[5].info));
                        fprintf (out, "%s        Modulo Mode: 0x%x\n", space, READ_WORD (parms2[6].info));
                        fprintf (out, "%s        Window Size: 0x%x\n", space, READ_WORD (parms2[7].info));
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wwwwwwww)\n");
                    }
                    break;
                  case 4:
                  case 5:
                    if (apiParse (parms1[5].info, parms1[5].len, "wwss", parms2))
                    {
                        if (b3protocol == 4)
                        {
                            fprintf (out, "\n%s        Resolution: 0x%x\n", space, READ_WORD (parms2[0].info));
                        }
                        else
                        {
                            fprintf (out, "\n%s        Options: 0x%x ", space, READ_WORD (parms2[0].info));
                            printMask16 (FaxOptions, READ_WORD (parms2[0].info), out);
                            fprintf (out, "\n");
                        }
                        fprintf (out, "%s        Format: 0x%x (%s)\n", space, READ_WORD (parms2[1].info),
                                 printString (Format, READ_WORD (parms2[1].info)));
                        fprintf (out, "%s        Station ID (0x%lx): ", space, parms2[2].len);
                        if (parms2[2].len)
                        {
                            long i = 0;
                            fprintf (out, "'");
                            while (i < parms2[2].len)
                            {
                                fprintf (out, "%c", parms2[2].info[i++]);
                            }
                            fprintf (out, "'");
                        }
                        fprintf (out, "\n");
                        fprintf (out, "%s        Headline (0x%lx): ", space, parms2[3].len);
                        if (parms2[3].len)
                        {
                            long i = 0;
                            fprintf (out, "'");
                            while (i < parms2[3].len)
                            {
                                fprintf (out, "%c", parms2[3].info[i++]);
                            }
                            fprintf (out, "'");
                        }
                        fprintf (out, "\n");
                    }
                    else
                    {
                        fprintf (out, "        --> wrong format! (expected wwss)\n");
                    }
                    break;

                  case 0x1f: {
                    API_PARSE info[9];
                    int nr_parameters = 0;

                    if (apiParse (parms1[5].info, parms1[5].len, "wwwwsss", info)) {
											nr_parameters = 7;
                    } else if (apiParse (parms1[5].info, parms1[5].len, "wwwwss", info)) {
											nr_parameters = 6;
                    } else if (apiParse (parms1[5].info, parms1[5].len, "wwwws", info)) {
                      nr_parameters = 5;
                    } else if (apiParse (parms1[5].info, parms1[5].len, "wwww", info)) {
                      nr_parameters = 4;
                    } else if (apiParse (parms1[5].info, parms1[5].len, "www", info)) {
                      nr_parameters = 3;
                    } else if (apiParse (parms1[5].info, parms1[5].len, "ww", info)) {
                      nr_parameters = 2;
                    }

                    if (nr_parameters != 0) {
                      fprintf (out, "\n%s        Transmit frame encapsulation: %d %s\n",
                               space,
                               READ_WORD(info[0].info) & 0x0007,
                               printString (B3_VoIP_Encapsulation, (word)(READ_WORD(info[0].info) & 0x0007)));
                      fprintf (out,   "%s        Receive  frame encapsulation: %d %s\n",
                               space,
                               READ_WORD(info[1].info) & 0x0007,
                               printString (B3_VoIP_Encapsulation, (word)(READ_WORD(info[1].info) & 0x0007)));
                      if (nr_parameters > 2) {
                        fprintf (out, "%s        Transmit header offset: %d\n",
                                 space, READ_WORD(info[2].info));
                        if (nr_parameters > 3) {
                          fprintf (out, "%s        Receive  header offset: %d\n",
                                   space, READ_WORD(info[3].info));
                          if (nr_parameters > 4) {
                            fprintf (out, "%s        Encapsulation header template (0x%02x): ",
                                     space, (int)info[4].len);
                            printHex (info[4].info, info[4].len, out);
                            fprintf (out, "\n");

                            if ((READ_WORD(info[0].info) & 0x0007) == 0x0002 &&
                                (READ_WORD(info[1].info) & 0x0007) == 0x0002 &&
                                READ_WORD(info[2].info) == 0 &&
                                READ_WORD(info[3].info) == 0 &&
                                info[4].len == 0x1c && *(byte*)info[4].info == 0x45) {
                              byte* ip_hdr_start = (byte*)info[4].info;
                              fprintf (out, "%s          IPv4\n", space);
                              fprintf (out, "%s            SRC %d.%d.%d.%d PORT %d\n",
                                       space,
                                       ip_hdr_start[12], ip_hdr_start[12+1],
                                       ip_hdr_start[12+2], ip_hdr_start[12+3],
                                       (word)ip_hdr_start[20] << 8 | (word)ip_hdr_start[21]);
                              fprintf (out, "%s            DST %d.%d.%d.%d PORT %d\n",
                                       space,
                                       ip_hdr_start[16], ip_hdr_start[16+1],
                                       ip_hdr_start[16+2], ip_hdr_start[16+3],
                                       (word)ip_hdr_start[22] << 8 | (word)ip_hdr_start[23]);
                            }
                            if (nr_parameters > 5) {
                              fprintf (out, "%s        RTP transport protocol, reserved (0x%02x): ",
                                       space, (int)info[5].len);
                              printHex (info[5].info, info[5].len, out);
                              fprintf (out, "\n");
                              if (nr_parameters > 6) {
																API_PARSE rtcp_info[7];

                                fprintf (out, "%s        RTCP transport protocol configuration (0x%02x): ",
                                         space, (int)info[6].len);
                                printHex (info[6].info, info[6].len, out);

																if (apiParse (info[6].info, info[6].len, "wwwws", rtcp_info)) {
                                  fprintf (out, "\n%s          Transmit frame encapsulation: %d %s\n",
                                           space,
                                           READ_WORD(rtcp_info[0].info) & 0x0007,
                                           printString (B3_VoIP_Encapsulation,
                                                        READ_WORD(rtcp_info[0].info) & 0x0007));
                                  fprintf (out,   "%s          Receive  frame encapsulation: %d %s\n",
                                           space,
                                           READ_WORD(rtcp_info[1].info) & 0x0007,
                                           printString (B3_VoIP_Encapsulation,
                                                        READ_WORD(rtcp_info[1].info) & 0x0007));
                                  fprintf (out, "%s          Transmit header offset: %d\n",
                                           space, READ_WORD(rtcp_info[2].info));
                                  fprintf (out, "%s          Receive  header offset: %d\n",
                                           space, READ_WORD(rtcp_info[3].info));
                                  fprintf (out, "%s          Encapsulation header template (0x%02x): ",
                                           space, (int)rtcp_info[4].len);
                                  printHex (rtcp_info[4].info, rtcp_info[4].len, out);
                                  fprintf (out, "\n");
                                  if ((READ_WORD(rtcp_info[0].info) & 0x0007) == 0x0002 &&
                                      (READ_WORD(rtcp_info[1].info) & 0x0007) == 0x0002 &&
                                      READ_WORD(rtcp_info[2].info) == 0 &&
                                      READ_WORD(rtcp_info[3].info) == 0 &&
                                      rtcp_info[4].len == 0x1c && *(byte*)rtcp_info[4].info == 0x45) {
                                    byte* ip_hdr_start = (byte*)rtcp_info[4].info;
                                    fprintf (out, "%s            IPv4\n", space);
                                    fprintf (out, "%s              SRC %d.%d.%d.%d PORT %d\n",
                                             space,
                                             ip_hdr_start[12], ip_hdr_start[12+1],
                                             ip_hdr_start[12+2], ip_hdr_start[12+3],
                                             (word)ip_hdr_start[20] << 8 | (word)ip_hdr_start[21]);
                                    fprintf (out, "%s              DST %d.%d.%d.%d PORT %d\n",
                                             space,
                                             ip_hdr_start[16], ip_hdr_start[16+1],
                                             ip_hdr_start[16+2], ip_hdr_start[16+3],
                                             (word)ip_hdr_start[22] << 8 | (word)ip_hdr_start[23]);
                                  }
																} else {
																	fprintf (out, "        --> wrong format! (expected wwwws)\n");
																}
                              }
                            }
                          }
                        }
                      }
                    } else {
                      fprintf (out, "        --> wrong format! (expected one of wwwwsss, wwwws, wwwws, wwww, www, ww)\n");
                    }
                  } break;

                  default:
                    printHex (parms1[5].info, parms1[5].len, out);
                    fprintf (out, "\n");
                    break;
                }
            }
            else
            {
                fprintf (out, "\n");
            }
            if (parms1[6].len > 0)
            {
                if (apiParse (parms1[6].info, parms1[6].len, "s", parms2))
                {
                    fprintf (out, "%s      Global configuration (0x%lx): ", space, parms2[0].len);
                    if (parms2[0].len)
                    {
                        if (apiParse (parms2[0].info, parms2[0].len, "w", parms2))
                        {
                            fprintf (out, "\n%s        B Channel Operation: %d (%s)\n", 
                                     space, READ_WORD (parms2[0].info),
                                     printString (GlobalConfig, READ_WORD (parms2[0].info)));
                        }
                        else
                        {
                            fprintf (out, "        --> wrong format! (expected w)\n");
                        }
                    }
                    else
                    {
                        fprintf (out, "\n");
                    }
                }
                else
                {
                    fprintf (out, "%s        --> wrong format! (expected s)\n", space);
                }
            }
        }
        else
        {
            fprintf (out, "%s      --> wrong format! (expected wwwsss)\n", space);
        }
    }
    // save B3 protocol value for later NCPI info (if any)
    if(flag&1)
    {
        plciInfo = PlciInfoNew (plci);
        plciInfo->B3Protocol = (byte) b3protocol;
    }
}
// --------------------------------------------------------------
static
void
Ncpi (byte * data, long len, FILE * out, dword plci)
{
    word b3protocol = 0;
      API_PARSE parms[8];
    TPlciInfo *plciInfo;
    // retrieve saved B3 protocol value
    plciInfo = PlciInfoLookup (plci);
    if (plciInfo != ((void*)0) )
        b3protocol = plciInfo->B3Protocol;
    fprintf (out, "    NCPI (0x%lx): ", len);
    if (len)
    {
        if (b3protocol == 0)
          b3protocol = dbg_capi_nspi_use_b3_protocol;

        switch (b3protocol)
        {
          case 2:
          case 3:
            if (apiParse (data, len, "bbbr", parms))
            {
                if (*parms[0].info)
                {
                    fprintf (out, "\n      D-Bit: on\n");
                }
                else
                {
                    fprintf (out, "      D-Bit: off\n");
                }
                fprintf (out, "      PVC group number: 0x%x\n", *parms[1].info);
                fprintf (out, "      PVC number: 0x%x\n", *parms[2].info);
                fprintf (out, "      Bytes following id field (0x%lx): ", parms[3].len);
                if (parms[3].len > 0)
                {
                    printHex (parms[3].info, parms[3].len, out);
                }
                fprintf (out, "\n");
            }
            else
            {
                fprintf (out, "        --> wrong format! (expected bbbl)\n");
            }
            break;
          case 4:
          case 5:
            if (len != 0)
            {
                if (apiParse (data, len, "wwwws", parms))
                {
                    fprintf (out, "\n        Rate: %d\n", READ_WORD (parms[0].info));
                    if (b3protocol == 4)
                    {
                        fprintf (out, "        Resolution: 0x%x\n", READ_WORD (parms[1].info));
                    }
                    else
                    {
                        fprintf (out, "        Options: 0x%x ", READ_WORD (parms[1].info));
                        printMask16 (FaxNcpiOptions, READ_WORD (parms[1].info), out);
                        fprintf (out, "\n");
                    }
                    fprintf (out, "        Format: 0x%x (%s)\n", READ_WORD (parms[2].info),
                             printString (Format, READ_WORD (parms[2].info)));
                    fprintf (out, "        Pages: %d\n", READ_WORD (parms[3].info));
                    fprintf (out, "        Receive ID (0x%lx): ", parms[4].len);
                    if (parms[4].len)
                    {
                        long i = 0;
                        fprintf (out, "'");
                        while (i < parms[4].len)
                        {
                            fprintf (out, "%c", parms[4].info[i++]);
                        }
                        fprintf (out, "'");
                    }
                    fprintf (out, "\n");
                }
                else
                {
                    fprintf (out, "        --> wrong format! (expected wwwws)\n");
                }
            }
            break;
          case 7:
            if (len != 0)
            {
                if (apiParse (data, len, "ww", parms))
                {
                    fprintf (out, "\n        Rate: %d\n", READ_WORD (parms[0].info));
                    fprintf (out, "        Protocol: 0x%x ", READ_WORD (parms[1].info));
                    printMask16 (ModemNcpiOptions, READ_WORD (parms[1].info), out);
                    fprintf (out, "\n");
                }
                else
                {
                    fprintf (out, "        --> wrong format! (expected ww)\n");
                }
            }
            break;

          case 0x1f:
            if (len != 0) {
              API_PARSE info[7];

              if (apiParse (data, len, "dsssss", info)) {
                {
                  dword i, v = READ_DWORD(info[0].info);

                  fprintf (out, "\n      Connect options: 0x%08x\n", v);

                  for (i = 0; i < sizeof(dword)*8; i++) {
                    if ((v & (1 << i)) != 0 ||
                        B1_VoIP_payload_specific_option_reserved32 (VoIP_NCPI_ConnectOptions, i) == 0)
                        fprintf (out, "        Bit%d %s: %s\n",
                                 i, printString32 (VoIP_NCPI_ConnectOptions, i),
                                 ((v & (1 << i)) != 0) ? "on" : "off");
                  }
                }

                fprintf (out,   "      RTP packet filter configuration (0x%02x): ", (int)info[1].len);
                printHex (info[1].info, info[1].len, out);
                fprintf (out, "\n");

                fprintf (out,   "      RTP header template in network byte order (0x%02x): ", (int)info[2].len);
                printHex (info[2].info, info[2].len, out);
                fprintf (out, "\n");
                if (info[2].len >= 12) {
                  byte* p = (byte*)info[2].info;
                  int len = (int)info[2].len - 12;
                  byte csrc_count = p[0] & 0x0f;

                  fprintf (out,   "        Version     : %d\n", p[0] >> 6);
                  fprintf (out,   "        X, Extension: %d\n", (p[0] >> 4) & 0x01);
                  fprintf (out,   "        CSRC count  : %d\n", csrc_count);
                  fprintf (out,   "        SSRC        : 0x%02x%02x%02x%02x\n", p[8], p[9], p[10], p[11]);

                  p += 12;
                  while (csrc_count != 0 && len >= 4) {
                    fprintf (out,   "        CSRC        : 0x%02x%02x%02x%02x\n", p[0], p[1], p[2], p[3]);
                    csrc_count--;
                    len -= 4;
                  }
                }

                fprintf (out,   "      RTP supported transmit payloads (0x%02x): %d mapping%s\n",
                         (int)info[3].len, (int)info[3].len/2, info[3].len/2 != 1 ? "s" : "");
                {
                  byte* p = (byte*)info[3].info;
                  int i, len = (int)info[3].len;

                  for (i = 0; i < len - 1; i += 2) {
                    fprintf (out,   "        RTP %2d - %2d %s\n",
                             p[i], p[i+1], printString(B1_VoIP_Payloads, p[i+1]));
                  }
                }

                fprintf (out,   "      RTP active receive payloads (0x%02x): %d mapping%s\n",
                         (int)info[4].len, (int)info[4].len/2, info[4].len/2 != 1 ? "s" : "");
                {
                  byte* p = (byte*)info[4].info;
                  int i, len = (int)info[4].len;

                  for (i = 0; i < len - 1; i += 2) {
                    fprintf (out,   "        RTP %2d - %2d %s\n",
                             p[i], p[i+1], printString(B1_VoIP_Payloads, p[i+1]));
                  }
                }
                fprintf (out,   "      RTP payload protocol settings (0x%02x):\n", (int)info[5].len);
                {
                  API_PARSE options[4];

                  options[2].info = info[5].info;
                  options[2].len  = info[5].len;
                  do {
                    if (apiParse (options[2].info, options[2].len, "bsr", options)) {
                      fprintf (out,   "        payload protocol selector: %d %s\n", 
                               *(byte*)options[0].info,
                               printString(B1_VoIP_Payloads, *(byte*)options[0].info));

                      if (options[1].len != 0 && *(byte*)options[0].info != 32) {
                        API_PARSE protocol_info[10];
                        const char* fmt;

                        switch (*(byte*)options[0].info) {
                          case 33:
                            fmt = "w";
                            break;

													case 28:
														fmt = "wwwwwwww";
														break;

                          default:
                            fmt = "ww";
                            break;
                        }
                        if (apiParse (options[1].info, options[1].len, fmt, protocol_info)) {
                          switch (*(byte*)options[0].info) {
                            case 4: { /* G723 G.723.1 */
                              word i, v = READ_WORD(protocol_info[0].info);

                              fprintf (out, "          Options: 0x%04x\n", v);
                              for (i = 0; i < sizeof(word)*8; i++) {
                                if ((v & (1 << i)) != 0 ||
                                    B1_VoIP_payload_specific_option_reserved
                                                  (B1_VoIP_PayloadOptionsG723, i) == 0)
                                fprintf (out, "            Bit%d %s: %s\n",
                                         i,
                                         printString (B1_VoIP_PayloadOptionsG723, i),
                                         ((v & (1 << i)) != 0) ? "on" : "off");
                              }

                              fprintf (out, "          Packetisation interval in sample times: %d\n",
                                       READ_WORD(protocol_info[1].info));
                            } break;

                            case 18: { /* G729 G.729 */
                              word i, v = READ_WORD(protocol_info[0].info);

                              fprintf (out, "          Options: 0x%04x\n", v);

                              for (i = 0; i < sizeof(word)*8; i++) {
                                if ((v & (1 << i)) != 0 || B1_VoIP_payload_specific_option_reserved
                                                                    (B1_VoIP_PayloadOptionsG729, i) == 0)
                                fprintf (out, "            Bit%d %s: %s\n",
                                         i,
                                         printString (B1_VoIP_PayloadOptionsG729, i),
                                         ((v & (1 << i)) != 0) ? "on" : "off");
                              }

                              fprintf (out, "          Packetisation interval in sample times: %d\n",
                                       READ_WORD(protocol_info[1].info));
                            } break;

														case 28: { /* AMR NR */
															word i, v = READ_WORD(protocol_info[0].info);


                              fprintf (out, "          Options: 0x%04x\n", v);

                              for (i = 0; i < sizeof(word)*8; i++) {
                                if ((v & (1 << i)) != 0 || B1_VoIP_payload_specific_option_reserved
                                                                    (B1_VoIP_PayloadOptionsAMRNB, i) == 0)
                                fprintf (out, "            Bit%d %s: %s\n",
                                         i,
                                         printString (B1_VoIP_PayloadOptionsAMRNB, i),
                                         ((v & (1 << i)) != 0) ? "on" : "off");
                              }

                              fprintf (out, "          Packetisation interval in sample times: %d\n",
                                       READ_WORD(protocol_info[1].info));

															v = READ_WORD(protocol_info[2].info);
                              fprintf (out, "          AMR options: 0x%04x\n", v);
															fprintf (out, "            Bits[3:0] Initial mode for receive direction: %u %s\n",
     v & 0x000f,
     printString (B1_VoIP_PayloadOptionsAMRNB_amrOptions_Initial_mode_for_receive_direction, v & 0x000f));
															fprintf (out,
                                       "            Bits[7:4] Interleaving length for receive packets: %u\n",
                                       (v >> 4) & 0x000f);

															fprintf (out, "            Bits[15:8] ");
															printMask16 (B1_VoIP_PayloadOptionsAMRNB_amrOptions, v & 0xff00, out);
															fprintf (out, "\n");

															v = READ_WORD(protocol_info[3].info);
                              fprintf (out, "          AMR reserved: 0x%04x\n", v);

															v = READ_WORD(protocol_info[4].info);
                              fprintf (out, "          AMR rx mode: ");
															printMask16 (B1_VoIP_PayloadOptionsAMRNB_amrModeSet, v, out);
															fprintf (out, "\n");

															v = READ_WORD(protocol_info[5].info);
                              fprintf (out, "          AMR rx mode change period: %u\n", v);

															v = READ_WORD(protocol_info[6].info);
                              fprintf (out, "          AMR tx mode: ");
															printMask16 (B1_VoIP_PayloadOptionsAMRNB_amrModeSet, v, out);
															fprintf (out, "\n");

															v = READ_WORD(protocol_info[7].info);
                              fprintf (out, "          AMR indication mode change period: %u\n", v);
														} break;

                            case 33: { /* Additional, CN, PT 19, Comfort noise */
                              word i, v = READ_WORD(protocol_info[0].info);

                              fprintf (out, "          Options: 0x%04x\n", v);

                              for (i = 0; i < sizeof(word)*8; i++) {
                                if ((v & (1 << i)) != 0 || B1_VoIP_payload_specific_option_reserved
                                                                    (B1_VoIP_PayloadOptionsCN, i) == 0)
                                fprintf (out, "            Bit%d %s: %s\n",
                                         i,
                                         printString (B1_VoIP_PayloadOptionsCN, i),
                                         ((v & (1 << i)) != 0) ? "on" : "off");
                              }
                            } break;

                            case 34: { /* Additional, DTMF dyn, DTMF as redundant data (RFC 2833) */
                              word i, v = READ_WORD(protocol_info[0].info);

                              fprintf (out, "          Options: 0x%04x\n", v);

                              for (i = 0; i < sizeof(word)*8; i++) {
                                if ((v & (1 << i)) != 0 || B1_VoIP_payload_specific_option_reserved
                                                                    (B1_VoIP_PayloadOptionsDTMF, i) == 0)
                                fprintf (out, "            Bit%d %s: %s\n",
                                         i,
                                         printString (B1_VoIP_PayloadOptionsDTMF, i),
                                         ((v & (1 << i)) != 0) ? "on" : "off");
                              }

                              fprintf (out, "          Redundancy time in milliseconds: %d\n",
                                       READ_WORD(protocol_info[1].info));

                            } break;

                            default: {
                              word i, v = READ_WORD(protocol_info[0].info);

                              fprintf (out, "          Options: 0x%04x\n", v);

                              for (i = 0; i < sizeof(word)*8; i++) {
                                if ((v & (1 << i)) != 0 ||
                                    B1_VoIP_payload_specific_option_reserved (B1_VoIP_PayloadOptions, i) == 0)
                                  fprintf (out, "            Bit%d %s: %s\n",
                                           i,
                                           printString (B1_VoIP_PayloadOptions, i),
                                           ((v & (1 << i)) != 0) ? "on" : "off");
                              }
                              fprintf (out, "          Packetisation interval in sample times: %d\n",
                                       READ_WORD(protocol_info[1].info));
                            } break;
                          }
                        } else {
                          fprintf (out, "              --> wrong format! (expected %s)\n", fmt);
                          break;
                        }
                      }
                    } else {
                      fprintf (out, "            --> wrong format! (expected bs)\n");
                      break;
                    }
                  } while (options[2].len != 0);
                }
              } else {
                fprintf (out, "        --> wrong format! (expected wsssss)\n");
              }
            }
            break;

          default:
            printHex (data, len, out);
            fprintf (out, "\n");
            break;
        }
    }
    else
    {
        fprintf (out, "\n");
    }
}
// --------------------------------------------------------------
static
void
ModemB1Options (word Options, FILE * out)
{
    long first = FALSE;
    fprintf (out, "(");
    if (Options & 0x0001)
    {
        fprintf (out, "disable retrain");
        first = TRUE;
    }
    if (Options & 0x0002)
    {
        if (first) fprintf (out, ", ");
        fprintf (out, "disable calling tone");
        first = TRUE;
    }
    if (first) fprintf (out, ", ");
    switch ((Options & 0x000c) >> 2)
    {
      case 0:
        fprintf (out, "no guard tone");
        break;
      case 1:
        fprintf (out, "guard tone 1800 Hz");
        break;
      case 2:
        fprintf (out, "guard tone 550 Hz");
        break;
    }
    fprintf (out, ", ");
    switch ((Options & 0x0030) >> 4)
    {
      case 0:
        fprintf (out, "speaker off");
        break;
      case 1:
        fprintf (out, "speaker on while negotiation");
        break;
      case 2:
        fprintf (out, "speaker on");
        break;
    }
    fprintf (out, ", ");
    switch ((Options & 0x00c0) >> 6)
    {
      case 0:
        fprintf (out, "silent");
        break;
      case 1:
        fprintf (out, "volume low");
        break;
      case 2:
        fprintf (out, "volume high");
        break;
      case 3:
        fprintf (out, "volume maximum");
        break;
    }
    fprintf (out, ")");
}
// --------------------------------------------------------------
static
void
CPartyNumber (byte * data, long len, FILE * out, long screen)
{
    long i = 0;
    fprintf (out, "0x%02X ", data[i++]);
    if (screen >= 1)
    {
        fprintf (out, "0x%02X ", data[i++]);
    }
    if (screen == 2)
    {
        fprintf (out, "0x%02X ", data[i++]);
    }
    fprintf (out, "'");
    while (i < len)
    {
        fprintf (out, "%c", data[i++]);
    }
    fprintf (out, "'");
}
/******************************************************************************/
static
void
putAsc (FILE *out, byte *data, long addr, long len)
{
      char buffer[80] ;
    long  i, j, ch ;
    if ( len > 16 )
        len = 16 ;
    if ( len <= 0 )
        return ;
    memset (&buffer[0], ' ', sizeof(buffer)) ;
    sprintf (&buffer[0], "\n  0x%04lx  ", (long) addr) ;
    for ( i = 0, j = 11 ; i < len ; ++i, j += 3 )
    {
        sprintf ((char *)&buffer[(i / 4) + j], "%02X", data[i]) ;
        buffer[(i / 4) + j + 2] = (char)' ' ;
        ch = data[i] & 0x007F ;
        if ( (ch < ' ') || (ch == 0x7F) )
            buffer[63 + i] = (char)'.' ;
        else
            buffer[63 + i] = (char)ch ;
    }
    buffer[79] = '\0' ;
    fputs (&buffer[0], out) ;
}
// --------------------------------------------------------------
static
char *
printString (INFO_TABLE * info, word value)
{
    long i;
    for (i = 0; (info[i].value != 0xffff) && (value != info[i].value); i++);
    return (info[i].name);
}
// --------------------------------------------------------------
static
char *
printString32 (INFO_TABLE32 * info, dword value)
{
    int i;
    for (i = 0; (info[i].value != 0xffffffff) && (value != info[i].value); i++);
    return (info[i].name);
}
// --------------------------------------------------------------
static
void
printHex (byte * data, long len, FILE * out)
{
    long i;
    for (i = 0; i < len; i++)
    {
        fprintf (out, "%02X ", data[i]);
    }
}
// --------------------------------------------------------------
static
void
printAscii (byte * data, long len, FILE * out)
{
    long i;
    for (i = 0; i < len; i++)
    {
        fprintf (out, "%c ", data[i]);
    }
}
// --------------------------------------------------------------
static
void
printMask16 (INFO_TABLE * info, word value, FILE * out)
{
    long i;
    long first = FALSE;
    if (!value)
    {
        return;
    }
    fprintf (out, "(");
    for (i = 0; info[i].value != 0xffff; i++)
    {
        if (value & info[i].value)
        {
            if (first)
            {
                fprintf (out, ", ");
            }
            fprintf (out, "%s", info[i].name);
            first = TRUE;
        }
    }
    if (!first && value)
    {
        fprintf (out, "%s", info[i].name);
    }
    fprintf (out, ")");
}
// --------------------------------------------------------------
static
void
printMask32 (INFO_TABLE32 * info, dword value, FILE * out)
{
    long i;
    long first = FALSE;
    if (!value)
    {
        return;
    }
    fprintf (out, "(");
    for (i = 0; info[i].value != 0xffffffffL; i++)
    {
        if (value & info[i].value)
        {
            if (first)
            {
                fprintf (out, ", ");
            }
            fprintf (out, "%s", info[i].name);
            first = TRUE;
        }
    }
    if (!first && value)
    {
        fprintf (out, "%s", info[i].name);
    }
    fprintf (out, ")");
}
// --------------------------------------------------------------
static
long
apiParse (byte * msg, long len, const char * format, API_PARSE * parms)
{
    long i, p;
    for (i = 0, p = 0; format[i]; i++)
    {
        parms[i].info = &msg[p];
        switch (format[i])
        {
          case 'b':
            parms[i].len = 1;
            p +=1;
            break;
          case 'w':
            parms[i].len = 2;
            p +=2;
            break;
          case 'd':
            parms[i].len = 4;
            p +=4;
            break;
          case 's':
            if (msg[p] == 0xff)
            {
                if (p + 2 > len)
                {
                    return (0);
                }
                parms[i].info += 3;
                parms[i].len = msg[p + 1] + (msg[p + 2] << 8);
                p += parms[i].len + 3;
            }
            else
            {
                parms[i].info += 1;
                parms[i].len = msg[p];
                p += parms[i].len + 1;
            }
            break;
          case 'r':
            parms[i].len = len - p;
            p += parms[i].len;
            break;
        }
        if (p > len)
        {
            return (0);
        }
    }
    if (parms)
    {
        parms[i].info = 0;
    }
    return (-1);
}
//-----------------------------------------------------------------------
// Info Element Parser
// calls given callback routine for every info element found with 
// the following parameters:
//  Type:  first octett of info element
//  *IE :  Pointer to start of info element, beginning with 1st octett
//  IElen: Overall octett length of info element (IE[IElen] points
//         to first octett after actual info element)
// SingleOctett: is 1 for a single octett element, else 0
// function returns number of found info elements
static int ParseInfoElement (byte *Buf,
											int  Buflen,
											ParseInfoElementCallbackProc_t callback,
											void* callback_context) {
  int pos, cnt, len;
  
  for (pos=0, cnt=0; pos < Buflen;)
  {
    // Single Octett Element
    if (*Buf &0x80)
    {
      callback(callback_context, *Buf, Buf, 1 , 1);
      pos++;
      Buf++;
      cnt++;
      continue;
    }

    // Variable length Info Element
		if (*Buf) {
    	len = (int)(Buf[1])+2;
		} else {
    	len = 1;
		}
    callback(callback_context, *Buf, Buf, len, 0);
    pos += len;
    Buf = &Buf[len];
    cnt++;
  }
  return cnt;
}

// --------------------------------------------------------------

