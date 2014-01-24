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
#include "root_impl.h"

 namespace gr {
 	namespace router {

        // This is the public shared-pointer constructor; it is called by the Scheduler at creation time
 		root::sptr
 		root::make(int number_of_children, boost::lockfree::queue< std::vector<float>* > &input_queue, boost::lockfree::queue< std::vector<float>* > &output_queue)
 		{
 			return gnuradio::get_initial_sptr (new root_impl(number_of_children, input_queue, output_queue));
 		}

        /*
        * The private constructor
        * 1 inputs: 1 from source
        * 1 outputs: 1 to sink
        */
        root_impl::root_impl(int numberofchildren, boost::lockfree::queue< std::vector<float>* > &input_queue, boost::lockfree::queue< std::vector<float>* > &output_queue)
        : gr::sync_block("root",
     	gr::io_signature::make(0,0,0),
     	gr::io_signature::make(0,0,0))
        {

            VERBOSE = true; // Used to dump useful information
            myfile.open("root_router.data");

            // Number of children for root node
            number_of_children = numberofchildren;

    		// Set global counter; no need to lock -> no contention
         	global_counter = 0;

            // Pointers to input/output queue of packets
    		in_queue = &input_queue;
    		out_queue = &output_queue;

            // Communication connector between nodes (size of elements, number of children, port number, are we root?)
    		connector =  new NetworkInterface(sizeof(float), number_of_children, 8080, true);

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
                std::cout << "Spawning new receiver thread for child #" << i << std::endl;
    			thread_vector.push_back(boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&root_impl::receive, this, _1), i)));
    		}

            if(VERBOSE)
                std::cout << "Finished calling Root Router's Constructor" << std::endl;

            file_lock.lock();
            myfile << "Calling Root Router Constructor\n";
            myfile << std::flush;
            file_lock.unlock();
	    }
        /*
         * Our virtual destructor.
         */
         root_impl::~root_impl()
         {

            if(VERBOSE)
                std::cout << "Calling Parent Router Destructor" << std::endl;

            file_lock.lock();
            myfile << "Calling Root Router Destructor\n";
            myfile << std::flush;
            file_lock.unlock();

            d_finished = true;

            // Join the send thread
            send_thread->interrupt();
            send_thread->join();

            // Join all of the child receiver threads
         	for(int i = 0; i < number_of_children; i++){
         		thread_vector[i]->interrupt();
         		thread_vector[i]->join();
         	}	

         }

         // This function is required by the agreement we have with the Gnu Radio Scheduler, but
            // ... it's not required, because work() doesn't do anything... we're using threads, so this is an empty method belonging to the parent thread.
         int
         root_impl::work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
         {
         	//return noutput_items;
            return 0;
         }

        // Sender thread; It's sole purpose is to send windows when available
        // Include packet format information in xml file ... functionality will be added later
        void root_impl::send(){

          std::vector<float> *temp; // Pointer to current vector of floats to be sent
          float *packet_size_buffer = new float[2]; // Pointer to single float array that will be sent before the data, indicating the size of the data segment

     	  // Until the program exits, continue sending
     	  while(!d_finished){

     		// If there is a window available, send it to indexed node
     		if(in_queue->pop(temp)){

     			float packet_size = (temp->at(1025))+2; // The size off the segment is located at the last position of the size-1026 array + 2 for index and size
                
                // setting the first float to -1 (indicator of a length message)
                packet_size_buffer[0] = -1;
                packet_size_buffer[1] = packet_size;

                // Grab index of next target node
                int index = min();
                weights[index]++;

                file_lock.lock();
                myfile << "Sending packet index=" << temp->at(0) << " to child=" << index << " size=" << packet_size << '\n';
                myfile << std::flush;
                file_lock.unlock();

                // Sending 1026 floats, not 1026*4 floats
                int sent = 0;
                while(sent < 2){
                    sent += connector->send(index, (char*)&(packet_size_buffer[sent]), (2-sent)); // Send the size of the segment being sent
                }

                sent = 0;
                while(sent < packet_size){
                    sent += connector->send(index, (char*)&((temp->data())[sent]), (packet_size-sent)); // Send the segment
                }

                // Future Work: Include additonal code for redundancy; keep copy of window until it has been ACKd;; Is this required given we're using TCP?

                delete temp; // Delete vector<float> that temp is pointing to (we've sent it, so no need to hold on to it)

     			increment(); // increment global counter (how many windows are out there?)
     		}
     	  }
        }

        // Receiver thread; one per child node (instead of sequentially iterating through all nodes or using interrupts)
        void root_impl::receive(int index){

     	  float *size_buffer = new float[1];
          int size_of_message_1;
          int size_of_message_2;
          int size;

          std::ofstream thread_file;

          char name_buff[32];
          sprintf(name_buff, "root_router_%d.data", index);
          thread_file.open(name_buff);

     	  float * buffer;
     	  std::vector<float> *temp;

          std::cout << "Started receiver thread for child #" << index << std::endl;
          std::cout << std::flush;

     	  // Until the thread is finished
     	  while(!d_finished){

            // Wait until there's something to receive (may want to replace with something more efficient than a spinning wait)
            size = 0;
     		while(size < 2){
                size += connector->receive(index, (char*)&(size_buffer[size]), (2-size));
            }

            size_of_message_1 = (int)size_buffer[0];
            size_of_message_2 = (int)size_buffer[1];

            if(size_of_message_1 != -1){
                thread_file << "ERROR: Root received unexpected or corrupted message\n";
                thread_file << "length message: (" << size_of_message_1 << ", " << size_of_message_2 << ")\n";
                thread_file << std::flush;
                return;
            }

            buffer = new float[size_of_message_2];

            thread_file << "Got a length message : Size= (" << size_of_message_1 << ", "<< size_of_message_2 << ")\n";
            thread_file << std::flush;
            // Receive data
            size = 0;
            while(size < size_of_message_2){
     			size += connector->receive(index, (char*)&(buffer[size]), (size_of_message_2-size));//*4))
                thread_file << "Still receiving data message\n" << std::flush;
            }

     		// Received weight (need to figure out how else to differentiate)
     		if(size_of_message_2 == 2){
     			int i = (int)buffer[0];

     			// We received valid 'weight' packet
				if((i >= 0) && (i < number_of_children)){
                    // Update weights table
					weights[index] = buffer[1];
                    thread_file << "Got a weight message : (" << buffer[0] << ", " << buffer[1] << ")" << std::endl;
                    thread_file << std::flush;
				}
				else{
                    std::cout << "ROOT RECEIVED (" << buffer[0] << ", " << buffer[1] << ")" << std::endl;
					std::cout << "ERROR: Received unsupported packet of size=" << size_of_message_2 << std::endl;
				}
     		}

     		// Received window segment
     		else if(size_of_message_2 == 1026){
     			std::vector<float> *arrival = new std::vector<float>();
				arrival->assign(buffer, buffer+1026);

                thread_file << "Got a window segment : start=" << arrival->at(0) << " end=" << arrival->at(1025) <<  std::endl; 
                thread_file << std::flush;

                // Keep trying to push segment into queue until successful
                bool success = false;
                do{

                    thread_file << "Pushing arrival on queue" << std::endl;
                    thread_file << std::flush;
                    out_queue_lock.lock();
                    success = out_queue->push(arrival);
                    out_queue_lock.unlock();
                } while(!success);// Push window reference into queue

				decrement(); // One fewer window is out with children
     		}

            else{
                std::cout << "ROOT RECEIVED (" << buffer[0] << ", " << buffer[1] << ")" << std::endl;
                std::cout << "ERROR: Received unsupported packet of size=" << size_of_message_2 << " size_of_message=" << size_of_message_2 << " " <<size_of_message_2-2 << std::endl;
            }
     	}

     	delete [] buffer; // We're done with our buffer
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