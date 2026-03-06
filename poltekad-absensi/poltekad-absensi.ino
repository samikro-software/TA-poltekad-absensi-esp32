/*Using LVGL with Arduino requires some extra steps:
 *Be sure to read the docs here: https://docs.lvgl.io/master/integration/framework/arduino.html  */

#include <lvgl.h>
#include "ui.h"
#include "actions.h"
#include <TFT_eSPI.h>
#include "rtc-ds3231.h"
RtcDs3231 rtc;

#include "rfid.h"
Rfid rf;

void action_change_screen(lv_event_t * e) {
    Serial.println("layar EEZ Studio telah ditekan!");
    
    // Contoh: Menyalakan LED fisik di ESP32
    // static bool status = false;
    // status = !status;
    // digitalWrite(2, status ? HIGH : LOW);
}

void action_bt_push(lv_event_t * e) {
    Serial.println("Tombol di layar EEZ Studio telah ditekan!");
    // PENTING: Cek apakah event-nya benar-benar "CLICKED"
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED) {
        Serial.println("Tombol benar-benar diklik!");
        // Taruh kode absensi/logika di sini
    }
    // Contoh: Menyalakan LED fisik di ESP32
    // static bool status = false;
    // status = !status;
    // digitalWrite(2, status ? HIGH : LOW);
}

/*To use the built-in examples and demos of LVGL uncomment the includes below respectively.
 *You also need to copy `lvgl/examples` to `lvgl/src/examples`. Similarly for the demos `lvgl/demos` to `lvgl/src/demos`.
 *Note that the `lv_examples` library is for LVGL v7 and you shouldn't install it for this version (since LVGL v8)
 *as the examples and demos are now part of the main LVGL library. */

//#include <examples/lv_examples.h>
//#include <demos/lv_demos.h>

/*Set to your screen resolution and rotation*/
#define TFT_HOR_RES   320
#define TFT_VER_RES   480
#define TFT_ROTATION  LV_DISPLAY_ROTATION_270

/*LVGL draw into this buffer, 1/10 screen size usually works well. The size is in bytes*/
#define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

TFT_eSPI tft = TFT_eSPI();

#if LV_USE_LOG != 0
void my_print( lv_log_level_t level, const char * buf )
{
    LV_UNUSED(level);
    Serial.println(buf);
    Serial.flush();
}
#endif

/* LVGL calls it when a rendered image needs to copied to the display*/
void my_disp_flush( lv_display_t *disp, const lv_area_t *area, uint8_t * px_map)
{
    /*Copy `px map` to the `area`*/

    /*For example ("my_..." functions needs to be implemented by you)
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);

    my_set_window(area->x1, area->y1, w, h);
    my_draw_bitmaps(px_map, w * h);
     */

    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);

    tft.dmaWait();
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)px_map, w * h, true);
    // tft.pushPixelsDMA((uint16_t *)px_map, w * h);
    // tft.pushImageDMA(area->x1, area->y1, w, h, (uint16_t *)px_map);
    tft.endWrite();

    /*Call it to tell LVGL you are ready*/
    lv_display_flush_ready(disp);
}

/*Read the touchpad*/
void my_touchpad_read( lv_indev_t * indev, lv_indev_data_t * data )
{
    /*For example  ("my_..." functions needs to be implemented by you)*/
    // uint16_t x, y;
    // bool touched = tft.getTouch( &x, &y );

    // if(!touched) {
    //     data->state = LV_INDEV_STATE_RELEASED;
    // } else {
    //     data->state = LV_INDEV_STATE_PRESSED;

    //     data->point.x = y;
    //     data->point.y = x;

    //     Serial.printf("Touch at: %d,%d\n", x, y);
    // }
}

/*use Arduinos millis() as tick source*/
static uint32_t my_tick(void)
{
    return millis();
}

unsigned long timeout;
bool pg2 = false;
unsigned char modal = 0;

TaskHandle_t Task1;
TaskHandle_t Task2;

void setup()
{
    String LVGL_Arduino = "Hello Arduino! ";
    LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

    Serial.begin( 115200 );
    Serial.println( LVGL_Arduino );

    // uint16_t calibrationData[] = { 265, 3556, 365, 3415, 3 }; //{345, 3424, 718, 2656, 1};  // { 265, 3556, 365, 3415, 3 };

    tft.begin();
    tft.setRotation(3); // Landscape
    // tft.initDMA(); // WAJIB AKTIF UNTUK DMA
    // tft.setTouch(calibrationData);

    lv_init();

    /*Set a tick source so that LVGL will know how much time elapsed. */
    lv_tick_set_cb(my_tick);

    /* register print function for debugging */
#if LV_USE_LOG != 0
    lv_log_register_print_cb( my_print );
#endif

    // /*TFT_eSPI can be enabled lv_conf.h to initialize the display in a simple way*/
    lv_display_t * disp = lv_tft_espi_create(TFT_HOR_RES, TFT_VER_RES, draw_buf, sizeof(draw_buf));

    // 3. Setup Display
    // lv_display_t * disp = lv_display_create(TFT_HOR_RES, TFT_VER_RES);
    // lv_display_set_flush_cb(disp, my_disp_flush);
    // lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    // // Alokasi memori di Internal RAM untuk kecepatan maksimal DMA
    // buf1 = (uint16_t *)heap_caps_malloc(BUF_SIZE * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    // buf2 = (uint16_t *)heap_caps_malloc(BUF_SIZE * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    // // Setup Display di LVGL 9
    // lv_display_t * disp = lv_display_create(TFT_HOR_RES, TFT_VER_RES);
    // lv_display_set_buffers(disp, buf1, buf2, BUF_SIZE * sizeof(uint16_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    // lv_display_set_flush_cb(disp, my_disp_flush);

    lv_display_set_rotation(disp, TFT_ROTATION);

    /*Initialize the (dummy) input device driver*/
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER); /*Touchpad should have POINTER type*/
    lv_indev_set_read_cb(indev, my_touchpad_read);

    /* Create a simple label
     * ---------------------
     lv_obj_t *label = lv_label_create( lv_screen_active() );
     lv_label_set_text( label, "Hello Arduino, I'm LVGL!" );
     lv_obj_align( label, LV_ALIGN_CENTER, 0, 0 );

     * Try an example. See all the examples
     *  - Online: https://docs.lvgl.io/master/examples.html
     *  - Source codes: https://github.com/lvgl/lvgl/tree/master/examples
     * ----------------------------------------------------------------

     lv_example_btn_1();

     * Or try out a demo. Don't forget to enable the demos in lv_conf.h. E.g. LV_USE_DEMO_WIDGETS
     * -------------------------------------------------------------------------------------------

     lv_demo_widgets();
     */

    // lv_obj_t *label = lv_label_create( lv_screen_active() );
    // lv_label_set_text( label, "Hello Arduino, I'm LVGL!" );
    // lv_obj_align( label, LV_ALIGN_CENTER, 0, 0 );

    ui_init();
    Serial.println( "Setup done" );

    rtc.init();
    rf.init();
    timeout = millis();

    //create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
    xTaskCreatePinnedToCore(
        Task1code,   /* Task function. */
        "Task1",     /* name of task. */
        10000,       /* Stack size of task */
        NULL,        /* parameter of the task */
        1,           /* priority of the task */
        &Task1,      /* Task handle to keep track of created task */
        0          /* pin task to core 0 */
    );

    xTaskCreatePinnedToCore(
        Task2code,   /* Task function. */
        "Task2",     /* name of task. */
        10000,       /* Stack size of task */
        NULL,        /* parameter of the task */
        1,           /* priority of the task */
        &Task2,      /* Task handle to keep track of created task */
        1          /* pin task to core 0 */
    );
}

void loop()
{
    // lv_timer_handler(); /* let the GUI do its work */
    // ui_tick(); // Penting untuk EEZ Flow
    // delay(10); /* let this time pass */
}

//Task1code: blinks an LED every 1000 ms
void Task1code( void * pvParameters ){
    Serial.print("Task1 running on core ");
    Serial.println(xPortGetCoreID());

    for(;;){
      lv_timer_handler(); /* let the GUI do its work */
      ui_tick(); // Penting untuk EEZ Flow

      if((millis() - timeout) > 5000){
        timeout = millis();

        switch(modal){
          case 0 : lv_screen_load_anim(objects.second_page, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, false); break;
          case 1 : lv_obj_remove_flag(objects.panel2, LV_OBJ_FLAG_HIDDEN); break;
          case 2 : lv_obj_remove_flag(objects.panel3, LV_OBJ_FLAG_HIDDEN); break;
          case 3 : lv_obj_remove_flag(objects.label3, LV_OBJ_FLAG_HIDDEN); break;
          default : 
            lv_obj_add_flag(objects.panel2, LV_OBJ_FLAG_HIDDEN); 
            lv_obj_add_flag(objects.panel3, LV_OBJ_FLAG_HIDDEN); 
            lv_obj_add_flag(objects.label3, LV_OBJ_FLAG_HIDDEN); 
            lv_screen_load(objects.home_page);
          break;
        }
        modal ++;
        if(modal > 4) modal = 0;
      }

      byte card_uuid[10];
      unsigned char card_length = rf.loop(card_uuid);
      if(card_length){
        Serial.printf("%02X %02X %02X\r\n", card_uuid[0], card_uuid[1], card_uuid[2]);
      }
      vTaskDelay(5);
    }
}

//Task2code: blinks an LED every 1000 ms
void Task2code( void * pvParameters ){
    Serial.print("Task2 running on core ");
    Serial.println(xPortGetCoreID());

    for(;;){
      DATETIME_TypeDef dt = rtc.getTime();
      // Serial.printf("%02d:%02d:%02d\r\n", dt.time.hour, dt.time.minute, dt.time.second);

      vTaskDelay(1000);
    }
}