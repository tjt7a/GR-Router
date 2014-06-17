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
  This is a test application for the LDPC application written by Manu
*/

// Include header files for each block used in flowgraph
#include <gnuradio/blocks/wavfile_source.h>
#include <gnuradio/blocks/file_sink.h>
#include <gnuradio/blocks/throttle.h>
#include <boost/lockfree/queue.hpp>
#include <router/queue_source.h>
#include <gnuradio/top_block.h>
#include <router/queue_sink.h> 
#include <router/throughput.h>
#include <router/throughput_sink.h>
#include <boost/thread.hpp>
#include <router/root.h>
#include "fft_ifft.h"
#include <vector>
#include <cstdio>

// For the LDPC blocks
#include "ldpc_decoder_bb_imp.h"

using namespace gr;

int main(int argc, char **argv)
{

  double throughput_value = 2e6; // Set the throughput of the throttle


  /*
  * Create a Top Block for the flowgraph
  */

  gr::top_block_sptr tb_1 = gr::make_top_block("Parent Router");

  const char* in_file_name = "inputs/out.wav"; // Location of the input WAV file
  const char* out_file_name = "/dev/null"; // Where the output will be saved (for metrics, it will be dumped)
  int number_of_children = 1;

  if(argc > 1)
	   number_of_children = atoi(argv[1]);

  /*
  * Input wavfile source for streaming data into the flowgraph from a WAV source file; enabled repeat
  * Output file sink for streaming output data to a file (in this case /dev/null)
  */

  // Wavfile source to read in the input WAV file
  gr::blocks::wavfile_source::sptr wavfile_source = gr::blocks::wavfile_source::make(in_file_name, true); // input file source (WAV) [input_file, repeat=false]
  gr::blocks::file_sink::sptr file_sink = gr::blocks::file_sink::make(sizeof(float), out_file_name); // output file sink (BIN) [sizeof(float), output_file]

  /*
  * Input and Output queues to hold vector 'message' addresses
  */
  
  boost::lockfree::queue< std::vector<float>*, boost::lockfree::fixed_sized<true> > input_queue(100);
  boost::lockfree::queue< std::vector<float>*, boost::lockfree::fixed_sized<true> > output_queue(100);

  /*
  * Load Balancing Router to distribute windows to children
  */

  gr::router::root::sptr root_router = gr::router::root::make(number_of_children, input_queue, output_queue, throughput_value); // parent router [1 child, input queue, output queue]
 
  /*
  * Throttle: Throttles set maximum throughput from the WAV file (this can be tuned to the maximum obtainable throughput)
  */

  gr::blocks::throttle::sptr throttle_0 = gr::blocks::throttle::make(sizeof(float), throughput_value);


  /*
  * Input queue Sink: Takes streams from a flow graph, packetizes, slaps on headers, and pushes the result into the input queue; last argument indicates if index is to be preserved from the stream tags
  * Input queue Source: Grabs windows from the input queue, depacketizes, pulls out the index, and streams through flowgraph; arguments: preserve index (stream tags), order (guarantee order of windows before streaming)
  */

  gr::router::queue_sink::sptr input_queue_sink = gr::router::queue_sink::make(sizeof(float), input_queue, false); // input queue sink [sizeof(float, input_queue, preserve index after = true)]
  gr::router::queue_source::sptr input_queue_source = gr::router::queue_source::make(sizeof(float), input_queue, false, false); // input queue source [sizeof(float), input_queue, preserve index = true, order = true]

  /*
  * Output queue Sink: Takes streams from the flow graph, packetized, slaps on headers, and pushes the result into the output queue; last argument indicates if the index is to be preserved from the stream tags
  * Output queue Source: Grabs windows from the output queue, depacketizes, pulls out the index, and streams through flowgraph; arguments: preserve index (stream tags), order (guarrantee order of windows befoe streaming)
  */

  gr::router::queue_sink::sptr output_queue_sink = gr::router::queue_sink::make(sizeof(float), output_queue, false);
  gr::router::queue_source::sptr output_queue_source = gr::router::queue_source::make(sizeof(float), output_queue, false, false); // Preserve index, order data, write file

  /*
  * Throughput: This block prints out the throughput of the Flowgraph in MegaSamples per second; first argument is the number of work() functions to be called between std::cout statements; the second argument is the index of the throughput block
  */

  //gr::router::throughput::sptr throughput = gr::router::throughput::make(sizeof(float), 10, 0);
  gr::router::throughput_sink::sptr throughput_sink = gr::router::throughput_sink::make(sizeof(float), 10, 0);



  tb_1->connect(wavfile_source, 0, throttle_0, 0);
  tb_1->connect(throttle_0, 0, input_queue_sink, 0);


  /*
  * Handler Code
  * Chain of 50 FFTs that processes a window of 1024 values from the input_queue, and dumps the resulting output window into the output_queue
  */


  /*
  * Sink Code
  * Connects the output_queue to a throughput block to get the throughput of the system, and then pass the results to the output file (/dev/null)
  */

  tb_1->connect(output_queue_source, 0, throughput_sink, 0);
  tb_1->connect(output_queue_source, 0, file_sink, 0);

  // Run flowgraph
  tb_1->run();

  return 0;
}
