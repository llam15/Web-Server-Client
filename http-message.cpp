#include "http-message.h"
#include <algorithm>

using namespace std;

/*************************************
 *           HTTP MESSAGE 	     	 *
 *************************************/

vector<char> HttpMessage::encodeHeaders()
{
	vector<char> headers;
	for(map<string, string>::iterator it = m_headers.begin(); it != m_headers.end(); ++it) {
		string h = it->first + ": " + it->second + "\r\n";
		headers.insert(headers.end(), h.begin(), h.end());
	}
	headers.push_back('\r');
	headers.push_back('\n');

	return headers;
}

int HttpMessage::decodeHeaderLine(vector<char> line)
{
	// Find the colon.
	vector<char>::iterator it = find(line.begin(), line.end(), ':');

	// Invalid syntax. Either no colon, or no key.
	if (it == line.end() || it == line.begin()) {
		return -1;
	}

	// Key = beginning to colon
	string key(line.begin(), it);


	// Have to advance ahead 2 indices. 
	// Check will not overrun end. Check there is a space after colon.
	if ((distance(it, line.end()) < 2) || (*(next(it)) != ' ')) {
		return -1;
	}

	advance(it, 2);

	// Iterator now points to beginning of key
	string value(it, line.end());

	setHeader(key, value);

	return 0;
}

void HttpMessage::setHeader(string key, string value)
{
	m_headers[key] = value;
}

void HttpMessage::setPayload(vector<char> payload)
{
	m_payload = payload;
}

void HttpMessage::setVersion(std::string version)
{
	m_version = version;
}

/*************************************
 *           HTTP REQUEST 	     	 *
 *************************************/

int HttpRequest::decodeFirstLine(vector<char> line)
{
	// Expected syntax of line:
	// <METHOD> <URL> <version>
	vector<char>::iterator it1 = find(line.begin(), line.end(), ' ');
	vector<char>::iterator it2 = find(next(it1), line.end(), ' ');
	string method;
	// Invalid syntax.
	if (it1 == line.end() || it2 == line.end()) {
		return -1;
	}

	method = string(line.begin(), it1);
	transform(method.begin(), method.end(), method.begin(), ::toupper);

	if (method != "GET") {
		return -1;
	}

	setMethod(method);

	setURL(string(next(it1), it2));

	// Invalid syntax. No HTTP version
	if (++it2 == line.end()) {
		return -1;
	}

	setVersion(string(it2, line.end()));

	return 0;
}

vector<char> HttpRequest::encode()
{
	vector<char> request;
	vector<char> headers = encodeHeaders();
	string method = getMethod();
	string url = getURL();
	string version = getVersion();
	string CRLF = "\r\n";

	request.insert(request.end(), method.begin(), method.end());
	request.push_back(' ');
	request.insert(request.end(), url.begin(), url.end());
	request.push_back(' ');
	request.insert(request.end(), version.begin(), version.end());
	request.insert(request.end(), CRLF.begin(), CRLF.end());
	request.insert(request.end(), headers.begin(), headers.end());

	return request;
}

void HttpRequest::setURL(string url)
{
	m_url = url;
}

void HttpRequest::setMethod(std::string method)
{
	m_method = method;
}


/*************************************
 *           HTTP RESPONSE 	     	 *
 *************************************/

int HttpResponse::decodeFirstLine(vector<char> line)
{
	// Expected syntax of line:
	// <version> <status code> <status description>
	vector<char>::iterator it1 = find(line.begin(), line.end(), ' ');
	vector<char>::iterator it2 = find(next(it1), line.end(), ' ');

	// Invalid syntax.
	if (it1 == line.end() || it2 == line.end()) {
		return -1;
	}

	setVersion(string(line.begin(), it1));

	setStatus(string(next(it1), it2));

	// Invalid syntax. No HTTP version
	if (++it2 == line.end()) {
		return -1;
	}

	setDescription(string(it2, line.end()));

	return 0;
}

vector<char> HttpResponse::encode()
{
	vector<char> response;
	vector<char> headers = encodeHeaders();
	string version = getVersion();
	string status = getStatus();
	string description = getDescription();
	string CRLF = "\r\n";

	response.insert(response.end(), version.begin(), version.end());
	response.push_back(' ');
	response.insert(response.end(), status.begin(), status.end());
	response.push_back(' ');
	response.insert(response.end(), description.begin(), description.end());
	response.insert(response.end(), CRLF.begin(), CRLF.end());
	response.insert(response.end(), headers.begin(), headers.end());

	return response;
}
void HttpResponse::setStatus(string status)
{
	m_status = status;
}

void HttpResponse::setDescription(string description)
{
	m_description = description;
}

 