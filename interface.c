/*******************************************************************************
* File Name: interface.c
*
* Description: This file contains the functions that helps to demonstrate Liquid
*              Level sensing (LLS) using capacitance.
*
* Related Document: README.md
*
*******************************************************************************
 * (c) 2023-2025, Infineon Technologies AG, or an affiliate of Infineon
 * Technologies AG. All rights reserved.
 * This software, associated documentation and materials ("Software") is
 * owned by Infineon Technologies AG or one of its affiliates ("Infineon")
 * and is protected by and subject to worldwide patent protection, worldwide
 * copyright laws, and international treaty provisions. Therefore, you may use
 * this Software only as provided in the license agreement accompanying the
 * software package from which you obtained this Software. If no license
 * agreement applies, then any use, reproduction, modification, translation, or
 * compilation of this Software is prohibited without the express written
 * permission of Infineon.
 *
 * Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
 * IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
 * THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
 * SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
 * Infineon reserves the right to make changes to the Software without notice.
 * You are responsible for properly designing, programming, and testing the
 * functionality and safety of your intended application of the Software, as
 * well as complying with any legal requirements related to its use. Infineon
 * does not guarantee that the Software will be free from intrusion, data theft
 * or loss, or other breaches ("Security Breaches"), and Infineon shall have
 * no liability arising out of any Security Breaches. Unless otherwise
 * explicitly approved by Infineon, the Software may not be used in any
 * application where a failure of the Product or any consequences of the use
 * thereof can reasonably be expected to result in personal injury.
*******************************************************************************/
#include "cy_pdl.h"
#include "cybsp.h"
#include "cycfg.h"
#include "interface.h"
#include "cy_em_eeprom.h"

#include<stdio.h>

/*******************************************************************************
* Global Variables
*******************************************************************************/
/* UART variables */
uint8_t uartTxMode = UART_BASIC;
uint8_t storeSampleFlag = FALSE;
uint8_t resetSampleFlag = FALSE;
int16_t arrayAxisLabel[NUM_SAMPLES] = {-5,0,10,20,30,40,50,60,70,80,90,100,110,120,130,140,150,153,160,0};

/*******************************************************************************
* Function Name: display_uart_commands
********************************************************************************
* Summary:
* This is the function for displaying the available commands in the UART.
*
* Parameters:
*    void    
*
* Return:
*  void
*******************************************************************************/

/* Transmit list of available commands */
void display_uart_commands(void)
{
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "\n\r");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "Commands \n\r");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "  stop - Stops displaying data over UART.\n\r");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "  cal - Stores empty container sensor values to EEPROM for calibration.\n\r");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "  basic - Outputs liquid level in mm and %.\n\r");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "  csv - Outputs intermediate computation values as well as liquid level in CSV format.\n\r");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "  'Enter' - Outputs the next set of level values from the sample array.\n\r");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "  reset - Resets the sample array pointer to 0 %.\n\r");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "\n\r");
}

/*******************************************************************************
* Function Name: display_current_cal_val
********************************************************************************
* Summary:
* This function displays currently stored calibration values in the UART terminal.
*
* Parameters:
*    void    
*
* Return:
*  void
*******************************************************************************/
void display_current_cal_val(void)
{
    uint8_t i;

    Cy_SCB_UART_PutString(CYBSP_UART_HW, "EmptyCal=");
    for(i = 0; i < NUMSENSORS; i++)
    {
        display_decimal_val(sensorEmptyOffset[i], 0);
        Cy_SCB_UART_PutString(CYBSP_UART_HW, ",");
    }
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "\r\n");
}

/*******************************************************************************
* Function Name: display_decimal_val
********************************************************************************
* Summary:
* This function displays the decimal representation of a int32 variable with
* optional leading zeros in the UART terminal.
*
* Parameters:
*    number           Number to be displayed in decimal format.
*    leading_zeros    Number of leading zeros to force display of.
*                     Useful for fractional numbers after decimal point.
*
* Return:
*  void
*******************************************************************************/
void display_decimal_val(int32_t number, int8_t leading_zeros)
{
    uint8_t digit = 0;
    uint8_t zero_flag = 0;
    int8_t i = 0;

    const int32_t decimal[] = {1000000000, 100000000, 10000000, 1000000, 100000, 10000, 1000, 100, 10, 1};

    /* Check for out of range parameters */
    if(leading_zeros > 10)
    {
        leading_zeros = 10;
    }
    if(number < 0)
    {
        number *= -1;
        Cy_SCB_UART_Put(CYBSP_UART_HW, '-');
    }

    /* Loop through each digit and subtract out represented decimal quantity */
    for(i = 0; i <= 9; i++)
    {
        digit = 0;
        while(number >= decimal[i])
        {
            zero_flag = 1;
            number -= decimal[i];
            digit++;
        }
        /* display digit (and any following) if digit > 0, 1s digit = 0, or
         *  we have reached the number of forced leading zeros.
         */
        if((zero_flag == 1) || (i == 9) || (i >= (10 - leading_zeros)))
        {
            while (!Cy_SCB_UART_Put(CYBSP_UART_HW, digit + 48));
        }
    }

}

/*******************************************************************************
* Function Name: store_calibration
********************************************************************************
* Summary:
* This function stores the current raw counts with an empty container to emulated
* EEPROM (in Flash). CapSense_CSD_ScanEnabledWidgets() function must be called
* prior to this function to ensure valid data is present for storage.
*
* Parameters:
*    void
*
* Return:
*  void
*******************************************************************************/
void store_calibration(void)
{
    uint8_t i;
    cy_en_em_eeprom_status_t em_eeprom_status;

    /* Calculate offset for each sensor */
    for(i = 0; i < NUMSENSORS; i++)
    {
          sensorEmptyOffset[i] = sensorDiff[i];
    }
    display_current_cal_val();

    /* Store new cal values */
    /* Write initial data to Emulated EEPROM. */
    em_eeprom_status = Cy_Em_EEPROM_Write(LOGICAL_EM_EEPROM_START,
                                             sensorEmptyOffset,
                                             LOGICAL_EM_EEPROM_SIZE,
                                             &em_eeprom_context);

    /* EEPROM Error handler */
    handle_error(em_eeprom_status, "Emulated EEPROM Write failed \r\n");
}

/********************************************************************************
* Function Name: handle_error
*********************************************************************************
* Summary:
* This function processes unrecoverable errors such as any component
* initialization errors etc. In case of such error the system will
* stay in the infinite loop of this function.
*
* Parameters:
*  uint32_t status: contains the status.
*  char* message: contains the message that is printed to the serial terminal.
*
* Note:
*  If error occurs interrupts are disabled.
*
********************************************************************************/
void handle_error(uint32_t status, char *message)
{
    if(CY_EM_EEPROM_SUCCESS != status)
    {
        __disable_irq();
        if(NULL != message)
        {
            Cy_SCB_UART_PutString(CYBSP_UART_HW, message);
        }
        while(1u);
    }

}

/*******************************************************************************
* Function Name: display_cur_liquid_level
********************************************************************************
* Summary:
* This function displays the the current liquid level in percent and mm in the
* UART terminal.
*
* Parameters:
*    void
*
* Return:
*  void
*******************************************************************************/
void display_cur_liquid_level(void)
{
    uint8_t i;
    
    if(uartTxMode == UART_BASIC)
    {
        /* Transmit current liquid level Percent */
        Cy_SCB_UART_PutString(CYBSP_UART_HW, "%=");
        display_decimal_val(levelPercent >> 8, 0);
        /* Add one decimal point digit to percent */
        Cy_SCB_UART_PutString(CYBSP_UART_HW, ".");
        display_decimal_val(((levelPercent & 0x000000FF) * 10) >> 8, 0);
        /* Transmit current liquid level mm */
        Cy_SCB_UART_PutString(CYBSP_UART_HW, "   mm=");
        display_decimal_val(levelMm >> 8, 0);
        Cy_SCB_UART_PutString(CYBSP_UART_HW, ".");
        display_decimal_val(((levelMm & 0x000000FF) * 10) >> 8, 0);
        Cy_SCB_UART_PutString(CYBSP_UART_HW, "\r\n");
    }
    else if ((uartTxMode == UART_CSVINIT) || (uartTxMode == UART_CSV))
    {
        if (uartTxMode == UART_CSVINIT)
        {
            for(i = 0; i < NUMSENSORS; i++)
            {
                Cy_SCB_UART_PutString(CYBSP_UART_HW, "Raw");
                display_decimal_val(i, 0);
                Cy_SCB_UART_PutString(CYBSP_UART_HW, ",");
                Cy_SCB_UART_PutString(CYBSP_UART_HW, "Diff");
                display_decimal_val(i, 0);
                Cy_SCB_UART_PutString(CYBSP_UART_HW, ",");
                Cy_SCB_UART_PutString(CYBSP_UART_HW, "Proc");
                display_decimal_val(i, 0);
                Cy_SCB_UART_PutString(CYBSP_UART_HW, ",");                            
            }
            Cy_SCB_UART_PutString(CYBSP_UART_HW, "SenActCnt,");

            Cy_SCB_UART_PutString(CYBSP_UART_HW, "Level%, LevelMm");
            Cy_SCB_UART_PutString(CYBSP_UART_HW, "\r\n");
            uartTxMode = UART_CSV;
        }
        else
        {
            for(i = 0; i < NUMSENSORS; i++)
            {
                display_decimal_val(sensorRaw[i], 0);
                Cy_SCB_UART_PutString(CYBSP_UART_HW, ",");
                display_decimal_val(sensorDiff[i], 0);
                Cy_SCB_UART_PutString(CYBSP_UART_HW, ",");
                display_decimal_val(sensorProcessed[i], 0);
                Cy_SCB_UART_PutString(CYBSP_UART_HW, ",");                             
            }
            display_decimal_val(sensorActiveCount, 0);
            Cy_SCB_UART_PutString(CYBSP_UART_HW, ",");

            display_decimal_fixed_val(levelPercent, 8, 1);
            Cy_SCB_UART_PutString(CYBSP_UART_HW, ",");
            /* Transmit current liquid level mm */
            display_decimal_fixed_val(levelMm, 8, 1);
            Cy_SCB_UART_PutString(CYBSP_UART_HW, "\r\n");
        }

    }

    
    /* Looking for UART commands. */
    receive_uart_cmd();
    /* Check if test UART message should be sent */
    display_next_level_val();
}

/*******************************************************************************
* Function Name: receive_uart_cmd 
********************************************************************************
* Summary:
* This function receives the command via the UART terminal.
*
* Parameters:
*    void
*
* Return:
*  void
*******************************************************************************/
void receive_uart_cmd(void)
{
    static uint16_t bufferIndex = 0;
    static char rxBuffer[32]= {'\0'};
    uint32_t read_data = 0;

    /* Check if there is a received character from user console */
    if (0UL != Cy_SCB_UART_GetNumInRxFifo(CYBSP_UART_HW))
    {
        /* Re-transmit whatever the user types on the console */
        read_data = Cy_SCB_UART_Get(CYBSP_UART_HW);

        if(read_data > '0')
        {
            while (0UL == Cy_SCB_UART_Put(CYBSP_UART_HW, read_data))
            {

            }
            rxBuffer[bufferIndex] = read_data;
            bufferIndex++;
        }
        if((read_data == '\r') || (read_data == '\n'))
        {
            rxBuffer[bufferIndex] = '\0';
            if(strcmp("cal", rxBuffer) == 0)
            {
                cal_flag = TRUE;
            }
            else if(strcmp("stop", rxBuffer) == 0)
            {
                uartTxMode = UART_NONE;
            }
            else if(strcmp("csv", rxBuffer) == 0)
            {
                uartTxMode = UART_CSVINIT;
            }
            else if(strcmp("basic", rxBuffer) == 0)
            {
                uartTxMode = UART_BASIC;
            }
            else if(strcmp("", rxBuffer) == 0)
            {
                storeSampleFlag = TRUE;
                uartTxMode = UART_NONE;
            }
            else if((strcmp("reset", rxBuffer) == 0))
            {
                resetSampleFlag = TRUE;
                uartTxMode = UART_NONE;
            }
            else
            {
                Cy_SCB_UART_PutString(CYBSP_UART_HW, "Command Error");
                Cy_SCB_UART_PutString(CYBSP_UART_HW, "\r\n");
            }

            bufferIndex = 0;
            memset(rxBuffer, '\0', strlen(rxBuffer));
        }

    }
}
/********************************************************************************
* Function Name: display_decimal_fixed_val
*********************************************************************************
* Summary:
* This function displays the decimal representation of a fixed precision int32_t
* variable.
*
* Parameters:
*  int32_t number: Number to be displayed in decimal.
*  int32_t fixed_shift: Number of bits for fractional portion of number.
*  int8_t num_decimal: Number of decimal digits after the decimal point to display.
*
* Note:
*  If error occurs interrupts are disabled.
*
********************************************************************************/
void display_decimal_fixed_val(int32_t number, uint8_t fixed_shift, uint8_t num_decimal)
{
    int8_t i = 0;
    int64_t decimal_num = 0;
    int32_t dec_digits = 1;

    /* Check for out of range parameters */
    if(fixed_shift > 31)
    {
        fixed_shift = 31;
    }
    if(num_decimal > 9)
    {
        num_decimal = 9;
    }

    /* Display whole number part of value */
    display_decimal_val(number >> fixed_shift, 0);
    /* Display fractional part of number if required */
    if(num_decimal > 0)
    {
        Cy_SCB_UART_Put(CYBSP_UART_HW, '.');
        dec_digits = 1;
        /* Calculate fractional portion scaling multiplier */
        for(i = 0; i < num_decimal; i++)
        {
            dec_digits *= 10;
        }
        decimal_num = ((number & (0xFFFFFFFF >> (31 - fixed_shift))) * dec_digits) >> fixed_shift;
        display_decimal_val(decimal_num, num_decimal);
    }
}

/*******************************************************************************
* Function Name: display_next_level_val
********************************************************************************
* Summary:
* This function displays the the next set of level values from the sample array
* in the UART terminal.
*
* Parameters:
*    void
*
* Return:
*  void
*******************************************************************************/
void display_next_level_val(void)
{
    uint8_t i;
    static uint16_t sampleIndex;

    /* Check if we should output sensor level data */
    if(storeSampleFlag == TRUE)
    {
        uartTxMode = UART_NONE;

        if(sampleIndex == 0)
        {
            Cy_SCB_UART_PutString(CYBSP_UART_HW, "PresetMm,");
            for(i = 1; i < NUMSENSORS; i++)
            {
                Cy_SCB_UART_PutString(CYBSP_UART_HW, "SenDiff");
                display_decimal_val(i, 0);
                Cy_SCB_UART_PutString(CYBSP_UART_HW, ",");
            }
            Cy_SCB_UART_PutString(CYBSP_UART_HW, "Level%, LevelMm");
            Cy_SCB_UART_PutString(CYBSP_UART_HW, "\r\n");
        }
        display_decimal_val(arrayAxisLabel[sampleIndex], 0);
        Cy_SCB_UART_PutString(CYBSP_UART_HW, ",");
        for(i = 1; i < NUMSENSORS; i++)
        {
            display_decimal_val(sensorProcessed[i], 0);
            Cy_SCB_UART_PutString(CYBSP_UART_HW, ",");
        }
        display_decimal_fixed_val(levelPercent, 8, 1);
        Cy_SCB_UART_PutString(CYBSP_UART_HW, ",");
        /* Transmit current liquid level mm */
        display_decimal_fixed_val(levelMm, 8, 1);
        Cy_SCB_UART_PutString(CYBSP_UART_HW, "\r\n");

        /* Increment and limit array index */
        sampleIndex += 1;
        if(sampleIndex >= NUM_SAMPLES)
        {
            sampleIndex = NUM_SAMPLES - 1;
        }

        /* Clear flag to allow user to press for next store request */
        storeSampleFlag = FALSE;
    }
    /* Check if we should reset for a new test sequence */
    if(resetSampleFlag == TRUE)
    {
        sampleIndex = 0;
        Cy_SCB_UART_PutString(CYBSP_UART_HW, "Reset Test Level");
        Cy_SCB_UART_PutString(CYBSP_UART_HW, "\r\n");
        /* Clear flags to allow user to press for next store request */
        resetSampleFlag = FALSE;
        storeSampleFlag = FALSE;
    }
}


/* [] END OF FILE */
