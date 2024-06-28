/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the PSoC 4 CAPSENSE liquid Level Sensing 
*              Application for ModusToolbox.
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2023-2024, Cypress Semiconductor Corporation (an Infineon company) or
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
 * Include header files
 ******************************************************************************/
#include "cy_pdl.h"
#include "cybsp.h"
#include "cycfg.h"
#include "cycfg_capsense.h"
#include "cy_em_eeprom.h"
#include "interface.h"


/*******************************************************************************
* Macros
*******************************************************************************/
#define CAPSENSE_INTR_PRIORITY    (3u)
#define CY_ASSERT_FAILED          (0u)

/* EZI2C interrupt priority must be higher than CAPSENSE interrupt. */
#define EZI2C_INTR_PRIORITY       (2u)

#define LEVELMM_MAX         (153u)/* Max sensor height in mm */
/* Height of a single sensor. Fixed precision 24.8 */
#define SENSORHEIGHT        ((LEVELMM_MAX * 256) / (NUMSENSORS - 1)) 

/* EEPROM constants */
#define Em_EEPROM_FLASH_BASE_ADDR        (CYDEV_FLASH_BASE)
#define Em_EEPROM_FLASH_SIZE             (CYDEV_FLASH_SIZE)

/* UART constants */
#define UART_DELAY          (100u)  /* Delay in ms to control data logging rate.*/

/* Enable this, if Tuner needs to be enabled */
#define CAPSENSE_TUNER_EN                            (0u)

/*******************************************************************************
* Global Variables
********************************************************************************/

/* Emulated EEPROM configuration and context structure. */
cy_stc_eeprom_config_t em_eeprom_config =
{
    .eepromSize         = EM_EEPROM_SIZE,           /* 256 bytes */
    .blockingWrite      = BLOCKING_WRITE,           /* Blocking writes enabled */
    .redundantCopy      = REDUNDANT_COPY,           /* Redundant copy enabled */
    .wearLevelingFactor = WEAR_LEVELLING_FACTOR,    /* Wear levelling factor of 2 */
    .simpleMode         = SIMPLE_MODE,              /* Simple mode disabled */
};

/* Sensor counts when empty to calculate diff counts. Loaded from EEPROM array */
CY_ALIGN(CY_EM_EEPROM_FLASH_SIZEOF_ROW)
const uint8_t eepromEmptyOffset[EM_EEPROM_PHYSICAL_SIZE] = {0u};

/* Number of sensors currently submerged */
uint8_t sensorActiveCount = 0u;               
int32_t levelPercent = 0u;                    /* fixed precision 24.8 */
int32_t levelMm = 0u;                         /* fixed precision 24.8 */
/* Height of a single sensor. Fixed precision 24.8 */
int32_t sensorHeight = SENSORHEIGHT;          

/* Flag to signal when new sensor calibration values should be stored to EEPROM */
uint8_t cal_flag = FALSE;                      

/* Liquid Level variables */
int32_t sensorRaw[NUMSENSORS] = {0u};         /* Sensor raw counts */
int32_t sensorDiff[NUMSENSORS] = {0u};        /* Sensor difference counts */
/* Sensor counts when empty to calculate diff counts. Loaded from EEPROM array */
int32_t sensorEmptyOffset[LOGICAL_EM_EEPROM_SIZE] = {0u}; 
/* Scaling factor to normalize sensor full scale counts. 0x0100 = 1.0 in fixed precision 8.8 */
int16_t sensorScale[NUMSENSORS] = {0x01D0, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x01C0}; 
int32_t sensorProcessed[NUMSENSORS] = {0u, 0u}; /* fixed precision 24.8 */

/* Threshold for determining if a sensor is submerged. */
int16_t sensorLimits[] = {900, 550, 480, 480, 500, 450, 440, 450, 450, 400, 400, 550};

/*******************************************************************************
* Global Definitions
*******************************************************************************/
cy_stc_scb_ezi2c_context_t ezi2c_context;
cy_stc_eeprom_context_t em_eeprom_context;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
static void initialize_capsense(void);
static void capsense_isr(void);

#if CAPSENSE_TUNER_EN
static void ezi2c_isr(void);
static void initialize_capsense_tuner(void);
#endif


/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
*  System entrance point. This function performs
*  - initial setup of device
*  - initialize CAPSENSE
*  - initialize tuner communication
*  - initialize EEPROM
*  - initialize UART
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    cy_stc_scb_uart_context_t CYBSP_UART_context;
    cy_en_em_eeprom_status_t em_eeprom_status;

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(CY_ASSERT_FAILED);
    }

    /* Configure and enable the UART peripheral */
    Cy_SCB_UART_Init(CYBSP_UART_HW, &CYBSP_UART_config, &CYBSP_UART_context);
    Cy_SCB_UART_Enable(CYBSP_UART_HW);

    /* Enable global interrupts */
    __enable_irq();

    /* Send a string over serial terminal */
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "\x1b[2J\x1b[;H");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "***************************************************************\r\n");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "CE239150 - PSoC 4 Capacitive Liquid Level Sensing\r\n");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "***************************************************************\r\n\n");

    display_uart_commands();
    display_current_cal_val();

    /* Initialize the flash start address in Emulated EEPROM configuration
     * structure
     */
    em_eeprom_config.userFlashStartAddr = (uint32_t) eepromEmptyOffset;

    /* Initialize Emulated EEPROM */
    em_eeprom_status = Cy_Em_EEPROM_Init(&em_eeprom_config, &em_eeprom_context);
    handle_error(em_eeprom_status, "Emulated EEPROM Initialization Error \r\n");

    /* Read stored empty offset values from EEPROM */
    for(uint8_t index = 0; index < NUMSENSORS; index++)
    {
        sensorEmptyOffset[index] = ((volatile int16_t *)eepromEmptyOffset)[index];
    }

#if CAPSENSE_TUNER_EN
    /* Initialize EZI2C */
    initialize_capsense_tuner();
#endif

    /* Initialize CAPSENSE */
    initialize_capsense();

    /* Start the first scan */
    Cy_CapSense_ScanAllWidgets(&cy_capsense_context);

    for (;;)
    {
        /* Check for CapSense scan complete*/
        if(CY_CAPSENSE_NOT_BUSY == Cy_CapSense_IsBusy(&cy_capsense_context))
        {
            /* Process all widgets */
            if (CY_CAPSENSE_STATUS_SUCCESS != Cy_CapSense_ProcessAllWidgets(&cy_capsense_context))
            {
                Cy_SCB_UART_PutString(CYBSP_UART_HW, "Error in processing widgets\r\n");
                CY_ASSERT(0u);
            }
            /* Delay to control data logging rate */
            Cy_SysLib_Delay(UART_DELAY);

            /* Read and store new sensor raw counts, remove empty offset
             * calibration from sensor raw counts.
             */
            for(uint8_t i = 0; i < NUMSENSORS; i++)
            {
                sensorRaw[i] = cy_capsense_tuner.sensorContext[i].raw;
                sensorDiff[i] = sensorRaw[i];

            }

            /* Start scan for next iteration */
            if (CY_CAPSENSE_STATUS_SUCCESS != Cy_CapSense_ScanAllWidgets(&cy_capsense_context))
            {
                Cy_SCB_UART_PutString(CYBSP_UART_HW, "Error in scanning widgets\r\n");
                CY_ASSERT(0u);
            }
            if(cal_flag == TRUE)
            {
                cal_flag = FALSE;
                store_calibration();
            }

            /* Remove empty offset calibration from sensor raw counts
             * and normalize sensor full count values.
             */
            sensorActiveCount = 0;
            for(uint8_t i = 0; i < NUMSENSORS; i++)
            {
                sensorDiff[i] -= sensorEmptyOffset[i];
                if (sensorDiff[i] < 0)
                {
                    sensorDiff[i] = 0;
                }
                sensorProcessed[i] = (sensorDiff[i] * sensorScale[i]) >> 8;
                /* Find the number of submerged sensors */
                if(sensorProcessed[i] > sensorLimits[i]/2)
                {
                    /* First and last sensor are half the height of middle sensors. */
                    if((i == 0) || (i == NUMSENSORS - 1))
                    {
                        sensorActiveCount += 1;
                    }
                    /* Middle sensors are twice the height of 1st and last sensors */
                    else
                    {
                        sensorActiveCount += 2;
                    }
                }

            }
        }

        /* Calculate liquid level height in mm */
        levelMm = sensorActiveCount * (sensorHeight >> 1);
        /* If level is near full value then round to full.
         * Avoids fixed precision rounding errors.
         */
        if(levelMm > ((int32_t)LEVELMM_MAX << 8) - (sensorHeight >> 2))
        {
            levelMm = LEVELMM_MAX << 8;
        }

        /* Calculate level percent. Stored in fixed precision
         * 24.8 format to hold fractional percent.
         */
        levelPercent = (levelMm * 100) / LEVELMM_MAX;
        /* Report level and process UART interfaces */
        display_cur_liquid_level();
#if CAPSENSE_TUNER_EN
        /* Establishes synchronized communication with the CapSense Tuner tool */
        Cy_CapSense_RunTuner(&cy_capsense_context);
#endif
    }
}

/*******************************************************************************
* Function Name: initialize_capsense
********************************************************************************
* Summary:
*  This function initializes the CAPSENSE and configures the CAPSENSE
*  interrupt.
*
*******************************************************************************/
static void initialize_capsense(void)
{
    cy_capsense_status_t status = CY_CAPSENSE_STATUS_SUCCESS;

    /* CAPSENSE interrupt configuration */
    const cy_stc_sysint_t capsense_interrupt_config =
    {
        .intrSrc = CYBSP_CAPSENSE_IRQ,
        .intrPriority = CAPSENSE_INTR_PRIORITY,
    };

    /* Capture the CSD HW block and initialize it to the default state. */
    status = Cy_CapSense_Init(&cy_capsense_context);

    if (CY_CAPSENSE_STATUS_SUCCESS == status)
    {
        /* Initialize CAPSENSE interrupt */
        Cy_SysInt_Init(&capsense_interrupt_config, capsense_isr);
        NVIC_ClearPendingIRQ(capsense_interrupt_config.intrSrc);
        NVIC_EnableIRQ(capsense_interrupt_config.intrSrc);

        /* Initialize the CAPSENSE firmware modules. */
        status = Cy_CapSense_Enable(&cy_capsense_context);
    }

    if(status != CY_CAPSENSE_STATUS_SUCCESS)
    {
        /* This status could fail before tuning the sensors correctly.
         * Ensure that this function passes after the CAPSENSE sensors are tuned
         * as per procedure give in the README.md file */
    }
}


/*******************************************************************************
* Function Name: capsense_isr
********************************************************************************
* Summary:
* Wrapper function for handling interrupts from CAPSENSE block.
*
*******************************************************************************/
static void capsense_isr(void)
{
    Cy_CapSense_InterruptHandler(CYBSP_CAPSENSE_HW, &cy_capsense_context);
}

#if CAPSENSE_TUNER_EN
/*******************************************************************************
* Function Name: initialize_capsense_tuner
********************************************************************************
* Summary:
* - EZI2C module to communicate with the CAPSENSE Tuner tool.
*
*******************************************************************************/
static void initialize_capsense_tuner(void)
{
    cy_en_scb_ezi2c_status_t status = CY_SCB_EZI2C_SUCCESS;

    /* EZI2C interrupt configuration structure */
    const cy_stc_sysint_t ezi2c_intr_config =
    {
        .intrSrc = CYBSP_EZI2C_IRQ,
        .intrPriority = EZI2C_INTR_PRIORITY,
    };

    /* Initialize the EzI2C firmware module */
    status = Cy_SCB_EZI2C_Init(CYBSP_EZI2C_HW, &CYBSP_EZI2C_config, &ezi2c_context);

    if(status != CY_SCB_EZI2C_SUCCESS)
    {
        CY_ASSERT(CY_ASSERT_FAILED);
    }

    Cy_SysInt_Init(&ezi2c_intr_config, ezi2c_isr);
    NVIC_EnableIRQ(ezi2c_intr_config.intrSrc);

    /* Set the CAPSENSE data structure as the I2C buffer to be exposed to the
     * master on primary slave address interface. Any I2C host tools such as
     * the Tuner or the Bridge Control Panel can read this buffer but you can
     * connect only one tool at a time.
     */
    Cy_SCB_EZI2C_SetBuffer1(CYBSP_EZI2C_HW, (uint8_t *)&cy_capsense_tuner,
                            sizeof(cy_capsense_tuner), sizeof(cy_capsense_tuner),
                            &ezi2c_context);

    /* Enables the SCB block for the EZI2C operation. */
    Cy_SCB_EZI2C_Enable(CYBSP_EZI2C_HW);

    /* EZI2C initialization failed */
    if(status != CY_SCB_EZI2C_SUCCESS)
    {
        CY_ASSERT(CY_ASSERT_FAILED);
    }
}


/*******************************************************************************
* Function Name: ezi2c_isr
********************************************************************************
* Summary:
*  Wrapper function for handling interrupts from EZI2C block.
*
*******************************************************************************/
static void ezi2c_isr(void)
{
    Cy_SCB_EZI2C_Interrupt(CYBSP_EZI2C_HW, &ezi2c_context);
}

#endif /* CAPSENSE_TUNER_EN */

/* [] END OF FILE */
