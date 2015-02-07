/*******************************************************
                          cache.cc
                  Ahmad Samih & Yan Solihin
                           2009
                {aasamih,solihin}@ece.ncsu.edu
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include "cache.h"
#include <stdio.h>
#include <vector>
using namespace std;

Cache::Cache(int s,int a,int b )
{
	ulong i, j;
	reads = readMisses = writes = 0; 
	writeMisses = writeBacks = currentCycle = 0;
	flushes = invalidations = memory = interventions = c2ctransfers = flush_opts = 0;

	size       = (ulong)(s);
	lineSize   = (ulong)(b);
	assoc      = (ulong)(a);   
	sets       = (ulong)((s/b)/a);
	numLines   = (ulong)(s/b);
	log2Sets   = (ulong)(log2(sets));   
	log2Blk    = (ulong)(log2(b));   

	//*******************//
	//initialize your counters here//
	//*******************//

	tagMask =0;
	for(i=0;i<log2Sets;i++)
	{
		tagMask <<= 1;
		tagMask |= 1;
	}

	/**create a two dimentional cache, sized as cache[sets][assoc]**/ 
	cache = new cacheLine*[sets];
	for(i=0; i<sets; i++)
	{
		cache[i] = new cacheLine[assoc];
		for(j=0; j<assoc; j++) 
		{
			cache[i][j].invalidate();
		}
	}      

}

Cache::Cache(const Cache& copy)
{
	ulong i, j;
	reads      = copy.reads;
	readMisses = copy.readMisses;
	writes     = copy.writes; 
	writeMisses = copy.writeMisses;
	writeBacks = copy.writeBacks;
	currentCycle = copy.currentCycle;
	flushes    =copy.flushes;
	invalidations    =copy.invalidations;
	memory     = copy.memory;
	interventions = copy.interventions;
	c2ctransfers = copy.c2ctransfers;
	size       = copy.size;
	lineSize   = copy.lineSize;
	assoc      = copy.assoc;   
	sets       = copy.sets;
	numLines   = copy.numLines;
	log2Sets   = copy.log2Sets;   
	log2Blk    = copy.log2Blk;   
	tagMask    = copy.tagMask;

	cache = new cacheLine*[sets];
	for (i =0; i<sets; i++)
	{
		cache[i] = new cacheLine[assoc];
		for(j=0;j<assoc;j++)
		{
			cache[i][j].invalidate();
		}
	}
}

/**you might add other parameters to Access()
  since this function is an entry point 
  to the memory hierarchy (i.e. caches)**/
void Cache::Access(ulong addr,uchar op)
{
	currentCycle++;/*per cache global counter to maintain LRU order 
			 among cache ways, updated on every cache access*/

	if(op == 'w') writes++;
	else          reads++;

	cacheLine * line = findLine(addr);
	if(line == NULL)/*miss*/
	{
		if(op == 'w') writeMisses++;
		else readMisses++;

		cacheLine *newline = fillLine(addr);
		if(op == 'w') newline->setFlags(DIRTY);    

	}
	else
	{
		/**since it's a hit, update LRU and update dirty flag**/
		updateLRU(line);
		if(op == 'w') line->setFlags(DIRTY);
	}
}

/* Access with MSI protocol */
int Cache::MSIAccess(ulong addr,uchar op)
{
	currentCycle++;/*per cache global counter to maintain LRU order 
			 among cache ways, updated on every cache access*/

	if(op == 'w') writes++;
	else          reads++;

	cacheLine * line = findLine(addr);
	if(line == NULL || (line && line->getFlags()== INVALID))
	{
		memory++;
		if(op == 'w') writeMisses++;
		else if(op == 'r') readMisses++;

		cacheLine *newline = fillLine(addr);
		if(op == 'w') 
		{
			newline->setFlags(MODIFIED);    
			return BUSRDX;
		}
		if(op == 'r') 
		{
			newline->setFlags(SHARED);    
			if(line && line->getFlags()== INVALID)
				interventions++;
			return BUSRD;
		}
	}
	else
	{
		/**since it's a hit, update LRU and update dirty flag**/
		updateLRU(line);
		if(line->getFlags() == SHARED && op =='w') 
		{
			/*Send BusRdX */
			line->setFlags(MODIFIED);
			memory++;
			return BUSRDX;
		}
	}
	return NO_OP;
}

int Cache::MSIBus(ulong addr,unsigned int busop)
{
	cacheLine * line = findLine(addr);
	if(line != NULL)
	{
		if(line->getFlags() == SHARED && busop == BUSRDX)
		{
			line->invalidate();
			invalidations++;
		}
		if(line->getFlags() == MODIFIED && busop == BUSRDX)
		{
			line->invalidate();
			invalidations++;
			writeBack(addr);
			flushes++;
			return FLUSH;
		}
		if(line->getFlags() == MODIFIED && busop == BUSRD)
		{
			line->setFlags(SHARED);
			writeBack(addr);
			flushes++;
			interventions++;
			return FLUSH;
		}
		return NO_OP;
	}
	return NO_OP;
}

void Cache::MSIFlush(ulong addr,unsigned int busop)
{
	cacheLine *line = findLine(addr);
	if(line == NULL)
		line = fillLine(addr);
	if (busop == BUSRD)
	{
		line->setFlags(SHARED);
	}
	if (busop == BUSRDX)
	{	
		line->setFlags(MODIFIED);
	}
}

int Cache::MESIAccess(ulong addr,uchar op,bool copy_exists)
{
	currentCycle++;/*per cache global counter to maintain LRU order 
			 among cache ways, updated on every cache access*/

	if(op == 'w') writes++;
	else          reads++;

	cacheLine * line = findLine(addr);
	if(line == NULL || (line && line->getFlags()== INVALID))
	{
		/* Block not found!! Start a memory access*/
		if(op == 'w') writeMisses++;
		else if(op == 'r') readMisses++;

		cacheLine *newline = fillLine(addr);
		if(op == 'w')
		{
			if(copy_exists == true)
				c2ctransfers++;
			else
				memory++;
			newline->setFlags(MODIFIED);

			return BUSRDX;
		}
		if(op == 'r') 
		{
			if(copy_exists == true)
			{
				/* Wait!! Found it. Cancel that expensive memory access */
				newline->setFlags(SHARED);    
				if(line && line->getFlags()== INVALID)
					interventions++;
				/* Somebody is going to flush the cache. */
				c2ctransfers++;
			}
			else
			{
				newline->setFlags(EXCLUSIVE);
				memory++;
			}
			return BUSRD;
		}

	}
	else
	{
		/**since it's a hit, update LRU and update dirty flag**/
		updateLRU(line);
		if(line->getFlags() == SHARED && op =='w') 
		{
			line->setFlags(MODIFIED);
			return BUSUPGR;
		}
		if(line->getFlags() == EXCLUSIVE && op =='w') 
		{
			line->setFlags(MODIFIED);
		}
	}
	return NO_OP;
}
int Cache::MESIBus(ulong addr,unsigned int busop)
{
	cacheLine * line = findLine(addr);
	if(line != NULL)
	{
		if(line->getFlags() == SHARED)
		{
			if (busop == BUSRD)
			{
				return FLUSHOPT;
			}
			if (busop == BUSRDX )
			{
				line->invalidate();
				invalidations++;
				return FLUSHOPT;
			}
			if (busop == BUSUPGR)
			{
				line->invalidate();
				invalidations++;
				return NO_OP;
			}
		}
		if(line->getFlags() == MODIFIED)
		{
			if (busop == BUSRD)
			{
				line->setFlags(SHARED);
				interventions++;
				writeBack(addr);
			}
			if (busop == BUSRDX)
			{
				line->invalidate();
				invalidations++;
				writeBack(addr);
			}
			flushes++;
			return FLUSH;
		}
		if(line->getFlags() == EXCLUSIVE)
		{
			if (busop == BUSRD)
			{
				line->setFlags(SHARED);
				interventions++;
			}
			if (busop == BUSRDX)
			{
				line->invalidate();
				invalidations++;
			}
			return FLUSHOPT;
		}
	}
	return NO_OP;
}
void Cache::MESIFlush(ulong addr,unsigned int busop)
{
	cacheLine *line = findLine(addr);
	if(line == NULL)
		line = fillLine(addr);
	if (busop == BUSRD)
	{
		line->setFlags(SHARED);
	}
	if (busop == BUSRDX)
	{	
		line->setFlags(MODIFIED);
	}	
}
int Cache::DragonAccess(ulong addr,uchar op,bool copy_exists)
{
	currentCycle++;/*per cache global counter to maintain LRU order 
			 among cache ways, updated on every cache access*/

	if(op == 'w') writes++;
	else          reads++;

	cacheLine * line = findLine(addr);
	if(line == NULL )
	{
		/* Block not found!! Start a memory access*/
		if(op == 'w') writeMisses++;
		else if(op == 'r') readMisses++;
		memory++;

		cacheLine *newline = DragonfillLine(addr);
		if(op == 'w')
		{
			if(copy_exists == true)
			{
				newline->setFlags(SHARED_MODIFIED);
				return BUSRD_BUSUPD;
			}
			else
			{
				newline->setFlags(MODIFIED);
				return BUSRD;
			}
		}
		if(op == 'r') 
		{
			if(copy_exists == true)
			{
				newline->setFlags(SHARED_CLEAN);    
			}
			else
			{
				newline->setFlags(EXCLUSIVE);
			}
			return BUSRD;
		}

	}
	else
	{
		/**since it's a hit, update LRU and update dirty flag**/
		updateLRU(line);
		if(line->getFlags() == SHARED_CLEAN && op =='w') 
		{
			if(copy_exists == true)
				line->setFlags(SHARED_MODIFIED);
			else
				line->setFlags(MODIFIED);
			return BUSUPD;
		}
		if(line->getFlags() == SHARED_MODIFIED && op =='w') 
		{	
			if (copy_exists == false)
				line->setFlags(MODIFIED);
			return BUSUPD;
		}
		if(line->getFlags() == EXCLUSIVE && op =='w') 
		{
			line->setFlags(MODIFIED);
		}
	}
	return NO_OP;
}
int Cache::DragonBus(ulong addr,unsigned int busop)
{
	cacheLine * line = findLine(addr);
	if(line != NULL)
	{
		if(line->getFlags() == SHARED_MODIFIED)
		{
			if (busop == BUSRD)
			{
				flushes++;
				return FLUSH;
			}
			if (busop == BUSUPD)
			{
				line->setFlags(SHARED_CLEAN);
				return UPDATE;
			}
		}
		if(line->getFlags() == MODIFIED)
		{
			if (busop == BUSRD)
			{
				line->setFlags(SHARED_MODIFIED);
				flushes++;
				interventions++;
				return FLUSH;
			}
		}
		if(line->getFlags() == EXCLUSIVE)
		{
			if (busop == BUSRD)
			{
				line->setFlags(SHARED_CLEAN);
				interventions++;
			}
		}
	}
	return NO_OP;

}
/*look up line*/
cacheLine * Cache::findLine(ulong addr)
{
	ulong i, j, tag, pos;

	pos = assoc;
	tag = calcTag(addr);
	i   = calcIndex(addr);

	for(j=0; j<assoc; j++)
		if(cache[i][j].isValid())
			if(cache[i][j].getTag() == tag)
			{
				pos = j; break; 
			}
	if(pos == assoc)
		return NULL;
	else
		return &(cache[i][pos]); 
}

/*upgrade LRU line to be MRU line*/
void Cache::updateLRU(cacheLine *line)
{
	line->setSeq(currentCycle);  
}

/*return an invalid line as LRU, if any, otherwise return LRU line*/
cacheLine * Cache::getLRU(ulong addr)
{
	ulong i, j, victim, min;

	victim = assoc;
	min    = currentCycle;
	i      = calcIndex(addr);

	for(j=0;j<assoc;j++)
	{
		if(cache[i][j].isValid() == 0) return &(cache[i][j]);     
	}   
	for(j=0;j<assoc;j++)
	{
		if(cache[i][j].getSeq() <= min) { victim = j; min = cache[i][j].getSeq();}
	} 
	assert(victim != assoc);

	return &(cache[i][victim]);
}

/*find a victim, move it to MRU position*/
cacheLine *Cache::findLineToReplace(ulong addr)
{
	cacheLine * victim = getLRU(addr);
	updateLRU(victim);

	return (victim);
}
/*allocate a new line*/
cacheLine *Cache::DragonfillLine(ulong addr)
{ 
	ulong tag;

	cacheLine *victim = findLineToReplace(addr);
	assert(victim != 0);
	if(victim->getFlags() == MODIFIED || victim->getFlags() == SHARED_MODIFIED) writeBack(addr);

	tag = calcTag(addr);   
	victim->setTag(tag);
	victim->setFlags(VALID);    
	/**note that this cache line has been already 
	  upgraded to MRU in the previous function (findLineToReplace)**/

	return victim;
}


/*allocate a new line*/
cacheLine *Cache::fillLine(ulong addr)
{ 
	ulong tag;

	cacheLine *victim = findLineToReplace(addr);
	assert(victim != 0);
	if(victim->getFlags() == MODIFIED) writeBack(addr);

	tag = calcTag(addr);   
	victim->setTag(tag);
	victim->setFlags(VALID);    
	/**note that this cache line has been already 
	  upgraded to MRU in the previous function (findLineToReplace)**/

	return victim;
}

void Cache::printStats(int proc_num, int protocol)
{ 
	printf("============ Simulation results (Cache %d) ============\n",proc_num);
	float miss_rate=(float)((float)(readMisses+writeMisses)/(float)(reads+writes))*100;
	/****print out the rest of statistics here.****/
	/****follow the ouput file format**************/
	ulong memory_transactions = memory;
	ulong num_writebacks = writeBacks+flushes;
	if (protocol == 0)
	{
		num_writebacks = writeBacks;
		memory_transactions = writeBacks + memory;
	}
	if (protocol == 1)
	{
		num_writebacks = writeBacks;
		memory_transactions = writeBacks + memory;
	}
	if (protocol == 2)
		memory_transactions = memory+writeBacks+flushes;

	printf("01. number of reads: 				%lu\n",reads);
	printf("02. number of read misses: 			%lu\n",readMisses);
	printf("03. number of writes: 				%lu\n",writes);
	printf("04. number of write misses:			%lu\n",writeMisses);
	printf("05. total miss rate: 				%.2f%%\n",miss_rate);
	printf("06. number of writebacks: 			%lu\n",num_writebacks);
	printf("07. number of cache-to-cache transfers: 	%lu\n",c2ctransfers);
	printf("08. number of memory transactions: 		%lu\n",memory_transactions);
	printf("09. number of interventions: 			%lu\n",interventions);
	printf("10. number of invalidations: 			%lu\n",invalidations);
	printf("11. number of flushes: 				%lu\n",flushes);

}
