data

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
[s,t,z] [a,b,c] [i,l,r] [d,e,m] => a
[s,t,z] [c] [i,l,r] [d,e,m] => b
[s,t,z] [] [i,l,r] [d,e,m] => c
[s,t,z] [] [i,l,r] [d,e,m] => d
[s,t,z] [] [i,l,r] [m] => e
[s,t,z] [] [l,r] [m] => i
[s,t,z] [] [r] [m] => l
[s,t,z] [] [r] [] => m
[s,t,z] [] [] [] => r
[t,z] [] [] [] => s
[z] [] [] [] => t
[] [] [] [] => z

the algo is not optimal because doesn't use all the available memory to get min. value at each step.
