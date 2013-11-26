/*
	Written by Tommy Tracy II (University of Virginia HPLP)
*/
#ifndef ETHERNETCONNECTOR_H
#define ETHERNETCONNECTOR_H

// Ethernet Connector
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>

#include <boost/thread.hpp>// used for lock

// Verbose Flag
#define V	true

// Class for children nodes; encapsulates child-specific data
class Node{
public:
	int socket_fd; // Socket File Descriptor
	int port; // Port
	sockaddr_in address; // IP address
	hostent *host; // hostname
	socklen_t length; // socket 'length'
};

// Class for Ethernet-specific connector
class EthernetConnector{
public:

	// Default Constructor / Destructor
	EthernetConnector(int number_of_children, int port);
	~EthernetConnector();
	
	// Parent functions
	bool connect_to_parent(char* hostname, int port);
	int write_parent(char * msg, int size); // Return number of bytes written
	int read_parent(char * outbuf, int size); // Return number of bytes read

	// Child functions
	bool connect_to_child(int index, int port);
	int write_child(int index, char * inbuf, unsigned long size); // Return number of bytes written
	int read_child(int index, char * outbuf, int size); // Return number of bytes read

	// Close all file descriptors
	void stop();

private:

	// Private Variables
	Node local;
	Node parent;
	Node *children; // Pointer to array of children nodes
	
	int numChildren;

	// Locks for File Descriptor access
	boost::mutex read_parent_mutex;
	boost::mutex write_parent_mutex;
	boost::mutex read_child_mutex;
	boost::mutex write_child_mutex;

	// Private Functions
	bool set_local_fd();
	bool set_parent_fd();
};

#endif
