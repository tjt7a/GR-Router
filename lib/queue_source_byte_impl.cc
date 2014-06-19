/* -*- c++ -*- */
/* 
* Written by Tommy Tracy II (University of Virginia HPLP)
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
      byte * 4 (float)< size :: [2,3,4,5] > -- contains the size of the data in the data field to come next
      byte * 50 < data :: [6,<data_size + 6 - 1] > -- contains data followed by zeros
    |
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "queue_source_byte_impl.h"
#include <stdio.h>

#define VERBOSE false 


namespace gr {
  namespace router {

    bool order_window(const std::vector<char>* a, const std::vector<char>* b){
      return(a->at(1) < b->at(1));
    }

    queue_source_byte::sptr
    queue_source_byte::make(int item_size, boost::lockfree::queue< std::vector<char>*, boost::lockfree::fixed_sized<true> > &shared_queue, bool preserve_index, bool order)
    {
      return gnuradio::get_initial_sptr
        (new queue_source_byte_impl(item_size, shared_queue, preserve_index, order));
    }

    /*
     * The private constructor
     */
    queue_source_byte_impl::queue_source_byte_impl(int size, boost::lockfree::queue< std::vector<char>*, boost::lockfree::fixed_sized<true> > &shared_queue, bool preserve_index, bool order_data)
      : gr::sync_block("queue_source_byte",
              gr::io_signature::make(0, 0, 0),
              gr::io_signature::make(1, 1, size)), queue(&shared_queue), item_size(size), preserve(preserve_index), order(order_data)
    {
      set_output_multiple(50); // 50 bytes per packet
      dead = false;

      if(VERBOSE)
        myfile.open("queue_source_byte.data");

      global_index = 0;

      if(VERBOSE){
        myfile << "Calling QUEUE_SOURCE_BYTE Constructor\n\n";
        myfile << std::flush;
      }

      found_kill = false;

    }

    /*
     * Our virtual destructor.
     */
    queue_source_byte_impl::~queue_source_byte_impl()
    {

      if(VERBOSE){
        std::cout << "*Calling Queue_Source_Byte Destructor*" << std::endl;
        myfile << "Calling Queue_Source_Byte Destructor\n";
        myfile << std::flush;
        myfile.close();
      }
    }

    int
    queue_source_byte_impl::work(int noutput_items,
			  gr_vector_const_void_star &input_items,
			  gr_vector_void_star &output_items)
    {
        char *out = (char *) output_items[0];

        std::vector<char> *temp_vector;
        std::vector<char> buffer;

        float index;
        int data_size;

        if(queue->pop(temp_vector)){

          char type = temp_vector->at(0);

          switch(type){
            case '2':
              memcpy(&index, &(temp_vector->at(1)), 4);
              memcpy(&data_size, &(temp_vector->at(5)), 4);

              //std::cout << "Popped [" << type << "], [" << index << "], [" << data_size << "]" << std::endl;

              memcpy(out, &(temp_vector->at(9)), data_size);

              delete temp_vector;

              return data_size;
            break;

          }

        }
        else{
          boost::this_thread::sleep(boost::posix_time::microseconds(100));
          return 0;
        }
    }

  } /* namespace router */
} /* namespace gr */

