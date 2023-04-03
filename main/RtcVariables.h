#ifndef MAIN_RTC_VARIABLES_H_
#define MAIN_RTC_VARIABLES_H_



class RtcVariables {
    public: 
        static void Init();
        static void Reset();

        static void IncModemRestarts();
        static int GetModemRestarts();

        enum ExtendedResetReason {
            EXTENDED_RESETREASON_NONE,
            EXTENDED_RESETREASON_USER,
            EXTENDED_RESETREASON_WATCHDOG,
            EXTENDED_RESETREASON_ERROR,
            EXTENDED_RESETREASON_FIRMWAREUPDATE,
            EXTENDED_RESETREASON_CONFIG,
            EXTENDED_RESETREASON_MODEMFULLPOWERFAILED,
            EXTENDED_RESETREASON_CONNECTIONRETRIES
        };

        static void SetExtendedResetReason(ExtendedResetReason resetReason);
        
        static ExtendedResetReason GetExtendedResetReason();

        /// @brief returns the extended reset reason message
        /// @return ESP reset reason, or if Software Reset and extended reason is set, then extended reason is returned.
        static const char * GetExtendedResetReasonText();

};


#endif