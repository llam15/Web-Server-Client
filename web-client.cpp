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
    cerr << "usage: web-client [URL]" << endl;
    exit(0);
}

void downloadFile(HttpResponse response) { //404 400 200 408
    if (response.getStatus() == "404") { //404
        cerr << "Error: The document contained in: " << URL << " is not found." << endl;
        return;
    }
    else if (response.getStatus() == "400") { //400
        cerr << "Error: There is a syntax error in the request: " << URL << endl;
        return;
    }
    else if (response.getStatus() == "408") { //408
        cerr << "Error: The request timed out." << endl;
        return;
    }
    else { //200 success, just download file
        string filenamestring(FILENAME);
        //if file is /, change it to index.hmtl
        if (filenamestring == "/" || filenamestring.size() == 0) {
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
        output_file.close();
        cerr << "Successfully downloaded file: " << FILENAME << endl;
    }
}

void readResponse(int sockfd) {
    vector<char> buffer(4096);
    vector<char>::iterator it1, it2;
    long bytes_read = 0;
    int read_location = 0;
    int dist_from_end = 0;
    bool throw_away_next = false;
    HttpResponse response;

    // Read request into a buffer
    while (1) {
        bytes_read = recv(sockfd, &buffer[read_location], buffer.size()-read_location, 0);

        if (bytes_read == -1) {
            cerr << "Error: Could not read from socket." << endl;
            return;
        }

        // Find end of first line.
        it1 = find(buffer.begin(), buffer.end(), '\r');

        // If could not find end of first line, then double buffer size and
        // try to read more.
        if (it1 == buffer.end()) {
            read_location = buffer.size();
            buffer.resize(buffer.size()*2);
        }
        else {
            break;
        }
    }

    response.decodeFirstLine(vector<char>(buffer.begin(), it1));

    // Try to move iterator past CRLF to the next header.
    // Check if possible.
    dist_from_end = distance(it1, buffer.end());
    if (dist_from_end < 2) {
        if (dist_from_end == 1) {
            // We only received the \r of the CRLF. Remember to throw away next
            // character (which should be a new line) when read from socket again.
            throw_away_next = true;
        }
        it1 = buffer.end();
    }
    else {
        // Otherwise advance past "\r\n" to next header.
        advance(it1, 2);
    }

    // Decode remaining headers.
    while (1) {
        it2 = find(it1, buffer.end(), '\r');
        
        // Consecutive CRLFs. End of HTTP Request
        if (it1 == it2) {
            // Now get the payload
            string content_length = response.getHeader("Content-Length");
            if (content_length == "") {
                cerr << "Error: Could not get content length" << endl;
                return;
            }
            int size = stoi(content_length);
            vector<char> payload(size);
            dist_from_end = distance(it1, buffer.end());
            if (dist_from_end < 2) {
                if (dist_from_end == 1)
                    throw_away_next = true;
                it1 = buffer.end();
            }
            else {
                it1 = next(it1, 2); //go to beginning of the payload
            }
            int bodybytes_read = distance(it1, buffer.end());
            it2 = next(it1, bodybytes_read);
            payload.insert(payload.begin(), it1, it2);
            payload.resize(size);
            if (throw_away_next) {
                bytes_read = recv(sockfd, &buffer[0], 1, 0);
            }

            //If the buffer is smaller than the payload size
            while (bodybytes_read != size) {
                //Read in next bytes from socket
                bytes_read = recv(sockfd, &payload[bodybytes_read], payload.size() - bodybytes_read, 0);

                if (bytes_read == -1) {
                    cerr << "Error: Could not read from socket." << endl;
                    return;
                }
                else if (bytes_read == 0) {
                    cerr << "Error: Socket has been closed by the server. The file name contained in: " << URL << " may be invalid." << endl;
                    return;
                }
                else {
                    bodybytes_read += bytes_read;
                }
                //Set it1 to beginning of new buffer
            }
            response.setPayload(payload);

            downloadFile(response);
            break;
        }
        
        // We reached end of buffer, but only received part of the message!
        if (it2 == buffer.end()) {
            // Get beginning of incomplete header
            read_location = distance(it1, buffer.end());
            
            // Copy remaining incomplete headers to beginning of buffer
            copy(it1, buffer.end(), buffer.begin());

            // Read in next section of request
            bytes_read = recv(sockfd, &buffer[read_location], buffer.size()-read_location, 0);

            if (bytes_read == -1) {
                cerr << "Error: Could not read from socket." << endl;
                return;
            }
            else if (bytes_read == 0) {
                cerr << "Error: Socket has been closed by the server. The file name contained in: " << URL << " may be invalid." << endl;
                return;
            }

            // If throw away next, that means we only got the first half of CRLF previously.
            // Ignore the first character (which should be a \n)
            if (throw_away_next) {
                it1 = buffer.begin()+1;
            }
            else {
                it1 = buffer.begin();
            }
            continue;
        }
        
        // Decode header line.
        else {
            response.decodeHeaderLine(vector<char>(it1, it2));
            
            // We have reached the end of the buffer.. but not the end of the request.
            // Set iterator to buffer.end() so that we will read more from socket.
            dist_from_end = distance(it2, buffer.end());
            if (dist_from_end < 2) {
                if (dist_from_end == 1) {
                    // We only received the \r of the CRLF. Remember to throw away next
                    // character (which should be a new line) when read from socket again.
                    throw_away_next = true;
                }
                it1 = buffer.end();
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
    int count = argc - 1;
    int number = 1;
    if (argc < 2) {
        usage_msg();
    }
    while (count > 0) {
        // Structs for resolving host names
        struct addrinfo hints;
        struct addrinfo *res_addr;
        // socket IDs
        int sockfd;
        // Invalid number of arguments
        string urlstring, host, filename, portname;
        URL = argv[number];
        
        // Resolve hostname to IP address
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        
        urlstring = (string) URL;
        portname = "80";
        parseURL(urlstring, host, portname, filename);
        HOSTNAME = (char*) host.c_str();
        PORT = (char*) portname.c_str();
        FILENAME = (char*) filename.c_str();
        
        if (getaddrinfo(HOSTNAME, PORT, &hints, &res_addr) != 0) {
            cerr << "Error: Could not resolve hostname: " << HOSTNAME << " to IP address." << endl;
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
        count--;
        number++;
    }
    return 0;
}
