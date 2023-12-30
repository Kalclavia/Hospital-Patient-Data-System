/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Abhishek Bhattacharyya
	UIN: 731002289
	Date: 9/18/2023
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <fstream>
#include <sstream>
#include "wait.h"

using namespace std;

int main (int argc, char *argv[]) {
	int opt;
	int p = -1;
	double t = -1;
	int e = -1;
	int m = MAX_MESSAGE;	//b is our variable for buffer capacity, default is the max message size
	string filename = "";

	int new_chan = false;
	vector<FIFORequestChannel*> channels;

	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'm':
				m = atoi(optarg);	//convert optarg to an integer
				break;
			case 'c':
				new_chan = true;
				break;
		}
	}
	
	// Step 1: Start the server process as a child
	char* cmd1[] = {(char*)"./server", (char*)"server", (char*)"-m", (char*)to_string(m).c_str(), nullptr};	// command and arguments for server process, passed to execv
	pid_t child1 = fork();		// fork to create child process
	
	if(child1 == 0) {			// if child process succesfully created, start server using execvp
		execvp(cmd1[0],cmd1);
		exit(EXIT_FAILURE);
	}

	// Connect to the server's control channel on the client side to request data
    FIFORequestChannel cont_chan("control", FIFORequestChannel::CLIENT_SIDE);
	channels.push_back(&cont_chan);

	if (new_chan) {
		// If the new channel arg is given, a new channel request to the server
		MESSAGE_TYPE nc = NEWCHANNEL_MSG;
		cont_chan.cwrite(&nc, sizeof(MESSAGE_TYPE));
		char new_channel_name[30];
		cont_chan.cread(new_channel_name, sizeof(char)*30);
		FIFORequestChannel* new_channel = new FIFORequestChannel(new_channel_name, FIFORequestChannel::CLIENT_SIDE);
		channels.push_back(new_channel);
	}

	FIFORequestChannel chan = *(channels.back());
	
	// Once server is running, we can send it queries:

	// Step 2. Implemented argument checking for single datapoint request (do not request unless arguments are given, default values are -1 for all)
	// Code for single data point request

	if(p!=-1 && t != -1 && e!=-1) {
		char buf[MAX_MESSAGE];	// set size of buf (buffer var) to the given buffersize
		datamsg x(p, t, e);	// change from hardcoding to the user's values
		
		memcpy(buf, &x, sizeof(datamsg));		// puts a buffer into your data mesage
		chan.cwrite(buf, sizeof(datamsg)); 		// send that request to the server 
		double reply;
		chan.cread(&reply, sizeof(double)); //answer
		cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
	}

	else if (p!=-1){		// request 1000 datapoints
		int loop = 0;
		char buf[MAX_MESSAGE];
		ofstream outFile;
		outFile.open("./received/x1.csv");
		datamsg x(p, t, e);
		
		if (abs(t + 1.00) < 0.1) {
			t = 0.00;
		}

		while (loop < 1000) {
			x.seconds = t;
			x.ecgno = 1;
			memcpy(buf, &x, sizeof(datamsg));		// puts a buffer into your data mesage
			chan.cwrite(buf, sizeof(datamsg)); 		// send that request to the server 
			double e1;
			chan.cread(&e1, sizeof(double)); //answer
			outFile << t << "," << e1 << ",";

			x.ecgno = 2;
			memcpy(buf, &x, sizeof(datamsg));		// puts a buffer into your data mesage
			chan.cwrite(buf, sizeof(datamsg)); 		// send that request to the server 
			double e2;
			chan.cread(&e2, sizeof(double)); //answer
			outFile << e2 << "\n";

			loop++;
			t += 0.004;
		}
		outFile.close();
	}	

	// Step 3: Requesting files (PSEUDOCODE)
		// If a file si reqeusted form the client:
		// Query the server for the file size.

		// Request a file
		// Start a timer when we are ready to request the file (ex. chrono, start chrono/end chrono)
		// Some Variables we need:
		//	1. file_length: Size of file in bytes. THIS IS STATIC, DOES NOT CHANGE.
		//	2. remaining_bytes:
		//	3. offset: 
		//	4. buffer_size: Size of the data that is sent over the buffer in one transfer
		//	5. part_size: Size of the part of data that will be sent over the buffer in this transfer.
		
		/* 
		while(remaining bytes to transfer) {
			if(buffer_size > remaining_bytes) {
				part_size = remaining_bytes;
			}
			else {
				part_size = buffer_size;
			}

			offset = file_length - remaining bytes

			fileMessage(offset, part_size)
			make file transfer()

			remaining_bytes -= part_size;

		}
		*/	

	else if (filename != "") {
		// Given code
		filemsg fm(0, 0);	
		
		int len = sizeof(filemsg) + (filename.size() + 1);
		char* buf2 = new char[len];
		memcpy(buf2, &fm, sizeof(filemsg));
		strcpy(buf2 + sizeof(filemsg), filename.c_str());
		chan.cwrite(buf2, len);  	// This is a crwrite made to the server that should return in this length variable the size of the file.

		// Assignment code
		int64_t fileSize = 0;
		chan.cread(&fileSize, sizeof(int64_t));

		char* buf3 = new char[m];	// Creates a buffer based on the size specified in -m argument
		ofstream outputFile("./received/" + filename, std::ios::binary);

		// Second: Loop over each segment (segment size based on buffer size) in the file to assemble the full file message
		while(fm.offset < fileSize) {
			// For each loop iter, we need to create an instance of the filemsg for a request
			filemsg* fileRequest_Message = (filemsg*) buf2;
			fileRequest_Message->offset = fm.offset;
			fileRequest_Message->length = min(m, static_cast<int>(fileSize - fm.offset));
			chan.cwrite(buf2, len);

			int bytes_received = chan.cread(buf3, fileRequest_Message-> length);
			if (bytes_received != fileRequest_Message->length) {	// Error handling code
				cerr << "Received data is not the expected length!" << endl;
				break;
			}
			outputFile.write(buf3, bytes_received);
			fm.offset += bytes_received;
		}
		outputFile.close();
		delete[] buf2;
		delete[] buf3;
	}

	// closing the channel    
    for(auto item : channels) {
		MESSAGE_TYPE m = QUIT_MSG;
    	item->cwrite(&m, sizeof(MESSAGE_TYPE));
	}

	waitpid(child1, nullptr, 0);

	return 0;
}
