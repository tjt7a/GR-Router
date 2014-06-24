/* -*- c++ -*- */
/*
 *  Written by Tommy Tracy II (University of Virginia HPLP) 2014
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 
 */

#ifndef NETWORKINTERFACE_H
#define NETWORKINTERFACE_H

#include <cstdio>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include "EthernetConnector.h"

#ifdef HAVE_IO_H
#include <io.h>
#endif

#define V   false

class NetworkInterface{
public:
    
	// Default Constructor/Destructor
	NetworkInterface(int itemsize, int children, int port, bool root);
	~NetworkInterface();
    
    // Build connection graph
    bool connect(char* parent_hostname);
    
    // Receive
    int receive(int child_index, char * outbuf, int noutput_items);
    
    // Send msg to child at index child_index
    int send(int child_index, char* msg, int num);
    
private:
    
    // Private functions
    int read_items(int child_index, char *buf, int nitems);
    int handle_residue(char *buf, int nbytes_read);
    void flush_residue(){d_residue_len = 0; }
    
    
    EthernetConnector *connector;
    int children;
    int port;
    bool root;
    size_t d_itemsize; //# Size of the items to be sent/received
    
    // For receiving
    unsigned char *d_residue;
    unsigned long d_residue_len;
};

#endif
