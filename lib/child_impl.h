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

#ifndef INCLUDED_ROUTER_CHILD_IMPL_H
#define INCLUDED_ROUTER_CHILD_IMPL_H

#include "NetworkInterface.h"
#include <router/child.h>
#include <memory>
#include <boost/lockfree/queue.hpp>
#include <boost/thread.hpp>
#include <vector>
#include <fstream>

namespace gr {
  namespace router {

    class child_impl : public child
    {
    private:
	
    	bool VERBOSE; // VERBOSITY flag
    	std::ofstream myfile; // Output file stream for debugging

		int master_thread_index;
		boost::mutex index_lock;

		int child_index;

		int number_of_children;
		bool d_finished;
		char * parent_hostname;

		// Queues used to read from and write to
		boost::lockfree::queue< std::vector<float>* > *in_queue;
		float in_queue_counter;

		boost::lockfree::queue< std::vector<float>* > *out_queue;
		float out_queue_counter;

		int global_counter;
		boost::mutex global_lock;


		// Threads for receiving and sending to Root (parent) Router
		boost::shared_ptr< boost::thread > d_thread_receive_root;
		boost::shared_ptr< boost::thread > d_thread_send_root;

		// Weights for each child (not used yet)
		float * weights;

		// Connectr used for networking between nodes
		NetworkInterface *connector;

		// Thread programs
		void receive_root(); // Receive messages from root
		void send_root(); // Send messages to root

		// This is not implemented yet
		void receive_child(int index);
		void send_child(int index);

		// Determine index of min child
		int min();

		// Global counter increment/decrement functions
		void increment();
		void decrement();

		// Return global counter value
		int get_weight();

    public:
     	child_impl(int number_of_children, int child_index, char* hostname, boost::lockfree::queue< std::vector<float>* > &in_queue, boost::lockfree::queue< std::vector<float>* > &out_queue);
      	~child_impl();

     	// Where all the action really happens
    	int work(int noutput_items,
	       gr_vector_const_void_star &input_items,
	       gr_vector_void_star &output_items);
    };

  } // namespace router
} // namespace gr

#endif /* INCLUDED_ROUTER_CHILD_IMPL_H */

