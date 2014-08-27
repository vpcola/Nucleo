#ifndef _TINY_SHELL_H_
#define _TINY_SHELL_H_

#include "mbed.h"
#include "cmsis_os.h"

#define SHELL_MAX_LINE_LENGTH       64
#define SHELL_MAX_ARGUMENTS         4

typedef void (*shellcmd_t) (Stream *, int , char **);

typedef struct {
    const char * sc_name;
    shellcmd_t sc_func;
} ShellCommand;

typedef struct {
    Stream * sc_channel;
    ShellCommand * sc_commands;
} ShellConfig;

#ifdef __cplusplus
extern "C" {
#endif

/* User commands */
void cmd_ls(Stream * chp, int argc, char * argv[]);	
void cmd_sensor(Stream * chp, int argc, char * argv[]);
void cmd_load(Stream * chp, int argc, char * argv[]);
void cmd_rollimages(Stream * chp, int argc, char * argv[]);

void shellStart(const ShellConfig *);
bool shellGetLine(Stream * chp, char *line, unsigned size);
void shellUsage(Stream * chp, const char *p);	
	
#ifdef __cplusplus
}
#endif

#endif
