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
 The following code is derived from the throttle block
 Unlike the throughput block, this block is a sink.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <boost/thread/thread_time.hpp>
#include <gnuradio/io_signature.h>
#include "throughput_sink_impl.h"

namespace gr {
    namespace router {
        
        /*!
         *  This is the public constructor for the throughput sink block.
         *
         *  @param itemsize The size (in bytes) of the data being measured
         *  @param print_counter The number of work() calls between prints of throughput value
         *  @param index How many tabs in front of the throughput value to space them
         *  @return A shared pointer to the throughput sink block
         */
        
        throughput_sink::sptr
        throughput_sink::make(size_t itemsize, int print_counter, int index)
        {
            return gnuradio::get_initial_sptr
            (new throughput_sink_impl(itemsize, print_counter, index));
        }
        
        /*!
         *  This is the private constructor for the throughput block.
         *
         *  @param itemsize The size (in bytes) of the data being measured
         *  @param print_counter The number of work() calls between prints of throughput value
         *  @param index How many tabs in front of the throughput value to space them
         */
        
        throughput_sink_impl::throughput_sink_impl(size_t itemsize, int print_counter, int index)
        : gr::sync_block("throughput_sink",
                         gr::io_signature::make(1, 1, itemsize),
                         gr::io_signature::make(0, 0, 0)), d_itemsize(itemsize), d_print_counter(print_counter), d_index(index)
        {
            d_start = boost::get_system_time();
            d_total_samples = 0;
            last_throughput = 0;
            running_count = 0;
            
        }
        
        /*!
         *  This is the destructor for the throughput sink block.
         */
        throughput_sink_impl::~throughput_sink_impl()
        {
            // Destruction Code
        }
        
        /*!
         *  This is the work() function of the throughput_sink
         */
        
        int
        throughput_sink_impl::work(int noutput_items,
                                   gr_vector_const_void_star &input_items,
                                   gr_vector_void_star &output_items)
        {
            const char *in = (const char *) input_items[0];
            
            boost::system_time now = boost::get_system_time();
            double ticks = (now - d_start).ticks(); // Total number of ticks since start
            
            double time_for_ticks = ticks / boost::posix_time::time_duration::ticks_per_second(); // Total time since start
            
            double throughput = (d_total_samples / time_for_ticks) / 1e6; // (Total number of samples / total time) in MegaSamples / Second
            d_total_samples += (double)noutput_items;
            
            running_count++;
            
            if((int)running_count % d_print_counter == 0){
                for(int i = 0; i < d_index; i++)
                    std::cout << '\t';
                
                std::cout << d_index << ". " << throughput << std::endl;
            }
            
            // Tell runtime system how many output items we produced.
            return noutput_items;
        }
        
    } /* namespace router */
} /* namespace gr */

