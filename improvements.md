BigSort performs 4 steps, almost in sequence as any steps wait the end of previous to advance to the next (1 - Read and store M blocks, 2 - internal sort of M blocks, 3 - write to temporary files, 4 - external sort).
Moreover almost any steps internally is performed waiting the completion of its IO operations. 
<pre>
M = number of blocks loadable in memory
While( !eof of unsorted file )
1)  Read one block (blocking read)
    Store block in memory 
    if blocks num == M {
2)      Sort M blocks stored in memory // this phase is monolithic and end when all element are sorted 
3)      Write sorted sequence to a file.x (Blocking to wait last write)
        Clear memory ( delete blocks stored in a collection )
    }
}
4)
External sort - merge temporary files (file.1...file.n)
</pre>

<h2>Some ideas</h2>

Maintaining the same design of bigsort, an easy solution to improve performance could be to parallelize internally any single phase, for example phase 1 could perform M non blocking reads, but a better solution could be to modify the design and join together involved phases, as for example, the phase 2 should not wait the end of phase 1 to run.
I list some possible optimizations that also leverage on seastar framework.

<b>Phase 1)</b> The read from unsorted file could be done in parallel, instead of sequentially, because the number of records than must be read is know in advance, therefore is possible to perform any read in loop without wait for the result, and just when a data block is read it will be stored in memory. Since this reads run on the same shard, async operations are done on the same thread and is not necessary to synchronize the access to the container that will store the elements read.
<pre>
M = number of block loadable in memory
While(!eof of unsorted file)
    Read one block (non blocking read) 
    .then (Phase 2)   
}
</pre>
<b>Phase 2) Internal sorting</b>
This phase as implemented in bigsort is a blackbox because depends by other library that provide very optimized sort algo. As the std::stable_sort used by bigsort.
Other options could be to use boost that provide interesting function also for parallel sort that probably is the fastest option.
BUT A different approach could be to concatenate this phase with the previous, in this way when a new element is available, sorting starts without waiting all the elements.
To do this it's needed a sort algo of the category of "insertion sort"  that allow to add data step by step. At the moment i've got in mind std::set.
For example std::set have a O(log(size)) complexity for each insert, could be a good candidate but for sure some performance tests in this scope are needed. 
Anyway by std::set is possible run the phase 2 without waiting the one to terminate, just when a new element has been read this could be added to the set and when the last element will be extracted from the file the set will just need to inser/sort this last element. In this way is possible to say that phase1 and 2 runs together and while phase1 wait for io latency the phase2 perform sort.
<pre>
if available data block.then{
    insert block to sorted collection
    if blocks num == M {
        next phase
    }
}
</pre>
<b>3) Write to temporary sorted file</b>
This phase could not be joined to the previous because we need to save on disk a sorted sequence of blocks and this is available only at the end of sort.
Moreover this phase is monolithic because if we need to read other elements from the main file (phase 1), the read cannot proceed as is waiting for memory, at this stage all the element are in memory, stored in a collection, and therefore write needs to be completed so stored blocks could be erased, deallocating the memory.
BUT here is possible another optimization. We can join this phase (3) with phase (1). It's possible to free a memory block just when it has been written. While the set containing the block is involved in the write, the new read 
could insert data in another set. this optimization add some complexity. The idea is that write phase do not block read. The algo needs 2 set ( or in general two containers ), and a variable that indicate which is the active set for store new elements. A semaphore can handle the availability of memory and pause read when no mem is available and just one write need to complete to free at least a memory block. 
<pre>
phase_3:
switch active set
for_each el in collection of inactive set{
    write el to file_x.then semaphore.signal(1)
}
</pre>
<pre>
phase_1:
M = number of block loadable in memory
static thread_local seastar::semaphore limit(M);
While(! eof of unsorted file )
    semaphore.wait(1)
        Read one block .then (Phase 2)   
}
</pre>
<pre>
phase_2:
if available bock.then{
    insert block to sorted collection
    if blocks num == M {
        phase(3)
    }
}
</pre>

<b>4 - External sort/merge of saved files (file.1...file.n)</b>

The current strategy for external sort expect to merge n-files already complete and sorted, therefore there's no ways to join this phase with previous, The best would be that while partial files grows the external sort keep new elements and move forward without waiting that previous phases are all completed. Maybe is possible but needs further investigation.</br>
</br>
Without big improvements is possible some optimization as explained here:

<b>Current external sort</b>
<pre>
sorted_files = sorted files generated by previous steps
F = count of partial sorted files
f_cur_pos = position on file_f to read next block 
while !eof of sorted files {
    for each file in sorted_files (k) { // loop -a-
        new_block = read block from file f at pos f_cur_pos
        if( new_block < min_block){
            min_block = new_block            
            file_index = k
        }
    }
    increment f_cur_pos[file_index]
    write min_block to target_file
}
</pre>

the loop -a- at each iteration read one block from any files involved, at the end it read sequentially F blocks.
In this way If we have to merge M elements, spreads in F files, the number of read is of M*F and the num of write is M.
BUT always looking at the loop(a), at the next round, it read again F block but just one is changed, because at the previous loop only the f_cur_pos[file_index] is changed, and, is the position of the file from which the min 
was got.
Therefore it's possible to avoid F reads at each loop, and just do F read at first loop and only one read in the followings. 
The F elements should be stored in memory.
In this case the number of read will be of F+M-1. 


<b>Levaragin on seastar framework</b>

Bigsort work on a big file that cannot be stored in memory, the input is split in loadable partition to be processed in memory and produce intermediate sorted files.
The final step of bigsort (phase 4) merge these files working mainly on the disk and loading in memory few elements.
With seastar framework it should be possible a further level of partitioning of the input file and therefore process any partition on its shard.
For example with a 4 core cpu, it's possible to have 4 shards and partitioning the file in 4 smallel files.
At the end of bigsort on each shard we have a sorted file related to its input file (a partition of the origin file).

The advantage is that the algo works in parallel on any partition by its own cpu.

The drawback is that another step of merge is required to merge the results of each shard and this final task must be done on a single shard.

A possible optimization to avoid the drawback, but needs more info and investigation, could be to avoid this final merge and perform the k-way merge of any shard directly to the target file.

To better explain the algo, when any shard starts the phase 4, instead of write the min to its local file could send it to a coordinator. Coordinator is a shard that collect min blocks, and wait exactly ncore blocks before write the min to target file. Therefore the coordinator needs to receive data from other shards, the cost for share data must be evaluated. But I guess that this option invalidate the shared-nothing constraint.  
