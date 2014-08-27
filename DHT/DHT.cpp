#include "DHT.h"


#define DHT_DATA_BIT_COUNT 41

DHT::DHT(PinName pin,int DHTtype)
	: _mutex()
{
    _pin = pin;
    _DHTtype = DHTtype;
    _firsttime=true;
}

DHT::~DHT() {
}

int DHT::readData() 
{

	
   int err = ERROR_NONE;
   Timer tmr;

     DigitalInOut data_pin(_pin);
		// We make sure we are the only ones
		// reading data right now.
	   _mutex.lock();
	
    // BUFFER TO RECEIVE
    uint8_t cnt = 7;
    uint8_t idx = 0;
    
    tmr.stop();
    tmr.reset();
 
    // EMPTY BUFFER
    for(int i=0; i< 5; i++) DHT_data[i] = 0;
 
    // REQUEST SAMPLE
    data_pin.output();
    data_pin.write(0);
    wait_ms(18);
    data_pin.write(1);
    wait_us(40);
    data_pin.input();
 
    // ACKNOWLEDGE or TIMEOUT
    unsigned int loopCnt = 10000;
    
    while(!data_pin.read())if(!loopCnt--)
		{
			_mutex.unlock();
			return ERROR_DATA_TIMEOUT;
		}
 
    loopCnt = 10000;
    
    while(data_pin.read())if(!loopCnt--)
		{
			_mutex.unlock();
			return ERROR_DATA_TIMEOUT;
		}
 
    // READ OUTPUT - 40 BITS => 5 BYTES or TIMEOUT
    for(int i=0; i<40; i++){
        
        loopCnt = 10000;
        
        while(!data_pin.read())if(loopCnt-- == 0)
				{
					_mutex.unlock();
					return ERROR_DATA_TIMEOUT;
				}
 
        //unsigned long t = micros();
        tmr.start();
 
        loopCnt = 10000;
        
        while(data_pin.read())if(!loopCnt--)
				{
					_mutex.unlock();
					return ERROR_DATA_TIMEOUT;
				}
 
        if(tmr.read_us() > 40) DHT_data[idx] |= (1 << cnt);
        
        tmr.stop();
        tmr.reset();
        
        if(cnt == 0){   // next byte?
        
            cnt = 7;    // restart at MSB
            idx++;      // next byte!
            
        }else cnt--;
        
    }
 
    // WRITE TO RIGHT VARS
    uint8_t sum = (DHT_data[0] + DHT_data[1] + DHT_data[2] + DHT_data[3]) & 0xFF;  
 
    if(DHT_data[4] != sum)
		{ 
			_mutex.unlock();
			return ERROR_CHECKSUM;
		}
        
    _lastTemperature= CalcTemperature();
    _lastHumidity= CalcHumidity();
    
		_mutex.unlock();
    return err;
}


float DHT::ReadHumidity() {
    return _lastHumidity;
}

float DHT::ConvertCelciustoFarenheit(float celsius) {
    return celsius * 9 / 5 + 32;
}

float DHT::ConvertCelciustoKelvin(float celsius) {
    return celsius + 273.15;
}

// dewPoint function NOAA
// reference: http://wahiduddin.net/calc/density_algorithms.htm
float DHT::CalcdewPoint(float celsius, float humidity) {
    float A0= 373.15/(273.15 + celsius);
    float SUM = -7.90298 * (A0-1);
    SUM += 5.02808 * log10(A0);
    SUM += -1.3816e-7 * (pow(10, (11.344*(1-1/A0)))-1) ;
    SUM += 8.1328e-3 * (pow(10,(-3.49149*(A0-1)))-1) ;
    SUM += log10(1013.246);
    float VP = pow(10, SUM-3) * humidity;
    float T = log(VP/0.61078);   // temp var
    return (241.88 * T) / (17.558-T);
}

// delta max = 0.6544 wrt dewPoint()
// 5x faster than dewPoint()
// reference: http://en.wikipedia.org/wiki/Dew_point
float DHT::CalcdewPointFast(float celsius, float humidity)
{
        float a = 17.271;
        float b = 237.7;
        float temp = (a * celsius) / (b + celsius) + log(humidity/100);
        float Td = (b * temp) / (a - temp);
        return Td;
}

float DHT::ReadTemperature(eScale Scale) {
    if (Scale == FARENHEIT)
        return ConvertCelciustoFarenheit(_lastTemperature);
    else if (Scale == KELVIN)
        return ConvertCelciustoKelvin(_lastTemperature);
    else
        return _lastTemperature;
}

float DHT::CalcTemperature() {
    int v;

    switch (_DHTtype) {
        case DHT11:
            v = DHT_data[2];
            return float(v);
        case DHT22:
            v = DHT_data[2] & 0x7F;
            v *= 256;
            v += DHT_data[3];
            v /= 10;
            if (DHT_data[2] & 0x80)
                v *= -1;
            return float(v);
    }
    return 0;
}

float DHT::CalcHumidity() {
    int v;

    switch (_DHTtype) {
        case DHT11:
            v = DHT_data[0];
            return float(v);
        case DHT22:
            v = DHT_data[0];
            v *= 256;
            v += DHT_data[1];
            v /= 10;
            return float(v);
    }
    return 0;
}

