
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

#ifndef _SER_IOCTL_H_
#define _SER_IOCTL_H_

#if !defined(word)
#define word unsigned short
#endif

#if !defined (byte)
#define byte unsigned char
#endif
#define ISDN_IOCTL_NUMDEVS 		(('I'<<8) | 0x2) 
#define ISDN_IOCTL_STATUS		(('I'<<8) | 0x3)
#define ISDN_IOCTL_PORT			(('I'<<8) | 0x4)
#define ISDN_IOCTL_ACCEPT               (('I'<<8) | 0x5)
#define ISDN_IOCTL_PROFILE		(('I'<<8) | 0x6)
typedef struct isdnNumDevs
{
	int NumDevices;
	int NumChannels;
	int Card_Type;
}isdnNumDevs;



typedef struct isdnStatusParams
{
	int DevProcess;      /*The number of the device being processed */
	int Open;
	int Locked;
	ulong Minor;
	byte Accept[48];
} isdnStatusParams;

typedef struct isdnPortStatus
{
	int DevNum;	
	int Class;
	int Direction;
	int State;
	byte Baud;
	byte Verbose;
	byte Quiet;
	byte Flow;
	byte Mode;
	byte Prot;
	byte Frame;
	byte Echo;
}isdnPortStatus;
 
typedef struct isdnAccept
{
	char Number[6];
	int AcceptDev;
}isdnAccept;

typedef struct isdnProfile
{
	int Number;
	int ProfDev;
	int Change;
	int DevSame;
	int NewProf;
	
}isdnProfile;
#endif
