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
  This is a test application for the GR-ROUTER library.
  An input WAV file is read, and the resulting samples are streamed through a series of FFT/IFFT blocks.
  GR-ROUTER distributes the data out to multiple nodes to increase throughput.

*/

// Include header files for each block used in flowgraph
#include <gnuradio/blocks/wavfile_source.h>
#include <gnuradio/blocks/file_sink.h>
#include <boost/lockfree/queue.hpp>
#include <router/queue_source.h>
#include <gnuradio/top_block.h>
#include <router/queue_sink.h>
#include <router/throughput.h>
#include <boost/thread.hpp> 
#include <router/child.h>
#include "fft_ifft.h"
#include <vector>
#include <cstdio>
#include <stdlib.h>

using namespace gr;

int main(int argc, char **argv)
{

  /*
  * Create a Top Block for the flowgraph
  */

  gr::top_block_sptr tb = gr::make_top_block("Child Router");

  int window_size = 1024; // Number of floats per window in FFT
  int fft_count = 50; // Number of FFT/IFFT pairs in a chain as test computation load
  char* parent_name = "localhost"; // Default parent on same machine for debuggin

  int child_index = 0;

  // If the hostname of the parent is given, use it. Otherwise, use 'localhost'
  if(argc > 1)
  	parent_name = argv[1];

  if(argc > 2)
    child_index = atoi(argv[2]);

  /*
  * Input and Output queues to hold 'windows' of 1025 floats. 1 index, and 1024 samples
  */

  boost::lockfree::queue< std::vector<float>*, boost::lockfree::fixed_sized<true> > input_queue(1026);
  boost::lockfree::queue< std::vector<float>*, boost::lockfree::fixed_sized<true> > output_queue(1026);

  /*
  * Load Balancing Router to receive windows from parent
  */

  gr::router::child::sptr child_router = gr::router::child::make(0, child_index, parent_name, input_queue, output_queue, 10e6);

  /*
  * Input queue Sink: Takes streams from a flow graph, packetizes, slaps on an index, and pushes the result into the input queue; last argument indicates if index is to be preserved from the stream tags
  * Input queue Source: Grabs windows from the input queue, depacketizes, pulls out the index, and streams through flowgraph; arguments: preserve index (stream tags), order (guarantee order of windows before streaming)
  */

  //gr::router::queue_sink::sptr input_queue_sink = gr::router::queue_sink::make(sizeof(float), input_queue, false); // input queue sink [sizeof(float, input_queue, preserve index after = true)]
  gr::router::queue_source::sptr input_queue_source = gr::router::queue_source::make(sizeof(float), input_queue, true, false); // input queue source [sizeof(float), input_queue, preserve index = true, order = true]

  /*
  * Output queue Sink: Takes streams from the flow graph, packetized, slaps on an index, and pushes the result into the output queue; last argument indicates if the index is to be preserved from the stream tags
  * Output queue Source: Grabs windows from the output queue, depacketizes, pulls out the index, and streams through flowgraph; arguments: preserve index (stream tags), order (guarrantee order of windows befoe streaming)
  */

  gr::router::queue_sink::sptr output_queue_sink = gr::router::queue_sink::make(sizeof(float), output_queue, true); // Construct from stream index?
  //gr::router::queue_source::sptr output_queue_source = gr::router::queue_source::make(sizeof(float), output_queue, false, false, false);


  gr::router::throughput::sptr throughput = gr::router::throughput::make(sizeof(float), 2, 0);



  /*
  * Handler Code
  * Chain of 50 FFTs that processes a window of 1024 values from the input_queue, and dumps the resulting output window into the output_queue
  */

  std::vector<fft_ifft_sptr> ffts;
  for(int i = 0; i < fft_count; i++){
  	ffts.push_back(fft_ifft_make(1024));
  	if(i == 0){
  		tb->connect(input_queue_source, 0, ffts.at(0), 0);
  	}
  	else{
  		tb->connect(ffts.at(i-1), 0, ffts.at(i), 0);
  	}
  } 
  tb->connect(ffts.at(ffts.size()-1), 0, throughput, 0);
	
  tb->connect(throughput, 0, output_queue_sink, 0);


  // Run flowgraph
  tb->run();

  // Exit normally.
  return 0;
}
