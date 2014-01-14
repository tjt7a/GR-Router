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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "queue_source_impl.h"
#include <stdio.h>

namespace gr {
namespace router {

/*
	Public Constructor

	Need to change shared queue types to generic 'event' type. Need to preserve index across event handlers on child/root routers, but not for final
	source to sink block. Use ordering for the final source block; this will make sure that the windows are ordered prior to sending samples forward.
 */

/*
	This is the compare function used by std::sort to sort the windows from low to high index.
	This is a critical function because ordering is necessary for correctness.
 */
bool order_window(const std::vector<float>* a, const std::vector<float>* b){
	return (a->at(0) < b->at(0));
}

queue_source::sptr
queue_source::make(int item_size, boost::lockfree::queue< std::vector<float>* > &shared_queue, bool preserve_index, bool order)
{
	return gnuradio::get_initial_sptr (new queue_source_impl(item_size, shared_queue, preserve_index, order));
}

/*
 * The private constructor
 * size: size of the data type that is passed in flowgraph = sizeof(float) = 4
 * shared_queue: pointer to queue from which to read windows
 * preserve_index (boolean): Use stream tags to forward along window index
 * order_data (boolean): If we need to order the windows by index
 */

queue_source_impl::queue_source_impl(int size, boost::lockfree::queue< std::vector<float>* > &shared_queue, bool preserve_index, bool order_data)
: gr::sync_block("queue_source",
		gr::io_signature::make(0, 0, 0),
		gr::io_signature::make(1, 1, sizeof(float)))
{

	VERBOSE = true; // Dump information to Std::out
	myfile.open("queue_source.data"); // Dump information to file

	if(VERBOSE)
		std::cout << "Calling Queue_Sink Constructor" << std::endl;

	queue = &shared_queue;
	item_size = size; // Set size of samples (float)
	preserve = preserve_index; // Does index need to be re-established?
	order = order_data; // Require that the windows be ordered before dumping their contents to *out
	global_index = 0; // Zero is the initial index used for ordering. All first Windows must be ordered from index 0
	
	set_output_multiple(1024); // Guarantee outputs in multiples of 1024!

	myfile << "Calling Queue_Source Constructor\n";
}

/*
 * Our virtual destructor.
 */
queue_source_impl::~queue_source_impl()
{
	// Any alloc'd structures I need to delete?
	if(VERBOSE)
		GR_LOG_INFO(d_logger, "*Calling Queue_Source Destructor*");

	myfile << "Calling Queue_Source Destructor\n";
	myfile.close();
}


/*
	The objective of the work function is to grab windows from the shared queue and dump their contents into the out memory location.
	If required, the windows are ordered prior to dumping their contents to maintain order across the flow graph.
	Also, if the index of the window is to be maintained, the indexes are shared via stream tags.
 */

int
queue_source_impl::work(int noutput_items,
		gr_vector_const_void_star &input_items,
		gr_vector_void_star &output_items)
{

	float *out = (float *) output_items[0]; // output float buffer pointer (where we're writing the floats to)
	
	std::vector<float> *temp_vector; // Temp vector pointer for popping vector pointers off of the shared queue
	float index; // Index of the window that the current vector pointer is pointing to
	std::vector<float> buffer; // A temporary buffer; might not be necessary


	// Pop next value off of shared queue if there is one available
	if(queue->pop(temp_vector)){

		index = temp_vector->at(0); // index of the current window
		data_size = temp_vector->at(1025); // Number of data samples in the segment

		total_floats += data_size;

		// Write segment information to file
		myfile << "index=" << index << ": size=" << data_size << ": ";
		for(int i = 1; i<= data_size; i++)
			myfile << temp_vector->at(i) << " ";
		myfile << "\n";

		// If we strictly care about the ordering of the Windows...
		if(order){

			// Add pointer to window to local vector for sorting
			local.push_back(temp_vector);

			// Use stl sort to sort vector
			std::sort(local.begin(), local.end(), order_window);

			//for(int i = 0; i < local.size(); i++){
			//	std::cout << local.at(i)->at(0) << std::endl;
			//}

			// If the window with the lowest index is present...
			if((int)((local.at(0))->at(0)) == (int)global_index){

				temp_vector = local.front();
				buffer.insert(buffer.end(), temp_vector->begin()+1, temp_vector->end()); // Copy the contents of the temp_vector to the buffer minus the index
				delete temp_vector; // Delete the window; don't need anymore
				local.erase(local.begin()); // Remove pointer from the local vector

				// If we want to preserve the window's index, write an index stream tag
				if(preserve){

					const size_t item_index = 0; //Let the first item in the stream contain the index tag
					const uint64_t offset = this->nitems_written(0) + item_index; // Determine offset from first element in stream where tag will be placed
					pmt::pmt_t key = pmt::string_to_symbol("i"); // Key associated with the index

					// Have to cast index to long (pmt does not handle floats)
					pmt::pmt_t value = pmt::from_long((long)global_index);

					//write a tag to output port 0 with given absolute item offset
					gr::tag_t temp_tag;
					temp_tag.key = key;
					temp_tag.value=value;
					temp_tag.offset = offset;

					this->add_item_tag(0, temp_tag); // write <index> to stream at location stream = 0+offset with key = key

				}

				memcpy(out, &(buffer[0]), sizeof(float)*1024);
				global_index++;
				return 1024;

			}
			else{
				if(VERBOSE)
					std::cout << "We are looking for: " << global_index << " but received: " << local.at(0)->at(0) << std::endl;
				return 0;
			}

		}
		else{

			// Insert segment samples from temp_vector to out buffer
			// Can combine these two into one
			buffer.insert(buffer.end(), (temp_vector->begin()+1), (temp_vector->begin()+data_size));
			memcpy(out, &(buffer[0]), sizeof(float) * data_size);

			if(preserve){

				const size_t item_index = 0; //Let the first item in the stream contain the index tag
				const uint64_t offset = this->nitems_written(0) + item_index; // Determine offset from first element in stream where tag will be placed
				pmt::pmt_t key = pmt::string_to_symbol("i"); // Key associated with the index

				// Have to cast index to long (pmt does not handle floats)
				pmt::pmt_t value = pmt::from_long((long)index);

				gr::tag_t temp_tag;
				temp_tag.key = key;
				temp_tag.value = value;
				temp_tag.offset = offset;

				//write a tag to output port 0 with given absolute item offset
				//this->add_item_tag(0, offset, key, value); // write <index> to stream at location stream = 0+offset with key = key
				this->add_item_tag(0, temp_tag);

			}

			if(VERBOSE)
				std::cout << "\t\tTOTAL RECEIVED: " << total_floats << std::endl;

			return data_size;
		}
	}

	// If there is nothing available yet, return 0
	else
		return 0;
}

} /* namespace router */
} /* namespace gr */