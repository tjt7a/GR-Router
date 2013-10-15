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

#ifndef INCLUDED_FFT_IFFT_H
#define INCLUDED_FFT_IFFT_H

#include <gr_hier_block2.h>

class fft_ifft;

typedef boost::shared_ptr<fft_ifft> fft_ifft_sptr;


/*!
 * \brief Return a shared_ptr to a new instance of fft_ifft.
 *
 * This is effectively the public constructor. To avoid accidental use
 * of raw pointers, fcd_source_c's constructor is private.
 * fft_make_ifft is the public interface for creating new instances.
 */
fft_ifft_sptr fft_make_ifft(int size);

// Hier2 block for fft_ifft block
class fft_ifft : public gr_hier_block2
{

public:
    fft_make_ifft(int size);
    ~fft_ifft();

private:

};

#endif /* INCLUDED_FFT_IFFT_H */