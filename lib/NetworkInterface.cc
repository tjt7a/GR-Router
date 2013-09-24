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


// Default Constructor
NetworkInterface::NetworkInterface(int itemsize, int children_count, int port_arg=8080, bool root_arg=false){

	d_itemsize = itemsize;
	children = children_count; 
	port = port_arg;
	root = root_arg; 
	d_residue  = new unsigned char[itemsize];
	d_residue_len = 0;

	// Create Ethernet Connector
	connector = new EthernetConnector(children, port);
}

// Destructor
NetworkInterface::~NetworkInterface(){
	delete [] d_residue;
	delete connector;
}

// Build graph by connecting to parent/children
bool NetworkInterface::connect(char* parent_hostname){

	// If ROOT, connect down to children
	if(root){
		for(int i = 0; i < children; i++){

			if(V)printf("Attempting to connect to child %d...\n", i);

			// Keep attempting to connect to children
			while(!connector->connect_to_child(port, i)){
				if(V)printf("Failed to connect to child %d...\n", i);
				sleep(1);
			}
		}
		return true;
	}

	// If not ROOT, connect up to parent, then down to children
	else{

		if(V)printf("Attempting to connect to Parent...\n");

		while(!connector->connect_to_parent(parent_hostname, port)){
			if(V)printf("Failed to connect to Parent...\n");
			sleep(5);
		}

		// Connect down to all children
		for(int i = 0; i < children; i++){

			if(V)printf("Attempting to connect to child %d...\n", i);

			while(!connector->connect_to_child(port, i)){
				if(V)printf("Failed to connect to child %d...\n", i);
				sleep(1);
			}
		}
		return true;
	}
}

// Code copied from file_descriptor_source_impl
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

// Code copied from file_descriptor_source_impl
int NetworkInterface::handle_residue(char *buf, int nbytes_read){
	assert(nbytes_read >= 0);

	int nitems_read = nbytes_read / d_itemsize;
	d_residue_len = nbytes_read % d_itemsize;
	if(d_residue_len > 0)
		memcpy(d_residue, buf + nbytes_read - d_residue_len, d_residue_len);

	return nitems_read;
}


// Code copied from file_descriptor_source_impl
int NetworkInterface::receive(int child_index, char * outbuf, int noutput_items){
	assert(noutput_items > 0);

	char *o = outbuf;
	int nread = 0;

	while(1){
		int r = read_items(child_index, o, noutput_items - nread);
		if(r == -1){
			if(errno == EINTR)
				continue;
			else{
				perror("NetworkInterface::receive");
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

	return nread;
}


// Send function derived from file_descriptor_sink_impl.cc's ::work function
// This function: send <num> bytes of <msg> data to the child at <child_index>
int NetworkInterface::send(int child_index, char* msg, int num){

	char *inbuf = msg;
	unsigned long byte_size = num * d_itemsize;

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
	return num;
}
