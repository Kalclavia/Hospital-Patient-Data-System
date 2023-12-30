#include <fstream>
#include <iostream>
#include <thread>
#include <sys/time.h>
#include <sys/wait.h>

#include "BoundedBuffer.h"
#include "common.h"
#include "Histogram.h"
#include "HistogramCollection.h"
#include "TCPRequestChannel.h"  // DONE: Update include from"FifoRequestChannel" to "TCPRequestChannel"

// ecgno to use for datamsgs
#define ECGNO 1 // corrected

using namespace std;

struct patientData {
    int patientNO;
    double patientECG;
};

void patient_thread_function (BoundedBuffer* request_buffer, int threadCount, int patientID) {
    // DONE: Added functionality of the patient threads
   
    double timeOffset = 0.0; // Starting time offset
    const double increment = 0.004; // Time increment value

    for (int i = 0; i < threadCount; ++i) {
        // Create a new message for each iteration
        datamsg message(patientID, timeOffset, ECGNO);
        char* messageBuffer = reinterpret_cast<char*>(&message);

        request_buffer->push(messageBuffer, sizeof(message)); 

        // Increment the time offset
        timeOffset += increment;
    }
}

void file_thread_function (string filename, BoundedBuffer* request_buffer, int totalSize, int blockSize) {
    // DONE: Added functionality of the file thread

    __uint64_t messageSize = sizeof(filemsg) + filename.size() + 1;
    __uint64_t totalBlocks = (totalSize + blockSize - 1) / blockSize; // Calculating total blocks including the last smaller block if any

    for (__uint64_t blockIndex = 0; blockIndex < totalBlocks; ++blockIndex) {
        __uint64_t currentOffset = blockIndex * blockSize;
        __uint64_t currentBlockSize = blockSize;

        // Adjusting the size for the last block if it's smaller than blockSize
        if (blockIndex == totalBlocks - 1) {
            currentBlockSize = totalSize - currentOffset;
        }

        filemsg currentMessage(currentOffset, currentBlockSize);
        char* buffer = new char[messageSize];

        // Copy the file message and the file name into the buffer
        memcpy(buffer, &currentMessage, sizeof(filemsg));
        strcpy(buffer + sizeof(filemsg), filename.c_str());

        // Push the buffer to the request buffer
        request_buffer->push(buffer, messageSize);

        // Clean up the dynamically allocated memory
        delete[] buffer;
    }
}

// DONE: Update function header from FIFO to TCP Request Channel
void worker_thread_function (TCPRequestChannel* channel, BoundedBuffer* buffer_requests, BoundedBuffer* buffer_responses, int buffer_size) {
    // DONE: Added functionality of the worker threads

    char data[MAX_MESSAGE];
    char* file_buffer = new char[buffer_size];

    for (;;) { // Infinite for loop

        buffer_requests->pop(data, MAX_MESSAGE);
        MESSAGE_TYPE* type = reinterpret_cast<MESSAGE_TYPE*>(data);

        if (*type == DATA_MSG) {
            // Processing DATA_MSG
            channel->cwrite(data, sizeof(datamsg));
            double ecg;
            channel->cread(&ecg, sizeof(double));
            patientData pdata = {((datamsg*)data)->person, ecg};
            buffer_responses->push(reinterpret_cast<char*>(&pdata), sizeof(pdata));
        } else if (*type == FILE_MSG) {
            // Processing FILE_MSG
            filemsg *fmsg = reinterpret_cast<filemsg*>(data);
            string filename = reinterpret_cast<char*>(fmsg + 1);
            string output_file = "received/" + filename;

            channel->cwrite(data, filename.size() + sizeof(filemsg) + 1);
            channel->cread(file_buffer, buffer_size);

            FILE *outfile = fopen(output_file.c_str(), "r+");
            fseek(outfile, fmsg->offset, SEEK_SET);
            fwrite(file_buffer, 1, fmsg->length, outfile);
            fclose(outfile);
        } else if (*type == QUIT_MSG) {
            // Handling QUIT_MSG
            channel->cwrite(type, sizeof(QUIT_MSG));
            delete[] file_buffer;
            return;
        }
    }
}



void histogram_thread_function(HistogramCollection *histograms, BoundedBuffer *buffer_responses) {
    // DONE: Implemented functionality of the histogram threads

    char data[sizeof(patientData)];

    while (true) {
        buffer_responses->pop(data, sizeof(patientData));
        patientData *pd = reinterpret_cast<patientData *>(data);

        if (pd->patientNO == -1 && pd->patientECG == -1.0) {
            break;
        }

        histograms->update(pd->patientNO, pd->patientECG);
    }
}

// DONE: Add arguments a (IP Address) and r (Port) to arg handling in main
// Provide default values for both
// I chose to handle port as an int as I had issues using the stoi() in the TCPRequestChannel file

int main (int argc, char* argv[]) {
    int n = 1000;	// default number of requests per "patient"
    int p = 10;		// number of patients [1,15]
    int w = 100;	// default number of worker threads
	int h = 20;		// default number of histogram threads
    int b = 20;		// default capacity of the request buffer (should be changed)
	int m = MAX_MESSAGE;	// default capacity of the message buffer
	string f = "";	// name of file to be transferred
    string IP = "";     // IP address for TCPRequestChannel
    int port = 0;   // Port for TCPRequestChannel
    
    // read arguments
    int opt;
	while ((opt = getopt(argc, argv, "n:p:w:h:b:m:f:a:r:")) != -1) {    // updated to include a:r: in getopt
		switch (opt) {
			case 'n':
				n = atoi(optarg);
                break;
			case 'p':
				p = atoi(optarg);
                break;
			case 'w':
				w = atoi(optarg);
                break;
			case 'h':
				h = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
                break;
			case 'm':
				m = atoi(optarg);
                break;
			case 'f':
				f = optarg;
                break;
            case 'a':               // added case for a (IP Address)
				IP = optarg;
                break;
			case 'r':               // added case for r (IP Address)
				port = atoi(optarg);
                break;
		}
	}

    // FIXED: Leaving the fork & exec protocol in caused the tests to hang, as the server has "moved out" to its own server.cpp file.
    // Since we do not need to fork and exec a server from within client.cpp, I commented out this full code block.
     
	// // fork and exec the server
    // int pid = fork();
    // if (pid == 0) {
    //     execl("./server", "./server", "-m", (char*) to_string(m).c_str(), nullptr);
    // }
    
	// initialize overhead (including the control channel)
	TCPRequestChannel* chan = new TCPRequestChannel(IP, port);  // DONE: Update from FIFORequestChannel to TCPRequestChannel
    BoundedBuffer request_buffer(b);
    BoundedBuffer response_buffer(b);
	HistogramCollection hc;

    // making histograms and adding to collection
    for (int i = 0; i < p; i++) {
        Histogram* h = new Histogram(10, -2.0, 2.0);
        hc.add(h);
    }
	
    // IMPORTANT: Make a vector of all channels allocated on heap.
    // This keeps track of them so they can be deleted later
    // Prevents memory leaks
    vector<TCPRequestChannel *> all_channels;    // DONE: Update from FIFORequestChannel to TCPRequestChannel

    // record start time
    struct timeval start, end;
    gettimeofday(&start, 0);

    /* create all threads here */
    thread *patient_thread_array = new thread[p];
    thread file_thread;
    thread *histogram_thread_array = new thread[h];

    if (f.empty()) { 
        for (int i = 0; i < p; ++i) {
            patient_thread_array[i] = thread(patient_thread_function, &request_buffer, n, i + 1);
        }
        for (int i = 0; i < h; ++i) {
            histogram_thread_array[i] = thread(histogram_thread_function, &hc, &response_buffer);
        }
    } else {
        FILE *file_ptr;
        filemsg file_message(0, 0);
        char *msg_buf = new char[sizeof(filemsg) + f.size() + 1];
        string file_name = "received/" + f;
        long long int file_size;
        
        file_ptr = fopen(file_name.c_str(), "w+");
        memcpy(msg_buf, &file_message, sizeof(filemsg));
        strcpy(msg_buf + sizeof(filemsg), f.c_str());
        chan->cwrite(msg_buf, sizeof(filemsg) + f.size() + 1);


        chan->cread(&file_size, sizeof(long long int));

        file_thread = thread(file_thread_function, f, &request_buffer, file_size, m);

        fclose(file_ptr);
        delete[] msg_buf;
    }

    thread *worker_thread_array = new thread[w];

    for (int i = 0; i < w; ++i) {
        // DONE: Update the messaging here to use a TCP Request Channel instead of a FIFORequestChannel
        TCPRequestChannel *new_channel = new TCPRequestChannel(IP, port);
        worker_thread_array[i] = thread(worker_thread_function, new_channel, &request_buffer, &response_buffer, m);
        all_channels.push_back(new_channel);
    }

    /* join all threads here */
    if (f.empty()) {
        for (int i = 0; i < p; ++i) {
            patient_thread_array[i].join();
        }
    } else {
        file_thread.join();
    }

    for (int i = 0; i < w; ++i) {
        datamsg quit_msg(0, 0.0, 0);
        quit_msg.mtype = QUIT_MSG;
        request_buffer.push((char *)&quit_msg, sizeof(datamsg));
    }

    for (int i = 0; i < w; ++i) {
        worker_thread_array[i].join();
    }

    if (f.empty()) {
        for (int i = 0; i < h; ++i) {
            patientData final_patient_data;

            final_patient_data.patientECG = -1.0;
            final_patient_data.patientNO = -1;

            response_buffer.push((char *)&final_patient_data, sizeof(patientData));
        }

        for (int i = 0; i < h; ++i) {
            histogram_thread_array[i].join();
        }
    }

    // record end time
    gettimeofday(&end, 0);

    // print the results
	if (f == "") {
		hc.print();
	}

    int secs = ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) / ((int) 1e6);
    int usecs = (int) ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) % ((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

	// quit and close control channel
    MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
    cout << "All Done!" << endl;
    delete chan;

	// wait for server to exit
	wait(nullptr);

    // delete all items on heap to prevent mem leak
    for (auto &channel : all_channels) {
    delete channel;
    }
    
    
    delete[] patient_thread_array;
    delete[] histogram_thread_array;
    delete[] worker_thread_array;
}