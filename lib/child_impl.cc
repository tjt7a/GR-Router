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
#define BOOLEAN_STRING(b) ((b) ? "true":"false")

#define VERBOSE     true

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
          if(VERBOSE) myfile.open("child_router.data");

		// Set index of the child
     	child_index = index;

		// Initialize global_counter for router 
     	global_counter = 0;

     	// These queues serve as the input/output queues for packetized data
     	// Assign queue pointers
		in_queue = &input_queue; 
		out_queue = &output_queue;

          // Connect to <hostname>

          if(VERBOSE)
               myfile << "Attempting to connect to parent\n";

          connector = new NetworkInterface(sizeof(float), 0, 8080, false);
          
          // Interconnect all blocks (hostname of Root)
          connector->connect(hostname);

          if(VERBOSE)
               myfile << "Connected to parent\n";
          
          if(VERBOSE)
               std::cout << "\tChild Router Finished connecting to hostname=" << hostname << std::endl;

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

		// Create single thread for receiving messages from root
		d_thread_receive_root = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&child_impl::receive_root, this)));	
	
          if(VERBOSE){
               file_lock.lock();
               myfile << "Calling Child Router Constructor v.2\n";
               myfile << "Arguments: number of children=" << numberofchildren << " index=" << index << " hostname= " << hostname << "\n\n" << std::flush;
               file_lock.unlock();
          }
     }

    /*
     * Our virtual destructor.
     */
     child_impl::~child_impl()
     {

          if(VERBOSE){
               std::cout << "Calling Child Router " << child_index << "'s destructor!" << std::endl;
               myfile << "Calling Child Router Destructor\n";
               myfile << std::flush;
          }
          
     	d_finished = true;

     	// Kill send thread
     	d_thread_send_root->interrupt();
     	d_thread_send_root->join();

     	// Kill receive thread
     	d_thread_receive_root->interrupt();
     	d_thread_receive_root->join();

          delete connector;
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

          float * temp_buffer = new float[1];
     	float * buffer;
          bool success = false;
          std::vector<float> *arrival;
          int size;

     	while(!d_finished){

     		// Spin wait; might want to change this later to interrupt handler
               size = 0;
               while(size < 1){
                    size += connector->receive(-1, (char*)&(temp_buffer[size]), (1-size));
                    if(size == 0 && d_finished){
                         delete[] temp_buffer;
                         return;
                    }
               }

               int packet_type = (int)temp_buffer[0];

               switch(packet_type){
                    case 1:
                         buffer = new float[1027];
                         size = 0;
                         while(size < 1026)
                              size += connector->receive(-1, (char*)&(buffer[size]), (1026-size)); // Receive the rest of the segment
                         
                         arrival = new std::vector<float>();
                         arrival->push_back(1);
                         arrival->insert(arrival->end(), &buffer[0], &buffer[1026]);

                         if(VERBOSE)
                              myfile << "Got a window segment : index=" << arrival->at(1) << std::endl;

                         success = false;
                         do{
                              success = in_queue->push(arrival);
                         }while(!success);

                         increment();

                         break;
                    case 2:
                         std::cout << "ERROR: Right now we're not supporting this format" << std::endl;
                         break;
                    case 3:
                         arrival = new std::vector<float>();
                         arrival->push_back(3);

                         success = false;
                         do{
                              success = in_queue->push(arrival);
                         }while(!success);
                         break;
                    default:
                         myfile << "ERROR: Got a message of unexpected type" << std::endl;

               }
     	}
     }

     // Thread send function
     void child_impl::send_root(){

          std::vector<float> *temp; // Pointer to current vector of floats to be sen
          int sent = 0;
          int packet_size = 0;
          int packet_type = 0;

     	while(!d_finished){

     		// If we have windows, send window

     		if(out_queue->pop(temp)){

                    packet_type = (int)temp->at(0); // Get the packet type

                    //Switch on the packet_type
                    switch(packet_type){
                         case 1:
                              if(VERBOSE){
                                   std::cout << "We have " << global_counter << "messages in the queue right now" << std::endl;
                                   myfile << "Popped and sending packet index=" << temp->at(1) << " to parent with index=" << temp->at(1) << std::endl;
                              }

                              packet_size = (temp->at(2) + 3); // The size of the segment to be sent

                              // Shove on a weight value, and make it a type-2 message
                              temp->push_back(get_weight());
                              temp->at(0) = 2;

                              packet_size++;

                              sent = 0;
                              while(sent < packet_size)
                                   sent += connector->send(-1, (char*)&((temp->data())[sent]), (packet_size-sent)); // *4

                              decrement();
                              delete temp;
                              break;
                         case 2:
                              break;
                         case 3: // Got a kill message
                              if(VERBOSE)
                                   myfile << "Got a kill message \n" << std::flush;

                              packet_size = 1;

                              sent = 0;
                              while(sent < packet_size)
                                   sent += connector->send(-1, (char*)&((temp->data())[sent]), (packet_size-sent)); 

                              d_finished = true;
                              return;
                              break;
                    }
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