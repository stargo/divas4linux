
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

#define DIR_LEVELS  7

/* Size of buffer to mount event information elements */
#define EBUF_SIZE   2200

/* Size of path request buffer */
#define PATHR_SIZE  129

/* Size of buffer to mount standard information elements */
#define IBUF_SIZE   (PATHR_SIZE - 1)

/* Size of variable request buffer */
#define VARR_SIZE   (PATHR_SIZE - 7)

typedef struct path_context_s PATH_CTXT;
typedef struct man_info_s     MAN_INFO;
typedef struct event_ele_s    EVENT_ELE;
typedef void                  INST_PARA;
typedef struct trc_buf_s      TRC_BUF;
typedef void *(*INFO_FUNC)(void *, PATH_CTXT *, int, INST_PARA *);

/*------------------------------------------------------------------*/
/* Internal representation of trace information                     */
/*------------------------------------------------------------------*/
struct trc_buf_s
{
  dword    time;   /* Timestamp in msec units                       */
  word     size;   /* Size of data that follows                     */
  word     code;   /* Code of trace event                           */
  dword    mask;   /* Info type mask, first used for text ev.       */
  word     mod;    /* Module number, first for text events          */
  word     align;  /* Filler to achieve dword alignment             */
  byte far *pdata;
};
  
/*------------------------------------------------------------------*/
/* Declarations/Definitions for use with the Event List Handling    */
/*                                                                  */
/* To realize the functions "event notification", "managing event   */
/* notifications" and "write protection" of variables, we need a    */
/* double linked list or queue of event elements. One link          */
/* collects all events related to a defined Id for event management */
/* and write protection, the other link connects all Id's that are  */
/* to be notified on a defined event. Both links or queues use the  */
/* same event elements.                                             */
/*                                                                  */
/* The starting point for the management queue is an Id related     */
/* pointer in the man_d structure, for the event list it's a        */
/* global pointer specific for each event. Last but not least we    */
/* have a free list pointer that has to be initalized and some      */
/* queue functions.                                                 */
/*                                                                  */
/* PATH_TRAC is used to store the complete path of a variable in a  */
/* memory saving way, because an EVENT_IND has to provide the       */
/* full path of the variable. The path is stored as an array of     */
/* pointers to the names of the node and -if it's an indexed node-  */
/* additionally storing the index number of that path.              */
/* ---------------------------------------------------------------- */

struct path_context_s
{
  MAN_INFO *node;  /* name of node                      */
  int  num;        /* index if node is an array element */
  void *fPara;     /* func para to get the next node/variable value */
};

struct event_ele_s
{
  EVENT_ELE     *NextIdLink;  /* links same Id's or free elements in pool       */
  EVENT_ELE     *NextEvLink;  /* links same events                              */
  MAN_INFO      *d;           /* pointer to my variable                         */
  int           dInst;        /* Instance# (1...) of related MAN_INFO element d */
  INST_PARA     *Ipar;        /* Pointer to instance data                       */
  int (*add_ie)(byte *header, byte *value, void *context); /* generates indicat.*/
  void          *context;     /* reference to task that owns this event         */
  PATH_CTXT     DCtxt[DIR_LEVELS]; /* node name route to this variable          */ 
  byte          status;       /* event element related bit mask                 */
  byte          dLevel;        /* Nesting level of this variable (for unqueing  */
  byte          stopped;       /* tracing stopped and instance was notified     */
  byte          HistoryReadMsgLost;
  dword         HistoryReadPos;
};

/*------------------------------------------------------------------*/
/* Comntains all instance (Id) related data                         */
/* For tracing, we need filter/lock parameters that are related to  */
/* an Id and specific for this Id. So we create an indexed array    */
/* of these filter variables inside the man_d structure.            */
/*------------------------------------------------------------------*/
struct inst_para_s
{
  byte          Id;       /* my Id                                  */
  byte          AckPending;
  byte          RemovePending;
  byte          RemoveId;
  byte          RemoveUser;
  int           RetryTimer;
  int           IndRetryCount;
  EVENT_ELE     *IdQ;     /* connects events for this Id            */
  dword         BChMsk;   /* Mask which channel to trace, B1 is bit0*/
  dword         AudioChMsk; /* Channels to do Audio tap tracing     */
  dword         EyeChMsk; /* Channels to do eye tracing             */
  word          TrcLen;   /* Number of data bytes to trace          */
  word          DbgLvl;   /* Trace events <= this debug level       */
};

/*------------------------------------------------------------------*/
/* The Management Information Directory Structure is internally     */
/* represented as linked arrays of MAN_INFO structures              */
/*------------------------------------------------------------------*/
struct man_info_s 
{
  char        *name;    /* variable's name as AsciiZ string   */
  byte        type;     /* variable's type                    */
  byte        attrib;   /* writeable, event notification,...  */
  byte        flags;    /* how to process info and context    */
  byte        max_len;  /* length of fized len variables      */
  void        *info;    /* function pointer                   */
  void        *context; /* parameter for info                 */
  int         wlock;    /* Instance# that locked this var     */
  EVENT_ELE   **EvQ ;   /* event notification list            */
};

/*------------------------------------------------------------------*/
/* MAN_INFO 'type' field coding                                     */
/* is completed by defintions for IE type coding in man_defs        */
/*------------------------------------------------------------------*/
#define MI_NEXT_DIR  0xff

/*------------------------------------------------------------------*/
/* MAN_INFO 'flags' field coding                                    */
/*------------------------------------------------------------------*/
#define MI_E      0x01  /* is last element in MAN_INFO type array   */
#define MI_CALL   0x02  /* info in MAN_INFO array is function ptr   */
#define MI_ENUM   0x04  /* element is an array element, name has to */
                        /* be completed by an index; the element    */
                        /* *context points to the max available     */
                        /* number of array elements                 */
#define MI_ENUMZ  0x08  /* element is an array element with         */
                        /* zerobased index                          */
#define MI_ENUM_MASK 0x0c

/*------------------------------------------------------------------*/
/* Command codes for info() in MAN_INFO structure                   */
/*------------------------------------------------------------------*/
#define CMD_READVAR     0x00  /* Get read pointer to variable       */
#define CMD_WRITEVAR    0x01  /* Get write pointer to variable      */
#define CMD_NEXTNODE    0x02  /* Get pointer to next node           */
#define CMD_EVENTQ      0x03  /* Get event queue                    */
#define CMD_LOCKVAR     0x04  /* Get pointer to wlock variable      */
#define CMD_EXECUTE     0x05  /* Execute command                    */

/*------------------------------------------------------------------*/
/* Exported functions                                               */
/*------------------------------------------------------------------*/
void man_read         (MAN_INFO  *d,
                       char      *path,
                       int (*add_ie)(byte *header, byte *value, void *context),
                       void      *context,
                       INST_PARA *Ipar, 
                       PATH_CTXT *pPCtxt,
                       byte      *pDLevel
                      );

int man_write         (MAN_INFO  *d,
                       char      *path,
                       void      *info,
                       void      *context,
                       INST_PARA *Ipar, 
                       PATH_CTXT *pPCtxt,
                       byte      *pDLevel
                      );

void man_execute      (MAN_INFO  *d,
                       char      *path,
                       void      *context,
                       INST_PARA *Ipar, 
                       PATH_CTXT *pPCtxt,
                       byte      *pDLevel
                      );

int man_event         (MAN_INFO  *d,
                       char      *path,
                       int (*add_ie)(byte *header, byte *value, void *context), 
                       int (*add_trace)(byte *header, byte *value, void *context), 
                       void      *context,
                       INST_PARA *Ipar, 
                       PATH_CTXT *pPCtxt,
                       byte      *pDLevel,
                       EVENT_ELE **pEvEle,
                       word      QueueIt
                      );
              
int man_lock          (MAN_INFO  *d,
                       char      *path,
                       void      *context,
                       INST_PARA *Ipar, 
                       PATH_CTXT *pPCtxt,
                       byte      *pDLevel
                      );

int man_unlock        (MAN_INFO  *d,
                       char      *path,
                       void      *context,
                       INST_PARA *Ipar, 
                       PATH_CTXT *pPCtxt,
                       byte      *pDLevel
                      );

int man_add_ie        (EVENT_ELE *Ele);
int PutVirtStrVariable( INST_PARA  *Ipar, 
                        int (* add_ie)(byte *header, byte *value, void *context), 
                        char far *StrName, 
                        char far *StrVal
                      );

void InitEvElePool    (void);

void FreeEvEle        (EVENT_ELE *Ele);

void QueueEvEle       (EVENT_ELE *Ele, 
                       EVENT_ELE **pIdQ, 
                       EVENT_ELE **pEvQ
                      );

void UnQueueEvEle     (EVENT_ELE *Ele, 
                       EVENT_ELE **IdQ
                      );

EVENT_ELE *FindEvEle  (EVENT_ELE *IdQ, 
                       PATH_CTXT *pPCtxt,
                       int       DLevel
                      );
                      
EVENT_ELE *AllocEvEle (void
                      );

/*------ End of file -----------------------------------------------*/
