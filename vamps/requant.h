// vamps requantization thread interfacing
//
// This file is part of Vamps.
//
// Vamps is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Vamps is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Vamps; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#include <stdint.h>
#include <pthread.h>

// keep gcc happy
#define WRITE      \
  orbuf  = orbuf;  \
  mloka1 = mloka1; \
  mloka2 = mloka2; \
  w_eof  = w_eof;

// meaningless
#define MIN_WRITE 0
#define MAX_READ  0
#define MOV_READ

// this is where we switch threads
#define LOCK(x)                          \
  if (unlikely ((x) > (rbuf - cbuf)))    \
  {                                      \
    if (likely (wbuf))                   \
    {                                    \
      pthread_mutex_lock (&mutw);        \
      rqt_wcnt = wbuf - owbuf;           \
      pthread_cond_signal (&condw);      \
      pthread_mutex_unlock (&mutw);      \
    }                                    \
    pthread_mutex_lock (&mutr);          \
    while (!rqt_rcnt)                    \
      pthread_cond_wait (&condr, &mutr); \
    cbuf       = rqt_rptr;               \
    rbuf       = cbuf;                   \
    rbuf      += rqt_rcnt + 3;           \
    rqt_rcnt   = 0;                      \
    owbuf      = rqt_wptr;               \
    fact_x     = rqt_fact;               \
    inbytecnt  = rqt_inbytes;            \
    outbytecnt = rqt_outbytes;           \
    orim2vsize = rqt_visize;             \
    pthread_mutex_unlock (&mutr);        \
    wbuf = owbuf;                        \
  }

#define COPY(x)           \
  memcpy (wbuf, cbuf, x); \
  cbuf += x;              \
  wbuf += x;

#define SEEKR(x) cbuf += x;

#define SEEKW(x) wbuf += x;


// global data for inter thread com
extern float           rqt_fact;
extern int             rqt_rcnt;
extern int             rqt_wcnt;
extern uint64_t        rqt_inbytes;
extern uint64_t        rqt_outbytes;
extern uint64_t        rqt_visize;
extern uint8_t        *rqt_rptr;
extern uint8_t        *rqt_wptr;
extern pthread_cond_t  condr;
extern pthread_cond_t  condw;
extern pthread_mutex_t mutr;
extern pthread_mutex_t mutw;
