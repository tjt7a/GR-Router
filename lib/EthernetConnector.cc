/*
	Written by Tommy Tracy II (University of Virginia HPLP)
*/
#include "EthernetConnector.h"

// Default Constructor
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

// Destructor
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
    	if(V) std::cout << "\tEthernetConnector: Serious Error: Could not create a local socket!" << std::endl;
        return false;
    }
	
	local.length = sizeof(local.address);
    bzero((char *) &(local.address), (local.length));

    local.address.sin_family = AF_INET;
    local.address.sin_addr.s_addr = INADDR_ANY;
    local.address.sin_port = htons(local.port);

    if(bind(local.socket_fd, (struct sockaddr *) &local.address, local.length) < 0){
    	if(V)printf("\tEthernetConnector: Serious Error: Could not bind to port %d\n", local.port);
    	return false;
    	// Throw error here
    }
    
    if(V)
    	printf("\tEthernetConnector: Set Local Socket and bound to port %d\n", local.port);
    return true;
}


// Wait for connection from child
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
		if(V)printf("Serious Error: Could not connect to child\n");
		return false;
	}
	
	// Child is now connected
	if(V)printf("Connected to Child!\n");
	return true;
}

// Write to the child at index Children[index] the msg of size size
int EthernetConnector::write_child(int index, char * inbuf, unsigned long size){
	if(index > (numChildren - 1)){
		if(V)printf("\tEthernetConnector: index > number of children - 1\n");
		return false;
	}

	// Write to child file descriptor
	write_child_mutex.lock();	
	ssize_t r = write((children[index]).socket_fd, inbuf, size);
	write_child_mutex.unlock();
	return r;
}

// Read from the child at index Children[index] of size size
int EthernetConnector::read_child(int index, char * outbuf, int size){
	
	// Read from child file descriptor
	read_child_mutex.lock();

	//std::cout << "\tEthernetConnector: Reading from Child[" << index << "]" <<std::endl;
	ssize_t r = read((children[index]).socket_fd, outbuf, size);
	read_child_mutex.unlock();
	return r;
}

// Connect to parent node
bool EthernetConnector::connect_to_parent(char* hostname, int port){

        // Create parent object
        //parent = Node();
        parent.port = port;
		parent.host = gethostbyname(hostname);

        // Create socket for parent
        if(!set_parent_fd()){
                if(V)printf("\tEthernetConnector: Could not create parent socket\n");
                return false;
        }

        if(V) 
        	std::cout << "\tEthernetConnector: Attempting to connect to parent=" << hostname << ":" << port << std::endl;

        // Attempt to connect to parent
        if(connect(parent.socket_fd, (sockaddr *)&parent.address, sizeof(parent.address))){
                if(V)printf("\tEthernetConnector: Serious Error: Failed connecting to Parent\n");
                return false;
        }

        if(V)printf("\tEthernetConnector: Connected to parent\n");
        return true;
}

// Sets parent socket
bool EthernetConnector::set_parent_fd(){
	
	if(parent.host == NULL){
			if(V)
				printf("\tEthernetConnector: Serious Error: No such host (%s)\n", (char *)parent.host);
			close(parent.socket_fd);
			return false;
	}

	parent.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(parent.socket_fd < 0){
		if(V)std::cout << "\tEthernetConnector: Serious Error: Could not create parent socket!" << std::endl;
		return false;
	}

	parent.length = sizeof(parent.address);
	bzero((char *) &parent.address, parent.length);
	parent.address.sin_family = AF_INET;
	bcopy((char *)parent.host->h_addr,
			(char *)&parent.address.sin_addr.s_addr,
			parent.host->h_length);
	parent.address.sin_port = htons(parent.port);

	if(V)
		std::cout << "\tEthernetConnector: Set parent socket fd" << std::endl;

	return true;
}

// Write the msg to parent
int EthernetConnector::write_parent(char * msg, int size){
	
	//std::cout << "\tEthernetConnector: Writing to (size=" << size << " bytes)" << std::endl;

	// Critical section, we dont want threads writing to the same FD at the same time
	write_parent_mutex.lock();
	ssize_t r = write(parent.socket_fd,msg,size);
	write_parent_mutex.unlock();	
	return r;
}

// Read message of size size from parent
int EthernetConnector::read_parent(char * outbuf, int size){

	//std::cout << "\tEthernetConnector: Reading from parent (size=" << size << " bytes)" << std::endl;

	read_parent_mutex.lock();	
	int r = read((parent.socket_fd), outbuf, size);
	read_parent_mutex.unlock();
	return r;
}

// Close sockets
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