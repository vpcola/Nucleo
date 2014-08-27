#include "mbed.h"
#include "cmsis_os.h"

#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include "Shell.h"
#include "Utility.h"
#include "DHT.h"
#include "SDFileSystem.h"
#include "SPI_TFT_ILI9341.h"


extern DHT dht;
extern SDFileSystem sdcard;
extern I2C i2c;
extern SPI_TFT_ILI9341 TFT;
extern std::vector<std::string> images;

void shellUsage(Stream * chp, const char *p) 
{
	 chp->printf("Usage: %s\r\n", p);
}

static void list_commands(Stream *chp, const ShellCommand *scp) {

  while (scp->sc_name != NULL) {
    chp->printf("%s ", scp->sc_name);
    scp++;
  }
}

static void cmd_info(Stream * chp, int argc, char *argv[]) 
{

  (void)argv;
  if (argc > 0) {
    shellUsage(chp, "info");
    return;
  }
	
	long sp = __current_sp();
	long pc = __current_pc();
	

	heapinfo(chp);

}


static ShellCommand local_commands[] = {
  {"info", cmd_info},
  {NULL, NULL}
};

static bool cmdexec(const ShellCommand *scp, Stream * chp,
                      char *name, int argc, char *argv[]) 
{
  while (scp->sc_name != NULL) {
    if (strcasecmp(scp->sc_name, name) == 0) {
      scp->sc_func(chp, argc, argv);
      return false;
    }
    scp++;
  }
  return true;
}

void shellStart(const ShellConfig *p) 
{
  int n;
  Stream *chp = p->sc_channel;
  const ShellCommand *scp = p->sc_commands;
  char *lp, *cmd, *tokp, line[SHELL_MAX_LINE_LENGTH];
  char *args[SHELL_MAX_ARGUMENTS + 1];

  chp->printf("\r\nEmbed/RX Shell\r\n");
  while (true) {
    chp->printf(">> ");
    if (shellGetLine(chp, line, sizeof(line))) {
      chp->printf("\r\nlogout");
      break;
    }
    lp = _strtok(line, " \t", &tokp);
    cmd = lp;
    n = 0;
    while ((lp = _strtok(NULL, " \t", &tokp)) != NULL) {
      if (n >= SHELL_MAX_ARGUMENTS) {
        chp->printf("too many arguments\r\n");
        cmd = NULL;
        break;
      }
      args[n++] = lp;
    }
    args[n] = NULL;
    if (cmd != NULL) {
      if (strcasecmp(cmd, "exit") == 0) {
        if (n > 0) {
          shellUsage(chp, "exit");
          continue;
        }
				// Break here breaks the outer loop
				// hence, we exit the shell.
        break;
      }
      else if (strcasecmp(cmd, "help") == 0) {
        if (n > 0) {
          shellUsage(chp, "help");
          continue;
        }
        chp->printf("Commands: help exit ");
        list_commands(chp, local_commands);
        if (scp != NULL)
          list_commands(chp, scp);
        chp->printf("\r\n");
      }
      else if (cmdexec(local_commands, chp, cmd, n, args) &&
          ((scp == NULL) || cmdexec(scp, chp, cmd, n, args))) {
        chp->printf("%s", cmd);
        chp->printf(" ?\r\n");
      }
    }
  }
}

bool shellGetLine(Stream *chp, char *line, unsigned size) {
  char *p = line;

  while (true) {
    char c;

    if ((c = chp->getc()) == 0)
      return true;
    if (c == 4) {
      chp->printf("^D");
      return true;
    }
    if (c == 8) {
      if (p != line) {
        chp->putc(c);
        chp->putc(0x20);
        chp->putc(c);
        p--;
      }
      continue;
    }
    if (c == '\r') {
      chp->printf("\r\n");
      *p = 0;
      return false;
    }
    if (c < 0x20)
      continue;
    if (p < line + size - 1) {
      chp->putc(c);
      *p++ = (char)c;
    }
  }
}

/**
 *  \brief List Directories and files 
 *	\param none
 *	\return int
 **/

void cmd_ls(Stream * chp, int argc, char * argv[])
{
	DIR * dp;
	struct dirent * dirp;
	FILINFO fileInfo;
	char dirroot[256];
	
	if (argc >= 1)
		sprintf(dirroot, "/sd/%s", argv[0]);
	else
		sprintf(dirroot, "/sd");
	
	chp->printf("Listing directory [%s]\r\n", dirroot);
	
	dp = opendir(dirroot);			
	while((dirp = readdir(dp)) != NULL)
	{
		if (sdcard.stat(dirp->d_name, &fileInfo) == 0)
		{
			if (fileInfo.fattrib & AM_DIR )
					chp->printf("<DIR>\t\t");
			else
					chp->printf("%ld\t\t", fileInfo.fsize);
		}
		chp->printf("%s\r\n", dirp->d_name);
	}
	closedir(dp);
}

void cmd_sensor(Stream * chp, int argc, char * argv[])
{
	float temperature, humidity;

	
	if (dht.readData() == ERROR_NONE)
  {
		temperature = dht.ReadTemperature(CELCIUS);
		humidity = dht.ReadHumidity();
		
		chp->printf("Temperature %.2fC \r\n", temperature);
    chp->printf("Humidity %.2f%% \r\n", humidity);
		chp->printf("Dew point %.2f\r\n", dht.CalcdewPoint(temperature, humidity));
  }
}

void cmd_load(Stream * chp, int argc, char * argv[])
{
	char filename[256];
	
	if (argc != 1)
	{
		shellUsage(chp, "load <bitmapfile>");
		return;
	}
	
	sprintf(filename, "/sd/%s", argv[0]);
		// Load a bitmap startup file
	int err = TFT.BMP_16(0,0, filename);
	if (err != 1) TFT.printf(" - Err: %d", err);	
	
}

void cmd_rollimages(Stream * chp, int argc, char * argv[])
{
			for(std::vector<std::string>::iterator i = images.begin();
				i != images.end(); i++)
			{
				chp->printf("Loading image %s...\r\n", (*i).c_str());
				TFT.cls();
				int err = TFT.BMP_16(0,0, (*i).c_str());
				if (err != 1) chp->printf(" - Err: %d", err);
			
				wait(1.0);
				heapinfo(chp);
			}	
}
