#ifndef __KEY_BOARD_H
#define __KEY_BOARD_H
#include "esp_err.h"
#include "hal/gpio_types.h"
#include <stdio.h>
#include "esp_system.h"
// #define R1 GPIO_NUM_14      
// #define R2 GPIO_NUM_27  
// #define R3 GPIO_NUM_26   
// #define R4 GPIO_NUM_25   
// #define C1 GPIO_NUM_33   
// #define C2 GPIO_NUM_32    
// #define C3 GPIO_NUM_35  
// #define C4 GPIO_NUM_34   
#define R1 GPIO_NUM_15      
#define R2 GPIO_NUM_2  
#define R3 GPIO_NUM_0   
#define R4 GPIO_NUM_4   
#define C1 GPIO_NUM_16   
#define C2 GPIO_NUM_17    
#define C3 GPIO_NUM_5  
#define C4 GPIO_NUM_18   
 

void keypad_row1_en(void);
void keypad_row2_en(void);
void keypad_row3_en(void);
void keypad_row4_en(void);
void KeyBoard_Init();
char waitKey(uint8_t row);

#endif