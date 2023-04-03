#include "esp_system.h"
#include "esp_log.h"
#include "RtcVariables.h"
#include "esputil.h"


// these variables survive software restarts
// those must be initialized explicitly by checking reset reason power-up 
RTC_NOINIT_ATTR int rtcVarModemRestarts;
RTC_NOINIT_ATTR RtcVariables::ExtendedResetReason rtcVarResetReason;
static RtcVariables::ExtendedResetReason resetReason;

static const char tag[] = "RtcVariables";

void RtcVariables::Init() {
    // reset diagnostics variables, if not soft-restart
    if (esp_reset_reason() == ESP_RST_POWERON || esp_reset_reason() == ESP_RST_BROWNOUT)
    {
        ESP_LOGI(tag, "Resetting diagnostics variables");
        rtcVarModemRestarts = 0;
    }

    resetReason = rtcVarResetReason;
    rtcVarResetReason = ExtendedResetReason::EXTENDED_RESETREASON_NONE;
}

void RtcVariables::Reset() {
    rtcVarModemRestarts = 0;
    rtcVarResetReason = ExtendedResetReason::EXTENDED_RESETREASON_NONE;
    ESP_LOGW(tag, "Resetting RTC Variables!!");
}

void RtcVariables::IncModemRestarts() {
    rtcVarModemRestarts += 1;
};

int RtcVariables::GetModemRestarts() {
    return rtcVarModemRestarts;
};

void RtcVariables::SetExtendedResetReason(ExtendedResetReason resetReason) {
    rtcVarResetReason = resetReason;
};

const char* RtcVariables::GetExtendedResetReasonText() {
    if (esp_reset_reason() == ESP_RST_SW) {
        switch (resetReason) {
            case ExtendedResetReason::EXTENDED_RESETREASON_NONE : break;
            case ExtendedResetReason::EXTENDED_RESETREASON_USER : return "User triggered restart";
            case ExtendedResetReason::EXTENDED_RESETREASON_WATCHDOG : return "Application Watchdog: Could not reach server.";
            case ExtendedResetReason::EXTENDED_RESETREASON_ERROR : return "Application Error";
            case ExtendedResetReason::EXTENDED_RESETREASON_FIRMWAREUPDATE : return "Restart after firmware update";
            case ExtendedResetReason::EXTENDED_RESETREASON_CONFIG : return "Restart after config change";
            case ExtendedResetReason::EXTENDED_RESETREASON_MODEMFULLPOWERFAILED : return "Could not switch modem into full power mode";
            case ExtendedResetReason::EXTENDED_RESETREASON_CONNECTIONRETRIES : return "Too many retries failed connecting the server";
        }
    }
    return esp32_getresetreasontext(esp_reset_reason());
};

RtcVariables::ExtendedResetReason RtcVariables::GetExtendedResetReason() {
    return resetReason;
};

