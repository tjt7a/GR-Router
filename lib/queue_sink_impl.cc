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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "queue_sink_impl.h"
#include <stdio.h>

namespace gr {
namespace router {

/*
 *	For LTE: Need to change shared queues to a generic 'events' type.
 *	For the last queue_sink in the system (on root router), index will need to be preserved.
 */

queue_sink::sptr
queue_sink::make(int item_size, boost::lockfree::queue< std::vector<float>* > &shared_queue, bool preserve_index)
{
	return gnuradio::get_initial_sptr (new queue_sink_impl(item_size, shared_queue, preserve_index));
}

/*
 * The private constructor
 * Arguments:
 * size: size of data type being accepted by queue sink = sizeof(float) = 4 bytes
 * shared_queue: pointer to lockfree queue to drop packets into
 * preserve_index: whether or not to preserve the index located in stream tag (if any)
 */
queue_sink_impl::queue_sink_impl(int size, boost::lockfree::queue< std::vector<float>* > &shared_queue, bool preserve_index)
: gr::sync_block("queue_sink",
		gr::io_signature::make(1, 1, sizeof(float)),
		gr::io_signature::make(0, 0, 0))
{
	set_output_multiple(1024); // Guarantee inputs in multiples of 1024!

	queue = &shared_queue; // Set shared_ptr queue
	item_size = size; // Set size of items
	preserve = preserve_index; // Does index need to be preserved?
	index_of_window = 0; // Set window index -- only relevant if we are not preserving a previously-defined index
	window = new std::vector<float>(); // Create a new vector for window pointer to point at
	index_vector = new std::vector<float>(); // vector of indexes (floats)
	if(VERBOSE)
		GR_LOG_INFO(d_logger, "*Calling Queue_Sink Constructor*");
}

/*
 * Destructor.
 */
queue_sink_impl::~queue_sink_impl()
{
	// delete any malloced structures
	// Do I have anything mallocd (that I want to delete)?
	GR_LOG_INFO(d_logger, "*Calling Queue_Sink Destructor*");
}

int
queue_sink_impl::work(int noutput_items,
		gr_vector_const_void_star &input_items,
		gr_vector_void_star &output_items)
{
	// Message buffer for VERBOSE messages
	char message_buffer[64];

	// Pointer to input
	const float *in = (const float *) input_items[0]; // Input float buffer pointer

	// Get the index of the current window
	const uint64_t nread = this->nitems_read(0); //number of items read on port 0 up until the start of this work function (index of first sample)
	const size_t ninput_items = noutput_items; //assumption for sync block, this can change

	// Do we want to pull indexes from the stream tags and use those for window indexes
	if(preserve){
		pmt::pmt_t key = pmt::string_to_symbol("index"); // Filter on key

		//read all tags associated with port 0 for items in this work function
		this->get_tags_in_range(tags, 0, nread, nread + ninput_items, key);

		//Convert all tags to floats and add to tags_vector
		if(tags.size() > 0){

			for(int i = 0; i < tags.size(); i++){
				gr::tag_t temp_tag = tags.at(0);
				pmt::pmt_t temp_value = temp_tag.value;

				if(temp_value != NULL){
					float temp_index = (float)(pmt::to_long(temp_value));
					index_vector->push_back(temp_index);

					if(VERBOSE){
						sprintf(message_buffer, "Got a tag!: %d", temp_index);
						GR_LOG_INFO(d_logger, message_buffer);
					}
				}
			}
			tags.clear();
		}
	}

	// Add index if window doesn't already have one; this is a new window (we cannot assume that our input always passes 1024 values)
	if(window->size() == 0){
		window->push_back(get_index());
	}

	// Determine how many floats we need to add to the window
	total_floats = window->size() + noutput_items - 1; // Determine number of floats available
	number_of_windows = total_floats / 1024; // determine number of windows we can make with floats


	if(number_of_windows > 0){

		// Put next 1024 floats into the window
		window->insert(window->end(), &in[0], &in[1024]);

		in += 1024; // Update pointer to move 1024 samples in the future (next window)

		// Keey trying to push until successful
		while(!queue->push(window)){
			queue->push(window);
		} // Push window reference into queue

		queue_counter++;
		number_of_windows--;

		window = new std::vector<float>(); // Create a new vector

		// Populate windows and push into queue
		for(int i = 0; i < number_of_windows; i++){

			// Get index
			window->push_back(get_index());

			if(VERBOSE){
				sprintf(message_buffer, "Pushing window with index: %f", window->at(0));
				GR_LOG_INFO(d_logger, message_buffer);
			}

			// Insert floats into the window
			window->insert(window->end(), &in[0], &in[1024]);

			// Keep trying to push window onto queue
			while(!queue->push(window)){
				queue->push(window);
			}

			queue_counter++;

			/*
          if(window_vector.size() == 1000){
            for(int i = 0; i < 1000; i++){
              for(int j = 0; j < 1025; j++){
                std::cout << window_vector.at(i).at(j);
              }
              std::cout << std::endl;
            }
          }
			 */

			window = new std::vector<float>();
			in += 1024;
		}

		// Put remaining floats in window array for next work execution
	}

	return noutput_items;
}

float queue_sink_impl::get_index(){

	// If not preserving an index, start from 0 and incremement for every subsequent windows
	if(!preserve)
		return index_of_window++;

	// If we do want to preserve index, pull index from stream tags and return the next one!
	else{
		if(index_vector->size() > 0){
			float temp_float = index_vector->at(0);
			index_vector->erase(index_vector->begin());
			return temp_float;
		}

		else{
			// We're out of tags... so return one from 0
			GR_LOG_WARN(d_logger, "**Looking to preserve index found in stream, but there are none!");
			return index_of_window++;
		}
	}

	return 0;
}
} /* namespace router */
} /* namespace gr */
