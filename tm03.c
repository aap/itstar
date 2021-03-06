/*

  Versions of inword() and outword() to convert between 36-bit PDP-10 words and
  the 8-bit "core dump" format used by the TM03 formatter found in the TU45,
  TU77 etc.

  The TM03 packs each 36-bit word into 5 consecutive tape frames.  The leftmost
  32 bits of the word are stored in the first 4 tape frames (ordered from left
  to right), and the remaining four bits are stored in the low half of the 5th
  tape frame.  The left half of the 5th bit is written as 0000 and treated as
  "don't care" data on read.

  Entry points:
  resetbuf, tapeflush, taperead, inword, outword, remaining.

  By John Wilson.

  03/14/1995  JMBW  Created.

  This file is part of itstar.

  itstar is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  itstar is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with itstar.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <stdio.h>
#include <stdlib.h>

#include "itstar.h"

//#define RECLEN (5*512)	/* we deal in 512-word records */
//			/* (AI:SYSDOC;DUMP FORMAT says 1024 but it's wrong) */
#define RECLEN (10*512)	/* maybe it's not so wrong after all */

void outword();

static char tapebuf[RECLEN];  /* tape I/O buffer */
static char *tapeptr;	/* ptr to next posn in tapebuf[] */
static int recl;	/* record length on read */

/* prepare to begin writing or reading a record (call before switching r/w!) */
/* (actually, only used for writing records now -- JMBW 07/14/98) */
void resetbuf()
{
	tapeptr=tapebuf;		/* used when writing */
	recl=0;				/* used when reading */
}

/* flush tape output buffer if needed */
void tapeflush()
{
	if(tapeptr!=tapebuf) {		/* something to flush */
		while((tapeptr-tapebuf)<12)
			outword(0L,0L);	/* pad if too short for tape hardware */
					/* (records must be >= 12 bytes) */
		putrec(tapebuf,tapeptr-tapebuf);
		tapeptr=tapebuf;
	}
}

/* read tape record into buf, return 0 on success or -1 on EOF */
int taperead()
{
	recl=getrec(tapebuf,RECLEN);
	if(recl<=0) return(-1);	/* EOF */
	if(recl%5) {		/* TM03 stores words as 5 tape frames */
		fprintf(stderr,"?Record length not word multiple\n");
		exit(1);
	}
	tapeptr=tapebuf;
	return(0);
}

/* read a word, store halfwords at the addresses given by call args */
void inword(long *l,long *r)
{
	register unsigned long a,b,c;

	if(recl==0) {			/* no more data */
		fprintf(stderr,"?Tape record too short\n");
		exit(1);
	}

	/* left half */
	a=*tapeptr++;
	b=*tapeptr++;
	c=*tapeptr++;
	*l=((a<<10)&0776000)|((b<<2)&0001774)|((c>>6)&03);

	/* right half */
	b=*tapeptr++;
	a=*tapeptr++;
	*r=((c<<12)&0770000)|((b<<4)&0007760)|(a&017);

	recl-=5;			/* count it */
}

/* as above but wraps to next rec if needed */
/* returns -1 on EOF, 0 on success */
int nextword(long *l,long *r)
{
	register unsigned long a,b,c;

	if(recl==0)			/* no more data */
		if(taperead()<0) return(-1);

	/* left half */
	a=*tapeptr++;
	b=*tapeptr++;
	c=*tapeptr++;
	*l=((a<<10)&0776000)|((b<<2)&0001774)|((c>>6)&03);

	/* right half */
	b=*tapeptr++;
	a=*tapeptr++;
	*r=((c<<12)&0770000)|((b<<4)&0007760)|(a&017);

	recl-=5;			/* count it */
	return(0);
}

/* return # of words remaining in buffer */
int remaining()
{
	return(recl/5);
}

/* write a word */
void outword(register unsigned long l,register unsigned long r)
{
	*tapeptr++=(l>>10)&0377;
	*tapeptr++=(l>>2)&0377;
	*tapeptr++=((l<<6)&0300)|((r>>12)&077);
	*tapeptr++=(r>>4)&0377;
	*tapeptr++=r&017;

	/* see if the buffer needs to be flushed */
	if(tapeptr==tapebuf+RECLEN) tapeflush();
}
