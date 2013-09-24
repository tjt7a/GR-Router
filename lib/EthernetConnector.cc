#include "EthernetConnector.h"

// Default Constructor
EthernetConnector::EthernetConnector(int count, int port){

	// Number of child nodes	
	numChildren = count;

	// If node has > 0 children, create array of children
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

	if(V)printf("Calling EthernetConnector Destructor\n");
	stop();
	exit(0);
}

// Set local socket FD
bool EthernetConnector::set_local_fd(){

	// Create a new Socket File Descriptor for local Node
        local.socket_fd = socket(AF_INET, SOCK_STREAM, 0);

        if(local.socket_fd < 0){
                if(V)printf("Serious Error: Could not create a local socket!\n");
                return false;
        }
	
		local.length = sizeof(local.address);
        bzero((char *) &(local.address), (local.length));

        local.address.sin_family = AF_INET;
        local.address.sin_addr.s_addr = INADDR_ANY;
        local.address.sin_port = htons(local.port);

        if(bind(local.socket_fd, (struct sockaddr *) &local.address,
                        (local.length)) < 0){
                if(V)printf("Serious Error: Could not bind to port %d\n", local.port);
               	return false;
        }
        if(V)printf("Set Local Socket and bound to port %d\n", local.port);
        return true;
}


// Wait for connection from child
bool EthernetConnector::connect_to_child(int index, int port){
	if(index > (numChildren - 1)){
		if(V)printf("index > number of children - 1\n");
		return false;
	}
	//char buffer[256];
	listen(local.socket_fd,3);

	(children[index]).length = sizeof((children[index].address));
	if(V) printf("Waiting for Child %d to connect...\n", index);
	(children[index]).socket_fd = accept(local.socket_fd,
						(sockaddr *) &(children[index].address),
						&(children[index].length));
	if((children[index].socket_fd) < 0){
		if(V)printf("Serious Error: Could not connect to child\n");
		return false;
	}
	
	if(V)printf("Connected to Child!\n");
	return true;
}

// Write to the child at index Children[index] the msg of size size
int EthernetConnector::write_child(int index, char * inbuf, unsigned long size){
	if(index > (numChildren - 1)){
		if(V)printf("index > number of children - 1\n");
		return false;
	}
	write_child_mutex.lock();	
	ssize_t r = write((children[index]).socket_fd,inbuf,size);
	write_child_mutex.unlock();
	return r;
}

// Read from the child at index Children[index] of size size
int EthernetConnector::read_child(int index, char * outbuf, int size){
	
	read_child_mutex.lock();
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
                if(V)printf("Could not create parent socket\n");
                return false;
        }

        if(V) printf("Attempting to connect to parent...\n");

        // Attempt to connect to parent
        if(connect(parent.socket_fd, (sockaddr *)&parent.address, sizeof(parent.address))){
                if(V)printf("Serious Error: Failed connecting to Parent\n");
                return false;
        }

        if(V)printf("Connected to parent\n");
        return true;
}

// Sets parent socket
bool EthernetConnector::set_parent_fd(){
	
	if(parent.host == NULL){
			if(V)printf("Serious Error: No such host (%s)\n", (char *)parent.host);
			close(parent.socket_fd);
			return false;
	}

	parent.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(parent.socket_fd < 0){
		if(V)printf("Serious Error: Could not create parent socket!\n");
		return false;
	}

	parent.length = sizeof(parent.address);
	bzero((char *) &parent.address, parent.length);
	parent.address.sin_family = AF_INET;
	bcopy((char *)parent.host->h_addr,
			(char *)&parent.address.sin_addr.s_addr,
			parent.host->h_length);
	parent.address.sin_port = htons(parent.port);

	if(V)printf("Set parent socket fd\n");
	return true;

}

// Write the msg to parent
int EthernetConnector::write_parent(char * msg, int size){
	// Critical section, we dont want threads writing to the same FD at the same time
	write_parent_mutex.lock();
	ssize_t r = write(parent.socket_fd,msg,size);
	write_parent_mutex.unlock();	
	return r;
}

// Read message of size size from parent
int EthernetConnector::read_parent(char * outbuf, int size){
	read_parent_mutex.lock();	
	int r = read((parent.socket_fd),outbuf,size);
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

