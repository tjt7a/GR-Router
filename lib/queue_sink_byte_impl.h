/* -*- c++ -*- */
/* 
 * Copyright 2014 <+YOU OR YOUR COMPANY+>.
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

#ifndef INCLUDED_ROUTER_QUEUE_SINK_BYTE_IMPL_H
#define INCLUDED_ROUTER_QUEUE_SINK_BYTE_IMPL_H

#include <router/queue_sink_byte.h>
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

    class queue_sink_byte_impl : public queue_sink_byte
    {
     private:
        std::ofstream myfile;
        int number_of_windows;
        int left_over;
        int out_mult;

        std::vector<gr::tag_t> tags;
        const char* symbol;

        boost::lockfree::queue< std::vector<char>*, boost::lockfree::fixed_sized<true> > *queue;
        int queue_counter;
        int item_size;

        std::vector<char> *window;
        std::vector<float> *index_vector;

        float index_of_window;
        bool preserve;

        char* get_index();

        bool waiting_on_window;


     public:
      queue_sink_byte_impl(int item_size, boost::lockfree::queue< std::vector<char>*, boost::lockfree::fixed_sized<true> > &shared_queue, bool preserve_index);

      ~queue_sink_byte_impl();

      // Where all the action really happens
      int work(int noutput_items,
	       gr_vector_const_void_star &input_items,
	       gr_vector_void_star &output_items);
    };

  } // namespace router
} // namespace gr

#endif /* INCLUDED_ROUTER_QUEUE_SINK_BYTE_IMPL_H */

