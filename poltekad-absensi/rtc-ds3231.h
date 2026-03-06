#ifndef __RTC_DS3231_H
#define __RTC_DS3231_H

#include <WiFi.h>
#include <Wire.h>
#include "time.h"

// Date Time structure
typedef enum{
    FLAG_SUNDAY = 0x01,
    FLAG_MONDAY = 0x02,
    FLAG_TUESDAY = 0x04,
    FLAG_WEDNESDAY = 0x08,
    FLAG_THURSDAY = 0x10,
    FLAG_FRIDAY = 0x20,
    FLAG_SATURDAY = 0x40,
}FlagDays_t;

typedef struct{
    uint8_t year;
    uint8_t month;
    uint8_t date;
}DATE_TypeDef;

typedef struct{
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
}TIME_TypeDef;

typedef struct{
    DATE_TypeDef date;
    FlagDays_t day;
    TIME_TypeDef time;
}DATETIME_TypeDef;

class RtcDs3231{
    private:
        /* RTC DS1307 */
        #define RTC_ADDRESS		0x68

        /* RTC DS3231 */
        /* REGISTER */
        #define RTC_SECOND_REGISTER	0x00
        #define RTC_MINUTE_REGISTER	0x01
        #define RTC_HOUR_REGISTER		0x02

        #define RTC_DAY_REGISTER		0x03

        #define RTC_DATE_REGISTER		0x04
        #define RTC_MONTH_REGISTER	0x05
        #define RTC_YEAR_REGISTER		0x06

        #define RTC_CR_REGISTER			0x0E
        #define RTC_SR_REGISTER			0x0F

        /* CONTROL REGISTER (CR) */
        #define RTC_CR_EOSC		0x80
        #define RTC_CR_BBSQW	0x40
        #define RTC_CR_CONV		0x20
        #define RTC_CR_RS			0x18
        #define RTC_CR_INTCN	0x04
        #define RTC_CR_A2IE		0x02
        #define RTC_CR_A1IE		0x01

        /* STATUS REGISER (SR) */
        #define RTC_SR_OSF			0x80
        #define RTC_SR_EN32KHZ 	0x08
        #define RTC_SR_BUSY			0x04
        #define RTC_SR_A1F			0x02
        #define RTC_SR_A2F			0x01

        uint8_t hexToDec(uint8_t hex);
        uint8_t decToHex(uint8_t dec);

        void write(uint8_t addr, uint8_t* data, uint8_t len);
        bool read(uint8_t addr, uint8_t* data, uint8_t len);

        void oscEnable(void);
        bool oscFlag(void);
        void clearOscStopFlag(void);
        bool oscStopFlag(void);

    public:
        void init();
        DATETIME_TypeDef getTime();
        void setTime(DATETIME_TypeDef dt);
        void correction(TIME_TypeDef* time, int correction);
        uint32_t minutes(DATETIME_TypeDef dt);
};

#endif