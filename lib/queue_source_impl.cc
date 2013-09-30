/* -*- c++ -*- */
/* 
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
#include "queue_source_impl.h"

namespace gr {
  namespace router {

    /*
	Public Constructor
    
	Need to change shared queue types to generic 'event' type. Need to preserve index across event handlers on child/root routers, but not for final
	source to sink block. Use ordering for the final source block; this will make sure that the windows are ordered prior to sending samples forward.

    */
    queue_source::sptr
    queue_source::make(int item_size, boost::shared_ptr< boost::lockfree::queue< std::vector<float>* > > shared_queue, boost::shared_ptr< boost::lockfree::queue<float> > indexes, bool preserve_index, bool order)
    {
      return gnuradio::get_initial_sptr (new queue_source_impl(item_size, shared_queue, indexes, preserve_index, order));
    }

    /*
     * The private constructor
     */
    queue_source_impl::queue_source_impl(int size, boost::shared_ptr< boost::lockfree::queue< std::vector<float>* > > shared_queue, boost::shared_ptr< boost::lockfree::queue<float> > index_queue, bool preserve_index, bool order_data)
      : gr::sync_block("queue_source",
		      gr::io_signature::make(0, 0, 0),
		      gr::io_signature::make(1, 1, sizeof(float)))
    {
      queue = shared_queue;
      
      indexes = index_queue; // Move this to stream tags!

      item_size = size;
      preserve = preserve_index; // Does index need to be re-established?
      order = order_data;
      global_index = 0; // Zero is the initial index used for ordering
    }

    /*
     * Our virtual destructor.
     */
    queue_source_impl::~queue_source_impl()
    {
    }


	/*
		This is the compare function used by std::sort to sort the windows from low to high index.
		This is a critical function because ordering is necessary for correctness.

	*/

     // Function used by std::sort to sort vector of Windows
     bool comp(const std::vector<float>* a, const std::vector<float>* b){
        return (a[0] >= b[0]);
     }


	/*
		The objective of the work function is to grab windows from the shared queue and dump their contents into the out memory location.
		If required, the windows are ordered prior to dumping their contents to maintain order across the flow graph.
		Also, if the index of the window is to be maintained, the indexes are pushed into a shared queue. This allows the queue sink to 
		reconstruct the windows with correct indexes after processing. 
	*/

    int
    queue_source_impl::work(int noutput_items,
			  gr_vector_const_void_star &input_items,
			  gr_vector_void_star &output_items)
    {

        float *out = (float *) output_items[0]; // output float buffer pointer

        std::vector<float> *temp_vector; // Temp vector pointer for popping vectors off of the shared queue
	int temp = 10; // Max number of windows we want to pop at once (arbitrary for now)
	std::vector<float> buffer;
	while(queue->pop(temp_vector)){
		float index = (*temp_vector)[0];
		local.push_back(temp_vector);
		
		// If we want to preserve the windows' index, we need to push the current window's index to the shared queue
		// The future queue sink block will grab the index and then reconstruct the window post-computation
		if(preserve){
			indexes->push(index);
		}
		if(temp == 0)
			break;
		else
			temp--;
	}

	// If stream must be in correct order...
	if(order){

		// Use heap and comp() to order by index	
		//std::make_heap(local.begin(), local.end(), comp);
	 	//std::sort_heap(local.begin(), local.end());

		// Used stl sort to sort vector
		std::sort(local.begin(), local.end(), &comp);		


		// Push continuous, ordered floats into buffer

		std::vector<float>* tmp;
		while((float)((*(local[0]))[0]) == global_index){
			tmp = local.back();
			local.pop_back();
			buffer.insert(buffer.end(), tmp->begin(), tmp->end());
		}
	}
	// If we don't need order, just add floats
	else{
		std::vector<float>* tmp;
		while(!local.empty()){
			tmp = local.back();
			local.pop_back();
			buffer.insert(buffer.end(), tmp->begin(), tmp->end());
		}
	}

        // Pull data out of window, and pass through flowgraph
        //std::copy(temp_vector->begin(), temp_vector->back(), out);
	out = (float *)&buffer[0];

        // Tell runtime system how many output items we produced.
        return buffer.size();
    }

  } /* namespace router */
} /* namespace gr */

