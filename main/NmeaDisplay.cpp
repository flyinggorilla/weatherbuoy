#include "NmeaDisplay.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esputil.h"
#include "math.h"

static const char tag[] = "NmeaDisplay";

// calculate rolling average
// https://tttapa.github.io/Pages/Mathematics/Systems-and-Control-Theory/Digital-filters/Simple%20Moving%20Average/C++Implementation.html

///////////////////// BEGIN NMEA SPECIFIC  ////////////////////////////////

#include "NMEA2000_esp32.h"
#include "N2kStream_esp32.h"
#include "N2kMessages.h"
extern "C"
{
    // Application execution delay. Must be implemented by application.
    void delay(uint32_t ms)
    {
        vTaskDelay(ms / portTICK_PERIOD_MS);
    };
    // Current uptime in milliseconds. Must be implemented by application.
    uint32_t millis()
    {
        return esp_timer_get_time() / 1000;
    };
}

void fDisplayTask(void *pvParameter)
{
    ((NmeaDisplay *)pvParameter)->DisplayTask();
    vTaskDelete(NULL);
}

/*void calcApparentWind() {
        // To display Apparent wind indpendently from True-Wind, we calculate Speed over Ground (which we dont need for other use)

        long double sog = 1.0l / 2.0l * (2.0l * aws * cos(awa) + sqrt(2.0l) * sqrt(-aws * aws + 2.0l * tws * tws + aws * aws * cos(2.0l * awa)));
        long double cog = acos((aws * cos(awa) - sog) / tws); // course over ground, heading vs. true north
        long double twd = fmod(M_TWOPI + cog + twa, M_TWOPI);

        //ESP_LOGI(tag, "SOG: %.2f, AWS: %.2f, AWA: %.2f, TWS: %.2f, TWD: %.2f, COG: %.2f", msToKnots(sog), msToKnots(aws), RadToDeg(awa), msToKnots(tws), RadToDeg(twd), RadToDeg(cog));
        //ESP_LOGI(tag, "SOG: %.2Lf, AWS: %.2Lf, AWA: %.2Lf, TWS: %.2Lf, TWD: %.2Lf, COG: %.2Lf", sog, aws, awa, tws, twd, cog);
}*/

void NmeaDisplay::Start()
{
    /// PGNS
    // https://static.garmin.com/pumac/Tech_Ref_for_Garmin_NMEA2k_EN.pdf
    // https://continuouswave.com/whaler/reference/PGN.html
    // https://endige.com/2050/nmea-2000-pgns-deciphered/

    //const short NMEA_DEVICE_ENVIRONMENTAL = 0;
    /////////////// HOW TO USE MULTIPLE DEVICES
    ////////////// https://github.com/ttlappalainen/NMEA2000/blob/master/Examples/MultiDevice/MultiDevice.ino
    /////////////////////////////////////////////////////////////////////
    mNmea.SetDeviceCount(1);                             // Enable multi device support for 2 devices
    mNmea.SetProductInformation("00000002",              // Manufacturer's Model serial code
                                100,                     // Manufacturer's product code
                                "weatherbuoy",           // Manufacturer's Model ID
                                "1.1.0.22 (2021-12-08)", // Manufacturer's Software version code
                                "1.1.0.0 (2021-12-08)"   // Manufacturer's Model version
    );

    mNmea.SetDeviceInformation(2,   // Unique number. Use e.g. Serial number.
                               130, // Device function=PC Gateway. See codes on http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
                               25,  // Device class=PC. See codes on  http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
                               2046 // Just choosen free from code list on http://www.nmea.org/Assets/20121020%20nmea%202000%20registration%20list.pdf
    );

    mNmea.SetMode(tNMEA2000::N2km_NodeOnly, 123);

// DEBUG LOG FORWARDING
#ifdef DEBUG
    mNmea.SetForwardStream(&mNmeaLogStream);
    mNmea.SetForwardOwnMessages(true);
    mNmea.EnableForward(true);
#endif
    // List here messages your device will transmit.
    //const unsigned long TransmitMessages[]={130306L, 0};    // 130306L PGN: Wind
    const unsigned long TransmitMessages[] = {129026L, 130306L, 127250L, 128259L, 129029L, 0}; // 130306L PGN: Wind
    mNmea.ExtendTransmitMessages(TransmitMessages);
    mNmea.Open();

    xTaskCreate(&fDisplayTask, "NmeaDisplay", 1024 * 16, this, ESP_TASK_MAIN_PRIO, NULL); // large stack is needed
}

int cnt = 0;

void NmeaDisplay::DisplayTask()
{
    while (true)
    {
        Data data;
        if (!mrDataQueue.GetLatestData(data, 90)) {
            mNmea.ParseMessages();
            delay(500);
            continue;
        }


        //static const double defaultLatitude = 47.940703;
        //static const double defaultLongitude = 13.595386;
        static const double defaultAltitude = 469;
        static const double hdop = 1; // horizontal dilution in meters
        double altitude = defaultAltitude;
        double longitude = data.lon;
        double latitude = data.lat;
        //double tws = data.cspeed;
        //int twd = data.cdir;
        double avgTws = data.avgcspeed;
        int avgTwd = data.avgcdir;
        int awa = data.dir; // > 180 ? data.dir - 360 : data.dir;
        double aws = data.speed;
        double heading = isnans(data.compassh) ? 0 : data.compassh;
        double sog = isnanf(data.gpsspeed) ? 0 : data.gpsspeed;
        double cog = isnans(data.gpsheading) ? 0 : data.gpsheading;

        /*if (isnanf(data.lat) || isnanf(data.lon) || data.lat == 0 || data.lon == 0) {
            latitude = defaultLatitude;
            longitude = defaultLongitude;
        }*/

        if (isnanf(data.lat) || isnanf(data.lon) || data.lat == 0 || data.lon == 0) {
            latitude = N2kDoubleNA;
            longitude = N2kDoubleNA;
        }
        // #### TEST ONLY 
        // sog = KnotsToms(1);


        // USERINF,GPSLOCATION,GPSSPEED,GPSHEADING,CSPEED,CGSPEED,AVGCSPEED,SPEED,GSPEED,AVGSPEED,DIR,GDIR,AVGDIR,CDIR,CGDIR,AVGCDIR,COMPASSH,XTILT,YTILT,STATUS,WINDSTAT,GPSSTATUS,TIME,CHECK
        /*ESP_LOGI(tag, "RAW: lat: %0.6f, lon: %0.6f, gpsheading: %d, compass: %d, cspeed: %0.1f, cdir: %d, avgcspeed: %0.1f, avgcdir: %d, speed: %0.1f, dir: %d, avgspeed: %0.1f, avgdir: %d", 
            data.lat, data.lon, data.gpsheading, data.compassh, msToKnots(data.cspeed), data.cdir, msToKnots(data.avgcspeed), data.avgcdir, msToKnots(data.speed), data.dir, msToKnots(data.avgspeed), data.avgdir);

        ESP_LOGI(tag, "lat: %0.6f, lon: %0.6f, cog: %d, heading: %d, tws: %0.1f, twd: %d, avgTws: %0.1f, avgTwd: %d, aws: %0.1f, awa: %d", 
            latitude, longitude, data.gpsheading, data.compassh, msToKnots(tws), twd, msToKnots(avgTws), avgTwd, msToKnots(aws), awa); */

        tN2kMsg n2kMsg1;

        // TODO REVERSE CALCULATION OF COG SOG APPARENT WIND!!!  ****************************************************************
        // https://en.wikipedia.org/wiki/Apparent_wind
       

        uint16_t systemDate = data.time / (60*60*24);
        double systemTime = data.time - systemDate*60*60*24;
        //SetN2kSystemTime(n2kMsg1, 1, systemDate, systemTime);
        
        altitude = data.time % 60; // ABUSE ALTITUDE FOR SECONDS DISPLAY!!!!! DELETE IF NOT NEEDED
        //ESP_LOGI(tag, "time:%ld, days:%d, seconds: %f", data.time, systemDate, systemTime);
          

        // PGN129029
        SetN2kGNSS(n2kMsg1, 1, systemDate, systemTime, latitude, longitude, altitude, N2kGNSSt_GPS, N2kGNSSm_GNSSfix, 10, hdop);
        if (!mNmea.SendMsg(n2kMsg1))
        {
            ESP_LOGW(tag, "NMEA SendMsg failed");
        }

        // PGN127250
        //SetN2kMagneticHeading(n2kMsg1, 1, DegToRad(heading), 0);  
        SetN2kTrueHeading(n2kMsg1, 1, DegToRad(heading));  
        if(!mNmea.SendMsg(n2kMsg1)) {
            ESP_LOGW(tag, "NMEA SendMsg failed");
        } 

        // PGN130306
        // AWA - Apparent Wind Data - requires GPS & COG/SOG data
        SetN2kWindSpeed(n2kMsg1, 1, aws, DegToRad(awa), N2kWind_Apparent);
        if (!mNmea.SendMsg(n2kMsg1))
        {
            ESP_LOGW(tag, "NMEA SendMsg failed");
        }

      
        // PGN129026 -- required, otherwise no TWS/TWD display
        SetN2kCOGSOGRapid(n2kMsg1, 1, N2khr_true, DegToRad(cog), sog);
        if (!mNmea.SendMsg(n2kMsg1))
        {
            ESP_LOGW(tag, "NMEA SendMsg failed");
        }

        // PGN130306
        // GWS - Ground Wind Speed - requires GPS & COG/SOG data;
        SetN2kWindSpeed(n2kMsg1, 1, avgTws, DegToRad(avgTwd), N2kWind_True_North);
        if (!mNmea.SendMsg(n2kMsg1))
        {
            ESP_LOGW(tag, "NMEA SendMsg failed");
        }

        // PGN126992 --- requires 129026 SOG/COG and 129029 GNSS
        // System Time is set via GNSS updates

        mNmea.ParseMessages();
    } 
}

NmeaDisplay::NmeaDisplay(gpio_num_t canTX, gpio_num_t canRX, DataQueue &dataQueue) : mrDataQueue(dataQueue), mNmea(canTX, canRX)
{
}



