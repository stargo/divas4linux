
/*****************************************************************************
 *
 * (c) COPYRIGHT 2005-2008       Dialogic Corporation
 *
 * ALL RIGHTS RESERVED
 *
 * This software is the property of Dialogic Corporation and/or its
 * subsidiaries ("Dialogic"). This copyright notice may not be removed,
 * modified or obliterated without the prior written permission of
 * Dialogic.
 *
 * This software is a Trade Secret of Dialogic and may not be copied,
 * transmitted, provided to or otherwise made available to any company,
 * corporation or other person or entity without written permission of
 * Dialogic.
 *
 * No right, title, ownership or other interest in the software is hereby
 * granted or transferred. The information contained herein is subject
 * to change without notice and should not be construed as a commitment of
 * Dialogic.
 *
 *----------------------------------------------------------------------------*/
/*
  Requires the following OS dependent functions to be implemented:
  int fp->sysFileRead (OsFileHandle *fp, void *buffer, unsigned long count);
    Returns count on success
  int fp->sysFileSeek (OsFileHandle *fp, long offset, int origin);
    Returns -1 on error
  int fp->sysCardLoad (OsFileHandle *fp, long length, (void  *  *) anchor);
    Reads length bytes from the file and writes it to the adapter. The address
    where the block has written to is stored in *p_anchor.
    Returns 0 on success
  void dsp_download_reserve_space (OsFileHandle *fp, long length)
    If necessary reserves memory space to store one piece of download
*/
/*****************************************************************************/
#if defined(DIVA_OS_DSP_FILE) /* do compile as os independent standalone file */
#include "platform.h"
#include "di_defs.h"
#include "pc.h"
#include "di.h"
#include "io.h"
#include "dsp_defs.h"
#endif
/*****************************************************************************/
static char dsp_combifile_magic[DSP_COMBIFILE_FORMAT_IDENTIFICATION_SIZE] =
{
  'E', 'i', 'c', 'o', 'n', '.', 'D', 'i',
  'e', 'h', 'l', ' ', 'D', 'S', 'P', ' ',
  'D', 'o', 'w', 'n', 'l', 'o', 'a', 'd',
  ' ', 'C', 'o', 'm', 'b', 'i', 'f', 'i',
  'l', 'e', '\0','\0','\0','\0','\0','\0',
  '\0','\0','\0','\0','\0','\0','\0','\0'
};
static char dsp_file_magic[DSP_FILE_FORMAT_IDENTIFICATION_SIZE] =
{
  'E', 'i', 'c', 'o', 'n', '.', 'D', 'i',
  'e', 'h', 'l', ' ', 'D', 'S', 'P', ' ',
  'D', 'o', 'w', 'n', 'l', 'o', 'a', 'd',
  '\0','F', 'i', 'l', 'e','\0','\0','\0',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0','\0','\0','\0','\0','\0','\0'
};
/*****************************************************************************/
static int
dsp_read (OsFileHandle *fp, long size, void *buffer, long buffer_size)
{
 long chunk, ret ;
 for ( ; size > 0 ; size -= chunk )
 {
  chunk = ((size >= buffer_size) ? buffer_size : size) ;
  ret = fp->sysFileRead (fp, buffer, (unsigned long)chunk) ;
  if ( ret != chunk )
   return (size) ;
 }
 return (0) ;
}
/*****************************************************************************/
static int
dsp_skip (OsFileHandle *fp, long position)
{
 return (fp->sysFileSeek (fp, position, OS_SEEK_CUR) < 0) ;
}
/*****************************************************************************/
static int
dsp_rewind (OsFileHandle *fp)
{
 return (fp->sysFileSeek (fp, 0, OS_SEEK_SET) != 0) ;
}
/*****************************************************************************/
static void
dsp_log (OsFileHandle *fp, long length, int rewind)
{
 char buffer[256] ;
 long n ;
 if ( dsp_read (fp, length, &buffer[0], sizeof(buffer)) )
  return ;
 for ( n = 0 ; n < length ; ++n )
 {
  if ( buffer[n] < ' ' )
  {
   buffer[n] = '\0' ;
   break ;
  }
 }
 buffer[length] = '\0' ;
 DBG_REG(("Loading %s", &buffer[0]))
 if ( rewind )
 {
  fp->sysFileSeek (fp, -length, OS_SEEK_CUR) ;
 }
}
/*****************************************************************************/
static int
dsp_card_load_proc (OsFileHandle *fp, long length, void  *  *p_anchor)
{
 *p_anchor = NULL ;
 if ( length == 0 )
  return (0) ;
 return (-1) ;
}
static int
dsp_card_load_portable (OsFileHandle *fp, long length, dword  *p_anchor)
{
 int      err ;
 void  *p ;
 if ( length == 0 )
 {
  *p_anchor = 0 ;
  return (0) ;
 }
 p = NULL ;
 err = fp->sysCardLoad (fp, length, &p) ;
 *p_anchor = PtrToUlong(p) ;
 return (err) ;
}
#define dsp_load_section(section,length) \
{ if ( dsp_card_load_proc (fp, length, section) ) \
 { goto download_failed ; } \
}
#define dsp_load_section_portable(section,length) \
{ if ( dsp_card_load_portable (fp, length, section) ) \
 { goto download_failed ; } \
}
#define dsp_reserve(length) \
{ if ( (length) > 0 ) \
 { dsp_download_reserve_space (fp, (length)) ; \
} }
char *
dsp_read_file (OsFileHandle         *fp,
               word                     card_type_number,
               word                 *p_download_count,
               t_dsp_desc           *p_download_table,
               t_dsp_portable_desc  *p_portable_download_table)
/*
  if called with p_download_table == NULL
    Reserves as much memory as required for the download.
  else
    Reads and stores download to the adapter, verifies that count is big enough
  Returns NULL on success, else an error text.
  Fills in number of DSP downloads, 0 for none (e.g. PICCOLA).
*/
{
 int                              required ;
 long                             i, n, length ;
 word                             required_count = 0 ;
 word                             usage_mask_offset ;
 word                             usage_mask_byte ;
 t_dsp_desc                      *p_desc ;
 t_dsp_portable_desc             *p_portable_desc ;
 t_dsp_combifile_header           combi_hdr ;
 t_dsp_file_header                file_hdr ;
 t_dsp_combifile_directory_entry  directory[2] ;
 char                             buffer[16] ;
/*
 * check combifile header contents
 */
 dsp_rewind (fp) ;
 if ( dsp_read (fp, sizeof(combi_hdr), &combi_hdr, sizeof(combi_hdr)) )
  goto file_too_short ;
 for ( i = 0 ; i < DSP_COMBIFILE_FORMAT_IDENTIFICATION_SIZE ; ++i )
 {
  if ( combi_hdr.format_identification[i] != dsp_combifile_magic[i] )
   return ("invalid header format") ;
 }
 if ( combi_hdr.format_version_bcd != DSP_COMBIFILE_FORMAT_VERSION_BCD )
  return ("unsupported header version") ;
 if ( dsp_skip (fp, (long)(combi_hdr.header_size - sizeof(combi_hdr))) )
  goto file_too_short ;
/*
 * log file description string
 */
 dsp_log (fp, (long)combi_hdr.combifile_description_size, FALSE ) ;
/*
 * search corresponding file set description
 */
 for ( i = combi_hdr.directory_entries, n = sizeof(directory[0]) ; ; )
 {
  if ( dsp_read (fp, n, &directory[0], n) )
   goto file_too_short ;
  if ( directory[0].card_type_number == card_type_number )
  {
   usage_mask_offset = directory[0].file_set_number >> 3 ;
   usage_mask_byte = 1 << (directory[0].file_set_number & 0x7) ;
   if ( dsp_skip (fp, (long)(--i * n)) )
    goto file_too_short ;
   break ;
  }
  if ( --i <= 0 )
   return ("card not supported") ;
 }
/*
 * parse the bunch of dsp download files
 */
 for ( i = 0 ; i < combi_hdr.download_count ; ++i )
 {
  n = (long)combi_hdr.usage_mask_size ;
  if ( dsp_read (fp, n, &buffer[0], n) )
   goto file_too_short ;
  required = buffer[usage_mask_offset] & usage_mask_byte ;
  if ( dsp_read (fp, sizeof(file_hdr), &file_hdr, sizeof(file_hdr)) )
   goto file_too_short ;
  for ( n = 0 ; n < DSP_FILE_FORMAT_IDENTIFICATION_SIZE ; ++n )
  {
   if ( file_hdr.format_identification[n] != dsp_file_magic[n] )
    return ("invalid file header format") ;
  }
  if ( file_hdr.format_version_bcd != DSP_FILE_FORMAT_VERSION_BCD )
   return ("unsupported file header version") ;
  if ( (file_hdr.memory_block_count != 0)
    && (file_hdr.memory_block_count != DSP_MEMORY_BLOCK_COUNT) )
   return ("invalid memory block count") ;
  if ( file_hdr.segment_count > DSP_DOWNLOAD_MAX_SEGMENTS )
   return ("too many segments") ;
/*
 * load all required dsp download files
 */
  if ( required && (p_download_table != NULL) )
  {
   if ( required_count >= *p_download_count )
    return ("download table overflow") ;
   p_desc                      = &p_download_table[required_count] ;
   p_desc->download_id         = file_hdr.download_id ;
   p_desc->download_flags      = file_hdr.download_flags ;
   p_desc->required_processing_power =
                                 file_hdr.required_processing_power ;
   p_desc->interface_channel_count =
                                 file_hdr.interface_channel_count ;
   p_desc->excess_header_size  = (word)(file_hdr.header_size
                               - sizeof(t_dsp_file_header)) ;
   p_desc->memory_block_count  = file_hdr.memory_block_count ;
   p_desc->segment_count       = file_hdr.segment_count ;
   p_desc->symbol_count        = file_hdr.symbol_count ;
   p_desc->data_block_count_dm = file_hdr.data_block_count_dm ;
   p_desc->data_block_count_pm = file_hdr.data_block_count_pm ;
   dsp_load_section((void  *  *)(&(p_desc->p_excess_header_data)),
                    p_desc->excess_header_size)
/*
 * log file description string
 */
   length = file_hdr.download_description_size ;
   dsp_log (fp, length, TRUE) ;
   dsp_load_section((void  *  *)(&(p_desc->p_download_description)),
                    length)
   dsp_load_section((void  *  *)(&(p_desc->p_memory_block_table)),
                    file_hdr.memory_block_table_size)
   dsp_load_section((void  *  *)(&(p_desc->p_segment_table)),
                    file_hdr.segment_table_size)
   dsp_load_section((void  *  *)(&(p_desc->p_symbol_table)),
                    file_hdr.symbol_table_size)
   dsp_load_section((void  *  *)(&(p_desc->p_data_blocks_dm)),
                    file_hdr.total_data_size_dm)
   dsp_load_section((void  *  *)(&(p_desc->p_data_blocks_pm)),
                    file_hdr.total_data_size_pm)
   ++required_count ;
   continue ;
  }
  else if ( required && (p_portable_download_table != NULL) )
  {
   if ( required_count >= *p_download_count )
    return ("download table overflow") ;
   p_portable_desc                      = &p_portable_download_table[required_count] ;
   p_portable_desc->download_id         = file_hdr.download_id ;
   p_portable_desc->download_flags      = file_hdr.download_flags ;
   p_portable_desc->required_processing_power =
                                 file_hdr.required_processing_power ;
   p_portable_desc->interface_channel_count =
                                 file_hdr.interface_channel_count ;
   p_portable_desc->excess_header_size  = (word)(file_hdr.header_size
                               - sizeof(t_dsp_file_header)) ;
   p_portable_desc->memory_block_count  = file_hdr.memory_block_count ;
   p_portable_desc->segment_count       = file_hdr.segment_count ;
   p_portable_desc->symbol_count        = file_hdr.symbol_count ;
   p_portable_desc->data_block_count_dm = file_hdr.data_block_count_dm ;
   p_portable_desc->data_block_count_pm = file_hdr.data_block_count_pm ;
   dsp_load_section_portable(&(p_portable_desc->p_excess_header_data),
                             p_portable_desc->excess_header_size)
/*
 * log file description string
 */
   length = file_hdr.download_description_size ;
   dsp_log (fp, length, TRUE) ;
   dsp_load_section_portable(&(p_portable_desc->p_download_description),
                             length)
   dsp_load_section_portable(&(p_portable_desc->p_memory_block_table),
                             file_hdr.memory_block_table_size)
   dsp_load_section_portable(&(p_portable_desc->p_segment_table),
                             file_hdr.segment_table_size)
   dsp_load_section_portable(&(p_portable_desc->p_symbol_table),
                             file_hdr.symbol_table_size)
   dsp_load_section_portable(&(p_portable_desc->p_data_blocks_dm),
                             file_hdr.total_data_size_dm)
   dsp_load_section_portable(&(p_portable_desc->p_data_blocks_pm),
                             file_hdr.total_data_size_pm)
   ++required_count ;
   continue ;
  }
/*
 * count only the memory amount needed
 */
  if ( required )
  {
   dsp_reserve(file_hdr.header_size - sizeof(file_hdr))
   dsp_reserve(file_hdr.download_description_size)
   dsp_reserve(file_hdr.memory_block_table_size)
   dsp_reserve(file_hdr.segment_table_size)
   dsp_reserve(file_hdr.symbol_table_size)
   dsp_reserve(file_hdr.total_data_size_dm)
   dsp_reserve(file_hdr.total_data_size_pm)
         ++required_count ;
  }
  length = ((long)(file_hdr.header_size - sizeof(file_hdr)))
         + ((long)(file_hdr.download_description_size))
         + ((long)(file_hdr.memory_block_table_size))
         + ((long)(file_hdr.segment_table_size))
         + ((long)(file_hdr.symbol_table_size))
         + ((long)(file_hdr.total_data_size_dm))
         + ((long)(file_hdr.total_data_size_pm)) ;
  if ( dsp_skip (fp, length) )
   goto file_too_short ;
 }
 *p_download_count = required_count ;
 return (NULL) ;
/*
 * simplified error handling
 */
file_too_short:
 return ("file too short") ;
download_failed:
 return ("download failed") ;
}
/*---------------------------------------------------------------------------*/
