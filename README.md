------------------------------------------
COUNT-WORD
------------------------------------------
The executable files take file as input and generate File1.txt and File2.txt as the result.

------------------------------------------
readDataOnlyMaster
------------------------------------------
This program has only one thread involved. I use an open source hashtable library uthash to store the word and frequency. 

uthash: 

    doc: https://troydhanson.github.io/uthash/userguide.html#_a_hash_in_c
    
    github: https://github.com/troydhanson/uthash
    
File1.txt can be generated as characters are read from inputfile, when the character is line break, space, nul terminator, tab space, the word stored between such characters will be count and will be put into hash table.
File2.txt can be generated as characters are iterating over the hashtable and store each word and its frequenct in each line.

------------------------------------------
readData
------------------------------------------
This program can have multiple threads involved. I use an open source hashtable library uthash to store the word and frequency, and implemented a linked list to store the data read from inputfile. 

uthash: 

    doc: https://troydhanson.github.io/uthash/userguide.html#_a_hash_in_c
    
    github: https://github.com/troydhanson/uthash

I use the map reduce approach by using several hashtables and merge them to one hashtable after all child threads has finished adding word to hashtables. The master thread will only read from inputfile and write File1 and put the data into linked list, each node in linked list store one word. Other threads have their own hashtables and each time the thread will get one node from linked list and put them into the hashtable. After all threads has completed their own hashtable, we merge the hashtables into the final hashtable and generate File2. The share variable is a global linked list and eof flag indicating if master thread has already read all data from input file.

------------------------------------------
Problem
------------------------------------------

The problem is that the linked-list has to be locked everytime we append new node and get node from it. Other threads have to wait for the linked-list until it is unlocked. And writing on linked list also consumes time. Thus this program is actually slower than the program with only master thread is working. I haven't found a solution yet and I also think that reading and writing file should also be the bottleneck and it depends on the I/O. 


