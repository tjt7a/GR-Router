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
 *  This is the  throughput block. It prints the average throughput to std::out every d_print_counter times work() is called.
 *
 *  The following code is derived from the throttle block.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <boost/thread/thread_time.hpp>
#include <gnuradio/io_signature.h>
#include "throughput_impl.h"

namespace gr {
    namespace router {
        
        /*!
         *  This is the public constructor for the throughput block.
         *
         *  @param itemsize The size (in bytes) of the data being measured
         *  @param print_counter The number of work() calls between prints of throughput value
         *  @param index How many tabs in front of the throughput value to space them
         *  @return A shared pointer to the throughput block
         */
        
        throughput::sptr
        throughput::make(size_t itemsize, int print_counter, int index)
        {
            return gnuradio::get_initial_sptr
            (new throughput_impl(itemsize, print_counter, index));
        }
        
        /*!
         *  This is the private constructor for the throughput block.
         *
         *  @param itemsize The size (in bytes) of the data being measured
         *  @param print_counter The number of work() calls between prints of throughput value
         *  @param index How many tabs in front of the throughput value to space them
         */
        
        throughput_impl::throughput_impl(size_t itemsize, int print_counter, int index)
        : gr::sync_block("throughput",
                         gr::io_signature::make(1, 1, itemsize),
                         gr::io_signature::make(1, 1, itemsize)), d_itemsize(itemsize), d_print_counter(print_counter), d_index(index)
        {
            d_start = boost::get_system_time();
            d_total_samples = 0;
            last_throughput = 0;
            running_count = 0;
        }
        
        /**
         *  The destructor for the throughput block.
         */
        throughput_impl::~throughput_impl()
        {
            // Destructor code
        }
      
        /*!
         *  This is the work() function. It prints the average throughput to std::out every d_print_counter times work() is called.
         *
         *  This block is transparent and the data on the input is copied to the output.
         *
         *  @param noutput_items The number of data samples
         *  @param &input_items Pointer to input vector
         *  @param &output_items Pointer to output vector
         */
        
        int
        throughput_impl::work(int noutput_items,
                              gr_vector_const_void_star &input_items,
                              gr_vector_void_star &output_items)
        {
            const char *in = (const char *) input_items[0];
            char *out = (char *) output_items[0];
            
            boost::system_time now = boost::get_system_time();
            double ticks = (now - d_start).ticks(); // Total number of ticks since start
            
            double time_for_ticks = ticks / boost::posix_time::time_duration::ticks_per_second(); // Total time since start
            
            // throughput (in Samples per second) = total samples / total time
            double throughput = (d_total_samples / time_for_ticks);
            
            d_total_samples += (double)noutput_items;
            
            running_count++;
            
            // Print out the throughput every d_print_counter times work() is called
            if((int)running_count % d_print_counter == 0){
                for(int i = 0; i < d_index; i++)
                    std::cout << '\t';
                
                
                std::cout << d_index << ". " << throughput << std::endl;
            }
			
            std::memcpy(out, in, noutput_items * d_itemsize);
            
            return noutput_items;
        }
        
    } /* namespace router */
} /* namespace gr */
