/*******************************************************
  cache.h
  Ahmad Samih & Yan Solihin
  2009
  {aasamih,solihin}@ece.ncsu.edu
 ********************************************************/

#ifndef CACHE_H
#define CACHE_H

#include <cmath>
#include <iostream>

typedef unsigned long ulong;
typedef unsigned char uchar;
typedef unsigned int uint;

/****add new states, based on the protocol****/
enum{
	INVALID = 0,
	VALID,
	DIRTY,
	MODIFIED,
	SHARED,
	EXCLUSIVE,
	SHARED_MODIFIED,
	SHARED_CLEAN,
	UPDATE
};

enum{
	NO_OP =0,
	BUSRD,
	BUSRDX,
	FLUSH,
	BUSUPGR,
	FLUSHOPT,
	BUSUPD,
	BUSRD_BUSUPD
};

class cacheLine 
{
	protected:
		ulong tag;
		ulong Flags;   // 0:invalid, 1:valid, 2:dirty 
		ulong seq; 

	public:
		cacheLine()            { tag = 0; Flags = 0; }
		ulong getTag()         { return tag; }
		ulong getFlags()			{ return Flags;}
		ulong getSeq()         { return seq; }
		void setSeq(ulong Seq)			{ seq = Seq;}
		void setFlags(ulong flags)			{  Flags = flags;}
		void setTag(ulong a)   { tag = a; }
		void invalidate()      { tag = 0; Flags = INVALID; }//useful function
		bool isValid()         { return ((Flags) != INVALID); }
};

class Cache
{
	protected:
		ulong size, lineSize, assoc, sets, log2Sets, log2Blk, tagMask, numLines;
		ulong reads,readMisses,writes,writeMisses,writeBacks;
		ulong flushes,invalidations,memory,interventions,c2ctransfers;

		//******///
		//add coherence counters here///
		//******///

		cacheLine **cache;
		ulong calcTag(ulong addr)     { return (addr >> (log2Blk) );}
		ulong calcIndex(ulong addr)  { return ((addr >> log2Blk) & tagMask);}
		ulong calcAddr4Tag(ulong tag)   { return (tag << (log2Blk));}

	public:
		ulong currentCycle;  
		ulong flush_opts;

		Cache(int,int,int);
		~Cache() { delete cache;}
		Cache(const Cache& copy);

		cacheLine *findLineToReplace(ulong addr);
		cacheLine *fillLine(ulong addr);
		cacheLine *DragonfillLine(ulong addr);
		cacheLine * findLine(ulong addr);
		cacheLine * getLRU(ulong);

		ulong getRM(){return readMisses;} ulong getWM(){return writeMisses;} 
		ulong getReads(){return reads;}ulong getWrites(){return writes;}
		ulong getWB(){return writeBacks;}

		void writeBack(ulong)   {writeBacks++;}
		void Access(ulong,uchar);
		int MSIAccess(ulong,uchar);
		int MSIBus(ulong,unsigned int);
		void MSIFlush(ulong,unsigned int);
		int MESIAccess(ulong,uchar,bool);
		int MESIBus(ulong,unsigned int);
		void MESIFlush(ulong,unsigned int);
		int DragonAccess(ulong,uchar,bool);
		int DragonBus(ulong,unsigned int);
		void printStats(int,int);
		void updateLRU(cacheLine *);

		//******///
		//add other functions to handle bus transactions///
		//******///

};

#endif
