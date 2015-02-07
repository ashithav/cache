/*******************************************************
                          main.cc
                  Ahmad Samih & Yan Solihin
                           2009
                {aasamih,solihin}@ece.ncsu.edu
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include <fstream>
#include <sstream>
#include <vector>
using namespace std;

#include "cache.h"

#define NUM_PROCESSORS 4

/* Check if there are caches copies. */
int MESICheckCopies(ulong addr, vector<Cache>* cachesArray)
{
	for(vector<Cache>::size_type i = 0;i != cachesArray->size();i++)
	{
		if((*cachesArray)[i].findLine(addr))
			return 1;
	}
	return 0;
}

int DragonCheckCopies(ulong addr, vector<Cache>* cachesArray)
{
	int state = 0;
	cacheLine * line = NULL;
	for(vector<Cache>::size_type i = 0;i != cachesArray->size();i++)
	{
		if((line = (*cachesArray)[i].findLine(addr)))
		{
			if(line->getFlags() == EXCLUSIVE)
				return EXCLUSIVE;
			if(line->getFlags() == MODIFIED)
				return MODIFIED;		
			if(line->getFlags() == SHARED_MODIFIED)			
				state = SHARED_MODIFIED;
			if(line->getFlags() == SHARED_CLEAN && state != SHARED_MODIFIED)
				state = SHARED_CLEAN;
		}
	}
	return state;
}

int main(int argc, char *argv[])
{
	
	ifstream fin;
	FILE * pFile;
	string line;
	int i=0;

	if(argv[1] == NULL){
		 printf("input format: ");
		 printf("./smp_cache <cache_size> <assoc> <block_size> <num_processors> <protocol> <trace_file> \n");
		 exit(0);
        }

	/*****uncomment the next five lines*****/
	int cache_size = atoi(argv[1]);
	int cache_assoc= atoi(argv[2]);
	int blk_size   = atoi(argv[3]);
	int num_processors = atoi(argv[4]);/*1, 2, 4, 8*/
	int protocol   = atoi(argv[5]);	 /*0:MSI, 1:MESI, 2:Dragon*/
	char *fname =  (char *)malloc(20);
 	fname = argv[6];

	
	/* Print name and unity ID*/
	printf("===== 506 Personal information =====\n");
	printf("Ashitha Velayudhan\navelayu\n");	
	printf("Section ECE/CSC001\n");
	//****************************************************//
	//**printf("===== Simulator configuration =====\n");**//
	//*******print out simulator configuration here*******//
	//****************************************************//
	printf("===== 506 SMP Simulator configuration =====\n");
	printf("L1_SIZE:		%d\n",cache_size);
	printf("L1_ASSOC:		%d\n",cache_assoc);
	printf("L1_BLOCKSIZE:		%d\n",blk_size);
	printf("NUMBER OF PROCESSORS:	%d\n",NUM_PROCESSORS);
	if(protocol == 0)
		printf("COHERENCE PROTOCOL:	MSI\n");
	else if(protocol == 1)
		printf("COHERENCE PROTOCOL:	MESI\n");
	else if(protocol == 2)
		printf("COHERENCE PROTOCOL:	Dragon\n");
	printf("TRACE FILE:		%s\n",fname);

 
	//*********************************************//
        //*****create an array of caches here**********//
	//*********************************************//	
	std::vector<Cache> cachesArray(num_processors,Cache(cache_size,cache_assoc,blk_size));
	//Cache cachesArray[4]={Cache(cache_size,cache_assoc,blk_size),Cache(cache_size,cache_assoc,blk_size),Cache(cache_size,cache_assoc,blk_size),Cache(cache_size,cache_assoc,blk_size)};

	pFile = fopen (fname,"r");
	if(pFile == 0)
	{   
		printf("Trace file problem\n");
		exit(0);
	}
	///******************************************************************//
	//**read trace file,line by line,each(processor#,operation,address)**//
	//*****propagate each request down through memory hierarchy**********//
	//*****by calling cachesArray[processor#]->Access(...)***************//
	///******************************************************************//
	ifstream infile(fname);
	while(getline(infile,line))
	{
		istringstream iss(line);
		int proc_num;
		uchar cmd;
		char addr_string[10];
		int busop=0;
		bool copy_exists = false;
		iss>>proc_num>>cmd>>addr_string;
		ulong addr = strtoul(addr_string,NULL,16);
		
		if (protocol == 0)
		{
			if((busop = cachesArray[proc_num].MSIAccess(addr,cmd)))
			{
				for(i=0;i<num_processors ;i++)
				{
					if(i!=proc_num && FLUSH == cachesArray[i].MSIBus(addr,busop))
					{
						cachesArray[proc_num].MSIFlush(addr,busop);
					}
						
				}
			}
		}
		else if(protocol == 1)
		{
			for(i=0;i<num_processors ;i++)
			{
				copy_exists = MESICheckCopies(addr,&cachesArray);
			}
			if((busop = cachesArray[proc_num].MESIAccess(addr,cmd,copy_exists)))
			{
				for(i=0;i<num_processors;i++)
				{
					if(i != proc_num )
					{
						cachesArray[i].MESIBus(addr,busop);
					}
				}
			}
		}
		else if(protocol == 2)
		{
			copy_exists = DragonCheckCopies(addr,&cachesArray);
			if((busop = cachesArray[proc_num].DragonAccess(addr,cmd,copy_exists)))
			{
				for(i=0;i<num_processors;i++)
				{
					if(i != proc_num )
					{
						if(busop == BUSRD_BUSUPD)
						{
							cachesArray[i].DragonBus(addr,BUSRD);
							cachesArray[i].DragonBus(addr,BUSUPD);
						}
						else
							cachesArray[i].DragonBus(addr,busop);
					}
				}
			}
		}
		copy_exists = false;
	}
	fclose(pFile);

	//********************************//
	//print out all caches' statistics //
	//********************************//
	for(i=0;i<num_processors ;i++)
		cachesArray[i].printStats(i,protocol);
	return 0;
}
