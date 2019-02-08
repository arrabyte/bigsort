<h2>bigsort</h2>

A sort application that allow to sort 4k blocks of data inside a blob that cannot be handled in main memory.

bigsort handle big data using two strategy: internal and external sort.

In the first step the input file is partitioned and each partition is bound to main memory.

Blocks of each partition can be sorted in main memory and stored to file : filename.partition_number

At the end of this phase we have a set of sorted files that need to be merged together, for this goal I've used a simple merge sort 

algorithm that compare the head element of each file and get the min at each iteration.  

Example:

unsorted data = [z,t,s,c,a,b,r,i,l,m,d,e] 
every character is a chunck of 4k, there are N element.

in memory it's possible to load M chunks at time
unsorted loadable chunks partitioning [z,t,s] [c,a,b] [r,i,l] [m,d,e]

every chunks could be sorted in memory and stored to disk
Sort by std::stable_sort, sort or boost

[z,t,s] [c,a,b] [r,i,l] [m,d,e] =>  [s,t,z] [a,b,c] [i,l,r] [d,e,m]

now we have (K) sorted subset of size C.
k-way mergesort could merge them obtaining N sorted element. 

read from disk the first element of every chuck, store the min on a new file.

[s,t,z] [a,b,c] [i,l,r] [d,e,m] => a<br>
[s,t,z] [c] [i,l,r] [d,e,m] => b<br>
[s,t,z] [] [i,l,r] [d,e,m] => c<br>
[s,t,z] [] [i,l,r] [d,e,m] => d<br>
[s,t,z] [] [i,l,r] [m] => e<br>
[s,t,z] [] [l,r] [m] => i<br>
[s,t,z] [] [r] [m] => l<br>
[s,t,z] [] [r] [] => m<br>
[s,t,z] [] [] [] => r<br>
[t,z] [] [] [] => s<br>
[z] [] [] [] => t<br>
[] [] [] [] => z<br>


<h2>Build</h2>
  
To build bigsort the seastar framework is needed.<br>

<pre>
create a build folder\n
cd build\n
cmake .. 
make
</pre>
</br></br>
<h2>Run</h2>
</br>
<code>./bigsort filename</code>
</br></br>
bigsort create a new file filename.sort in the same folder of filename.</br> 
</br>
<h2>Run test</h2>
</br>
to setup the folder where test files are handled edit TEST_PATTERN_DIR inside test/CMakeFiles.txt</br>
<code>tests/unit_test -t test_internal_sort</code></br>
<code>tests/unit_test -t test_external_sort</code></br>

<h2>Big test pattern utility</h2>

To test in dept bigsort a big file to be sorted is needed.</br>
The exe genbigfile build two patterns of ~10GB (default) up to ~60GB specifing size by cmdline, one file is ordered and the other unordered to compare bigsort results.</br>
The pattern is build using a text binary sequence inside the block, therefore are possibile 4096^2 permutation of any block, therefore 4096^2 * block_size = 68.719.476.736 bytes as bound, but tt's easily extendible to produce any size by repeat patterns.</br> 
<code>tests/genbigfile path</code>


<h2>Example</h2>
</br>
Let's start generating a pattern of 2GB
<pre>
alex@wildcat:~/develop/bigsort/build_res$ ./tests/genbigfile --size 2 big2
Start genbigfile big2 of size:2 GB -- blocks:524288
16 Mbytes has been written
32 Mbytes has been written
48 Mbytes has been written
64 Mbytes has been written
...
...
2032 Mbytes has been written
big file have been done in 162480ms
2048 Mbytes has been written
</pre>

<pre>
alex@wildcat:~/develop/bigsort/build_res$ ls -l big2.\*
-rw-r--r-- 1 alex alex 2147483648 feb 11 00:50 big2.sorted
-rw-r--r-- 1 alex alex 2147483648 feb 11 00:50 big2.unsorted
</pre>

Now sort the unsorted pattern
<pre>
alex@wildcat:~/develop/bigsort/build_res$ ./bigsort big2.unsorted
bigsort lexicographic sort of 4K blocks.
Available memory for internal sort 888 Mb
file name big2.unsorted
</pre>

At the end is possible compare the results by diff or better by md5sum.

