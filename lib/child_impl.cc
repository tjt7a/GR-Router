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
#include "child_impl.h"

 namespace gr {
 	namespace router {

 		// Return a shared pointer to the Scheduler; this public constructor calls the private constructor
 		child::sptr
 		child::make(int number_of_children, int child_index, char * hostname, boost::lockfree::queue< std::vector<float>* > &input_queue, boost::lockfree::queue< std::vector<float>* > &output_queue)
 		{
 			return gnuradio::get_initial_sptr (new child_impl(number_of_children, child_index, hostname, input_queue, output_queue));
 		}

    /*
     * The private constructor
     */
     child_impl::child_impl( int numberofchildren, int index, char * hostname, boost::lockfree::queue< std::vector<float>* > &input_queue, boost::lockfree::queue< std::vector<float>* > &output_queue)
     : gr::sync_block("child",
     	gr::io_signature::make(0, 0, 0),
     	gr::io_signature::make(0, 0, 0))
     {

          GR_LOG_INFO(d_logger, "Calling private Child constructor");
		// Set index of the child
     	child_index = index;

		// Initialize global_counter for router 
     	global_counter = 0;

     	// These queues serve as the input/output queues for packetized data
     	// Assign queue pointers
		in_queue = &input_queue; 
		out_queue = &output_queue;

          // Connect to Localhost
          connector = new NetworkInterface(sizeof(float), 0, 8080, false);
          connector->connect(hostname);

          std::cout << "Finished connecting to hostname=" << hostname << std::endl;

		// Used to kill all threads
		d_finished = false;

		number_of_children = numberofchildren; // How many children does this node have?
		parent_hostname = hostname; // What is the hostname of this child's parent?
		
		// Weights table to keep track of the 'business' of child nodes
		weights = new float[number_of_children];

		// Create a thread per child for listeners (future work)
		//for(int i = 0; i < number_of_children; i++){
		//	thread_vector.append(boost::shared_ptr<boost::thread>(new Boost::thread(boost::bind(&child_impl::run, this))));
		//}
		
		// Create single thread for sending windows back to root
		d_thread_send_root = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&child_impl::send_root, this)));

          GR_LOG_INFO(d_logger, "Finished making d_thread_send_root");

		// Create single thread for receiving messages from root
		d_thread_receive_root = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&child_impl::receive_root, this)));	
	
          GR_LOG_INFO(d_logger, "Finished making d_thread_receive_root");
     }

    /*
     * Our virtual destructor.
     */
     child_impl::~child_impl()
     {
     	d_finished = true;

     	// Kill send thread
     	d_thread_send_root->interrupt();
     	d_thread_send_root->join();

     	// Kill receive thread
     	d_thread_receive_root->interrupt();
     	d_thread_receive_root->join();
     }

     // This is the work function sthat is called by the scheduler
     // Because this block is completely asynchronous, this function is of no importance
     int
     child_impl::work(int noutput_items,
     	gr_vector_const_void_star &input_items,
     	gr_vector_void_star &output_items)
     {
        // This block is completely asynchronous; therefore there is no need for 'work()'ing
     	return noutput_items;
     }


     // Thread receive function receives from root
     void child_impl::receive_root(){

     	float *buffer = new float[1025];

     	while(!d_finished){

     		int size = 0;

     		// Spin wait; might want to change this later to interrupt handler
     		while(size < 1)
     			size = connector->receive(-1, (char*)buffer, (4*1025));

               std::cout << "\t\t\t\tChild received packet of size=" << size << std::endl;

               // Given this application; this should not happen
     		if(size == (2*4)){
                    std:: cout << "We are in the CHILD and getting weights... not good" << std::endl;
     			int index = (int)buffer[0];
     			if((index >= 0) && (index < number_of_children))
     				weights[index] = buffer[1];
     		}
     		else{
     			//If we receive a packet, push to in_queue
     			if(size == (1025 * 4)){
     				std::vector<float> *arrival;
     				arrival->assign((float*)buffer, (float*)buffer + 1025);


                         bool pushed = false;
                         pushed = in_queue->push(arrival);
     				while(!pushed)
                              pushed = in_queue->push(arrival);

     				increment();
     			}
     			else{
     				std::cout << "This is no good; we are receiving data of unexpected size: " << size << std::endl;     		
                    }

               }
     	}
     }

     // Thread send function
     void child_impl::send_root(){
     	std::vector<float> *temp;

     	while(!d_finished){

     		// If we have windows, send window
     		if(out_queue->pop(temp)){
     			connector->send(-1, (char*)temp->data(), (1025*4));
     			decrement();

     			// Send weight
                    delete temp;
     			//temp->clear();

     			temp->push_back((float)child_index);
     			temp->push_back((float)get_weight());
     			connector->send(-1, (char*)temp->data(), (2*4));
     		}
     	}
     }

     // Thread receive function receives from each child
     void child_impl::receive_child(int index){
     	// Recieve from children
     	// To be implemented
     }

     // Thread send function sends to all children
     void child_impl::send_child(int index){
     	// Send to children
     	// To be implemented
     }

    // Determine child with minimum weight (big_o(n)) -- not really a big deal with few children
    // Determine better solution
	int child_impl::min(){
		float min = weights[0];
		int index = 0;
		for(int i = 1; i < number_of_children; i++){
			if(weights[i] < min){
				min = weights[i];
				index = i;
			}
		}
		return index;
	}

    // Calculate weight of subtree (1-deep star architecture) -- how many packets are out for computation at the current node?
	inline int child_impl::get_weight(){

		//For simple application with no sub-trees, simply return outstandng windows
		return global_counter;	
	}

	// These functions are intended for a multi-threaded implementation; locking is not necessary with a single thread

    // Decrement global counter
	inline void child_impl::decrement(){
		global_lock.lock();
		global_counter--;
		global_lock.unlock();
	}

    // Increment global counter
	inline  void child_impl::increment(){
		global_lock.lock();
		global_counter++;
		global_lock.unlock();
	}
  } /* namespace router */
} /* namespace gr */