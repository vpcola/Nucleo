#ifndef __M_UTILITY_H__
#define __M_UTILITY_H__

#include "mbed.h"
#include <string>
#include <vector>

#define SHELL_SWITCH 0x80
#define CONN_LED 0x10

#define I2C_CTR_PANEL_ADDR ((0x21) << 1)

void heapinfo(Stream * chp);
char *_strtok(char *str, const char *delim, char **saveptr);
void split( vector<string> & theStringVector,  /* Altered/returned value */
       const  string  & theString,
       const  string  & theDelimiter);

void set_ctrl_panel(int paneladdr, uint8_t value);

			 
std::string trim(const std::string& str, const std::string& whitespace = " \t");
void capitalize_word_start(std::string & str);	

#endif
 
