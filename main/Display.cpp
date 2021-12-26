#include "Display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esputil.h"
#include "math.h"

static const char tag[] = "Display";

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

Display::Display(gpio_num_t canTX, gpio_num_t canRX) : mNmea(canTX, canRX)
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
    const unsigned long TransmitMessages[]={129026L, 130306L, 127250L, 128259L, 129029L, 0};    // 130306L PGN: Wind
    mNmea.ExtendTransmitMessages(TransmitMessages);
    mNmea.Open();
}
///////////////////// END NMEA SPECIFIC ////////////////////////////////

void Display::Send(float temp)
{

    long double tws = KnotsToms(10.0); // true wind speed
    long double twa = DegToRad(0.0); // true wind angle (relative to heading)
    long double aws = KnotsToms(temp); // apparent wind speed
    long double awa = DegToRad(180.0+44.0); // apparent wind angle
    //////////////////////////////////
    //////// NMEA TEST CODE
    tN2kMsg n2kMsg1;
    /*windspeed++;
        if (windspeed > 20) {
            windspeed = 0;
        }
        windangle = windspeed*15.0;*/


// TODO REVERSE CALCULATION OF COG SOG APPARENT WIND!!!  ****************************************************************
    // https://en.wikipedia.org/wiki/Apparent_wind


    // PGN129029
    static const double latitude = 47.940703;
    static const double longitude = 13.595386;
    static const double altitude = 469;
    static const double hdop = 1; // horizontal dilution in meters
    SetN2kGNSS(n2kMsg1, 1, 18610, 118000, latitude, longitude, altitude, N2kGNSSt_GPS, N2kGNSSm_GNSSfix, 10, hdop);
    if (!mNmea.SendMsg(n2kMsg1))
    {
        ESP_LOGW(tag, "NMEA SendMsg failed");
    }

    // PGN127250
    /*SetN2kMagneticHeading(n2kMsg1, 1, N2khr_magnetic, DegToRad(0), 0);
    if(!mNmea.SendMsg(n2kMsg1)) {
        ESP_LOGW(tag, "NMEA SendMsg failed");
    } */

    // PGN130306
    // AWA - Apparent Wind Data - requires GPS & COG/SOG data
    //windspeedAvg = mMovingWindspeedAvg(windspeed);
    //windangleAvg = mMovingWindangleAvg(windangle);
    SetN2kWindSpeed(n2kMsg1, 1, aws, awa, N2kWind_Apparent);
    if (!mNmea.SendMsg(n2kMsg1))
    {
        ESP_LOGW(tag, "NMEA SendMsg failed");
    }
    mNmea.ParseMessages();

    // Boat Velocity = 1/2 (2 A cos(β) + sqrt(2) sqrt(-A^2 + 2 W^2 + A^2 cos(2 β)))
    //SetN2kCOGSOGRapid(n2kMsg1, 1, N2khr_magnetic, DegToRad(12), KnotsToms(34));

    // To display Apparent wind indpendently from True-Wind, we calculate Speed over Ground (which we dont need for other use)

    long double sog = 1.0l / 2.0l * ( 2.0l * aws * cos(awa) + sqrt(2.0l) * sqrt(- aws*aws + 2.0l * tws*tws + aws*aws * cos(2.0l * awa)));
    long double cog = acos((aws * cos(awa) - sog) / tws); // course over ground, heading vs. true north
    long double twd = fmod(M_TWOPI + cog + twa, M_TWOPI);

    ESP_LOGI(tag, "SOG: %.2Lf, AWS: %.2Lf, AWA: %.2Lf, TWS: %.2Lf, TWD: %.2Lf, COG: %.2Lf", sog, aws, awa, tws, twd, cog);

    // PGN127250
    SetN2kTrueHeading(n2kMsg1, 1, DegToRad(cog));
    if (!mNmea.SendMsg(n2kMsg1))
    {
        ESP_LOGW(tag, "NMEA SendMsg failed");
    }


    // PGN128259
    SetN2kBoatSpeed(n2kMsg1, 1, KnotsToms(sog), N2kDoubleNA, N2kSWRT_Paddle_wheel); // does not influence TWD, but must be present
    if (!mNmea.SendMsg(n2kMsg1))
    {
        ESP_LOGW(tag, "NMEA SendMsg failed");
    }


    //sog = KnotsToms(7);
    // PGN129026
    SetN2kCOGSOGRapid(n2kMsg1, 1, N2khr_true, cog, sog);
    if (!mNmea.SendMsg(n2kMsg1))
    {
        ESP_LOGW(tag, "NMEA SendMsg failed");
    }

     // PGN130306
    // GWS - Ground Wind Speed - requires GPS & COG/SOG data; 
    SetN2kWindSpeed(n2kMsg1, 1, tws, twd, N2kWind_True_North); 
    if (!mNmea.SendMsg(n2kMsg1))
    {
        ESP_LOGW(tag, "NMEA SendMsg failed");
    }



}