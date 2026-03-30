#include <WiFi.h>         // ESP32 version 2.0.17
#include <WiFiUDP.h>
#include <NTPClient.h>
const long utcOffsetInSeconds = 25200;
// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "id.pool.ntp.org", utcOffsetInSeconds);

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <Firebase_ESP_Client.h>

#include <lvgl.h>         // version 9.4.0 by kisvegabor
#include <TFT_eSPI.h>     // version 2.5.43 by Bodmer
#include "ui.h"

// const char* ssid     = "OFFICE RND";
// const char* password = "seipandaan";

// const char* ssid     = "Valerie2";
// const char* password = "ve1234567";

const char* ssid     = "samikro.id";
const char* password = "samikroid23";

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

#include <Adafruit_Fingerprint.h>
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Serial2);

/*Set to your screen resolution and rotation*/
#define TFT_HOR_RES   320
#define TFT_VER_RES   480
#define TFT_ROTATION  LV_DISPLAY_ROTATION_270

#define DRAW_BUF_SIZE (480 * 2)
uint32_t draw_buf[DRAW_BUF_SIZE];

TFT_eSPI tft = TFT_eSPI();
SemaphoreHandle_t p_mutex;

#define BUTTON_PIN  25
#define BUTTON_IS_PRESSED()   (digitalRead(BUTTON_PIN) == LOW)

#define DOOR_PIN  32
#define DOOR_IS_PRESSED()   (digitalRead(DOOR_PIN) == LOW)

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

typedef enum{
  ENROLL_RESET = 0,
  ENROLL_FINGER1,
  ENROLL_FINGER2,
  ENROLL_STORE,
  ENROLL_FINISH
}ENROLL_t;
ENROLL_t enroll_process;

typedef struct{
  unsigned long wifi;
  unsigned long sdcard;
  unsigned long timer;
  unsigned long send;
  unsigned long card;
  unsigned long rfid;
  unsigned long scan;
  unsigned long sensor;
  unsigned long info;
  unsigned long button;
  unsigned long enroll;
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
  bool finger;
  bool sensor;
  bool pressed;
  bool enroll;
}FLAG_TypeDef;
FLAG_TypeDef flag;

typedef struct{
  char uuid[10];
  char name[50];
  char dts[20];
  int finger;
  DATETIME_TypeDef dt;
}ID_TypeDef;
ID_TypeDef id;

unsigned char modal = 0;

TaskHandle_t Task1;
TaskHandle_t Task2;

void writeFile(fs::FS &fs, const char * path, const char * message){
  char directory[40];
  sprintf(directory, "/absensi/%s.txt", path);
  directory[22] = '_';
  directory[25] = '_';

  Serial.printf("Writing file: %s\n", directory);

  File file = fs.open(directory, FILE_WRITE);
  if(!file){
      Serial.println("Failed to open file for writing");
      return;
  }
  
  if(file.print(message)) Serial.println("File written");
  else Serial.println("Write failed");
  
  file.close();
}

void deleteFile(fs::FS &fs, const char * path){
  char directory[40];
  sprintf(directory, "/absensi/%s.txt", path);
  directory[22] = '_';
  directory[25] = '_';

  Serial.printf("Deleting file: %s\n", directory);

  if(fs.remove(directory)) Serial.println("File deleted");
  else Serial.println("Delete failed");
}

bool waitRemoveFinger(){
  return (finger.getImage() == FINGERPRINT_NOFINGER)? true : false;
}

int setFingerprint(){
  int p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    p = finger.storeModel(id.finger);
  } 

  return p;
}

int getFingerprint(unsigned char slot){
  int p = finger.getImage();

  if(p == FINGERPRINT_OK){
    p = finger.image2Tz(slot);
  }
  
  return p;
}

// returns -1 if failed, otherwise returns ID #
int getFingerprintIDez() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) return -1;

  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);
  return finger.fingerID;
}

void fingerInit(){
  // set the data rate for the sensor serial port
  finger.begin(57600);
  delay(5);
  if (finger.verifyPassword()) flag.sensor = true;
  else flag.sensor = false;

  Serial.println(F("Reading sensor parameters"));

  finger.getParameters();
  Serial.print(F("Status: 0x")); Serial.println(finger.status_reg, HEX);
  Serial.print(F("Sys ID: 0x")); Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: ")); Serial.println(finger.capacity);
  Serial.print(F("Security level: ")); Serial.println(finger.security_level);
  Serial.print(F("Device address: ")); Serial.println(finger.device_addr, HEX);
  Serial.print(F("Packet len: ")); Serial.println(finger.packet_len);
  Serial.print(F("Baud rate: ")); Serial.println(finger.baud_rate);

  // finger.getTemplateCount();

  // if (finger.templateCount == 0) {
  //   Serial.print("Sensor doesn't contain any fingerprint data. Please run the "
  //                "'enroll' example.");
  // } else {
  //   Serial.println("Waiting for valid finger...");
  //   Serial.print("Sensor contains ");
  //   Serial.print(finger.templateCount);
  //   Serial.println(" templates");
  // }
}

void setup()
{
  delay(3000);
  Serial.begin( 115200 );
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(DOOR_PIN, INPUT_PULLUP);

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

  fingerInit();

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

    if(BUTTON_IS_PRESSED()){
      flag.pressed = true;
    }
    else {
      if(flag.pressed && !flag.sending){
        if((millis() - timeout.button) > 3000){
          flag.enroll = ~flag.enroll;
          if(flag.enroll){
            enroll_process = ENROLL_RESET;
            id.finger = 1;
            timeout.enroll = millis();
          }
        }
        else if((millis() - timeout.button) > 50){
          if(flag.enroll){
            if(enroll_process == ENROLL_RESET){
              id.finger++;
            }
          }
          else{

          }
        }
      }
      flag.pressed = false;
      timeout.button = millis();
    }

    if(!flag.sensor && (millis() - timeout.sensor) > 5000){
      timeout.sensor = millis();
      fingerInit();
    }

    if((millis() - timeout.timer) > 1000){
      if(xSemaphoreTake(p_mutex, pdMS_TO_TICKS(100))){
        unsigned long delta = millis() - timeout.info;
        if(delta > 9000){
          timeout.info = millis();
        }
        else if(delta > 6000){
          if(flag.sdcard) lv_label_set_text(objects.lb_total_offline, "SD Card OK");
          else lv_label_set_text(objects.lb_total_offline, "SD Card Fail");
        }
        else if(delta > 3000){
          if(flag.sensor) lv_label_set_text(objects.lb_total_offline, "Finger Sensor OK");
          else lv_label_set_text(objects.lb_total_offline, "Finger Sensor Fail");
        }
        else {
          lv_label_set_text_fmt(objects.lb_total_offline, "total offline");
        }

        id.dt = rtc.getTime();
        lv_label_set_text_fmt(objects.lb_time, "%02d:%02d:%02d", id.dt.time.hour, id.dt.time.minute, id.dt.time.second);
        lv_label_set_text_fmt(objects.lb_date, "%02d-%02d-20%02d", id.dt.date.date, id.dt.date.month, id.dt.date.year);

        if(flag.enroll){
          if((millis() - timeout.enroll) > 300000){
            flag.enroll = false;
          }

          lv_label_set_text(objects.lb_connection, "enroll");
          lv_label_set_text_fmt(objects.lb_state, "Finger ID : %d", id.finger);

          if(enroll_process == ENROLL_FINISH){
            lv_label_set_text(objects.lb_mark, "Berhasil Tersimpan");
          }
          else if(enroll_process == ENROLL_STORE){
            lv_label_set_text(objects.lb_mark, "Menyimpan...");
            if(setFingerprint() == FINGERPRINT_OK){
              enroll_process = ENROLL_FINISH;
              timeout.enroll = millis() - 297000;
            }
          }
          else if(enroll_process == ENROLL_FINGER2){
            lv_label_set_text(objects.lb_mark, "Tempel Jari yang sama");
            if(getFingerprint(2) == FINGERPRINT_OK){
              enroll_process = ENROLL_FINISH;
            }
          }
          else if(enroll_process == ENROLL_FINGER1){
            lv_label_set_text(objects.lb_mark, "Angkat Jari Anda !!!");
            if(waitRemoveFinger()){
              enroll_process = ENROLL_FINGER2;
            }
          }
          else{
            lv_label_set_text(objects.lb_mark, "Pilih Finger ID, lalu tempel Jari");
            if(getFingerprint(1) == FINGERPRINT_OK){
              enroll_process = ENROLL_FINGER1;
            }
          }
        }
        else{
          lv_label_set_text(objects.lb_state, "MASUK");

          if(flag.wifi) lv_label_set_text_fmt(objects.lb_connection, "%s", WiFi.localIP().toString());
          else lv_label_set_text_fmt(objects.lb_connection, "%s", ssid);

          if(flag.success){
            lv_label_set_text(objects.lb_mark, "Berhasil");
            lv_label_set_text(objects.lb_name, id.name);
            lv_label_set_text(objects.lb_report_time, id.dts);
            lv_obj_add_flag(objects.spn_load, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(objects.pl_success, LV_OBJ_FLAG_HIDDEN);

            if(!flag.card && !flag.store){
              flag.store = true;
              deleteFile(SD, id.dts);
            }

            flag.card = true;
          }
          else if(flag.fail){
            lv_label_set_text(objects.lb_mark, "Simpan");
            lv_label_set_text(objects.lb_name, "Memori");
            lv_label_set_text(objects.lb_report_time, id.dts);
            lv_obj_add_flag(objects.spn_load, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(objects.pl_success, LV_OBJ_FLAG_HIDDEN);

            if(!flag.store){
              flag.store = true;
              writeFile(SD, id.dts, id.uuid);
            }

            flag.card = true;
          }
          else if(flag.sending){
            lv_label_set_text(objects.lb_mark, "Mengirim !!!");
            lv_obj_remove_flag(objects.pl_modal, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(objects.spn_load, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.pl_success, LV_OBJ_FLAG_HIDDEN);
          }

          if(flag.wifi && !flag.card && ! flag.sending && (millis() - timeout.sdcard) > 60000){
            File root = SD.open("/absensi");
            if(!root) Serial.println("Failed to open directory");
            else if(!root.isDirectory()) Serial.println("Not a directory");
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

                  memset(id.dts, 0, sizeof(id.dts));
                  memset(id.uuid, 0, sizeof(id.uuid));

                  memcpy(id.dts, file.name(), 19);
                  id.dts[13] = ':';
                  id.dts[16] = ':';

                  int size = file.size();
                  int i=0;
                  while(file.available()){
                    if(i<size){
                      id.uuid[i] = file.read();
                      i++;
                    }
                  }

                  Serial.println(id.uuid);

                  flag.sending = true;
                  flag.store = false;

                  break;
                }
                file = root.openNextFile();

                yield();
              }
              file.close();
            }
            root.close();

            timeout.sdcard = millis();
          }
          else if(!flag.wifi) timeout.sdcard = millis();
        }

        xSemaphoreGive(p_mutex);
      }

      timeout.timer = millis();
    }

    if((millis() - timeout.scan) > 250){
      if(xSemaphoreTake(p_mutex, pdMS_TO_TICKS(100))){
        if(!flag.sending && !flag.enroll){
          if(!flag.card){
            byte card_uuid[5];
            unsigned char card_length = rf.loop(card_uuid);
            if(card_length){
              sprintf(id.uuid, "%02X%02X%02X%02X", card_uuid[0], card_uuid[1], card_uuid[2], card_uuid[3]);
              sprintf(id.dts, "20%02d-%02d-%02d %02d:%02d:%02d", id.dt.date.year, id.dt.date.month, id.dt.date.date, id.dt.time.hour, id.dt.time.minute, id.dt.time.second);
              timeout.rfid = millis();
              flag.card = true;
              timeout.card = millis();
              lv_label_set_text_fmt(objects.lb_scan, "%s", id.uuid);
              lv_label_set_text(objects.lb_mark, "Silahkan Tempel Sidik Jari !!!");
            }
          }
          else if((millis() - timeout.card) > 10000){
            flag.card = false;
            flag.success = false;
            flag.fail = false;
            flag.finger = false;
            lv_label_set_text_fmt(objects.lb_scan, "kosong");
            lv_label_set_text(objects.lb_mark, "Tempel Kartu Di Bawah Ini !!!");
            lv_obj_add_flag(objects.pl_modal, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.spn_load, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.pl_success, LV_OBJ_FLAG_HIDDEN);
          }
          else if(!flag.finger){
            id.finger = getFingerprintIDez();
            if(id.finger > 0){
              lv_label_set_text_fmt(objects.lb_scan, "%s-%d", id.uuid, id.finger);
              flag.finger = true;
              flag.sending = true;
              flag.store = false;
            }
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

        flag.wifi = true;
      }
      else{
        bool ready = false;
        String jsonStr;

        if(xSemaphoreTake(p_mutex, pdMS_TO_TICKS(100))){
          if(flag.sending){
            FirebaseJson payload;
            payload.add("uuid", id.uuid);
            payload.add("finger", id.finger);
            payload.add("dateAt", id.dts);
            payload.add("state", "masuk");

            payload.toString(jsonStr, true);
            ready = true;
          }
          xSemaphoreGive(p_mutex);
        }

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
            } 
            else {
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

        if(flag.sending){  
          flag.sending = false;
          flag.fail = true;
        }
        
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

