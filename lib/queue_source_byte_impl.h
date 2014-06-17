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

#ifndef INCLUDED_ROUTER_QUEUE_SOURCE_BYTE_IMPL_H
#define INCLUDED_ROUTER_QUEUE_SOURCE_BYTE_IMPL_H

#include <router/queue_source_byte.h>
#include <vector>
#include <boost/thread.hpp>
#include <algorithm>
#include <boost/lockfree/queue.hpp>
#include <memory>
#include <gnuradio/tagged_stream_block.h>
#include <iostream>
#include <fstream>

namespace gr {
  namespace router {

    class queue_source_byte_impl : public queue_source_byte
    {
     private:

        std::ofstream myfile;

        bool dead;

        int number_of_windows;
        int left_over_values;
        int global_index;

        bool found_kill;

        bool order;
        std::vector<std::vector<char>* > local;

        boost::lockfree::queue< std::vector<char>*, boost::lockfree::fixed_sized<true> > *queue;

        int item_size;

        std::vector<char> window;
        bool preserve;

     public:
      queue_source_byte_impl(int size, boost::lockfree::queue< std::vector<char>*, boost::lockfree::fixed_sized<true> > &shared_queue, bool preserve_index, bool order);
      ~queue_source_byte_impl();

      // Where all the action really happens
      int work(int noutput_items,
	       gr_vector_const_void_star &input_items,
	       gr_vector_void_star &output_items);
    };

  } // namespace router
} // namespace gr

#endif /* INCLUDED_ROUTER_QUEUE_SOURCE_BYTE_IMPL_H */

