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

// Include header files for each block used in flowgraph
#include <gnuradio/top_block.h>
#include <gnuradio/blocks/wavfile_source.h>
#include <gnuradio/blocks/file_sink.h>
#include <vector>
#include <boost/lockfree/queue.hpp>
#include <boost/thread.hpp>
#include <router/queue_sink.h> 
#include <router/queue_source.h>
#include <router/root.h>
#include "fft_ifft.h"
#include <cstdio>
using namespace gr;

int main(int argc, char **argv)
{

  /*
  * Create a Top Block for the flowgraph
  */

  gr::top_block_sptr tb_1 = gr::make_top_block("fft_ifft_parent");

  int window_size = 1024;
  int fft_count = 50;
  const char* in_file_name = "inputs/out.wav";
  const char* out_file_name = "output.out";
  int number_of_children = 1;

  if(argc > 1){
	   number_of_children = atoi(argv[1]);
  }	

  /*
  * Input wavfile source for streaming data into the flowgraph
  * Output file sink for streaming output data to a file
  */

  // Wavfile source to read in the input WAV file
  gr::blocks::wavfile_source::sptr wavfile_source = gr::blocks::wavfile_source::make(in_file_name, false); // input file source (WAV) [input_file, repeat=false]
  gr::blocks::file_sink::sptr file_sink = gr::blocks::file_sink::make(sizeof(float), out_file_name); // output file sink (BIN) [sizeof(float), output_file]

  /*
  * Input and Output queues to hold 'windows' of 1025 floats. 1 index, and 1024 samples
  */

  //boost::shared_ptr<boost::lockfree::queue< std::vector<float>* > > input_queue(new boost::lockfree::queue<std::vector<float>* >(0)); // input queue
  //boost::shared_ptr<boost::lockfree::queue< std::vector<float>* > > output_queue(new boost::lockfree::queue<std::vector<float>* >(0)); // output queue
  boost::lockfree::queue< std::vector<float>* > input_queue(1025);
  boost::lockfree::queue< std::vector<float>* > output_queue(1025);

  /*
  * Load Balancing Router to distribute windows to children
  */

  gr::router::root::sptr root_router = gr::router::root::make(number_of_children, input_queue, output_queue); // parent router [1 child, input queue, output queue]
  
  /*
  * Input queue Sink: Takes streams from a flow graph, packetizes, slaps on an index, and pushes the result into the input queue; last argument indicates if index is to be preserved from the stream tags
  * Input queue Source: Grabs windows from the input queue, depacketizes, pulls out the index, and streams through flowgraph; arguments: preserve index (stream tags), order (guarantee order of windows before streaming)
  */

  //tb_1->connect(wavfile_source, 0, file_sink, 0);
  gr::router::queue_sink::sptr input_queue_sink = gr::router::queue_sink::make(sizeof(float), input_queue, false); // input queue sink [sizeof(float, input_queue, preserve index after = true)]
  gr::router::queue_source::sptr input_queue_source = gr::router::queue_source::make(sizeof(float), input_queue, false, false); // input queue source [sizeof(float), input_queue, preserve index = true, order = true]

  /*
  * Output queue Sink: Takes streams from the flow graph, packetized, slaps on an index, and pushes the result into the output queue; last argument indicates if the index is to be preserved from the stream tags
  * Output queue Source: Grabs windows from the output queue, depacketizes, pulls out the index, and streams through flowgraph; arguments: preserve index (stream tags), order (guarrantee order of windows befoe streaming)
  */

  gr::router::queue_sink::sptr output_queue_sink = gr::router::queue_sink::make(sizeof(float), output_queue, false);
  gr::router::queue_source::sptr output_queue_source = gr::router::queue_source::make(sizeof(float), output_queue, false, false); // Riht now out of order; need to reorder

  tb_1->connect(wavfile_source, 0, input_queue_sink, 0);

  /*
  * Handler Code
  * Chain of 50 FFTs that processes a window of 1024 values from the input_queue, and dumps the resulting output window into the output_queue
  */

  std::vector<fft_ifft_sptr> ffts;
  for(int i = 0; i < fft_count; i++){
  	ffts.push_back(fft_ifft_make(1024));
  	if(i == 0){
  		tb_1->connect(input_queue_source, 0, ffts.at(0), 0);
    }
  	else{
  		tb_1->connect(ffts.at(i-1), 0, ffts.at(i), 0);
  	}
  }
  tb_1->connect(ffts.at(ffts.size()-1), 0, output_queue_sink, 0);

  /*
  * Sink Code
  * Connects the output_queue to the sink
  */

  tb_1->connect(output_queue_source, 0, file_sink, 0);

  // Run flowgraph
  tb_1->run();

  return 0;
}
