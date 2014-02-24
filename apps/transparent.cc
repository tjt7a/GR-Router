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
#include <gnuradio/blocks/wavfile_source.h>
#include <gnuradio/blocks/file_sink.h>
#include <boost/lockfree/queue.hpp>
#include <router/queue_source.h>
#include <gnuradio/top_block.h>
#include <router/queue_sink.h> 
#include <router/throughput.h>
#include <boost/thread.hpp>
#include "fft_ifft.h"
#include <vector>
#include <cstdio>

using namespace gr;

int main(int argc, char **argv)
{

  gr::top_block_sptr tb_1 = gr::make_top_block("transparent");

  const char* in_file_name = "inputs/out.wav";
  const char* out_file_name = "transparent.out";

  boost::lockfree::queue< std::vector<float>* > input_queue(1026);

  gr::blocks::wavfile_source::sptr wavfile_source = gr::blocks::wavfile_source::make(in_file_name, true); // input file source (WAV) [input_file, repeat=false]
  gr::blocks::file_sink::sptr file_sink = gr::blocks::file_sink::make(sizeof(float), out_file_name); // output file sink (BIN) [sizeof(float), output_file]

  gr::router::queue_sink::sptr input_queue_sink = gr::router::queue_sink::make(sizeof(float), input_queue, false); // input queue sink [sizeof(float, input_queue, preserve index after = true)]
  gr::router::queue_source::sptr input_queue_source = gr::router::queue_source::make(sizeof(float), input_queue, false, false, false); // input queue source [sizeof(float), input_queue, preserve index = true, order = true]

  gr::router::throughput::sptr throughput = gr::router::throughput::make(sizeof(float), 2);
  
   tb_1->connect(wavfile_source, 0, throughput, 0);
   tb_1->connect(throughput, 0, input_queue_sink, 0);
  //tb_1->connect(wavfile_source, 0, input_queue_sink, 0);
 
   //tb_1->connect(input_queue_source, 0, throughput, 0);
  //tb_1->connect(throughput, 0, file_sink, 0);
   tb_1->connect(input_queue_source, 0, file_sink, 0);

  //tb_1->connect(wavfile_source, 0, input_queue_source, 0);

  //tb_1->connect(wavfile_source, 0, file_sink, 0);

//tb_1->connect(wavfile_source, 0, input_queue_sink, 0);
/*
  std::vector<fft_ifft_sptr> ffts;
  for(int i = 0; i < 50; i++){
    ffts.push_back(fft_ifft_make(1024));
    if(i == 0){
      tb_1->connect(input_queue_source, 0, ffts.at(0), 0);
      //tb_1->connect(wavfile_source, 0, ffts.at(0), 0);
    }
    else{
      tb_1->connect(ffts.at(i-1), 0, ffts.at(i), 0);
    }
  }
  //tb_1->connect(ffts.at(ffts.size()-1), 0, output_queue_sink, 0);
  tb_1->connect(ffts.at(ffts.size()-1), 0, file_sink, 0);
*/
  tb_1->run();

  return 0;
}
