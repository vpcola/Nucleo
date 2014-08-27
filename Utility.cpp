#include "Utility.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <string>
#include <cctype>
#include <algorithm>

extern I2C i2c;

static void DisplayBuffer(Stream * chp, char * buff)
{
	int i = 0;
	while(buff[i] != 0)
	{
		char c = buff[i];
		if (c == '\n') {
			chp->printf("\r%c", c);
			break;
		}
		else
			chp->printf("%c", c);
		
		i++;
	}
}


static int DisplayHeap(void* pBuffer, char const* pFormatString, ...)
{
    char*   pStringEnd = (char*)pBuffer + strlen((char*)pBuffer);
    va_list valist;
    
    va_start(valist, pFormatString);
    
    return vsprintf(pStringEnd, pFormatString, valist);
}


void heapinfo(Stream * chp)  {
		char OutputBuffer[256];
		chp->printf("Build Date/Time: %s %s\r\n", __DATE__, __TIME__);
		chp->printf("Current Stack : 0x%08X\r\n", __current_sp());
		chp->printf("Current Heap : 0x%08X\r\n", __current_pc());

	  OutputBuffer[0] = '\0';
    typedef int (*__heapprt)(void *, char const *, ...);
    __heapstats( (__heapprt)DisplayHeap, OutputBuffer ) ;
		chp->printf("Available Memory: ");
		DisplayBuffer(chp, OutputBuffer);
}

char *_strtok(char *str, const char *delim, char **saveptr) 
{
  char *token;
  if (str)
    *saveptr = str;
  token = *saveptr;

  if (!token)
    return NULL;

  token += strspn(token, delim);
  *saveptr = strpbrk(token, delim);
  if (*saveptr)
    *(*saveptr)++ = '\0';

  return *token ? token : NULL;
}

void split( vector<string> & theStringVector,  /* Altered/returned value */
       const  string  & theString,
       const  string  & theDelimiter)
{
    size_t  start = 0, end = 0;
    while ( end != string::npos)
    {
        end = theString.find( theDelimiter, start);

        // If at end, use length=maxLength.  Else use length=end-start.
        theStringVector.push_back( theString.substr( start,
                       (end == string::npos) ? string::npos : end - start));

        // If at end, use start=maxSize.  Else use start=end+delimiter.
        start = (   ( end > (string::npos - theDelimiter.size()) )
                  ?  string::npos  :  end + theDelimiter.size());
    }
}

std::string trim(const std::string& str,
                 const std::string& whitespace)
{
    size_t strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos)
        return ""; // no content

    size_t strEnd = str.find_last_not_of(whitespace);
    size_t strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
}

static void transform_if_first_of_word( char& c )
{
    // if the previous character was a space, transform it toupper
    if( (*(&c - sizeof(char))) == ' ')
        c = toupper( c );
}

void capitalize_word_start(std::string & str)
{
	str[ 0 ] = toupper( str[ 0 ]);
        
  std::for_each( str.begin()+1, str.end(), transform_if_first_of_word );	
}

void set_ctrl_panel(int paneladdr, uint8_t value)
{
		char cmd[2];
		uint8_t addr;
	
		switch(paneladdr)
		{
			case 0: 
				addr = I2C_CTR_PANEL_ADDR;
				cmd[0] = 0x12;
				break;
			case 1:
				addr = I2C_CTR_PANEL_ADDR;
				cmd[0] = 0x13;
				break;
			default:
				// don't set anything for non-existing address
				return;
		}
	
		cmd[1] = value;
		
		i2c.write(addr, cmd, 2);
}


