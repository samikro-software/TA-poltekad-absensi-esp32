/* ESP32 version 2.1.17  */

#include <WiFi.h>
#include <lvgl.h>         // version 9.4.0 by kisvegabor
#include <TFT_eSPI.h>     // version 2.5.43 by Bodmer
#include "ui.h"

const char* ssid     = "OFFICE RND";
const char* password = "seipandaan";

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
}TIMEOUT_TypeDef;
TIMEOUT_TypeDef timeout;

bool pg2 = false;
unsigned char modal = 0;

TaskHandle_t Task1;
TaskHandle_t Task2;

void setup()
{
  delay(1000);
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
  bool sdcard = SD.begin(SD_CS);

  if(!sdcard) {
    Serial.println("Error accessing microSD card!");
  }

  timeout.timer = millis();
  timeout.sdcard = millis();
  timeout.wifi = millis();

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

  // xTaskCreatePinnedToCore(
  //   Task2code,   /* Task function. */
  //   "Task2",     /* name of task. */
  //   4096,       /* Stack size of task */
  //   NULL,        /* parameter of the task */
  //   1,           /* priority of the task */
  //   &Task2,      /* Task handle to keep track of created task */
  //   1          /* pin task to core 0 */
  // );
}

void loop(){
  if(WiFi.status() == WL_CONNECTED){
    Serial.println("Connected");
  }
  else{
    if((millis() - timeout.wifi) > 60000){
      Serial.println("Connecting...");
      WiFi.disconnect();
      WiFi.mode(WIFI_STA);

      yield();
      WiFi.begin(ssid, password);
  
      timeout.wifi = millis(); //  reset wifi_timeout
      Serial.println("Done");
    }
  }
}

//Task1code: blinks an LED every 1000 ms
void Task1code( void * pvParameters ){
  for(;;){
    lv_timer_handler(); /* let the GUI do its work */
    ui_tick(); // Penting untuk EEZ Flow

    if((millis() - timeout.timer) > 1000){
      if(xSemaphoreTake(p_mutex, pdMS_TO_TICKS(100))){
        DATETIME_TypeDef dt = rtc.getTime();
        lv_label_set_text_fmt(objects.lb_time, "%02d:%02d:%02d", dt.time.hour, dt.time.minute, dt.time.second);
        lv_label_set_text_fmt(objects.lb_date, "%02d-%02d-20%02d", dt.date.date, dt.date.month, dt.date.year);

        xSemaphoreGive(p_mutex);
      }

      // switch(modal){
      //   case 0 : lv_screen_load_anim(objects.second_page, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, false); break;
      //   case 1 : lv_obj_remove_flag(objects.panel2, LV_OBJ_FLAG_HIDDEN); break;
      //   case 2 : lv_obj_remove_flag(objects.panel3, LV_OBJ_FLAG_HIDDEN); break;
      //   case 3 : lv_obj_remove_flag(objects.label3, LV_OBJ_FLAG_HIDDEN); break;
      //   default : 
      //     lv_obj_add_flag(objects.panel2, LV_OBJ_FLAG_HIDDEN); 
      //     lv_obj_add_flag(objects.panel3, LV_OBJ_FLAG_HIDDEN); 
      //     lv_obj_add_flag(objects.label3, LV_OBJ_FLAG_HIDDEN); 
      //     lv_screen_load(objects.home_page);
      //     rtc.init();
      //   break;
      // }
      // modal ++;
      // if(modal > 4) modal = 0;

      // File root = SD.open("/");
      // if(!root){
      //     Serial.println("Failed to open directory");
      //     // flag.sdcard = false;
      //     return;
      // }
      // if(!root.isDirectory()){
      //     Serial.println("Not a directory");
      //     return;
      // }

      // File file = root.openNextFile();
      // while(file){
      //     if(file.isDirectory()){
      //         Serial.print("  DIR : ");
      //         Serial.println(file.name());
      //     } else {
      //         Serial.print("  FILE: ");
      //         Serial.print(file.name());
      //         Serial.print("  SIZE: ");
      //         Serial.println(file.size());
      //     }
      //     file = root.openNextFile();

      //     yield();
      // }
      // root.close();
      // file.close();

      timeout.timer = millis();
    }

    byte card_uuid[10];
    unsigned char card_length = rf.loop(card_uuid);
    if(card_length){
      Serial.printf("%02X %02X %02X\r\n", card_uuid[0], card_uuid[1], card_uuid[2]);
    }

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

//Task2code: blinks an LED every 1000 ms
// void Task2code( void * pvParameters ){
//   for(;;){
//     if(WiFi.status() == WL_CONNECTED){
//       // Serial.println("Connected");
//     }
//     else{
//       if((millis() - timeout.wifi) > 60000){
//         Serial.println("Connecting...");
//         WiFi.disconnect();
//         WiFi.mode(WIFI_STA);

//         yield();
//         WiFi.begin(ssid, password);

//         timeout.wifi = millis(); //  reset wifi_timeout
//         Serial.println("Done");
//       }
//     }
//     vTaskDelay(pdMS_TO_TICKS(1000));
//   }
// }

