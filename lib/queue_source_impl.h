/* -*- c++ -*- */
/* Written by Tommy Tracy II (University of Virginia HPLP)
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifndef INCLUDED_ROUTER_QUEUE_SOURCE_IMPL_H
#define INCLUDED_ROUTER_QUEUE_SOURCE_IMPL_H

#include <router/queue_source.h>
#include <vector>
#include <boost/thread.hpp>
#include <algorithm>
#include <boost/lockfree/queue.hpp>
#include <memory>
#include <gnuradio/tagged_stream_block.h>
#include <iostream>
#include <fstream>

namespace gr {
namespace router {

class queue_source_impl : public queue_source
{
private:

	std::ofstream myfile; // output file stream

	bool dead;

	int number_of_windows; // Number of Windows we can construct from available samples
	int left_over_values; // Values left after filling Windows
	int global_index; // Current Index to maintain ordering

	bool found_kill; // Received kill message


	bool order; // Do we need to enforce ordering of leaving Windows' data?
	std::vector<std::vector<float>* > local; // Local vector for ordering

	//boost::shared_ptr< boost::lockfree::queue< std::vector<float>* > > *queue; // shared pointer to queue of windows
	boost::lockfree::queue< std::vector<float>*, boost::lockfree::fixed_sized<true> > *queue;

	// Right now everything is Floats, but future versions need to support any data type
	int item_size; // size of items to be windowd

	std::vector<float> window; // Window buffer
	bool preserve; // Preserve indexes across flow graph


public:
	//queue_source_impl(int size, boost::shared_ptr< boost::lockfree::queue< std::vector<float>* > > shared_queue, bool preserve_index, bool order);
	queue_source_impl(int size, boost::lockfree::queue< std::vector<float>*, boost::lockfree::fixed_sized<true> > &shared_queue, bool preserve_index, bool order);
	~queue_source_impl();

	// Where all the action really happens
	int work(int noutput_items,
			gr_vector_const_void_star &input_items,
			gr_vector_void_star &output_items);
};

} // namespace router
} // namespace gr

#endif /* INCLUDED_ROUTER_QUEUE_SOURCE_IMPL_H */
