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

			Format of type-1 Segments
		|
			float < type :: [0] > -- contains the message type
			float < index :: [1] > -- contains the index of the data segment
			float < size :: [2] > -- contains the size of the data in the data field to come next
			float < data :: [3, 770] > -- contains data followed by zeros
		|
 */

/*
	Important Note
		This code functions on groups of 768 float values. Any subsequent floats that cannot complete such a set are thrown away.
		This can be modified in the future.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "queue_source_impl.h"
#include <stdio.h>

#define BOOLEAN_STRING(b) ((b) ? "true":"false")
#define VERBOSE false	

namespace gr {
namespace router {

/*
	This is the compare function used by std::sort to sort the windows from low to high index.
	This is a critical function because ordering is necessary for correctness.
 */

bool order_window(const std::vector<float>* a, const std::vector<float>* b){
	return (a->at(1) < b->at(1));	
}

/*
	Public Constructor

	Need to change shared queue types to generic 'event' type. Need to preserve index across event handlers on child/root routers, but not for final
	source to sink block. Use ordering for the final source block; this will make sure that the windows are ordered prior to sending samples forward.
 */

queue_source::sptr
queue_source::make(int item_size, boost::lockfree::queue< std::vector<float>*, boost::lockfree::fixed_sized<true> > &shared_queue, bool preserve_index, bool order)
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

queue_source_impl::queue_source_impl(int size, boost::lockfree::queue< std::vector<float>*, boost::lockfree::fixed_sized<true> > &shared_queue, bool preserve_index, bool order_data)
: gr::sync_block("queue_source",
		gr::io_signature::make(0, 0, 0),
		gr::io_signature::make(1, 1, size)), queue(&shared_queue), item_size(size), preserve(preserve_index), order(order_data)
{

	set_output_multiple(768); // Guarantee outputs in multiples of 768!
	dead = false;

	if(VERBOSE)
		myfile.open("queue_source.data"); // Dump information to file

	global_index = 0; // Zero is the initial index used for ordering. All first Windows must be ordered from index 0

	if(VERBOSE){
		myfile << "Calling Queue_Source Constructor\n\n";
		myfile << "Arguments: size=" << size << " preserve_index=" << BOOLEAN_STRING(preserve_index) << " order_data=" << BOOLEAN_STRING(order_data) << std::endl;
		myfile << std::flush;
	}

	found_kill = false;
}

/*
 * Our virtual destructor.
 */
queue_source_impl::~queue_source_impl()
{
	// Any alloc'd structures I need to delete?

	if(VERBOSE){
		std::cout << "*Calling Queue_Source Destructor*" << std::endl;
		myfile << "Calling Queue_Source Destructor\n";
		myfile << std::flush;
		myfile.close();
	}
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
	std::vector<float> buffer; // A temporary buffer; might not be necessary

	float index;
	float data_size;

	// Pop next value off of shared queue if there is one available


		if(queue->pop(temp_vector)){

			//std::cout << "Popped!" << std::endl;

			int type = (int)temp_vector->at(0);

			switch(type){
				case 1:
					index = temp_vector->at(1);
					data_size = (int)temp_vector->at(2);

					if(order){
						local.push_back(temp_vector);
						std::sort(local.begin(), local.end(), order_window);

						int temp_counter = 0;

						while((local.size() > 0) && ((int)(local.at(0)->at(1)) == global_index)){
							if(VERBOSE)
								myfile << "Got the next window; index=" << global_index << std::endl;

							buffer.insert(buffer.end(), local.front()->begin()+3, local.front()->end()); // Copy the contents of the temp_vector to the buffer minus the type, index, size

							local.erase(local.begin()); // Remove the pointer from the local vector

							//If we want to preserve index, write an index stream tag
							if(preserve){

								const size_t item_index = 0; //Let the first item in the stream contain the index tag
								const uint64_t offset = this->nitems_written(0) + item_index; // Determine offset from first element in stream where tag will be placed
								pmt::pmt_t key = pmt::string_to_symbol("i"); // Key associated with the index

								// Have to cast index to long (pmt does not handle floats)
								pmt::pmt_t value = pmt::from_long((long)global_index);

								//write a tag to output port 0 with given absolute item offset
								gr::tag_t temp_tag;
								temp_tag.key = key;
								temp_tag.value = value;
								temp_tag.offset = offset;

								this->add_item_tag(0, temp_tag); // write <index> to stream at location stream = 0+offset with key = key

								if(VERBOSE)
									myfile << "Writing stream tag: (key=i, offset=" << offset << ", value=" << index << "\n" << std::flush;
							}

							global_index++;
							temp_counter++;

						}

						if(temp_counter > 0){
							memcpy(out, &(buffer.data()[0]), sizeof(float)*data_size*temp_counter);
							return temp_counter*data_size;
						}
						else{
							if(VERBOSE)
								myfile << "Looking for: " << global_index << " but our lowest index is: " << local.at(0)->at(0) << "\n" << std::flush;
							return 0;
						}

					}
					else{
						if(VERBOSE)
							myfile << "Queue Source Memcpy size=" << sizeof(float)*data_size << std::endl;

						memcpy(out, &(temp_vector->at(3)), sizeof(float)*data_size);
						//out += data_size;

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

							if(VERBOSE)
								myfile << "Writing stream tag: (key=i, offset=" << offset << ", value=" << index << "\n" << std::flush;

						}

						//std::cout << "Popped [" << type << "], [" << index << "], [" << data_size << "]" << std::endl;


						delete temp_vector;

						return data_size;

					}	
					break;

				case 2:
					std::cout << "ERROR: We don't expect type-2 messages" << std::endl;
					break;

				case 3:
					dead = true;
					if(local.size() == 0){
						return -1;
					}
					else{
						if(VERBOSE)
							myfile << "ERROR: Got the kill msg, but there's still stuff in local! No good" << std::endl;
						return -1;
					}
					break;
				default:
					return 0;
			}
		}
		else{
			boost::this_thread::sleep(boost::posix_time::microseconds(0.01)); // Arbitrary sleep time
			//std::cout << "Failed to pop :(" << std::endl;
			return 0;
		}

}

} /* namespace router */
} /* namespace gr */