/*
 * Copyright (C) 2019 Alessandro Arrabito
 */

/*
 * This file is part of bigsort.
 *
 * bigsort is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bigsort is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bigsort.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "block.hh"

int const block_size(4096);

namespace datablock{

void sort_blocks(blocks_vector &blocks)
{
    std::stable_sort(blocks.begin(),
        blocks.end(),
        [](const blocks_ptr& a, const blocks_ptr& b){
            return std::lexicographical_compare(a.get(), a.get() + block_size, b.get(), b.get() + block_size);
        });
}

}