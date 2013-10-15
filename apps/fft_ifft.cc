/* -*- c++ -*- */
/*
 * Copyright 2013 Tommy James Tracy II (University of Virginia)
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <fft_ifft.h>
#include <gr_io_signature.h>
#include <gr_float_to_complex.h>
#include <gnuradio/hier_block2.h>
#include <gnuradio/blocks/vector_to_stream.h>
#include <gnuradio/blocks/stream_to_vector.h>
#include <gnuradio/blocks/divide_cc.h>
#include <gnuradio/blocks/complex_to_float.h>
#include <gnuradio/fft/fft_vfc.h>
#include <gnuradio/fft/fft_vcc.h>
#include <vector>

/*
 * The public constructor.
 */
fft_ifft_sptr fft_make_ifft(const std::string device_name)
{
    return gnuradio::get_initial_sptr(new fft_ifft(device_name));
}

//fft/ifft hierarchical block
 fft_ifft::fft_ifft(int SIZE)
        :gr_hier_block2("fft_ifft", 
                            gr_make_io_signature(1, 1, sizeof(gr_float)),
                            gr_make_io_signature(1, 1, sizeof(gr_float)))
 {

    // Number of threads to execute FFT
    int num_threads = 1;

    // Create shared_ptrs for each block (less cleanup)
    gr::blocks::stream_to_vector::sptr stream_to_vector = gr::blocks::stream_to_vector::make(sizeof(gr::float), SIZE);
    gr::blocks::vector_to_stream::sptr vector_to_stream = gr::blocks::vector_to_stream::make(sizeof(gr::float), SIZE);
    gr::blocks::divide_cc divide::sptr = gr::blocks::divide_cc::make(1);
    gr::blocks::complex_to_float::sptr complex_to_float = gr::blocks::complex_to_float::make(1);
    gr::fft::fft_vfc::sptr fft_forward = gr::fft::fft_vfc::make(SIZE, true, gr::filter::firdes::WIN_BLACKMAN_HARRIS, num_threads); // Window might be wrong
    gr::fft::fft:vcc::sptr fft_reverse = gr::fft::fft_vcc::make(SIZE, false, gr::filter::firdes::WIN_BLACKMAN_HARRIS, false, num_threads); // Window might be wrong
    gr::analog::sig_source_c::sptr constant = gr::analog::sig_source_c::make(0, gr::analog::GR_CONST_WAVE, 0, SIZE, 0);

    // Connect the blocks in the hierarchical block
    connect(self(), 0, stream_to_vector, 0);
    connect(stream_to_vector, 0, fft_forward, 0);
    connect(fft_forward, 0, fft_backward, 0);
    connect(fft_backward, 0, vector_to_stream, 0);
    connect(vector_to_stream, 0, divide, 0);  
    connect(constant, 0, divide, 1);
    connect(divide, 0, complex_to_float, 0);
    connect(complex_to_float, 0, self(), 0);
 }

// Destructor
fft_ifft:~fft_ifft ()
{

}