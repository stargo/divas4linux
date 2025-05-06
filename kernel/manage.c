
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
#include "pc.h"
#include "manage.h"
#include "man_defs.h"

#include "debuglib.h"
/*------------------------------------------------------------------*/
/* local function prototypes                                        */
/*------------------------------------------------------------------*/
static MAN_INFO *find_path  (MAN_INFO   *d, 
                             char       *path, 
                             PATH_CTXT  *pPCtxt,
                             byte       *pDLevel,
                             void       *Ipar
                            );
static int      dir_compare (MAN_INFO   *d, 
                             char       *b
                            );
static int      add_p       (MAN_INFO   *d, 
                             int (*add_ie)(byte *header, byte *value, void *context), 
                             void       *context, 
                             void       *Ipar, 
                             PATH_CTXT  *pPCtxt,
                             byte       DLevel,
                             char       *path,
                             byte       PathIsDir
                            );
static int      info_len    (byte       type, 
                             byte       max_len, 
                             void       *info
                            );
static int      ctxt_compare(PATH_CTXT *Ctxt1, PATH_CTXT *Ctxt2, int DLevel);

static int man_power10_table[] = { 1, 10, 100, 1000, 10000 };

/*------------------------------------------------------------------*/
/* Read Directory elements or single Information element            */
/*------------------------------------------------------------------*/
void man_read(MAN_INFO  *d, 
              char      *path,
              int (*add_ie)(byte *header, byte *value, void *context), 
              void      *context, 
              void      *Ipar, 
              PATH_CTXT *pPCtxt,
              byte      *pDLevel
             )
{
  char  mi_dir;
  int   count;

  mi_dir = 0;
  if (!*path)  /* root path is a directory */
  {
    mi_dir = 1;
  }
  else 
  {
    d = find_path(d, path, pPCtxt, pDLevel, Ipar);  /* get pointer to specified element */
    if (!d || !d->info) return;

    if (d->type==MI_DIR) /* if element is a directory, get all elements (set mi_dir) */
    {
      mi_dir = 1;
      if (d->flags &MI_CALL)    /* by a function call */
      {
        d = ((INFO_FUNC)d->info)((void *)0, &pPCtxt[*pDLevel], CMD_NEXTNODE, Ipar);
      }
      else  /* or by direct reading of the MAN_INFO structute */
      {
        d = d->info;      
      }
      if (!d) return; /* the MI_CALL function returns NULL if the entry should 
                         not be displayed! */
    }
  }

  /* In case of a directory convert ALL elements to information elements */
  if (mi_dir)
  {
    if (*path && !(*pDLevel)--) return;

    /* get all information elements found in this folder */
    for (;;) 
    {
      if (d->type==MI_NEXT_DIR)
      {
        pPCtxt[*pDLevel].node = d;
        if (!(*pDLevel)--) return;
        d = d->info;
      }
      else
      { 
        if (d->flags & MI_ENUM_MASK)  /* is an array element */
        {
          for (count=0; count < *(int *)d->context; count++)
          {
            pPCtxt[*pDLevel].fPara = IntToPtr(count);
            pPCtxt[*pDLevel].num   = ((d->flags & MI_ENUM_MASK) == MI_ENUM) ? count + 1 : count;
            pPCtxt[*pDLevel].node  = d;
            add_p(d, add_ie, context, Ipar, pPCtxt, *pDLevel, path, 1);
          }
          if (count < *(int *)d->context) break;
        } 
        else  /* is a single element */
        {
          pPCtxt[*pDLevel].fPara = d->context;
          pPCtxt[*pDLevel].num   = 0;
          pPCtxt[*pDLevel].node  = d;
          add_p(d, add_ie, context, Ipar, pPCtxt, *pDLevel, path, 1);
        }
        if (d->flags &MI_E) break;  /* all elements processed */
        d++;  /* next element */
      }
    }
  }
  else  /* get single information element */
  {
    add_p(d, add_ie, context, Ipar, pPCtxt, *pDLevel, path, 0);
  }
  return;
}

/*------------------------------------------------------------------*/
/* Write single Information element                                 */
/*------------------------------------------------------------------*/
int man_write(MAN_INFO  *d, 
              char      *path,
              void      *info, 
              void      *context, 
              void      *Ipar, 
              PATH_CTXT *pPCtxt,
              byte      *pDLevel
             )
{
  int       i_len;
  void      *w_info;
  int       *pwlock;
  
  d = find_path(d, path, pPCtxt, pDLevel, Ipar);  /* get pointer to specified element */
  if (!d || !d->info) return 0;
  if (!(d->attrib &MI_WRITE)) return 0;     /* not writeable */
  i_len = info_len(d->type, d->max_len, info);
  if ((unsigned int)i_len > d->max_len) return 0;  /* wrong size */
  
  /* check if variable is locked by another Id */
  if (d->flags &MI_CALL)
  {
    pwlock = ((INFO_FUNC)d->info)((void *)0, &pPCtxt[*pDLevel], CMD_LOCKVAR, Ipar);
  }
  else 
  {
    pwlock = &d->wlock;
  }
  
  /* get pointer to destination */
  if (d->flags &MI_CALL)
  {
    /* returns pointer to data that can be written.  Returns 0, if data was already written */
    w_info = ((INFO_FUNC)d->info)(info, &pPCtxt[*pDLevel], CMD_WRITEVAR, Ipar);
  }
  else
  {
    w_info = d->info;
  }
  if (w_info) 
  {
    memcpy(w_info, info,   i_len);
  }  
  return i_len;
}

/*------------------------------------------------------------------*/
/* Execute command                                                  */
/*------------------------------------------------------------------*/
void man_execute(MAN_INFO   *d, 
                 char       *path, 
                 void       *context, 
                 void       *Ipar, 
                 PATH_CTXT  *pPCtxt,
                 byte       *pDLevel
                )
{
  d = find_path(d, path, pPCtxt, pDLevel, Ipar);  /* get pointer to specified element */
  if (!d || !d->info) return;
  if (d->type != MI_EXECUTE) return;

  ((INFO_FUNC)d->info)((void *)0, &pPCtxt[*pDLevel], CMD_EXECUTE, Ipar);
}

/*------------------------------------------------------------------*/
/* write a management IE                                            */
/*------------------------------------------------------------------*/
static int add_p(MAN_INFO   *d, 
                 int (* add_ie)(byte *header, byte *value, void *context), 
                 void       *context, 
                 void       *Ipar,
                 PATH_CTXT  *pPCtxt,
                 byte       DLevel,
                 char       *path,
                 byte       PathIsDir
                )
{
  int           n_len;
  int           i_len;
  int           num_len = 0;
  int           num     = 0;
  int           p_len;
  int           indx;
  byte          c;
  void          *info;
  EVENT_ELE     *pEvent;
  int           *pwlock;
  byte  buffer[IBUF_SIZE];
  MI_XLOG_HDR   *pTData;

  /* get variable content and length */
  if (d->flags &MI_CALL) 
  {
    info   = ((INFO_FUNC)d->info)((void *)0, &pPCtxt[DLevel], CMD_READVAR, Ipar);
    pwlock = ((INFO_FUNC)d->info)((void *)0, &pPCtxt[DLevel], CMD_LOCKVAR, Ipar);
  }
  else 
  {
    if (d->type==MI_DIR)  info = "";
    else                  info = d->info;
    pwlock = &d->wlock;
  }
  if (!info) return(0);
  i_len = info_len(d->type, d->max_len, info);  /* variable's length */
  if (d->type == MI_TRACE) i_len = sizeof(MI_XLOG_HDR); /* just header, data makes no sense */
  p_len = strlen(path);    /* length of basic node name */
  num_len = 0;
  if (PathIsDir)  /* supplied path is a directory (node) */
  {
    n_len = strlen(d->name); /* length of element name added to path */
    if (d->flags & MI_ENUM_MASK)    /* get length if index number (array element) */
    {
      num = PtrToInt(pPCtxt[DLevel].fPara);
      if ((d->flags & MI_ENUM_MASK) == MI_ENUM)
        num++;
      num_len = 1;
      while ((num_len < (sizeof(man_power10_table) / sizeof(man_power10_table[0])))
       && (man_power10_table[num_len] <= num))
      {
        num_len++;
      }
    }
  }
  else            /* supplied path includes element name */
  {
    n_len = 0;
  }
  if (p_len+n_len+num_len+i_len+8 > IBUF_SIZE) return 0;  /* too long for my local buffer */
  
  /* Now mount this Management Information Element */
  buffer[0] = 0x7f;
  buffer[2] = 0x80;
  buffer[3] = d->type;
  buffer[4] = d->attrib;
  buffer[5] = pwlock ? ((d->attrib &MI_WRITE) ? (*pwlock ? MI_LOCKED : 0) : 0) : 0;
  if (!*path) pPCtxt[DIR_LEVELS-1].node = d; /* else ctxt_compare would fail */
  pEvent = 0 /* Ipar->IdQ */;     /* get Id related event queue head */
  for (; pEvent; pEvent = pEvent->NextIdLink) /* check event notification status */
  {
    if (ctxt_compare(pPCtxt, pEvent->DCtxt, DLevel))
    {
      if (pEvent->status &MI_EVENT_ON)
      {
        buffer[5] |= MI_EVENT_ON;
      }  
      if (pwlock                      &&
          (pEvent->status &MI_LOCKED) &&
          (*pwlock == pEvent->dInst)     )
      {
        buffer[5] |= MI_PROTECTED;
        buffer[5] &= ~MI_LOCKED;
      }  
    }
  }
  
  buffer[6] = d->max_len; /* return max length for variable length variables */
  memcpy(&buffer[8],path,p_len);
  if (p_len && n_len+num_len)  /* add no separator to root */
  {
    memcpy(&buffer[8+p_len],"\\",1);
    p_len++;
  }
  if (n_len+num_len) /* any variable name to add? */
  {
    if (n_len)
      memcpy(&buffer[8+p_len],d->name,n_len);
    if (num_len) /* convert number to ASCII */
    {
      indx = num_len - 1;
      while (indx != 0)
      {
        c = '0';
        while (num >= man_power10_table[indx])
        {
          num -= man_power10_table[indx];
          c++;
        }
        buffer[8+p_len+n_len+num_len-indx-1] = c;
        indx--;
      }
      buffer[8+p_len+n_len+num_len-1] = (byte)('0' + num);
    }
  }
  indx = 8+p_len+n_len+num_len;
  
  /* get variable's value */
  if (d->type == MI_TRACE)  /* special: makes no sense because buffer is undefined */
  {                         /*          --> return 0 in tcode field                */
    pTData = (MI_XLOG_HDR *)&buffer[indx];
    WRITE_WORD ((byte *)&pTData->size, 0); /* no data */
    WRITE_DWORD((byte *)&pTData->time, 0);
    WRITE_WORD ((byte *)&pTData->code, 0);
  }
  else 
  {
    memcpy(&buffer[indx], info, i_len);  /* variable's value */
  }

  buffer[1] = p_len + n_len + num_len + i_len + 6;
  buffer[7] = p_len + n_len + num_len;
  return add_ie(buffer, (void *) 0, Ipar);               /* post it */
}

/*------------------------------------------------------------------*/
/* Search directory for given path                                  */
/*------------------------------------------------------------------*/
static MAN_INFO *find_path(MAN_INFO   *d, 
                           char       *path,
                           PATH_CTXT  *pPCtxt,
                           byte       *pDLevel,
                           void       *Ipar)
{
  int    num;
  
  /* check all elements in the given directory */
  for (;;) 
  {
    if (d->type==MI_NEXT_DIR) 
    {
      pPCtxt[*pDLevel].node = d;
      if (!(*pDLevel)--) return 0;
      d = d->info;
      if (!d) return (0);
    }
    
    /* element found */
    else if ((num = dir_compare(d, path)) >= 0) /* node found, returns enum number if any */
    {
      if (d->flags & MI_ENUM_MASK) 
      {
        pPCtxt[*pDLevel].fPara = IntToPtr(((d->flags & MI_ENUM_MASK) == MI_ENUM) ? num - 1 : num);
        pPCtxt[*pDLevel].num   = num;
      }
      else 
      {
        pPCtxt[*pDLevel].fPara = d->context;
        pPCtxt[*pDLevel].num   = 0;
      }
      pPCtxt[*pDLevel].node = d;            /* track node */
      for (; *path && *path!='\\'; path++); /* point to end or next path separator */
      if (!*path) 
      {
        return d;                           /* no more nodes -> work done */
      }
      if (d->type != MI_DIR) return 0;      /* path separator must refer to a directory */
      if (d->flags &MI_CALL)                /* switch to next level directory */
      {
        if (!d->info) return (0);
        d = ((INFO_FUNC)d->info)((void *)0, &pPCtxt[*pDLevel], CMD_NEXTNODE, Ipar);
      }
      else 
      {
        d = d->info;
      }
      if (!d) return (0);
      if (!(*pDLevel)--) return 0;
      path++;
    }

    /* nothing found */
    else 
    {
      if (d->flags &MI_E) break;
      d++;
    }
  }
  return 0;
}

/*------------------------------------------------------------------*/
/* Compare directory names                                          */
/* returns -1 if not found                                          */
/* returns index 0..99 if the specified array element was found     */
/* returns 0 if a non array element was found                       */
/*------------------------------------------------------------------*/
static int dir_compare(MAN_INFO * d, char *b)
{
  int i;
  int num;

  /* does input string contain name in MIF structure */
  for (i=0; d->name[i]; i++) if (d->name[i] != b[i]) return -1;

  num = 0;
  if (d->flags & MI_ENUM_MASK)
  {
    if (!d->context) return -1;

    if (b[i]<'0' || b[i]>'9') return -1; /* allow [0...9] */
    num = b[i]-'0';
    i++;  /* next digit */
    while (num<*(int *)d->context && b[i]>='0' && b[i]<='9') 
    {
      num *=10;
      num +=b[i]-'0';
      i++;
    }

    /* check if number found is in allowed range */
    if ((d->flags & MI_ENUM_MASK) == MI_ENUM)
    {
      if (num == 0 || num > *(int *)d->context) return -1;
    }
    else
    {
      if (num >= *(int *)d->context) return -1;
    }
  }
  /* input string has no index or contains more directories */
  if (!b[i] || b[i]=='\\') {
    return num;
  }

  return -1;
}

/*------------------------------------------------------------------*/
/* Compare context structures (name and num). Return 1 if identical */
/*------------------------------------------------------------------*/
static int ctxt_compare(PATH_CTXT *Ctxt1, PATH_CTXT *Ctxt2, int DLevel)
{
  int i;
  for (i=DIR_LEVELS-1; i>=DLevel; i--)
  {
    if ((Ctxt1[i].node->name != Ctxt2[i].node->name) || (Ctxt1[i].num != Ctxt2[i].num)) return 0;
  }
  return 1;
}

/*------------------------------------------------------------------*/
/* Evaluate the length of an element                                */
/*------------------------------------------------------------------*/
static int info_len(byte type, byte max_len, void * info)
{
  if (type &MI_FIXED_LENGTH) 
  {
    return max_len;
  }
  else if (type==MI_ASCIIZ || type==MI_DIR) 
  {
    return (strlen(info) + 1); /* recognize terminating zero */
  }
  else if (type==MI_EXECUTE)
  {
    return(0);
  }
  else if (type==MI_TRACE)
  {
    return (((TRC_BUF *)info)->size + sizeof(MI_XLOG_HDR));
  }
  else
  {
    return ((byte *)info)[0]+1; /* for Length/Value format variables */
  }
}

/*------ End of file -----------------------------------------------*/
