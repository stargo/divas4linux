
/*
 *
  Copyright (c) Dialogic, 2008.
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
#include "platform.h"
#include "fpga_rom.h"


typedef enum _diva_fpga_rom_features {
	DivaFpgaRomFeatureTelecommunicationInterface = 0x04U,
	DivaFpgaRomFeatureHostInterface              = 0x08U,
	DivaFpgaRomFeatureLocalInterfaceFrequency    = 0x0cU,
	DivaFpgaRomFeatureVersionNumber              = 0x10U,
	DivaFpgaRomFeatureTestVersionNumber          = 0x14U,
	DivaFpgaRomFeatureBetaVersionFlag            = 0x18U,
	DivaFpgaRomFeatureDate                       = 0x1cU,
	DivaFpgaRomFeatureFeature_01                 = 0x20U,
} diva_fpga_rom_features_t;

typedef struct _diva_fpga_feature_description {
	diva_fpga_rom_features_t feature;
	const char* name;
	void (*decode_proc)(dword feature, dword* hardware_features, dword* fpga_features, word (*dprintf_proc)(char *, ...));
	dword bits;
} diva_fpga_feature_description_t;


static void telecom_interface_decode_proc (dword feature, dword* hardware_features, dword* fpga_features, word (*dprintf_proc)(char *, ...));
static void host_interface_decode_proc (dword feature, dword* hardware_features, dword* fpga_features, word (*dprintf_proc)(char *, ...));
static void local_interface_frequency_decode_proc (dword feature, dword* hardware_features, dword* fpga_features, word (*dprintf_proc)(char *, ...));
static void version_number_decode_proc (dword feature, dword* hardware_features, dword* fpga_features, word (*dprintf_proc)(char *, ...));
static void test_version_number_decode_proc (dword feature, dword* hardware_features, dword* fpga_features, word (*dprintf_proc)(char *, ...));
static void beta_version_flag_decode_proc (dword feature, dword* hardware_features, dword* fpga_features, word (*dprintf_proc)(char *, ...));
static void date_decode_proc (dword feature, dword* hardware_features, dword* fpga_features, word (*dprintf_proc)(char *, ...));
static void supported_fpga_features_decode_proc (dword feature, dword* hardware_features, dword* fpga_features, word (*dprintf_proc)(char *, ...));

static OSCONST diva_fpga_feature_description_t info[] = {
	{ DivaFpgaRomFeatureTelecommunicationInterface,
		"Telecommunication interface",
		telecom_interface_decode_proc,
		0xffffffff
	},
	{ DivaFpgaRomFeatureHostInterface,
		"Host interface",
		host_interface_decode_proc,
		0xffff,
	},
	{ DivaFpgaRomFeatureLocalInterfaceFrequency,
		"Local interface frequency",
		local_interface_frequency_decode_proc,
		0xffff,
	},
	{ DivaFpgaRomFeatureVersionNumber,
		"Major version",
		version_number_decode_proc,
		0xffffffff,
	},
	{ DivaFpgaRomFeatureTestVersionNumber,
		"Minor version",
		test_version_number_decode_proc,
		0xffffffff
	},
	{ DivaFpgaRomFeatureBetaVersionFlag,
		"Release flag",
		beta_version_flag_decode_proc,
		0x00000001,
	},
	{ DivaFpgaRomFeatureDate,
		"Date D/M/Y",
		date_decode_proc,
		0xffffffff,
	},
	{ DivaFpgaRomFeatureFeature_01,
		"Supported FPGA features",
		supported_fpga_features_decode_proc,
		0xffffffff
	}
};

static void telecom_interface_decode_proc (dword feature, dword* hardware_features, dword* fpga_features,
                                           word (*dprintf_proc)(char *, ...)) {
	const char* interface_type = 0;
	const char* interface_mode = 0;
	dword dsp_nr = 0;

	if ((feature & 1U) != 0) {
		interface_type = "Analog";
	} else if ((feature & (1U << 1)) != 0) {
		interface_type = "BRI";
	} else if ((feature & (1U << 2)) != 0) {
		interface_type = "PRI";
	}

	dsp_nr = (feature >> 16) & 0xffU;

	if ((feature & ((1U << 24) | (1U << 25))) != 0) {
		interface_mode = "TE/NT";
	} else if ((feature & (1U << 24)) != 0) {
		interface_mode = "TE";
	} else if ((feature & (1U << 24)) != 0) {
		interface_mode = "NT";
	}

	if (interface_type != 0 && interface_mode != 0 && dsp_nr != 0) {
		(*dprintf_proc) ("  %s %s %d %s", interface_type, interface_mode, dsp_nr, dsp_nr > 1 ? "dsps" : "dsp");
	}
}

static void host_interface_decode_proc (dword feature, dword* hardware_features, dword* fpga_features,
                                        word (*dprintf_proc)(char *, ...)) {
	const char* host_interface = 0;

	if ((feature & 1U) != 0) {
		host_interface = "PCI 32Bit";
	} else if ((feature & (1U << 1)) != 0) {
		host_interface = "PCI 64Bit";
	} else if ((feature & (1U << 2)) != 0) {
		host_interface = "PCI Express";
	}

	if (host_interface != 0) {
		(*dprintf_proc) ("  %s", host_interface);
	}
}

static void local_interface_frequency_decode_proc (dword feature, dword* hardware_features, dword* fpga_features,
                                                   word (*dprintf_proc)(char *, ...)) {
	const char* local_interface_frequency = 0;

	if ((feature & 1U) != 0) {
		local_interface_frequency = "44.4 MHz";
	} else if ((feature & (1U << 1)) != 0) {
		local_interface_frequency = "49.152 MHz";
	} else if ((feature & (1U << 2)) != 0) {
		local_interface_frequency = "50 MHz";
	} else if ((feature & (1U << 3)) != 0) {
		local_interface_frequency = "66.667 MHz";
	} else if ((feature & (1U << 4)) != 0) {
		local_interface_frequency = "133.333 MHz";
	}
	if (local_interface_frequency != 0) {
		(*dprintf_proc) ("  %s", local_interface_frequency);
	}
}

static void version_number_decode_proc (dword feature, dword* hardware_features, dword* fpga_features,
                                        word (*dprintf_proc)(char *, ...)) {
}

static void test_version_number_decode_proc (dword feature, dword* hardware_features, dword* fpga_features,
                                             word (*dprintf_proc)(char *, ...)) {
}

static void beta_version_flag_decode_proc (dword feature, dword* hardware_features, dword* fpga_features,
                                           word (*dprintf_proc)(char *, ...)) {
	(*dprintf_proc) ("  %s",
					 (feature & 0x00000001) == 0 ? "Release" : "Beta");
}

static void date_decode_proc (dword feature, dword* hardware_features, dword* fpga_features,
                              word (*dprintf_proc)(char *, ...)) {
	(*dprintf_proc) ("  %d/%d/%02d%02d",
					 (feature >> 24) & 0xff,
					 (feature >> 16) & 0xff,
					 (feature >>  8) & 0xff,
					 feature & 0xff);
}

static void supported_fpga_features_decode_proc (dword feature, dword* hardware_features, dword* fpga_features,
                                                 word (*dprintf_proc)(char *, ...)) {
	const char* features[32];
	int i;

	fpga_features[0] = feature;

	for (i = 0; i < sizeof(features)/sizeof(features[0]); i++) {
    features[i] = "";
	}
	i = 0;

	if ((feature & 1U) != 0) {
		features[i++] = "UUID";
	}
	if ((feature & (1U << 1)) != 0) {
		if (i != 0) {
			features[i++] = ", ";
		}
		features[i++] = "BUS activity trace";
	}
	if ((feature & (1U << 3)) != 0) {
		if (i != 0) {
			features[i++] = ", ";
		}
		features[i++] = "Watchdog";

	}
	if ((feature & (1U << 4)) != 0) {
		if (i != 0) {
			features[i++] = ", ";
		}
		features[i++] = "SPI I/O MMU";
	} else if ((feature & (1U << 3)) != 0) {
		if (i != 0) {
			features[i++] = ", ";
		}
		features[i++] = "memory only SPI I/O MMU";
	}
	if ((feature & (1U << 5)) != 0) {
		if (i != 0) {
			features[i++] = ", ";
		}
		features[i++] = "FPGA DMA";
	}
	if ((feature & (1U << 6)) != 0) {
		if (i != 0) {
			features[i++] = ", ";
		}
		features[i++] = "New PPI";
	}
	if ((feature & (1U << 7)) != 0) {
		if (i != 0) {
			features[i++] = ", ";
		}
		features[i++] = "HW Timer";
	}
	if ((feature & (1U << 8)) != 0) {
		if (i != 0) {
			features[i++] = ", ";
		}
		features[i++] = "DMA desc cache";
	}
	if ((feature & (1U << 9)) != 0) {
		if (i != 0) {
			features[i++] = ", ";
		}

		features[i++] = "ProgOpMode";
	}
	if ((feature & (1U << 10)) != 0) {
		if (i != 0) {
			features[i++] = ", ";
		}
		features[i++] = "NMI"; /* NMI Interrupt Support */
	}
	if ((feature & (1U << 11)) != 0) {
		if (i != 0) {
			features[i++] = ", ";
		}
		features[i++] = "CPU L3"; /* CPU External Cache */
	}
	if ((feature & (1U << 12)) != 0) {
		if (i != 0) {
			features[i++] = ", ";
		}
		features[i++] = "PLL"; /* PLL Programming Support */
	}


	(*dprintf_proc) ("  %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
           features[0],
           features[1],
           features[2],
           features[3],
           features[4],
           features[5],
           features[6],
           features[7],
           features[8],
           features[9],
           features[10],
           features[10+1],
           features[10+2],
           features[10+3],
           features[10+4],
           features[10+5],
           features[10+6],
           features[10+7],
           features[10+8],
           features[10+9],
           features[10+10],
           features[10+11]);
}

void diva_show_fpga_rom_features (void* context,
                                  dword address,
                                  volatile dword* dummy_read_address,
                                  dword* hardware_features_address,
                                  dword* fpga_features_address,
                                  word (*dprintf_proc)(char *, ...),
                                  dword (*read_fpga_register_proc)(void*, dword),
                                  dword (*dummy_read_proc)(void*)) {
	dword fpga_rom_length_entries;
	dword i, max_entry_address, hardware_features = 0, fpga_features = 0;

	if (hardware_features_address == 0)
		hardware_features_address = &hardware_features;

	if (fpga_features_address == 0)
		fpga_features_address = &fpga_features;

	/*
		One uncached dummy read with cast to byte must follow FPGA info ROM access
		to workaround problem in first version of FPGA
		*/
	fpga_rom_length_entries = (*read_fpga_register_proc)(context, address) & 0xffff;
	*dummy_read_address = (byte)(*dummy_read_proc)(context);
	(*dprintf_proc) ("FPGA rom length: %d", fpga_rom_length_entries);

	if (fpga_rom_length_entries >= 8 && fpga_rom_length_entries < 32) {
		max_entry_address = sizeof(dword)*(fpga_rom_length_entries+1);

		for (i = 0; i < (dword)(sizeof(info)/sizeof(info[0])) && info[i].feature < (diva_fpga_rom_features_t)max_entry_address; i++) {
			dword v = (*read_fpga_register_proc)(context, address+info[i].feature) & info[i].bits;
			(*dprintf_proc) (" %-30s: %08x", info[i].name, v);
			(*(info[i].decode_proc))(v, hardware_features_address, fpga_features_address, dprintf_proc);
		}
	}
}

