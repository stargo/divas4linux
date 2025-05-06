
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

#ifndef _FAX2_H_
#define _FAX2_H_

#pragma pack (1)


/*********************************************************************(GEI)**\
*                                                                            *
*                            several definitions                             *
*                                                                            *
\****************************************************************************/

#define ETX                     0x03
#define DLE                     0x10
#define XON                     0x11
#define XOFF                    0x13

#define BYTE_BITSIZE            8
#define DWORD_BITSIZE           (sizeof (DWORD) * BYTE_BITSIZE)


/*********************************************************************(GEI)**\
*                                                                            *
*                            class 2 definitions                             *
*                                                                            *
\****************************************************************************/

#define T4_EOL                  0x800
#define T4_EOL_BITSIZE          12

#if defined(UNIXWARE)

#define T4_EOL_DWORD		0x80000000
#define T4_EOL_MASK_DWORD	0xFFF00000

#else

#define T4_EOL_DWORD            (T4_EOL << (DWORD_BITSIZE - T4_EOL_BITSIZE))
#define T4_EOL_MASK_DWORD       ((DWORD) -1 << (DWORD_BITSIZE - T4_EOL_BITSIZE))

#endif /* UNIXWARE */

#define CL2_PHASE_IDLE          0   /* local operations */
#define CL2_PHASE_A             1   /* establish the connection */
#define CL2_PHASE_B             2   /* negotiate session parameter */
#define CL2_PHASE_C             3   /* transfer one page */
#define CL2_PHASE_D             4   /* post page exchange */
#define CL2_PHASE_E             5   /* hang up the connection */

#define OBJECT_DOCU             1   /* document header */
#define OBJECT_PAGE             2   /* page header */
#define OBJECT_LINE             3   /* line data or page header */
#define OBJECT_LINE_DATA        4   /* line data */


/*********************************************************************(GEI)**\
*                                                                            *
*                              SFF definitions                               *
*                                                                            *
\****************************************************************************/

#define	SFF_LEN_FLD_SIZE		3

typedef struct
{
    DWORD                   ID;
    BYTE                    Version;
    BYTE                    Reserve;
    WORD                    UserInfo;
    WORD                    PageCount;
    WORD                    OffFirstPage;
    DWORD                   OffLastPage;
    DWORD                   OffDocuEnd;
}
SFF_DOCU_HEADER, *PSFF_DOCU_HEADER;

typedef struct
{
    BYTE                    ID;
    BYTE                    Length;
    BYTE                    ResVert;
    BYTE                    ResHorz;
    BYTE                    Coding;
        #define	SFF_DF_1D_HUFFMANN      0
    BYTE                    Reserve;
    WORD                    LineLength;
        #define	SFF_WD_A4               1728
        #define	SFF_WD_B4               2048
        #define	SFF_WD_A3               2432
        #define	SFF_WD_A5               1216
        #define	SFF_WD_A6               864
/*
	Line Length for R16 based resolutions
	*/
				#define SFF_WD_A4_R16						3456
        #define	SFF_WD_B4_R16           4096
        #define	SFF_WD_A3_R16           4864
        #define	SFF_WD_A5_R16           2432
        #define	SFF_WD_A6_R16           1728


        #define	SFF_WD_A4_200           1728
        #define	SFF_WD_B4_200           2048
        #define	SFF_WD_A3_200           2432
        #define	SFF_WD_A5_200           1216
        #define	SFF_WD_A6_200           864

    WORD                    PageLength;
        #define	SFF_LN_A4_NORMAL        1143
        #define	SFF_LN_A4_FINE          2287
        #define	SFF_LN_B4_NORMAL        1401
        #define	SFF_LN_B4_FINE          2802
    DWORD                   OffPrePage;
    DWORD                   OffNxtPage;
}
SFF_PAGE_HEADER, *PSFF_PAGE_HEADER;


/*********************************************************************(GEI)**\
*                                                                            *
*                              T30 definitions                               *
*                                                                            *
\****************************************************************************/

#if 0
    /* 
        T30PAR  = Modem/Session-Parameter, like Pagelength and resolution
        _B_     = Bytenumber in the parameter array
        _M_     = Bitmask to extract the parameters bits
        _S_     = Shiftcount to right align inside the byte
    */

    #define T30_B_GRP1_SND          0x00
    #define T30_M_GRP1_SND          0x01
    #define T30_S_GRP1_SND          0x00
                                
    #define T30_B_GRP1_RCV          0x00
    #define T30_M_GRP1_RCV          0x02
    #define T30_S_GRP1_RCV          0x01
                                
    #define T30_B_GRP1_IOC176       0x00
    #define T30_M_GRP1_IOC176       0x04
    #define T30_S_GRP1_IOC176       0x02
                                
    #define T30_B_GRP2_SND          0x00
    #define T30_M_GRP2_SND          0x08
    #define T30_S_GRP2_SND          0x03
                                
    #define T30_B_GRP2_RCV          0x00
    #define T30_M_GRP2_RCV          0x10
    #define T30_S_GRP2_RCV          0x04
                                
    #define T30_B_GRP2_XXX          0x00
    #define T30_M_GRP2_XXX          0x20
    #define T30_S_GRP2_XXX          0x05
                                
    #define T30_B_GRP2_YYY          0x00
    #define T30_M_GRP2_YYY          0x40
    #define T30_S_GRP2_YYY          0x06
                                
    #define T30_B_GRP2_ZZZ          0x00
    #define T30_M_GRP2_ZZZ          0x80
    #define T30_S_GRP2_ZZZ          0x07
                                
    #define T30_B_GRP3_SND          0x01
    #define T30_M_GRP3_SND          0x01
    #define T30_S_GRP3_SND          0x00
                                
    #define T30_B_GRP3_RCV          0x01
    #define T30_M_GRP3_RCV          0x02
    #define T30_S_GRP3_RCV          0x01

    #define T30_B_RATE              0x01
    #define T30_M_RATE              0x3c
    #define T30_S_RATE              0x02

    #   define T30_RATE_V27FB       0x00
    #   define T30_RATE_V27         0x01
    #   define T30_RATE_V29         0x02
    #   define T30_RATE_V2729       0x03
    #   define T30_RATE_V272933     0x07
    #   define T30_RATE_V27293317   0x0b

    #   define T30_RATE_V27_24      0x00
    #   define T30_RATE_V27_48      0x01
    #   define T30_RATE_V29_96      0x02
    #   define T30_RATE_V29_72      0x03
    #   define T30_RATE_V33_144     0x04
    #   define T30_RATE_V33_120     0x05
    #   define T30_RATE_V17_144     0x08
    #   define T30_RATE_V17_120     0x09
    #   define T30_RATE_V17_96      0x0a
    #   define T30_RATE_V17_72      0x0b

    #define T30_B_RES               0x01
    #define T30_M_RES               0x40
    #define T30_S_RES               0x06

    #   define T30_RES_NORMAL       0x00
    #   define T30_RES_FINE         0x01
                                
    #define T30_B_COD_2D            0x01
    #define T30_M_COD_2D            0x80
    #define T30_S_COD_2D            0x07
                                
    #define T30_B_PGWID             0x02
    #define T30_M_PGWID             0x03
    #define T30_S_PGWID             0x00

    #   define T30_PGWID_1728       0x00
    #   define T30_PGWID_2048       0x01
    #   define T30_PGWID_2432       0x02
    #   define T30_PGWID_INVALID    0x03
                                
    #define T30_B_PGLEN             0x02
    #define T30_M_PGLEN             0x0c
    #define T30_S_PGLEN             0x03

    #   define T30_PGLEN_A4         0x00
    #   define T30_PGLEN_B4         0x01
    #   define T30_PGLEN_UNLIMITED  0x02
    #   define T30_PGLEN_INVALID    0x03
                                
    #define T30_B_SCTIME            0x02
    #define T30_M_SCTIME            0x70
    #define T30_S_SCTIME            0x04

    #   define T30_SCTIME_20		0x00
    #   define T30_SCTIME_40		0x01
    #   define T30_SCTIME_10		0x02
    #   define T30_SCTIME_5			0x04
    #   define T30_SCTIME_10_5		0x03
    #   define T30_SCTIME_20_10		0x06
    #   define T30_SCTIME_40_20		0x05
    #   define T30_SCTIME_0			0x07

    #define T30_B_
    #define T30_M_
    #define T30_S_
#endif


#pragma pack ()

#endif  /* _FAX2_H_ */

/**** end ************************************************************(GEI)**/
