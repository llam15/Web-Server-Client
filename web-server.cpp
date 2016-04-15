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

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>

#include "http-message.h"

using namespace std;

#define BACKLOG 5 // Number of pending connections in queue

char* HOSTNAME = (char*) "localhost";
char* PORT = (char*) "4000";
char* FILEDIR = (char*) ".";

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
	"\r\n"
	"<html><body><h1>404 Not Found</h1></body></html>";

void usage_msg()
{
	cout << "usage: web-server [hostname] [port] [file-dir]" << endl;
	exit(0);
}

void test()
{
	cout << "HOST: " << HOSTNAME << " | PORT: " << stoi(PORT) << " | FILEDIR: " << FILEDIR << endl << endl;

	string line = "GET /index.html HTTP/1.0";
	vector<char> firstline(line.begin(), line.end());

	line = "Host: localhost:4000";
	vector<char> testheader(line.begin(), line.end());

	HttpRequest request;
	if (request.decodeFirstLine(firstline) == -1) {
		cout << "Error: HTTP Request Line 1" << endl;
		exit(0);
	}
	if (request.decodeHeaderLine(testheader) == -1) {
		cout << "Error: HTTP Request Header" << endl;
		exit(0);
	}

	vector<char> encoded_request = request.encode();
	cout << string(encoded_request.begin(), encoded_request.end());

	line = "HTTP/1.1 200 OK";
	vector<char> responseline(line.begin(), line.end());

	line = "Server: Apache/2.4.18 (FreeBSD)";
	vector<char> responseheader(line.begin(), line.end());

	HttpResponse response;
	if (response.decodeFirstLine(responseline) == -1) {
		cout << "Error: HTTP Response Line 1" << endl;
		exit(0);
	}
	if (response.decodeHeaderLine(responseheader) == -1) {
		cout << "Error: HTTP Response Header" << endl;
		exit(0);
	}

	vector<char> encoded_response = response.encode();
	cout << string(encoded_response.begin(), encoded_response.end());
}

void sendResponse(int client_sockfd, const HttpRequest& request)
{
	// Open File and send response
}

void readRequest(int client_sockfd)
{
	vector<char> buffer(4096);
	// vector<char> encoded_response;
	vector<char>::iterator it1, it2;
	int bytes_read = 0;
	HttpRequest request;
	// HttpResponse response;

	bytes_read = recv(client_sockfd, &buffer[0], buffer.size(), 0);
	if (bytes_read == -1) {
		cerr << "Error: Could not read from socket." << endl;
		return;
	}

	// Find end of first line.
	it1 = find(buffer.begin(), buffer.end(), '\r');

	// Decode first line.
	if (request.decodeFirstLine(vector<char>(buffer.begin(), it1)) == -1) {
		// Bad Request
		send(client_sockfd, BAD_REQUEST.c_str(), BAD_REQUEST.size(), 0);
		return;
	}

	advance(it1, 2);

	// Decode remaining headers.
	while (1) {
		it2 = find(it1, buffer.end(), '\r');

		// Consecutive CRLFs. End of HTTP Request
		if (it1 == it2) {
			break;
		}

		// We reached end of buffer, but only received part of the message!
		if (it2 == buffer.end()) {
			bytes_read = recv(client_sockfd, &buffer[0], buffer.size(), 0);

			if (bytes_read == -1) {
				cerr << "Error: Could not read from socket." << endl;
				return;
			}

			it1 = buffer.begin();
			continue;
		}

		// Decode header line.
		else {
			if (request.decodeHeaderLine(vector<char>(it1, it2)) == -1) {
				// Bad Request
				send(client_sockfd, BAD_REQUEST.c_str(), BAD_REQUEST.size(), 0);
				return;
			}

			if (distance(it2, buffer.end()) < 2) {
				it2 = buffer.end();
			}
			else {
				it1 = next(it2, 2);
			}
		}
	}

	sendResponse(client_sockfd, request);
	//send(client_sockfd, NOT_FOUND.c_str(), NOT_FOUND.size(), 0);
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
