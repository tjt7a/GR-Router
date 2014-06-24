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

#include "EthernetConnector.h"

/*!
 *	This is the public constuctor for the Ethernet Connector.
 *
 *  @param count The number of children that the router will connect to.
 *  @param port The port on which the router will communicate.
 */

EthernetConnector::EthernetConnector(int count, int port){
    
	// Number of child nodes
	numChildren = count;
    
	// If node has > 0 children, create array of children
	if(V)
		std::cout <<"\tEthernetConnector: Creating array of " << numChildren << " children nodes" << std::endl;
    
	// Set local file descriptor
	if(numChildren > 0){
        
		// Create array of Children Nodes
		children = new Node[numChildren];
        
		// Create a local node and set port, then set FD
		local.port = port;
		set_local_fd();
        
	}
}

/*!
 *	This is the destructor for the Ethernet Connector.
 */

EthernetConnector::~EthernetConnector(){
    
	if(V)
		std::cout <<"\tEthernetConnector: Calling EthernetConnector Destructor" << std::endl;
	stop();
	delete[] children;
	exit(0);
}

// Set local socket FD
bool EthernetConnector::set_local_fd(){
    
	// Create a new Socket File Descriptor for local Node
	local.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    
	// We could not get a socket FD
    if(local.socket_fd < 0){
    	std::cout << "\tEthernetConnector: Serious Error: Could not create a local socket!" << std::endl;
        return false;
    }
	
	local.length = sizeof(local.address);
    bzero((char *) &(local.address), (local.length));
    
    local.address.sin_family = AF_INET;
    local.address.sin_addr.s_addr = INADDR_ANY;
    local.address.sin_port = htons(local.port);
    
    if(bind(local.socket_fd, (struct sockaddr *) &local.address, local.length) < 0){
    	printf("\tEthernetConnector: Serious Error: Could not bind to port %d\n", local.port);
    	return false;
    	// Throw error here
    }
    
    if(V)
    	printf("\tEthernetConnector: Set Local Socket and bound to port %d\n", local.port);
    return true;
}


/*!
 *	This function will connect the router to it's child
 *
 *  @param index The index of the child to connect to.
 *  @param port The port on which the router will communicate with its child on.
 */

bool EthernetConnector::connect_to_child(int index, int port){
    
	if(V)
		std::cout << "\tEthernetConnector: Attempting to connect to child at index: " << index << " on port " << port << std::endl;
    
	if(index > (numChildren - 1)){
		if(V)
			std::cout << "Number of Children=" << numChildren << " index of child accessed=" << index << std::endl;
        
        printf("index > number of children - 1\n");
		return false;
	}
    
	//unsigned char buffer[256];
	listen(local.socket_fd,3);
    
	(children[index]).length = sizeof((children[index].address));
    
	if(V)
		printf("Waiting for Child %d to connect...\n", index);
    
	(children[index]).socket_fd = accept(local.socket_fd,
                                         (sockaddr *) &(children[index].address),
                                         &(children[index].length));
    
	// Could not connect to the child
	if((children[index].socket_fd) < 0){
		printf("Serious Error: Could not connect to child\n");
		return false;
	}
	
	// Child is now connected
	if(V)printf("Connected to Child!\n");
	return true;
}

// Write to the child at index Children[index] the msg of size size

/*!
 *	This is the public constuctor for the Ethernet Connector.
 *
 *  @param count The number of children that the router will connect to.
 *  @param port The port on which the router will communicate.
 *  @return r True if the data was written to the child; False if not.
 */

int EthernetConnector::write_child(int index, char * inbuf, unsigned long size){
	
    // If the index is not valid, return ERROR
    if(index > (numChildren - 1)){
		if(V)printf("\tERROR: EthernetConnector: index > number of children - 1\n");
		return false;
	}
    
	// Write to child file descriptor
	ssize_t r = write((children[index]).socket_fd, inbuf, size);
    
	return r;
}

/*!
 *	Read from the child at index Children[index]
 *
 *  @param index The index of the child to read from.
 *  @param outbuf A pointer to an array of characters to write the data into.
 *  @return r The number of bytes read from the child.
 */

int EthernetConnector::read_child(int index, char * outbuf, int size){
    
    // Read from the child file descriptor
	ssize_t r = read((children[index]).socket_fd, outbuf, size);
    
	return r;
}

/*!
 *	Connect to the parent.
 *
 *  @param hostname The hostname / ip address of the parent node to connect to.
 *  @param port The port for the child to connect to the parent on.
 *  @return bool Return True if the node could connect to it's parent; False if not.
 */

bool EthernetConnector::connect_to_parent(char* hostname, int port){
    
    // Create parent object
    parent.port = port;
    parent.host = gethostbyname(hostname);
    
    // Create socket for parent
    if(!set_parent_fd()){
        if(V)printf("\tEthernetConnector: Could not create parent socket\n");
        return false;
    }
    
    // Attempt to connect to parent
    if(connect(parent.socket_fd, (sockaddr *)&parent.address, sizeof(parent.address))){
        printf("\tEthernetConnector: Serious Error: Failed connecting to Parent\n");
        return false;
    }
    
    return true;
}

/// Set the parent file descriptor for communicate with the parent.
bool EthernetConnector::set_parent_fd(){
	
	if(parent.host == NULL){
        printf("\tEthernetConnector: Serious Error: No such host (%s)\n", (char *)parent.host);
        close(parent.socket_fd);
        return false;
	}
    
	parent.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(parent.socket_fd < 0){
		std::cout << "\tEthernetConnector: Serious Error: Could not create parent socket!" << std::endl;
		return false;
	}
    
	parent.length = sizeof(parent.address);
	bzero((char *) &parent.address, parent.length);
	parent.address.sin_family = AF_INET;
	bcopy((char *)parent.host->h_addr,
          (char *)&parent.address.sin_addr.s_addr,
          parent.host->h_length);
	parent.address.sin_port = htons(parent.port);
    
	return true;
}

/*!
 *	Write the data in the msg buffer to parent.
 *
 *  @param msg Pointer to a byte array buffer containing a message to be sent to parent.
 *  @param size The number of bytes to be sent to the parent.
 *  @return r The number of bytes to be sent to the parent.
 */

int EthernetConnector::write_parent(char * msg, int size){
    
	// Critical section, we dont want threads writing to the same FD at the same time
	write_parent_mutex.lock();
	ssize_t r = write(parent.socket_fd,msg,size);
	write_parent_mutex.unlock();
	return r;
}

/*!
 *	Read data from the parent.
 *
 *  @param outbuf A pointer to a byte array where the data from the parent would be written to.
 *  @param size The number of bytes to be received from the parent.
 *  @return r The number of bytes received.
 */

int EthernetConnector::read_parent(char * outbuf, int size){
    
    // Critical section; we don't want to have multiple threads read from the same FD at the same time
	read_parent_mutex.lock();
	int r = read((parent.socket_fd), outbuf, size);
	read_parent_mutex.unlock();
	return r;
}

/*!
 *	Close the file descriptors between this node and its parent and children.
 */

void EthernetConnector::stop()
{
	// Close all open file descriptors (local, parent, children)
	if(numChildren > 0){
		close(local.socket_fd);
		close(parent.socket_fd);
	}
    
	for(int i = 0; i < numChildren; i++){
		close((children[i].socket_fd));
	}
}