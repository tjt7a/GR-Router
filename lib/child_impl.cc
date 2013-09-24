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
#include "child_impl.h"

 namespace gr {
 	namespace router {

 		child::sptr
 		child::make(int number_of_children, int child_index, char * hostname, boost::shared_ptr< boost::lockfree::queue< std::vector<float>* > > input_queue, boost::shared_ptr< boost::lockfree::queue< std::vector<float>* > > output_queue)
 		{
 			return gnuradio::get_initial_sptr (new child_impl(number_of_children, child_index, hostname, input_queue, output_queue));
 		}

    /*
     * The private constructor
     */
     child_impl::child_impl( int numberofchildren, int index, char * hostname, boost::shared_ptr< boost::lockfree::queue< std::vector<float>* > > input_queue, boost::shared_ptr< boost::lockfree::queue< std::vector<float>* > > output_queue)
     : gr::sync_block("child",
     	gr::io_signature::make(0, 0, 0),
     	gr::io_signature::make(0, 0, 0))
     {

		// Set index of the child
     	child_index = index;

		// Initialize global_counter for router 
     	global_counter = 0;


		in_queue = input_queue; // Assign queue pointers
		out_queue = output_queue;


		d_finished = false;
		number_of_children = numberofchildren;
		parent_hostname = hostname;
		
		weights = new double[number_of_children];

		// Index of current window
		index_of_window = 0;

		// Create a thread per child
		//for(int i = 0; i < number_of_children; i++){
		//	thread_vector.append(boost::shared_ptr<boost::thread>(new Boost::thread(boost::bind(&child_impl::run, this))));
		//}
		
		// Create single thread; no children
		d_thread = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&child_impl::run, this)));	
	}

    /*
     * Our virtual destructor.
     */
     child_impl::~child_impl()
     {
     	d_finished = true;
     	d_thread->interrupt();
     	d_thread->join();
     }

     int
     child_impl::work(int noutput_items,
     	gr_vector_const_void_star &input_items,
     	gr_vector_void_star &output_items)
     {
        // This block is completely asynchronous; therefore there is no need for 'work()'ing
     	return noutput_items;
     }

    // To work around trying to pass arguments to boost, I'm going to have all threads grab their index from a master index repo
     int child_impl::get_index(){
     	int ret;

     	index_lock.lock();
     	ret = master_thread_index++;
     	index_lock.unlock();

     	return ret;
     }

    // thread execution
    // Receive, send, send
     void child_impl::run(){ // Split into send() and receive()

     	float *buffer = new float[1025];
     	std::vector<float> *temp;
     	int run_index = get_index();

	// Until the thread is finished...
     	while(!d_finished){

     		int size = 0;

// Receive WINDOW OR WEIGHT
		// Spin wait; might want to do an interrupt handler here	
     		while(size < 1)
			size = connector->receive(-1, (char*)buffer, (4*1025)); // Receive window from parent; returns byte size


		// If we get a weight message
		if(size < 12){
			int index = (int)buffer[0];
			if((index >= 0) && (index < number_of_children))
				weights[index] = buffer[1];
		}
		else{
			// If we recieve a window, push to in_queue
			if(size == (1025*4)){
				std::vector<float> *arrival;
				arrival->assign((float*)buffer, (float*)buffer + 1025);
				in_queue->push(arrival);
				increment(); // We have recevied another window!
			}
			else{
				printf("This is no good; we are receiving data of unexpected size: %d\n", size);
			}
		}		

// SEND WEIGHT
		temp->clear();
		temp->push_back((float)child_index);
		temp->push_back((float)get_weight());
		connector->send(-1, (char*)temp->data(), (2*4));


// SEND WINDOW OR WEIGHT

		if(out_queue->pop(temp)){ // If popping from out_queue was successful
			connector->send(-1, (char*)temp->data(), (1025*4)); // Send computed window to parent
			// Sent one of our windows; decrement window count
			decrement();
		}
		else{ // If we don't have a window ready to send to parent, just send weight
			temp->clear();
		temp->push_back((float)child_index);
		temp->push_back((float)get_weight());	
		connector->send(-1, (char*)temp->data(), (2*4));	
	}
// SEND WEIGHT
	temp->clear();
	temp->push_back((float)child_index);
	temp->push_back((float)get_weight());
	connector->send(-1, (char*)temp->data(), (2*4));

	size = 0;
	while(size < 1)
		size = connector->receive(-1, (char*)buffer, (1025*4));

		// Received weight
	if(size < 12){
		int index = (int)buffer[0];
		if(index < number_of_children)
			weights[index] = buffer[1];
	}
	else{
		printf("Received something unexepected of size: %d\n", size);
	}
}
	delete [] buffer; // We're done with our buffer
}

    // Determine child with minimum weight
int child_impl::min(){
	int min = weights[0];
	int index = 0;
	for(int i = 1; i < number_of_children; i++){
		if(weights[i] < min){
			min = weights[i];
			index = i;
		}
	}
	return index;
}

    // Calculate weight of subtree
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