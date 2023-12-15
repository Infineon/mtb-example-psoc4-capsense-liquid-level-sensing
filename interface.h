/*******************************************************************************
* File Name: interface.h
*
* Description: This file is the public interface of interface.c source file.
*              
*
* Related Document: README.md
*
*******************************************************************************
* Copyright 2023, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/
/*******************************************************************************
 * Include guard
 ******************************************************************************/
#ifndef SOURCE_INTERFACE_H_
#define SOURCE_INTERFACE_H_

#include "cy_em_eeprom.h"

/*******************************************************************************
* Global constants
*******************************************************************************/
/* General constants */
#define TRUE                (1u)
#define FALSE               (0u)

/* Liquid Level constants */
#define NUMSENSORS          (12u)          /* Number of CapSense sensors */

/* UART constants */
#define UART_NONE           (0u)
#define UART_BASIC          (1u)
#define UART_CSVINIT        (2u)
#define UART_CSV            (3u)

/* Logical Size of Emulated EEPROM in bytes */
#define LOGICAL_EM_EEPROM_SIZE      (32u)
#define LOGICAL_EM_EEPROM_START     (0u)

/* Emulated EEPROM Configuration details. All the sizes mentioned are in bytes.
 * For details on how to configure these values refer to cy_em_eeprom.h. The
 * middleware documentation is provided in Emulated EEPROM API Reference Manual.
 * The user can access it from the Documentation section in the Quick Panel.
 */
#define EM_EEPROM_SIZE              CY_EM_EEPROM_FLASH_SIZEOF_ROW
#define BLOCKING_WRITE              (1u)
#define REDUNDANT_COPY              (1u)
#define WEAR_LEVELLING_FACTOR       (2u)
#define SIMPLE_MODE                 (0u)

#define EM_EEPROM_PHYSICAL_SIZE     (CY_EM_EEPROM_GET_PHYSICAL_SIZE(EM_EEPROM_SIZE, SIMPLE_MODE, WEAR_LEVELLING_FACTOR, REDUNDANT_COPY))

#define NUM_SAMPLES                  (20u)

/*******************************************************************************
* External variables
*******************************************************************************/
extern int32_t sensorEmptyOffset[];
extern int32_t sensorEmptyOffset_2[];
extern int32_t sensorDiff[NUMSENSORS];
extern cy_stc_eeprom_context_t em_eeprom_context;
extern uint8_t uartTxMode;
extern int32_t levelPercent;
extern int32_t levelMm;
extern int32_t sensorRaw[NUMSENSORS]; 
extern int32_t sensorDiff[NUMSENSORS];
extern int32_t sensorProcessed[NUMSENSORS];
extern uint8_t sensorActiveCount;
extern uint8_t cal_flag;
extern uint8_t storeSampleFlag;
extern uint8_t resetSampleFlag;


/*******************************************************************************
 * Function prototype
 ******************************************************************************/
void display_uart_commands(void);
void display_decimal_val(int32_t number, int8_t leading_zeros);
void display_current_cal_val(void);
void handle_error(uint32_t status, char *message);
void display_cur_liquid_level(void);
void display_decimal_fixed_val(int32_t number, uint8_t fixed_shift, uint8_t num_decimal);
void display_next_level_val(void);
void receive_uart_cmd(void);
void store_calibration(void);

#endif /* SOURCE_INTERFACE_H_ */


/* [] END OF FILE  */
