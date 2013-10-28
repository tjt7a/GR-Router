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
#include "fft_ifft.h"
#include <cstdio>
using namespace gr;

int main(int argc, char **argv)
{

  gr::top_block_sptr tb_1 = gr::make_top_block("fft_ifft_parent");

  int window_size = 1024;
  int fft_count = 50;
  const char* in_file_name = "input.in";
  const char* out_file_name = "output.out";

  // Wavfile source to read in the input WAV file
  gr::blocks::wavfile_source::sptr wavfile_source = gr::blocks::wavfile_source::make(in_file_name, false); // input file source (WAV) [input_file, repeat=false]
  gr::blocks::file_sink::sptr file_sink = gr::blocks::file_sink::make(sizeof(float), out_file_name); // output file sink (BIN) [sizeof(float), output_file]
  

  boost::shared_ptr<boost::lockfree::queue< std::vector<float>* > > input_queue(new boost::lockfree::queue<std::vector<float>* >()); // input queue
  boost::shared_ptr<boost::lockfree::queue< std::vector<float>* > > output_queue(new boost::lockfree::queue<std::vector<float>* >()); // output queue

  gr::router::queue_sink::sptr input_queue_sink = gr::router::queue_sink::make(sizeof(float), input_queue, true); // input queue sink [sizeof(float, input_queue, preserve index after = true)]
  gr::router::queue_source::sptr input_queue_source = gr::router::queue_source::make(sizeof(float), input_queue, true, false);

  gr::router::queue_sink::sptr output_queue_sink = gr::router::queue_sink::make(sizeof(float), output_queue, true);
  gr::router::queue_source::sptr output_queue_source = gr::router::queue_source::make(sizeof(float), output_queue, false, true);

  tb_1->connect(wavfile_source, 0, input_queue_sink, 0);

  std::vector<fft_ifft_sptr> ffts;
  for(int i = 0; i < fft_count; i++){
  	ffts.push_back(fft_ifft_make(1024));
  	if(i == 0){
  		tb_1->connect(input_queue_source, 0, ffts[0], 0);
  	}
  	else{
  		tb_1->connect(ffts[i-1], 0, ffts[i], 0);
  	}
  }

  tb_1->connect(wavfile_source, 0, output_queue_sink, 0);

  tb_1->connect(output_queue_source, 0, file_sink, 0);
  printf("Finished building tb_1\n");

  tb_1->run();
  // Exit normally.
  return 0;
}