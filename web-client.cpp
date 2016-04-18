#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <iterator>


#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>

#include "http-message.h"

using namespace std;

char* HOSTNAME;
char* PORT;
char* FILENAME;
char* URL;

void usage_msg()
{
    cout << "usage: web-client [URL]" << endl;
    exit(0);
}

/*string createRequest(string URL) {
 return "asdf";
 }*/

void downloadFile(HttpResponse response) { //404 400 200
    if (response.getStatus() == "404") { //404
        cerr << "Error: The document is not found." << endl;
        return;
    }
    else if (response.getStatus() == "400") { //400
        cerr << "Error: There is a syntax error in the request." << endl;
        return;
    }
    else { //200 success, just download file
        string filenamestring(FILENAME);
        //if file is /, change it to index.hmtl
        if (filenamestring == "/") {
            FILENAME = (char*) "index.html";
        }
        else {
            int pos = filenamestring.find_last_of('/');
            filenamestring = filenamestring.substr(pos + 1, -1);
            FILENAME = (char*) filenamestring.c_str();
            //make FILENAME point to current directory
        }
        vector<char> payload = response.getPayload();
        remove(FILENAME); //Deletes the file with the same name, so the new downloaded one can replace
        
        //Create output stream to filename specified by FILENAME
        ofstream output_file(FILENAME);
        
        //Outputs the contents of the vector to the file
        ostream_iterator<char> it(output_file);
        copy(payload.begin(), payload.end(), it);
    }
}

void readResponse(int sockfd) {
    vector<char> buffer(4096);
    vector<char>::iterator it1, it2;
    long bytes_read = 0;
    HttpResponse response;
    int totalbytes_read = 0;
    // Read request into a buffer
    bytes_read = recv(sockfd, &buffer[0], buffer.size(), 0);
    if (bytes_read == -1) {
        cerr << "Error: Could not read from socket." << endl;
        return;
    }
    else {
        totalbytes_read += bytes_read;
    }
    
    // Find end of first line.
    it1 = find(buffer.begin(), buffer.end(), '\r');
    response.decodeFirstLine(vector<char>(buffer.begin(), it1));
    
    // Move iterator past "\r\n"
    advance(it1, 2);
    
    // Decode remaining headers.
    while (1) {
        it2 = find(it1, buffer.end(), '\r');
        
        // Consecutive CRLFs. End of HTTP Request
        if (it1 == it2) {
            // Now get the payload
            string content_length = response.getHeader("Content-Length");
            int size = stoi(content_length);
            vector<char> payload(size);
            
            it1 = next(it1, 2); //go to beginning of the payload
            int bodybytes_read = totalbytes_read - (int) distance(buffer.begin(), it1);
            it2 = next(it1, bodybytes_read);
            payload.insert(payload.begin(), it1, it2);
            payload.resize(size);
            //If the buffer is smaller than the payload size
            while (bodybytes_read != size) {
                
                //Read in next bytes from socket
                bytes_read = recv(sockfd, &payload[bodybytes_read], payload.size() - bodybytes_read, 0);
                if (bytes_read == -1) {
                    cerr << "Error: Could not read from socket." << endl;
                    return;
                }
                else {
                    bodybytes_read += bytes_read;
                }
                //Set it1 to beginning of new buffer
            }
            response.setPayload(payload);
            
            //Test output
            /*for (std::vector<char>::const_iterator i = payload.begin(); i != payload.end(); ++i)
             cout << *i;
             cout << endl;*/
            downloadFile(response);
            break;
        }
        
        // We reached end of buffer, but only received part of the message!
        if (it2 == buffer.end()) {
            bytes_read = recv(sockfd, &buffer[0], buffer.size(), 0);
            
            if (bytes_read == -1) {
                cerr << "Error: Could not read from socket." << endl;
                return;
            }
            else {
                totalbytes_read += bytes_read;
            }
            
            it1 = buffer.begin();
            continue;
        }
        
        // Decode header line.
        else {
            response.decodeHeaderLine(vector<char>(it1, it2));
            
            // We have reached the end of the buffer.. but not the end of the request.
            // Set iterator to buffer.end() so that we will read more from socket.
            if (distance(it2, buffer.end()) < 2) {
                it2 = buffer.end();
            }
            else {
                // Otherwise advance past "\r\n" to next header.
                it1 = next(it2, 2);
            }
        }
    }
}

void parseURL(string urlstring, string& host, string& portname, string& filename) {
    host = urlstring;
    host.erase(0, 7); //getting rid of http://
    filename = host;
    size_t delimiter = host.find(":"); //looking for port number now if included
    if ((signed) delimiter != -1) {
        portname = host;
        host = host.substr(0, delimiter);
        portname.erase(0, delimiter + 1);
        portname = portname.substr(0, portname.find("/"));
    }
    else {
        host = host.substr(0, host.find("/"));
    }
    filename.erase(0, filename.find("/")); //looking for file directory
}

int main(int argc, char *argv[])
{
    // Structs for resolving host names
    struct addrinfo hints;
    struct addrinfo *res_addr;
    
    // socket IDs
    int sockfd;
    
    // Invalid number of arguments
    if (argc != 2) {
        usage_msg();
    }
    else {
        URL = argv[1];
    }
    
    // Resolve hostname to IP address
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    string urlstring = (string) URL;
    string host, filename;
    string portname = "80";
    parseURL(urlstring, host, portname, filename);
    HOSTNAME = (char*) host.c_str();
    PORT = (char*) portname.c_str();
    FILENAME = (char*) filename.c_str();
    
    if (getaddrinfo(HOSTNAME, PORT, &hints, &res_addr) != 0) {
        cerr << "Error: Could not resolve hostname to IP address." << endl;
        return -1;
    }
    
    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    
    // Connect socket to server
    if (connect(sockfd, res_addr->ai_addr, res_addr->ai_addrlen) == -1) {
        cerr << "Error: Could not connect to server" << endl;
        return -1;
    }
    string message = "GET " + urlstring + " HTTP/1.0\r\n"
    "Host: " + host + "\r\n"
    "Connection: close\r\n"
    "User-Agent: Wget/1.15 (linux-gnu)\r\n"
    "Accept: */*\r\n"
    "\r\n";
    send(sockfd, message.c_str(), message.size(), 0);
    readResponse(sockfd);
    close(sockfd);
    return 0;
}