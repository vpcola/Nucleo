#ifndef MBED_DHT_H
#define MBED_DHT_H

#include "mbed.h"
#include "cmsis_os.h"
#include "Mutex.h"

using namespace rtos;

enum eType{
        DHT11     = 11,
        SEN11301P = 11,
        RHT01     = 11,
        DHT22     = 22,
        AM2302    = 22,
        SEN51035P = 22,
        RHT02     = 22,
        RHT03     = 22
    } ;

enum eError {
    ERROR_NONE = 0,
    BUS_BUSY =1,
    ERROR_NOT_PRESENT =2 ,
    ERROR_ACK_TOO_LONG =3 ,
    ERROR_SYNC_TIMEOUT = 4,
    ERROR_DATA_TIMEOUT =5 ,
    ERROR_CHECKSUM = 6,
    ERROR_NO_PATIENCE =7
} ;

typedef enum {
    CELCIUS =0 ,
    FARENHEIT =1,
    KELVIN=2
} eScale;


class DHT {

public:

    DHT(PinName pin,int DHTtype);
    ~DHT();

    int readData(void);
    float ReadHumidity(void);
    float ReadTemperature(eScale Scale);

        float CalcdewPoint(float celsius, float humidity);
        float CalcdewPointFast(float celsius, float humidity);

private:
    time_t  _lastReadTime;
    float _lastTemperature;
    float _lastHumidity;

    PinName _pin;
    bool _firsttime;
    int _DHTtype;
    int DHT_data[6];

    float ConvertCelciustoFarenheit(float celsius);
    float ConvertCelciustoKelvin(float celsius);
    float CalcTemperature();
    float CalcHumidity();

		Mutex _mutex;

};

#endif
