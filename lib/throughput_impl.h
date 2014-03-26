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

#ifndef INCLUDED_ROUTER_THROUGHPUT_IMPL_H
#define INCLUDED_ROUTER_THROUGHPUT_IMPL_H

#include <router/throughput.h>

namespace gr {
  namespace router {

    class throughput_impl : public throughput
    {
     private:
	   boost::system_time d_start;
	   size_t d_itemsize;
	   double d_total_samples;

	   int d_print_counter;
	   double last_throughput;
     double running_count;
	
     public:
      throughput_impl(size_t itemsize, int print_counter);
      ~throughput_impl();

      // Where all the action really happens
      int work(int noutput_items,
	       gr_vector_const_void_star &input_items,
	       gr_vector_void_star &output_items);
    };

  } // namespace router
} // namespace gr

#endif /* INCLUDED_ROUTER_THROUGHPUT_IMPL_H */

