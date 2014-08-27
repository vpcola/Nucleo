#include "HttpClient.h"
#include "Utility.h"
#include <string>
#include <cstring>
#include <vector>

extern Serial pc;

#if 1
//Enable debug
#include <cstdio>
#define DBG(x, ...) pc.printf("[HTTPClient : DBG]"x"\r\n", ##__VA_ARGS__); 
#define WARN(x, ...) pc.printf("[HTTPClient : WARN]"x"\r\n", ##__VA_ARGS__); 
#define ERR(x, ...) pc.printf("[HTTPClient : ERR]"x"\r\n", ##__VA_ARGS__); 
#else
//Disable debug
#define DBG(x, ...) 
#define WARN(x, ...)
#define ERR(x, ...) 
#endif




HttpClient::HttpClient(const char * hostname)
	: _hostname(hostname)
{
}

int HttpClient::get(const char * url, char * buffer, size_t buf_len, HeaderInfo & hinfo, int timeout)
{
	char scheme[8];
  uint16_t port;
  char host[32];
  char path[64];

	_timeout = timeout;
	
  if(parse_url(url, scheme, sizeof(scheme), host, sizeof(host), &port, path, sizeof(path)))
  {
    ERR("Error parsing URL");
    return -1;
  }

  if(port == 0) //TODO do handle HTTPS->443
  {
    port = 80;
  }

  DBG("Scheme: %s", scheme);
  DBG("Host: %s", host);
  DBG("Port: %d", port);
  DBG("Path: %s", path);
	
	// Connect to server
  int ret = m_sock.connect(host, port);
  if (ret < 0)
  {
    m_sock.close();
    ERR("Could not connect");
    return -1;
  }

  //Send request
	std::string req_header = build_http_headers("GET", (const char *) path);
  ret = send((char *) req_header.c_str(), req_header.size());
  if(ret)
  {
    m_sock.close();
    ERR("Could not write request\r\n");
    return -1;
  }
	// Process the response
	// First get the header
	DBG("Reading HTTP header ...\r\n");
	char header_buf[512];
	int header_line = 0, numread = 0;
	
	do {
		memset(header_buf, 0, sizeof(header_buf));
		numread = recv_until(header_buf, sizeof(header_buf), '\n');
		// remove trailing \r\n
		std::string line(header_buf, numread);
		line = trim(line, " \t\r\n");
		
		DBG("Header line [%s]\r\n", line.c_str());
		parse_headers(line, hinfo, header_line);
		header_line++;
	}while(numread > 2); // empty line for \r\n
	
	// Show the header information
	DBG("Http response: %d\r\n", hinfo.http_response);
	DBG("Http response text: %s\r\n", hinfo.http_response_text.c_str());
	for(std::map<std::string, std::string>::iterator it = hinfo.http_info.begin();
		it != hinfo.http_info.end();
		it++)
	{
		DBG("Key [%s] : Value [%s]\r\n", it->first.c_str(), it->second.c_str());
	}
	
	// Read the rest of the data based on content type
	return read_data(hinfo, buffer, buf_len);
	
}

int HttpClient::read_data(HeaderInfo & hinfo, char * buffer, size_t buf_len)
{
	int chunk_size = 0;
	char tmp_buf[10]; // just for reading the \r\n values
	char * buf_ptr = buffer;
	int numread = 0;
	
	m_sock.set_blocking(false, _timeout);
	if (hinfo.http_info["Transfer-Encoding"] == "chunked")
	{
		do {
			// read the chunk size
			DBG("Reading chunk size ...\r\n");
			if ( m_sock.receive_until(tmp_buf, 10, '\n') > 0)
			{
				chunk_size = strtoul(tmp_buf, NULL, 16);
				DBG("Chunk size: %d\r\n", chunk_size);
			}else
				break;

			if (chunk_size > 0)
			{
				// read one char at a time untill chunk size
				int cur_read2 = 0;
				do {
						if (m_sock.receive(buf_ptr,1) < 0)
						{
							DBG("Error reading chunks with total read [%d]...\r\n", numread);
							return -1;
						}
						cur_read2 ++;
						buf_ptr ++;
				}while(cur_read2 < chunk_size);
				DBG("Read %d chunk\r\n", cur_read2);
			}
	
			numread += chunk_size;
			DBG("Consuming new lines ...\r\n");
			// Consume the next new line
			m_sock.receive_until(tmp_buf, 10, '\n');
			
		}while((chunk_size > 0) && ((buf_ptr - buffer) < buf_len)); 
		
	}else
	{
			DBG("Reading complete data in one go ...\r\n");
			numread = m_sock.receive_all(buffer, buf_len);
	}
	
	DBG("Read %d bytes", numread);
	
	// Finally close the socket
	m_sock.close();
		
	return numread;
}

int HttpClient::parse_headers(std::string header_line, HeaderInfo & hinfo, int lineno)
{
	std::vector<std::string> tokens;
	// remove the trailing \n
	
	if (lineno == 0)
	{
		// Tokenize the first line using spaces
		split(tokens, header_line, " ");
		if (tokens.size() == 3)
		{
			//for(int i=0; i < tokens.size() ; i++)
			//	pc.printf("Token[%d] = [%s]\r\n", i, tokens[i].c_str());
			
			// The second part is the http response
			hinfo.http_response = atol(tokens[1].c_str());
			DBG("HTTP response [%d]\r\n", hinfo.http_response);
			hinfo.http_response_text = trim(tokens[2]);
			DBG("HTTP text [%s]\r\n", hinfo.http_response_text.c_str());
		}
	}else
	{
		
		// Scan for key value pairs
		split(tokens, header_line, ":");
		if (tokens.size() == 2)
		{
			//for(int i=0; i < tokens.size() ; i++)
			//	pc.printf("Token[%d] = [%s]\r\n", i, tokens[i].c_str());
			
			std::string key = trim(tokens[0]);
			std::string value = trim(tokens[1]);
			
			hinfo.http_info[key] = value;
		}
	}
	
	return 0;
}

std::string HttpClient::build_http_headers(std::string type, std::string path)
{
	std::string request_hdr = type + " " + path + " " + "HTTP/1.1" + "\r\n";
	request_hdr += "Host:" + _hostname + "\r\n";
	request_hdr += "\r\n"; // empty line terminates the header
	
	return request_hdr;
	
}

int HttpClient::parse_url(const char* url, char* scheme, 
	size_t scheme_len, 
	char* host, 
	size_t host_len, 
	uint16_t* port, 
	char* path, 
	size_t path_len) //Parse URL
{
  char* schemePtr = (char*) url;
  char* hostPtr = (char*) strstr(url, "://");
  if(hostPtr == NULL)
	{
		ERR("Invalid URL [%s]\r\n", url);
    return -1; //URL is invalid
	}

  if( scheme_len < hostPtr - schemePtr + 1 ) //including NULL-terminating char
  {
    ERR("Scheme str is too small (%d >= %d)\r\n", scheme_len, hostPtr - schemePtr + 1);
    return -1;
  }
  memcpy(scheme, schemePtr, hostPtr - schemePtr);
  scheme[hostPtr - schemePtr] = '\0';

  hostPtr+=3;

  size_t hostLen = 0;

  char* portPtr = strchr(hostPtr, ':');
  if( portPtr != NULL )
  {
    hostLen = portPtr - hostPtr;
    portPtr++;
    if( sscanf(portPtr, "%hu", port) != 1)
    {
      ERR("Could not find port\r\n");
      return -1;
    }
  }
  else
  {
    *port=0;
  }
  char* pathPtr = strchr(hostPtr, '/');
  if( hostLen == 0 )
  {
    hostLen = pathPtr - hostPtr;
  }

  if( host_len < hostLen + 1 ) //including NULL-terminating char
  {
    ERR("Host str is too small (%d >= %d)\r\n", host_len, hostLen + 1);
    return -1;
  }
  memcpy(host, hostPtr, hostLen);
  host[hostLen] = '\0';

  size_t pathLen;
  char* fragmentPtr = strchr(hostPtr, '#');
  if(fragmentPtr != NULL)
  {
    pathLen = fragmentPtr - pathPtr;
  }
  else
  {
    pathLen = strlen(pathPtr);
  }

  if( path_len < pathLen + 1 ) //including NULL-terminating char
  {
    ERR("Path str is too small (%d >= %d)\r\n", path_len, pathLen + 1);
    return -1;
  }
  memcpy(path, pathPtr, pathLen);
  path[pathLen] = '\0';

  return 0;
}

int HttpClient::recv_until(char * buf, size_t len, char term)
{
	int numread = 0;
  if(!m_sock.is_connected())
  {
    ERR("Connection was closed by server\r\n");
    return -1; //Connection was closed by server 
  }
  
  m_sock.set_blocking(false, _timeout);
  int ret = m_sock.receive_until(buf, len, term);
  if(ret > 0)
  {
    numread += ret;
  }
  else if( ret == 0 )
  {
    ERR("Connection was closed by server\r\n");
    return -1; //Connection was closed by server
  }
  else
  {
    ERR("Connection error (recv returned %d)\r\n", ret);
    return -1;
  }
  
  DBG("Received %d bytes", numread);
  return numread;	
}

int HttpClient::send(char* buf, size_t len) //0 on success, err code on failure
{
  if(len == 0)
  {
    len = strlen(buf);
  }
  //pc.printf("Trying to write %d bytes\r\n", len);
  size_t writtenLen = 0;
    
  if(!m_sock.is_connected())
  {
    ERR("Connection was closed by server\r\n");
    return -1; //Connection was closed by server 
  }
  
  m_sock.set_blocking(false, _timeout);
  int ret = m_sock.send_all(buf, len);
  if(ret > 0)
  {
    writtenLen += ret;
  }
  else if( ret == 0 )
  {
    ERR("Connection was closed by server\r\n");
    return -1; //Connection was closed by server
  }
  else
  {
    ERR("Connection error (send returned %d)\r\n", ret);
    return -1;
  }
  
  DBG("Written %d bytes\r\n", writtenLen);
  return 0;
}
