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
#include "root_impl.h"

#define PARENT_INDEX	0

 namespace gr {
 	namespace router {

        // This is the public shared-pointer constructor; it is called by the Scheduler at creation time
 		root::sptr
 		root::make(int number_of_children, boost::shared_ptr< boost::lockfree::queue< std::vector<float>* > > input_queue, boost::shared_ptr< boost::lockfree::queue< std::vector<float>* > > output_queue)
 		{
 			return gnuradio::get_initial_sptr (new root_impl(number_of_children, input_queue, output_queue));
 		}

        /*
        * The private constructor
        * 1 inputs: 1 from source
        * 1 outputs: 1 to sink
        */
        root_impl::root_impl(int numberofchildren, boost::shared_ptr< boost::lockfree::queue< std::vector<float>* > > input_queue, boost::shared_ptr< boost::lockfree::queue< std::vector<float>* > > output_queue)
        : gr::sync_block("root",
     	gr::io_signature::make(0,0,0),
     	gr::io_signature::make(0,0,0))
        {

         	master_thread_index = 0;

    		// Set global counter; no need to lock -> no contention:wq
         	global_counter = 0;

            // Pointers to input/output queue of packets
    		in_queue = input_queue;
    		out_queue = output_queue;

            // Communication connector between nodes
    		connector =  new NetworkInterface(sizeof(float), number_of_children, 8080, true);

    	   	// Interconnect all blocks
    		connector->connect(NULL);

        	// Initialize counters for both queues to 0 (not sure we need this)
    		in_queue_counter = 0;
    		out_queue_counter = 0;

    	    // Number of children for root node
    		number_of_children = numberofchildren;

    	  	// Array of weights values for each child + local (index 0)
    		weights = new double[number_of_children];

    	   	// Finished flag for threads(true if finished)
    		d_finished = false;

    		// populate window vector with index 0 for first window
    		index_of_window = 0;

            //Thread for parent to send
            send_thread = boost::shared_ptr< boost::thread >(new boost::thread(boost::bind(&root_impl::send, this)));
            
            // Threads for parent to receive from all children
    		for(int i = 0; i < number_of_children; i++){
    			thread_vector.push_back(boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&root_impl::receive, this), i)));
    		}
	   }
        /*
         * Our virtual destructor.
         */
         root_impl::~root_impl()
         {

            d_finished = true;

            send_thread->interrupt();
            send_thread->join();

         	for(int i = 0; i < number_of_children; i++){
         		thread_vector[i]->interrupt();
         		thread_vector[i]->join();
         	}	

         }

         // This function is required by the agreement we have with the Gnu Radio Scheduler, but
            // ... it's not required, because work() doesn't do anything
         int
         root_impl::work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
         {
         	return noutput_items;
         }



        // To work around trying to pass arguments to boost, I'm going to have all threads grab their index from a master index repo
        int root_impl::get_index(){
     	  int ret;

     	  index_lock.lock();
     	  ret = master_thread_index++;
     	  index_lock.unlock();

     	  return ret;
        }

        // Sender thread; It's sole purpose is to send windows when available
        void root_impl::send(){

     	  float * buffer = new float[1025];
     	  std::vector<float> *temp;
     	  int packet_size;

     	  // Until the program exits, continue sending
     	  while(!d_finished){
     	
     		// Grab index of next target node
     		int index = get_index();

     		// If there is a window available, send it to indexed node
     		if(in_queue->pop(temp)){
     			packet_size = 1025 * 4; // (float = 4 bytes) * 1025 floats = 1024 size window + 1 for index
     			connector->send(index, (char*)(temp->data()), packet_size);
     			increment(); // incremenet global counter (how many windows are out there?)
     		}
     	  }
        }

        // Receiver thread; one per child node (instead of sequentially iterating through all nodes)
        void root_impl::receive(int index){
     	
     	  int size = 0;
     	  float * buffer = new float[1025];
     	  std::vector<float> *temp;

     	  // Until the thread is finished
     	  while(!d_finished){

            // Wait until there's something to receive
     		while(size < 1)
     			size = connector->receive(index, (char*) buffer, (1025*4));

     		// Received weight (need to figure out how else to differentiate)
     		if(size == 8){
     			int i = (int)buffer[0];

     			// We received valid 'weight' packet
				if((i > 0) && (i < number_of_children)){
                    // Update weights table
					weights[index] = buffer[1];
				}
				else{
					printf("ERROR: Received broken packet\n");
				}
     		}

     		// Received window
     		if(size > (1025*4)){
     			std::vector<float> *arrival;
				arrival->assign(((float*)buffer), ((float*)buffer)+1025);
				out_queue->push(arrival);
				decrement(); // One fewer window is out with children
     		}
     	}

     	delete [] buffer; // We're done with our buffer
     }

    	// Find index of child with minimum weight BIG_OH(N)
    	// Might want to use a better algorithm for this
    	// Configurable based on application
		int root_impl::min(){
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

    // Comparison function to sort windows by index
  	// Compare vector[0] to determine vector with smallest index
  	//bool root_impl::compare_by_index(const vector<float> &a, const vector<float> &b){
  	//	return a[0] < b[0];
  	//}	

	// These functions are intended for a multi-threaded implementation; locking is not necessary with a single thread

	//Decrement global counter
		void root_impl::decrement(){
			global_lock.lock();
			global_counter--;
			global_lock.unlock();
		}

	// Increment global counter
		void root_impl::increment(){
			global_lock.lock();
			global_counter++;
			global_lock.unlock();
		}
    } /* namespace router */
} /* namespace gr */