#ifndef __HTTP_CLIENT_H__
#define __HTTP_CLIENT_H__

#include "mbed.h"
#include <string>
#include <map>
#include "TCPSocketConnection.h"

enum content_type{
	CONTENT_TYPE_TEXT,
	CONTENT_TYPE_APPLICATION,
	CONTENT_TYPE_IMAGE,
	CONTENT_TYPE_MESSAGE
};


typedef struct {
	int http_response;
	std::string http_response_text;
	std::map<std::string, std::string> http_info;
} HeaderInfo;

class HttpClient
{
	public:
		HttpClient(const char * hostname);
		int get(const char * url, char * buffer, size_t buf_len, HeaderInfo & hinfo, int timeout = 5000);
	
	private:
		std::string build_http_headers(std::string type, std::string path);
		int parse_url(const char* url, 
			char* scheme, 
			size_t scheme_len, 
			char* host, 
			size_t host_len, 
			uint16_t* port, 
			char* path, size_t max_path);
			
			int parse_headers(std::string header_line, HeaderInfo & hinfo, int line);
			
		int read_data(HeaderInfo & hinfo, char * buffer, size_t buf_len);

		int send(char* buf, size_t len);
		// Receive untill a char term is received
		int recv_until(char * buf, size_t len, char term);
			
		std::string _hostname; // The name of this local host
		int _timeout;
		TCPSocketConnection m_sock;
		int _httpResponse;
};

#endif
