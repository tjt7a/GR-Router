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
 * the Free Software Foundation, Inc., 51 Franklin Street
 * Boston, MA 02110-1301, USA.
 */

/*
            Format of type-1 Segments
        |
            < type :: [0] > -- contains the message type
            < index :: [1] > -- contains the index of the window
            < size :: [2] > -- contains the size of the data in the data field to come next
            < data :: [3,<datasize-1>] > -- contains data followed by zeros

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "root_impl.h"

#define VERBOSE false

 namespace gr {
 	namespace router {

        // This is the public shared-pointer constructor; it is called by the Scheduler at creation time
 		root::sptr
 		root::make(int number_of_children, boost::lockfree::queue< std::vector<float>*, boost::lockfree::fixed_sized<true> > &input_queue, boost::lockfree::queue< std::vector<char>*, boost::lockfree::fixed_sized<true> > &output_queue, double throughput)
 		{
 			return gnuradio::get_initial_sptr (new root_impl(number_of_children, input_queue, output_queue, throughput));
 		}

        /*
        * The private constructor
        * 1 inputs: 1 from source
        * 1 outputs: 1 to sink
        */
        root_impl::root_impl(int numberofchildren, boost::lockfree::queue< std::vector<float>*, boost::lockfree::fixed_sized<true> > &input_queue, boost::lockfree::queue< std::vector<char>*, boost::lockfree::fixed_sized<true> > &output_queue, double throughput)
        : gr::sync_block("root",
     	gr::io_signature::make(0,0,0),
     	gr::io_signature::make(0,0,0)), number_of_children(numberofchildren), in_queue(&input_queue), out_queue(&output_queue), d_throughput(throughput)
        {

            // Throughput stuff ----------
          	// How fast are we going to check the queue? Don't want to over-work here
            
            
            d_start = boost::get_system_time();
            d_total_samples = 0;
            d_samples_per_tick = d_throughput/boost::posix_time::time_duration::ticks_per_second();
            d_samples_per_us = d_throughput/1e6; // Number of samples in a micro-second 
            
            // ----------


            if(VERBOSE)
                myfile.open("root_router.data");

            num_killed = 0;

    		// Set global counter; no need to lock -> no contention
         	global_counter = 0;

            // Communication connector between nodes (size of elements, number of children, port number, are we root?)
    		connector =  new NetworkInterface(sizeof(char), number_of_children, 8080, true);

    	   	// Interconnect all blocks (we're root, so localhost=NULL)
    		connector->connect(NULL);

        	// Initialize counters for both queues to 0 (not sure we need this)
    		in_queue_counter = 0;
    		out_queue_counter = 0;

    	  	// Array of weights values for each child + local (index 0)
    		weights = new float[number_of_children];

    	   	// Finished flag for threads(true if finished)
    		d_finished = false;

            //Thread for parent to send
            send_thread = boost::shared_ptr< boost::thread >(new boost::thread(boost::bind(&root_impl::send, this)));
            
            // Threads for parent to receive from all children
    		for(int i = 0; i < number_of_children; i++){

    			// _1 is a place holder for the argument of arguments passed to the functor ;; in this case the index
          		if(VERBOSE)
            		std::cout << "Spawning new receiver thread for child #" << i << std::endl;
    				
    			   thread_vector.push_back(boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&root_impl::receive, this, _1), i)));
    		
        }

        	if(VERBOSE){
          		std::cout << "Finished calling Root Router's Constructor" << std::endl;
          		myfile << "Calling Root Router Constructor v.2\n" << std::flush;
        	}
	    }
        /*
         * Our virtual destructor.
         */
         root_impl::~root_impl()
         {

            if(VERBOSE){
                std::cout << "Calling Parent Router Destructor" << std::endl;
                myfile << "Calling Root Router Destuctor\n" << std::flush;
            }

            d_finished = true;

            // Join the send thread
            send_thread->interrupt();
            send_thread->join();

            // Join all of the child receiver threads
         	for(int i = 0; i < number_of_children; i++){
         		thread_vector[i]->interrupt();
         		thread_vector[i]->join();
         	}	

            delete connector;
            delete[] weights;

         }

         // This function is required by the agreement we have with the Gnu Radio Scheduler, but
            // ... it's not required, because work() doesn't do anything... we're using threads, so this is an empty method belonging to the parent thread.
         int
         root_impl::work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
         {
         	//return noutput_items;
            if(d_finished || (num_killed == number_of_children)){
                d_finished = true;
                return -1; // We're done
            }
            else{
                return 0;
            }
         }

        // Sender thread; It's sole purpose is to send windows when available
        // Include packet format information in xml file ... functionality will be added later
        void root_impl::send(){

          std::vector<float> *temp; // Pointer to current vector of floats to be sent
          int sent = 0;

          int index, data_size, window_count, packet_size;

     	    // Until the program exits, continue sending
     	    while(!d_finished){

            // Throughput Stuff------
            // Code derived from throughput block
            
            boost::system_time now = boost::get_system_time();
            boost::int64_t ticks = (now - d_start).ticks(); // total number of ticks since start time
            uint64_t expected_samples = uint64_t(d_samples_per_tick * ticks); // The total number of samples we expect to pass through since then

            if(d_total_samples > expected_samples){
              boost::this_thread::sleep(boost::posix_time::microseconds(long((d_total_samples - expected_samples) / d_samples_per_us)));
            }
          
            //----------

		        int min_weight = weights[min()];
	           //std::cout << "Min weight is: " << min_weight << std::endl;

     		   // If there is a window available, send it to indexed node
     		   if(in_queue->pop(temp)){

                	int packet_type = (int)temp->at(0); // Get packet type

                	if(VERBOSE)
                    		myfile << "Packet type: packet_type" << std::endl;

                	// Switch on the packet_type
                	switch(packet_type){
                    	case 1:
                    	{
                        	index = min(); // Grab index of next target

                        	data_size = (int)temp->at(2); // The size of the data segment is located at index 2
                        	window_count = data_size / 768;
                        	packet_size = (data_size + 3) * 4; // Size of the data + headers * 4 (chars per byte)

                        	char* data_bytes = new char[packet_size];

                        	weights[index] += window_count;

                        	d_total_samples += data_size;

                        	if(VERBOSE)
                            		myfile << "Sending packet index=" << temp->at(1) << " to child=" << index << std::endl;

                        	memcpy(&(data_bytes[0]), &(temp->data()[0]), packet_size);

                        	sent = 0;
                        	while(sent < packet_size)
                            		sent += connector->send(index, &(data_bytes[sent]), (packet_size - sent));

                        	if(VERBOSE)
                            		myfile << "Finished sending" << std::endl;

                        	// We've sent the data, so don't need the memory anymore
                        	delete[] data_bytes;

                        	for(int i = 0; i < window_count; i++)
                          		increment();

                        	break;
                    	}
                    	case 3:
                    	{
                        	for(int i = 0; i < number_of_children; i++){
                            		sent = 0;
                            		while(sent < 1)
                                		sent += connector->send(i, (char*)&((temp->data())[sent]), (1-sent)); // Send the segment
                        	}

                        	break;
			                 }
                    	default:
                        {
				                  std::cout << "ERROR: Parent Router is trying to parse an incorrectly formatted packet" << std::endl;
                        	break;
                   	    }
		 	            }
            }
		        else{
			       boost::this_thread::sleep(boost::posix_time::microseconds(1000));
		        }
            // Future Work: Include additonal code for redundancy; keep copy of window until it has been ACKd;; Is this required given we're using TCP?

     }
	
    delete temp;
}

        /*
            Format of type-2 Segments
        |
            < type :: [0] > -- contains the message type
            < index :: [1,2,3,4] > -- contains the index of the window
            < size :: [5,6,7,8] > -- contains the size of the data in the data field to come next
            < data :: [...] > -- contains data followed by zeros
            < weight :: [1,2,3,4] > -- contains the weight of the sending child
        */

        /*
            Format of type-3 Segments
        |
            < type :: [0] > -- contains the message type
        */


        // Receiver thread; one per child node (instead of sequentially iterating through all nodes or using interrupts)
        void root_impl::receive(int index){

          /* Output file Setup */
          std::ofstream thread_file;

          if(VERBOSE){
            char name_buff[32];
            sprintf(name_buff, "root_router_%d.data", index);
            thread_file.open(name_buff);
          }

          char * temp_buffer = new char[9];
     	    std::vector<char> *temp;
          int size = 0;
          std::vector<char> *arrival;
          std::vector<float> *kill_msg;

          float weight;

          if(VERBOSE)
            std::cout << "Started receiver thread for child #" << index << std::endl;

     	    // Until the thread is finished
     	    while(!d_finished){

            // Wait until there's something to receive (may want to replace with something more efficient than a spinning wait)
           size = 0;
     		   while(size < 9){
                size += connector->receive(index, (char*)&(temp_buffer[size]), (9-size)); // Grab first byte (type), float (index), float (size)
                if(size == 0 && d_finished)
                    return; // We're done
            }

            char packet_type = temp_buffer[0];

            float message_index;
            memcpy(&message_index, &(temp_buffer[1]), 4);

            float data_size;
            memcpy(&data_size, &(temp_buffer[5]), 4);

            int number_of_windows = data_size / 768;
            int remaining_message_size = data_size + (1 * sizeof(int)); // One footer at the end (the weight)

            switch(packet_type){
                case '1':
                {
                    std::cout << "ERROR: Right now we're not supporting format 1 from the child routers" << std::endl;
                    break;
                }
                case '2':
                {
                    std::cout << "ERROR: Right now we're not supporting format 2 from the child routers" << std::endl;
                    break;
                }
                case '3':
                {
                    char * buffer = new char[remaining_message_size];

                    size = 0;
                    while(size < remaining_message_size)
                        size += connector->receive(index, (char*)&(buffer[size]), (remaining_message_size-size)); // Receive the data

                    float weight;
                    memcpy(&weight, &(buffer[remaining_message_size-4]), 4);

                    arrival = new std::vector<char>();
                    arrival->push_back('2'); // type 1 message
                    arrival->insert(arrival->end(), &(temp_buffer[1]), &(temp_buffer[9])); // Insert index and data_size bytes
                    arrival->insert(arrival->end(), &buffer[0], &buffer[(int)data_size]);

                    while(!out_queue->push(arrival))
                      ;

                    for(int i = 0; i < number_of_windows; i++)
                      decrement();

                    weights[index] = weight;
                    delete[] buffer;
                    break;
                }
                case '4':
                {
                  /*
                    killed_lock.lock();
                    num_killed++;
                    killed_lock.unlock();

                    if(num_killed == number_of_children){
                        kill_msg = new std::vector<char>();
                        kill_msg->push_back('3');
                        if(VERBOSE)
                            thread_file << "Pushing kill message" << std::endl;

                        while(!out_queue->push(kill_msg))
                            ;
                    }
                    return;
                  */
                    break;
                }
                default:
                {
                    std::cout << "ERROR: Receiving unacceptable image format" << std::endl;
                    break;
                }
            }

     	}

     	delete [] temp_buffer; // We're done with our buffer
     }

    	// Find index of child with minimum weight BIG_OH(N)
    	// Might want to use a better algorithm for this
    	// Configurable based on application (include XML for this)
	int root_impl::min(){
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

	// These functions are intended for a multi-threaded implementation; locking is not necessary with a single thread
    // Increments/decrements the outstanding windows

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
