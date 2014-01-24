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

#ifndef INCLUDED_ROUTER_ROOT_IMPL_H
#define INCLUDED_ROUTER_ROOT_IMPL_H

#include "NetworkInterface.h"
#include <router/root.h>
#include <memory>
#include <boost/lockfree/queue.hpp>
#include <boost/thread.hpp>
#include <vector>
#include <fstream>


 namespace gr {
 	namespace router {

 		class root_impl : public root
 		{
 		private:

 			bool VERBOSE; // VERBOSITY flag
 			std::ofstream myfile; // Output file stream for debugging

 			int number_of_children;	// Set the number of children to listen for
 			bool d_finished; // variable for destruction (kill threads)

			// Shared pointer to queues (input, output) and counters (implemented later)
 			boost::lockfree::queue< std::vector<float>* > *in_queue;
 			float in_queue_counter;

 			boost::lockfree::queue< std::vector<float>* > *out_queue;
 			float out_queue_counter;


 			int global_counter;
 			boost::mutex global_lock;

 			boost::mutex file_lock;

 			boost::mutex out_queue_lock;

			// Vector to send
 			boost::shared_ptr< boost::thread > send_thread;

			// Vector of threads (for receiving)
 			std::vector<boost::shared_ptr< boost::thread > > thread_vector;

			// Weights for each child
 			float * weights;

			// Connector used for networking between nodes
 			NetworkInterface *connector;

 			// Keep track of floats and count for window segments
 			int total_floats, number_of_windows, left_over_values;

			// window buffer (used for sending/receiving between children)
 			std::vector<float> window;

			// Thread program for sending messages to children
 			void send();

			// Thread program for receiving for each index
 			void receive(int index);

			// Determine index of min child
 			int min();

			// Compare function for SORT (may need to update to heap for speed)
 			bool compare_by_index(const std::vector<float> &a, const std::vector<float> &b);

			// Global counter increment/decrement functions
 			void increment();
 			void decrement();

 		public:
 			root_impl(int number_of_children, boost::lockfree::queue< std::vector<float>* > &in_queue, boost::lockfree::queue< std::vector<float>* > &out_queue);
 			~root_impl();

      		// Where all the action really happens
 			int work(int noutput_items, 
 				gr_vector_const_void_star &input_items,
 				gr_vector_void_star &output_items);
 		};

  } // namespace router
} // namespace gr

#endif /* INCLUDED_ROUTER_ROOT_IMPL_H */

