README
------
This SMP cache simulator is based on the uni core cache class developed by Ahmad Samih & Dr.Yan Solihin. 
It implements three cache coherence protocols: MSI, MESI and Dragon. 

Cache organization
------------------
To compile the code, enter the Cache directory and run make. 

 \#cd Cache; make

This generates an executable smp_cache. 

To run the simulator enter: 

  \#./smp_cache \<cache_size\>  \<assoc\> \<block_size\> \<num_processors\> \<protocol\> \<trace_file\>

sample trace files are given in the trace directory.



