#ifndef HTTP_H
#define HTTP_H

#include <vector>
#include <string>
#include <map>

class HttpMessage {
public:
	/**
	 * Abstract function to decode first line of HTTP Message.
	 */
	virtual int decodeFirstLine(std::vector<char> line) = 0;

	/**
	 * Abstract function to decode first line of HTTP Message.
	 */
	virtual int decodeFirstLine(std::string line) = 0;

	/**
	 * Abstract function to encode the HTTP Message.
	 */
	virtual std::vector<char> encode() const = 0;

	/**
	 * Abstract function to encode the HTTP Message headers.
	 */
	std::vector<char> encodeHeaders() const;

	/**
	 * Given a vector of bytes, decode the header line
	 * Returns 0 if success. -1 if error.
	 */
	int decodeHeaderLine(std::vector<char> line);

	/**
	 * Given a string, decode the header line
	 * Returns 0 if success. -1 if error.
	 */
	int decodeHeaderLine(std::string line);

	/**
	 * Sets a header
	 */
	void setHeader(std::string key, std::string value);

	/**
	 * Given a vector of bytes, set the payload
	 */
	void setPayload(std::vector<char> payload);

	/**
	 * Sets the HTTP version
	 */
	void setVersion(std::string version);

	/**
	 * Returns the HTTP version. Either HTTP/1.0 or HTTP/1.1
	 */
	std::string getVersion() const { return m_version; };

	/**
	 * Returns the payload as a vector of bytes
	 */
	std::vector<char> getPayload() const { return m_payload; };

	/**
	 * Returns the value of the header specified by the key
	 */
	std::string getHeader(std::string key) const;

private:
	std::string m_version;
	std::map<std::string, std::string> m_headers;
	std::vector<char> m_payload;

};

class HttpRequest : public HttpMessage {
public:
	/**
	 * Decodes the first line of the HTTP request
	 * Returns 0 if success. -1 if error.
	 */
	virtual int decodeFirstLine(std::vector<char> line);

	/**
	 * Decodes the first line of the HTTP request (given string)
	 * Returns 0 if success. -1 if error.
	 */
	virtual int decodeFirstLine(std::string line);

	/**
	 * Encode the HTTP Request as a vector of chars.
	 */
	virtual std::vector<char> encode() const;

	/**
	 * Sets the URL of the HTTP Request
	 */
	void setURL(std::string url);

	/**
	 * Sets the method of the HTTP Request
	 */
	void setMethod(std::string method);

	/**
	 * Returns the URL of the HTTP Request
	 */
	std::string getURL() const { return m_url; };

	/**
	 * Returns the method of the HTTP Request
	 */
	std::string getMethod() const { return m_method; };

private:
	std::string m_url;
	std::string m_method;
};

class HttpResponse : public HttpMessage {
public:
	/**
	 * Decodes the first line of the HTTP response
	 * Returns 0 if success. -1 if error.
	 */
	virtual int decodeFirstLine(std::vector<char> line);

	/**
	 * Decodes the first line of the HTTP request (given string)
	 * Returns 0 if success. -1 if error.
	 */
	virtual int decodeFirstLine(std::string line);

	/**
	 * Encode the HTTP Response as a vector of chars.
	 */
	virtual std::vector<char> encode() const;

	/**
	 * Sets the status of the HTTP response
	 */
	void setStatus(std::string status);

	/**
	 * Sets the status description of the HTTP response
	 */
	void setDescription(std::string description);

	/**
	 * Returns the status of the HTTP response
	 */
	std::string getStatus() const { return m_status; };

	/**
	 * Returns the status description of the HTTP response
	 */
	std::string getDescription() const { return m_description; };


private:
	std::string m_status;
	std::string m_description;

};

#endif /* HTTP_H */