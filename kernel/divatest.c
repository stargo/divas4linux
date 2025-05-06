
/*
 *
  Copyright (c) Sangoma Technologies, 2018-2024
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
#include "cardtype.h"
#include "debuglib.h"
#include "divatest.h"
#include "pc.h"
#include "di_defs.h"
#include "di.h" 
#include "io.h"
#include "mi_pc.h"
#include "dsrv4bri.h"
#include "dsrv_analog.h"

#if defined(DIVA_ADAPTER_TEST_SUPPORTED) /* { */

#define DMA_OFFSET 0x00000000

/*
  Data access
  */
#define BYTE_ACCESS       0
#define MULTIBYTE_ACCESS  1

/*
	LOCALS
	*/
static int dma_memory_test (PISDN_ADAPTER IoAdapter);

/*
------------------------------------------------------------------
 Quick memory check. Just writes a string and reads it back
 Prints detailed info and returns 1 on error
------------------------------------------------------------------
*/
static int
memtest_quick(void *address) 
{
  #define QUICK_MEMORY_TEST_PATTERN "Quick-Mode Memory Test"
  
  memcpy(address, QUICK_MEMORY_TEST_PATTERN, sizeof(QUICK_MEMORY_TEST_PATTERN));
  if (memcmp(address, QUICK_MEMORY_TEST_PATTERN, 
             sizeof(QUICK_MEMORY_TEST_PATTERN)  ))
  {
    DBG_FTL((QUICK_MEMORY_TEST_PATTERN" failed!"))
	  DBG_BLK((address, sizeof(QUICK_MEMORY_TEST_PATTERN)))
    return 1;
  }
  return 0;
}

/*
------------------------------------------------------------------
 Static memory test. Write memory with pattern and compares afterwards.
 Prints detailed info and returns 1 on error
------------------------------------------------------------------
*/
static int
memtest_static (void *address, 
               dword length, 
               dword pattern, 
               int accessmode)
{
  dword pos;

  switch (accessmode)
  {
    case BYTE_ACCESS:
      {
        volatile byte *p;
				byte v;

        memset(address, (byte)pattern, length);
        for (pos=0, p=address; pos<length; pos++, p++)
        {
          if ((v = *p) != (byte)pattern)
          {
            DBG_FTL(("Memory byte pattern at %08x = %02x != %02x", pos, v, (byte)pattern))
            return 1;
          }
        }
      }
      return 0;
      

    case MULTIBYTE_ACCESS:
      {
        volatile dword *p;
				dword v;

        for (pos=0, p=address; pos<(length/sizeof(dword)); pos++, p++)
          *p = (dword)pattern;
        for (pos=0, p=address; pos<(length/sizeof(dword)); pos++, p++)
        {
          if ((v = *p) != (dword)pattern)
          {
            DBG_FTL(("Memory dword pattern at %08x = %08x != %08x", pos, v, (dword)pattern))
            return 1;
          }
        }
      }
      return 0;
  }    
  return 0;
}

/*
------------------------------------------------------------------
 Walking data bit test with one memory location
 Prints detailed info and returns 1 on error
------------------------------------------------------------------
*/
static int
memtest_datwalk(void *address, int accessmode)
{
  switch (accessmode)
  {
    case BYTE_ACCESS:
      {
        byte data;
        byte mem;
        for (data=1; data; data <<= 1)
        {
          *((volatile byte*)address) = data;
          mem = *((volatile byte*)address);
          if (mem != data)
          {
            DBG_FTL(("Walking Byte Data bit test failed data<->mem: 0x%02x<->0x%02x !",
                     data, mem))
            return 1;
          }
        }
      }
      return 0;
      
    case MULTIBYTE_ACCESS:
      {
        dword data;
        dword mem;
        for (data=1; data; data <<= 1)
        {
          *((volatile dword*)address) = data;
          mem = *((volatile dword*)address);
          if (mem != data)
          {
            DBG_FTL(("Walking Multibyte Data bit test failed data<->mem: 0x%08x<->0x%08x !",
                     data, mem))
            return 1;
          }
        }
      }
      return 0;
  }
  return 1;
}

/*
------------------------------------------------------------------
 Walking address bit test
 Prints detailed info and returns 1 on error
------------------------------------------------------------------
*/
static int
memtest_addrwalk(void *address, 
                 dword length,
                 dword pattern,
                 int accessmode)
{
  switch (accessmode)
  {
    case BYTE_ACCESS:
      {
        volatile byte *cptr;
        byte cpat;
        dword tloc, idx;
        
        cpat = (byte)pattern;
        memset(address, cpat, length);  /* set starting condition */
        cptr = address;
        for (tloc=1; tloc < length; tloc <<= 1)
        {
          cptr[tloc] = ~cpat; /* set test location with inverse pattern */
          DBG_LOG(("Walking Address Bit Test, setting bit at 0x%08x", tloc))
          for (idx=0; idx<length; idx++)  /* check other locations for consistency */
          {
            if (idx == tloc) continue;
            if (cptr[idx] != pattern)
            {
              DBG_FTL(("Walk Addr Bit Test failed at 0x%08x. SetAddr=0x%08x!",
                       idx, tloc))
              DBG_BLK(((void *)(&cptr[idx &= 0xfffffff0]), 0x10))
              return 1;
            }
          }
          cptr[tloc] = cpat;  /* recover test location to original value */
        }
      }
      return 0;
      
    case MULTIBYTE_ACCESS:
      {
        volatile dword *ptr;
        dword tloc, idx;
        
        memset(address, (byte)pattern, length);  /* set starting condition */
        ptr = address;
        for (tloc=1; tloc < (length/sizeof(dword)); tloc <<= 1)
        {
          ptr[tloc] = ~pattern; /* set test location with inverse pattern */
          DBG_LOG(("Walking Address Bit Test, setting location 0x%08x", 
                    tloc * sizeof(dword)))
          for (idx=0; idx<(length/sizeof(dword)); idx++)  /* check other locations for consistency */
          {
            if (idx == tloc) continue;
            if (ptr[idx] != pattern)
            {
              DBG_FTL(("Walk Addr Bit Test failed at 0x%08x. SetAddr=0x%08x!",
                       idx, tloc))
              DBG_BLK(((void *)(&ptr[idx &0xfffffffC]), 0x10))
              return 1;
            }
          }
          ptr[tloc] = pattern;  /* recover test location to original value */
        }
      }
      return 0;
  }
  return 1;
}

/*
------------------------------------------------------------------
 Modulo Test
 Prints detailed info and returns 1 on error
------------------------------------------------------------------
*/
static int
memtest_modulo(void *address, 
               dword length,
               dword pattern)
{
  volatile dword *ptr = address;
  dword idx, mod;

  length /= sizeof(dword);
  for (mod=0; mod < 20; mod++)
  {
    DBG_LOG(("Modulo Test, setting mod=%d", mod)) 
    /* write every 20th location with pattern */
    for (idx=mod; idx < length; idx +=20)
    {
      ptr[idx] = pattern;
    }
    
    /* write all other locations with inverse patterns 2 times */
    for (idx=0; idx < length; idx ++)
    {
      if (mod == (idx%20)) continue;
      ptr[idx] = ~pattern;
    }
    for (idx=0; idx < length; idx ++)
    {
      if (mod == (idx%20)) continue;
      ptr[idx] = ~pattern;
    }

    /* now check every 20th location for written pattern */
    for (idx=mod; idx < length; idx +=20)
    {
			dword tmp;
      if ((tmp = ptr[idx]) != pattern)
      {
				DBG_FTL(("mem[%d]=%08x != %08x", idx, tmp, pattern))
        DBG_FTL(("Modulo Test failed at 0x%08x (mod=%d)!", idx, mod))
        DBG_BLK(((void *)(&ptr[idx &0xfffffffc]), 0x10))
        return 1;
      }
    }
  }
  return 0;
}

/*
------------------------------------------------------------------
 entry point for this module
 return mode mask of failed test on error and print trace message
 return 0 if ok
------------------------------------------------------------------
*/
static int DivaAdapterMemtest (PISDN_ADAPTER IoAdapter,
                               void *address, 
                               dword length, 
                               dword* mode)
{

  if (*mode & DIVA_MEM_TEST_MODE_CPU)
  {
    volatile byte* mem = (byte*)address;
    dword live_counter = 0;
    int i;

    *mode &= ~DIVA_MEM_TEST_MODE_CPU;

    if (*mode == 0) {
      DBG_ERR(("Invalid command"))
      return (1);
    }

    if (*(volatile dword*)mem != 0)
    {
      *(volatile dword*)&mem[0x100] = (dword)*mode; /* command */
      *(volatile dword*)&mem[0x104] = (dword)length; /* memory length */

      switch (IoAdapter->cardType) {
        case CARDTYPE_DIVASRV_P_8M_V30_PCI:
        case CARDTYPE_DIVASRV_P_24M_V30_PCI:
        case CARDTYPE_DIVASRV_P_30M_V30_PCI:
        case CARDTYPE_DIVASRV_P_2V_V30_PCI:
        case CARDTYPE_DIVASRV_P_24V_V30_PCI:
        case CARDTYPE_DIVASRV_P_30V_V30_PCI:
        case CARDTYPE_DIVASRV_P_24UM_V30_PCI:
        case CARDTYPE_DIVASRV_P_30UM_V30_PCI:
          *(volatile dword*)&mem[0x104] = 64*1024*1024;
          *(volatile dword*)&mem[0x108] = 1; /* Card type */
          start_pri_v3_hardware (IoAdapter);
          break;

        case CARDTYPE_DIVASRV_2P_V_V10_PCI:
        case CARDTYPE_DIVASRV_4P_V_V10_PCI:
        case CARDTYPE_DIVASRV_2P_M_V10_PCI:
        case CARDTYPE_DIVASRV_4P_M_V10_PCI:
        case CARDTYPE_DIVASRV_V4P_V10H_PCIE:
        case CARDTYPE_DIVASRV_V2P_V10H_PCIE:
        case CARDTYPE_DIVASRV_V1P_V10H_PCIE:
        case CARDTYPE_DIVASRV_IPMEDIA_300_V10:
        case CARDTYPE_DIVASRV_IPMEDIA_600_V10:
        case CARDTYPE_DIVASRV_V4P_V10H_PCIE_HYPERCOM:
        case CARDTYPE_DIVASRV_V2P_V10H_PCIE_HYPERCOM:
        case CARDTYPE_DIVASRV_V1P_V10H_PCIE_HYPERCOM:
          *(volatile dword*)&mem[0x104] = 64*1024*1024;
          *(volatile dword*)&mem[0x108] = 7; /* Card type */
          start_4pri_hardware (IoAdapter);
          break;

        case CARDTYPE_DIVASRV_V4P_V10Z_PCIE:
        case CARDTYPE_DIVASRV_V8P_V10Z_PCIE:
        case CARDTYPE_DIVASRV_V4P_V10Z_PCIE_HYPERCOM:
        case CARDTYPE_DIVASRV_V8P_V10Z_PCIE_HYPERCOM:
        case CARDTYPE_DIVASRV_M4P_V10Z_PCIE:
        case CARDTYPE_DIVASRV_M8P_V10Z_PCIE:
          *(volatile dword*)&mem[0x104] = 128*1024*1024;
          *(volatile dword*)&mem[0x108] = 7; /* Card type */
          start_4prie_hardware (IoAdapter);
          break;

        case CARDTYPE_DIVASRV_Q_8M_V2_PCI:
        case CARDTYPE_DIVASRV_VOICE_Q_8M_V2_PCI:
        case CARDTYPE_DIVASRV_V_Q_8M_V2_PCI:
          *(volatile dword*)&mem[0x108] = 2;
          start_qBri_hardware (IoAdapter);
          break;

        case CARDTYPE_DIVASRV_4BRI_V1_PCIE:
        case CARDTYPE_DIVASRV_4BRI_V1_V_PCIE:
        case CARDTYPE_DIVASRV_BRI_V1_PCIE:
        case CARDTYPE_DIVASRV_BRI_V1_V_PCIE:
          *(volatile dword*)&mem[0x104] = 64*1024*1024;
          *(volatile dword*)&mem[0x108] = 2;
          start_qBri_hardware (IoAdapter);
          break;

        case CARDTYPE_DIVASRV_B_2M_V2_PCI:
        case CARDTYPE_DIVASRV_VOICE_B_2M_V2_PCI:
        case CARDTYPE_DIVASRV_V_B_2M_V2_PCI:
          *(volatile dword*)&mem[0x108] = 3;
          start_qBri_hardware (IoAdapter);
          break;

        case CARDTYPE_DIVASRV_B_2F_PCI:
        case CARDTYPE_DIVASRV_BRI_CTI_V2_PCI:
          *(volatile dword*)&mem[0x108] = 4;
          start_qBri_hardware (IoAdapter);
          break;

        case CARDTYPE_DIVASRV_ANALOG_8PORT:
        case CARDTYPE_DIVASRV_V_ANALOG_8PORT:
        case CARDTYPE_DIVASRV_ANALOG_4PORT:
        case CARDTYPE_DIVASRV_V_ANALOG_4PORT:
        case CARDTYPE_DIVASRV_ANALOG_2PORT:
        case CARDTYPE_DIVASRV_V_ANALOG_2PORT:
          *(volatile dword*)&mem[0x108] = 5;
          start_analog_hardware (IoAdapter);
          break;

        case CARDTYPE_DIVASRV_ANALOG_8P_PCIE:
        case CARDTYPE_DIVASRV_V_ANALOG_8P_PCIE:
        case CARDTYPE_DIVASRV_ANALOG_4P_PCIE:
        case CARDTYPE_DIVASRV_V_ANALOG_4P_PCIE:
        case CARDTYPE_DIVASRV_ANALOG_2P_PCIE:
        case CARDTYPE_DIVASRV_V_ANALOG_2P_PCIE:
        case CARDTYPE_DIVASRV_V_ANALOG_2P_PCIE_HYPERCOM:
        case CARDTYPE_DIVASRV_V_ANALOG_4P_PCIE_HYPERCOM:
        case CARDTYPE_DIVASRV_V_ANALOG_8P_PCIE_HYPERCOM:
          *(volatile dword*)&mem[0x104] = DIVA_ANALOG_PCIE_REAL_SDRAM_SIZE;
          *(volatile dword*)&mem[0x108] = 5;
          start_analog_hardware (IoAdapter);
          break;

        case CARDTYPE_DIVASRV_Q_8M_PCI:
          *(volatile dword*)&mem[0x104] = 4*1024*1024;
          *(volatile dword*)&mem[0x108] = 2;
          start_qBri_hardware (IoAdapter);
          break;

        default:
          DBG_ERR(("Unsuported adapter type: %d", IoAdapter->cardType))
          return (*mode);
      }
      i = 100;
      while ((*(volatile dword*)&mem[0x110] == 0) && i--);
      if (*(volatile dword*)&mem[0x110] != 1) {
        DBG_ERR(("Test failed, boot counter=%08x", *(volatile dword*)&mem[0x110]))
        IoAdapter->stop(IoAdapter);
        return (*mode);
      }
      live_counter = *(volatile dword*)&mem[0x10c];
			i = 0;
      do {
        diva_os_sleep(200);
        if (live_counter == *(volatile dword*)&mem[0x10c]) {
          if (++i > 500) {
            DBG_ERR(("Memory test %08x terminated, live counter=%08x",
                    *(volatile dword*)&mem[0x118], live_counter))
            IoAdapter->stop(IoAdapter);
            return (*mode);
          }
        } else {
          i = 0;
        }
        live_counter = *(volatile dword*)&mem[0x10c];
        if (*(volatile dword*)&mem[0x110] != 1)
        {
          DBG_ERR(("Memory test restarted (%08x)", *(volatile dword*)&mem[0x110]))
          IoAdapter->stop(IoAdapter);
          return (*mode);
        }
      } while (*(volatile dword*)&mem[0x114] == 0);
      if (*(volatile dword*)&mem[0x114] != 1) {
        DBG_ERR(("Memory test %08x failed (%d) at %08x read %08x instead of %08x",
                  *(volatile dword*)&mem[0x118],
                  *(volatile dword*)&mem[0x114],
                  *(volatile dword*)&mem[0x11c],
                  *(volatile dword*)&mem[0x120],
                  *(volatile dword*)&mem[0x124]))
        IoAdapter->stop(IoAdapter);
        return (*mode);
      }
    }
    else
    {
      DBG_ERR(("Firmware missing"))
      return (*mode);
    }

    DBG_LOG(("CPU test %08x at %08x length %08x passed",
            *mode,
            *(volatile dword*)&mem[0x11c],
            *(volatile dword*)&mem[0x104]))

    if ((*mode & DIVA_MEM_TEST_MODE_CPU_ENDLESS) == 0) {
      IoAdapter->stop(IoAdapter);
    }
  }
  if ((*mode & DIVA_MEM_TEST_MODE_CPU_ENDLESS) != 0) {
    /*
      Endless test active
      */
    return (0);
  }
  if ((*mode & DIVA_MEM_TEST_MODE_CPU_CACHED) != 0) {
    return (0);
  }

  /*--- Write a short fixed string and read it back (default test) */
  if (*mode & DIVA_MEM_TEST_MODE_QUICK)
  {
    if (memtest_quick(address)) return  DIVA_MEM_TEST_MODE_QUICK;
    DBG_LOG(("Quick Memory test passed"))
  }

  /*--- more intensive but fast test */
  if (*mode & DIVA_MEM_TEST_MODE_FAST)
  {
    if (memtest_datwalk(address, BYTE_ACCESS))                              return  DIVA_MEM_TEST_MODE_FAST;
    if (memtest_datwalk(&((dword *)address)[1], MULTIBYTE_ACCESS))  return  DIVA_MEM_TEST_MODE_FAST;
    if (memtest_static(address, length, 0x00, BYTE_ACCESS))                 return  DIVA_MEM_TEST_MODE_FAST;
    if (memtest_static(address, length, 0xffffffff, MULTIBYTE_ACCESS))      return  DIVA_MEM_TEST_MODE_FAST;
		DBG_LOG(("Fast Memorytest passed"))
  }
  
  /*--- add walking bit address test */
  if (*mode & DIVA_MEM_TEST_MODE_MEDIUM)
  {
    if (memtest_addrwalk(address, 0x10, 0xa5, BYTE_ACCESS))               return  DIVA_MEM_TEST_MODE_MEDIUM;
    if (memtest_addrwalk(address, length, 0x5a5a5a5a, MULTIBYTE_ACCESS))  return  DIVA_MEM_TEST_MODE_MEDIUM;
		DBG_LOG(("Medium Memorytest passed"))
  }
  
  if (*mode & DIVA_MEM_TEST_MODE_LARGE)
  {
    if (memtest_modulo(address, length, 0xaaaaaaaa)) return  DIVA_MEM_TEST_MODE_LARGE;
		DBG_LOG(("Large Memorytest passed"))
  }

  return 0;
}

static int DivaAdapterDmaMemtest (PISDN_ADAPTER IoAdapter) {
	int ret = 0;

	if ((IoAdapter->AdapterTestMask & (DIVA_MEM_TEST_MODE_DMA_0_RX_TEST |
																		 DIVA_MEM_TEST_MODE_DMA_1_RX_TEST |
																		 DIVA_MEM_TEST_MODE_DMA_0_TX_TEST |
																		 DIVA_MEM_TEST_MODE_DMA_1_TX_TEST)) != 0) {
		ret = dma_memory_test (IoAdapter);
		IoAdapter->AdapterTestMask &= ~(DIVA_MEM_TEST_MODE_DMA_0_RX_TEST |
																		DIVA_MEM_TEST_MODE_DMA_1_RX_TEST |
																		DIVA_MEM_TEST_MODE_DMA_0_TX_TEST |
																		DIVA_MEM_TEST_MODE_DMA_1_TX_TEST);
		IoAdapter->AdapterTestMask |=   DIVA_MEM_TEST_MODE_QUICK;
	}

	return (ret);
}

int DivaAdapterTest(PISDN_ADAPTER IoAdapter) {
  int ret = 0;

  if (IoAdapter->AdapterTestMask) {
    if (IoAdapter->AdapterTestMemoryStart && IoAdapter->AdapterTestMemoryLength) {
      /*
         In case OS abstraction layer provides information about memory
         that can be tested then use this information
         */
      DBG_LOG(("Diva memory test: %s (Type %d, Serial: %d), command:%08x, addr=%08lx, length=%ld",
               IoAdapter->Properties.Name, IoAdapter->cardType, IoAdapter->serialNo,
               IoAdapter->AdapterTestMask,
               IoAdapter->AdapterTestMemoryStart,
               IoAdapter->AdapterTestMemoryLength))
			ret = DivaAdapterDmaMemtest (IoAdapter);
      if ( ret == 0 && (ret = DivaAdapterMemtest(IoAdapter,
                                     IoAdapter->AdapterTestMemoryStart,
                                     IoAdapter->AdapterTestMemoryLength,
                                     &IoAdapter->AdapterTestMask)) == 0) {
        DBG_FTL(("Diva adapter memory pass"))
      } else {
        DBG_FTL(("Diva adapter memory test failed (%d)", ret))
      }
    } else {
      switch (IoAdapter->cardType) {
        case CARDTYPE_DIVASRV_P_30M_V30_PCI:
        case CARDTYPE_DIVASRV_P_24M_V30_PCI:
        case CARDTYPE_DIVASRV_P_8M_V30_PCI:
        case CARDTYPE_DIVASRV_P_30V_V30_PCI:
        case CARDTYPE_DIVASRV_P_24V_V30_PCI:
        case CARDTYPE_DIVASRV_P_2V_V30_PCI:
        case CARDTYPE_DIVASRV_P_30UM_V30_PCI:
        case CARDTYPE_DIVASRV_P_24UM_V30_PCI:
          DBG_LOG(("Diva memory test: %s (Type %d, Serial: %d), command:%08x, addr=%08lx, length=%ld",
                   IoAdapter->Properties.Name, IoAdapter->cardType, IoAdapter->serialNo,
                   IoAdapter->AdapterTestMask,
                   IoAdapter->ram,
                   IoAdapter->MemorySize - MP_SHARED_RAM_OFFSET - 1))
          if (!(ret = DivaAdapterMemtest(IoAdapter,
                                         IoAdapter->ram,
                                         IoAdapter->MemorySize - MP_SHARED_RAM_OFFSET - 1,
                                         &IoAdapter->AdapterTestMask))) {
            DBG_FTL(("Diva adapter memory pass"))
          } else {
            DBG_FTL(("Diva adapter memory test failed (%d)", ret))
          }
          break;

        default:
          DBG_FTL(("Adapter type '%d' not supported by Diva memory test")) 
          ret = -1;
          break;
      }
    }
  } else {
    DBG_LOG(("Diva memory test: idle"))
  }

  return (ret);
}

static void create_descriptor_list (volatile byte* mem,
																		dword pci_addr,
																		dword pci_addr_hi,
																		dword offset,
																		int rx) {
	volatile byte *data     = (byte*)&mem[1024*1024+offset];
	dword addr = 0x100 + offset;
	dword command = (rx != 0) ? (1U << 3) : 0;
	int i;

	for (i = 0; i < 4*1024; i++) {
		data[i] = (byte)i;
	}

	for (i = 0; i < 1024; i++) {
		*(volatile dword*)&mem[addr] = pci_addr;  addr += 4; /* PCIe address */
		*(volatile dword*)&mem[addr] = DMA_OFFSET | (1024*1024+4+offset); addr += 4; /* sdram address */
		*(volatile dword*)&mem[addr] = 4*1024;    addr += 4; /* Transfer length */
		*(volatile dword*)&mem[addr] = DMA_OFFSET | (addr + 4+16) | command; addr += 4; /* sdram to pci */
		*(volatile dword*)&mem[addr] = pci_addr_hi; addr += 4;
		addr += 12; /* Align to 32 byte boundary */
	}

	*(volatile dword*)&mem[addr] = pci_addr;  addr += 4; /* PCIe address */
	*(volatile dword*)&mem[addr] = DMA_OFFSET | (1024*1024+offset); addr += 4; /* sdram address */
	*(volatile dword*)&mem[addr] = 4*1024;    addr += 4; /* Transfer length */
	*(volatile dword*)&mem[addr] = command | (1L << 1); addr += 4; /* sdram to pci */
	*(volatile dword*)&mem[addr] = pci_addr_hi; addr += 4;
}

/*
	plx         - PLX device base
	mem         - SDRAM base, supposed 8Mbyte long
  dma_map     - dma map
  dma_channel - dmal channel 0 or 1
	rx          - 0 if transfer from host to board 1 if transfer from board to host
	*/
static int start_dma_memory_test (byte* plx,
																	byte* mem,
                                  struct _diva_dma_map_entry* dma_map,
                                  int dma_channel,
																	int rx,
																	int burst) {
  volatile dword *dmamode0 = (dword*)&plx[(dma_channel == 0) ? 0x80 : 0x94];
  volatile dword *dmadpr0  = (dword*)&plx[(dma_channel == 0) ? 0x90 : 0xa4];
  volatile byte  *dmacsr0  = (byte*)&plx[(dma_channel == 0)  ? 0xa8 : 0xa9];
  int entry_nr;

  *dmacsr0  &= ~1U; /* Stop channel */
	if (burst != 0) {
		*dmamode0 |= (1U <<  8); /* Local burst enable */
	} else {
		*dmamode0 &= ~(1U <<  8); /* Local burst enable */
	}
  /* *dmamode0 |= (1U <<  6); */
  *dmamode0 |= (1U <<  9); /* Scatter gatter mode */
	if (burst != 0) {
		*dmamode0 |= (1U <<  7); /* Contineous burst mode */
	} else {
		*dmamode0 &= ~(1U <<  7); /* Contineous burst mode */
	}

  if ((entry_nr = diva_alloc_dma_map_entry ((struct _diva_dma_map_entry*)dma_map)) >= 0) {
    unsigned long dma_magic;
    unsigned long dma_magic_hi;
    void* local_addr;
		int i;

    diva_get_dma_map_entry (dma_map, entry_nr, &local_addr, &dma_magic);

		for (i = 0; i < 4*1024;i++) {
			byte* addr = local_addr;
			addr[i] = (byte)(0xfU + i);
		}

		diva_get_dma_map_entry_hi(dma_map, entry_nr, &dma_magic_hi);
		*dmamode0 |= (1U << 18); /* Long descriptor format */
    create_descriptor_list (mem, dma_magic, dma_magic_hi, (dma_channel == 0) ? 0 : 4*1024*1024, rx);
    *dmacsr0 |= 0x01; /* Start channel */
    *dmadpr0 = (DMA_OFFSET | 0x100) + ((dma_channel == 0) ? 0 : 4*1024*1024); /* Pointer to descriptor list */

		return (entry_nr);
  }

	return (-1);
}

typedef struct _diva_plx_registers {
	const char* name;
	byte        offset;
} diva_plx_registers_t;

static void diva_print_plx_registers (volatile byte* plx) {
	static diva_plx_registers_t plx_registers[] = {
		{ "MARBR ", 0x08 },
		{ "LMISC ", 0x0d },
		{ "LMISC2", 0x0f },
		{ "LBRD0 ", 0x18 },
		{ "LBRD1 ", 0xf8 },
		{ "DMPBAM", 0x28 }
	};
	int i;

	for (i = 0; i < sizeof(plx_registers)/sizeof(plx_registers[0]); i++) {
		switch ((plx_registers[i].offset & 0x03)) {
			case 1:
			case 3:
				DBG_LOG(("%s %02x %02x",
								plx_registers[i].name, plx_registers[i].offset,
								plx[plx_registers[i].offset]))
				break;

			case 2:
				DBG_LOG(("%s %02x %04x",
								plx_registers[i].name, plx_registers[i].offset,
								*(volatile word*)&plx[plx_registers[i].offset]))
				break;

			default:
				DBG_LOG(("%s %02x %08x",
								plx_registers[i].name, plx_registers[i].offset,
								*(volatile dword*)&plx[plx_registers[i].offset]))
				break;
		}
	}

}

static int dma_memory_test (PISDN_ADAPTER IoAdapter) {
	byte *plx = &(((byte*)IoAdapter->reset)[IoAdapter->plx_offset]);
	volatile byte  *dmacsr0  = (byte*)&plx[0xa8];
	volatile byte  *dmacsr1  = (byte*)&plx[0xa9];
	int dma_channel_0 = (IoAdapter->AdapterTestMask &
													(DIVA_MEM_TEST_MODE_DMA_0_RX_TEST | DIVA_MEM_TEST_MODE_DMA_0_TX_TEST)) != 0;
	int dma_channel_1 = (IoAdapter->AdapterTestMask &
													(DIVA_MEM_TEST_MODE_DMA_1_RX_TEST | DIVA_MEM_TEST_MODE_DMA_1_TX_TEST)) != 0;
	int dma_channel_0_rx = (IoAdapter->AdapterTestMask & DIVA_MEM_TEST_MODE_DMA_0_RX_TEST) != 0;
	int dma_channel_1_rx = (IoAdapter->AdapterTestMask & DIVA_MEM_TEST_MODE_DMA_1_RX_TEST) != 0;
	int entry_nr_0 = -1, entry_nr_1 = -1, i = 0;
	dword t[4];

	if (plx != 0 && IoAdapter->dma_map != 0 &&
			IoAdapter->AdapterTestMemoryStart && IoAdapter->AdapterTestMemoryLength) {
		int burst = DIVA_4BRI_REVISION(IoAdapter) == 0;

		diva_print_plx_registers (plx);

		if (dma_channel_0 != 0) {
			entry_nr_0 = start_dma_memory_test (plx,
																					IoAdapter->AdapterTestMemoryStart,
																					IoAdapter->dma_map, 0, dma_channel_0_rx,
																					burst);
		}
		if (dma_channel_1 != 0) {
			entry_nr_1 = start_dma_memory_test (plx,
																					IoAdapter->AdapterTestMemoryStart,
																					IoAdapter->dma_map, 1, dma_channel_1_rx,
																					burst);
		}

		if (entry_nr_0 >= 0) {
			DBG_LOG(("Start channel 0 %s", dma_channel_0_rx != 0 ? "from board to host" : "from host to board"))
		}
		if (entry_nr_1 >= 0) {
			DBG_LOG(("Start channel 1 %s", dma_channel_1_rx != 0 ? "from board to host" : "from host to board"))
		}

    diva_os_get_time (&t[0], &t[1]);

		if (entry_nr_0 >= 0) {
			*dmacsr0 |= 0x03; /* Start transfer */
		}
		if (entry_nr_1 >= 0) {
			*dmacsr1 |= 0x03; /* Start transfer */
		}

		while ((*dmacsr0 & (1 << 4)) == 0 || (*dmacsr1 & (1 << 4)) == 0) {
			i++;
		}

    diva_os_get_time (&t[2], &t[3]);

		if (t[1] > t[3]) {
			t[2]--;
			t[3] += 1000000;
		}
		t[2] -= t[0];
		t[3] -= t[1];

		DBG_LOG(("OK (%d %d Sec %d uSec)", i, t[2], t[3]))

		*dmacsr0 &= ~0x01; /* Stop dma */
		*dmacsr1 &= ~0x01; /* Stop dma */

		if (entry_nr_0 >= 0) {
			unsigned long dma_magic;
			volatile byte* data = IoAdapter->AdapterTestMemoryStart;
			void* local_addr;
			int i, error_detected;

			data += 1024*1024;

			diva_get_dma_map_entry (IoAdapter->dma_map, entry_nr_0, &local_addr, &dma_magic);

			for (i = 0, error_detected = 0; i < 4*1024; i++) {
				byte* addr = local_addr;

				if (addr[i] != data[i]) {
					error_detected = 1;
					DBG_ERR(("data pattern test error addr[%d]=%02x data[%d]=%02x", i, addr[i], i, data[i]))
				}
			}

			diva_free_dma_map_entry (IoAdapter->dma_map, entry_nr_0);

			if (error_detected == 0) {
				DBG_LOG(("data pattern test OK"))
			}
		}

		if (entry_nr_1 >= 0) {
			diva_free_dma_map_entry (IoAdapter->dma_map, entry_nr_1);
		}

		if ((dma_channel_0 != 0 && entry_nr_0 < 0) || (dma_channel_1 != 0 && entry_nr_1 < 0)) {
			return (-1);
		}

		return (0);
	}

	return (-1);
}

#if 0
void diva_print_seaville_registers (PISDN_ADAPTER IoAdapter) {
	static struct {
		word offset;
		const char* name;
		byte width;
	} descr[] = {
		{ 0x00, "LAS0RR", 4 },
		{ 0x08, "MARBR", 4 },
		{ 0x0d, "LMISC", 1 },
		{ 0x18, "LBRD0", 4 },
		{ 0x28, "DMPBAM", 4 },
		{ 0x120, "MSINUM", 4 },
		{ 0xf0, "LAS1RR", 4 },
		{ 0xf4, "LAS1BA", 4 },
		{ 0xf8, "LBRD1", 4 },
		{ 0xec, "QBRD", 4, },
		{ 0x110, "DMQUANT", 4 },
		{ 0x38, "TARTHR", 4, },
		{ 0x150, "PBDATA0", 4 },
		{ 0x154, "PBDATA1", 4 },
		{ 0x158, "PBDATA2", 4 },
		{ 0x15C, "PBDATA3", 4 },
		{ 0x130, "GASKTAR", 4 },
		{ 0x134, "GASKMAS", 4 },
		{ 0x160, "GBCTRL", 4 }
	};
	int i;

	if ((IDI_PROP_SAFE(IoAdapter->cardType,HardwareFeatures) & DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE) == 0)
		return;

	DBG_LOG(("Seavill register dump: %s [", IoAdapter->Properties.Name))
	for (i = 0; i < sizeof(descr)/sizeof(descr[0]); i++) {
		switch (descr[i].width) {
			case 1:
				DBG_LOG((" %s[%04x]=%02x",
								descr[i].name, descr[i].offset, *(volatile byte*)&IoAdapter->reset[descr[i].offset]))
				break;

			case 2:
				DBG_LOG((" %s[%04x]=%04x",
								descr[i].name, descr[i].offset, *(volatile word*)&IoAdapter->reset[descr[i].offset]))
				break;

			default:
				DBG_LOG((" %s[%04x]=%08x",
								descr[i].name, descr[i].offset, *(volatile dword*)&IoAdapter->reset[descr[i].offset]))
				break;
		}
	}
	DBG_LOG(("]"))
}
#endif

#else /* } { */

int DivaAdapterTest(PISDN_ADAPTER IoAdapter) {
  DBG_LOG(("Diva memory test: not supported"))
  return (0);
}

#endif /* } */

// vim: set tabstop=2 softtabstop=2 shiftwidth=2 expandtab :
