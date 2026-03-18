#include <WiFi.h>         // ESP32 version 2.1.17
#include <WiFiUDP.h>
#include <NTPClient.h>
const long utcOffsetInSeconds = 25200;
// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "id.pool.ntp.org", utcOffsetInSeconds);

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <Firebase_ESP_Client.h>

// #define API_KEY "AIzaSyDhZvfxFcPW4HEu9khIkBshOMeb0le6rBQ"
// #define DATABASE_URL "https://absensi-poltekad-default-rtdb.asia-southeast1.firebasedatabase.app"

// #define USER_EMAIL "samikro.id@gmail.com"
// #define USER_PASSWORD "CobaRtdb"

// Objek data Firebase
// FirebaseData fbdo;
// FirebaseAuth auth;
// FirebaseConfig config;

#include <lvgl.h>         // version 9.4.0 by kisvegabor
#include <TFT_eSPI.h>     // version 2.5.43 by Bodmer
#include "ui.h"

// const char* ssid     = "OFFICE RND";
// const char* password = "seipandaan";

const char* ssid     = "Valerie2";
const char* password = "ve12345678";

#include "rtc-ds3231.h"
RtcDs3231 rtc;

#include "rfid.h"
Rfid rf;

#include "SD.h"           // if conflict, delete SD folder in C:Users/../AppData/Local/Arduino15
#include "FS.h"
// microSD Card Reader connections
#define SD_CS         5
#define SPI_MOSI      23 
#define SPI_MISO      19
#define SPI_SCK       18
File dir;

/*Set to your screen resolution and rotation*/
#define TFT_HOR_RES   320
#define TFT_VER_RES   480
#define TFT_ROTATION  LV_DISPLAY_ROTATION_270

/*LVGL draw into this buffer, 1/10 screen size usually works well. The size is in bytes*/
// #define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 10 * (LV_COLOR_DEPTH / 8))
// uint32_t draw_buf[DRAW_BUF_SIZE / 4];

#define DRAW_BUF_SIZE (480 * 2)
uint32_t draw_buf[DRAW_BUF_SIZE];

TFT_eSPI tft = TFT_eSPI();
SemaphoreHandle_t p_mutex;

/* LVGL calls it when a rendered image needs to copied to the display*/
void my_disp_flush( lv_display_t *disp, const lv_area_t *area, uint8_t * px_map){
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);

  tft.dmaWait();
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)px_map, w * h, true);
  tft.endWrite();

  /*Call it to tell LVGL you are ready*/
  lv_display_flush_ready(disp);
}

/*use Arduinos millis() as tick source*/
static uint32_t my_tick(void) {
  return millis();
}

typedef struct{
  unsigned long wifi;
  unsigned long sdcard;
  unsigned long timer;
  unsigned long send;
  unsigned long card;
  unsigned long rfid;
  unsigned long scan;
}TIMEOUT_TypeDef;
TIMEOUT_TypeDef timeout;

typedef struct{
  bool sdcard;
  bool card;
  bool wifi;
  bool sending;
  bool success;
  bool fail;
  bool store;
}FLAG_TypeDef;
FLAG_TypeDef flag;

typedef struct{
  unsigned char finger;
  char uuid[10];
  char name[50];
  char dts[20];
  DATETIME_TypeDef dt;
}ID_TypeDef;
ID_TypeDef id;

unsigned char modal = 0;

TaskHandle_t Task1;
TaskHandle_t Task2;

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}

void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\n", path);
    if(fs.remove(path)){
        Serial.println("File deleted");
    } else {
        Serial.println("Delete failed");
    }
}

void setup()
{
  delay(3000);
  Serial.begin( 115200 );
  String LVGL_Arduino = "Hello World! ";
  LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.println( LVGL_Arduino );

  p_mutex = xSemaphoreCreateMutex();

  tft.begin();
  tft.setRotation(3); // Landscape

  lv_init();
  /*Set a tick source so that LVGL will know how much time elapsed. */
  lv_tick_set_cb(my_tick);

  // /*TFT_eSPI can be enabled lv_conf.h to initialize the display in a simple way*/
  lv_display_t * disp = lv_tft_espi_create(TFT_HOR_RES, TFT_VER_RES, draw_buf, sizeof(draw_buf));
  lv_display_set_rotation(disp, TFT_ROTATION);

  ui_init();
  lv_obj_add_flag(objects.pl_modal, LV_OBJ_FLAG_HIDDEN);

  rtc.init();
  rf.init();

  // Set microSD Card CS as OUTPUT and set HIGH
  pinMode(SD_CS, OUTPUT);      
  digitalWrite(SD_CS, HIGH); 
  
  // Initialize SPI bus for microSD Card
  // SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  
  // Start microSD Card
  flag.sdcard = SD.begin(SD_CS);

  if(!flag.sdcard) {
    Serial.println("Error accessing microSD card!");
  }

  timeout.timer = millis();
  timeout.sdcard = millis();
  timeout.wifi = millis() - 55000;
  timeout.rfid = millis();

  flag.wifi = false;
  flag.sending = false;
  flag.success = false;
  flag.fail = false;

  //create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(
    Task1code,   /* Task function. */
    "Task1",     /* name of task. */
    8192,       /* Stack size of task */
    NULL,        /* parameter of the task */
    1,           /* priority of the task */
    &Task1,      /* Task handle to keep track of created task */
    0          /* pin task to core 0 */
  );

  xTaskCreatePinnedToCore(
    Task2code,   /* Task function. */
    "Task2",     /* name of task. */
    8192,       /* Stack size of task */
    NULL,        /* parameter of the task */
    1,           /* priority of the task */
    &Task2,      /* Task handle to keep track of created task */
    1          /* pin task to core 0 */
  );
}

void loop(){}

//Task1code: blinks an LED every 1000 ms
void Task1code( void * pvParameters ){
  for(;;){
    lv_timer_handler(); /* let the GUI do its work */
    ui_tick(); // Penting untuk EEZ Flow

    if((millis() - timeout.timer) > 1000){
      if(xSemaphoreTake(p_mutex, pdMS_TO_TICKS(100))){
        id.dt = rtc.getTime();
        lv_label_set_text_fmt(objects.lb_time, "%02d:%02d:%02d", id.dt.time.hour, id.dt.time.minute, id.dt.time.second);
        lv_label_set_text_fmt(objects.lb_date, "%02d-%02d-20%02d", id.dt.date.date, id.dt.date.month, id.dt.date.year);

        if(flag.wifi) lv_label_set_text_fmt(objects.lb_connection, "%s", WiFi.localIP().toString());
        else lv_label_set_text_fmt(objects.lb_connection, "%s", ssid);

        if(flag.success){
          lv_label_set_text(objects.lb_mark, "Berhasil");
          lv_label_set_text(objects.lb_name, id.name);
          lv_label_set_text(objects.lb_report_time, id.dts);
          lv_obj_add_flag(objects.spn_load, LV_OBJ_FLAG_HIDDEN);
          lv_obj_remove_flag(objects.pl_success, LV_OBJ_FLAG_HIDDEN);
        }
        else if(flag.fail){
          lv_label_set_text(objects.lb_mark, "Simpan");
          lv_label_set_text(objects.lb_name, "Memori");
          lv_label_set_text(objects.lb_report_time, id.dts);
          lv_obj_add_flag(objects.spn_load, LV_OBJ_FLAG_HIDDEN);
          lv_obj_remove_flag(objects.pl_success, LV_OBJ_FLAG_HIDDEN);

          if(!flag.store){
            flag.store = true;
            char directory[20];
            sprintf(directory, "/%s", id.dts);
            writeFile(SD, directory, id.uuid);
          }
        }
        else if(flag.sending){
          lv_label_set_text(objects.lb_mark, "Mengirim !!!");
          lv_obj_remove_flag(objects.pl_modal, LV_OBJ_FLAG_HIDDEN);
          lv_obj_remove_flag(objects.spn_load, LV_OBJ_FLAG_HIDDEN);
          lv_obj_add_flag(objects.pl_success, LV_OBJ_FLAG_HIDDEN);
        }

        xSemaphoreGive(p_mutex);
      }

      timeout.timer = millis();
    }

    if((millis() - timeout.sdcard) > 10000){
      File root = SD.open("/");
      if(!root){
        Serial.println("Failed to open directory");
        // flag.sdcard = false;
        // return;
      }
      else if(!root.isDirectory()){
        Serial.println("Not a directory");
        // return;
      }
      else {
        File file = root.openNextFile();
        while(file){
            if(file.isDirectory()){
                Serial.print("  DIR : ");
                Serial.println(file.name());
            } else {
                Serial.print("  FILE: ");
                Serial.print(file.name());
                Serial.print("  SIZE: ");
                Serial.println(file.size());
            }
            file = root.openNextFile();

            yield();
            // break;
        }
        file.close();
      }
      root.close();

      timeout.sdcard = millis();
    }
    if((millis() - timeout.scan) > 250){
      if(xSemaphoreTake(p_mutex, pdMS_TO_TICKS(100))){
        if(!flag.sending){
          if(!flag.card){
            byte card_uuid[5];
            unsigned char card_length = rf.loop(card_uuid);
            if(card_length){
              sprintf(id.uuid, "%02X%02X%02X%02X", card_uuid[0], card_uuid[1], card_uuid[2], card_uuid[3]);
              sprintf(id.dts, "20%02d-%02d-%02d %02d:%02d:%02d", id.dt.date.year, id.dt.date.month, id.dt.date.date, id.dt.time.hour, id.dt.time.minute, id.dt.time.second);
              timeout.rfid = millis();
              flag.card = true;
              flag.sending = true;
              flag.store = false;
              timeout.card = millis();
              lv_label_set_text_fmt(objects.lb_scan, "%s", id.uuid);
              lv_label_set_text(objects.lb_mark, "Silahkan Tempel Sidik Jari !!!");
            }
          }
          else if((millis() - timeout.card) > 10000){
            flag.card = false;
            flag.success = false;
            flag.fail = false;
            lv_label_set_text_fmt(objects.lb_scan, "kosong");
            lv_label_set_text(objects.lb_mark, "Tempel Kartu Di Bawah Ini !!!");
            lv_obj_add_flag(objects.pl_modal, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.spn_load, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.pl_success, LV_OBJ_FLAG_HIDDEN);
          }
        }
        else if((millis() - timeout.rfid) > 60000){
          rf.init();
          timeout.rfid = millis();
        }

        xSemaphoreGive(p_mutex);
      }

      timeout.scan = millis();
    }

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

//Task2code: blinks an LED every 1000 ms
void Task2code( void * pvParameters ){
  for(;;){
    if(WiFi.status() == WL_CONNECTED){
      if(!flag.wifi){
        timeClient.setUpdateInterval(60000);
        timeClient.begin();

        // // Gunakan server NTP terdekat (Indonesia)
        configTime(7 * 3600, 0, "0.id.pool.ntp.org", "1.id.pool.ntp.org");

        // // Konfigurasi Firebase
        // config.api_key = API_KEY;
        // config.database_url = DATABASE_URL;

        // /* Assign the user sign in credentials */
        // auth.user.email = USER_EMAIL;
        // auth.user.password = USER_PASSWORD;

        // // Since v4.4.x, BearSSL engine was used, the SSL buffer need to be set.
        // // Large data transmission may require larger RX buffer, otherwise connection issue or data read time out can be occurred.
        // fbdo.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);

        // // Limit the size of response payload to be collected in FirebaseData
        // fbdo.setResponseSize(2048);

        // // // Login sebagai anonim (karena rules kita set true)
        // Firebase.reconnectWiFi(true);
        // fbdo.keepAlive(true, 5, 20);
        // Firebase.begin(&config, &auth);

        // Firebase.setDoubleDigits(5);

        // config.timeout.serverResponse = 10 * 1000;
        // config.signer.test_mode = true; // Mode testing terkadang bypass beberapa check

        flag.wifi = true;
      }
      else{
        bool ready = false;
        String jsonStr;

        if(xSemaphoreTake(p_mutex, pdMS_TO_TICKS(100))){
          if(flag.sending){
            FirebaseJson payload;
            payload.add("uuid", id.uuid);
            payload.add("dateAt", id.dts);
            payload.add("state", "masuk");

            payload.toString(jsonStr, true);
            ready = true;
          }
          xSemaphoreGive(p_mutex);
        }
        // if (Firebase.ready() && (millis() - timeout.send) > 10000) {
        //   timeout.send = millis();
        //   Serial.println("Kirim");
        //   // Menggunakan setFloat untuk angka desimal
        //   if (Firebase.RTDB.setFloat(&fbdo, "/sensor/suhu", 13.0)) {
        //     Serial.println("Data terkirim ke: " + fbdo.dataPath());
        //   } else {
        //     Serial.println("Gagal: " + fbdo.errorReason());
        //   }
        // }

        if(ready){
          WiFiClientSecure client;
          client.setInsecure(); // <--- Ini kuncinya, tidak perlu CA Cert
          client.setTimeout(20);

          HTTPClient http;
          http.setTimeout(20000);
          // URL wajib diawali https://
          if(http.begin(client, "https://us-central1-absensi-poltekad.cloudfunctions.net/api")){
            Serial.println("start");
            http.addHeader("Content-Type", "application/json");
            http.addHeader("Host", "https://us-central1-absensi-poltekad.cloudfunctions.net");

            int httpResponseCode = http.POST(jsonStr);

            if (httpResponseCode > 0) {
              String responBody = http.getString();
              Serial.println(responBody);

              FirebaseJson json;
              json.setJsonData(responBody); // Masukkan string JSON ke objek

              FirebaseJsonData result; // Objek penampung hasil ekstraksi

              // Ambil data dengan sistem Path
              json.get(result, "status");

              if(xSemaphoreTake(p_mutex, pdMS_TO_TICKS(100))){
                if (result.success) {
                  memset(id.name, 0, sizeof(id.name));
                  sprintf(id.name, "%s", result.stringValue.c_str());
                }

                flag.sending = false;                
                flag.success = true;
                timeout.card = millis();
                xSemaphoreGive(p_mutex);
              }

              Serial.printf("Berhasil, kode: %d\n", httpResponseCode);
            } else {
              if(xSemaphoreTake(p_mutex, pdMS_TO_TICKS(100))){
                flag.sending = false;
                flag.fail = true;
                timeout.card = millis();
                xSemaphoreGive(p_mutex);
              }
              
              Serial.printf("Gagal: %s\n", http.errorToString(httpResponseCode).c_str());
            }
            http.end();
          }
        }
        
        /* NTP Section */
        if(timeClient.update() && timeClient.isTimeSet()){
          if(xSemaphoreTake(p_mutex, pdMS_TO_TICKS(100))){
            DATETIME_TypeDef dt;
            time_t epochTime = timeClient.getEpochTime();

            struct tm *ptm = gmtime ((time_t *)&epochTime); 
            dt.date.date = ptm->tm_mday;
            dt.date.month = ptm->tm_mon+1;
            dt.date.year = (ptm->tm_year - 100);		// (1900 - 2000)
            dt.time.hour = ptm->tm_hour;
            dt.time.minute = ptm->tm_min;
            dt.time.second = ptm->tm_sec;
            dt.day = (FlagDays_t) timeClient.getDay();

            rtc.setTime(dt);

            timeClient.end();

            xSemaphoreGive(p_mutex);
          }
        }
      }      
    }
    else{
      if(xSemaphoreTake(p_mutex, pdMS_TO_TICKS(100))){
        flag.wifi = false;
        flag.sending = false;
        xSemaphoreGive(p_mutex);
      }

      if((millis() - timeout.wifi) > 60000){
        Serial.println("Connecting...");
        WiFi.disconnect();
        WiFi.mode(WIFI_STA);

        yield();
        WiFi.begin(ssid, password);

        timeout.wifi = millis(); //  reset wifi_timeout
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

