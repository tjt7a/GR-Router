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
 Format of Segments :: Currently still hard-coded
 |
 floats
 < type :: [0] > -- contains the message type (1)
 < index :: [1] > -- contains the index of the window
 < size :: [2] > -- contains the size of the data in the data field to come next
 < data :: [3,<data_size + 2] > -- contains data followed by zeros
 |
 */

/*
 Important Note
 This code functions on groups of 768 float values.
 This can be modified in the future.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "queue_sink_impl.h"
#include <stdio.h>
#include <stdlib.h>

#define BOOLEAN_STRING(b) ((b) ? "true":"false")

#define VERBOSE false

namespace gr {
    namespace router {
        
        /*!
         *	This is the public constuctor for the queue sink block.
         *
         *  @param item_size The size (in bytes) of the data units.
         *  @param &shared_queue A pointer to the fixed-sized lockfree queue in which the segments will be pushed.
         *  @param preserve_index True if there is an index preserved in the stream tags, and if it is to be preserved in the resulting segments. Else, False.
         */
        
        queue_sink::sptr
        queue_sink::make(int item_size, boost::lockfree::queue< std::vector<float>*, boost::lockfree::fixed_sized<true> > &shared_queue, bool preserve_index)
        {
            return gnuradio::get_initial_sptr (new queue_sink_impl(item_size, shared_queue, preserve_index));
        }
        
        /*!
         * This is the private constructor of the queue sick block.
         *
         * @param size  The size (in bytes) of data units.
         * @param &shared_queue A pointer to the fixed-sized lockfree queue in which the segments will be pushed.
         * @param preserve_index True if there is an index preserved in the stream tags, and if it is to be preserved in the resulting segments. Else, False.
         */
        
        queue_sink_impl::queue_sink_impl(int size, boost::lockfree::queue< std::vector<float>*, boost::lockfree::fixed_sized<true> > &shared_queue, bool preserve_index)
        : gr::sync_block("queue_sink",
                         gr::io_signature::make(1, 1, sizeof(float)),
                         gr::io_signature::make(0, 0, 0)), queue(&shared_queue), item_size(size), preserve(preserve_index), index_of_window(0), window(NULL)
        {
            
            /*
             Read from XML to get size information
             */
            /*
             // XML file containing the format of the messages
             const char *filename = "queue_sink.xml";
             
             const char *fields[] = {"Properties", "Format"};
             
             TiXmlDocument doc(filename);
             
             if(doc.LoadFile()){
             
             TiXmlElement *root = doc.FirstChildElement("Message");
             
             std::vector<float> format;
             
             if(root){
             
             for(int i = 0; i < 2; i++){
             TiXmlElement *element = root->FirstChildElement(fields[i]);
             
             if(element){
             
             TiXmlAttribute *pAttrib = element->FirstAttribute();
             
             while(pAttrib){
             
             //If Properties attribute
             if(i == 0){
             
             if(pAttrib->Name() == "multiple"){
             
             if(pAttrib->QueryIntValue(&out_mult) != TIXML_SUCCESS){
             out_mult = 1024;
             std::cerr << "Cannot parse <multiple>; setting to 1024" << std::endl;
             }
             
             }
             else if(pAttrib->Name() == "preserve"){
             
             if(pAttrib->Value() == "false" || pAttrib->Value() == "False" || pAttrib->Value() == "FALSE"){
             preserve = false;
             }
             else if(pAttrib->Value() == "true" || pAttrib->Value() == "True" || pAttrib->Value() == "TRUE"){
             preserve = true;
             }
             else
             preserve = false;
             
             }
             
             else if(pAttrib->Name() == "symbol"){
             symbol = pAttrib->Value();
             }
             
             // Grab next attribute
             }
             
             // Else, Format attributes
             else{
             
             if(pAttrib->Name() == "type"){
             format.clear();
             format.push_back(atof(pAttrib->Value()));
             }
             else if(pAttrib->Name() == "index"){
             // We want an index; true
             if(pAttrib->Value() == "true" || pAttrib->Value() == "True" || pAttrib->Value() == "TRUE"){
             format.push_back(1);
             }
             // Assume false
             else
             }
             }
             
             pAttrib = pAttrib->Next();
             }
             }
             else
             break;
             }
             
             }
             else{
             std::cerr << "Can't read message" << std::endl;
             return -1;
             }
             
             }
             else{
             if(VERBOSE)
             myfile << "Cannot read configuration file " << filename << " using defaults" << std::endl;
             }
             */
            
            
            set_output_multiple(768); // Guarantee inputs in multiples of 768 floats!
            
            index_vector = new std::vector<float>(); // vector of indexes (floats) -- populated with indexes that we pull from stream tag
            
            waiting_on_window = false;
        }
        
        /**
         *  The destructor for the queue sink block.
         */
        queue_sink_impl::~queue_sink_impl()
        {
            
            delete index_vector;
            
            /* Kill message is currently not supported
             // Push kill messages for any blocks sourcing from the queue
             window = new std::vector<float>(); // Create a new vector for window pointer to point at
             window->push_back(3); // Append the type of the KILL message (3)
             
             while(!queue->bounded_push(window)){
             boost::this_thread::sleep(boost::posix_time::microseconds(1)); // Arbitrary sleep time
             }
             
             if(VERBOSE)
             myfile.close();
             
             delete window;
             */
        }
        
        /*!
         *  This is the work() function. It segments the stream, and pushes the resulting segments into the lockfree queue.
         *
         *  @param noutput_items The number of data samples
         *  @param &input_items Pointer to input vector
         *  @param &output_items Pointer to output vector
         */
        
        int
        queue_sink_impl::work(int noutput_items,
                              gr_vector_const_void_star &input_items,
                              gr_vector_void_star &output_items)
        {
            
            // Pointer to input data vector
            const float *in = (const float *) input_items[0]; // Input float buffer pointer
            
            // Do we want to pull indexes from the stream tags and use those for window indexes?
            if(preserve){
                
                // Get the index of the current window
                const uint64_t nread = this->nitems_read(0); //number of items read on port 0 up until the start of this work function (index of first sample)
                const size_t ninput_items = noutput_items; //assumption for sync block, this can change
                
                pmt::pmt_t key = pmt::string_to_symbol("i"); // Filter on key (i is for index)
                
                //read all tags associated with port 0 for items in this work function
                this->get_tags_in_range(tags, 0, nread, nread + ninput_items, key);
                
                //Convert all tags to floats and add to tags_vector
                if(tags.size() > 0){
                    
                    for(int i = 0; i < tags.size(); i++){
                        gr::tag_t temp_tag = tags.at(i);
                        pmt::pmt_t temp_value = temp_tag.value;
                        
                        if(temp_value != NULL){
                            
                            float temp_index = (float)(pmt::to_long(temp_value));
                            
                            // After pushing tags into the index_vector, we can pull from here when constructing window segments
                            index_vector->push_back(temp_index);
                            
                        }
                    }
                    tags.clear();
                }
            }
            
            // If we don't have a segmnet ready to push... let's make one
            if(!waiting_on_window){
                
                // Build type-1 segment
                window = new std::vector<float>();
                window->push_back(1); // Push back the type (message type 1)
                window->push_back(get_index()); // Push the index of this window
                window->push_back(noutput_items); // Push the number of floats we're packing into this message
                window->insert(window->end(), &in[0], &in[noutput_items]);
            }
            
            int push_attempts = 0 ;
            waiting_on_window = false; // False unless we can't push 10 times in a row
            
            // try to push the window 10 times; if that doesn't work, we'll try again next time this block is called
            while(!queue->push(window)){
                
                boost::this_thread::sleep(boost::posix_time::microseconds(10)); // wait 10 microsecond
                
                if(++push_attempts == 10){
                    waiting_on_window = true;
                    break;
                }
            }
            
            if(!waiting_on_window){
                window = NULL; // We're done with this window; it's on the queue
                queue_counter++; // We have one more outstanding window
                return noutput_items; // Number_of_windows*768;
            }
            else{
                return 0;
            }
        }
        
        /*!
         *  This is the get_index function. It returns a float for the index of the current segment. This index is either pulled from the index_vector if the index was preserved with stream tags, or it is generated from 0.
         *
         *  This block is transparent and the data on the input is copied to the output.
         *
         *  @return index_of_window A float representing the index of the current window.
         */
        
        
        float queue_sink_impl::get_index(){
            
            // If not preserving an index, start from 0 and incremement for every subsequent windows
            if(!preserve)
                return index_of_window++;
            
            // If we do want to preserve index, pull index from stream tags and return the next one!
            else{
                if(index_vector->size() > 0){
                    index_of_window = index_vector->at(0);
                    index_vector->erase(index_vector->begin());
                }
                
                return index_of_window++;
            }
        }
    } /* namespace router */
} /* namespace gr */
