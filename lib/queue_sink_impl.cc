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
			< index :: [0] > -- contains the index of the window
			< data :: [1,1024] > -- contains data followed by zeros
			< size :: [1025] > -- contains the size of the data in the data field
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

	/*
	Read from XML to get size information
	*/
	set_output_multiple(1024); // Guarantee inputs in multiples of 1024! **Would not be used with application that has varying packet size

	myfile.open("queue_sink.data"); // Dump information to file
	VERBOSE = true; // Dump information to Std::out

	queue = &shared_queue; // Set shared_ptr queue
	item_size = size; // Set size of individual item, not window size

	preserve = preserve_index; // Does index need to be preserved? -- Do we pull indexes from the stream tags, or regenerate them?

	index_of_window = 0; // Set window index -- only relevant if we are not preserving a previously-defined index (from stream tags)

	index_vector = new std::vector<float>(); // vector of indexes (floats) -- populated with indexes that we pull from stream tag

	left_over = 0; // Initialize left-over count to 0 (what's left in the window after a work() call)

	total_floats = 0;

	myfile << "Calling Queue_Sink Constructor\n";

}

/*
 * Destructor.
 */
queue_sink_impl::~queue_sink_impl()
{
	// delete any malloced structures
	// Do I have anything mallocd (that I want to delete)?
	myfile << "Calling Queue_Sink Destructor\n";
	myfile.close();
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

		//Convert all tags to floats and add to tags_vector
		if(tags.size() > 0){

			for(int i = 0; i < tags.size(); i++){
				gr::tag_t temp_tag = tags.at(i);
				pmt::pmt_t temp_value = temp_tag.value;

				if(temp_value != NULL){

					float temp_index = (float)(pmt::to_long(temp_value));

					if(VERBOSE)
						std::cout << "Got tag=" << temp_index << std::endl;

					// After pushing tags into the index_vector, we can pull from here when constructing window segments
					index_vector->push_back(temp_index);

				}
			}
			tags.clear();
		}
	}

	// Determine how many windows worth of floats we have available
	number_of_windows = noutput_items / 1024; // determine number of windows we can make with floats
	left_over = noutput_items % 1024;

	if(VERBOSE)
		std::cout << "\t Number of floats=" << noutput_items << " Number of Windows=" << number_of_windows << "; left over=" << left_over << std::endl;

	// Build window segments, and push into queue
	for(int i = 0; i < number_of_windows; i++){

		window = new std::vector<float>(); // Create a new vector for window pointer to point at
		window->push_back(get_index());

		// Put next 1024 floats into the window
		window->insert(window->end(), &in[0], &in[1024]);

		in += 1024; // Update pointer to move 1024 samples in the future (next window)

		window->push_back(1024); // Append the number of floats in the segment

		total_floats += 1024;

		// Grab index and data size
		int data_size = window->at(1025);
		int index = window ->at(0);

		// Write segment information to file.
		myfile << "index=" << index << ": size=" << data_size << ": ";
		for(int z = 1; z<= data_size; z++)
			myfile << window->at(z) << " ";
		myfile << "\n";

		// Keep trying to push segment into queue until successful
		bool success = false;
		do{
			success = queue->push(window);
		} while(!success);// Push window reference into queue

		// NULL pointer to segment and incremement queue_counter
		window = NULL;
		queue_counter++;
	}

	return number_of_windows*1024;

/*
	if(left_over > 0){
		window = new std::vector<float>();
		window->push_back(get_index());

		// Put next left_over into the window
		window->insert(window->end(), &in[0], &in[left_over]);
		int window_size = window->size();
		in += left_over;

		total_floats += left_over;

		for(int i = window_size; i < 1025; i++)
			window->push_back(0);

		window->push_back(window_size-1);

		int data_size = window->at(1025);
		int index = window ->at(0);

		myfile << "index=" << index << ": size=" << data_size << ": ";

		for(int i = 1; i<= data_size; i++)
			myfile << window->at(i) << " ";
		myfile << "\n";


		// Keep trying to push until successful
		bool success = false;
		do{
			success = queue->push(window);
		}while(!success);

		window = NULL;
		queue_counter++;
	}

	std::cout << "\t\t\t Total Floats: " << total_floats << std::endl;

	return noutput_items;
	*/
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
			if(VERBOSE)
				GR_LOG_WARN(d_logger, "**Looking to preserve index found in stream, but there are none!");
		}

		return index_of_window++;
	}
}
} /* namespace router */
} /* namespace gr */