/*
 * Copyright 2013 Tommy Tracy II (University of Virginia) 
 *
 *
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */


/*
 * GNU Radio C++ Gr-Router Example
 * This example uses a change of FFT/IFFTs for the test
 *
 * GNU Radio makes extensive use of Boost shared pointers.  Signal processing
 * blocks are typically created by calling a "make" factory function, which
 * returns an instance of the block as a typedef'd shared pointer that can
 * be used in any way a regular pointer can.  Shared pointers created this way
 * keep track of their memory and free it at the right time, so the user
 * doesn't need to worry about it (really).
 *
 */

// Include header files for each block used in flowgraph
#include <gnuradio/top_block.h>
#include <gnuradio/blocks/wavfile_source.h>
#include <gnuradio/blocks/file_sink.h>
#include <vector>
#include <boost/lockfree/queue.hpp>
#include <boost/thread.hpp>
#include <router/queue_sink.h> 
#include <router/queue_source.h>
#include <router/child.h>
#include "fft_ifft.h"
#include <cstdio>
#include "fft_ifft.h"

using namespace gr;

int main(int argc, char **argv)
{
   // Construct a top block that will contain flowgraph blocks.  Alternatively,
  // one may create a derived class from top_block and hold instantiated blocks
  // as member data for later manipulation.

  gr::top_block_sptr tb = gr::make_top_block("fft_ifft_child");

  int window_size = 1024;
  int fft_count = 50;


  // Lockfree queues for input and output windows
  boost::lockfree::queue< std::vector<float>* > input_queue(1025);
  boost::lockfree::queue< std::vector<float>* > output_queue(1025);

  GR_LOG_INFO(d_logger, "1. Created input/output queues");

  gr::router::child::sptr child_router = gr::router::child::make(0, 0, "localhost", input_queue, output_queue);

  GR_LOG_INFO(d_logger, "2. Finished building child router");

  gr::router::queue_sink::sptr input_queue_sink = gr::router::queue_sink::make(sizeof(float), input_queue, true); // input queue sink [sizeof(float, input_queue, preserve index after = true)]
  gr::router::queue_source::sptr input_queue_source = gr::router::queue_source::make(sizeof(float), input_queue, true, false); // input queue source [sizeof(float), input_queue, preserve index = true, order = true]

  GR_LOG_INFO(d_logger, "3. Finished building input queue sink/source");

  gr::router::queue_sink::sptr output_queue_sink = gr::router::queue_sink::make(sizeof(float), output_queue, true);
  gr::router::queue_source::sptr output_queue_source = gr::router::queue_source::make(sizeof(float), output_queue, true, false);

  GR_LOG_INFO(d_logger, "4. Finished building output queue sink/source");


  std::vector<fft_ifft_sptr> ffts;
  for(int i = 0; i < fft_count; i++){
  	ffts.push_back(fft_ifft_make(1024));
  	if(i == 0){
  		tb->connect(input_queue_source, 0, ffts.at(0), 0);
  	}
  	else{
  		tb->connect(ffts[i-1], 0, ffts[i], 0);
  	}
  }

  tb->connect(ffts.at(ffts.size()-1), 0, output_queue_sink, 0);

  GR_LOG_INFO(d_logger, "5. Finished connecting all blocks.");

  tb->run();

  // Exit normally.
  return 0;
}
