/* -*- c++ -*- */
/*
 *  Written by Tommy Tracy II (University of Virginia HPLP) 2014
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

/*
 * This is the Child Router Block. This block receives messages from its parent, and pushes the messages into an input_queue.
 *
 * The child router also pops messages off of the output_queue, tacks on a weight (indicating how busy it is) and sends the output message back to the parent.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "child_impl.h"

#define VERBOSE     false

namespace gr {
 	namespace router {
        
        /*!
         *  This is the public constructor for the child router block.
         *
         *  @param number_of_children The number of children that the child router has. (0 for now)
         *  @param child_index The index of this child.
         *  @param hostname The hostname (or ip address) of the child's parent.
         *  @param &input_queue A pointer to the input lockfree queue, where segments sent from the parent will be pushed.
         *  @param &output_queue A pointer to the output lockfree queue, where completed segments will be pulled from to send to the parent.
         *  @param throughput The maximum rate at which the child router will pull from the output queue. (Not currently being used)
         *  @return A shared pointer to the child router block.
         */
        
        child::sptr
 		child::make(int number_of_children, int child_index, char * hostname, boost::lockfree::queue< std::vector<float>*, boost::lockfree::fixed_sized<true> > &input_queue, boost::lockfree::queue< std::vector<char>*, boost::lockfree::fixed_sized<true> > &output_queue, double throughput)
 		{
 			return gnuradio::get_initial_sptr (new child_impl(number_of_children, child_index, hostname, input_queue, output_queue, throughput));
 		}
        
        /*!
         *  This is the private constructor for the child router block.
         *
         *  @param number_of_children The number of children that the child router has. (0 for now)
         *  @param child_index The index of this child.
         *  @param hostname The hostname (or ip address) of the child's parent.
         *  @param &input_queue A pointer to the input lockfree queue, where segments sent from the parent will be pushed.
         *  @param &output_queue A pointer to the output lockfree queue, where completed segments will be pulled from to send to the parent.
         *  @param throughput The maximum rate at which the child router will pull from the output queue. (Not currently being used)
         */
        
        child_impl::child_impl( int numberofchildren, int index, char * hostname, boost::lockfree::queue< std::vector<float>*, boost::lockfree::fixed_sized<true> > &input_queue, boost::lockfree::queue< std::vector<char>*, boost::lockfree::fixed_sized<true> > &output_queue, double throughput)
        : gr::sync_block("child",
                         gr::io_signature::make(0, 0, 0),
                         gr::io_signature::make(0, 0, 0)), in_queue(&input_queue), out_queue(&output_queue), child_index(index), global_counter(0), parent_hostname(hostname), number_of_children(numberofchildren), d_finished(false), d_throughput(throughput)
        {
            
            
            if(VERBOSE)
                myfile.open("child_router.data");
            
            // Connect to <hostname>
            if(VERBOSE)
                myfile << "Attempting to connect to parent\n";
            
            connector = new NetworkInterface(sizeof(char), 0, 8080, false);
            
            // Interconnect all blocks (hostname of Root)
            connector->connect(hostname);
            
            if(VERBOSE){
                myfile << "Connected to parent\n";
                std::cout << "\tChild Router Finished connecting to hostname=" << hostname << std::endl;
            }
            
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
                myfile << "Calling Child Router Constructor v.2\n";
                myfile << "Arguments: number of children=" << numberofchildren << " index=" << index << " hostname= " << hostname << "\n\n" << std::flush;
            }
        }
        
        /**
         * The destructor for the child router block.
         *
         *  Interrupt and join all threads, and delete the Connector.
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
        
        /**
         *  This is the work() function. It doesn't do anything, because all sending and receiving is done with separate threads.
         */
        
        int
        child_impl::work(int noutput_items,
                         gr_vector_const_void_star &input_items,
                         gr_vector_void_star &output_items)
        {
            // This block is completely asynchronous; therefore there is no need for 'work()'ing
            // Might want to put something else in here; not sure yet.
            return 0;
        }
        
        
        /**
         * The receive_root thread function listens and receives messages from the parent router to populate the input queue.
         */
        
        void child_impl::receive_root(){
            
            char * temp_header_bytes = new char[3*sizeof(float)]; // Grab the first three header values
            char * temp_buffer;
     	    float * buffer;
            std::vector<float> *arrival;
            int size;
            
     	    while(!d_finished){
                
                // Calling the blocking receive; receive array of bytes
                size = 0;
                while(size < (3*sizeof(float))){
                    size += connector->receive(-1, &(temp_header_bytes[0]), (3*sizeof(float)-size));
                    if(size == 0 && d_finished){
                        delete[] temp_header_bytes;
                        continue;
                    }
                }
                
                float packet_type; // The message type of the current segment
                memcpy(&packet_type, &(temp_header_bytes[0]), 4);
                
                float index; // Index of the current segment
                memcpy(&index, &(temp_header_bytes[4]), 4);
                
                float data_size; // size in floats
                memcpy(&data_size, &(temp_header_bytes[8]), 4);
                
                // Switch on packet type and parse messages; only type 1 is current supported
                switch((int)packet_type){
                    case 1:
                        temp_buffer = new char[(int)data_size*sizeof(float)];
                        buffer = new float[(int)data_size];
                        size = 0;
                        
                        // Wait for the rest of the message bytes
                        while(size < ((int)data_size*sizeof(float)))
                            size += connector->receive(-1, &(temp_buffer[size]), ((int)data_size*sizeof(float)-size)); // Receive the rest of the segment
                        
                        memcpy(&buffer[0], &temp_buffer[0], (int)data_size*sizeof(float));
                        
                        // Rebuild float vectors and push those into the input queue
                        arrival = new std::vector<float>();
                        arrival->push_back(packet_type);
                        arrival->push_back(index);
                        arrival->push_back(data_size);
                        arrival->insert(arrival->end(), &buffer[0], &buffer[(int)data_size]);
                        
                        // Keep attempting to push the segment until successful (may want to make this more efficient)
                        while(!in_queue->push(arrival))
                            ;
                        
                        // Keep incrementing the number of segments being used (change this)
                        for(int i = 0; i < (data_size/1024); i++)
                            increment();
                        
                        break;
                    case 2:
                        std::cout << "ERROR: Right now we're not supporting this format" << std::endl;
                        break;
                    case 3:
                        arrival = new std::vector<float>();
                        arrival->push_back(3);
                        
                        while(!in_queue->push(arrival))
                            ;
                        
                        break;
                    default:
                        myfile << "ERROR: Got a message of unexpected type" << std::endl;
                        
                }
            }
            
            delete[] temp_header_bytes;
            
        }
        
        
        /**
         * The send_root thread function grabs segments from the output queue, appends a weight (business) and sends the message to the child's parent.
         */
        
        void child_impl::send_root(){
            
            std::vector<char> *temp; // Pointer to current vector of bytes to be sent
            int sent = 0;
            
            // Until the thread is killed, keep sending
     	    while(!d_finished){
                
                
                // This is an internal throttle; it is not being used yet.
                // Throughput Stuff------
                // Code derived from throughput block
                /*
                 boost::system_time now = boost::get_system_time();
                 boost::int64_t ticks = (now - d_start).ticks();
                 uint64_t expected_samples = uint64_t(d_samples_per_tick * ticks);
                 
                 if(d_total_samples > expected_samples)
                 boost::this_thread::sleep(boost::posix_time::microseconds(long((d_total_samples - expected_samples) / d_samples_per_us)));
                 */
                //----------
                
                
                // If there is a segment in the output queue, pop it and send it
                if(out_queue->pop(temp)){
                    
                    char packet_type = temp->at(0); // Get the packet type
                    
                    float index; // Get the packet index
                    memcpy(&index, &(temp->at(1)), 4);
                    
                    float data_size; // Get the packet data_size
                    memcpy(&data_size, &(temp->at(5)), 4);
                    
                    float num_windows = data_size / 50;
                    float packet_size = data_size + 1 + 2 * sizeof(float);
                    
                    //Switch on the packet_type
                    switch(packet_type){
                        case '2':
                        {
                            
                            d_total_samples += data_size;
                            
                            // Shove on a weight value, and make it a type-2 message
                            
                            int weight = get_weight(); // Grab the current weight of the child
                            
                            char* weight_bytes = new char[4];
                            memcpy(weight_bytes, &weight, 4);
                            
                            temp->insert(temp->end(), &(weight_bytes[0]), &(weight_bytes[4]));
                            temp->at(0) = '3'; // Change to type 3 message
                            
                            packet_size += 4; // Increment the packet size; we're adding a weight
                            
                            sent = 0;
                            while(sent < packet_size)
                                sent += connector->send(-1, &((temp->data())[sent]), (packet_size-sent)); // *4
                            
                            for(int i = 0; i < num_windows; i++)
                                decrement();
                            
                            delete temp;
                            break;
                        }
                        case '1':
                        {
                            
                            std::cout << "Child router cannot deal with receiving type 1 messages right now" << std::endl;
                            
                            break;
                            
                        }
                        case '3': // Got a kill message
                        {
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
                        default:
                        {
                            std::cout << "WOW; shit's going down!!" << std::endl;
                            std::cout << (int)packet_type << std::endl;
                            break;
                        }
                    }
                }
                else{
                    boost::this_thread::sleep(boost::posix_time::microseconds(1000));
                    
                }
     	    }
        }
        
        /*!
         *  This is an incomplete thread function. It is meant to be used by the child router to receive from it's children. (for multiple levels of routers)
         *
         *  @param index The index of the child to receive from
         */
        
        void child_impl::receive_child(int index){
            // Recieve from children
            // To be implemented
        }
        
        /*!
         *  This is an incomplete thread function. It is meant to be used by the child router to send to it's children. (for multiple levels of routers)
         *
         *  @param index The index of the child to send to.
         */
        
        void child_impl::send_child(int index){
            // Send to children
            // To be implemented
        }
        
        /*!
         *  The min() function returns the index of the child with the minimum weight.
         *
         *  @return index The index of the child witht he minimum weight.
         */
        
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
        
        /*!
         *  The get_weight() function returns the child router's current weight.
         *
         *  @return global_counter The weight of the child router.
         */
        
        inline int child_impl::get_weight(){
            
            //For simple application with no sub-trees, simply return outstandng windows
            return global_counter;
        }
        
        
        /*!
         *  The decrement() function is a thread-safe method that decreases the weight of the child router by one.
         */
        
        inline void child_impl::decrement(){
            global_lock.lock();
            global_counter--;
            global_lock.unlock();
        }
        
        /*!
         *  The decrement() function is a thread-safe method that increases the weight of the child router by one.
         */
        
        inline  void child_impl::increment(){
            global_lock.lock();
            global_counter++;
            global_lock.unlock();
        }
    } /* namespace router */
} /* namespace gr */
