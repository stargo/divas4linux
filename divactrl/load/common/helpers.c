
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

#include "platform.h"
#include <stdarg.h>
#include "di_defs.h"
#include "pc.h"
#include "pr_pc.h"
#include "di.h"
#include "mi_pc.h"
#include "pc_maint.h"
#include "divasync.h"
#include "io.h"
#include "dsp_defs.h"
#include "pc_init.h"
#include "helpers.h"
/* --------------------------------------------------------------------------
  Get protocols features from the protocol file for MIPS based cards
  -------------------------------------------------------------------------- */
dword diva_get_protocol_file_features  (byte* File,
                    int offset,
                    char *IdStringBuffer,
                    dword IdBufferSize) {
 byte ch, *features = NULL ;
 dword i, featureMask = 0 ;
 for ( File += offset, i = 0 ;
       (i < IdBufferSize - 1) && (File[i] >= ' ') && (File[i] < 0x7F) ;
       ++i )
 {
  IdStringBuffer[i] = (char)(File[i]) ;
  if ( (File[i] == '[') && (File[i+1] == 'F') && (File[i+2] == '#') )
   features = (byte*)(&IdStringBuffer[i+3]) ;
 }
 IdStringBuffer[i] = '\0' ;
 if ( features )
 {
  while ( (ch = *features++) != 0 )
  {
   if ( (ch >= '0') && (ch <= '9') )
   {
    featureMask = (featureMask << 4) | (ch - '0') ;
    continue ;
   }
   ch &= 0xDF ;
   if ( (ch >= 'A') && (ch <= 'F') )
   {
    featureMask = (featureMask << 4) | (ch - 'A' + 10) ;
    continue ;
   }
   break ;
  }
 }
 return (featureMask) ;
}
/* --------------------------------------------------------------------------
  Local helpers used for protocol configuration
  -------------------------------------------------------------------------- */
#if !defined(DIVA_XDI_CFG_LIB_ONLY)
static void
CopyNumber (ADAPTER *a, unsigned char *number, unsigned long offset)
{
 dword i ;
 for ( i = 0 ; number[i] ; ++i, ++offset )
 {
  a->ram_out (a, NumToPtr(offset), number[i]) ;
 }
 a->ram_out (a, NumToPtr(offset), 0) ;
}
#endif
/*------------------------------------------*/
static dword
CopyUlongIfNonzero (ADAPTER *a, unsigned char type, dword value,
      unsigned char length, unsigned long offset)
{
 dword i;
 if (value)
 {
  a->ram_out (a, NumToPtr(offset), type) ;
  offset++ ;
  if ( !(type & 0x80) )
  {
   a->ram_out (a, NumToPtr(offset), length) ;
   offset++ ;
   for ( i = 0 ; i < length ; i++ )
   {
    a->ram_out (a, NumToPtr(offset), (byte) value) ;
    offset++ ;
    value >>= 8;
   }
  }
 }
 return (offset);
}
/*------------------------------------------*/
static dword
CopyQwordIfNonzero (ADAPTER *a, unsigned char type, dword value_lo,
                    dword value_hi, unsigned long offset)
{
 dword i;
 if (value_lo != 0 || value_hi != 0)
 {
  a->ram_out (a, NumToPtr(offset), type) ;
  offset++ ;
  a->ram_out (a, NumToPtr(offset), 8) ;
  offset++ ;
  for ( i = 0 ; i < 4 ; i++ )
  {
   a->ram_out (a, NumToPtr(offset), (byte)value_lo) ;
   offset++ ;
   value_lo >>= 8;
  }
  for ( i = 0 ; i < 4 ; i++ )
  {
   a->ram_out (a, NumToPtr(offset), (byte)value_hi) ;
   offset++ ;
   value_hi >>= 8;
  }
 }
 return (offset);
}
/*------------------------------------------*/
static dword _cdecl
CopyInfoBuffer (ADAPTER *a, unsigned char type, unsigned long offset,
                const char* format, ...)
{
  va_list ap;
  dword info_length = 0, length_offset, tmp;
  int i;

  a->ram_out (a, NumToPtr(offset), type);
  offset++;
  length_offset = offset;
  offset++;

  va_start(ap,format);
  for (i = 0; format[i] != 0; i++) {
    switch (format[i]) {
      case 'b':
        tmp = va_arg(ap,dword);
        info_length += sizeof(byte);
        a->ram_out (a, NumToPtr(offset), (byte)tmp);
        offset++;
        break;
      case 'w':
        tmp = va_arg(ap,dword);
        info_length += sizeof(word);
        a->ram_out (a, NumToPtr(offset), (byte)tmp);
        offset++;
        a->ram_out (a, NumToPtr(offset), (byte)(tmp>>8));
        offset++;
        break;
      case 'd':
        tmp = va_arg(ap,dword);
        info_length += sizeof(dword);
        a->ram_out (a, NumToPtr(offset), (byte)tmp);
        offset++;
        a->ram_out (a, NumToPtr(offset), (byte)(tmp>>8));
        offset++;
        a->ram_out (a, NumToPtr(offset), (byte)(tmp>>16));
        offset++;
        a->ram_out (a, NumToPtr(offset), (byte)(tmp>>24));
        offset++;
        break;
    }
  }
  va_end(ap);

  a->ram_out (a, NumToPtr(length_offset), info_length);

  return (offset);
}
/*------------------------------------------*/
static dword
CopyBuffer (ADAPTER *a, byte type,   byte *buffer,
                        byte length, unsigned long offset   )
{
  byte i;
  if (buffer)
  {
    a->ram_out(a, NumToPtr(offset), type);
    offset++;
    a->ram_out(a, NumToPtr(offset), length);
    offset++;
    for (i=0; i<length; i++)
    {
      a->ram_out (a, NumToPtr(offset), buffer[i]);
      offset++ ;
    }
  }
  return (offset);
}
#define PCINIT_OFFSET 224
/*
 * A zero terminated buffer will be analyzed for Memory or PC-Init Param values.
 * Valid configuration lines satisfy following rule:
 * <name>'='<value>'\n'
 *
 * name: {'Param'|'Mem'}<text>
 * value: {id|pos}[<BLANK>]{type}[<BLANK>]<const>
 * text: some chars, but not '\n' or '\0'
 * BLANK: ' ' | '\t'
 * type: 'B' - byte sequence
 *   'Y' - byte
 *   'W' - word
 *   'D' - dword
 *   'S' - string
 * const: constant number value, only for strings '"'<string>'"' is valid
 */
#define SKIP_BLANK(p) while ( (*p == ' ') || (*p == '\t') ) p++
#define str2dec(p, i) for ( i = 0; (*p>='0')&&(*p<='9'); p++ ) i = 10 * i + (*p - '0')
#define str2hex(p, i) for ( i = 0; ; p++ ) \
       if ((*p>='0')&&(*p<='9')) i = 16 * i + (*p - '0') ; \
       else if ((*p>='a')&&(*p<='f')) i = 16 * i + (*p - 'a' + 10) ; \
       else if ((*p>='A')&&(*p<='F')) i = 16 * i + (*p - 'A' + 10) ; \
       else break
static unsigned long
CopyCfgBuffer(ADAPTER *a, unsigned char *pbuffer, unsigned long offset, unsigned long ramsize) {
 unsigned char *pc, *pn, *pv, *pe, *ps, mc, bv[64] ;
 dword          v ;
 int            idx, cnt, i ;
 if ( !pbuffer ) return ( offset ) ;
 for ( pe=pc=pbuffer; (*pc != '\0'); pc++, pe++  ) {
  while ( (*pe!='\0') && (*pe!='\n') ) pe++; /* parameter end */
  SKIP_BLANK(pc) ;
  switch ( *pc ) { /* {Param|Mem}<text> - check only the first character */
  case 'P':
  case 'p':
   pn = pc ;
   *pn = 'P' ;
   break ;
  case 'M':
  case 'm':
   pn = pc ;
   *pn = 'M' ;
   break ;
  case '\0':
   return ( offset ) ;
  default:
   pc = pe ;
   if ( *pc == '\0' ) return ( offset ) ;
   continue;
  }
  while ( (*pc != '=') && (pc < pe) ) pc++ ; /* = */
  if ( pc >= pe ) {
   pc = pe ;
   if ( *pc == '\0' ) return ( offset ) ;
   continue ;
  }
  pv = pc + 1 ;
  SKIP_BLANK(pv) ;
  if ( pv >= pe ) {
   pc = pe ;
   if ( *pc == '\0' ) return ( offset ) ;
   continue ;
  }
/*  *pc = '\0'; name termination */
  pc = pv ; /* {id|pos}{type}[<blank>]<const> */
  /* scan number */
  if ( (*pc=='0') && ((*(pc+1)=='x')||(*(pc+1)=='X')) ) {
   pc += 2;
   str2hex(pc, idx) ;
  } else if ( ((*(pc+1)=='x')||(*(pc+1)=='X')) ) {
   pc++ ;
   str2hex(pc, idx) ;
  } else if ( (*pc >= '0') && (*pc <= '9') ) {
   str2dec(pc, idx) ;
  } else { /* idx is invalid */
   pc = pe ;
   if ( *pc == '\0' ) return ( offset ) ;
   continue ;
  }
  if ( (idx < 0) || (idx > 255) ) { /* only a Byte is valid */
   pc = pe ;
   if ( *pc == '\0' ) return ( offset ) ;
   continue ;
  }
  SKIP_BLANK(pc) ;
  switch ( *pc++ ) { /* {type} */
  case 'B':
  case 'Y':
   SKIP_BLANK(pc) ;
   for ( cnt = 0; pc < pe ; ) {
    if ( (*pc=='0') && ((*(pc+1)=='x')||(*(pc+1)=='X')) ) {
     pc += 2;
     str2hex(pc, v) ;
    } else if ( ((*(pc+1)=='x')||(*(pc+1)=='X')) ) {
     pc++ ;
     str2hex(pc, v) ;
    } else if ( (*pc >= '0') && (*pc <= '9') ) {
     str2dec(pc, v) ;
    } else { /* value is invalid */
     pc = pe ;
     continue ;
    }
    if ( v > 0xff ) {
     pc = pe ;
     continue ;
    }
    if ( *pn == 'M' ) { /* Mem */
     if ( idx >= PCINIT_OFFSET ) {
      pc = pe ;
      if ( *pc == '\0' ) return ( offset ) ;
      break ;
     }
     DBG_REG(("  Mem[0x%x]=0x%x", idx+cnt, (byte)v))
     a->ram_out(a, IntToPtr(idx+cnt), (byte)v) ;
     SKIP_BLANK(pc) ;
     while ( *pc=='\r' ) pc++ ;
    } else {
     if ( ( cnt + 1 ) < sizeof(bv) ) {
      bv[cnt++] = (byte)v ;
     } else {
      DBG_ERR(("CopyCfgBuffer: Temporay buffersize %d reached",
         sizeof(bv)))
      pc = pe ;
     }
     SKIP_BLANK(pc) ;
     while ( *pc=='\r' ) pc++ ;
    }
   }
   if ( *pn == 'P' ) { /* Param */
    if ( offset+2+cnt < ramsize ) {
     if ( !(idx & 0x80) ) {
      DBG_REG(("  Param[0x%x]=...(%d)",idx, cnt))
      DBG_BLK((bv, cnt))
      a->ram_out(a, NumToPtr(offset), (byte)idx) ;
      offset++ ;
      a->ram_out (a, NumToPtr(offset), (byte)cnt) ;
      offset++ ;
      for ( i = 0; i < cnt; i++ ) {
       a->ram_out (a, NumToPtr(offset), bv[i]) ;
       offset++ ;
      }
     } else {
      DBG_ERR(("There is something to do in file %s line %d", __FILE__, __LINE__))
     }
    }
   }
   break ;
  case 'W':
   SKIP_BLANK(pc) ;
   for ( ; pc < pe ; ) {
    if ( (*pc=='0') && ((*(pc+1)=='x')||(*(pc+1)=='X')) ) {
     pc += 2;
     str2hex(pc, v) ;
    } else if ( ((*(pc+1)=='x')||(*(pc+1)=='X')) ) {
     pc++ ;
     str2hex(pc, v) ;
    } else if ( (*pc >= '0') && (*pc <= '9') ) {
     str2dec(pc, v) ;
    } else { /* value is invalid */
     pc = pe ;
     continue ;
    }
    if ( v > 0xffff ) {
     pc = pe ;
     continue ;
    }
    if ( *pn == 'M' ) { /* Mem */
     if ( idx + 1 >= PCINIT_OFFSET ) {
      pc = pe ;
      continue ;
     }
     DBG_REG(("  Mem[0x%x]=0x%x", idx, (word)v))
     a->ram_out(a, IntToPtr(idx), (byte)v) ;
     idx++ ; v >>= 8 ;
     a->ram_out(a, IntToPtr(idx), (byte)v) ;
     idx++ ;
     SKIP_BLANK(pc) ;
     while ( *pc=='\r' ) pc++ ;
    } else { /* Param - only one value is valid */
     if ( offset+2+sizeof(word) < ramsize ) {
      DBG_REG(("  Param[0x%x]=0x%x",idx, (word)v))
      offset = CopyUlongIfNonzero(a, (byte)idx, v, sizeof(word), offset) ;
     }
     pc = pe ;
    }
   }
   break ;
  case 'D':
   SKIP_BLANK(pc) ;
   for ( ; pc < pe ; ) {
    if ( (*pc=='0') && ((*(pc+1)=='x')||(*(pc+1)=='X')) ) {
     pc += 2;
     str2hex(pc, v) ;
    } else if ( ((*(pc+1)=='x')||(*(pc+1)=='X')) ) {
     pc++ ;
     str2hex(pc, v) ;
    } else if ( (*pc >= '0') && (*pc <= '9') ) {
     str2dec(pc, v) ;
    } else { /* value is invalid */
     pc = pe ;
     continue ;
    }
    if ( *pn == 'M' ) { /* Mem */
     if ( idx + 3 >= PCINIT_OFFSET ) {
      pc = pe ;
      continue ;
     }
     DBG_REG(("  Mem[0x%x]=0x%x", idx, (word)v))
     a->ram_out(a, IntToPtr(idx), (byte)v) ;
     idx++ ; v >>= 8 ;
     a->ram_out(a, IntToPtr(idx), (byte)v) ;
     idx++ ;
     a->ram_out(a, IntToPtr(idx), (byte)v) ;
     idx++ ;
     a->ram_out(a, IntToPtr(idx), (byte)v) ;
     idx++ ;
     SKIP_BLANK(pc) ;
     while ( *pc=='\r' ) pc++ ;
    } else { /* Param - only one value is valid */
     if ( offset+2+sizeof(dword) < ramsize ) {
      DBG_REG(("  Param[0x%x]=0x%x",idx, (dword)v))
      offset = CopyUlongIfNonzero(a, (byte)idx, v, sizeof(dword), offset) ;
     }
     pc = pe ;
    }
   }
   break ;
  case 'Q':
   SKIP_BLANK(pc) ;
   break ;
  case 'S':
   SKIP_BLANK(pc) ;
   ps = pc++ ;
   if ( *ps == '"' ) { /* '"'string'"' */
    while ( (pc<pe) && (*pc!='"') ) pc++ ;
    if ( pc == pe ) { /* string termination missing */
     if ( *pc == '\0' ) return ( offset ) ;
     continue ;
    }
    ps++ ;
   } else {
    while ( (pc<pe) && (*pc!='\r') ) pc++ ;
    if ( pc == ps + 1 ) { /* no valid character */
     pc = pe ;
     if ( *pc == '\0' ) return ( offset ) ;
     continue ;
    }
   }
   if ( *pn == 'M' ) { /* Mem */
    if ( idx + (pc - ps) >= PCINIT_OFFSET ) {
     pc = pe ;
     if ( *pc == '\0' ) return ( offset ) ;
     continue ;
    }
    mc = *pc ; *pc = '\0' ;
    DBG_REG(("  Mem[0x%x]=%s", idx, ps))
    *pc = mc ;
    while ( ps < pc ) {
     a->ram_out(a, IntToPtr(idx), *ps) ;
     idx++; ps++ ;
    }
   } else { /* Param - only one value is valid */
    if ( offset+2+(pc - ps) < ramsize ) {
     if ( !(idx & 0x80) ) {
      if ( (pc - ps) > 255 ) { /* length is only a byte */
       DBG_ERR(("CopyCfgBuffer: too many chars %d", pc - ps))
       pc = pe ;
       if ( *pc == '\0' ) return ( offset ) ;
       break ;
      }
      a->ram_out(a, NumToPtr(offset), (byte)idx) ;
      offset++ ;
      a->ram_out (a, NumToPtr(offset), (byte)(pc - ps)) ;
      offset++ ;
      mc = *pc ; *pc = '\0' ;
      DBG_REG(("  Param[0x%x]=%s", idx, ps))
      *pc = mc ;
      while ( ps < pc ) {
       a->ram_out (a, NumToPtr(offset), *ps) ;
       offset++ ;
       ps++ ;
      }
     } else {
      DBG_ERR(("There is something to do in file %s line %d", __FILE__, __LINE__))
     }
    }
    pc = pe ;
    if ( *pc == '\0' ) return ( offset ) ;
   }
   break ;
  default:
   pc = pe ;
   if ( *pc == '\0' ) return ( offset ) ;
   continue ;
  }
  pc = pe ;
  if ( *pc == '\0' ) return ( offset ) ;
 }
 return ( offset ) ;
}
/* --------------------------------------------------------------------------
  Write configuration parameters to the card
  -------------------------------------------------------------------------- */
void diva_configure_protocol (PISDN_ADAPTER IoAdapter, dword ramSize) {
 ADAPTER *a = &IoAdapter->a ;
 unsigned long offset = PCINIT_OFFSET ;
 dword xdiFeatures ;
 if (IoAdapter->cfg_lib_memory_init == 0) {
#if !defined(DIVA_XDI_CFG_LIB_ONLY)
/* ---------------------------------------------- *
 * Hardware independent configuration parameters  *
 * ---------------------------------------------- */
 int i ;
 DBG_REG(("  Protocol = %s (id %d prot %d)",
   IoAdapter->protocol_name, IoAdapter->protocol_id,
    IoAdapter->ProtVersion & ~0x80))
 if ( IoAdapter->tei )
 {
  DBG_REG(("  TEI = %ld", IoAdapter->tei))
 }
 else
 {
  DBG_REG(("  TEI = automatic"))
 }
 DBG_REG(("  NT2 = %ld", IoAdapter->nt2))
 DBG_REG(("  DidLen = %ld", IoAdapter->DidLen))
 if ( IoAdapter->WatchDog )
 {
  DBG_REG(("  Watchdog = %ld", IoAdapter->WatchDog))
 }
 if ( IoAdapter->Permanent )
 {
  DBG_REG(("  Permanent = %ld", IoAdapter->Permanent))
 }
 if ( IoAdapter->StableL2 )
 {
  DBG_REG(("  StableL2 = %ld", IoAdapter->StableL2))
 }
 if ( IoAdapter->NoOrderCheck )
 {
  DBG_REG(("  NoOrderCheck = %ld", IoAdapter->NoOrderCheck))
 }
 if ( IoAdapter->ForceLaw )
 {
  DBG_REG(("  ForceLaw = %ld", IoAdapter->ForceLaw))
 }
 if ( IoAdapter->LowChannel )
 {
  DBG_REG(("  LowChannel = %ld", IoAdapter->LowChannel))
 }
 if ( IoAdapter->NoHscx30 ) {
  DBG_REG(("  NoHscx30   = %ld", IoAdapter->NoHscx30))
 }
 else{
  if(IoAdapter->Properties.Channels > 2 &&
    IoAdapter->Channels <= (IoAdapter->InitialDspInfo >> 16) &&
    IoAdapter->Channels < IoAdapter->Properties.Channels){
   IoAdapter->NoHscx30=1;
   DBG_REG(("  NoHscx30   = %ld", IoAdapter->NoHscx30))
  }
 }
 if ( IoAdapter->Properties.Channels <= 2 )
 {
  DBG_REG(("  OAD1  = \"%s\"", IoAdapter->Oad1))
  DBG_REG(("  OSA1  = \"%s\"", IoAdapter->Osa1))
  DBG_REG(("  SPID1 = \"%s\"", IoAdapter->Spid1))
  DBG_REG(("  OAD2  = \"%s\"", IoAdapter->Oad2))
  DBG_REG(("  OSA2  = \"%s\"", IoAdapter->Osa2))
  DBG_REG(("  SPID2 = \"%s\"", IoAdapter->Spid2))
 }
 else
 {
  DBG_REG(("  CRC4  = %ld", IoAdapter->crc4))
  switch ( IoAdapter->protocol_id ) {
  default: break ;
  case PROTTYPE_RBSCAS:
  case PROTTYPE_R2CAS:
   if ( (i = IoAdapter->Spid1[0]) ) {
    DBG_REG(("  RbsAnswerDelay = %ld", IoAdapter->Spid1[1]))
    DBG_REG(("  RbsConfig = %ld", IoAdapter->Spid1[2]))
    DBG_REG(("  RbsDigitTimeout = %ld", IoAdapter->Spid1[3]))
    DBG_REG(("  RbsBearerCap = %ld", IoAdapter->Spid1[4]))
    DBG_PRV0(("  RbsDebug = %x%x%x%x",
       IoAdapter->Spid1[5],IoAdapter->Spid1[6],IoAdapter->Spid1[7],IoAdapter->Spid1[8] ))
   }
   break ;
  }
 }
 if ( IoAdapter->L1TristateOrQsig )
 {
  DBG_REG(("  L1TristateOrQsig = 0x%x", IoAdapter->L1TristateOrQsig))
 }
 DBG_REG(("  DSPInfo = 0x%lx(0x%lx)",
   IoAdapter->InitialDspInfo, IoAdapter->InitialDspInfo << 14))
 a->ram_out (a, (void *) 8, (byte)IoAdapter->tei) ;
 a->ram_out (a, (void *) 9, (byte)IoAdapter->nt2) ;
 if ( IoAdapter->Properties.Family == FAMILY_MAESTRA )
  a->ram_out (a, (void *)10, (byte)IoAdapter->DidLen) ;
 else /* not availabble for S Cards */
  a->ram_out (a, (void *)10, (byte)0) ;
 a->ram_out (a, (void *)11, (byte)IoAdapter->WatchDog) ;
 a->ram_out (a, (void *)12, (byte)IoAdapter->Permanent) ;
 if ( IoAdapter->Properties.Family == FAMILY_MAESTRA )
  a->ram_out (a, (void *)13, (byte)IoAdapter->L1TristateOrQsig) ;
 else
  a->ram_out (a, (void *)13, (byte)0) ;
 a->ram_out (a, (void *)14, (byte)IoAdapter->StableL2) ;
 a->ram_out (a, (void *)15, (byte)IoAdapter->NoOrderCheck) ;
 if ( IoAdapter->Properties.Family == FAMILY_MAESTRA )
  a->ram_out (a, (void *)16, (byte)IoAdapter->ForceLaw) ;
 else
  a->ram_out (a, (void *)16, (byte)0) ;
 a->ram_out (a, (void *)17, (byte)0);
 a->ram_out (a, (void *)18, (byte)IoAdapter->LowChannel) ;
 a->ram_out (a, (void *)19, (byte)IoAdapter->ProtVersion) ;
 a->ram_out (a, (void *)20, (byte)IoAdapter->crc4) ;
 a->ram_out (a, (void *)21, (byte)(IoAdapter->NoHscx30 & 0x07));
 if ( IoAdapter->Properties.Channels <= 2 )
 {
  CopyNumber (a, &IoAdapter->Oad1[0],   32) ;
  CopyNumber (a, &IoAdapter->Osa1[0],   64) ;
  CopyNumber (a, &IoAdapter->Spid1[0],  96) ;
  CopyNumber (a, &IoAdapter->Oad2[0],  128) ;
  CopyNumber (a, &IoAdapter->Osa2[0],  160) ;
  CopyNumber (a, &IoAdapter->Spid2[0], 192) ;
 } else {
  switch ( IoAdapter->protocol_id ) {
  default : break ;
  case PROTTYPE_RBSCAS:
  case PROTTYPE_R2CAS:
   if ( (i = IoAdapter->Spid1[0]) ) {
    /* take the Spid1 area, because Spids are not used */
    while ( i >= 0 ) {
     a->ram_out (a, IntToPtr(i+96), (byte)IoAdapter->Spid1[i]);
     i-- ;
    }
   }
   break ;
  }
 }
/*
 * Special modem/fax tuneable parameters
 */
 if ( IoAdapter->ModemGuardTone )
 {
  DBG_REG(("  ModemGuardTone = %ld", IoAdapter->ModemGuardTone))
  offset = CopyUlongIfNonzero (a, PCINIT_MODEM_GUARD_TONE,
          IoAdapter->ModemGuardTone, 1, offset);
 }
 if  ( IoAdapter->ModemMinSpeed )
 {
  DBG_REG(("  ModemMinSpeed = %ld", IoAdapter->ModemMinSpeed))
  offset = CopyUlongIfNonzero (a, PCINIT_MODEM_MIN_SPEED,
          IoAdapter->ModemMinSpeed, 2, offset);
 }
 if  ( IoAdapter->ModemMaxSpeed )
 {
  DBG_REG(("  ModemMaxSpeed = %ld", IoAdapter->ModemMaxSpeed))
  offset = CopyUlongIfNonzero (a, PCINIT_MODEM_MAX_SPEED,
          IoAdapter->ModemMaxSpeed, 2, offset);
 }
 if  ( IoAdapter->ModemOptions )
 {
  DBG_REG(("  ModemOptions = 0x%08lx", IoAdapter->ModemOptions))
  offset = CopyUlongIfNonzero (a, PCINIT_MODEM_PROTOCOL_OPTIONS,
          IoAdapter->ModemOptions, 2, offset);
 }
 if  ( IoAdapter->ModemOptions2 )
 {
  DBG_REG(("  ModemOptions2 = 0x%08lx", IoAdapter->ModemOptions2))
  offset = CopyUlongIfNonzero (a, PCINIT_MODEM_OPTIONS,
          IoAdapter->ModemOptions2, 4, offset);
 }
 if  ( IoAdapter->ModemNegotiationMode )
 {
  DBG_REG(("  ModemNegotiationMode = 0x%08lx", IoAdapter->ModemNegotiationMode))
  offset = CopyUlongIfNonzero (a, PCINIT_MODEM_NEGOTIATION_MODE,
          IoAdapter->ModemNegotiationMode, 1, offset);
 }
 if  ( IoAdapter->ModemModulationsMask )
 {
  DBG_REG(("  ModemModulationsMask = 0x%08lx", IoAdapter->ModemModulationsMask))
  offset = CopyUlongIfNonzero (a, PCINIT_MODEM_MODULATIONS_MASK,
          IoAdapter->ModemModulationsMask, 3, offset);
 }
 if  ( IoAdapter->ModemTransmitLevel )
 {
  DBG_REG(("  ModemTransmitLevel = %ld", IoAdapter->ModemTransmitLevel))
  offset = CopyUlongIfNonzero (a, PCINIT_MODEM_TRANSMIT_LEVEL,
          IoAdapter->ModemTransmitLevel, 1, offset);
 }
 if  ( IoAdapter->ModemCarrierWaitTimeSec )
 {
  DBG_REG(("  Modem Carrier Wait Time = %d Sec",
        IoAdapter->ModemCarrierWaitTimeSec))
  offset = CopyUlongIfNonzero (a, PCINIT_MODEM_CARRIER_WAIT_TIME,
        (dword)IoAdapter->ModemCarrierWaitTimeSec, 1, offset);
 }
 if  ( IoAdapter->ModemCarrierLossWaitTimeTenthSec )
 {
  DBG_REG(("  Modem Carrier Loss Wait Time = %d Sec/10",
        IoAdapter->ModemCarrierLossWaitTimeTenthSec))
  offset = CopyUlongIfNonzero (a, PCINIT_MODEM_CARRIER_LOSS_TIME,
        (dword)IoAdapter->ModemCarrierLossWaitTimeTenthSec, 1, offset);
 }
 if  ( IoAdapter->FaxOptions )
 {
  DBG_REG(("  FaxOptions = 0x%08lx", IoAdapter->FaxOptions))
  offset = CopyUlongIfNonzero (a, PCINIT_FAX_OPTIONS,
          IoAdapter->FaxOptions, 4, offset);
 }
 if  ( IoAdapter->FaxMaxSpeed )
 {
  DBG_REG(("  FaxMaxSpeed = %ld", IoAdapter->FaxMaxSpeed))
  offset = CopyUlongIfNonzero (a, PCINIT_FAX_MAX_SPEED,
          IoAdapter->FaxMaxSpeed, 2, offset);
 }
 if  ( IoAdapter->Part68LevelLimiter )
 {
  DBG_REG(("  Part68LevelLimiter = %ld", IoAdapter->Part68LevelLimiter))
  offset = CopyUlongIfNonzero (a, PCINIT_PART68_LIMITER,
                               IoAdapter->Part68LevelLimiter, 1, offset);
 }
 if ( IoAdapter->QsigDialect )
 {
  DBG_REG(("  QsigDialect = 0x%x", IoAdapter->QsigDialect))
  offset = CopyUlongIfNonzero (a, PCINIT_QSIG_DIALECT,
                                 (dword)IoAdapter->QsigDialect, 1, offset);
 }
/*
 * Special North American parameters - BRI only
 */
 if ( IoAdapter->UsEktsNumCallApp ) {
  DBG_REG(("  Number Call Appearences %ld", IoAdapter->UsEktsNumCallApp))
  offset = CopyUlongIfNonzero (a, PCINIT_US_EKTS_CACH_HANDLES,
          IoAdapter->UsEktsNumCallApp, 1, offset);
 }
 if ( IoAdapter->UsEktsFeatAddConf ) {
  DBG_REG(("  Begin Conference Feature Activator = 0x%x", IoAdapter->UsEktsFeatAddConf))
  offset = CopyUlongIfNonzero (a, PCINIT_US_EKTS_BEGIN_CONF,
         (dword)IoAdapter->UsEktsFeatAddConf, 1, offset);
 }
 if ( IoAdapter->UsEktsFeatRemoveConf ) {
  DBG_REG(("  Drop Conference Feature Activator = =x%x", IoAdapter->UsEktsFeatRemoveConf))
  offset = CopyUlongIfNonzero (a, PCINIT_US_EKTS_DROP_CONF,
         (dword)IoAdapter->UsEktsFeatRemoveConf, 1, offset);
 }
 if ( IoAdapter->UsEktsFeatCallTransfer ) {
  DBG_REG(("  Call Transfer Feature Activator = 0x%x", IoAdapter->UsEktsFeatCallTransfer))
  offset = CopyUlongIfNonzero (a, PCINIT_US_EKTS_CALL_TRANSFER,
         (dword)IoAdapter->UsEktsFeatCallTransfer, 1, offset);
 }
 if ( IoAdapter->UsEktsFeatMsgWaiting ) {
  DBG_REG(("  Message Waiting = 0x%x", IoAdapter->UsEktsFeatMsgWaiting))
  offset = CopyUlongIfNonzero (a, PCINIT_US_EKTS_MWI,
         (dword)IoAdapter->UsEktsFeatMsgWaiting, 1, offset);
 }
 if ( IoAdapter->ForceVoiceMailAlert ) {
  DBG_REG(("  Force Voice Mail Alert = 0x%x", IoAdapter->ForceVoiceMailAlert))
  offset = CopyUlongIfNonzero (a, PCINIT_FORCE_VOICE_MAIL_ALERT,
         (dword)IoAdapter->ForceVoiceMailAlert, 1, offset);
 }
 if ( IoAdapter->DisableAutoSpid) {
  DBG_REG(("  Disable Auto SPID = 0x%x", IoAdapter->DisableAutoSpid))
  offset = CopyUlongIfNonzero (a, PCINIT_DISABLE_AUTOSPID_FLAG,
         (dword)IoAdapter->DisableAutoSpid, 1, offset);
 }
/*
 * it's possible to generate a ringtone, if the switch doesn't take this task
 */
 if ( IoAdapter->GenerateRingtone ) {
  DBG_REG(("  GenerateRingtone = 0x%x", IoAdapter->GenerateRingtone))
  offset = CopyUlongIfNonzero (a, PCINIT_RINGERTONE_OPTION,
         (dword)IoAdapter->GenerateRingtone, 1, offset);
 }
/*
 * Parameter for unchannelized protocols T1UNCH/E1UNCH
 */
 if ( IoAdapter->BChMask )
 {
  DBG_REG(("  BChMask = 0x%08lx", IoAdapter->BChMask))
  offset = CopyUlongIfNonzero (a, PCINIT_UNCHAN_B_MASK,
                               IoAdapter->BChMask, 4, offset);
 }
/*
 * PIAFS link turnaround time, in PIAFS frames
 */
 if ( IoAdapter->PiafsLinkTurnaroundInFrames )
 {
  DBG_REG(("  PIAFS Link Turnaround = %d frames",
           IoAdapter->PiafsLinkTurnaroundInFrames))
  offset = CopyUlongIfNonzero (a, PCINIT_PIAFS_TURNAROUND_FRAMES,
                               IoAdapter->PiafsLinkTurnaroundInFrames, 1, offset);
 }
/*
 * QSIG Features
 */
 if ( IoAdapter->QsigFeatures )
 {
  DBG_REG(("  QSIG Features = %04x", IoAdapter->QsigFeatures))
  offset = CopyUlongIfNonzero (a, PCINIT_QSIG_FEATURES,
                               IoAdapter->QsigFeatures, 2, offset);
 }
/*
 * Parameter to prevent D channel signalling
 */
 if ( IoAdapter->nosig )
 {
  DBG_REG(("  nosig = %d", IoAdapter->nosig))
  offset = CopyUlongIfNonzero (a, PCINIT_NO_SIGNALLING,
                               IoAdapter->nosig, 1, offset);
 }
/*
 * Amount of TEI's that adapter will support in P2MP mode
 */
 if ( IoAdapter->BriLayer2LinkCount )
 {
  DBG_REG(("  L2 link count = %d", IoAdapter->BriLayer2LinkCount))
  offset = CopyUlongIfNonzero (a, PCINIT_L2_COUNT,
                               IoAdapter->BriLayer2LinkCount, 1, offset);
 }
/*
 * Supplementary Services Features
 */
 if ( IoAdapter->SupplementaryServicesFeatures )
 {
  DBG_REG(("  Suppl. Services Features = 0x%08lx", IoAdapter->SupplementaryServicesFeatures))
  offset = CopyUlongIfNonzero (a, PCINIT_SUPPL_SERVICE_FEATURES,
                               IoAdapter->SupplementaryServicesFeatures, 4, offset);
 }
/*
 * R2 Dialect
 */
 if ( IoAdapter->R2Dialect )
 {
  DBG_REG(("  R2Dialect = 0x%08lx", IoAdapter->R2Dialect))
  offset = CopyUlongIfNonzero (a, PCINIT_R2_DIALECT,
                               IoAdapter->R2Dialect, 4, offset);
 }
/*
 * R2 CtryLength
 */
 if ( IoAdapter->R2CtryLength )
 {
  DBG_REG(("  R2CtryLength = %d", IoAdapter->R2CtryLength))
  offset = CopyUlongIfNonzero (a, PCINIT_R2_CTRYLENGTH,
                               IoAdapter->R2CtryLength, 1, offset);
 }
/*
 * R2 Cas Options
 */
 if ( IoAdapter->R2CasOptions )
 {
  DBG_REG(("  R2CasOptions = 0x%08lx", IoAdapter->R2CasOptions))
  offset = CopyUlongIfNonzero (a, PCINIT_R2_CASOPTIONS,
                               IoAdapter->R2CasOptions, 4, offset);
 }
/*
 * V.34 Fax Options
 */
 if ( IoAdapter->FaxV34Options )
 {
  DBG_REG(("  V.34 Fax Options = 0x%08lx", IoAdapter->FaxV34Options))
  offset = CopyUlongIfNonzero (a, PCINIT_FAX_V34_OPTIONS,
                               IoAdapter->FaxV34Options, 4, offset);
 }
/*
 * Disabled DSP Mask
 */
 if ( IoAdapter->DisabledDspMask )
 {
  DBG_REG(("  Disabled DSP Mask = 0x%08lx", IoAdapter->DisabledDspMask))
  offset = CopyUlongIfNonzero (a, PCINIT_DISABLED_DSPS_MASK,
                               IoAdapter->DisabledDspMask, 4, offset);
 }
/*
 * Alert Timeout (in 20 mSec steps)
 */
 if ( IoAdapter->AlertToIn20mSecTicks )
 {
  DBG_REG(("  AlsertTo = %d mSec", IoAdapter->AlertToIn20mSecTicks*20))
  offset = CopyUlongIfNonzero (a, PCINIT_ALERTTO,
                               IoAdapter->AlertToIn20mSecTicks, 2, offset);
 }
/*
 * Modem EYE Setup
 */
 if ( IoAdapter->ModemEyeSetup )
 {
  DBG_REG(("  Modem EYE Setup = 0x%04x", IoAdapter->ModemEyeSetup ))
  offset = CopyUlongIfNonzero (a, PCINIT_MODEM_EYE_SETUP,
                               IoAdapter->ModemEyeSetup, 2, offset);
 }
/*
 * CCBS Rel Timer
 */
 if ( IoAdapter->CCBSRelTimer )
 {
  DBG_REG(("  CCBS Rel. Timer = %d", IoAdapter->CCBSRelTimer))
  offset = CopyUlongIfNonzero (a, PCINIT_CCBS_REL_TIMER,
                               IoAdapter->CCBSRelTimer, 1, offset);
 }
 if ( IoAdapter->DiscAfterProgress )
 {
  DBG_REG(("  Disc After Progress is %s", IoAdapter->DiscAfterProgress ? "OFF" : "ON"))
  offset = CopyUlongIfNonzero (a, PCINIT_DISCAFTERPROGRESS,
                               IoAdapter->DiscAfterProgress, 1, offset);
 }
 if ( IoAdapter->AniDniLimiter[1] )
 {
  dword tmp = IoAdapter->AniDniLimiter[0]                |
              (dword)(IoAdapter->AniDniLimiter[1] <<  8) |
              (dword)(IoAdapter->AniDniLimiter[2] << 16);
  DBG_REG(("  AniDniLimiter is: %02x/%02x/%02x",
      IoAdapter->AniDniLimiter[0],
      IoAdapter->AniDniLimiter[1],
      IoAdapter->AniDniLimiter[2]))
  offset = CopyUlongIfNonzero (a, PCINIT_ANIDNILIMITER, tmp, 3, offset);
 }
 if  ( IoAdapter->LiViaHostMemory )
 {
  DBG_REG(("  LiViaHostMemory = %ld", IoAdapter->LiViaHostMemory))
  offset = CopyUlongIfNonzero (a, PCINIT_XCONNECT_EXPORT,
                               IoAdapter->LiViaHostMemory, 1, offset);
 }
/*
 * Layer 1 transmit attenuation
 */
 if ( IoAdapter->TxAttenuation )
 {
  DBG_REG(("  TxAttenuation = %d", IoAdapter->TxAttenuation ))
  offset = CopyUlongIfNonzero (a, PCINIT_TXATTENUATION,
                               IoAdapter->TxAttenuation, 1, offset);
 }
#endif
 } else {
/* --------------------------------------------- *
 * Hardware independent configuration parameters *
 * as supplied by CFGLib.                        *
 * --------------------------------------------- */
   unsigned long i;
  DBG_REG(("  CfgLib init memory length = %lu", IoAdapter->cfg_lib_memory_init_length))
  for (i = 0; i < IoAdapter->cfg_lib_memory_init_length; i++) {
   a->ram_out (a, NumToPtr(i), IoAdapter->cfg_lib_memory_init[i]);
  }
  offset = IoAdapter->cfg_lib_memory_init_length - 1;
 }
/* --------------------------------------------- *
 * Hardware dependent configuration parameters   *
 * which are not know to configuration           *
 * application.                                  *
 * --------------------------------------------- */
 DBG_REG(("  InitialDspInfo = 0x%02x", (byte)IoAdapter->InitialDspInfo))
 a->ram_out (a, (void *)22, (byte)IoAdapter->InitialDspInfo) ;
 DBG_REG(("  CardType = 0x%02x", (byte)IoAdapter->cardType))
 a->ram_out (a, (void *)26, (byte)(IoAdapter->cardType)) ;
/*
 * DSP image length (Used internally by Diva adapter)
 */
 if ( IoAdapter->DspImageLength )
 {
  DBG_REG(("  Dsp image length = 0x%08lx", IoAdapter->DspImageLength))
  offset = CopyUlongIfNonzero (a, PCINIT_DSP_IMAGE_LENGTH,
                               IoAdapter->DspImageLength, 4, offset);
 }
/*
 * Adapter serial number
 */
 if ( IoAdapter->serialNo )
 {
  DBG_REG(("  Serial No = %d", IoAdapter->serialNo))
  offset = CopyUlongIfNonzero (a, PCINIT_CARD_SN,
                               IoAdapter->serialNo, 4, offset);
 }
/*
 * Hardware Information structure
 */
 if ( IoAdapter->hw_info[0] &&
     (IoAdapter->hw_info[0] < sizeof(IoAdapter->hw_info)))
 {
  DBG_REG(("  Hardware Information structure available"));
  offset = CopyBuffer (a, PCINIT_CARD_HIS, IoAdapter->hw_info,
                       sizeof(IoAdapter->hw_info), offset     );
 }
/*
 * Adapter number
 */
 if ( IoAdapter->ControllerNumber )
 {
  DBG_REG(("  Port No = %d", IoAdapter->ControllerNumber))
  offset = CopyUlongIfNonzero (a, PCINIT_CARD_PORT,
                               (byte)IoAdapter->ControllerNumber, 1, offset);
 }
 if  ( IoAdapter->fpga_features )
 {
  DBG_REG(("  FPGAFeatures = 0x%08lx", IoAdapter->fpga_features))
  offset = CopyUlongIfNonzero (a, PCINIT_FPGA_FEATURES,
          IoAdapter->fpga_features, 1, offset);
 }
 xdiFeatures = 0 ;
 if  ( IoAdapter->features & PROTCAP_CMA_ALLPR )
  xdiFeatures |= PCINIT_XDI_CMA_FOR_ALL_NL_PRIMITIVES ;
 if ( xdiFeatures )
 {
  DBG_REG(("  XDIFeatures = 0x%08lx", xdiFeatures))
  offset = CopyUlongIfNonzero (a, PCINIT_XDI_FEATURES,
                               xdiFeatures, 1, offset);
 }
 {
  dword data[3] = { 0, 0, 0 };
#if (defined(DIVA_IDI_RX_DMA) || defined(DIVA_SOFTIP)) /* { */
#if !defined(DIVA_SOFTIP)
  data[2] = DIVA_DMA_CREATE_CARD_ID(DIVA_DMA_CARD_ID_SERVER, IoAdapter->ANum);
#else
  data[2] = DIVA_DMA_CREATE_CARD_ID(DIVA_DMA_CARD_ID_SOFT_IP, IoAdapter->ANum);
#endif
  data[0] = (dword)IoAdapter->sdram_bar;
  offset = CopyBuffer (a, PCINIT_CARD_ADDRESS,
     (byte*)&data[0], sizeof(data), offset);
#else /* } { */
  offset = CopyUlongIfNonzero (a, PCINIT_CARD_ADDRESS,
          IoAdapter->sdram_bar, 4, offset);
#endif /* } */
    DBG_REG(("  CardAddress = 0x%08lx, CardID:%08x", IoAdapter->sdram_bar, data[2]))
 }
 if (IoAdapter->host_vidi.vidi_active != 0) {
  dword vidi_info[8];
  vidi_info[0] = IoAdapter->host_vidi.req_buffer_base_dma_magic;
  vidi_info[1] = IoAdapter->host_vidi.ind_buffer_base_dma_magic;
  vidi_info[2] = IoAdapter->host_vidi.dma_segment_length;
  vidi_info[3] = IoAdapter->host_vidi.dma_req_buffer_length;
  vidi_info[4] = IoAdapter->host_vidi.dma_ind_buffer_length;
  vidi_info[5] = IoAdapter->host_vidi.remote_indication_counter_offset;
  vidi_info[6] = IoAdapter->host_vidi.req_buffer_base_dma_magic_hi;
  vidi_info[7] = IoAdapter->host_vidi.ind_buffer_base_dma_magic_hi;

  DBG_REG(("  VIDI active:"))
  DBG_REG(("    req base          : %08x:%08x", vidi_info[0], vidi_info[6]))
  DBG_REG(("    ind base          : %08x:%08x", vidi_info[1], vidi_info[7]))
  DBG_REG(("    segment length    : %d",   vidi_info[2]))
  DBG_REG(("    req buffer length : %d",   vidi_info[3]))
  DBG_REG(("    ind buffer length : %d",   vidi_info[4]))
  DBG_REG(("    ind counter offset: %d",   vidi_info[5]))
  offset = CopyBuffer (a, PCINIT_VIDI_INFO, (byte*)&vidi_info[0], sizeof(vidi_info), offset);
 }
#if (defined(DIVA_IDI_RX_DMA) || defined(DIVA_SOFTIP))
 if ( IoAdapter->dma_map ) {
  unsigned long dma_magic_lo;
  unsigned long dma_magic_hi;
  void* local_addr;
  int i, nr, LiChannels = IoAdapter->Properties.nrExportXconnectDescriptors[0][IoAdapter->xconnectExportMode];

	DBG_TRC(("Alloc %d LI exports", LiChannels))

  for (i = 0; i < LiChannels;)
  {
   if ((nr = diva_alloc_dma_map_entry ((struct _diva_dma_map_entry*)IoAdapter->dma_map)) < 0)
   {
    break;
   }
   diva_get_dma_map_entry ((struct _diva_dma_map_entry*)IoAdapter->dma_map,
               nr, &local_addr, &dma_magic_lo);
   diva_get_dma_map_entry_hi ((struct _diva_dma_map_entry*)IoAdapter->dma_map,
                 nr, &dma_magic_hi);
   if (dma_magic_hi == 0) {
    DBG_REG(("  XCONNECT DMA[%d] = 0x%08lx", i, (dword)dma_magic_lo))
    offset = CopyUlongIfNonzero (a, PCINIT_XCONNECT_EXPORT_MEMORY, (dword)dma_magic_lo, 4, offset);
   } else {
    DBG_REG(("  XCONNECT DMA[%d] = 0x%08lx:0x%08lx", i, (dword)dma_magic_lo, (dword)dma_magic_hi))
    offset = CopyQwordIfNonzero (a, PCINIT_XCONNECT_EXPORT_MEMORY, (dword)dma_magic_lo, (dword)dma_magic_hi, offset);
   }
      i++;
  }
 }
#endif

  /*
    Clock data memory
    */
  {
    int mem_length = MAX(sizeof(IDI_SYNC_REQ),(sizeof(DESCRIPTOR)*MAX_DESCRIPTORS));
    byte* mem = (byte*)diva_os_malloc(0, mem_length);

    if (mem != 0) {
      IDI_CALL dadapter_request = 0;
      DESCRIPTOR* t = (DESCRIPTOR*)mem;
      int i;

      memset (t, 0x00, sizeof(*t)*MAX_DESCRIPTORS);
      diva_os_read_descriptor_array (t, sizeof(*t)*MAX_DESCRIPTORS);

      for (i = 0; i < MAX_DESCRIPTORS; i++) {
        if (t[i].type == IDI_DADAPTER) {
          dadapter_request = t[i].request;
          break;
        }
      }

      if (dadapter_request != 0) {
        IDI_SYNC_REQ* sync_req = (IDI_SYNC_REQ*)mem;

        memset (sync_req, 0x00, sizeof(*sync_req));

        sync_req->xdi_clock_data.Rc = IDI_SYNC_REQ_XDI_GET_CLOCK_DATA;
        (*dadapter_request)((ENTITY*)sync_req);
        if (sync_req->xdi_clock_data.Rc == 0xff) {
          DBG_REG(("  CLOCK DATA = 0x%08lx:0x%08lx base:%d length:%d",
                  sync_req->xdi_clock_data.info.bus_addr_lo,
                  sync_req->xdi_clock_data.info.bus_addr_hi,
                  64 + IoAdapter->ANum*64,
                  sync_req->xdi_clock_data.info.length))

          offset = CopyInfoBuffer (a, PCINIT_CLOCK_REF_MEMORY, offset, "ddww",
                                   sync_req->xdi_clock_data.info.bus_addr_lo,
                                   sync_req->xdi_clock_data.info.bus_addr_hi,
                                   64 + IoAdapter->ANum*64,
                                   sync_req->xdi_clock_data.info.length);
        }
      }

      diva_os_free (0, mem);
    }
 }

 if ( IoAdapter->PcCfgBuffer ) {
  offset = CopyCfgBuffer(a, IoAdapter->PcCfgBuffer, offset, ramSize-1) ;
 }
 if ( IoAdapter->PcCfgBufferFile ) {
  offset = CopyCfgBuffer(a, IoAdapter->PcCfgBufferFile, offset, ramSize-1) ;
 }
 a->ram_out (a, NumToPtr(offset), 0);
 offset++ ;
 DBG_REG(("Used configuration space %d of %d Bytes!", offset, ramSize))
}
