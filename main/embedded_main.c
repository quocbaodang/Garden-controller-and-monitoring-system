//Include
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "string.h"

#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "freertos/FreeRTOS.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "driver/gpio.h"
#include "driver/i2c.h"

#include "sdkconfig.h"
#include "esp32/rom/uart.h"
#include "freertos/event_groups.h"
#include "smbus.h"
#include "i2c_lcd1602.h"
#include "keyboard.h"
#include "dht11.h"

//Define 
static const char *TAG = "DO_AN_CN";
#define BOM_NUM GPIO_NUM_33
#define QUAT_NUM GPIO_NUM_25
#define PS_NUM GPIO_NUM_26
#define LIGHT_SENSOR GPIO_NUM_35
#define LED_NUM GPIO_NUM_27

#define BAT_QUAT gpio_set_level(QUAT_NUM, 0)
#define TAT_QUAT gpio_set_level(QUAT_NUM, 1)
#define BAT_BOM gpio_set_level(BOM_NUM, 0)
#define TAT_BOM gpio_set_level(BOM_NUM, 1)
#define BAT_PS gpio_set_level(PS_NUM, 1)
#define TAT_PS gpio_set_level(PS_NUM, 0)
#define BAT_LED gpio_set_level(LED_NUM, 1)
#define TAT_LED gpio_set_level(LED_NUM, 0)

#define MANUAL_MODE 0
#define AUTO_MODE 1
#define CONFIG_MODE 2
#define ASSIST_MODE 3

#define LCD_NUM_ROWS 4
#define LCD_NUM_COLUMNS 40
#define LCD_NUM_VISIBLE_COLUMNS 20
#define USE_STDIN 1
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_TX_BUF_LEN 0 // disabled
#define I2C_MASTER_RX_BUF_LEN 0 // disabled
#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_SDA_IO 21 // 19
#define I2C_MASTER_SCL_IO 22 // 18

//=============================================================================

//Client MQTT
static esp_mqtt_client_handle_t client;

//adc
static esp_adc_cal_characteristics_t adc1_chars;

void manual_mode(i2c_lcd1602_info_t *infolcd); //tu dong
void Auto_mode(i2c_lcd1602_info_t *infolcd); //bang tay
void config(i2c_lcd1602_info_t *infolcd); // cai dat
void Assist(i2c_lcd1602_info_t *infolcd); // huong dan

QueueHandle_t xQueue_dht = NULL;
QueueHandle_t xQueue_soil = NULL;
/*=======================================================================*/
static int u8Count = 0;
static char key;

uint8_t mode;
uint8_t bom = 0, quat = 0, ps =0;

static struct dht11_reading dht11_last_data, dht11_cur_data;
static struct soil_reading soil_last_data, soil_cur_data;
static struct soil_reading {
    uint32_t per_soil;
};

typedef void (*gpio_check_logic_t)(void);
typedef void (*CHECK_MODE_t)(i2c_lcd1602_info_t *);
typedef void (*Interface_t)(i2c_lcd1602_info_t *);

Interface_t Interface;
const CHECK_MODE_t CHECK_MODE[4] = {
    manual_mode,
    Auto_mode,
    config,
    Assist,};

static int temp_config = 50, humi_config = 50, soil_config = 50;
static char temp[10], humi[5], soil[10];
static char temp_config_t[10], humi_config_t[10], soil_config_t[10];

const gpio_check_logic_t keypad_row_en[4] = {
    keypad_row1_en,
    keypad_row2_en,
    keypad_row3_en,
    keypad_row4_en};

//==========================================================================================//

//Ham chuyen doi gia tri 0-3124mV -> 0-100%
long map(long x, long in_min, long in_max, long out_min, long out_max)
{
return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

//Khoi tao i2c
static void i2c_master_init(void)
{
   int i2c_master_port = I2C_MASTER_NUM;
   i2c_config_t conf;
   conf.mode = I2C_MODE_MASTER;
   conf.sda_io_num = I2C_MASTER_SDA_IO;
   conf.sda_pullup_en = GPIO_PULLUP_DISABLE; // GY-2561 provides 10kΩ pullups
   conf.scl_io_num = I2C_MASTER_SCL_IO;
   conf.scl_pullup_en = GPIO_PULLUP_DISABLE; // GY-2561 provides 10kΩ pullups
   conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
   i2c_param_config(i2c_master_port, &conf);
   i2c_driver_install(i2c_master_port, conf.mode,
                      I2C_MASTER_RX_BUF_LEN,
                      I2C_MASTER_TX_BUF_LEN, 0);
}

//Chế độ bằng tay
void manual_mode(i2c_lcd1602_info_t *infolcd)
{
   esp_mqtt_client_publish(client, "auto/state","0", 0, 0, 0);

   struct dht11_reading *newcmd;
   struct soil_reading *newcmd1;

   i2c_lcd1602_clear(infolcd);
   i2c_lcd1602_move_cursor(infolcd, 0, 0);
   i2c_lcd1602_write_string(infolcd, "[B]->AUTO/[C]CONFIG");

   i2c_lcd1602_move_cursor(infolcd, 0, 1);
   i2c_lcd1602_write_string(infolcd, "TEMP:");

   i2c_lcd1602_move_cursor(infolcd, 0, 2);
   i2c_lcd1602_write_string(infolcd, "HUMID:");

   i2c_lcd1602_move_cursor(infolcd, 0, 3);
   i2c_lcd1602_write_string(infolcd, "SOIL:");

   i2c_lcd1602_move_cursor(infolcd, 12, 1);
   i2c_lcd1602_write_string(infolcd, "[1]");
   i2c_lcd1602_move_cursor(infolcd, 16, 1);
   i2c_lcd1602_write_string(infolcd, "[2]");
   i2c_lcd1602_move_cursor(infolcd, 12, 2);
   i2c_lcd1602_write_string(infolcd, "[4]");
   i2c_lcd1602_move_cursor(infolcd, 16, 2);
   i2c_lcd1602_write_string(infolcd, "[5]");
   i2c_lcd1602_move_cursor(infolcd, 12, 3);
   i2c_lcd1602_write_string(infolcd, "[7]");
   i2c_lcd1602_move_cursor(infolcd, 16, 3);
   i2c_lcd1602_write_string(infolcd, "[8]");

   while (1)
   {

      xQueueReceive(xQueue_dht, (void *)&newcmd, (TickType_t)0);
      xQueueReceive(xQueue_soil, (void *)&newcmd1, (TickType_t)0);

      sprintf(temp, "%.1f", newcmd->temperature);
      sprintf(humi, "%.1f%%", newcmd->humidity);
      sprintf(soil, "%d%%", newcmd1->per_soil);

      esp_mqtt_client_publish(client, "temperature/state", temp, 0, 0, 0);
      esp_mqtt_client_publish(client, "humidity/state", humi, 0, 0, 0);
      esp_mqtt_client_publish(client, "soil/state", soil, 0, 0, 0);

      i2c_lcd1602_move_cursor(infolcd, 6, 1);
      i2c_lcd1602_write_string(infolcd, temp);

      i2c_lcd1602_move_cursor(infolcd, 6, 2);
      i2c_lcd1602_write_string(infolcd, humi);

      i2c_lcd1602_move_cursor(infolcd, 6, 3);
      i2c_lcd1602_write_string(infolcd, soil);
      keypad_row_en[u8Count]();
      key = waitKey(u8Count);

      //==================DIEU KHIEN====================
      if (key == '1' && quat == 0)
      {
         quat = 1;
         printf("quat bat\n");
         BAT_QUAT;
         esp_mqtt_client_publish(client, "fan/state","1", 0, 0, 0);
      }
      if (key == '2' && quat == 1)
      {
         quat = 0;
         printf("quat tat\n");
         TAT_QUAT;
         esp_mqtt_client_publish(client, "fan/state","0", 0, 0, 0);
      }

      if (key == '4' && ps == 0)
      {
         ps = 1;
         printf("phun suong bat\n");
         BAT_PS;
         esp_mqtt_client_publish(client, "mist/state","1", 0, 0, 0);
      }
      if (key == '5' && ps == 1)
      {
         ps = 0;
         printf("phun suong tat\n");
         TAT_PS;
         esp_mqtt_client_publish(client, "mist/state","0", 0, 0, 0);
      }
      
      if (key == '7' && bom == 0)
      {
         bom = 1;
         printf("bom bat\n");
         BAT_BOM;
         esp_mqtt_client_publish(client, "water/state","1", 0, 0, 0);
      }
      if (key == '8' && bom == 1)
      {
         bom = 0;
         printf("bom tat\n");
         TAT_BOM;
         esp_mqtt_client_publish(client, "water/state","0", 0, 0, 0);
      }

      if (key == '*')
      {
         printf("den bat\n");
         BAT_LED;
         esp_mqtt_client_publish(client, "lightt/state","1", 0, 0, 0);
      }
      if (key == '0')
      {
         printf("Den tat\n");
         TAT_LED;
         esp_mqtt_client_publish(client, "lightt/state","0", 0, 0, 0);
      }

      //========================================================
      if (key == 'C')
         mode = CONFIG_MODE;
      if (key == 'B')
      {
         mode = AUTO_MODE;
         esp_mqtt_client_publish(client, "auto/state","1", 0, 0, 0);
      }
      if (key == 'D')
         mode = ASSIST_MODE;
      if (mode == CONFIG_MODE || mode == AUTO_MODE || mode == ASSIST_MODE)
      {
         break;
      }

      if (++u8Count == 4)
      {
         u8Count = 0;
      }
      vTaskDelay(20 / portTICK_PERIOD_MS);
   }
}

// Cài đặt
void config(i2c_lcd1602_info_t *infolcd)
{
   uint8_t lc = 0;

   i2c_lcd1602_clear(infolcd);
   i2c_lcd1602_move_cursor(infolcd, 0, 0);
   i2c_lcd1602_write_string(infolcd, "****** CONFIG ******");
   i2c_lcd1602_move_cursor(infolcd, 0, 1);
   i2c_lcd1602_write_string(infolcd, "Nhiet Do:");
   i2c_lcd1602_move_cursor(infolcd, 0, 2);
   i2c_lcd1602_write_string(infolcd, "Do am kk:");
   i2c_lcd1602_move_cursor(infolcd, 0, 3);
   i2c_lcd1602_write_string(infolcd, "Do am dat:");

   while (1)
   {
      sprintf(temp_config_t, "%d", temp_config);
      sprintf(humi_config_t, "%d", humi_config);
      sprintf(soil_config_t, "%d", soil_config);

      i2c_lcd1602_move_cursor(infolcd, 11, 1);
      i2c_lcd1602_write_string(infolcd, temp_config_t);
      i2c_lcd1602_move_cursor(infolcd, 11, 2);
      i2c_lcd1602_write_string(infolcd, humi_config_t);
      i2c_lcd1602_move_cursor(infolcd, 11, 3);
      i2c_lcd1602_write_string(infolcd, soil_config_t);

      keypad_row_en[u8Count]();
      key = waitKey(u8Count);

      //=========================================================================
      if (lc == 0)
      {
        
         if (key == '#')
         {
            temp_config++;
            if(temp_config > 100)
               temp_config = 100;
         }
         else if (key == '*')
         {
            temp_config--;
            if(temp_config < 0) 
               temp_config = 0;
         }

         sprintf(temp_config_t, "%d", temp_config);
         esp_mqtt_client_publish(client, "temp/state", temp_config_t, 0, 0, 0);

         i2c_lcd1602_move_cursor(infolcd, 11, 1);
         i2c_lcd1602_write_string(infolcd, temp_config_t);
         i2c_lcd1602_write_string(infolcd, "   ");

         if (key == 'D')
         {
            lc = 1;   
         }
      }

      //============================================================================
      else if (lc == 1)
      {
        
         if (key == '#')
         {
            humi_config++;
            if(humi_config>100)
               humi_config = 100;
         }
         else if (key == '*')
         {
            humi_config--;
            if (humi_config < 0)
               humi_config = 0;
         }
         
         sprintf(humi_config_t, "%d", humi_config);
         esp_mqtt_client_publish(client, "humi/state",humi_config_t, 0, 0, 0);
        
         i2c_lcd1602_move_cursor(infolcd, 11, 2);
         i2c_lcd1602_write_string(infolcd, humi_config_t);
         i2c_lcd1602_write_string(infolcd, "   ");

         if (key == 'D')
         {
            lc = 2;
         }
      }  

      //=============================================================================
      else if (lc == 2)
      {
         if (key == '#')
         {
            soil_config++;
            if (soil_config > 100)
               soil_config = 100;
         }
         else if (key == '*')
         {
            soil_config--;
            if (soil_config < 0)
               soil_config = 0;
         }

         sprintf(soil_config_t, "%d", soil_config);
         esp_mqtt_client_publish(client, "s/state",soil_config_t, 0, 0, 0);

         i2c_lcd1602_move_cursor(infolcd, 11, 3);
         i2c_lcd1602_write_string(infolcd, soil_config_t);
         i2c_lcd1602_write_string(infolcd, "   ");

         if (key == 'D')
         {  
            esp_mqtt_client_publish(client, "auto/state","1", 0, 0, 0);

            mode = AUTO_MODE;
            break;
         }
      }

      //===========================chuyen mode =============================
      if (key == 'A')
      {
         mode = MANUAL_MODE;
         esp_mqtt_client_publish(client, "auto/state","0", 0, 0, 0);
      }

      if (key == 'B')
      {
         mode = AUTO_MODE;
         esp_mqtt_client_publish(client, "auto/state","1", 0, 0, 0);
      }

      if (mode == MANUAL_MODE || mode == AUTO_MODE)
      {        
         break;
      }
      if (++u8Count == 4)
      {
         u8Count = 0;
      }
      vTaskDelay(20 / portTICK_PERIOD_MS);
   }
}

//Chế độ tự động
void Auto_mode(i2c_lcd1602_info_t *infolcd)
{
   esp_mqtt_client_publish(client, "auto/state","1", 0, 0, 0);

   struct dht11_reading *newcmd;
   struct soil_reading *newcmd1;

   i2c_lcd1602_clear(infolcd);
   i2c_lcd1602_move_cursor(infolcd, 0, 1);
   i2c_lcd1602_write_string(infolcd, "Nhiet Do:");
   i2c_lcd1602_move_cursor(infolcd, 0, 2);
   i2c_lcd1602_write_string(infolcd, "Do am kk:");
   i2c_lcd1602_move_cursor(infolcd, 0, 3);
   i2c_lcd1602_write_string(infolcd, "Do am dat:");
   i2c_lcd1602_move_cursor(infolcd, 0, 0);
   i2c_lcd1602_write_string(infolcd, "[A]-MANUAL/[C]CONFIG");

   while (1)
   {
      xQueueReceive(xQueue_dht, (void *)&newcmd, (TickType_t)0);
      xQueueReceive(xQueue_soil, (void *)&newcmd1, (TickType_t)0);

      sprintf(temp, "%.1f", newcmd->temperature);
      sprintf(humi, "%.1f%%", newcmd->humidity);
      sprintf(soil, "%d%%", newcmd1->per_soil);

      esp_mqtt_client_publish(client, "temperature/state", temp, 0, 1, 0);
      esp_mqtt_client_publish(client, "humidity/state", humi, 0, 1, 0);
      esp_mqtt_client_publish(client, "soil/state", soil, 0, 1, 0);

      i2c_lcd1602_move_cursor(infolcd, 11, 1);
      i2c_lcd1602_write_string(infolcd, temp);
      i2c_lcd1602_move_cursor(infolcd, 11, 2);
      i2c_lcd1602_write_string(infolcd, humi);
      i2c_lcd1602_move_cursor(infolcd, 11, 3);
      i2c_lcd1602_write_string(infolcd, soil);

      keypad_row_en[u8Count]();
      key = waitKey(u8Count);

      //Bat quat khi nhiet do cao
      if ((int) newcmd->temperature > temp_config && quat == 0)
      {
         quat = 1;
         BAT_QUAT;
         printf("bat quat\n");
         esp_mqtt_client_publish(client, "fan/state","1", 0, 0, 0);      
      }

      //Tat quat khi nhiet do thap
      if ((int)newcmd->temperature < temp_config && quat == 1)
      {
         quat = 0;
         TAT_QUAT;
         printf("tat quat\n");
         esp_mqtt_client_publish(client, "fan/state","0", 0, 0, 0);
      }

      //Bat phun suong khi do am kk thap
      if ((int)newcmd->humidity < humi_config && ps == 0)
      {
         ps = 1;
         BAT_PS;
         printf("bat phun suong\n");
         esp_mqtt_client_publish(client, "mist/state","1", 0, 0, 0);
      }

      //Tat phun suong khi do am kk cao
      else if ((int)newcmd->humidity > humi_config && ps == 1)
      {
         ps = 0;
         TAT_PS;
         printf("tat phun suong\n");
         esp_mqtt_client_publish(client, "mist/state","0", 0, 0, 0);
      }

      //Bat bom khi do am dat thap
      if ((int)newcmd1->per_soil < soil_config && bom == 0)
      {
         bom = 1;
         BAT_BOM;
         printf("bat bom\n");
         esp_mqtt_client_publish(client, "water/state","1", 0, 0, 0);
      }

      //Tat bom khi do am dat cao
      else if ((int)newcmd1->per_soil > soil_config && bom == 1)
      {
         bom = 0;
         TAT_BOM;
         printf("tat bom\n");
         esp_mqtt_client_publish(client, "water/state","0", 0, 0, 0);
      }

      if (gpio_get_level(LIGHT_SENSOR) == 1)
      {   
         BAT_LED;
         printf("Bat den\n");
         esp_mqtt_client_publish(client, "lightt/state","1", 0, 0, 0);
      }        
      else if (gpio_get_level(LIGHT_SENSOR) == 0)
      {
         TAT_LED;
         printf("Tat den\n");
         esp_mqtt_client_publish(client, "lightt/state","0", 0, 0, 0);      
      }

      // =======================chuyen mode============================================
      if (key == 'A')
      {
         mode = MANUAL_MODE;
         esp_mqtt_client_publish(client, "auto/state","0", 0, 0, 0);
      }
      if (key == 'C')
         mode = CONFIG_MODE;
      if (key == 'D')
         mode = ASSIST_MODE;
      if (mode == MANUAL_MODE || mode == CONFIG_MODE || mode == ASSIST_MODE)
      {
         break;
      }
      if (++u8Count == 4)
      {
         u8Count = 0;
      }
      vTaskDelay(20 / portTICK_PERIOD_MS);
   }
}

//Huong dan
void Assist(i2c_lcd1602_info_t *infolcd)
{ 
   i2c_lcd1602_clear(infolcd);
   i2c_lcd1602_move_cursor(infolcd, 0, 1);
   i2c_lcd1602_write_string(infolcd, "[A]CHE DO DIEU KHIEN");
   i2c_lcd1602_move_cursor(infolcd, 0, 2);
   i2c_lcd1602_write_string(infolcd, "[B]CHE DO TU DONG");
   i2c_lcd1602_move_cursor(infolcd, 0, 3);
   i2c_lcd1602_write_string(infolcd, "[C]CAI DAT NGUONG");
   i2c_lcd1602_move_cursor(infolcd, 0, 0);
   i2c_lcd1602_write_string(infolcd, "****** ASSIST ******");

   vTaskDelay(3000 / portTICK_PERIOD_MS);
   i2c_lcd1602_clear(infolcd);
   i2c_lcd1602_move_cursor(infolcd, 0, 0);
   i2c_lcd1602_write_string(infolcd, "[1] [2] BAT TAT QUAT");
   i2c_lcd1602_move_cursor(infolcd, 0, 1);
   i2c_lcd1602_write_string(infolcd, "[4] [5] BAT TAT PS");
   i2c_lcd1602_move_cursor(infolcd, 0, 2);
   i2c_lcd1602_write_string(infolcd, "[7] [8] BAT TAT BOM");
   i2c_lcd1602_move_cursor(infolcd, 0, 3);
   i2c_lcd1602_write_string(infolcd, "[*] [0] BAT TAT DEN");


   while (1)
   { 
      keypad_row_en[u8Count]();
      key = waitKey(u8Count);

   
// ===============================chuyen mode============================================
      if (key == 'A')
      {
         mode = MANUAL_MODE;
         esp_mqtt_client_publish(client, "auto/state","0", 0, 0, 0);
      }
      if (key == 'B')
      {
         mode = AUTO_MODE;
         esp_mqtt_client_publish(client, "auto/state","1", 0, 0, 0);
      }
      if (key == 'C')
         mode = CONFIG_MODE;
      if (mode == MANUAL_MODE || mode == CONFIG_MODE || mode == AUTO_MODE)
      {
         break;
      }
      if (++u8Count == 4)
      {
         u8Count = 0;
      }
      vTaskDelay(20 / portTICK_PERIOD_MS);
   }
   
}

//Task LCD
void lcd1602_task(void *pvParameter)
{
   // Set up I2C
   i2c_master_init();
   i2c_port_t i2c_num = I2C_MASTER_NUM;
   uint8_t address = 0x27;
   printf("chao\n");
   // Set up the SMBus
   smbus_info_t *smbus_info = smbus_malloc();
   ESP_ERROR_CHECK(smbus_init(smbus_info, i2c_num, address));
   ESP_ERROR_CHECK(smbus_set_timeout(smbus_info, 1000 / portTICK_RATE_MS));

   // Set up the LCD1602 device with backlight off
   i2c_lcd1602_info_t *lcd_info = i2c_lcd1602_malloc();
   ESP_ERROR_CHECK(i2c_lcd1602_init(lcd_info, smbus_info, true,
                                    LCD_NUM_ROWS, LCD_NUM_COLUMNS, LCD_NUM_VISIBLE_COLUMNS));

   ESP_ERROR_CHECK(i2c_lcd1602_reset(lcd_info));

   // turn off backlight
   ESP_LOGI(TAG, "backlight off");
   //_wait_for_user();
   i2c_lcd1602_set_backlight(lcd_info, false);

   // turn on backlight
   ESP_LOGI(TAG, "backlight on");
   i2c_lcd1602_set_backlight(lcd_info, true);
   ESP_LOGI(TAG, "cursor on");
   i2c_lcd1602_set_cursor(lcd_info, false);
   while (1)
   {
      Interface = CHECK_MODE[mode];
      Interface(lcd_info);
   }
}

//Task DHT11
void Dht11_task(void *pvParameter)
{
   struct dht11_reading *newcmd;
   newcmd = &dht11_last_data;
   while (1)
   {
      dht11_cur_data = DHT11_read();
      xQueueSend(xQueue_dht, &newcmd, (TickType_t)0);
      if (dht11_cur_data.status == 0) // read oke
      {
         dht11_last_data = dht11_cur_data;
         printf(": %.1f   %.1f\n", newcmd->humidity, newcmd->temperature);
      }
      else
      {
         printf("dht11 read fail\n");
      }
      vTaskDelay(1000 / portTICK_PERIOD_MS);
   }
}

//do am dat task
void soil_task(void *pvParameter)
{
   struct soil_reading *newcmd;
   newcmd = &soil_last_data;
 	uint32_t voltage;

    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 0, &adc1_chars);

    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_12));
    ESP_ERROR_CHECK(adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11)); // channel 6 is GPIO 34

    while (1) 
    {
      voltage = esp_adc_cal_raw_to_voltage(adc1_get_raw(ADC1_CHANNEL_6), &adc1_chars);
      //ESP_LOGI(TAG, "ADC1_CHANNEL_6: %d mV", voltage);
      soil_cur_data.per_soil = 100 - map(voltage, 0, 3124, 0, 100);
      soil_last_data = soil_cur_data;
      xQueueSend(xQueue_soil, &newcmd, (TickType_t)0);
      printf(": %d\n",newcmd->per_soil);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

//==================================================================================================
//MQTT
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
   esp_mqtt_client_handle_t client = event->client;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            esp_mqtt_client_subscribe(client, "fan/command", 1u);
            esp_mqtt_client_subscribe(client, "water/command", 1u);
            esp_mqtt_client_subscribe(client, "mist/command", 1u);
            esp_mqtt_client_subscribe(client, "auto/command", 1u);
            esp_mqtt_client_subscribe(client, "lightt/command", 1u);
            esp_mqtt_client_subscribe(client, "temp/command", 1u);
            esp_mqtt_client_subscribe(client, "humi/command", 1u);
            esp_mqtt_client_subscribe(client, "s/command", 1u);

            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
           // ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);

            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
           // ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);

            //Quat
            //So sanh 2 chuoi dau voi do dai topic_len
            if(strncmp(event->topic, "fan/command", event->topic_len)==0)
            {
               if (mode == MANUAL_MODE)
               {
                     if(strncmp(event->data, "0", event->data_len) == 0 && quat == 1)
                     {
                        quat = 0;
                        ESP_LOGI("Data_mqtt", "tat quoat");
                        TAT_QUAT;
                     }
                     if(strncmp(event->data, "1", event->data_len) == 0 && quat == 0)
                     {   
                        quat = 1;
                        ESP_LOGI("Data_mqtt", "bat quoat");
                         BAT_QUAT;
                     }
               }
            }

            //Bom
             //So sanh 2 chuoi dau voi do dai topic_len
            if(strncmp(event->topic, "water/command", event->topic_len)==0)
            {
               if(mode == MANUAL_MODE)
               {
                   if(strncmp(event->data, "0", event->data_len) == 0 && bom == 1)
                   {
                     bom = 0;
                      ESP_LOGI("Data_mqtt", "tat bom");
                      TAT_BOM;
                   }
                     if(strncmp(event->data, "1", event->data_len) == 0 && bom == 0)
                   {
                     bom = 1;
                      ESP_LOGI("Data_mqtt", "bat bom");
                      BAT_BOM;
                   }
               }

            }

            //Phun suong
            //So sanh 2 chuoi dau voi do dai topic_len
            if(strncmp(event->topic, "mist/command", event->topic_len)==0)
            {
               if(mode == MANUAL_MODE)
               {
                   if(strncmp(event->data, "0", event->data_len) == 0 && ps == 1)
                   {
                     ps = 0;
                      ESP_LOGI("Data_mqtt", "tat phun suong");
                      TAT_PS;
                   }
                     if(strncmp(event->data, "1", event->data_len) == 0 && ps == 0)
                   {
                     ps = 1;
                      ESP_LOGI("Data_mqtt", "bat phun suong");
                      BAT_PS;
                   }
               }

            }

            //chuyen che do
             //So sanh 2 chuoi dau voi do dai topic_len
            if(strncmp(event->topic, "auto/command", event->topic_len)==0)
            {
               if(strncmp(event->data, "0", event->data_len) == 0)
               {
                  ESP_LOGI("Data_mqtt", "Che do dieu khien bang tay");
                  mode = MANUAL_MODE;
               }
                if(strncmp(event->data, "1", event->data_len) == 0)
               {
                  ESP_LOGI("Data_mqtt", "che do tu dong");
                  mode = AUTO_MODE;
               }
            }

            //Den
             //So sanh 2 chuoi dau voi do dai topic_len
            if(strncmp(event->topic, "lightt/command", event->topic_len)==0)
            {
               if (mode == MANUAL_MODE)
               {
                     if(strncmp(event->data, "0", event->data_len) == 0)
                     {
                         ESP_LOGI("Data_mqtt", "tat den");
                         TAT_LED;
                     }
                     if(strncmp(event->data, "1", event->data_len) == 0)
                     {   
                        ESP_LOGI("Data_mqtt", "bat den");
                         BAT_LED;
                     }
               }
            }
            
             //Temp config 
             //So sanh 2 chuoi dau voi do dai topic_len
            if(strncmp(event->topic, "temp/command", event->topic_len) == 0)
            {
               temp_config = atoi(event->data);
            }
            //Humi config 
             //So sanh 2 chuoi dau voi do dai topic_len
            if(strncmp(event->topic, "humi/command", event->topic_len) == 0)
            {
               humi_config = atoi(event->data);
            }
            //soil config 
            //So sanh 2 chuoi dau voi do dai topic_len
            if(strncmp(event->topic, "s/command", event->topic_len) == 0)
            {
               soil_config = atoi(event->data);
            }

            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;

        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
      
     // .uri = "mqtt://broker.hivemq.com:1883",
      .uri = "mqtt://test.mosquitto.org:1883",
     // .password = "123456789bao",
      //.username = "GDB",
     
      //  .keepalive =60,
      .event_handle = mqtt_event_handler,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}


//Hàm main
void app_main()
{

   ESP_ERROR_CHECK(nvs_flash_init());
   ESP_ERROR_CHECK(esp_netif_init());
   ESP_ERROR_CHECK(esp_event_loop_create_default());
   ESP_ERROR_CHECK(example_connect());
    
   mqtt_app_start();

   gpio_pad_select_gpio(GPIO_NUM_25); // bom
   gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);
   gpio_pad_select_gpio(GPIO_NUM_33); // quat
   gpio_set_direction(GPIO_NUM_33, GPIO_MODE_OUTPUT);
   gpio_pad_select_gpio(GPIO_NUM_26); // phun suong
   gpio_set_direction(GPIO_NUM_26, GPIO_MODE_OUTPUT);
   gpio_pad_select_gpio(GPIO_NUM_27); // den
   gpio_set_direction(GPIO_NUM_27, GPIO_MODE_OUTPUT);
   gpio_set_direction(LIGHT_SENSOR, GPIO_MODE_INPUT);

   TAT_BOM;
   TAT_QUAT;
   TAT_PS;
   bom = quat = ps = 0;

   esp_mqtt_client_publish(client, "water/state","0", 0, 0, 0);
   esp_mqtt_client_publish(client, "fan/state","0", 0, 0, 0);
   esp_mqtt_client_publish(client, "mist/state","0", 0, 0, 0);
   
   DHT11_init(GPIO_NUM_19);
   KeyBoard_Init();

   xQueue_dht = xQueueCreate(1, sizeof(struct dht11_reading *));
   xQueue_soil = xQueueCreate(1, sizeof(struct soil_reading *));

   if (xQueue_soil != NULL)
   {
      xTaskCreate(&soil_task, "soil_task", 4096, NULL, 5, NULL);
   }

   if (xQueue_dht != NULL)
   {
      xTaskCreate(&lcd1602_task, "lcd1602_task", 4096, NULL, 5, NULL);
      xTaskCreate(&Dht11_task, "dht11", 4096, NULL, 6, NULL);
   }
}
