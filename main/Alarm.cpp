//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include "Alarm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "math.h"
#include <cmath>
#include "stdlib.h"
#include "GeoUtil.h"

static const char tag[] = "Alarm";

void fAlarmTask(void *pvParameter)
{
    ((Alarm *)pvParameter)->AlarmTask();
    vTaskDelete(NULL);
}

void Alarm::Start()
{
    xTaskCreate(&fAlarmTask, "Alarm", 1024 * 4, this, ESP_TASK_MAIN_PRIO, NULL); // large stack is needed
}

void Alarm::AlarmTask()
{
    if (mrConfig.mbAlarmSound)
    {
        gpio_config_t gpioConfig;
        gpioConfig.mode = GPIO_MODE_OUTPUT;
        gpioConfig.intr_type = GPIO_INTR_DISABLE;
        gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
        gpioConfig.pull_up_en = GPIO_PULLUP_DISABLE;
        gpioConfig.pin_bit_mask = (1ULL<<mGpioBuzzer);
        gpio_config(&gpioConfig);
        gpio_set_drive_capability(mGpioBuzzer, GPIO_DRIVE_CAP_3);
        ESP_LOGI(tag, "Buzzer enabled on GPIO%d", mGpioBuzzer);
        BuzzerOff();
    }

    if (mrConfig.miAlarmRadius < 10)
    {
        // ESP_LOGE(tag, "Alarm Radius too small")
    }

    static const short ARMING_SECONDS = 30;   // 60 counts = 60 seconds
    static const short COOLDOWN_SECONDS = 60; // 30 seconds
    static const short MAXALARMACTIVESECONDS = 300; // 5 minutes

    unsigned int countArming = 0;
    unsigned int countTilt = 0;
    unsigned int countOrient = 0;
    unsigned int countUnplugged = 0;
    unsigned int countGeoFence = 0;

    unsigned int alarmTriggers = 0;

    int timestampAlarmTriggered = 0;
    int timestampAlarmCooldown = 0;
    while (true)
    {
        Data data;
        unsigned short absAvgTilt = 0;
        unsigned int geoDislocation = 0;

        if (mrDataQueue.GetAlarmData(data, 3))
        {

            // 60 seconds of data before arming system
            if (countArming < ARMING_SECONDS)
            {
                //ESP_LOGI(tag, "Arming %d", countArming);
                countArming++;
            }
            else if (countArming == ARMING_SECONDS)
            {
                countArming++;
                ESP_LOGW(tag, "ARMED! Buzzer %s", mrConfig.mbAlarmSound ? "activated" : "muted");
            }

            countUnplugged = 0;

            // average the individual angles, so that a rocking buoy is averaging upright
            // however, when a malicious activity happens, the tilt is likely to happen steady in one direction
            float avgXTilt = movAvgTiltXmm(data.xtilt);
            float avgYTilt = movAvgTiltYmm(data.ytilt);
            absAvgTilt = abs(avgXTilt) + abs(avgYTilt);

            #if LOG_LOCAL_LEVEL >= LOG_DEFAULT_LEVEL_DEBUG || CONFIG_LOG_DEFAULT_LEVEL >= LOG_DEFAULT_LEVEL_DEBUG
                unsigned short absTilt = abs(data.xtilt) + abs(data.ytilt);
                ESP_LOGD(tag, "Tilt angles: xtilt=%d, ytilt=%d tilt=%d avgtilt=%d", data.xtilt, data.ytilt, absTilt, absAvgTilt);
            #endif 



            // if zorient is -1 then maximet is tilted upside down (more than 90° tilt)
            if (data.zorient < 0)
            {
                countOrient++;
                //ESP_LOGD(tag, "UPSIDE DOWN!");
            }
            else
            {
                countOrient = 0;
            }

            // the sum of both tilts is max 90°
            if (absAvgTilt > miTiltThreshold)
            {
                countTilt++;
                //ESP_LOGD(tag, "TILT! %d°/%d° (%d°)", data.xtilt, data.ytilt, absAvgTilt);
            }
            else
            {
                countTilt = 0;
            }

            // geofence
            if (mrConfig.mdAlarmLatitude && mrConfig.mdAlarmLongitude && data.lat && data.lon && !isnan(mrConfig.mdAlarmLatitude) && !isnan(mrConfig.mdAlarmLongitude) && !isnan(data.lat) && !isnan(data.lon))
            {
                geoDislocation = geoDistance(mrConfig.mdAlarmLatitude, mrConfig.mdAlarmLongitude, data.lat, data.lon);
                if (geoDislocation > mrConfig.miAlarmRadius)
                {
                    // if bogus distance of >100km reset geofence
                    if (geoDislocation > 100000)
                    {
                        countGeoFence = 0;
                    }
                    else
                    {
                        countGeoFence++;
                    }
                }
                else
                {
                    countGeoFence = 0;
                }
            }

            if (countTilt >= 1)
            {
                alarmTriggers |= TILT;
            }

            if (countOrient >= 9)
            {
                alarmTriggers |= ORIENT;
            }

            if (countGeoFence >= 5)
            {
                alarmTriggers |= GEOFENCE;
            }
        }
        else
        {
            countUnplugged++;
        }

        if (countUnplugged >= 2)
        {
            alarmTriggers |= UNPLUGGED;
        }

        if (countArming < ARMING_SECONDS)
        {
            // reset triggers during arming phase, extend arming --> arm only if all is good
            if (countUnplugged || countGeoFence || countOrient || countTilt || alarmTriggers) {
                countArming = 0;
                alarmTriggers = 0;
                countGeoFence = 0;
                countUnplugged = 0;
                countOrient = 0;
                countTilt = 0;
            }
            continue;
        }

        int timestampSecondsNow = esp_timer_get_time() / 1000000;                   // seconds since start (good enough as int can store seconds over 68 years in 31 bits)
        int secondsAlarmActive = timestampSecondsNow - timestampAlarmTriggered + 1; // add one second to be sure value > 0
        if (alarmTriggers)
        {
            if (timestampAlarmTriggered)
            {
                // turn off buzzer in any case after this #seconds
                if (secondsAlarmActive > MAXALARMACTIVESECONDS)
                {
                    //ESP_LOGW(tag, "BUZZER DURATION EXCEEDED. TURNING OFF!");
                    BuzzerOff();
                }
            }
            else
            {
                timestampAlarmTriggered = timestampSecondsNow;
                timestampAlarmCooldown = 0;
                if (mrConfig.mbAlarmSound)
                {
                    BuzzerOn();
                }
                if (alarmTriggers & GEOFENCE)
                {
                    //ESP_LOGE(tag, "GEOFENCE ALARM: %.0dm is outside of %d radius", geoDislocation, mrConfig.miAlarmRadius);
                    String info;
                    info.printf("WEATHERBUOY OUTSIDE GEOFENCE!\r\nBuoy is %.0dm off! (max: %d)\r\nhttps://maps.google.com/?q=%0.8f,%0.8f", geoDislocation, mrConfig.miAlarmRadius, data.lat, data.lon);
                    msAlarmInfo += info;
                }
                if (alarmTriggers & TILT)
                {
                    //ESP_LOGE(tag, "TILT ALARM: %d° (%d°/%d°) %s", absAvgTilt, data.xtilt, data.ytilt, data.zorient < 0 ? "UPSIDE DOWN!!" : "");
                    String info;
                    info.printf("WEATHERBUOY MAST MANIPULATION!\r\nTILT: %ddeg (%dx/%dy) %s", absAvgTilt, data.xtilt, data.ytilt, data.zorient < 0 ? "UPSIDE DOWN!!" : "UP");
                    msAlarmInfo += info;
                }
                if (alarmTriggers & ORIENT)
                {
                    //ESP_LOGE(tag, "ORIENT ALARM: %d° (%d°/%d°) %s", absAvgTilt, data.xtilt, data.ytilt, data.zorient < 0 ? "UPSIDE DOWN!!" : "");
                    String info;
                    info.printf("WEATHERBUOY MAST MANIPULATION!\r\nMAXIMET UPSIDE DOWN %d READINGS: -%ddeg (%dx/%dy)", countOrient, absAvgTilt, data.xtilt, data.ytilt );
                    msAlarmInfo += info;
                }
                if (alarmTriggers & UNPLUGGED)
                {
                    //ESP_LOGE(tag, "UNPLUGGED ALARM!");
                    String info;
                    info.printf("WEATHERBUOY SABOTAGE!\r\nMAXIMET OFFLINE/UNPLUGGED.");
                    msAlarmInfo += info;
                }
                ESP_LOGE(tag, "ALARM INFO: %s", msAlarmInfo.c_str());

                // trigger main-thread --- dont worry about duplicate data in case of alarm
                mbAlarm = true;
                mrDataQueue.PutData(data);
            }

        }
        else
        {
            if (timestampAlarmTriggered)
            {
                // start cooldown-phase
                if (!timestampAlarmCooldown)
                {
                    timestampAlarmCooldown = timestampSecondsNow;
                }

                int secondsCooldownActive = timestampSecondsNow - timestampAlarmCooldown;
                if (secondsCooldownActive > COOLDOWN_SECONDS)
                {
                    timestampAlarmTriggered = 0;
                    timestampAlarmCooldown = 0;
                    countGeoFence = 0;
                    countOrient = 0;
                    countTilt = 0;
                    countUnplugged = 0;
                    countArming = 0;
                    alarmTriggers = 0;
                    BuzzerOff();
                    ESP_LOGW(tag, "DISARMED!");
                }
            }
            else
            {
                BuzzerOff();
            }
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void Alarm::BuzzerOn()
{
    if (mrConfig.mbAlarmSound)
    {
        gpio_set_level(mGpioBuzzer, 1);
    }
};

void Alarm::BuzzerOff()
{
    gpio_set_level(mGpioBuzzer, 0);
};

String Alarm::GetAlarmInfo()
{
    return msAlarmInfo;
}

Alarm::Alarm(DataQueue &dataQueue, Config &config, gpio_num_t buzzer) : mrDataQueue(dataQueue), mrConfig(config), mGpioBuzzer(buzzer)
{
}
