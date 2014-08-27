#include "mbed.h"
#include "cmsis_os.h"
#include "DHT.h"
#include "SDFileSystem.h"
#include "cc3000.h"
#include "SPI_TFT_ILI9341.h"
#include "Arial12x12.h"
#include "Arial24x23.h"
#include "Arial28x28.h"
//#include "Arial34x40.h"
//#include "Arial48x51.h"
//#include "Arial35x38.h"
#include "font_big.h"
#include "Shell.h"
#include "ini.h"
#include "Utility.h"
#include "HttpClient.h"
#include "JsonParser.h"


#include <vector>
#include <string>
#include <cstring>

#include "TCPSocketConnection.h"
#include "TCPSocketServer.h"

using namespace mbed_cc3000;

typedef struct
{
		char startupimage[30];
    char webaddress[256];
    float imagedelay;
} configuration;

typedef struct
{
	// Values read from DHT11
	double current_temperature;
	double current_humidity;
	double current_dewpoint;
	// Values read from openweathermap.com
	int weatherid;
	int sunrise;
	int sunset;
	double temperature;
	double humidity;
	double pressure;
	double windspeed;
	double clouds;
	std::string icon;
	std::string location;
	std::string description;
	
	void debug(Stream & strm)
	{
		strm.printf("Weather id : [%d]\r\n", weatherid);
		strm.printf("Current temperature: [%0.2f C]\r\n", current_temperature);
		strm.printf("Current humidity: [%0.2f%%]\r\n", current_humidity);
		strm.printf("Dew Point  : [%0.2f C]\r\n", current_dewpoint);
		strm.printf("Temperature: [%0.2f C]\r\n", temperature);
		strm.printf("Humidity   : [%0.2f%%]\r\n", humidity);
		strm.printf("Pressure   : [%0.2fhpa]\r\n", pressure);
		strm.printf("Wind Speed : [%%0.2f]\r\n", windspeed);
		strm.printf("Cloud Coverage: [%0.2f]\r\n", clouds);
		strm.printf("Location   : [%s]\r\n", location.c_str());
		strm.printf("Description: [%s]\r\n", description.c_str());
		strm.printf("Icon id    : [%s]\r\n", icon.c_str());
	}
} weather_info;

/* Define ALL Hardware used here and their pin configuration */
Serial pc(USBTX, USBRX); // tx, rx
DHT dht(D9, DHT11);
//DigitalOut conLed(D6);		// PB_10
DigitalIn shellSwitch(D6); // PB_0 (normally pulled high)
I2C i2c(I2C_SDA, I2C_SCL); // SDA, SCL
SDFileSystem sdcard(PB_15, PB_14, PB_13, PB_1, "sd"); // mosi, miso, sclk, cs, name
// the TFT is connected to SPI pin 5-7
SPI_TFT_ILI9341 TFT(PC_12, PC_11, PC_10, PC_0, PC_2, PC_3,"TFT"); 	// mosi, miso, sclk, cs, reset, dc
DigitalOut lcdOn(PC_1);
cc3000 wifi(PA_10, PA_8, PB_6, SPI(PA_7, PA_6, PA_5), "dlink-B15C", "fricu35860", WPA, false); //irq, en, cs, spi, irq-port


/** Globals **/
std::vector<std::string> images;
char webdata[1024];
configuration cfg;
weather_info winfo;
HeaderInfo info;


static int confighandler(void* user, const char* section, const char* name,
                   const char* value)
{
    configuration* pconfig = (configuration*)user;

    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
    if (MATCH("rotator", "imagedelay")) {
        pconfig->imagedelay = atof(value);
    } else if (MATCH("weather", "webaddress")) {
				strncpy(pconfig->webaddress, value, sizeof(pconfig->webaddress));
		} else if (MATCH("boot", "startupimage")) {
			  strncpy(pconfig->startupimage, value, sizeof(pconfig->startupimage));
    } else {
        return 0;  /* unknown section/name, error */
    }
    return 1;
}

/**
 *  \brief Setup interrupt priorities for Nucleo board
 *  \param none
 *  \return none
 */
void init() 
{
	// First and last IRQ defined in stm32f401xe.h
  int irqnum;
  for(irqnum = SysTick_IRQn ; irqnum < SPI4_IRQn + 1 ; irqnum++)
      NVIC_SetPriority((IRQn_Type)irqnum, 0x04);
	
  NVIC_SetPriority(SPI1_IRQn, 0x0);
  NVIC_SetPriority(FPU_IRQn, 0x1);
	NVIC_SetPriority(SPI2_IRQn, 0x2);
	NVIC_SetPriority(SPI3_IRQn, 0x02);
  NVIC_SetPriority(SysTick_IRQn, 0x3);	
}

/**
 * \brief Initialize CC3000
 * \param none
 * \return void
 **/
void init_cc3000()
{
	printf("Loading wifi startup settings ...\r\n");
	
  printf("Initializing cc3000 wifi ...\r\n");
  wifi.init();
	printf("Connecting to AP ...\r\n");
  if (wifi.connect() == -1) {
     printf("Failed to connect. Please verify connection details and try again. \r\n");
  } else {
     printf("IP address: %s \r\n", wifi.getIPAddress());
		 printf("Gateway address: %s \r\n", wifi.getGateway());
		 set_ctrl_panel(1, 1);
  }
}

/**
 * \brief Initialize LCD
 * \param none
 * \return void
 **/
void init_LCD()
{
		printf("Initializing LCD Screen ...\r\n");
	
		lcdOn = 1;
	
		TFT.claim(stdout);  
    TFT.set_orientation(1);
    TFT.background(Black);    // set background to black
    TFT.foreground(White);    // set chars to white
    TFT.cls();                // clear the screen
 

    TFT.set_font((unsigned char*) Arial12x12);
    TFT.locate(0,0);

}
/**
 * \brief Initialize switch/led panel on I2C bus
 * \param none
 * \return void
 **/
void init_i2cpanel()
{
		char cmd[2];
		// Setup direction reg for panel address 1
	  cmd[0] = 0x00; // Last bit is input
		cmd[1] = 0x00;
		// Write to the command register
		i2c.write(I2C_CTR_PANEL_ADDR, cmd, 2);
	
		// Setup direction reg for panel address 2
		cmd[0] = 0x01;
		cmd[1] = 0x00;
		
		i2c.write(I2C_CTR_PANEL_ADDR, cmd, 2);

}

/**
 * \brief Counter thread to test rtos
 * \param arg arguments to the thread
 * \return none
 **/
void counter_thread(void const *args) 
{
		uint8_t count;

    while (true) 
		{
			set_ctrl_panel(0, count);
			
			count++;
		
      osDelay(1000);
    }
}
osThreadDef(counter_thread, osPriorityNormal, DEFAULT_STACK_SIZE);


int find_images(const char * root, const char * ext)
{
	DIR * dp;
	struct dirent * dirp;
	FILINFO fileInfo;

	dp = opendir(root);			
	while((dirp = readdir(dp)) != NULL)
	{
		if (sdcard.stat(dirp->d_name, &fileInfo) == 0)
		{
			if (!(fileInfo.fattrib & AM_DIR ))
			{
				std::string filename = dirp->d_name;
				std::size_t found = filename.find(ext);
				if (found!=std::string::npos)
				{
					std::string fullpathname = root;
					fullpathname += "/";
					fullpathname += filename;
					images.push_back(fullpathname);
				}
			}
		}
	}
	closedir(dp);
	
	return images.size();
}

bool get_current_weather()
{
	  // First fetch current weather conditions from sensor
		pc.printf("Getting current conditions...\r\n");
	  if(dht.readData() == ERROR_NONE)
		{
			float temperature = dht.ReadTemperature(CELCIUS);
			float humidity = dht.ReadHumidity();
			float dewpoint = dht.CalcdewPoint(temperature, humidity);
			
			winfo.current_temperature = temperature;
			winfo.current_humidity = humidity;
			winfo.current_dewpoint = dewpoint;
			
			pc.printf("Temperature : %.2f\r\n", temperature);
			pc.printf("Humidity : %.2f\r\n", humidity);
			pc.printf("Dew Point : %.2f\r\n", dewpoint);
			
			return true;
		}	else
			return false;
}

bool get_weather_info()
{
		bool retval = false;
		HttpClient client("test.com");
			
		memset(webdata, 0, sizeof(webdata));
		
		// Fetch page from openweather
		pc.printf("\r\nTrying to fetch page [%s]... \r\n", cfg.webaddress);
		int ret = client.get(cfg.webaddress, webdata, sizeof(webdata), info);
		if (ret > 0)  
		{
			using namespace JSON;
			pc.printf("Page fetched successfully - read %d characters \r\n", strlen(webdata));
			pc.printf("Web Data [%s]\r\n", webdata);
			
			MVJSONReader reader(webdata);
			// pc.printf("Result: %s \r\n", webdata);
			if (reader.root->hasField("sys"))
			{
					winfo.sunrise = reader.root->getField("sys")->getFieldInt("sunrise");
					winfo.sunset = reader.root->getField("sys")->getFieldInt("sunset");
          winfo.location = reader.root->getFieldString("name");
					winfo.temperature = reader.root->getField("main")->getFieldDouble("temp") - 273.15;
					winfo.pressure = reader.root->getField("main")->getFieldDouble("pressure");
					winfo.humidity = reader.root->getField("main")->getFieldDouble("humidity");
					winfo.windspeed = reader.root->getField("wind")->getFieldDouble("speed");
					winfo.clouds = reader.root->getField("clouds")->getFieldDouble("all");
					if (reader.root->getField("weather") != NULL)
            if (reader.root->getField("weather")->size() > 0)
            {
                // we take only first element from list
                winfo.weatherid = reader.root->getField("weather")->at(0)->getFieldInt("id");
                winfo.icon = reader.root->getField("weather")->at(0)->getFieldString("icon");
                winfo.description = reader.root->getField("weather")->at(0)->getFieldString("description");
            }
					retval = true;
			}	
		}else
			retval = false;
		
		return retval;
}

// Update LCD display based on weather info		
void update_lcd(bool update_locale)
{
	
		// TFT.cls();
		if (get_current_weather())
		{
			// Use large fonts to display temperature
			TFT.background(Black);
			TFT.foreground(White);
			TFT.locate(170, 120);
			TFT.set_font((unsigned char *) Neu42x35);
			TFT.printf("%.0f C",winfo.temperature);

			// At the bottom
			TFT.fillrect(0,185,320,216,Blue);
			TFT.background(Blue);
			TFT.foreground(White);
			TFT.set_font((unsigned char*) Arial28x28);
			TFT.locate(0,187);
			TFT.printf("%.0f%% Humidity", 
				winfo.current_humidity);
			
			TFT.locate(0, 218);
			TFT.set_font((unsigned char *) Arial12x12);
			TFT.background(Black);
			TFT.foreground(White);
			TFT.printf("Dew Point : %0.2fC", winfo.current_dewpoint);
		}
		
		if (update_locale)
		{
			// Fill banner with white
			TFT.background(White);
			TFT.foreground(Black);
			TFT.fillrect(0,0,320,100, White);
			
			// Load the graphic icon 
			std::string filename = "/sd/icons/" + winfo.icon + ".bmp";
			TFT.BMP_16(0,10, filename.c_str());
			
			// Fill the header - top most of LCD with info
			TFT.locate(100,20);
			TFT.set_font((unsigned char *) Arial28x28);
			TFT.background(White);    // set background to black
			TFT.foreground(Black);    // set chars to white			

			TFT.printf("%s", winfo.location.c_str());
			TFT.locate(100,60);
			TFT.set_font((unsigned char *) Arial12x12);
			capitalize_word_start(winfo.description);
			TFT.printf("%s", winfo.description.c_str());
		}
			
	
}

static const ShellCommand cmds[] = {
	{"ls", cmd_ls},
	{"load", cmd_load},
	{"sensor", cmd_sensor},
	{"roll", cmd_rollimages},
	{NULL, NULL}
};

static const ShellConfig shell_cfg1 = {
  (Serial *) &pc,
  (ShellCommand *) cmds
};

int main() 
{
   
	// Set NVIC interrupt priorities
	init();
	pc.baud(115200);
	i2c.frequency(100000);
	init_i2cpanel();
	
	bool startShell = (shellSwitch.read() == 0);
	pc.printf("Starting shell: [%s]\r\n", startShell ? "Yes":"No");

	int numbmps = find_images("/sd", ".bmp");
	pc.printf("Found %d bmps in directory /sd\r\n", numbmps);
	// Read the configuration file
	pc.printf("Reading /sd/config.ini ...\r\n");
  if (ini_parse("/sd/config.ini", confighandler, &cfg) < 0) {
		// Initialize the cfg 
		strcpy(cfg.webaddress, "http://192.168.1.75/hmap/booksearch.html");
		strcpy(cfg.startupimage, "boot.bmp");
		cfg.imagedelay = 1000;
  }
	
	pc.printf("Initializing hardware ...\r\n");
	init_cc3000();
	
	pc.printf("Starting counter leds ....\r\n");
	osThreadCreate(osThread(counter_thread), NULL);
	
	init_LCD();	

	
	if (startShell)
	{
		pc.printf("Starting debug shell ...\r\n");
		shellStart(&shell_cfg1);
	}
	else
	{
//		pc.printf("Starting image rotator ...\r\n");
	  unsigned int count = 0;
		bool update_weather = true;
		while(true)
		{
				// If we have an RTC, we can detect new day
				if (update_weather && get_weather_info())
				{ 
					winfo.debug(pc);
					update_lcd(true);
        }
				else
					update_lcd(false);
	
        wait(5.0);
				count++;
				if ((count % 10) == 0)
					update_weather = true;
				else
					update_weather = false;
				
		
			  //heapinfo(&pc);
/*		
			for(std::vector<std::string>::iterator i = images.begin();
				i != images.end(); i++)
			{
				pc.printf("Loading image %s...\r\n", (*i).c_str());
				TFT.cls();
				int err = TFT.BMP_16(0,0, (*i).c_str());
				if (err != 1) TFT.printf(" - Err: %d", err);
			
				wait(1.0);
				heapinfo(&pc);
			}
*/						
		}
	}
}
