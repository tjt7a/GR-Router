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

 /*
			Format of Segments
		|
			< type :: [0] > -- contains the message type
			< index :: [1] > -- contains the index of the window
			< size :: [2] > -- contains the size of the data in the data field to come next
			< data :: [3,1026] > -- contains data followed by zeros
		|
 */

/*
	Important Note
		This code functions on groups of 1024 float values. Any subsequent floats that cannot complete such a set are thrown away.
		This can be modified in the future.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "queue_sink_impl.h"
#include <stdio.h>

#define BOOLEAN_STRING(b) ((b) ? "true":"false")

#define VERBOSE false	

namespace gr {
namespace router {

/*
 *	For LTE: Need to change shared queues to a generic 'events' type.
 *	For the last queue_sink in the system (on root router), index will need to be preserved.
 */

queue_sink::sptr
queue_sink::make(int item_size, boost::lockfree::queue< std::vector<float>*, boost::lockfree::fixed_sized<true> > &shared_queue, bool preserve_index)
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
queue_sink_impl::queue_sink_impl(int size, boost::lockfree::queue< std::vector<float>*, boost::lockfree::fixed_sized<true> > &shared_queue, bool preserve_index)
: gr::sync_block("queue_sink",
		gr::io_signature::make(1, 1, sizeof(float)),
		gr::io_signature::make(0, 0, 0)), queue(&shared_queue), item_size(size), preserve(preserve_index), index_of_window(0), window(NULL)
{

	/*
	Read from XML to get size information
	*/
	set_output_multiple(1024); // Guarantee inputs in multiples of 1024! **Would not be used with application that has varying packet size**

	if(VERBOSE)
		myfile.open("queue_sink.data"); // Dump information to file	

	index_vector = new std::vector<float>(); // vector of indexes (floats) -- populated with indexes that we pull from stream tag

	if(VERBOSE){
		myfile << "Calling Queue_Sink Constructor\n";
		myfile << "Arguments: size=" << size << " preserve_index=" << BOOLEAN_STRING(preserve_index) << "\n\n" << std::flush;
	}
}

/*
 * Destructor.
 */
queue_sink_impl::~queue_sink_impl()
{

	delete index_vector;

	// Push kill messages for any blocks sourcing from the queue
	window = new std::vector<float>(); // Create a new vector for window pointer to point at
    window->push_back(3); // Append the type of the KILL message (3)

    while(!queue->bounded_push(window)){
   		boost::this_thread::sleep(boost::posix_time::microseconds(1)); // Arbitrary sleep time
    }

 	if(VERBOSE)
		myfile.close();

	delete window;
}

int
queue_sink_impl::work(int noutput_items,
		gr_vector_const_void_star &input_items,
		gr_vector_void_star &output_items)
{

	// Pointer to input
	const float *in = (const float *) input_items[0]; // Input float buffer pointer

	// Do we want to pull indexes from the stream tags and use those for window indexes?
	if(preserve){

		// Get the index of the current window
		const uint64_t nread = this->nitems_read(0); //number of items read on port 0 up until the start of this work function (index of first sample)
		const size_t ninput_items = noutput_items; //assumption for sync block, this can change

		pmt::pmt_t key = pmt::string_to_symbol("i"); // Filter on key (i is for index)

		//read all tags associated with port 0 for items in this work function
		this->get_tags_in_range(tags, 0, nread, nread + ninput_items, key);

		if(VERBOSE){
			myfile << "Grabing indexes from stream; got " << tags.size() << "\n";
			myfile << std::flush;
		}

		//Convert all tags to floats and add to tags_vector
		if(tags.size() > 0){

			for(int i = 0; i < tags.size(); i++){
				gr::tag_t temp_tag = tags.at(i);
				pmt::pmt_t temp_value = temp_tag.value;

				if(temp_value != NULL){

					float temp_index = (float)(pmt::to_long(temp_value));

					if(VERBOSE){
						myfile << "Got tag=" << temp_index << "\n";
						myfile << std::flush;
					}

					// After pushing tags into the index_vector, we can pull from here when constructing window segments
					index_vector->push_back(temp_index);

				}
				else{
					if(VERBOSE){
						myfile << "Got NULL tag\n";
						myfile << std::flush;
					}
				}
			}
			tags.clear();
		}
	}

	// Determine how many windows worth of floats we have available
	number_of_windows = noutput_items / 1024; // determine number of windows we can make with floats
	/*if(number_of_windows >= 1){
		number_of_windows = 1; // Only going to pass one window at a time
	}
	else{
		return 0;
	}*/

	//left_over = noutput_items % 1024; // In this case will always be 0

	if(VERBOSE)
		myfile << "\t Number of floats=" << noutput_items << " Number of Windows=" << number_of_windows << "; left over=" << left_over << std::endl << std::flush;

	// Build window segments, and push into queue
	
	for(int i = 0; i < number_of_windows; i++){

		window = new std::vector<float>(); // Create a new vector for window pointer to point at
		window->push_back(1); // push the type (1)
		window->push_back(get_index()); // push the index (get_index())

		// Put next 1024 floats into the window
		window->push_back(1024); // there are 1024 floats in this window
		window->insert(window->end(), &in[0], &in[1024]);

		//std::cout << "PUSHING WINDOW SIZE: " << window->size() << std::endl;

		in += 1024; // Update pointer to move 1024 samples in the future (next window)

		// Keep trying to push segment into queue until successful
		while(!queue->push(window)){
			boost::this_thread::sleep(boost::posix_time::microseconds(0.00001)); // Arbitrary sleep time
		}

		// NULL pointer to segment and incremement queue_counter
		window = NULL; // Not strictly necessary
		queue_counter++;
	}
	

/*
	// Build type-1 segment
	window = new std::vector<float>();
	window->push_back(1);
	window->push_back(get_index());
	window->push_back(1024);
	window->insert(window->end(), &in[0], &in[1024]);

	while(!queue->push(window))
		;

	window = NULL;
	queue_counter++;

*/
	return number_of_windows*1024;
}

float queue_sink_impl::get_index(){

	// If not preserving an index, start from 0 and incremement for every subsequent windows
	if(!preserve)
		return index_of_window++;

	// If we do want to preserve index, pull index from stream tags and return the next one!
	else{
		if(index_vector->size() > 0){
			index_of_window = index_vector->at(0);
			index_vector->erase(index_vector->begin());
		}

		else{
			// We're out of tags... so return one from [0, inf]
			if(VERBOSE){
				myfile << "Error: Looking for tag value, but can't find one!!\n";
				myfile << std::flush;
			}
		}

		return index_of_window++;
	}
}
} /* namespace router */
} /* namespace gr */