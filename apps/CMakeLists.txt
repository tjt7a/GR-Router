# Copyright 2011 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# GNU Radio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# GNU Radio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Radio; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.

include_directories(
        ${GR_AUDIO_INCLUDE_DIRS}
        ${GR_ANALOG_INCLUDE_DIRS}
        ${GNURADIO_RUNTIME_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS}
	${GR_BLOCKS_INCLUDE_DIRS}
)

add_executable(dial_tone ${CMAKE_CURRENT_SOURCE_DIR}/dial_tone.cc)
add_executable(fft_ifft_test ${CMAKE_CURRENT_SOURCE_DIR}/fft_ifft_test.cc ${CMAKE_CURRENT_SOURCE_DIR}/fft_ifft.cc)
add_executable(router_fft_test_parent ${CMAKE_CURRENT_SOURCE_DIR}/router_fft_test_parent.cc ${CMAKE_CURRENT_SOURCE_DIR}/fft_ifft.cc)
add_executable(router_fft_test_child ${CMAKE_CURRENT_SOURCE_DIR}/router_fft_test_child.cc ${CMAKE_CURRENT_SOURCE_DIR}/fft_ifft.cc)
add_executable(transparent ${CMAKE_CURRENT_SOURCE_DIR}/transparent.cc ${CMAKE_CURRENT_SOURCE_DIR}/fft_ifft.cc)

target_link_libraries(dial_tone
  ${Boost_LIBRARIES} ${GNURADIO_RUNTIME_LIBRARIES} ${GNURADIO_ANALOG_LIBRARIES} ${GNURADIO_AUDIO_LIBRARIES} ${GNURADIO_BLOCKS_LIBRARIES})

target_link_libraries(fft_ifft_test
  ${BOOST_LIBRARIES} ${BOOST_SYSTEM_LIBRARIES} ${GNURADIO_RUNTIME_LIBRARIES} ${GNURADIO_ANALOG_LIBRARIES} ${GNURADIO_AUDIO_LIBRARIES} ${GNURADIO_BLOCKS_LIBRARIES} ${GR_ROUTER_LIBRARIES} ${BOOST_SYSTEM_LIBRARIES} ${GNURADIO_FFT_LIBRARIES} ${GNURADIO_PMT_LIBRARIES} ${GNURADIO_FILTER_LIBRARIES} )

target_link_libraries(router_fft_test_parent
  ${BOOST_LIBRARIES} ${BOOST_SYSTEM_LIBRARIES} ${GNURADIO_RUNTIME_LIBRARIES} ${GNURADIO_ANALOG_LIBRARIES} ${GNURADIO_AUDIO_LIBRARIES} ${GNURADIO_BLOCKS_LIBRARIES} ${GR_ROUTER_LIBRARIES} ${BOOST_SYSTEM_LIBRARIES} ${GNURADIO_FFT_LIBRARIES} ${GNURADIO_PMT_LIBRARIES} ${GNURADIO_FILTER_LIBRARIES} )

target_link_libraries(router_fft_test_child
 ${BOOST_LIBRARIES} ${BOOST_SYSTEM_LIBRARIES} ${GNURADIO_RUNTIME_LIBRARIES} ${GNURADIO_ANALOG_LIBRARIES} ${GNURADIO_AUDIO_LIBRARIES} ${GNURADIO_BLOCKS_LIBRARIES} ${GR_ROUTER_LIBRARIES} ${BOOST_SYSTEM_LIBRARIES} ${GNURADIO_FFT_LIBRARIES} ${GNURADIO_PMT_LIBRARIES} ${GNURADIO_FILTER_LIBRARIES} )

target_link_libraries(transparent
 ${BOOST_LIBRARIES} ${BOOST_SYSTEM_LIBRARIES} ${GNURADIO_RUNTIME_LIBRARIES} ${GNURADIO_ANALOG_LIBRARIES} ${GNURADIO_AUDIO_LIBRARIES} ${GNURADIO_BLOCKS_LIBRARIES} ${GR_ROUTER_LIBRARIES} ${BOOST_SYSTEM_LIBRARIES} ${GNURADIO_FFT_LIBRARIES} ${GNURADIO_PMT_LIBRARIES} ${GNURADIO_FILTER_LIBRARIES})

INSTALL(TARGETS
    dial_tone fft_ifft_test router_fft_test_parent router_fft_test_child transparent 
    DESTINATION "/home/tjt7a/src/GR-Router/bin"
)
