#include <algorithm>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>

#include "http-message.h"

using namespace std;

#define BACKLOG 5 // Number of pending connections in queue

char* HOSTNAME = (char*) "localhost";
char* PORT = (char*) "4000";
string FILEDIR =  ".";

const string BAD_REQUEST =
	"HTTP/1.0 400 Bad Request\r\n"
	"Content-type: text/html\r\n"
	"Content-length: 50\r\n"
	"\r\n"
	"<html><body><h1>400 Bad Request</h1></body></html>";

const string NOT_FOUND =
	"HTTP/1.0 404 Not Found\r\n"
	"Content-type: text/html\r\n"
	"Content-length: 48\r\n"
	"\r\n";
const string REQUEST_TIMEOUT =
	"HTTP/1.0 408 Request Timeout\r\n"
	"Content-type: text/html\r\n"
	"Content-length: 54\r\n"
	"\r\n"
	"<html><body><h1>408 Request Timeout</h1></body></html>";

const string TYPE_JPG   = "image/jpg";
const string TYPE_JPEG  = "image/jpeg";
const string TYPE_GIF   = "image/gif";
const string TYPE_HTML  = "text/html";
const string TYPE_OCT   = "application/octet-stream";
const string TYPE_PDF   = "application/pdf";
const string TYPE_PNG   = "image/png";
const string TYPE_TXT   = "text/plain";

const timeval TIMEOUT{5, 0};

void usage_msg()
{
	cerr << "usage: web-server [hostname] [port] [file-dir]" << endl;
	exit(0);
}

string getContentType(string filename)
{
	size_t pos = filename.find_last_of('.');
	string type;

	if (pos == string::npos || (pos+1) >= filename.size()) {
		return "";
	}

	// Type = everything after last '.'
	type = filename.substr(pos+1);
	std::transform(type.begin(), type.end(), type.begin(), ::tolower);

	if (type == "jpg")
		return TYPE_JPG;
	else if (type == "jpeg")
		return TYPE_JPEG;
	else if (type == "gif")
		return TYPE_GIF;
	else if (type == "html")
		return TYPE_HTML;
	else if (type == "pdf")
		return TYPE_PDF;
	else if (type == "png")
		return TYPE_PNG;
	else if (type == "txt")
		return TYPE_TXT;
	else return TYPE_OCT;

}

void sendResponse(int client_sockfd, const HttpRequest& request)
{
	int filefd;
	string filename = request.getURL();

	// Throw away everything before the first single slash to get the filename.
	unsigned int i;
	for (i = 0; i < filename.size(); i++) {
		if (filename[i] == '/') {
			// Check if there is a slash before or after.
			if (((i > 0) && filename[i-1] == '/') ||
				((i < filename.size()-1) && filename[i+1] == '/')){
				continue;
			}

			filename = filename.substr(i);
			break;
		}
	}

	// Default to index.html
	if (filename == "/" || i == filename.size()) {
		filename = "/index.html";
	}

	// Throw away extra slash at end of file name.
	if (filename.back() == '/') {
		filename = filename.substr(0, filename.size()-1);
	}

	// Build full file path.
	// Check if file directory already has ending slash
	if (FILEDIR.back() == '/') {
		// remove extraneous slash
		filename = FILEDIR.substr(0, FILEDIR.size()-1) + filename;
	}
	else {
		filename = FILEDIR + filename;
	}

	// Open the file
	filefd = open(filename.c_str(), O_RDONLY);

	if (filefd == -1) {
		// File not found
		send(client_sockfd, NOT_FOUND.c_str(), NOT_FOUND.size(), 0);
		return;
	}

	// Get file size information
	struct stat fileStat;
    if (fstat(filefd, &fileStat) < 0) {
    	// Could not get file size information.
    	cerr << "Error: Could not get requested file information: " << filename << endl;
		send(client_sockfd, NOT_FOUND.c_str(), NOT_FOUND.size(), 0);
		return;
    }

    // Build HTTP response
	string ok = "HTTP/1.0 200 OK";
	string content = "Content-type: " + getContentType(filename);
	string content_length = "Content-length: " + to_string(fileStat.st_size);

	HttpResponse response;
	response.decodeFirstLine(ok);
	response.decodeHeaderLine(content);
	response.decodeHeaderLine(content_length);

	// Encode the header as a stream of bytes to send
	vector<char> header = response.encode();

	// Send HTTP Response headers
	send(client_sockfd, &header[0], header.size(), 0);

	// Send file
	int total_sent = 0;
	do {
		int bytes_sent = sendfile(client_sockfd, filefd, NULL, fileStat.st_size);

		if (bytes_sent < 0) {
	    	// Error while sending.
			cerr << "Error: Could not send requested file: " << filename << endl;
			send(client_sockfd, NOT_FOUND.c_str(), NOT_FOUND.size(), 0);
			return;
		}
		total_sent += bytes_sent;
	} while (total_sent < fileStat.st_size);

}

void readRequest(int client_sockfd)
{
	vector<char> buffer(4096);
	vector<char>::iterator it1, it2;
	int bytes_read = 0;
	int read_location = 0;
	int dist_from_end = 0;
	bool throw_away_next = false;
	HttpRequest request;

	// Read request into a buffer
	while (1) {
		bytes_read = recv(client_sockfd, &buffer[read_location], buffer.size()-read_location, 0);

		// If errno set, then time out
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
			// Timed out
			send(client_sockfd, REQUEST_TIMEOUT.c_str(), REQUEST_TIMEOUT.size(), 0);
			return;
		}

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

	// Decode first line.
	if (request.decodeFirstLine(vector<char>(buffer.begin(), it1)) == -1) {
		// Bad Request
		send(client_sockfd, BAD_REQUEST.c_str(), BAD_REQUEST.size(), 0);
		return;
	}

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
			break;
		}

		// We reached end of buffer, but only received part of the message!
		if (it2 == buffer.end()) {
			do {
				// Get beginning of incomplete header
				read_location = distance(it1, buffer.end());
				// Copy remaining incomplete headers to beginning of buffer
				copy(it1, buffer.end(), buffer.begin());

				// Read in next section of request
				bytes_read = recv(client_sockfd, &buffer[read_location], buffer.size()-read_location, 0);

				// If errno set, then timeout
				if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
					// Timed out
					send(client_sockfd, REQUEST_TIMEOUT.c_str(), REQUEST_TIMEOUT.size(), 0);
					return;
				}

				if (bytes_read == -1) {
					cerr << "Error: Could not read from socket." << endl;
					return;
				}
			} while (bytes_read == 0);
			
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
			if (request.decodeHeaderLine(vector<char>(it1, it2)) == -1) {
				// Bad Request
				send(client_sockfd, BAD_REQUEST.c_str(), BAD_REQUEST.size(), 0);
				return;
			}

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

	// Respond to the HTTP request
	sendResponse(client_sockfd, request);
	return;
}

int main(int argc, char *argv[])
{
	// Structs for resolving host names
	struct addrinfo hints;
	struct addrinfo *res_addr;

	// Client connection struct / size
	struct sockaddr_in client_addr;
	socklen_t client_addr_size = sizeof(client_addr);

	// socket IDs
	int yes, sockfd;

	// Invalid number of arguments
	if (argc != 1 && argc != 4) {
		usage_msg();
	}

	// Set values if passed in
	if (argc == 4) {
		HOSTNAME = argv[1];
		PORT = argv[2];
		FILEDIR = argv[3];
	}

	// Resolve hostname to IP address
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(HOSTNAME, PORT, &hints, &res_addr) != 0) {
		cerr << "Error: Could not resolve hostname to IP address." << endl;
		return -1;
	}

	// Create socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	// Allow concurrent connections?
	yes = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		cerr << "Error: Could not set socket options" << endl;
		return -1;
	}

	// Bind address to socket
	bind(sockfd, res_addr->ai_addr, res_addr->ai_addrlen);

	// set socket to listen status
	if (listen(sockfd, BACKLOG) == -1) {
		cerr << "Error: Could not set socket to listen" << endl;
		return -1;
	}

	while (1) {
		// accept a new connection
		int client_sockfd = accept(sockfd, (struct sockaddr*) &client_addr, &client_addr_size);

		if (client_sockfd == -1) {
			cerr << "Error: Could not accept connection";
			return -1;
		}

		if (setsockopt(client_sockfd, SOL_SOCKET, SO_RCVTIMEO, &TIMEOUT, sizeof(timeval)) == -1) {
			cerr << "Error: Could not set socket options" << endl;
			close(client_sockfd);
			return -1;
		}

		// Create a child process to handle the client connection
		int pid = fork();

		if (pid < 0) {
			cerr << "Error: Could not create child process for client connection";
			return -1;
		}

		// Child
		if (pid == 0) {
			// Child does not need listening socket
			close(sockfd);

			// Read the HTTP Request and respond
			readRequest(client_sockfd);

			// Done with request. Close connection and exit
			close(client_sockfd);
			exit(0);
		}

		// Parent
		else {
			// Parent does not need client socket
			close(client_sockfd);
		}

	}
}
