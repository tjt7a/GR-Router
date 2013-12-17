#!/usr/bin/env python

def test(in_filename, out_filename, fft_count):

	from gnuradio import analog
	from gnuradio import eng_notation
	from gnuradio import fft
	from gnuradio import gr
	from gnuradio import filter
	from gnuradio.eng_option import eng_option
	from gnuradio.filter import firdes
	from optparse import OptionParser
	from time import time
	from grc_gnuradio import blks2 as grc_blks2
	from gnuradio import blocks

	class FFT_IFFT(gr.hier_block2):
		def __init__(self, window_size):
			gr.hier_block2.__init__(self, "FFT_IFFT", gr.io_signature(1,1,gr.sizeof_float), gr.io_signature(1,1,gr.sizeof_float))
			vector_to_stream = blocks.vector_to_stream(gr.sizeof_gr_complex, window_size)
			stream_to_vector = blocks.stream_to_vector(gr.sizeof_float, window_size)
			divide = blocks.divide_cc(1)
			complex_to_float = blocks.complex_to_float(1)
			fft_forward = fft.fft_vfc(window_size, True, (fft.window.blackmanharris(window_size)), 1)
			fft_backward = fft.fft_vcc(window_size, False, (fft.window.blackmanharris(window_size)), False, 1)
			constant = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, window_size)

			#print("1. Connect self to stream_to_vector")
			self.connect((self, 0), (stream_to_vector, 0))
			#print("2. Connect stream_to_vector to fft_forward")
			self.connect((stream_to_vector, 0), (fft_forward, 0))
			#print("3. Connect fft_forward to fft_backward")
			self.connect((fft_forward, 0), (fft_backward, 0))
			#print("4. Connect fft_backward to vector_to_stream")
			self.connect((fft_backward, 0), (vector_to_stream, 0))
			#print("5. Connect vector_to_stream to port 0 of divide")
			self.connect((vector_to_stream, 0), (divide, 0))
			#print("6. Connect constant to port 1 of divide")
			self.connect(constant, (divide, 1))
			#print("7. Connect divide to complex_to_float")
			self.connect((divide, 0), (complex_to_float, 0))
			#print("8. Connect complex_to_float to self")
			self.connect((complex_to_float, 0), (self, 0))

	class fft_test(gr.top_block):

		def __init__(self, in_filename, out_filename, fft_count):
			gr.top_block.__init__(self, "Fft Test")
			
			windowSize = 1024
			print "FFT Count: ", fft_count
			 
			self.wavfile_source = blocks.wavfile_source(in_filename, False)
			self.file_sink = blocks.file_sink(gr.sizeof_float, out_filename)

			ffts = []
			for i in range(fft_count):
				ffts.append(FFT_IFFT(windowSize))
				if i == 0:
					self.connect((self.wavfile_source, 0), (ffts[0], 0))
				else:
					self.connect((ffts[i-1], 0), (ffts[i], 0))

			self.connect((ffts[fft_count-1], 0), (self.file_sink, 0))
			#self.connect((self.wavfile_source, 0), (self.file_sink, 0))

	tb = fft_test(in_filename, out_filename, fft_count)
	tb.run()

import sys
from gnuradio import fft

# Parse Input
in_file = "inputs/out.wav" #str(sys.argv[1])
out_file = "tr_fft_out" #str(sys.argv[2])
fft_count = 50 #int(sys.argv[3])

#Execute test
test(in_file, out_file, fft_count)
