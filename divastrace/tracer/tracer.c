
/*
 *
  Copyright (c) Dialogic(R), 2009-2014.
 *
  This source file is supplied for the use with
  Dialogic range of DIVA Server Adapters.
 *
  Dialogic(R) File Revision :    2.1
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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>

#include "st_ifc.h"

/*
	IMPORTS
	*/
extern word maxout;
extern word xlog (char *stream, void* buffer);

/*
	Functions provided by user application in order to get notifications
	from library about they state changs
	*/
static void example_error_proc (void* user_context,
																diva_strace_library_interface_t* hLib,
																int Adapter,
																int error,
																const char* file,
																int line);
static void state_change_notify_proc (void* user_context,
																			diva_strace_library_interface_t* hLib,
																			int Adapter,
																			diva_trace_line_state_t* channel,
																			int notify_subject);
static void trace_notify_proc (void* user_context,
															 diva_strace_library_interface_t* hLib,
															 int Adapter,
															 void* xlog_buffer);
static void print_usage (void);

/*
	LOCAL FUNCTIONS
	*/
static void print_line_status (diva_trace_line_state_t* channel);
static void print_modem_status (diva_trace_modem_state_t* modem);
static void print_fax_status (diva_trace_fax_state_t* fax);

/*
	LOCALS
	*/
static char trace_number[128];

int main (int argc, char** argv) {
	diva_trace_library_user_interface_t user = { 0,
																							 state_change_notify_proc,
																							 trace_notify_proc,
																							 example_error_proc };
	diva_strace_library_interface_t* hLib = \
					DivaSTraceLibraryCreateInstance (1, &user); /* Use adapter 1 */
	char buffer[24];

	if (argc > 1) {
		strcpy (trace_number, argv[1]); /* Save number to trace */
	} else {
		trace_number[0] = 0;
	}

	print_usage();

	maxout = 2048; /* Important action - set trace length for 'xlog' module */

	if (hLib) {
		int fd = (int)(long)(*(hLib->DivaSTraceGetHandle))(hLib->hLib);
		struct pollfd fds[2];

		memset (&fds, 0x00, sizeof (fds));
		fds[0].fd     = fd;
		fds[0].events = POLLIN;
		fds[1].fd     = STDIN_FILENO;
		fds[1].events = POLLIN;

		if (!(*(hLib->DivaSTraceSetDChannel))(hLib->hLib, 1)) {
			while (poll (fds, 2, -1) > 0) {
				if (fds[0].revents & POLLIN) {
					if ((*(hLib->DivaSTraceMessageInput))(hLib->hLib)) {
						break; /* error happens */
					}
				}
				if (fds[1].revents & POLLIN) {
					int length = read (STDIN_FILENO, buffer, sizeof(buffer)-1);
					if (length) {
						char* p;

						buffer[length] = 0;

						if        (buffer[0] == 'q') { /* quit */
							break;
						} else if (buffer[0] == 'c') { /* clear connection */
							int Channel = atoi(&buffer[1]);

							if (Channel) {
								if ((*(hLib->DivaSTraceClearCall))(hLib->hLib, Channel)) {
									break;
								}
							}
						} else if (buffer[0] == 's') { /* get statistics   */
							if ((*(hLib->DivaSTraceGetOutgoingCallStatistics))(hLib->hLib)) {
								break;
							}
							if ((*(hLib->DivaSTraceGetIncomingCallStatistics))(hLib->hLib)) {
								break;
							}
							if ((*(hLib->DivaSTraceGetLayer1Statistics))(hLib->hLib)) {
								break;
							}
							if ((*(hLib->DivaSTraceGetModemStatistics))(hLib->hLib)) {
								break;
							}
							if ((*(hLib->DivaSTraceGetFaxStatistics))(hLib->hLib)) {
								break;
							}
							if ((*(hLib->DivaSTraceGetBLayer1Statistics))(hLib->hLib)) {
								break;
							}
							if ((*(hLib->DivaSTraceGetBLayer2Statistics))(hLib->hLib)) {
								break;
							}
							if ((*(hLib->DivaSTraceGetDLayer1Statistics))(hLib->hLib)) {
								break;
							}
							if ((*(hLib->DivaSTraceGetDLayer2Statistics))(hLib->hLib)) {
								break;
							}
						}
					}
				}
				memset (&fds, 0x00, sizeof (fds));
				fds[0].fd     = fd;
				fds[0].events = POLLIN;
				fds[1].fd     = STDIN_FILENO;
				fds[1].events = POLLIN;
			}
		}

		(*(hLib->DivaSTraceLibraryFinit))(hLib->hLib);
	}
	return (0);
}

/*
	If library invoked this function then it means that some unrecoverable
	error is happens. The applications should shutdown library and exit in
	case 'error' is negative.
	In other case this is notification (warning) and application should
	self decide that should be done.
	*/
static void example_error_proc (void* user_context,
																diva_strace_library_interface_t* hLib,
																int Adapter,
																int error,
																const char* file,
																int line) {
	if (error < 0) {
		fprintf (stderr,
						 "E: Trace Library Error(%d) in file '%s' at line %d, errno=%d\n",
						 error, file, line, errno);
	} else {
		fprintf (stderr,
						 "W: Trace Library Warning(%d) '%s' - %d\n",
						 error, file, line);
	}

	fflush (0);
}

/*
	This function is called by library to inform used application about change
	in the channel state or in the state of modem/fax hardware that is
	associated with this state.
	*/
static void state_change_notify_proc (void* user_context,
																			diva_strace_library_interface_t* hLib,
																			int Adapter,
																			diva_trace_line_state_t* channel,
																			int notify_subject) {
	int on;
	int ret = 0;

	switch (notify_subject) {

		case DIVA_SUPER_TRACE_TEMPERATURE_CHANGE:
      printf ("Temperature: initial:%d min:%d max:%d, current:%d\n",
              channel->pInterface->InitialTemperature,
              channel->pInterface->MinTemperature,
              channel->pInterface->MaxTemperature,
              channel->pInterface->Temperature);
			break;

		case DIVA_SUPER_TRACE_NOTIFY_LINE_CHANGE:
			print_line_status (channel);
			/*
				In this example we turn for every activated channel
				Audio Traces and B-channel traces if channel was activated and
				off after channel was de-activated
				*/
			if (!trace_number[0] ||
					!strcmp (trace_number, &channel->RemoteAddress[0])) {
				on = (strcmp(channel->Line, "Idle") != 0);
				if ((*(hLib->DivaSTraceSetBChannel))(hLib->hLib,
																						 (int)channel->ChannelNumber, on)) {
					printf ("E: can't change B-channel trace mask\n");
				}
				if ((*(hLib->DivaSTraceSetAudioTap))(hLib->hLib,
																						 (int)channel->ChannelNumber, on)) {
					printf ("E: can't change Audio Tap trace mask\n");
				}
			}
			break;

		case DIVA_SUPER_TRACE_NOTIFY_MODEM_CHANGE:
			print_modem_status (&channel->modem);
			break;

		case DIVA_SUPER_TRACE_INTERFACE_CHANGE:
			printf ("\n----------------------------------------------\n");
			printf ("Interface Layer 1 -> [%s], Layer 2 -> [%s]\n",
							channel->pInterface->Layer1,
							channel->pInterface->Layer2);
			printf ("\n----------------------------------------------\n");
			break;

		case DIVA_SUPER_TRACE_NOTIFY_RESOURCE_UPDATE:
			printf ("\n----------------------------------------------\n");
			for (;channel->pInterface->Resource->resourceName[0] != 0; channel->pInterface->Resource++) {
				printf ("Name:'%s' Type:%c Standard:%u Visual Name:'%s' Count:%u\n",
								channel->pInterface->Resource->resourceName,
								channel->pInterface->Resource->resourceType,
								channel->pInterface->Resource->resourceStandard,
								channel->pInterface->Resource->resourceVisualName,
								channel->pInterface->Resource->resourceCount);
			}
			printf ("\n----------------------------------------------\n");
			break;

		case DIVA_SUPER_TRACE_NOTIFY_STAT_CHANGE:
			printf ("\n----------------------------------------------\n");
			printf ("Cumulative Interface Statistics\n");
			printf ("\nIncoming Calls:\n\n");
			printf ("Calls                     =%lu\n",
																channel->pInterfaceStat->inc.Calls);
			printf ("Connected                 =%lu\n",
																channel->pInterfaceStat->inc.Connected);
			printf ("User Busy                 =%lu\n",
																channel->pInterfaceStat->inc.User_Busy);
			printf ("Call Rejected             =%lu\n",
																channel->pInterfaceStat->inc.Call_Rejected);
			printf ("Wrong Number              =%lu\n",
																channel->pInterfaceStat->inc.Wrong_Number);
			printf ("Incompatible Destination  =%lu\n",
																channel->pInterfaceStat->inc.Incompatible_Dst);
			printf ("Out of Order              =%lu\n",
																channel->pInterfaceStat->inc.Out_of_Order);
			printf ("Ignored                   =%lu\n",
																channel->pInterfaceStat->inc.Ignored);

			printf ("\nOutgoing Calls:\n\n");
			printf ("Calls                     =%lu\n",
																channel->pInterfaceStat->outg.Calls);
			printf ("Connected                 =%lu\n",
																channel->pInterfaceStat->outg.Connected);
			printf ("User Busy                 =%lu\n",
																channel->pInterfaceStat->outg.User_Busy);
			printf ("No Answer                 =%lu\n",
																channel->pInterfaceStat->outg.No_Answer);
			printf ("Wrong Number              =%lu\n",
																channel->pInterfaceStat->outg.Wrong_Number);
			printf ("Call Rejected             =%lu\n",
																channel->pInterfaceStat->outg.Call_Rejected);
			printf ("Other Failures            =%lu\n",
																channel->pInterfaceStat->outg.Other_Failures);

			printf ("\nB-Layer1:\n\n");
			printf ("X-Frames                 =%lu\n",
																channel->pInterfaceStat->b1.X_Frames);
			printf ("X-Bytes                  =%lu\n",
																channel->pInterfaceStat->b1.X_Bytes);
			printf ("X-Errors                 =%lu\n",
																channel->pInterfaceStat->b1.X_Errors);
			printf ("R-Frames                 =%lu\n",
																channel->pInterfaceStat->b1.R_Frames);
			printf ("R-Bytes                  =%lu\n",
																channel->pInterfaceStat->b1.R_Bytes);
			printf ("R-Errors                 =%lu\n",
																channel->pInterfaceStat->b1.R_Errors);

			printf ("\nB-Layer2:\n\n");
			printf ("X-Frames                 =%lu\n",
																channel->pInterfaceStat->b2.X_Frames);
			printf ("X-Bytes                  =%lu\n",
																channel->pInterfaceStat->b2.X_Bytes);
			printf ("X-Errors                 =%lu\n",
																channel->pInterfaceStat->b2.X_Errors);
			printf ("R-Frames                 =%lu\n",
																channel->pInterfaceStat->b2.R_Frames);
			printf ("R-Bytes                  =%lu\n",
																channel->pInterfaceStat->b2.R_Bytes);
			printf ("R-Errors                 =%lu\n",
																channel->pInterfaceStat->b2.R_Errors);

			printf ("\nD-Layer1:\n\n");
			printf ("X-Frames                 =%lu\n",
																channel->pInterfaceStat->d1.X_Frames);
			printf ("X-Bytes                  =%lu\n",
																channel->pInterfaceStat->d1.X_Bytes);
			printf ("X-Errors                 =%lu\n",
																channel->pInterfaceStat->d1.X_Errors);
			printf ("R-Frames                 =%lu\n",
																channel->pInterfaceStat->d1.R_Frames);
			printf ("R-Bytes                  =%lu\n",
																channel->pInterfaceStat->d1.R_Bytes);
			printf ("R-Errors                 =%lu\n",
																channel->pInterfaceStat->d1.R_Errors);

			printf ("\nD-Layer2:\n\n");
			printf ("X-Frames                 =%lu\n",
																channel->pInterfaceStat->d2.X_Frames);
			printf ("X-Bytes                  =%lu\n",
																channel->pInterfaceStat->d2.X_Bytes);
			printf ("X-Errors                 =%lu\n",
																channel->pInterfaceStat->d2.X_Errors);
			printf ("R-Frames                 =%lu\n",
																channel->pInterfaceStat->d2.R_Frames);
			printf ("R-Bytes                  =%lu\n",
																channel->pInterfaceStat->d2.R_Bytes);
			printf ("R-Errors                 =%lu\n",
																channel->pInterfaceStat->d2.R_Errors);

			printf ("\n----------------------------------------------\n");
			break;

		case DIVA_SUPER_TRACE_NOTIFY_MDM_STAT_CHANGE:
			printf ("\nModem:\n\n");
			printf ("Disconnect Normal         =%lu\n",
																channel->pInterfaceStat->mdm.Disc_Normal);
			printf ("Disconnect Unspecified    =%lu\n",
																channel->pInterfaceStat->mdm.Disc_Unspecified);
			printf ("Disconnect Busy Tone      =%lu\n",
																channel->pInterfaceStat->mdm.Disc_Busy_Tone);
			printf ("Disconnect Congestion     =%lu\n",
																channel->pInterfaceStat->mdm.Disc_Congestion);
			printf ("Disconnect Carrier Wait   =%lu\n",
																channel->pInterfaceStat->mdm.Disc_Carr_Wait);
			printf ("Disconnect Trn Timeout    =%lu\n",
																channel->pInterfaceStat->mdm.Disc_Trn_Timeout);
			printf ("Disconnect Incompatible   =%lu\n",
																channel->pInterfaceStat->mdm.Disc_Incompat);
			printf ("Disconnect Frame Rejected =%lu\n",
																channel->pInterfaceStat->mdm.Disc_Frame_Rej);
			printf ("Disconnect V.42bis        =%lu\n",
																channel->pInterfaceStat->mdm.Disc_V42bis);
			break;

		case DIVA_SUPER_TRACE_NOTIFY_FAX_STAT_CHANGE:
			printf ("\nFax:\n\n");
			printf ("Disconnect Normal         =%lu\n",
																channel->pInterfaceStat->fax.Disc_Normal);
			printf ("Disconnect Not Ident      =%lu\n",
																channel->pInterfaceStat->fax.Disc_Not_Ident);
			printf ("Disconnect No Response    =%lu\n",
																channel->pInterfaceStat->fax.Disc_No_Response);
			printf ("Disconnect Retries        =%lu\n",
																channel->pInterfaceStat->fax.Disc_Retries);
			printf ("Disconnect Unexp. Msg.    =%lu\n",
																channel->pInterfaceStat->fax.Disc_Unexp_Msg);
			printf ("Disconnect no Polling     =%lu\n",
																channel->pInterfaceStat->fax.Disc_No_Polling);
			printf ("Disconnect Training       =%lu\n",
																channel->pInterfaceStat->fax.Disc_Training);
			printf ("Disconnect Unexpected     =%lu\n",
																channel->pInterfaceStat->fax.Disc_Unexpected);
			printf ("Disconnect Application    =%lu\n",
																channel->pInterfaceStat->fax.Disc_Application);
			printf ("Disconnect Incompatible   =%lu\n",
																channel->pInterfaceStat->fax.Disc_Incompat);
			printf ("Disconnect No Command     =%lu\n",
																channel->pInterfaceStat->fax.Disc_No_Command);
			printf ("Disconnect Long Msg       =%lu\n",
																channel->pInterfaceStat->fax.Disc_Long_Msg);
			printf ("Disconnect Supervisor     =%lu\n",
																channel->pInterfaceStat->fax.Disc_Supervisor);
			printf ("Disconnect SUP SEP PWD    =%lu\n",
																channel->pInterfaceStat->fax.Disc_SUB_SEP_PWD);
			printf ("Disconnect Invalid Msg    =%lu\n",
																channel->pInterfaceStat->fax.Disc_Invalid_Msg);
			printf ("Disconnect Page Coding    =%lu\n",
																channel->pInterfaceStat->fax.Disc_Page_Coding);
			printf ("Disconnect App Timeout    =%lu\n",
																channel->pInterfaceStat->fax.Disc_App_Timeout);
			printf ("Disconnect Unspecified    =%lu\n",
																channel->pInterfaceStat->fax.Disc_Unspecified);
			break;

		case DIVA_SUPER_TRACE_NOTIFY_FAX_CHANGE:
			print_fax_status (&channel->fax);
			break;

		default:
			break;
	}

	fflush (0);
}

static void print_ie (diva_trace_ie_t* ie) {
	int i;
	printf (" <");
	for (i = 0; i < ie->length; i++) {
		printf ("%02x", ie->data[i]);
		if (i < (ie->length-1))
			printf (" ");
	}
	printf (">");
}

static void print_line_status (diva_trace_line_state_t* channel) {
	printf ("\n----------------------------------------\n");
	printf ("Channel               = %d\n",   (int)channel->ChannelNumber);
	printf ("\n");
	printf ("Line Status           = <%s>\n", &channel->Line[0]);
	printf ("\n");
	printf ("Layer1                = <%s>\n", &channel->Framing[0]);
	printf ("Layer2                = <%s>\n", &channel->Layer2[0]);
	printf ("Layer3                = <%s>\n", &channel->Layer3[0]);
	printf ("\n");
	printf ("Remote Address        = <%s>\n", &channel->RemoteAddress[0]);
	printf ("Remote SubAddress     = <%s>\n", &channel->RemoteSubAddress[0]);
	printf ("Local Address         = <%s>\n", &channel->LocalAddress[0]);
	printf ("Local SubAddress      = <%s>\n", &channel->LocalSubAddress[0]);
	printf ("BC                    ="); print_ie(&channel->call_BC);printf("\n");
	printf ("HLC                   ="); print_ie(&channel->call_HLC);printf("\n");
	printf ("LLC                   ="); print_ie(&channel->call_LLC);printf("\n");
	printf ("Charding units        = %lu\n", channel->Charges);
	printf ("Call Reference        = 0x%x\n", channel->CallReference);
	printf ("Last Disconnect Cause = 0x%x\n", channel->LastDisconnectCause);
	printf ("\n");
	printf ("Call Owner            = <%s>\n", &channel->UserID[0]);
	printf ("\n----------------------------------------\n");
}

static void print_modem_status (diva_trace_modem_state_t* modem) {
	printf ("\n----------------------------------------\n");
	printf ("Channel               = %lu\n",     (int)modem->ChannelNumber);
	printf ("\n");
	printf ("Event                 = %lu\n",      modem->Event);
	printf ("Norm                  = %lu\n",      modem->Norm);
	printf ("Options               = 0x%08x\n",   modem->Options);
	printf ("Transmit Speed        = %lu Bps\n",  modem->TxSpeed);
	printf ("Receive  Speed        = %lu Bps\n",  modem->RxSpeed);
	printf ("Roundtrip             = %lu mSec\n", modem->RoundtripMsec);
	printf ("Symbol Rate           = %lu\n",      modem->SymbolRate);
	printf ("Rx Level              = %d dBm\n",   modem->RxLeveldBm);
	printf ("Echo Level            = %d dBm\n",   modem->EchoLeveldBm);
	printf ("SNR                   = %lu dB\n",   modem->SNRdb);
	printf ("MAE                   = %lu\n",      modem->MAE);
	printf ("Local Retrains        = %lu\n",      modem->LocalRetrains);
	printf ("Remote Retrains       = %lu\n",      modem->RemoteRetrains);
	printf ("Local Resyncs         = %lu\n",      modem->LocalResyncs);
	printf ("Remote Resyncs        = %lu\n",      modem->RemoteResyncs);
	printf ("Disconnect Reason     = %lu\n",      modem->DiscReason);
	printf ("\n----------------------------------------\n");
}

static void print_fax_status (diva_trace_fax_state_t* fax) {
	printf ("\n----------------------------------------\n");
	printf ("Channel               = %lu\n",     (int)fax->ChannelNumber);
	printf ("\n");
	printf ("Event                 = %lu\n",     fax->Event);
	printf ("Page Counter          = %lu\n",     fax->Page_Counter);
	printf ("Features              = 0x%08x\n",  fax->Features);
	printf ("Station ID            = <%s>\n",    &fax->Station_ID[0]);
	printf ("Subaddress            = <%s>\n",    &fax->Subaddress[0]);
	printf ("Password              = <%s>\n",    &fax->Password[0]);
	printf ("Speed                 = %lu\n",     fax->Speed);
	printf ("Resolution            = 0x%08x\n",  fax->Resolution);
	printf ("Paper Width           = %lu\n",     fax->Paper_Width);
	printf ("Paper Length          = %lu\n",     fax->Paper_Length);
	printf ("Scan Line Time        = %lu\n",     fax->Scanline_Time);
	printf ("Disconnect Reason     = %lu\n",     fax->Disc_Reason);
	printf ("\n----------------------------------------\n");
}


/*
	Notification about new trace information
	This function receives information that is contained inside of one
	'struct XLOG' buffer, that is filled with binary data and is self-contained
	(i.e. it contains information about event type, sub-type, channel and
	length).
	*/
static void trace_notify_proc (void* user_context,
															 diva_strace_library_interface_t* hLib,
															 int Adapter,
															 void* xlog_buffer) {
	byte tmp_buffer[4096];
	word length;

	/*
		Call to XLOG function that is able to decode any type of TRACE event
		issued by the card.
		*/
	tmp_buffer[0] = 0;
	length = xlog(&tmp_buffer[0], xlog_buffer);
	tmp_buffer[length] = 0;

	printf ("%s", tmp_buffer);
	fflush (0);
}

static void print_usage (void) {
	fprintf (stderr, "\n");
	fprintf (stderr, "Example Application for 'libDivaSTrace.a'\n");
	fprintf (stderr, "Management Interface Access Library for Diva range\n");
	fprintf (stderr, "of active adapters.\n\n");
	fprintf (stderr, "Copyright 1993 - 2001 by Eicon Networks\n");
	fprintf (stderr, "\n");
	fprintf (stderr, "Usage:\n");
	fprintf (stderr, "  tracer phone > log.txt - trace calls from user with\n");
	fprintf (stderr, "                           Calling Party Number 'phone'\n");
	fprintf (stderr, "  tracer       > log.txt - trace all calls\n");
	fprintf (stderr, "\n");
}

