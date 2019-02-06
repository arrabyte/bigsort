#include "block.hh"

const int block_size = 4096;

void sort_blocks(blocks_vector &blocks)
{
    std::stable_sort(blocks.begin(),
        blocks.end(),
        [](const blocks_ptr& a, const blocks_ptr& b){
            return std::lexicographical_compare(a.get(), a.get() + block_size, b.get(), b.get() + block_size);
        });
}