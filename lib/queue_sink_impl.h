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

#ifndef INCLUDED_ROUTER_QUEUE_SINK_IMPL_H
#define INCLUDED_ROUTER_QUEUE_SINK_IMPL_H

#include <router/queue_sink.h>
#include <vector>
#include <boost/thread.hpp>
#include <algorithm>
#include <boost/lockfree/queue.hpp>
#include <memory>
#include <gnuradio/tagged_stream_block.h>

namespace gr {
namespace router {

class queue_sink_impl : public queue_sink
{
private:
	int total_floats; // Number of available floats
	int number_of_windows; // Number of windows we can fill with floats
	std::vector<gr::tag_t> tags; // Vector of tags pulled from stream

	bool VERBOSE;

	boost::lockfree::queue< std::vector<float>* > *queue; // Pointer to shared queue
	int queue_counter; // Counter for windows in queue
	int item_size;

	std::vector<float> *window; // Window buffer for building windows
	std::vector<float> *index_vector; // Vector of stream tags; used as indexes

	float index_of_window; // window indexing if not preserved from stream tags
	bool preserve; // Re-establish index from source?

	float get_index(); // Returns the next index

public:
	queue_sink_impl(int item_size, boost::lockfree::queue< std::vector<float>* > &shared_queue, bool preserve_index);
	~queue_sink_impl();

	// Where all the action really happens
	int work(int noutput_items,
			gr_vector_const_void_star &input_items,
			gr_vector_void_star &output_items);
};

} // namespace router
} // namespace gr

#endif /* INCLUDED_ROUTER_QUEUE_SINK_IMPL_H */
