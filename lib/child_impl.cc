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
          VERBOSE = true; // Used to dump useful information
          myfile.open("child_router.data");

		// Set index of the child
     	child_index = index;

		// Initialize global_counter for router 
     	global_counter = 0;

     	// These queues serve as the input/output queues for packetized data
     	// Assign queue pointers
		in_queue = &input_queue; 
		out_queue = &output_queue;

          // Connect to <hostname>

          file_lock.lock();
          myfile << "Attempting to connect to parent\n";
          file_lock.unlock();

          connector = new NetworkInterface(sizeof(float), 0, 8080, false);
          
          // Interconnect all blocks (hostname of Root)
          connector->connect(hostname);

          file_lock.lock();
          myfile << "Connected to parent\n";
          file_lock.unlock();

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
	
          file_lock.lock();
          myfile << "Calling Child Router Constructor\n";
          myfile << "Arguments: number of children=" << numberofchildren << " index=" << index << " hostname= " << hostname << "\n\n";
          myfile << std::flush;
          file_lock.unlock();
     }

    /*
     * Our virtual destructor.
     */
     child_impl::~child_impl()
     {


          if(VERBOSE)
               std::cout << "Calling Child Router " << child_index << "'s destructor!" << std::endl;

          file_lock.lock();
          myfile << "Calling Child Router Destructor\n";
          myfile << std::flush;
          file_lock.unlock();

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

          float * size_buffer = new float[2];
     	float * buffer;
          int size_of_message_1;
          int size_of_message_2;
          std::vector<float> *arrival;
          int size;

     	while(!d_finished){

     		// Spin wait; might want to change this later to interrupt handler
               size = 0;
               while(size < 2)
                    size += connector->receive(-1, (char*)&(size_buffer[size]), (2-size));

               size_of_message_1 = (int)size_buffer[0];
               size_of_message_2 = (int)size_buffer[1];

               file_lock.lock();
               myfile << "Size message: Got: "<< size << " what we expected: " << "2\n";
               myfile << std::flush;
               file_lock.unlock();

               if(size != 2){
                    file_lock.lock();
                    myfile << "ERROR: Expecting size message!\n";
                    myfile << std::flush;
                    for(int i = 0; i< size; i++)
                         myfile << size_buffer[i] << " ";
                    myfile << "\n";
                    file_lock.unlock();

                    return;
               }

               if(size_of_message_1 != -1){
                    file_lock.lock();
                    myfile << "ERROR: Child did not received size message\n";
                    myfile << std::flush;
                    file_lock.unlock();
                    buffer = new float[1026];
                    size_of_message_2 = 1026;
                    return;
               }
               else{
                    buffer = new float[size_of_message_2];
               }

               // Receive data message
               size = 0;
               while(size < size_of_message_2)
     		   size += connector->receive(-1, (char*)&(buffer[size]), (size_of_message_2-size)); // * 4


               if(size != size_of_message_2){
                    file_lock.lock();
                    myfile << "ERROR: Expecting message of size: " << size_of_message_2 << "\n";
                    myfile << std::flush;
                    for(int i = 0; i < size; i++)
                         myfile << buffer[i] << " ";
                    myfile << "\n";
                    file_lock.unlock();

                    return;
               }

               // Given this application; this should not happen (only if children have children)
     		if(size_of_message_2 == 2){
                    file_lock.lock();
                    std:: cout << "Error: Child is receiving a weight message -- This is not current supported\n";
                    myfile << std::flush;
                    file_lock.unlock();
     			int index = (int)buffer[0];
     			if((index >= 0) && (index < number_of_children))
     				weights[index] = buffer[1];
     		}
     		else{
     			//If we receive a packet, push to in_queue
     			if(size_of_message_2 == 1026){
     				arrival = new std::vector<float>();
     				arrival->assign((float*)buffer, (float*)buffer + 1026);
                         delete[] buffer;

                         // Keep trying to push segment into queue until successful
                         bool success = false;
                         do{
                              success = in_queue->push(arrival);
                          } while(!success);// Push window reference into queue

                         file_lock.lock();
                         myfile << "Pushed Arrival: (" << arrival->at(0) << ", " << arrival->at(1025) << ")\n";
                         myfile << std::flush;
                         file_lock.unlock();
     				increment();
     			}
     			else{
                         file_lock.lock();
     				myfile << "ERROR: We are receiving data of unexpected size: " << size_of_message_1 << "\n";
                         myfile << std::flush;
                         file_lock.unlock();		
                    }
               }
     	}
     }

     // Thread send function
     void child_impl::send_root(){

          float * weight_buffer = new float[2];
          weight_buffer[0] = -1;
          weight_buffer[1] = 2;

          std::vector<float> *temp; // Pointer to current vector of floats to be sent
          float *packet_size_buffer = new float[2]; // Pointer to single float array that will be sent before the data, indicating the size of the data segment

     	while(!d_finished){

     		// If we have windows, send window

     		if(out_queue->pop(temp)){

                    float packet_size = (temp->at(1025))+2; // The size of the segment is located at the last position of the size-1026 array
                    packet_size_buffer[0] = -1;
                    packet_size_buffer[1] = packet_size;

                    int index = -1;
                    
                    int sent = 0;
                    while(sent < 2)
                         sent += connector->send(index, (char*)&(packet_size_buffer[sent]), (2-sent)); // Send a one-float size message indicating the size of the window to be sent
     			
                    sent = 0;
                    while(sent < packet_size)
                         sent += connector->send(index, (char*)&((temp->data())[sent]), (packet_size-sent)); // *4

                    file_lock.lock();
                    myfile << "Popped and Sending packet index=" << temp->at(0) << " to parent with size =" <<temp->at(1025) << "\n";
                    myfile << std::flush;
                    file_lock.unlock();
     			
                    decrement();

                    delete temp;

                    temp = new std::vector<float>();

     			temp->push_back((float)child_index);
     			temp->push_back((float)get_weight());

                    file_lock.lock();
                    myfile << "\tSending weight message (index,weight): (" << temp->at(0) << ", " << temp->at(1) << ")\n";
                    myfile << std::flush;
                    file_lock.unlock();

                    // Send length and then WEIGHT
                    sent = 0;
                    while(sent < 2)
                         sent += connector->send(-1, (char*)&(weight_buffer[sent]), (2-sent)); // Send a one-float size message indicating the size of the window to be sent
     			
                    sent = 0;
                    while(sent < 2)
                         sent += connector->send(-1, (char*)&((temp->data())[sent]), (2-sent)); // Send the weight buffer
                    
                    delete temp;
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