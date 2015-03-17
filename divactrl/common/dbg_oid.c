
/*
 *
  Copyright (c) Eicon Networks, 2005.
 *
  This source file is supplied for the use with
  Eicon Networks range of DIVA Server Adapters.
 *
  Eicon File Revision :    2.1
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
/*
 * debug driver OID constants filter
 */
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dbgioctl.h"
/******************************************************************************/
static char *
GetOidString (dword Oid)
{
 static char name[80] ;
 switch ((Oid >> 24) & 0xFF)
 {
/*
 * General Objects
 */
 case 0x00:
  switch ((Oid >> 8) & 0xFFFF)
  {
  case 0x0101:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_GEN_SUPPORTED_LIST") ;
   case 0x02: return ("OID_GEN_HARDWARE_STATUS") ;
   case 0x03: return ("OID_GEN_MEDIA_SUPPORTED") ;
   case 0x04: return ("OID_GEN_MEDIA_IN_USE") ;
   case 0x05: return ("OID_GEN_MAXIMUM_LOOKAHEAD") ;
   case 0x06: return ("OID_GEN_MAXIMUM_FRAME_SIZE") ;
   case 0x07: return ("OID_GEN_LINK_SPEED") ;
   case 0x08: return ("OID_GEN_TRANSMIT_BUFFER_SPACE") ;
   case 0x09: return ("OID_GEN_RECEIVE_BUFFER_SPACE") ;
   case 0x0A: return ("OID_GEN_TRANSMIT_BLOCK_SIZE") ;
   case 0x0B: return ("OID_GEN_RECEIVE_BLOCK_SIZE") ;
   case 0x0C: return ("OID_GEN_VENDOR_ID") ;
   case 0x0D: return ("OID_GEN_VENDOR_DESCRIPTION") ;
   case 0x0E: return ("OID_GEN_CURRENT_PACKET_FILTER") ;
   case 0x0F: return ("OID_GEN_CURRENT_LOOKAHEAD") ;
   case 0x10: return ("OID_GEN_DRIVER_VERSION") ;
   case 0x11: return ("OID_GEN_MAXIMUM_TOTAL_SIZE") ;
   case 0x12: return ("OID_GEN_PROTOCOL_OPTIONS") ;
   case 0x13: return ("OID_GEN_MAC_OPTIONS") ;
   case 0x14: return ("OID_GEN_MEDIA_CONNECT_STATUS") ;
   case 0x15: return ("OID_GEN_MAXIMUM_SEND_PACKETS") ;
   case 0x16: return ("OID_GEN_VENDOR_DRIVER_VERSION") ;
   default: break ;
   }
   break ;
  case 0x0201:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_GEN_XMIT_OK") ;
   case 0x02: return ("OID_GEN_RCV_OK") ;
   case 0x03: return ("OID_GEN_XMIT_ERROR") ;
   case 0x04: return ("OID_GEN_RCV_ERROR") ;
   case 0x05: return ("OID_GEN_RCV_NO_BUFFER") ;
   default: break ;
   }
   break ;
  case 0x0202:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_GEN_DIRECTED_BYTES_XMIT") ;
   case 0x02: return ("OID_GEN_DIRECTED_FRAMES_XMIT") ;
   case 0x03: return ("OID_GEN_MULTICAST_BYTES_XMIT") ;
   case 0x04: return ("OID_GEN_MULTICAST_FRAMES_XMIT") ;
   case 0x05: return ("OID_GEN_BROADCAST_BYTES_XMIT") ;
   case 0x06: return ("OID_GEN_BROADCAST_FRAMES_XMIT") ;
   case 0x07: return ("OID_GEN_DIRECTED_BYTES_RCV") ;
   case 0x08: return ("OID_GEN_DIRECTED_FRAMES_RCV") ;
   case 0x09: return ("OID_GEN_MULTICAST_BYTES_RCV") ;
   case 0x0A: return ("OID_GEN_MULTICAST_FRAMES_RCV") ;
   case 0x0B: return ("OID_GEN_BROADCAST_BYTES_RCV") ;
   case 0x0C: return ("OID_GEN_BROADCAST_FRAMES_RCV") ;
   case 0x0D: return ("OID_GEN_RCV_CRC_ERROR") ;
   case 0x0E: return ("OID_GEN_TRANSMIT_QUEUE_LENGTH") ;
   default: break ;
   }
   break ;
  default:
   break ;
  }
  break ;
/*
 * 802.3 Objects (Ethernet)
 */
 case 0x01:
  switch ((Oid >> 8) & 0xFFFF)
  {
  case 0x0101:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_802_3_PERMANENT_ADDRESS") ;
   case 0x02: return ("OID_802_3_CURRENT_ADDRESS") ;
   case 0x03: return ("OID_802_3_MULTICAST_LIST") ;
   case 0x04: return ("OID_802_3_MAXIMUM_LIST_SIZE") ;
   case 0x05: return ("OID_802_3_MAC_OPTIONS") ;
   default: break ;
   }
   break ;
  case 0x0201:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_802_3_RCV_ERROR_ALIGNMENT") ;
   case 0x02: return ("OID_802_3_XMIT_ONE_COLLISION") ;
   case 0x03: return ("OID_802_3_XMIT_MORE_COLLISIONS") ;
   default: break ;
   }
   break ;
  case 0x0202:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_802_3_XMIT_DEFERRED") ;
   case 0x02: return ("OID_802_3_XMIT_MAX_COLLISIONS") ;
   case 0x03: return ("OID_802_3_RCV_OVERRUN") ;
   case 0x04: return ("OID_802_3_XMIT_UNDERRUN") ;
   case 0x05: return ("OID_802_3_XMIT_HEARTBEAT_FAILURE") ;
   case 0x06: return ("OID_802_3_XMIT_TIMES_CRS_LOST") ;
   case 0x07: return ("OID_802_3_XMIT_LATE_COLLISIONS") ;
   default: break ;
   }
   break ;
  default:
   break ;
  }
/*
 * 802.5 Objects (Token-Ring)
 */
 case 0x02:
  switch ((Oid >> 8) & 0xFFFF)
  {
  case 0x0101:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_802_5_PERMANENT_ADDRESS") ;
   case 0x02: return ("OID_802_5_CURRENT_ADDRESS") ;
   case 0x03: return ("OID_802_5_CURRENT_FUNCTIONAL") ;
   case 0x04: return ("OID_802_5_CURRENT_GROUP") ;
   case 0x05: return ("OID_802_5_LAST_OPEN_STATUS") ;
   case 0x06: return ("OID_802_5_CURRENT_RING_STATUS") ;
   case 0x07: return ("OID_802_5_CURRENT_RING_STATE") ;
   default: break ;
   }
   break ;
  case 0x0201:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_802_5_LINE_ERRORS") ;
   case 0x02: return ("OID_802_5_LOST_FRAMES") ;
   default: break ;
   }
   break ;
  case 0x0202:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_802_5_BURST_ERRORS") ;
   case 0x02: return ("OID_802_5_AC_ERRORS") ;
   case 0x03: return ("OID_802_5_ABORT_DELIMETERS") ;
   case 0x04: return ("OID_802_5_FRAME_COPIED_ERRORS") ;
   case 0x05: return ("OID_802_5_FREQUENCY_ERRORS") ;
   case 0x06: return ("OID_802_5_TOKEN_ERRORS") ;
   case 0x07: return ("OID_802_5_INTERNAL_ERRORS") ;
   default: break ;
   }
   break ;
  default:
   break ;
  }
  break ;
/*
 * FDDI Objects
 */
 case 0x03:
  switch ((Oid >> 8) & 0xFFFF)
  {
  case 0x0101:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_FDDI_LONG_PERMANENT_ADDR") ;
   case 0x02: return ("OID_FDDI_LONG_CURRENT_ADDR") ;
   case 0x03: return ("OID_FDDI_LONG_MULTICAST_LIST") ;
   case 0x04: return ("OID_FDDI_LONG_MAX_LIST_SIZE") ;
   case 0x05: return ("OID_FDDI_SHORT_PERMANENT_ADDR") ;
   case 0x06: return ("OID_FDDI_SHORT_CURRENT_ADDR") ;
   case 0x07: return ("OID_FDDI_SHORT_MULTICAST_LIST") ;
   case 0x08: return ("OID_FDDI_SHORT_MAX_LIST_SIZE") ;
   default: break ;
   }
   break ;
  case 0x0201:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_FDDI_ATTACHMENT_TYPE") ;
   case 0x02: return ("OID_FDDI_UPSTREAM_NODE_LONG") ;
   case 0x03: return ("OID_FDDI_DOWNSTREAM_NODE_LONG") ;
   case 0x04: return ("OID_FDDI_FRAME_ERRORS") ;
   case 0x05: return ("OID_FDDI_FRAMES_LOST") ;
   case 0x06: return ("OID_FDDI_RING_MGT_STATE") ;
   case 0x07: return ("OID_FDDI_LCT_FAILURES") ;
   case 0x08: return ("OID_FDDI_LEM_REJECTS") ;
   case 0x09: return ("OID_FDDI_LCONNECTION_STATE") ;
   default: break ;
   }
   break ;
  case 0x0302:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_FDDI_SMT_STATION_ID") ;
   case 0x02: return ("OID_FDDI_SMT_OP_VERSION_ID") ;
   case 0x03: return ("OID_FDDI_SMT_HI_VERSION_ID") ;
   case 0x04: return ("OID_FDDI_SMT_LO_VERSION_ID") ;
   case 0x05: return ("OID_FDDI_SMT_MANUFACTURER_DATA") ;
   case 0x06: return ("OID_FDDI_SMT_USER_DATA") ;
   case 0x07: return ("OID_FDDI_SMT_MIB_VERSION_ID") ;
   case 0x08: return ("OID_FDDI_SMT_MAC_CT") ;
   case 0x09: return ("OID_FDDI_SMT_NON_MASTER_CT") ;
   case 0x0A: return ("OID_FDDI_SMT_MASTER_CT") ;
   case 0x0B: return ("OID_FDDI_SMT_AVAILABLE_PATHS") ;
   case 0x0C: return ("OID_FDDI_SMT_CONFIG_CAPABILITIES") ;
   case 0x0D: return ("OID_FDDI_SMT_CONFIG_POLICY") ;
   case 0x0E: return ("OID_FDDI_SMT_CONNECTION_POLICY") ;
   case 0x0F: return ("OID_FDDI_SMT_T_NOTIFY") ;
   case 0x10: return ("OID_FDDI_SMT_STAT_RPT_POLICY") ;
   case 0x11: return ("OID_FDDI_SMT_TRACE_MAX_EXPIRATION") ;
   case 0x12: return ("OID_FDDI_SMT_PORT_INDEXES") ;
   case 0x13: return ("OID_FDDI_SMT_MAC_INDEXES") ;
   case 0x14: return ("OID_FDDI_SMT_BYPASS_PRESENT") ;
   case 0x15: return ("OID_FDDI_SMT_ECM_STATE") ;
   case 0x16: return ("OID_FDDI_SMT_CF_STATE") ;
   case 0x17: return ("OID_FDDI_SMT_HOLD_STATE") ;
   case 0x18: return ("OID_FDDI_SMT_REMOTE_DISCONNECT_FLAG") ;
   case 0x19: return ("OID_FDDI_SMT_STATION_STATUS") ;
   case 0x1A: return ("OID_FDDI_SMT_PEER_WRAP_FLAG") ;
   case 0x1B: return ("OID_FDDI_SMT_MSG_TIME_STAMP") ;
   case 0x1C: return ("OID_FDDI_SMT_TRANSITION_TIME_STAMP") ;
   case 0x1D: return ("OID_FDDI_SMT_SET_COUNT") ;
   case 0x1E: return ("OID_FDDI_SMT_LAST_SET_STATION_ID") ;
   case 0x1F: return ("OID_FDDI_MAC_FRAME_STATUS_FUNCTIONS") ;
   case 0x20: return ("OID_FDDI_MAC_BRIDGE_FUNCTIONS") ;
   case 0x21: return ("OID_FDDI_MAC_T_MAX_CAPABILITY") ;
   case 0x22: return ("OID_FDDI_MAC_TVX_CAPABILITY") ;
   case 0x23: return ("OID_FDDI_MAC_AVAILABLE_PATHS") ;
   case 0x24: return ("OID_FDDI_MAC_CURRENT_PATH") ;
   case 0x25: return ("OID_FDDI_MAC_UPSTREAM_NBR") ;
   case 0x26: return ("OID_FDDI_MAC_DOWNSTREAM_NBR") ;
   case 0x27: return ("OID_FDDI_MAC_OLD_UPSTREAM_NBR") ;
   case 0x28: return ("OID_FDDI_MAC_OLD_DOWNSTREAM_NBR") ;
   case 0x29: return ("OID_FDDI_MAC_DUP_ADDRESS_TEST") ;
   case 0x2A: return ("OID_FDDI_MAC_REQUESTED_PATHS") ;
   case 0x2B: return ("OID_FDDI_MAC_DOWNSTREAM_PORT_TYPE") ;
   case 0x2C: return ("OID_FDDI_MAC_INDEX") ;
   case 0x2D: return ("OID_FDDI_MAC_SMT_ADDRESS") ;
   case 0x2E: return ("OID_FDDI_MAC_LONG_GRP_ADDRESS") ;
   case 0x2F: return ("OID_FDDI_MAC_SHORT_GRP_ADDRESS") ;
   case 0x30: return ("OID_FDDI_MAC_T_REQ") ;
   case 0x31: return ("OID_FDDI_MAC_T_NEG") ;
   case 0x32: return ("OID_FDDI_MAC_T_MAX") ;
   case 0x33: return ("OID_FDDI_MAC_TVX_VALUE") ;
   case 0x34: return ("OID_FDDI_MAC_T_PRI0") ;
   case 0x35: return ("OID_FDDI_MAC_T_PRI1") ;
   case 0x36: return ("OID_FDDI_MAC_T_PRI2") ;
   case 0x37: return ("OID_FDDI_MAC_T_PRI3") ;
   case 0x38: return ("OID_FDDI_MAC_T_PRI4") ;
   case 0x39: return ("OID_FDDI_MAC_T_PRI5") ;
   case 0x3A: return ("OID_FDDI_MAC_T_PRI6") ;
   case 0x3B: return ("OID_FDDI_MAC_FRAME_CT") ;
   case 0x3C: return ("OID_FDDI_MAC_COPIED_CT") ;
   case 0x3D: return ("OID_FDDI_MAC_TRANSMIT_CT") ;
   case 0x3E: return ("OID_FDDI_MAC_TOKEN_CT") ;
   case 0x3F: return ("OID_FDDI_MAC_ERROR_CT") ;
   case 0x40: return ("OID_FDDI_MAC_LOST_CT") ;
   case 0x41: return ("OID_FDDI_MAC_TVX_EXPIRED_CT") ;
   case 0x42: return ("OID_FDDI_MAC_NOT_COPIED_CT") ;
   case 0x43: return ("OID_FDDI_MAC_LATE_CT") ;
   case 0x44: return ("OID_FDDI_MAC_RING_OP_CT") ;
   case 0x45: return ("OID_FDDI_MAC_FRAME_ERROR_THRESHOLD") ;
   case 0x46: return ("OID_FDDI_MAC_FRAME_ERROR_RATIO") ;
   case 0x47: return ("OID_FDDI_MAC_NOT_COPIED_THRESHOLD") ;
   case 0x48: return ("OID_FDDI_MAC_NOT_COPIED_RATIO") ;
   case 0x49: return ("OID_FDDI_MAC_RMT_STATE") ;
   case 0x4A: return ("OID_FDDI_MAC_DA_FLAG") ;
   case 0x4B: return ("OID_FDDI_MAC_UNDA_FLAG") ;
   case 0x4C: return ("OID_FDDI_MAC_FRAME_ERROR_FLAG") ;
   case 0x4D: return ("OID_FDDI_MAC_NOT_COPIED_FLAG") ;
   case 0x4E: return ("OID_FDDI_MAC_MA_UNITDATA_AVAILABLE") ;
   case 0x4F: return ("OID_FDDI_MAC_HARDWARE_PRESENT") ;
   case 0x50: return ("OID_FDDI_MAC_MA_UNITDATA_ENABLE") ;
   case 0x51: return ("OID_FDDI_PATH_INDEX") ;
   case 0x52: return ("OID_FDDI_PATH_RING_LATENCY") ;
   case 0x53: return ("OID_FDDI_PATH_TRACE_STATUS") ;
   case 0x54: return ("OID_FDDI_PATH_SBA_PAYLOAD") ;
   case 0x55: return ("OID_FDDI_PATH_SBA_OVERHEAD") ;
   case 0x56: return ("OID_FDDI_PATH_CONFIGURATION") ;
   case 0x57: return ("OID_FDDI_PATH_T_R_MODE") ;
   case 0x58: return ("OID_FDDI_PATH_SBA_AVAILABLE") ;
   case 0x59: return ("OID_FDDI_PATH_TVX_LOWER_BOUND") ;
   case 0x5A: return ("OID_FDDI_PATH_T_MAX_LOWER_BOUND") ;
   case 0x5B: return ("OID_FDDI_PATH_MAX_T_REQ") ;
   case 0x5C: return ("OID_FDDI_PORT_MY_TYPE") ;
   case 0x5D: return ("OID_FDDI_PORT_NEIGHBOR_TYPE") ;
   case 0x5E: return ("OID_FDDI_PORT_CONNECTION_POLICIES") ;
   case 0x5F: return ("OID_FDDI_PORT_MAC_INDICATED") ;
   case 0x60: return ("OID_FDDI_PORT_CURRENT_PATH") ;
   case 0x61: return ("OID_FDDI_PORT_REQUESTED_PATHS") ;
   case 0x62: return ("OID_FDDI_PORT_MAC_PLACEMENT") ;
   case 0x63: return ("OID_FDDI_PORT_AVAILABLE_PATHS") ;
   case 0x64: return ("OID_FDDI_PORT_MAC_LOOP_TIME") ;
   case 0x65: return ("OID_FDDI_PORT_PMD_CLASS") ;
   case 0x66: return ("OID_FDDI_PORT_CONNECTION_CAPABILITIES") ;
   case 0x67: return ("OID_FDDI_PORT_INDEX") ;
   case 0x68: return ("OID_FDDI_PORT_MAINT_LS") ;
   case 0x69: return ("OID_FDDI_PORT_BS_FLAG") ;
   case 0x6A: return ("OID_FDDI_PORT_PC_LS") ;
   case 0x6B: return ("OID_FDDI_PORT_EB_ERROR_CT") ;
   case 0x6C: return ("OID_FDDI_PORT_LCT_FAIL_CT") ;
   case 0x6D: return ("OID_FDDI_PORT_LER_ESTIMATE") ;
   case 0x6E: return ("OID_FDDI_PORT_LEM_REJECT_CT") ;
   case 0x6F: return ("OID_FDDI_PORT_LEM_CT") ;
   case 0x70: return ("OID_FDDI_PORT_LER_CUTOFF") ;
   case 0x71: return ("OID_FDDI_PORT_LER_ALARM") ;
   case 0x72: return ("OID_FDDI_PORT_CONNNECT_STATE") ;
   case 0x73: return ("OID_FDDI_PORT_PCM_STATE") ;
   case 0x74: return ("OID_FDDI_PORT_PC_WITHHOLD") ;
   case 0x75: return ("OID_FDDI_PORT_LER_FLAG") ;
   case 0x76: return ("OID_FDDI_PORT_HARDWARE_PRESENT") ;
   case 0x77: return ("OID_FDDI_SMT_STATION_ACTION") ;
   case 0x78: return ("OID_FDDI_PORT_ACTION") ;
   case 0x79: return ("OID_FDDI_IF_DESCR") ;
   case 0x7A: return ("OID_FDDI_IF_TYPE") ;
   case 0x7B: return ("OID_FDDI_IF_MTU") ;
   case 0x7C: return ("OID_FDDI_IF_SPEED") ;
   case 0x7D: return ("OID_FDDI_IF_PHYS_ADDRESS") ;
   case 0x7E: return ("OID_FDDI_IF_ADMIN_STATUS") ;
   case 0x7F: return ("OID_FDDI_IF_OPER_STATUS") ;
   case 0x80: return ("OID_FDDI_IF_LAST_CHANGE") ;
   case 0x81: return ("OID_FDDI_IF_IN_OCTETS") ;
   case 0x82: return ("OID_FDDI_IF_IN_UCAST_PKTS") ;
   case 0x83: return ("OID_FDDI_IF_IN_NUCAST_PKTS") ;
   case 0x84: return ("OID_FDDI_IF_IN_DISCARDS") ;
   case 0x85: return ("OID_FDDI_IF_IN_ERRORS") ;
   case 0x86: return ("OID_FDDI_IF_IN_UNKNOWN_PROTOS") ;
   case 0x87: return ("OID_FDDI_IF_OUT_OCTETS") ;
   case 0x88: return ("OID_FDDI_IF_OUT_UCAST_PKTS") ;
   case 0x89: return ("OID_FDDI_IF_OUT_NUCAST_PKTS") ;
   case 0x8A: return ("OID_FDDI_IF_OUT_DISCARDS") ;
   case 0x8B: return ("OID_FDDI_IF_OUT_ERRORS") ;
   case 0x8C: return ("OID_FDDI_IF_OUT_QLEN") ;
   case 0x8D: return ("OID_FDDI_IF_SPECIFIC") ;
   default: break ;
   }
   break ;
  default:
   break ;
  }
  break ;
/*
 * WAN objects
 */
 case 0x04:
  switch ((Oid >> 8) & 0xFFFF)
  {
  case 0x0101:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_WAN_PERMANENT_ADDRESS") ;
   case 0x02: return ("OID_WAN_CURRENT_ADDRESS") ;
   case 0x03: return ("OID_WAN_QUALITY_OF_SERVICE") ;
   case 0x04: return ("OID_WAN_PROTOCOL_TYPE") ;
   case 0x05: return ("OID_WAN_MEDIUM_SUBTYPE") ;
   case 0x06: return ("OID_WAN_HEADER_FORMAT") ;
   case 0x07: return ("OID_WAN_GET_INFO") ;
   case 0x08: return ("OID_WAN_SET_LINK_INFO") ;
   case 0x09: return ("OID_WAN_GET_LINK_INFO") ;
   case 0x0A: return ("OID_WAN_LINE_COUNT") ;
   default: break ;
   }
   break ;
  case 0x0102:
   switch (Oid & 0xFF)
   {
   case 0x0A: return ("OID_WAN_GET_BRIDGE_INFO") ;
   case 0x0B: return ("OID_WAN_SET_BRIDGE_INFO") ;
   case 0x0C: return ("OID_WAN_GET_COMP_INFO") ;
   case 0x0D: return ("OID_WAN_SET_COMP_INFO") ;
   case 0x0E: return ("OID_WAN_GET_STATS_INFO") ;
   default: break ;
   }
   break ;
  default:
   break ;
  }
  break ;
/*
 * LocalTalk objects
 */
 case 0x05:
  switch ((Oid >> 8) & 0xFFFF)
  {
  case 0x0101:
   switch (Oid & 0xFF)
   {
   case 0x02: return ("OID_LTALK_CURRENT_NODE_ID") ;
   default: break ;
   }
   break ;
  case 0x0201:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_LTALK_IN_BROADCASTS") ;
   case 0x02: return ("OID_LTALK_IN_LENGTH_ERRORS") ;
   default: break ;
   }
   break ;
  case 0x0202:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_LTALK_OUT_NO_HANDLERS") ;
   case 0x02: return ("OID_LTALK_COLLISIONS") ;
   case 0x03: return ("OID_LTALK_DEFERS") ;
   case 0x04: return ("OID_LTALK_NO_DATA_ERRORS") ;
   case 0x05: return ("OID_LTALK_RANDOM_CTS_ERRORS") ;
   case 0x06: return ("OID_LTALK_FCS_ERRORS") ;
   default: break ;
   }
   break ;
  default:
   break ;
  }
  break ;
/*
 * Arcnet objects
 */
 case 0x06:
  switch ((Oid >> 8) & 0xFFFF)
  {
  case 0x0101:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_ARCNET_PERMANENT_ADDRESS") ;
   case 0x02: return ("OID_ARCNET_CURRENT_ADDRESS") ;
   default: break ;
   }
   break ;
  case 0x0202:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_ARCNET_RECONFIGURATIONS") ;
   default: break ;
   }
   break ;
  default:
   break ;
  }
  break ;
/*
 * TAPI objects
 */
 case 0x07:
  switch ((Oid >> 8) & 0xFFFF)
  {
  case 0x0301:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_TAPI_ACCEPT") ;
   case 0x02: return ("OID_TAPI_ANSWER") ;
   case 0x03: return ("OID_TAPI_CLOSE") ;
   case 0x04: return ("OID_TAPI_CLOSE_CALL") ;
   case 0x05: return ("OID_TAPI_CONDITIONAL_MEDIA_DETECTION") ;
   case 0x06: return ("OID_TAPI_CONFIG_DIALOG (NT only)") ;
   case 0x07: return ("OID_TAPI_DEV_SPECIFIC") ;
   case 0x08: return ("OID_TAPI_DIAL") ;
   case 0x09: return ("OID_TAPI_DROP") ;
   case 0x0A: return ("OID_TAPI_GET_ADDRESS_CAPS") ;
   case 0x0B: return ("OID_TAPI_GET_ADDRESS_ID") ;
   case 0x0C: return ("OID_TAPI_GET_ADDRESS_STATUS") ;
   case 0x0D: return ("OID_TAPI_GET_CALL_ADDRESS_ID") ;
   case 0x0E: return ("OID_TAPI_GET_CALL_INFO") ;
   case 0x0F: return ("OID_TAPI_GET_CALL_STATUS") ;
   case 0x10: return ("OID_TAPI_GET_DEV_CAPS") ;
   case 0x11: return ("OID_TAPI_GET_DEV_CONFIG") ;
   case 0x12: return ("OID_TAPI_GET_EXTENSION_ID") ;
   case 0x13: return ("OID_TAPI_GET_ID") ;
   case 0x14: return ("OID_TAPI_GET_LINE_DEV_STATUS") ;
   case 0x15: return ("OID_TAPI_MAKE_CALL") ;
   case 0x16: return ("OID_TAPI_NEGOTIATE_EXT_VERSION") ;
   case 0x17: return ("OID_TAPI_OPEN") ;
   case 0x18: return ("OID_TAPI_PROVIDER_INITIALIZE") ;
   case 0x19: return ("OID_TAPI_PROVIDER_SHUTDOWN") ;
   case 0x1A: return ("OID_TAPI_SECURE_CALL") ;
   case 0x1B: return ("OID_TAPI_SELECT_EXT_VERSION") ;
   case 0x1C: return ("OID_TAPI_SEND_USER_USER_INFO") ;
   case 0x1D: return ("OID_TAPI_SET_APP_SPECIFIC") ;
   case 0x1E: return ("OID_TAPI_SET_CALL_PARAMS") ;
   case 0x1F: return ("OID_TAPI_SET_DEFAULT_MEDIA_DETECTION") ;
   case 0x20: return ("OID_TAPI_SET_DEV_CONFIG") ;
   case 0x21: return ("OID_TAPI_SET_MEDIA_MODE") ;
   case 0x22: return ("OID_TAPI_SET_STATUS_MESSAGES") ;
/*
 * The following has been added for Windows 95
 * to complete 1.4 TSPI_line and TSPI_phone support
 */
   case 0x23: return ("OID_TAPI_ADD_TO_CONFERENCE (W95/1.4)") ;
   case 0x24: return ("OID_TAPI_BLIND_TRANSFER (W95/1.4)") ;
   case 0x25: return ("OID_TAPI_COMPLETE_CALL (W95/1.4)") ;
   case 0x26: return ("OID_TAPI_COMPLETE_TRANSFER (W95/1.4)") ;
   case 0x27: return ("OID_TAPI_DEV_SPECIFIC_FEATURE (W95/1.4)") ;
   case 0x28: return ("OID_TAPI_FORWARD (W95/1.4)") ;
   case 0x29: return ("OID_TAPI_GATHER_DIGITS (W95/1.4)") ;
   case 0x2A: return ("OID_TAPI_GENERATE_DIGITS (W95/1.4)") ;
   case 0x2B: return ("OID_TAPI_GENERATE_TONE (W95/1.4)") ;
   case 0x2C: return ("OID_TAPI_HOLD (W95/1.4)") ;
   case 0x2D: return ("OID_TAPI_MONITOR_DIGITS (W95/1.4)") ;
   case 0x2E: return ("OID_TAPI_MONITOR_MEDIA (W95/1.4)") ;
   case 0x2F: return ("OID_TAPI_MONITOR_TONES (W95/1.4)") ;
   case 0x30: return ("OID_TAPI_PARK (W95/1.4)") ;
   case 0x31: return ("OID_TAPI_PICKUP (W95/1.4)") ;
   case 0x32: return ("OID_TAPI_PREPARE_ADD_TO_CONFERENCE (W95/1.4)") ;
   case 0x33: return ("OID_TAPI_REDIRECT (W95/1.4)") ;
   case 0x34: return ("OID_TAPI_REMOVE_FROM_CONFERENCE (W95/1.4)") ;
   case 0x35: return ("OID_TAPI_SET_MEDIA_CONTROL (W95/1.4)") ;
   case 0x36: return ("OID_TAPI_SET_TERMINAL (W95/1.4)") ;
   case 0x37: return ("OID_TAPI_SETUP_CONFERENCE (W95/1.4)") ;
   case 0x38: return ("OID_TAPI_SETUP_TRANSFER (W95/1.4)") ;
   case 0x39: return ("OID_TAPI_SWAP_HOLD (W95/1.4)") ;
   case 0x3A: return ("OID_TAPI_UNCOMPLETE_CALL (W95/1.4)") ;
   case 0x3B: return ("OID_TAPI_UNHOLD (W95/1.4)") ;
   case 0x3C: return ("OID_TAPI_UNPARK (W95/1.4)") ;
   case 0x3D: return ("OID_TAPI_RELEASE_USER_USER_INFO (W95/1.4)") ;
   case 0x3E: return ("OID_TAPI_PROVIDER_ENUM_DEVICES (W95/1.4)") ;
   case 0x3F: return ("OID_TAPI_LINE_NEGOTIATE_TSPI_VERSION (W95/1.4)") ;
   case 0x81: return ("OID_TAPI_PHONE_DEV_SPECIFIC (W95/1.4)") ;
   case 0x82: return ("OID_TAPI_PHONE_GET_BUTTON_INFO (W95/1.4)") ;
   case 0x83: return ("OID_TAPI_PHONE_GET_DATA (W95/1.4)") ;
   case 0x84: return ("OID_TAPI_PHONE_GET_DEV_CAPS (W95/1.4)") ;
   case 0x85: return ("OID_TAPI_PHONE_GET_DISPLAY (W95/1.4)") ;
   case 0x86: return ("OID_TAPI_PHONE_GET_EXTENSION_ID (W95/1.4)") ;
   case 0x87: return ("OID_TAPI_PHONE_GET_GAIN (W95/1.4)") ;
   case 0x88: return ("OID_TAPI_PHONE_GET_HOOK_SWITCH (W95/1.4)") ;
   case 0x89: return ("OID_TAPI_PHONE_GET_ID (W95/1.4)") ;
   case 0x8A: return ("OID_TAPI_PHONE_GET_LAMP (W95/1.4)") ;
   case 0x8B: return ("OID_TAPI_PHONE_GET_RING (W95/1.4)") ;
   case 0x8C: return ("OID_TAPI_PHONE_GET_STATUS (W95/1.4)") ;
   case 0x8D: return ("OID_TAPI_PHONE_GET_VOLUME (W95/1.4)") ;
   case 0x8E: return ("OID_TAPI_PHONE_NEGOTIATE_EXT_VERSION (W95/1.4)") ;
   case 0x8F: return ("OID_TAPI_PHONE_NEGOTIATE_TSPI_VERSION (W95/1.4)") ;
   case 0x90: return ("OID_TAPI_PHONE_OPEN (W95/1.4)") ;
   case 0x91: return ("OID_TAPI_PHONE_SELECT_EXT_VERSION (W95/1.4)") ;
   case 0x92: return ("OID_TAPI_PHONE_SET_BUTTON_INFO (W95/1.4)") ;
   case 0x93: return ("OID_TAPI_PHONE_SET_DATA (W95/1.4)") ;
   case 0x94: return ("OID_TAPI_PHONE_SET_DISPLAY (W95/1.4)") ;
   case 0x95: return ("OID_TAPI_PHONE_SET_GAIN (W95/1.4)") ;
   case 0x96: return ("OID_TAPI_PHONE_SET_HOOK_SWITCH (W95/1.4)") ;
   case 0x97: return ("OID_TAPI_PHONE_SET_LAMP (W95/1.4)") ;
   case 0x98: return ("OID_TAPI_PHONE_SET_RING (W95/1.4)") ;
   case 0x99: return ("OID_TAPI_PHONE_SET_STATUS_MESSAGES (W95/1.4)") ;
   case 0x9A: return ("OID_TAPI_PHONE_SET_VOLUME (W95/1.4)") ;
   case 0x9B: return ("OID_TAPI_PHONE_CLOSE (W95/1.4)") ;
   default: break ;
   }
   break ;
  default:
   break ;
  }
  break ;
/*
 * ATM Connection Oriented Ndis
 */
 case 0x08:
  switch ((Oid >> 8) & 0xFFFF)
  {
  case 0x0101:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_ATM_SUPPORTED_VC_RATES") ;
   case 0x02: return ("OID_ATM_SUPPORTED_QOS") ;
   case 0x03: return ("OID_ATM_SUPPORTED_AAL_TYPES") ;
   case 0x04: return ("OID_ATM_GET_TIME_CAPS") ;
   case 0x05: return ("OID_ATM_HW_CURRENT_ADDRESS") ;
   case 0x06: return ("OID_ATM_GET_NETCARD_TIME") ;
   case 0x08: return ("OID_ATM_GET_HARDWARE_LINE_STATE") ;
   case 0x09: return ("OID_ATM_ALIGNMENT_REQUIRED") ;
   case 0x0A: return ("OID_ATM_SIGNALING_VPIVCI") ;
   case 0x0B: return ("OID_ATM_ASSIGNED_VPI") ;
   case 0x0C: return ("OID_ATM_MAX_ACTIVE_VCS") ;
   case 0x0D: return ("OID_ATM_MAX_ACTIVE_VCI_BITS") ;
   case 0x0E: return ("OID_ATM_MAX_ACTIVE_VPI_BITS") ;
   case 0x0F: return ("OID_ATM_ACQUIRE_ACCESS_NET_RESOURCES") ;
   case 0x10: return ("OID_ATM_RELEASE_ACCESS_NET_RESOURCES") ;
   case 0x11: return ("OID_ATM_ILMI_VPIVCI") ;
   case 0x12: return ("OID_ATM_DIGITAL_BROADCAST_VPIVCI") ;
   default: break ;
   }
   break ;
  case 0x0201:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_ATM_GET_RESERVED_VC_LIST") ;
   case 0x02: return ("OID_ATM_GET_SPECIFIC_VC_STATS") ;
   case 0x03: return ("OID_ATM_GET_GENERAL_VC_STATS") ;
   case 0x04: return ("OID_ATM_NETCARD_LOAD") ;
   case 0x05: return ("OID_ATM_SET_REQUIRED_STATISTICS") ;
   default: break ;
   }
   break ;
  default:
   break ;
  }
  break ;
/*
 * PCCA (Wireless) object
 */
 case 0x09:
  switch ((Oid >> 8) & 0xFFFF)
  {
  case 0x0101:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_WW_GEN_NETWORK_TYPES_SUPPORTED") ;
   case 0x02: return ("OID_WW_GEN_NETWORK_TYPE_IN_USE") ;
   case 0x03: return ("OID_WW_GEN_HEADER_FORMATS_SUPPORTED") ;
   case 0x04: return ("OID_WW_GEN_HEADER_FORMAT_IN_USE") ;
   case 0x05: return ("OID_WW_GEN_INDICATION_REQUEST") ;
   case 0x06: return ("OID_WW_GEN_DEVICE_INFO") ;
   case 0x07: return ("OID_WW_GEN_OPERATION_MODE") ;
   case 0x08: return ("OID_WW_GEN_LOCK_STATUS") ;
   case 0x09: return ("OID_WW_GEN_DISABLE_TRANSMITTER") ;
   case 0x0A: return ("OID_WW_GEN_NETWORK_ID") ;
   case 0x0B: return ("OID_WW_GEN_PERMANENT_ADDRESS") ;
   case 0x0C: return ("OID_WW_GEN_CURRENT_ADDRESS") ;
   case 0x0D: return ("OID_WW_GEN_SUSPEND_DRIVER") ;
   case 0x0E: return ("OID_WW_GEN_BASESTATION_ID") ;
   case 0x0F: return ("OID_WW_GEN_CHANNEL_ID") ;
   case 0x10: return ("OID_WW_GEN_ENCRYPTION_SUPPORTED") ;
   case 0x11: return ("OID_WW_GEN_ENCRYPTION_IN_USE") ;
   case 0x12: return ("OID_WW_GEN_ENCRYPTION_STATE") ;
   case 0x13: return ("OID_WW_GEN_CHANNEL_QUALITY") ;
   case 0x14: return ("OID_WW_GEN_REGISTRATION_STATUS") ;
   case 0x15: return ("OID_WW_GEN_RADIO_LINK_SPEED") ;
   case 0x16: return ("OID_WW_GEN_LATENCY") ;
   case 0x17: return ("OID_WW_GEN_BATTERY_LEVEL") ;
   case 0x18: return ("OID_WW_GEN_EXTERNAL_POWER") ;
   default: break ;
   }
   break ;
/*
 * Network Dependent OIDs - Mobitex:
 */
  case 0x0501:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_WW_MBX_SUBADDR") ;
   case 0x03: return ("OID_WW_MBX_FLEXLIST") ;
   case 0x04: return ("OID_WW_MBX_GROUPLIST") ;
   case 0x05: return ("OID_WW_MBX_TRAFFIC_AREA") ;
   case 0x06: return ("OID_WW_MBX_LIVE_DIE") ;
   case 0x07: return ("OID_WW_MBX_TEMP_DEFAULTLIST") ;
   default: break ;
   }
   break ;
/*
 * Network Dependent OIDs - Pinpoint:
 */
  case 0x0901:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_WW_PIN_LOC_AUTHORIZE") ;
   case 0x02: return ("OID_WW_PIN_LAST_LOCATION") ;
   case 0x03: return ("OID_WW_PIN_LOC_FIX") ;
   default: break ;
   }
   break ;
/*
 * Network Dependent - CDPD:
 */
  case 0x0D01:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_WW_CDPD_SPNI") ;
   case 0x02: return ("OID_WW_CDPD_WASI") ;
   case 0x03: return ("OID_WW_CDPD_AREA_COLOR") ;
   case 0x04: return ("OID_WW_CDPD_TX_POWER_LEVEL") ;
   case 0x05: return ("OID_WW_CDPD_EID") ;
   case 0x06: return ("OID_WW_CDPD_HEADER_COMPRESSION") ;
   case 0x07: return ("OID_WW_CDPD_DATA_COMPRESSION") ;
   case 0x08: return ("OID_WW_CDPD_CHANNEL_SELECT") ;
   case 0x09: return ("OID_WW_CDPD_CHANNEL_STATE") ;
   case 0x0A: return ("OID_WW_CDPD_NEI") ;
   case 0x0B: return ("OID_WW_CDPD_NEI_STATE") ;
   case 0x0C: return ("OID_WW_CDPD_SERVICE_PROVIDER_IDENTIFIER") ;
   case 0x0D: return ("OID_WW_CDPD_SLEEP_MODE") ;
   case 0x0E: return ("OID_WW_CDPD_CIRCUIT_SWITCHED") ;
   case 0x0F: return ("OID_WW_CDPD_TEI") ;
   case 0x10: return ("OID_WW_CDPD_RSSI") ;
   default: break ;
   }
   break ;
/*
 * Network Dependent - Ardis:
 */
  case 0x1101:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_WW_ARD_SNDCP") ;
   case 0x02: return ("OID_WW_ARD_TMLY_MSG") ;
   case 0x03: return ("OID_WW_ARD_DATAGRAM") ;
   default: break ;
   }
   break ;
/*
 * Network Dependent - DataTac:
 */
  case 0x1501:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_WW_TAC_COMPRESSION") ;
   case 0x02: return ("OID_WW_TAC_SET_CONFIG") ;
   case 0x03: return ("OID_WW_TAC_GET_STATUS") ;
   case 0x04: return ("OID_WW_TAC_USER_HEADER") ;
   default: break ;
   }
   break ;
/*
 * Network Dependent - Metricom:
 */
  case 0x1901:
   switch (Oid & 0xFF)
   {
   case 0x01: return ("OID_WW_MET_FUNCTION") ;
   default: break ;
   }
   break ;
  default:
   break ;
  }
  break ;
/*
 * IRDA objects
 */
 case 0x0A:
  switch ((Oid >> 8) & 0xFFFF)
  {
  case 0x0101:
   switch (Oid & 0xFF)
   {
   case 0x00: return ("OID_IRDA_RECEIVING") ;
   case 0x01: return ("OID_IRDA_TURNAROUND_TIME") ;
   case 0x02: return ("OID_IRDA_SUPPORTED_SPEEDS") ;
   case 0x03: return ("OID_IRDA_LINK_SPEED") ;
   case 0x04: return ("OID_IRDA_MEDIA_BUSY") ;
   default: break ;
   }
   break ;
  case 0x0102:
   switch (Oid & 0xFF)
   {
   case 0x00: return ("OID_IRDA_EXTRA_RCV_BOFS") ;
   case 0x01: return ("OID_IRDA_RATE_SNIFF") ;
   case 0x02: return ("OID_IRDA_UNICAST_LIST") ;
   case 0x03: return ("OID_IRDA_MAX_UNICAST_LIST_SIZE") ;
   default: break ;
   }
   break ;
  default:
   break ;
  }
  break ;
/*
 * unknown_group !
 */
 default:
  break ;
 }
 sprintf (&name[0], "OID_0x%08X", Oid) ;
 return (&name[0]) ;
}
/*****************************************************************************/
void
oidFilter (FILE *out, char *msgText)
{
 char          *buffer, *outPtr, *ptr ;
 char           outputLine[MSG_FRAME_MAX_SIZE] ;
 dword oid;
 unsigned long  len ;
/*
 * scan string for OID_0x01234567_ constants
 */
 buffer = msgText ;
 outPtr = &outputLine[0] ;
 while ( (ptr = strchr (buffer, 'O')) != ((void*)0)  )
 {
  len = (ptr - buffer) + 1 ;
  memcpy (outPtr, buffer, len) ;
  buffer += len ;
  outPtr += len ;
  if ( !strncmp (ptr, "OID_0x", 6) && (ptr[14] == '_') )
  {
   oid = strtoul (&ptr[6], ((void*)0) , 16) ;
   buffer = &ptr[15] ;
   ptr = GetOidString (oid) ;
   strcpy (outPtr, ptr + 1) ;
   outPtr = strchr (outPtr, '\0') ;
  }
  else
  {
   *outPtr = '\0' ;
  }
 }
/*
 * check for any substitutions
 */
 if ( outPtr == &outputLine[0] )
 {
  fprintf (out, "%s\n", buffer) ;
 }
 else
 {
  if ( *buffer )
  {
   strcpy (outPtr, buffer) ;
  }
  fprintf (out, "%s\n", &outputLine[0]) ;
 }
}
