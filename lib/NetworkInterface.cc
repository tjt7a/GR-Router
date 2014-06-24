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

#include "NetworkInterface.h"

#include <cstdio>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdexcept>
#include <stdio.h>
#include <iostream>
#include <assert.h>

/*!
 *	Public Constructor for the Network Interface.
 *
 *  @param itemsize The size of the samples (in bytes); For now use sizeof(char)
 *  @param children_count The number of children that this node has.
 *  @param port_arg The port on which this node will communicate on.
 *  @param root_arg True if this node is the root; else, False.
 */

NetworkInterface::NetworkInterface(int itemsize, int children_count, int port_arg, bool root_arg){
    
	d_itemsize = itemsize; // The size of each element in the packet; going to be using bytes = 1
	children = children_count;  // Number of children
	port = port_arg; // Port to connect with
	root = root_arg;  // Is this node the Root node?
	d_residue  = new unsigned char[itemsize];
	d_residue_len = 0;
    
	// Create Ethernet Connector
	connector = new EthernetConnector(children, port);
}

/// Destructor
NetworkInterface::~NetworkInterface(){
	delete [] d_residue;
	delete connector;
}

/*!
 *	Connect function: The current node connects to it's parent and children.
 *
 *  @param parent_hostname The hostname or ip address of this node's parent.
 *  @return bool True if the node had connected to its neighbors; else if False.
 */

bool NetworkInterface::connect(char* parent_hostname){
    
	// If ROOT, connect down to children
	if(root){
		for(int i = 0; i < children; i++){
            
			if(V)printf("Attempting to connect to child %d...\n", i);
            
			// Keep attempting to connect to children
			while(!connector->connect_to_child(i, port)){
				if(V)
					printf("Failed to connect to child %d...\n", i);
				sleep(1);
			}
		}
		return true;
	}
    
	// If not ROOT, connect up to parent, then down to children
	else{
        
		if(V)printf("Attempting to connect to Parent...\n");
        
        // Keep attempting to connect to parent
		while(!connector->connect_to_parent(parent_hostname, port)){
			if(V)printf("Failed to connect to Parent...\n");
			sleep(1);
		}
        
		// Connect down to all children
		for(int i = 0; i < children; i++){
            
			if(V)printf("Attempting to connect to child %d...\n", i);
            
            // Keep attempting to connect to children
			while(!connector->connect_to_child(i, port)){
				if(V)printf("Failed to connect to child %d...\n", i);
				sleep(1);
			}
		}
		return true;
	}
}

/// Code copied from file_descriptor_source_impl from GNURADIO code
/// Used to read items from packet and rebuild stream

int NetworkInterface::read_items(int index, char *buf, int nitems){
	
	int r;
	assert(nitems > 0);
	assert(d_residue_len < d_itemsize);
    
	int nbytes_read = 0;
    
	if(d_residue_len > 0){
		memcpy(buf, d_residue, d_residue_len);
		nbytes_read = d_residue_len;
		d_residue_len = 0;
	}
    
    // Index -1 is parent index
	if(index == -1){
		r = connector->read_parent(buf + nbytes_read, nitems * d_itemsize - nbytes_read);
	}
	else{
		r = connector->read_child(index, buf + nbytes_read, nitems * d_itemsize - nbytes_read);
	}
    
	if(r <= 0){
		handle_residue(buf, nbytes_read);
		return r;
	}
    
	r = handle_residue(buf, r+ nbytes_read);
    
	if(r == 0)
		return read_items(index, buf, nitems);
    
	return r;
}

/// Code copied from file_descriptor_source_impl from GNURADIO code
int NetworkInterface::handle_residue(char *buf, int nbytes_read){
	assert(nbytes_read >= 0);
    
	int nitems_read = nbytes_read / d_itemsize;
	d_residue_len = nbytes_read % d_itemsize;
	if(d_residue_len > 0)
		memcpy(d_residue, buf + nbytes_read - d_residue_len, d_residue_len);
    
	return nitems_read;
}

/*!
 *	Receive function: Receive data from node with index child_index of size packet_size
 *  Code copied from file_descriptor_source_impl.cc
 *
 *  @param child_index Index of the node to receive from; -1 for parent; >= 0 for child
 *  @param outbuf Pointer to byte array to write to.
 *  @param noutput_items The size in bytes of the data to be received.
 *  @return nread The number of bytes received from the node.
 */

int NetworkInterface::receive(int child_index, char * outbuf, int noutput_items){
	assert(noutput_items > 0);
    
	char *o = outbuf;
	int nread = 0;
    
	if(V) std::cout << "\t\t\t\tNetworkInterface Receiving from child " << child_index << std::endl;
	if(V)std::cout << std::flush;
    
	while(1){
		int r = read_items(child_index, o, noutput_items - nread);
		if(r == -1){
			if(errno == EINTR)
				continue;
			else{
				perror("\t\tNetworkInterface::receive");
				return -1;
			}
		}
		else if(r == 0){
			break;
		}
		else{
			o += r * d_itemsize;
			nread += r;
			break;
		}
	}
    
	if(nread == 0) // eof!
		return -1;
    
	if(V) std::cout << "\t\t\t\tNetworkInterface done receiving from child " << child_index << std::endl;
	if(V) std::cout << std::flush;
    
	return nread;
}


/*!
 *	Send function: Sends data from send buffer to node with index child_index of size packet_size
 *  Code copied from file_descriptor_sink_impl.cc
 *
 *  @param child_index Index of the node to send to; -1 for parent; >= 0 for child
 *  @param msg Pointer to byte array containing data to be sent to node.
 *  @param packet_size The size in bytes of the data to be sent.
 *  @return packet_size The number of bytes sent to the intended node.
 */

int NetworkInterface::send(int child_index, char* msg, int packet_size){
    
	char *inbuf = msg;
	unsigned long byte_size = packet_size * d_itemsize;
    
	if(V) std::cout << "\t\t\t\tNetworkInterface Sending to child " << child_index << std::endl;
	if(V) std::cout << std::flush;
    
	while(byte_size > 0){
		ssize_t r;
        
		if(child_index == -1){
			r = connector->write_parent(inbuf, byte_size);
		}
		else{
			r = connector->write_child(child_index, inbuf, byte_size);
		}
        
		if(r == -1){
			if(errno == EINTR)
				continue;
			else{
				perror("NetworkInterface::send");
				return -1;
			}
		}
		else{
			byte_size -= r;
			inbuf += r;
		}
	}
    
 	return packet_size;
}
