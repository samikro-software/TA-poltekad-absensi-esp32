#include "rtc-ds3231.h"

uint8_t RtcDs3231::hexToDec(uint8_t hex){
    uint8_t dec = (hex & 0x0F);
    dec += ((hex & 0xF0) >> 4) * 10;

    return dec;
}

uint8_t RtcDs3231::decToHex(uint8_t dec){
    uint8_t hex = (dec / 10);
    hex <<= 4;
    hex |= (dec % 10);

    return hex;
}

void RtcDs3231::write(uint8_t addr, uint8_t* data, uint8_t len){
    Wire.beginTransmission(addr);
    for(uint8_t i=0; i<len; i++){
        Wire.write(data[i]);
    }
    Wire.endTransmission();
};

bool RtcDs3231::read(uint8_t addr, uint8_t* data, uint8_t len){
    byte res = Wire.requestFrom(addr, len);
    
    if(res == len){
        for(uint8_t i=0; i<len; i++){
            data[i] = Wire.read();
        }
        return true;  
    }
    return false;
};

void RtcDs3231::oscEnable(void){
    uint8_t data[2];
    data[0] = RTC_CR_REGISTER;
    data[1] = 0x00;
    write(RTC_ADDRESS, data, 2);
};

bool RtcDs3231::oscFlag(void){
    uint8_t data[2];
    data[0] = RTC_CR_REGISTER;
    
    write(RTC_ADDRESS, data, 1);
    
    data[0] = 0;
    read(RTC_ADDRESS, data, 1);
    
    return (data[0] & RTC_CR_EOSC)? true:false;
};

void RtcDs3231::clearOscStopFlag(void){
    uint8_t data[2];
    data[0] = RTC_SR_REGISTER;
    data[1] = 0x00;
    write(RTC_ADDRESS, data, 2);
};

bool RtcDs3231::oscStopFlag(void){
    uint8_t data[2];
	  data[0] = RTC_SR_REGISTER;

	  write(RTC_ADDRESS, data, 1);
	
	  data[0] = 0;
  	read(RTC_ADDRESS, data, 1);

	  return (data[0] & RTC_SR_OSF)?true:false;
};

void RtcDs3231::init(){
    Wire.begin();

    oscEnable();
    /**
    * if Oscilator Stop Flag (OSF) is Set
    * Enable Oscilator and Set Default DateTime 07:30:00 21-09-2023 (Date Software Created)
    */
    if(oscStopFlag()){  
        clearOscStopFlag();
        oscEnable();
    }
};

DATETIME_TypeDef RtcDs3231::getTime(){
    DATETIME_TypeDef dt;
    uint8_t buf[7];

    buf[0] = RTC_SECOND_REGISTER;
    write(RTC_ADDRESS, buf, 1);

    read(RTC_ADDRESS, buf, 7);

    // Serial.printf("%X, %X, %X, %X, %X, %X, %X\r\n", buf[2], buf[1], buf[0], buf[6], buf[5], buf[4], buf[3]);
    dt.time.second = hexToDec(buf[0]);
    dt.time.minute = hexToDec(buf[1]);
    dt.time.hour = hexToDec(buf[2]);
    
    dt.date.date = hexToDec(buf[4]);
    dt.date.month = hexToDec(buf[5]);
    dt.date.year = hexToDec(buf[6]);

    struct tm ptm;
    ptm.tm_hour = dt.time.hour;
    ptm.tm_min = dt.time.minute;
    ptm.tm_sec = dt.time.second;
    ptm.tm_mday = dt.date.date;
    ptm.tm_mon = dt.date.month - 1;
    ptm.tm_year = (dt.date.year + 2000) - 1900;
    ptm.tm_isdst = 0;
    
    time_t epochTime = mktime(&ptm);
    ptm = *gmtime (&epochTime); 

    /**
    * @brief days
    *  minggu = 0
    *  senin = 1
    *  selasa = 2
    *  rabu = 3 
    *  kamis = 4
    *  jumat = 5
    *  sabtu = 6
    */
    switch (hexToDec(ptm.tm_wday)) {
        /* Sunday */
        case 0: dt.day = FLAG_SUNDAY; break;
        case 1: dt.day = FLAG_MONDAY; break;
        case 2: dt.day = FLAG_TUESDAY; break;
        case 3: dt.day = FLAG_WEDNESDAY; break;
        case 4: dt.day = FLAG_THURSDAY; break;
        case 5: dt.day = FLAG_FRIDAY; break;
        case 6: dt.day = FLAG_SATURDAY; break;
    }

    if(oscFlag() == true){
        oscEnable();
    }

    return dt;
};

void RtcDs3231::setTime(DATETIME_TypeDef dt){
    struct tm ptm;
    ptm.tm_hour = dt.time.hour;
    ptm.tm_min = dt.time.minute;
    ptm.tm_sec = dt.time.second;
    ptm.tm_mday = dt.date.date;
    ptm.tm_mon = dt.date.month - 1;
    ptm.tm_year = (dt.date.year + 2000) - 1900;
    ptm.tm_isdst = 0;
    
    time_t epochTime = mktime(&ptm);
    ptm = *gmtime (&epochTime); 
    
    uint8_t buf[8];
    buf[0] = RTC_SECOND_REGISTER;
    buf[1] = decToHex(dt.time.second);
    buf[2] = decToHex(dt.time.minute);
    buf[3] = decToHex(dt.time.hour);
    buf[4] = decToHex(ptm.tm_wday) + 1;
    buf[5] = decToHex(dt.date.date);
    buf[6] = decToHex(dt.date.month);
    buf[7] = decToHex(dt.date.year);
    write(RTC_ADDRESS, buf, 8);

    delay(10);
};

void RtcDs3231::correction(TIME_TypeDef* time, int correction){
    if(correction < 0){
        correction *= -1;

        while(correction != 0){
            if(time->minute < correction){
                if(time->hour == 0) time->hour = 23;
                else time->hour--;

                if(correction >= 60)  correction -= 60;
                else{
                    time->minute = 60 - (correction - time->minute);
                    correction = 0;
                }
            }
            else{
                time->minute -= correction;
                correction = 0;
            }
        }
    }
    else{
        while(correction != 0){
            if(correction >= 60){
                time->minute += 60;
                correction -= 60;
            }
            else{
                time->minute += correction;
                correction = 0;
            }

            if(time->minute >= 60){
                time->minute -= 60;
                time->hour++;

                if(time->hour >= 24) time->hour -= 24;
            }
        }
    }
};

uint32_t RtcDs3231::minutes(DATETIME_TypeDef dt){
    uint32_t minute = 0;
    uint32_t day = 0;
    for(uint8_t i=0; i<dt.date.year; i++){
        if(!(i % 4))  day += 366;
        else          day += 365;
    }

    for(uint8_t i=1; i<dt.date.month; i++){
        switch(i){
            case 2 :
                if(!(dt.date.year % 4)) day += 29;
                else                    day += 28; 
            break;
            case 1:
            case 3:
            case 5:
            case 7:
            case 8:
            case 10:
            case 11:
                day += 31;
            break;
                day += 30;
            default :
            break;
        }
    }

    day += dt.date.date - 1;
    minute += day * 24 * 60;
    minute += dt.time.hour * 60;
    minute += dt.time.minute;

    return minute;
};