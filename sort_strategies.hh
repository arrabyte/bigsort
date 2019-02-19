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

#pragma once

#include <seastar/core/sstring.hh>
#include <seastar/core/future.hh>
#include "block.hh"

namespace sort_algorithm {

seastar::future<> external_sort(seastar::sstring root_filename, int files_count);

}
