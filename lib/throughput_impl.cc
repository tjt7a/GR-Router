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
	The following code is derived from the throttle block
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <boost/thread/thread_time.hpp>
#include <gnuradio/io_signature.h>
#include "throughput_impl.h"

namespace gr {
  namespace router {

    throughput::sptr
    throughput::make(size_t itemsize, double print_counter)
    {
      return gnuradio::get_initial_sptr
        (new throughput_impl(itemsize, print_counter));
    }

    /*
     * The private constructor
     */
    throughput_impl::throughput_impl(size_t itemsize, double print_counter)
      : gr::sync_block("throughput",
              gr::io_signature::make(1, 1, itemsize),
              gr::io_signature::make(1, 1, itemsize)), d_itemsize(itemsize), d_print_counter(print_counter)
    {
	   d_start = boost::get_system_time();
	   d_last_samples = 0;
	   current_count = 0;
	   last_throughput = 0;
	   //smoothing_coeff = 0.1;
     	   //running_count = 0;
     	   //running_sum = 0;
	 }

    /*
     * Our virtual destructor.
     */
    throughput_impl::~throughput_impl()
    {
    }

    int
    throughput_impl::work(int noutput_items,
			  gr_vector_const_void_star &input_items,
			  gr_vector_void_star &output_items)
    {
        const char *in = (const char *) input_items[0];
        char *out = (char *) output_items[0];

	boost::system_time now = boost::get_system_time();
	double ticks = (now - d_start).ticks();
	d_start = now;

	double time_for_ticks = ticks / boost::posix_time::time_duration::ticks_per_second();

	double throughput = (d_last_samples / time_for_ticks)/d_itemsize;
	d_last_samples = (double)noutput_items;

	       //last_throughput += (smoothing_coeff)*(throughput - last_throughput);

         //running_sum += throughput/1e6;
         //running_count++;

	      // if(current_count == d_print_counter){

	std::cout << std::setprecision(3) << throughput/1e6 << std::endl;
		        //std::cout << std::setprecision(3) << "Throughput: "<< (throughput/1e6) << " Ms/s"<< "\t\t" << "Smoothed: " << (last_throughput/1e6) << " Ms/s" << std::endl;
		        //std::cout << (throughput/1e6) << "\t\t Running Average: "<< running_sum/running_count << std::endl;
            //current_count = 0;
	      // }
	       //else{
	//	        current_count++;	
	  //     }
			
	std::memcpy(out, in, noutput_items * d_itemsize);

        return noutput_items;
    }

  } /* namespace router */
} /* namespace gr */
