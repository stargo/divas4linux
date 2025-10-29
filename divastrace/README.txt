
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

										Diva Management Interface Accesss API
                                    -
                   Copyright (c) 1993 - 2007 by Dialogic Corp.
                                    -

The "DivaSTrace" Management Interface Access Library allows access to contained
on Diva range of cards Management Interface.

The library interface is contained in the file "include/st_ifc.h".

'tracer' directory contains Management Interface Application example, that
uses Management Interface Access library.


  SHORT DESCRIPTION
----------------------------------------------------------------------------

The application creates one Management Interface Application instance by call
to function:

diva_strace_library_interface_t* DivaSTraceLibraryCreateInstance (int Adapter,
													const diva_trace_library_user_interface_t* user_proc);
Where:
  Adapter       - Logical Adapter number where Management Interface will be
                  accessed.
  user_proc     - structure that is provided by application and contains:
                  user_context      - Value provided by Library user and that
                                      will be passed to every callback function
                                      as first parameter.
                  notify_proc       - Callback function that will be called by
                                      library on state change.
                  trace_proc        - Callbck function that will be called by
                                      library if trace information is available.
                  error_notify_proc - Callbas function that will be called by
                                      library in case of internal error or
                                      warning event.

  Return Value  - pointer to Library interface that contains Library Instance
                  handle and Library Interface functions:

    hLib                                - Library Instance Handle. Shoule be
                                          passed as first parameter to all
                                          Library interface functions.
    DivaSTraceLibraryFinit              - Used to free the allocated by library
                                          resources and terminate the session.
                                          The Library Handle receives invalid
                                          after call to this function.
    DivaSTraceMessageInput              - Should be called in case one or more
                                          messages from the Diva adapter are
                                          available.
    DivaSTraceGetHandle                 - Called to conver Library Handle to OS
                                          dependent file/devive handle that can
                                          be used in OS specific functions like
                                          'poll', 'select',
                                          'WaitForMultipleObjects'.
    DivaSTraceSetAudioTap               - Called to turn Audio Tap trace on
                                          specified channel ON or OFF.
    DivaSTraceSetBChannel               - Called to turn B-channel trace on
                                          specified channel ON or OFF.
    DivaSTraceSetDChannel               - Called to turn Layer1, Layer2 and
                                          Layer3 traces for D-channel.
    DivaSTraceGetOutgoingCallStatistics - Initiate retrival of cumulative
                                          outgoing call statistics.
    DivaSTraceGetIncomingCallStatistics - Initiate retrival of cumulative
                                          incoming call statistics.
    DivaSTraceGetModemStatistics        - Initiate  retrival of cumulative
                                          modem statistics.
    DivaSTraceGetFaxStatistics          - Initiate retrival if cumulative
                                          fax statistics.
    DivaSTraceGetBLayer1Statistics      - Initiate retrival of cumulative
                                          bearer channel layer 1 statistics
    DivaSTraceGetBLayer2Statistics      - Initiate retrival of cumulative
                                          bearer channel layer 2 statistics
    DivaSTraceGetDLayer1Statistics      - Initiate retrival of cumulative
                                          signling channel layer 1 statistics
    DivaSTraceGetDLayer2Statistics      - Initiate retrival of cumulative
                                          signling channel layer 2 statistics
    DivaSTraceClearCall                 - Called to clear call that owned
                                          specified bearer channel.
    adapter_name                        - Adapter name. Filled by DivaSTrace library.
                                          Application can overwrite this value if necessary.
    adapter_serial_number               - Adapter serial number. Dilled by DivaSTrace library.
                                          Application can overwrite this value if necessary.

    USER INTERFACE DESCRIPTION
----------------------------------------------------------------------------

STRUCTURE: diva_trace_library_user_interface_t

MEMBERS TO BE FILLED BY CALLER:
  void*                                   user_context;
  diva_trace_channel_state_change_proc_t  notify_proc;
  diva_trace_channel_trace_proc_t         trace_proc;
  diva_trace_error_proc_t                 error_notify_proc;


void* user_context
  Arbitrary value that will be passed as first parameter to all user provided
  callback functions.


diva_trace_channel_state_change_proc_t  notify_proc;
typedef void (*diva_trace_channel_state_change_proc_t)(void* user_context,
                               struct _diva_strace_library_interface* hLib,
                               int Adapter,
                               diva_trace_line_state_t* channel,
                               int notify_subject);

  User provided callback function called in case event of interest (i.e. event
  that was requested to be reported to user) is arrived from the Diva Card.

  user_context:
    User provided value

  hLib:
    Pointer to library interface of library instance that reports this event
    (obtained by library initialization from 'DivaSTraceLibraryCreateInstance').

  Adapter:
    Logical Diva Adapter number (i.e. Interface Number) that reports this event.

  channel:
    Pointer to structure that contains information about bearer channel state,
    interface state and state of data processing instance (Fax, Modem)
    associated with this bearer channell.

  notify_subjects:
    Indicates event type (i.e. identify that exact was changed in the data
    structures that represent bearer and interface channel state).
    Following vaules possible:
      DIVA_SUPER_TRACE_NOTIFY_LINE_CHANGE
        Indicates change in the bearer channel state (i.e. incoming call,
        outgoing call, disconnect).

      DIVA_SUPER_TRACE_NOTIFY_MODEM_CHANGE
        Indicated change in the state of modem instance that is associated with
        this bearer channel.

      DIVA_SUPER_TRACE_NOTIFY_FAX_CHANGE
        Indicated change in the state of fax instance that is associated with
        this bearer channel.

      DIVA_SUPER_TRACE_INTERFACE_CHANGE
        Indicated change in the state of interface to that belongs this channel.

      DIVA_SUPER_TRACE_NOTIFY_STAT_CHANGE
        Indicates that requested by user update of cumulative interface
        statistics is completed.


diva_trace_channel_trace_proc_t trace_proc;
typedef void (*diva_trace_channel_trace_proc_t)(void* user_context,
                       struct _diva_strace_library_interface* hLib,
                       int Adapter,
                       void* xlog_buffer);

  User provided callback function called in case trace event (supposed
  appropriate type of tracing events is turned on) is arrived from Diva Card.

  user_context:
    User provided value

  hLib:
    Pointer to library interface of library instance that reports this event
    (obtained by library initialization from 'DivaSTraceLibraryCreateInstance').

  Adapter:
    Logical Diva Adapter number (i.e. Interface Number) that reports this event.

  xlog_buffer:
   Pointer to buffer that contains trace event in the XLOG format.
   In this way D-channel, Interface, B-channel and Audio Tap trace information
   is provided to the user.
   You will find description of this structure and the example function (xlog)
   that decodes these events in the file  src/log_b.c


  diva_trace_error_proc_t error_notify_proc;
  typedef void (*diva_trace_error_proc_t)(void* user_context,
                 struct _diva_strace_library_interface* hLib,
                 int Adapter,
                 int error,
                 const char* file,
                 int line);

  User provided callback function called in case of internal library error or
  management interface error.
  Dependent on error type user can ignore error (warning) or should terminate
  library instance.

  user_context:
    User provided value

  hLib:
    Pointer to library interface of library instance that reports this event
    (obtained by library initialization from 'DivaSTraceLibraryCreateInstance').

  Adapter:
    Logical Diva Adapter number (i.e. Interface Number) that reports this event.

  error:
    Error facility.
    Negative value indicates fatal error (i.e. user should terminate library
    instance).
    Positive value or zero indicates warning (i.e. can ignore this error or can
    re-init the library).
    Currently only one warning exists: '1' - "Event Lost", that indicats that
    one of the events was dropped by the card due to lack of internal resources.
    Dependent on type of application it can ignore this warning or should
    re-initialize (restart) library instance that had indicated this event.

  file:
    Source file name that raised error.

  line:
    Source code line (in the 'file') that raised error.


  LIBRARY INTERFACE DESCRIPTION
----------------------------------------------------------------------------
STRUCTURE: diva_strace_library_interface_t

MEMBERS FILLED BY LIBRARY:
  void* hLib;
  DivaSTraceLibraryFinit_proc_t DivaSTraceLibraryFinit;
  DivaSTraceMessageInput_proc_t DivaSTraceMessageInput;
  DivaSTraceGetHandle_proc_t    DivaSTraceGetHandle;
  DivaSTraceSetAudioTap_proc_t  DivaSTraceSetAudioTap;
  DivaSTraceSetBChannel_proc_t  DivaSTraceSetBChannel;
  DivaSTraceSetDChannel_proc_t  DivaSTraceSetDChannel;
  DivaSTraceGetOutgoingCallStatistics_proc_t
                              DivaSTraceGetOutgoingCallStatistics;
  DivaSTraceGetIncomingCallStatistics_proc_t
                              DivaSTraceGetIncomingCallStatistics;
  DivaSTraceGetModemStatistics_proc_t
                              DivaSTraceGetModemStatistics;
  DivaSTraceGetFaxStatistics_proc_t
                              DivaSTraceGetFaxStatistics;
  DivaSTraceGetBLayer1Statistics_proc_t
                              DivaSTraceGetBLayer1Statistics;
  DivaSTraceGetBLayer2Statistics_proc_t
                              DivaSTraceGetBLayer2Statistics;
  DivaSTraceGetDLayer1Statistics_proc_t
                              DivaSTraceGetDLayer1Statistics;
  DivaSTraceGetDLayer2Statistics_proc_t
                              DivaSTraceGetDLayer2Statistics;
  DivaSTraceClearCall_proc_t    DivaSTraceClearCall;
  char                        adapter_name[MAX_DIVA_ADAPTER_NAME];
  unsigned long               adapter_serial_number;


  All functions contained in the library interface return zero on success and
  negative value (error code) on error.

  void* hLib;
    Library Handle. Passed as first parameter to all library interface functions
    contained in the 'diva_strace_library_interface_t' structure.

  DivaSTraceLibraryFinit_proc_t DivaSTraceLibraryFinit;
  typedef int (*DivaSTraceLibraryFinit_proc_t)(void* hLib);

    Provided by library function used to terminate library instance and free
    all resources associated with library handle. Library handle is not more
    valid after this function call.

    hLib:
      Library handle contained in the library interface returned by function
      'DivaSTraceLibraryCreateInstance'



  DivaSTraceMessageInput_proc_t DivaSTraceMessageInput;
  typedef int   (*DivaSTraceMessageInput_proc_t)(void* hLib);

    Provided by library function used to notify library about incoming message.
    (i.e. if object returned by 'DivaSTraceGetHandle' function changes in the
     signaled state).

    hLib:
      Library handle contained in the library interface returned by function
      'DivaSTraceLibraryCreateInstance'



  DivaSTraceGetHandle_proc_t    DivaSTraceGetHandle;
  typedef void* (*DivaSTraceGetHandle_proc_t)(void* hLib);

    Provided by library function used to convert library handle to OS dependent
    HANDLE object. (i.e. to file handle that can be passed to 'poll' and
    'select' functions on UNIX systems, to HANDLE that can be passed to
    WaitForSingleObject/WaitForMultipleObjects on WIN32 system).
    Once this file handle changes to signaled state 'DivaSTraceMessageInput'
    library interface function should be called by application.

    hLib:
      Library handle contained in the library interface returned by function
      'DivaSTraceLibraryCreateInstance'



  DivaSTraceSetAudioTap_proc_t  DivaSTraceSetAudioTap;
  typedef int (*DivaSTraceSetAudioTap_proc_t)(void* hLib, int Channel, int on);

    Provided by library function used to turn Audio Tap (G.711 coded audio
    stream captured direct from TDM interface) processing on or off.
    Audio Tap information is delivered in XLOG format via 'trace_proc' as
    sequence of binary data blocks. Every block consists from sequence of pair
    of 8 Bit values. First byte in pair contains transmit path, second byte
    receive path data. Values are coded as A-Law or u-Law, in accordance with
    national settings.

    hLib:
      Library handle contained in the library interface returned by function
      'DivaSTraceLibraryCreateInstance'

    Channel:
      Bearer channel number to turn Audio Tap processong on or off.

    on:
      Use 1 to turn Audio Tap processing on. Use 0 to turn Audio Tap processing
      off.



  DivaSTraceSetBChannel_proc_t  DivaSTraceSetBChannel;
  typedef int (*DivaSTraceSetBChannel_proc_t)(void* hLib, int Channel, int on);

    Provided by library function used to turn B-channel (i.e. data sent or
    received by application) on or off.
    The data is captured at interface between OSI Layer 1 and 2. This means that
    traces contains Layer 2 header and compressed bearer data in case non
    transparent and data correction is used.
    B-channel trace information is delivered in XLOG format via 'trace_proc' as
    of binary data blocks. Every block contains data in transmit or in receive
    direction.

    hLib:
      Library handle contained in the library interface returned by function
      'DivaSTraceLibraryCreateInstance'

    Channel:
      Bearer channel number to turn B-channel tracing processong on or off.

    on:
      Use 1 to turn B-channel tracing on. Use 0 to turn B-channel tracing off.



  DivaSTraceSetDChannel_proc_t  DivaSTraceSetDChannel;
  typedef int (*DivaSTraceSetDChannel_proc_t)(void* hLib, int on);

    Provided by library function used to turn D-channel tracing information on
    or off. D-channel trace includes:
      - Layer 1 interface events that represent physical state of the E1/T1
        link access hardware (i.e. UP, DOWN, SYNC_LOST, SYNC, ...).
      - All D-channel frames received or sent over adapter interface inclusive
        Layer 2 header, Layer 2 link management frames and TEI management
        frames.
      - Events from Layer 2 instance responsible for transfer of signaling
        protocol D-channel frames, i.e. from signaling link (EVENT- ...).
      - D-channel frames send, received and generated by signaling protocol
        state machine.
      - Events from signaling protocol state machine (SIG-EVENT ...).
    D-channel trace information is delivered in XLOG format via 'trace_proc' as
    binary data blocks. Every block contains data associated with specific type
    of event.

    hLib:
      Library handle contained in the library interface returned by function
      'DivaSTraceLibraryCreateInstance'

    on:
      Use 1 to turn D-channel tracing on. Use 0 to turn D-channel tracing off.



  DivaSTraceGetOutgoingCallStatistics_proc_t
                                       DivaSTraceGetOutgoingCallStatistics;
  typedef int (*DivaSTraceGetOutgoingCallStatistics_proc_t)(void* hLib);

  Provided by library function used to retrieve information contained in the
  "Statistics\Outgoing Calls" management interface directory, that contains
  cumulative outgoing call statistics for Diva Adapter, that is represented by
  library instance.
  Requested information is stored inside of 'diva_outgoing_call_statistics_t'
  structure. Application is informed about update by means of 'notify_proc'with
  notify_subjects equal to DIVA_SUPER_TRACE_NOTIFY_STAT_CHANGE;
	The requested information can be accessed via channel pointer, that is passed
  as argument to the 'notify_proc': every channel pointer (that represents
  one bearer channel) contains 'pInterfaceStat' pointer to global for this
  library instance structure 'diva_ifc_statistics_t'. This structure
  contains requested information in the 'outg' member
  (i.e. 'channel->pInterfaceStat->outg').
  The information in this directory is related to the D-channel protocol
  (i.e. if from the view if the D-channel protocol bearer channel was
  established, and call was add to 'Connected', then it is still possibme that
  high level bearer protocol or application will later indicate that call is
  failed due to protocol - specific error)

  STRUCTURE: diva_outgoing_call_statistics_t
	  dword Calls            - Total amount of calls, inclusive calls that
                             currently not in connected state
	  dword Connected        - Total amount of successful calls
	  dword User_Busy        - Opposite side was BUSY
	  dword No_Answer        - Opposite sede not alswered the call
	  dword Wrong_Number     - Wrong destination number was reported from network
	  dword Call_Rejected    - Call was rejected by opposite side
	  dword Other_Failures   - Other failure cause was reported by opposite side
                             or by network

    hLib:
      Library handle contained in the library interface returned by function
      'DivaSTraceLibraryCreateInstance'



  DivaSTraceGetIncomingCallStatistics_proc_t
                                       DivaSTraceGetIncomingCallStatistics;
  typedef int (*DivaSTraceGetIncomingCallStatistics_proc_t)(void* hLib);

  Provided by library function used to retrieve information contained in the
  "Statistics\Incoming Calls" management interface directory, that contains
  cumulative incoming call statistics for Diva Adapter, that is represented by
  library instance.
  Requested information is stored inside of 'diva_incoming_call_statistics_t'
  structure. Application is informed about update by means of 'notify_proc'with
  notify_subjects equal to DIVA_SUPER_TRACE_NOTIFY_STAT_CHANGE;
	The requested information can be accessed via channel pointer, that is passed
  as argument to the 'notify_proc': every channel pointer (that represents
  one bearer channel) contains 'pInterfaceStat' pointer to global for this
  library instance structure 'diva_ifc_statistics_t'. This structure
  contains requested information in the 'inc' member
  (i.e. 'channel->pInterfaceStat->inc').
  The information in this directory is related to the D-channel protocol
  (i.e. if from the view if the D-channel protocol bearer channel was
  established, and call was add to 'Connected', then it is still possibme that
  high level bearer protocol or application will later indicate that call is
  failed due to protocol - specific error)

  STRUCTURE: diva_incoming_call_statistics_t
    dword Calls            - Total amount of calls, inclusive calls that
                             currently not in connected state
    dword Connected        - Total amount of successful calls
    dword User_Busy        - Application was BUSY
    dword Call_Rejected    - Application rejected the call
    dword Wrong_Number     - Wrong Number was dialed
    dword Incompatible_Dst - Call was incompatible with Application
    dword Out_of_Order     - Call failed due to 'out of order'
    dword Ignored          - Call was ignored

    hLib:
      Library handle contained in the library interface returned by function
      'DivaSTraceLibraryCreateInstance'



  DivaSTraceGetModemStatistics_proc_t DivaSTraceGetModemStatistics;
  typedef int (*DivaSTraceGetModemStatistics_proc_t)(void* hLib);

  Provided by library function used to retrieve information from the
  "Statistics\Modem" management interface directory, that contains cumulative
  statistics for modem instances running on Diva Adapter that is represented by
  library instance.
  Requested information is stored inside of 'diva_modem_call_statistics_t'
  structure. Application is informed about update by means of 'notify_proc'with
  notify_subjects equal to DIVA_SUPER_TRACE_NOTIFY_STAT_CHANGE;
	The requested information can be accessed via channel pointer, that is passed
  as argument to the 'notify_proc': every channel pointer (that represents
  one bearer channel) contains 'pInterfaceStat' pointer to global for this
  library instance structure 'diva_ifc_statistics_t'. This structure
  contains requested information in the 'mdm' member
  (i.e. 'channel->pInterfaceStat->mdm').

  STRUCTURE: diva_modem_call_statistics_t
    dword Disc_Normal        - Total amount of successful modem calls
    dword Disc_Unspecified   - Disconnects due to unusual condition
                               (carrier loss, ...)
    dword Disc_Busy_Tone     - Disconnects due to BUSY tone detection
    dword Disc_Congestion    - Disconnects due to congestion at disconnect phase
                               (both sides tried to disconnect simultaneous,
                                possible if longer line quality degradation
                                arrived)
    dword Disc_Carr_Wait     - Disconnects due to carrier wait timeout
                               (possible if opposite side is not a modem, or
                                modem at opposite side violates timing
                                recommndations)
    dword Disc_Trn_Timeout   - Disconnects due to training timeout
    dword Disc_Incompat      - Disconnects due to incompatible opposite side
                               (for example if opposite side does not support
                                modulations thar are supported by Diva Card and
                                vice versa)
    dword Disc_Frame_Rej     - Disconnects due to invalid frame (MNP, V.42)
    dword Disc_V42bis        - Disconnects due to V.42bis error (dictionary
                                inconsistence).

    hLib:
      Library handle contained in the library interface returned by function
      'DivaSTraceLibraryCreateInstance'



  DivaSTraceGetFaxStatistics_proc_t DivaSTraceGetFaxStatistics;
  typedef int (*DivaSTraceGetFaxStatistics_proc_t)(void* hLib);

  Provided by library function used to retrieve information from the
  "Statistics\Fax" management interface directory, that contains cumulative
  statistics for fax instances running on Diva Adapter that is represented by
  library instance.
  Requested information is stored inside of 'diva_fax_call_statistics_t'
  structure. Application is informed about update by means of 'notify_proc'with
  notify_subjects equal to DIVA_SUPER_TRACE_NOTIFY_STAT_CHANGE;
	The requested information can be accessed via channel pointer, that is passed
  as argument to the 'notify_proc': every channel pointer (that represents
  one bearer channel) contains 'pInterfaceStat' pointer to global for this
  library instance structure 'diva_ifc_statistics_t'. This structure
  contains requested information in the 'fax' member
  (i.e. 'channel->pInterfaceStat->fax').

  STRUCTURE: diva_fax_call_statistics_t
    dword Disc_Normal       - Total successful fax calls
    dword Disc_Not_Ident    -
    dword Disc_No_Response  - Opposite side not answered the call
                              (possible cause: opposite side not a fax device)
    dword Disc_Retries      - Maximal amount of retries was reached (for example
                              for page or HDLC frame re-transmission)
    dword Disc_Unexp_Msg    - Unexpected message was received from opposite side
                              (T.30 protocol)
    dword Disc_No_Polling   - Opposite side rejected polling request
    dword Disc_Training     - Disconnect due to training failure (possible
                              if by poor line quality opposite side does not
                              switch to lower speen)
    dword Disc_Unexpected   - Disconnect due to unexpected condition
                              (for example if line is disconnected in the middle
                               of fax documant transmission)
    dword Disc_Application  - Disconnects due to request from application
    dword Disc_Incompat     - Disconnects due to incompatiblity (it was not
                              possible to agree with opposite side on common
                              set of parameters)
    dword Disc_No_Command   -
    dword Disc_Long_Msg     -
    dword Disc_Supervisor   -
    dword Disc_SUB_SEP_PWD  - Wrong polling password was received from opposite
                              side
    dword Disc_Invalid_Msg  - Invalid message was received from opposite side
                              (T.30 protocol)
    dword Disc_Page_Coding  -
    dword Disc_App_Timeout  - Disconnects due to timing vi
    dword Disc_Unspecified  -

    hLib:
      Library handle contained in the library interface returned by function
      'DivaSTraceLibraryCreateInstance'



  DivaSTraceGetBLayer1Statistics_proc_t DivaSTraceGetBLayer1Statistics;
  typedef int (*DivaSTraceGetBLayer1Statistics_proc_t)(void* hLib);

  Provided by library function used to retrieve information from the
  "Statistics\B-Layer1" management interface directory, that contains cumulative
  statistics for layer 1 hardware interface of Diva Adapter that is represented
  by library instance.
  Requested information is stored inside of 'diva_prot_statistics_t' structure.
  Application is informed about update by means of 'notify_proc'with
  notify_subjects equal to DIVA_SUPER_TRACE_NOTIFY_STAT_CHANGE.
	The requested information can be accessed via channel pointer, that is passed
  as argument to the 'notify_proc': every channel pointer (that represents
  one bearer channel) contains 'pInterfaceStat' pointer to global for this
  library instance structure 'diva_ifc_statistics_t'. This structure
  contains requested information in the 'b1' member (i.e.
  'channel->pInterfaceStat->b1').

  STRUCTURE: diva_prot_statistics_t
    dword X_Frames   - Amount of gata frames sent over the einterface
    dword X_Bytes    - Amount of bytes sent over the interrace
    dword X_Errors   - Amount of transmit errors
    dword R_Frames   - Amount of gata frames received over the einterface
    dword R_Bytes    - Amount of bytes received over the interrace
    dword R_Errors   - Amount of receiving errors

    hLib:
      Library handle contained in the library interface returned by function
      'DivaSTraceLibraryCreateInstance'



  DivaSTraceGetBLayer2Statistics_proc_t DivaSTraceGetBLayer2Statistics;
  typedef int (*DivaSTraceGetBLayer2Statistics_proc_t)(void* hLib);

  Provided by library function used to retrieve information from the
  "Statistics\B-Layer2" management interface directory, that contains cumulative
  statistics for layer 2 interface of Diva Adapter that is represented by
  library instance.
  Requested information is stored inside of 'diva_prot_statistics_t' structure.
  Application is informed about update by means of 'notify_proc'with
  notify_subjects equal to DIVA_SUPER_TRACE_NOTIFY_STAT_CHANGE.
	The requested information can be accessed via channel pointer, that is passed
  as argument to the 'notify_proc': every channel pointer (that represents
  one bearer channel) contains 'pInterfaceStat' pointer to global for this
  library instance structure 'diva_ifc_statistics_t'. This structure
  contains requested information in the 'b2' member (i.e.
  'channel->pInterfaceStat->b2').

  STRUCTURE: diva_prot_statistics_t
    dword X_Frames   - Amount of gata frames sent over the einterface
    dword X_Bytes    - Amount of bytes sent over the interrace
    dword X_Errors   - Amount of transmit errors
    dword R_Frames   - Amount of gata frames received over the einterface
    dword R_Bytes    - Amount of bytes received over the interrace
    dword R_Errors   - Amount of receiving errors

    hLib:
      Library handle contained in the library interface returned by function



  DivaSTraceGetDLayer1Statistics_proc_t DivaSTraceGetDLayer1Statistics;
  typedef int (*DivaSTraceGetDLayer1Statistics_proc_t)(void* hLib);

  Provided by library function used to retrieve information from the
  "Statistics\D-Layer1" management interface directory, that contains cumulative
  statistics for layer 1 D-channel hardware interface of Diva Adapter that is
  represented by library instance.
  Requested information is stored inside of 'diva_prot_statistics_t' structure.
  Application is informed about update by means of 'notify_proc'with
  notify_subjects equal to DIVA_SUPER_TRACE_NOTIFY_STAT_CHANGE.
	The requested information can be accessed via channel pointer, that is passed
  as argument to the 'notify_proc': every channel pointer (that represents
  one bearer channel) contains 'pInterfaceStat' pointer to global for this
  library instance structure 'diva_ifc_statistics_t'. This structure
  contains requested information in the 'd1' member (i.e.
  'channel->pInterfaceStat->d1').

  STRUCTURE: diva_prot_statistics_t
    dword X_Frames   - Amount of gata frames sent over the einterface
    dword X_Bytes    - Amount of bytes sent over the interrace
    dword X_Errors   - Amount of transmit errors
    dword R_Frames   - Amount of gata frames received over the einterface
    dword R_Bytes    - Amount of bytes received over the interrace
    dword R_Errors   - Amount of receiving errors

    hLib:
      Library handle contained in the library interface returned by function



  DivaSTraceGetDLayer2Statistics_proc_t DivaSTraceGetDLayer2Statistics;
  typedef int (*DivaSTraceGetDLayer2Statistics_proc_t)(void* hLib);

  Provided by library function used to retrieve information from the
  "Statistics\D-Layer2" management interface directory, that contains cumulative
  statistics for layer 2 D-channel interface of Diva Adapter that is represented
  by library instance.
  Requested information is stored inside of 'diva_prot_statistics_t' structure.
  Application is informed about update by means of 'notify_proc' with
  notify_subjects equal to DIVA_SUPER_TRACE_NOTIFY_STAT_CHANGE.
	The requested information can be accessed via channel pointer, that is passed
  as argument to the 'notify_proc': every channel pointer (that represents
  one bearer channel) contains 'pInterfaceStat' pointer to global for this
  library instance structure 'diva_ifc_statistics_t'. This structure
  contains requested information in the 'd2' member (i.e.
  'channel->pInterfaceStat->d2').

  STRUCTURE: diva_prot_statistics_t
    dword X_Frames   - Amount of gata frames sent over the einterface
    dword X_Bytes    - Amount of bytes sent over the interrace
    dword X_Errors   - Amount of transmit errors
    dword R_Frames   - Amount of gata frames received over the einterface
    dword R_Bytes    - Amount of bytes received over the interrace
    dword R_Errors   - Amount of receiving errors

    hLib:
      Library handle contained in the library interface returned by function



  DivaSTraceClearCall_proc_t    DivaSTraceClearCall;
  typedef int (*DivaSTraceClearCall_proc_t)(void* hLib, int Channel);

    Provided by library function used to clear a call that taken ownership over
    specified bearer channel.
    In case bearer channel is free no action will be taken.

    hLib:
      Library handle contained in the library interface returned by function
      'DivaSTraceLibraryCreateInstance'

    Channel:
      Bearer channel (B-channel) number



    INTERFACE STATE CHANGE NOTIFICATION
----------------------------------------------------------------------------
  The "State\Layer1" and "State\Layer2 ..." management interface directory
  allows notification about changes of the state of Layer 1 hardware interfaces
  and about state changes of the Layer 2 associated with signaling channel.
  In the difference to the information provided by Layer 1 trace
  interface this information is presented in a format independent from
  national standards and interface type.

  This information is provided by means of 'notify_proc' with notify_subject
  equal to DIVA_SUPER_TRACE_INTERFACE_CHANGE.
  The information is contained in the "diva_trace_interface_state_t" structure
  that is accessible via 'pInterface' pointer that is contained in the 'channel'
  pointer that is passed to the 'notify_proc' callback (i.e.
  channel->pInterface).

  STRUCTURE: diva_trace_interface_state_t
    char Layer1[DIVA_TRACE_LINE_TYPE_LEN]  - Layer 1 interface state coided as
                                             ASCII string.
    char Layer2[DIVA_TRACE_LINE_TYPE_LEN]  - Layer 2 state coded as ASCII
                                             string.
    diva_ifc_config_t * pConfig            - Pointer to struct describing general
                                             interface type and mode (see below
                                             "INTERFACE GENERAL CONFIG AND INFO")



    BEARER CHANNEL STATE CHANGE NOTIFICATION
----------------------------------------------------------------------------

  Every library instance reports to the application state of the bearer
  channels on the Diva adapter represented by this library instance.
  The information about line status is delivered to application via
  'notify_proc' with notify subject equal to
  DIVA_SUPER_TRACE_NOTIFY_LINE_CHANGE.
  The information about current line state (contained in the Management
  Interface directory "State\BX", where - X bearer channel number) is storef in
  the 'diva_trace_line_state_t' structure that is accessible via pointer passed
  to user provided callback function 'notify_proc'.

  STRUCTURE: diva_trace_line_state_t
    dword	ChannelNumber - Bearer Channel number
    char Line[DIVA_TRACE_LINE_TYPE_LEN]
                        - Berer channel state coded as ASCII string
    char Framing[DIVA_TRACE_LINE_TYPE_LEN]
                        - Type of Layer 1 hardware assigned by application to
                          process bearer data, coded as ASCII string
    char Layer2[DIVA_TRACE_LINE_TYPE_LEN]
                        - Type of Layer 2 protocol assigned by application to
                          process bearer data, coded as ASCII string
    char Layer3[DIVA_TRACE_LINE_TYPE_LEN]
                        - Type of Layer 3 protocol assigned by application to
                          process bearer data, coded as ASCII string
    char RemoteAddress[DIVA_TRACE_LINE_TYPE_LEN]
                        - Remote Address, coded as ASCII string
                          (Calling Party Number for incoming calls and
                           Called Party Number for outgoing calls)
    char RemoteSubAddress[DIVA_TRACE_LINE_TYPE_LEN]
                        - Remote Sub Address, coded as ASCII string
    char LocalAddress[DIVA_TRACE_LINE_TYPE_LEN]
                        - Local Address, coded as ASCII string
                          (MSN for incoming calls and
                           Origination Number for outgoing calls)
    char LocalSubAddress[DIVA_TRACE_LINE_TYPE_LEN]
                        - Local Sub Address, coded as ASCII string
    diva_trace_ie_t	call_BC
                        - Bearer Capabilitues, coded as information element
                          in accordance with Q.931
    diva_trace_ie_t	call_HLC
                        - High Level Capabilitues, coded as information element
                          in accordance with Q.931
    diva_trace_ie_t	call_LLC
                        - Low Level Capabilitues, coded as information element
                          in accordance with Q.931
    dword Charges       - Amount of charring units for current call
    dword CallReference - Call reference used for current call
    dword LastDisconnectCause
                        - Disconnect cause of the last call, coded in accordance
                          with Q.931
    char UserID[DIVA_TRACE_LINE_TYPE_LEN]
                        - Name of application that owned this call, coded as
                          ASCII string (for example name of the TTY interface).



    MODEM INSTANCE STATE CHANGE NOTIFICATION
----------------------------------------------------------------------------

  Every library instance reports to the application state of modem instances
  associated with bearer channels of Diva adapter represented by this library
  instance.
  The information about modem instance state change is delivered to application
  via 'notify_proc' with notify subject equal to
  DIVA_SUPER_TRACE_NOTIFY_MODEM_CHANGE.
  This event is generated every time if state of the modem instance changes, for
  example after re-train and re-negotiation. The event is generated still if
  modem parameters do not change their values.
  The information about current line state (contained in the Management
  Interface directory "State\BX\Modem", where - X bearer channel number) is
  stored in the 'diva_trace_modem_state_t' structure that is contained in the
  'modem' member of the 'diva_trace_line_state_t' structure that is accessible
  via pointer passed to user provided callback function 'notify_proc' (i.e.
  channel->modem).

  STRUCTURE: diva_trace_modem_state_t
    dword	ChannelNumber     - Bearer channel number
    dword	Event             - Current Modem Instance state
                              0 - Idle
                              1 - Connected
                              2 - Connecting
                              3 - Disconnecting
    dword	Norm              - Current Connected norm
    dword Options           - Options used to create this call (Eicon Internal)
    dword	TxSpeed           - Current Transmit speed, Bps
    dword	RxSpeed           - Current Receive speed, Bps
    dword RoundtripMsec     - Current Round Trip value, mSec
    dword SymbolRate        - Current Symbol Rate
    int		RxLeveldBm        - Current Receive Signal level, dBm
    int		EchoLeveldBm      - Current Echo Signal level, dBm
    dword	SNRdb             - Current Signal to Noise Ratio, dB
    dword MAE               - Current Mean Absolute Error
    dword LocalRetrains     - Amount of local retrains
    dword RemoteRetrains    - Amount of remote retrains
    dword LocalResyncs      - Amount of local re-synchronizations
    dword RemoteResyncs     - Amount of remote re-synchronizations
    dword DiscReason        - Disconnect reason of previous connection (i.e.
                              changes if the 'Event' is set to 3).

    FAX INSTANCE STATE CHANGE NOTIFICATION
----------------------------------------------------------------------------

  Every library instance reports to the application state of fax instances
  associated with bearer channels of Diva adapter represented by this library
  instance.
  The information about fax instance state change is delivered to application
  via 'notify_proc' with notify subject equal to
  DIVA_SUPER_TRACE_NOTIFY_FAX_CHANGE.
  This event is generated every time if state of the fax istance changes.
  The information about current line state (contained in the Management
  Interface directory "State\BX\FAX", where - X bearer channel number) is stores
  in the 'diva_trace_fax_state_t' structure that is contained in the 'fax'
  member of the 'diva_trace_line_state_t' structure that is accessible via
  pointer passed to 'notify_proc' user provided callback function (i.e.
  channel->fax).

  STRUCTURE: diva_trace_fax_state_t
    dword	ChannelNumber      - Bearer Channel Number
    dword Event              - Current Fax Instance State
    dword Page_Counter       - Page Counter
    dword Features           - Fax Features Bit Mask
                               0x0001 - Fine Resolution
                               0x0002 - Error Correction Mode
                               0x0004 - ECM packet length 64 Bytes
                               0x0008 - 2D coding
                               0x0010 - T.6 coding
                               0x0020 - Enable uncompressed T.6 frames
                               0x0040 - Polling
                               0x0100 - More Documents (T.30) in process
    char Station_ID[DIVA_TRACE_FAX_PRMS_LEN]
                             - Current used Station Identifyer
    char Subaddress[DIVA_TRACE_FAX_PRMS_LEN]
                             - Current used Sub-Address
    char Password[DIVA_TRACE_FAX_PRMS_LEN]
                             - Current used polling password
    dword Speed              - Current Data Transfer Speed, Bps
    dword Resolution         - Current Resolution
    dword Paper_Width        - Current Paper Width
    dword Paper_Length       - Current Paper Length
    dword Scanline_Time      - Current Line Scan Time
    dword Disc_Reason        - Disconnect reason
    dword	dummy              - Unused



    INTERFACE GENERAL CONFIG AND INFO
----------------------------------------------------------------------------
  During initialisation of the library instances the type of adapter and the
  active protocol is read and stored in the following structure.
  For this information there is no notification provided. The information can
  be retrieved by reading the below mentioned struct which can be accessed by
  the pointer channel->pInterface->pConfig. (See also "INTERFACE STATE CHANGE
  NOTIFICATION" above). For the PRI alarms a notification of type
  DIVA_SUPER_TRACE_INTERFACE_CHANGE is triggered.


  STRUCTURE: diva_ifc_config_t
    int    type       - Type of adapter: PRI / BRI / Analog.
    dword  protocol   - Active Protocol as reported by Config\DChannel\Protocol
                         or Config\Protocol (fallback for older protocol versions
                         or 1TR6).
    dword  channels   - Number of available channels.
    dword  NTmode     - 0 = TE mode, 1 = NT mode
    dword  TEI        - Value of SPID-1\TEI. Needed to distinguish between
                         PointToPoint and PointToMultiPoint.
    dword  alarm_red
    dword  alarm_yellow
    dword  alarm_blue - State of the corresponding HW alarms. Only valid for PRI.

