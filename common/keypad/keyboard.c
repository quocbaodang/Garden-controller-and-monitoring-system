
#include "keyboard.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
char checkcolum1( void );
char checkcolum2( void );
char checkcolum3( void );
char checkcolum4( void );

typedef char (*check_keypad_t)(void);

check_keypad_t check_keypad[4] = {
                                                checkcolum1,
                                                checkcolum2,
                                                checkcolum3,
                                                checkcolum4
                                            };

 

void keypad_row1_en(void)
{
   gpio_set_level(R1, 0);
   gpio_set_level(R2, 1);
   gpio_set_level(R3, 1);
   gpio_set_level(R4, 1);
}
void keypad_row2_en(void)
{
   gpio_set_level(R1, 1);
   gpio_set_level(R2, 0);
   gpio_set_level(R3, 1);
   gpio_set_level(R4, 1);
}
void keypad_row3_en(void)
{
   gpio_set_level(R1, 1);
   gpio_set_level(R2, 1);
   gpio_set_level(R3, 0);
   gpio_set_level(R4, 1);
}
void keypad_row4_en(void)
{
   gpio_set_level(R1, 1);
   gpio_set_level(R2, 1);
   gpio_set_level(R3, 1);
   gpio_set_level(R4, 0);
}

void KeyBoard_Init()
{
   // output_io_create(R1);
   gpio_pad_select_gpio(R1);
   gpio_set_direction(R1, GPIO_MODE_OUTPUT);
   // output_io_create(R2);
   gpio_pad_select_gpio(R2);
   gpio_set_direction(R2, GPIO_MODE_OUTPUT);
   // output_io_create(R3);
   gpio_pad_select_gpio(R3);
   gpio_set_direction(R3, GPIO_MODE_OUTPUT);
   // output_io_create(R4);
   gpio_pad_select_gpio(R4);
   gpio_set_direction(R4, GPIO_MODE_OUTPUT);

   gpio_set_level(R1, 1);
   gpio_set_level(R2, 1);
   gpio_set_level(R3, 1);
   gpio_set_level(R4, 1);
   //
   gpio_pad_select_gpio(C1);
   gpio_set_direction(C1, GPIO_MODE_INPUT);
   gpio_set_pull_mode(C1, GPIO_PULLUP_ONLY);
   //
   gpio_pad_select_gpio(C2);
   gpio_set_direction(C2, GPIO_MODE_INPUT);
   gpio_set_pull_mode(C2, GPIO_PULLUP_ONLY);
   //
   gpio_pad_select_gpio(C3);
   gpio_set_direction(C3, GPIO_MODE_INPUT);
   gpio_set_pull_mode(C3, GPIO_PULLUP_ONLY);
   //
   gpio_pad_select_gpio(C4);
   gpio_set_direction(C4, GPIO_MODE_INPUT);
   gpio_set_pull_mode(C4, GPIO_PULLUP_ONLY);
}

char checkcolum1( void )    // HANG1
{
   if (gpio_get_level(C1) == 0)
   {
      while (gpio_get_level(C1) == 0)
      {
      }
      return '1';
   };

   if (gpio_get_level(C2) == 0)
   {
      while (gpio_get_level(C2) == 0)
      {
      }
      return '2';
   };

   if (gpio_get_level(C3) == 0)
   {
      while (gpio_get_level(C3) == 0)
      {
      }
      return '3';
   };

   if (gpio_get_level(C4) == 0)
   {
      while (gpio_get_level(C4) == 0)
      {
      }
      return 'A';
   };
    return 'o';
}
char checkcolum2( void )
{
   if (gpio_get_level(C1) == 0)
   {
      while (gpio_get_level(C1) == 0)
      {
      }
      return '4';
   };

   if (gpio_get_level(C2) == 0)
   {
      while (gpio_get_level(C2) == 0)
      {
      }
      return '5';
   };

   if (gpio_get_level(C3) == 0)
   {
      while (gpio_get_level(C3) == 0)
      {
      }
      return '6';
   };

   if (gpio_get_level(C4) == 0)
   {
      while (gpio_get_level(C4) == 0)
      {
      }
      return 'B';
   };
    return 'o';
}
char checkcolum3( void )
{
   if (gpio_get_level(C1) == 0)
   {
      while (gpio_get_level(C1) == 0)
      {
      }
      return '7';
   };

   if (gpio_get_level(C2) == 0)
   {
      while (gpio_get_level(C2) == 0)
      {
      }
      return '8';
   };

   if (gpio_get_level(C3) == 0)
   {
      while (gpio_get_level(C3) == 0)
      {
      }
      return '9';
   };

   if (gpio_get_level(C4) == 0)
   {
      while (gpio_get_level(C4) == 0)
      {
      }
      return 'C';
   };
    return 'o';
}
char checkcolum4( void )
{
   if (gpio_get_level(C1) == 0)
   {
      while (gpio_get_level(C1) == 0)
      {
      }
      return '*';
   };

   if (gpio_get_level(C2) == 0)
   {
      while (gpio_get_level(C2) == 0)
      {
      }
      return '0';    // bam so 0
   };

   if (gpio_get_level(C3) == 0)
   {
      while (gpio_get_level(C3) == 0)
      {
      }
      return '#';
   };

   if (gpio_get_level(C4) == 0)
   {
      while (gpio_get_level(C4) == 0)
      {
      }
      return 'D';
   };
   return 'o';
}

char waitKey(uint8_t row)
{
   return check_keypad[row]();
}



 