/* -*- c++ -*- */
/*
 * Written by Tommy Tracy II (University of Virginia HPLP) 2014
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
 Format of Segments
 |
 byte < type :: [0] > -- contains the message type (2)
 byte * 4 (float)< index :: [1,2,3,4] > -- contains the index of the window
 byte * 4 (int)< size :: [5,6,7,8] > -- contains the size of the data in the data field to come next
 byte * 50 < data :: [6,<data_size + 6 - 1] > -- contains data followed by zeros
 |
 */

/*
 Important Note
 This code functions on groups of 50 char values (bytes).
 This can be modified in the future.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "queue_sink_byte_impl.h"
#include <stdio.h>
#include <stdlib.h>

#define VERBOSE false

namespace gr {
    namespace router {
        
        /*!
         *  This is the public constructor for the queue_sink_byte block.
         *
         *  @param itemsize The size (in bytes) of the data being measured
         *  @param &shared_queue A reference to the shared queue where segments would be pushed.
         *  @param preserve_index True if index is to be reconstructed from stream tags; generate new index from 0 otherwise.
         *  @return A shared pointer to the queue sink byte block
         */
        
        queue_sink_byte::sptr
        queue_sink_byte::make(int item_size, boost::lockfree::queue< std::vector<char>*, boost::lockfree::fixed_sized<true> > &shared_queue, bool preserve_index)
        {
            return gnuradio::get_initial_sptr
            (new queue_sink_byte_impl(item_size, shared_queue, preserve_index));
        }
        
        /*!
         *  This is the public constructor for the queue_sink_byte block.
         *
         *  @param size The size (in bytes) of the data being measured
         *  @param &shared_queue A reference to the shared queue where segments would be pushed.
         *  @param preserve_index True if index is to be reconstructed from stream tags; generate new index from 0 otherwise.
         */
        
        queue_sink_byte_impl::queue_sink_byte_impl(int size, boost::lockfree::queue< std::vector<char>*, boost::lockfree::fixed_sized<true> > &shared_queue, bool preserve_index)
        : gr::sync_block("queue_sink_byte",
                         gr::io_signature::make(1, 1, sizeof(char)),
                         gr::io_signature::make(0, 0, 0)), queue(&shared_queue), item_size(size), preserve(preserve_index), index_of_window(0), window(NULL)
        {
            
            set_output_multiple(50); // Only pack bytes in multiples of 50
            
            if(VERBOSE)
                myfile.open("queue_byte_sink.data");
            
            index_vector = new std::vector<float>();
            
            if(VERBOSE){
                myfile << "Calling queue_sink_byte Constructor" << std::endl;
            }
            
            waiting_on_window = false;
        }
        
        /*!
         *  This is the destructor.
         */
        queue_sink_byte_impl::~queue_sink_byte_impl()
        {
            /*
             The current code never calls this
             */
        }
        
        /*!
         *  This is the work() function of the queue sink byte block.
         */
        
        int
        queue_sink_byte_impl::work(int noutput_items,
                                   gr_vector_const_void_star &input_items,
                                   gr_vector_void_star &output_items)
        {
            const char *in = (const char *) input_items[0];
            
            if(preserve){
                
                // Get the index of the current window
                const uint64_t nread = this->nitems_read(0); //number of items read on port 0 up until the start of this work function (index of first sample)
                const size_t ninput_items = noutput_items; //assumption for sync block, this can change
                
                pmt::pmt_t key = pmt::string_to_symbol("i"); // Filter on key (i is for index)
                
                //read all tags associated with port 0 for items in this work function
                this->get_tags_in_range(tags, 0, nread, nread + ninput_items, key);
                
                if(VERBOSE){
                    myfile << "Grabing indexes from stream; got " << tags.size() << "\n";
                    myfile << std::flush;
                }
                
                //Convert all tags to floats and add to tags_vector
                if(tags.size() > 0){
                    
                    for(int i = 0; i < tags.size(); i++){
                        gr::tag_t temp_tag = tags.at(i);
                        pmt::pmt_t temp_value = temp_tag.value;
                        
                        if(temp_value != NULL){
                            
                            float temp_index = (float)(pmt::to_long(temp_value));
                            
                            if(VERBOSE){
                                myfile << "Got tag=" << temp_index << "\n";
                                myfile << std::flush;
                            }
                            
                            // After pushing tags into the index_vector, we can pull from here when constructing window segments
                            index_vector->push_back(temp_index);
                            
                        }
                        else{
                            if(VERBOSE){
                                myfile << "Got NULL tag\n";
                                myfile << std::flush;
                            }
                        }
                    }
                    tags.clear();
                }
            }
            
            
            if(!waiting_on_window){
                
                // Build type-2 segment
                window = new std::vector<char>();
                window->push_back('2'); // Push back the type (message type 2)
                
                char* index = get_index();
                window->insert(window->end(), &index[0], &index[4]); // Push the index of this window
                
                float number = (float)noutput_items;
                char* number_of_items = new char[4]; // Convert int into array of bytes (chars)
                memcpy(number_of_items, &(number), 4);
                window->insert(window->end(), &number_of_items[0], &number_of_items[4]); // Push the number of chars we're packing into this message; it needs to be a multiple of window size (50)
                
                window->insert(window->end(), &in[0], &in[noutput_items]);
            }
            
            int push_attempts = 0;
            waiting_on_window = false;
            
            // Keep trying to push the segment
            while(!queue->push(window)){
                
                boost::this_thread::sleep(boost::posix_time::microseconds(10));
                
                if(++push_attempts == 10){
                    waiting_on_window = true;
                    break;
                }
            }
            
            // Tell runtime system how many output items we produced.
            if(!waiting_on_window){
                window = NULL;
                queue_counter++;
                return noutput_items;
            }
            
            else{
                return 0;
            }
        }
        
        /*!
         *  This method returns a pointer to an array of bytes representing the index of the segment.
         *
         *  @return A pointer to an array of bytes representing an index float value. (4 bytes)
         */
        
        char* queue_sink_byte_impl::get_index(){
            
            char* index = new char[4];
            
            // If index is not preserved, use local index number (starting from 0)
            if(!preserve){
                
                memcpy(index, &index_of_window, 4); // Copy float into byte array
                index_of_window++;
                return index;
            }
            
            // If index is meant to be preserved, use stream tag value.
            else{
                if(index_vector->size() > 0){
                    index_of_window = index_vector->at(0);
                    index_vector->erase(index_vector->begin());
                }
                else{
                    if(VERBOSE){
                        myfile << "Error: Looking for tag value, but couldnt'd find any" << std::endl;
                    }
                }
                
                memcpy(index, &index_of_window, 4);
                index_of_window++;
                return index;
            }
        }
        
    } /* namespace router */
} /* namespace gr */

