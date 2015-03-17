
/*
 *
  Copyright (c) Dialogic, 2007.
 *
  This source file is supplied for the use with
  Dialogic range of DIVA Server Adapters.
 *
  Dialogic File Revision :    2.1
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

#ifndef DIVATEST_H_  
#define DIVATEST_H_

#define DIVA_MEM_TEST_MODE_QUICK         0x00000001 /* write short string        */
#define DIVA_MEM_TEST_MODE_FAST          0x00000002 /* fast memory life check    */
#define DIVA_MEM_TEST_MODE_MEDIUM        0x00000004 /* add address test to above */
#define DIVA_MEM_TEST_MODE_LARGE         0x00000008 /* add modulo test to above  */
#define DIVA_MEM_TEST_MODE_CPU           0x00000100 /* memory test using on board CPU */
#define DIVA_MEM_TEST_MODE_CPU_ENDLESS   0x00000200 /* endless test */
#define DIVA_MEM_TEST_MODE_CPU_CACHED    0x00000400 /* turn CPU cache on */
#define DIVA_MEM_TEST_MODE_DMA_0_RX_TEST 0x00001000 /* DMA channel 0 from board to host */
#define DIVA_MEM_TEST_MODE_DMA_1_RX_TEST 0x00002000 /* DMA channel 1 from board to host */
#define DIVA_MEM_TEST_MODE_DMA_0_TX_TEST 0x00004000 /* DMA channel 0 from host to board */
#define DIVA_MEM_TEST_MODE_DMA_1_TX_TEST 0x00008000 /* DMA channel 1 from host to board */

/*
  Return zero on success.
  Return error code on fail
  */
int DivaAdapterTest(PISDN_ADAPTER IoAdapter);
/*
void diva_print_seaville_registers (PISDN_ADAPTER IoAdapter);
*/

#endif
