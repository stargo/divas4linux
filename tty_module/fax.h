
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

#ifndef _FAX_H_
#define _FAX_H_

typedef int (* FAX_CMD) (struct ISDN_PORT *P, int Req, int Val, byte **Str);


void faxRsp (struct ISDN_PORT *P, byte *Text);

/* the common callout fuctions */

void             faxInit     (struct ISDN_PORT *P);
void             faxFinit    (struct ISDN_PORT *P);
void             faxSetClassSupport (struct ISDN_PORT *P);
int              faxPlusF    (struct ISDN_PORT *P, byte **Arg);
struct T30_INFO *faxConnect  (struct ISDN_PORT *P);
int              faxUp       (struct ISDN_PORT *P);
int              faxDelay    (struct ISDN_PORT *P,
							  byte *Response, word sizeResponse,
							  dword *RspDelay);
int              faxHangup   (struct ISDN_PORT *P);
word             faxRecv     (struct ISDN_PORT *P,
                              byte **Data, word sizeData,
                              byte *Stream, word sizeStream, word FrameType);
void             faxTxEmpty  (struct ISDN_PORT *P);
word             faxWrite    (struct ISDN_PORT *P,
                              byte **Data, word sizeData,
                              byte *Frame, word sizeFrame, word *FrameType);

/* the class1 implementations */

void             fax1Init     (struct ISDN_PORT *P);
void             fax1Finit    (struct ISDN_PORT *P);
void             fax1SetClass (struct ISDN_PORT *P);
struct T30_INFO *fax1Connect  (struct ISDN_PORT *P);
void             fax1Up       (struct ISDN_PORT *P);
int              fax1Delay    (struct ISDN_PORT *P,
							   byte *Response, word sizeResponse,
							   dword *RspDelay);
int              fax1Hangup   (struct ISDN_PORT *P);
word             fax1Write	  (struct ISDN_PORT *P,
                               byte **Data, word sizeData,
                               byte *Frame, word sizeFrame, word *FrameType);
void             fax1TxEmpty  (struct ISDN_PORT *P);
word             fax1Recv     (struct ISDN_PORT *P,
                               byte **Data, word sizeData,
                               byte *Stream, word sizeStream, word FrameType);
int              fax1Online   (struct ISDN_PORT *P);
int              fax1V8Setup  (struct ISDN_PORT *P);
int              fax1V8Menu   (struct ISDN_PORT *P);

int  fax1CmdTS    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax1CmdRS    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax1CmdTH    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax1CmdRH    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax1CmdTM    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax1CmdRM    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax1CmdAR    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax1CmdCL    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax1CmdDD    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax1Cmd34    (struct ISDN_PORT *P, int Req, int Val, byte **Str);

/* the class2 implementations */

void             fax2Init     (struct ISDN_PORT *P);
void             fax2Finit    (struct ISDN_PORT *P);
void             fax2SetClass (struct ISDN_PORT *P);
struct T30_INFO *fax2Connect  (struct ISDN_PORT *P);
void             fax2Up       (struct ISDN_PORT *P);
int              fax2Delay    (struct ISDN_PORT *P,
							   byte *Response, word sizeResponse,
							   dword *RspDelay);
int              fax2Hangup   (struct ISDN_PORT *P);
word             fax2Write    (struct ISDN_PORT *P,
                               byte **Data, word sizeData,
                               byte *Frame, word sizeFrame, word *FrameType);
void             fax2TxEmpty  (struct ISDN_PORT *P);
word             fax2Recv     (struct ISDN_PORT *P,
                               byte **Data, word sizeData,
                               byte *Stream, word sizeStream, word FrameType);

int  fax2CmdBOR   (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax2CmdLPL   (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax2CmdCR    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax2CmdDCC   (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax2CmdDIS   (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax2CmdDR    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax2CmdDT    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax2CmdET    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax2CmdK     (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax2CmdTD    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax2CmdPH    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax2CmdLID   (struct ISDN_PORT *P, int Req, int Val, byte **Str);

#endif
