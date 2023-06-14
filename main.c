/*******************************************************************************
* File Name:   main.c
*
* Description: This example project demonstrates the basic operation of SPI
* resource as Master using HAL. The SPI master sends command packetsto the SPI
* slave to control an user LED.
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2019-2023, Cypress Semiconductor Corporation (an Infineon company) or
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
* Header Files
*******************************************************************************/
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

#include "amux.h"
#include "sampler.h"

/*******************************************************************************
* Macros
*******************************************************************************/
#define SAR_ADC_SAMPLING_RATE_SPS   920000
#define SAR_ADC_ACQUISTION_TIME_NS  180  

/*******************************************************************************
* Global Variables
*******************************************************************************/
amux_t adc_mux;
sampler_t adc_sampler;

int16_t adc_samples[SAMPLER_MAX_NUM_CHANNELS];

/*******************************************************************************
* Function Prototypes
*******************************************************************************/


/*******************************************************************************
* Function Definitions
*******************************************************************************/

/*******************************************************************************
* Function Name: handle_error
********************************************************************************
* Summary:
* User defined error handling function
*
* Parameters:
*  uint32_t status - status indicates success or failure
*
* Return:
*  void
*
*******************************************************************************/
void handle_error(uint32_t status)
{
    if (status != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }
}

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* The main function.
*   1. Initializes the board, retarget-io and led
*   2. Configures the SPI Master to send command packet to the slave
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init();
    /* Board init failed. Stop program execution */
    handle_error(result);

    /* Initialize retarget-io for uart logs */
    result = cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX,
                                  CY_RETARGET_IO_BAUDRATE);
    /* Retarget-io init failed. Stop program execution */
    handle_error(result);

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");

    printf("************************************ \r\n"
           "Extending the number of ADC channels \r\n"
           "************************************ \r\n\n");

    /* Enable interrupts */
    __enable_irq();

    /* Initialize the AREF */
    Cy_SysAnalog_Init(&CYBSP_AREF_config);
    Cy_SysAnalog_Enable();

    /* Initialize the ADC */
    Cy_SAR_Init(CYBSP_ADC_HW, &CYBSP_ADC_config);
    Cy_SAR_Enable(CYBSP_ADC_HW);

    /* Initiate the Amux and add pins to it */
    AMux_Init(&adc_mux, AMUX_B);
    AMux_AddPort(&adc_mux, GPIO_PRT9,  0xFF);
    AMux_AddPort(&adc_mux, GPIO_PRT10, 0xFF);
    AMux_AddPort(&adc_mux, GPIO_PRT12, 0xFF);
    /* Setup the DMA AMux */
    AMux_SetupDMA(&adc_mux, CYBSP_DMA_AMUX_HW, CYBSP_DMA_AMUX_CHANNEL);
    AMux_StartDMA(&adc_mux);

    /* Initalize and configure the Sampler */
    Sampler_Init(&adc_sampler, CYBSP_ADC_HW, CYBSP_TIMER_HW, CYBSP_TIMER_NUM);
    Sampler_SetScanRate(&adc_sampler, SAR_ADC_SAMPLING_RATE_SPS, SAR_ADC_ACQUISTION_TIME_NS);
    Sampler_Configure(&adc_sampler, adc_mux.num_conn, adc_samples);
    /* Setup the Sampler DMA and start the Sampler */
    Sampler_SetupDMA(&adc_sampler, CYBSP_DMA_ADC_HW, CYBSP_DMA_ADC_CHANNEL);
    Sampler_Start(&adc_sampler);

    for (;;)
    {
        CyDelay(1000);
        
        printf("\x1b[2J\x1b[;H");
        printf("------------------------------------------------------------\n\r");
        printf("Port| Pin0 | Pin1 | Pin2 | Pin3 | Pin4 | Pin5 | Pin6 | Pin7\n\r");
        printf("----|------|------|------|------|------|------|------|------\n\r");
        printf("  9 | %.4d | %.4d | %.4d | %.4d | %.4d | %.4d | %.4d | %.4d\n\r", adc_samples[0], adc_samples[1], 
                                                                                  adc_samples[2], adc_samples[3], 
                                                                                  adc_samples[4], adc_samples[5], 
                                                                                  adc_samples[6], adc_samples[7]);
        printf(" 10 | %.4d | %.4d | %.4d | %.4d | %.4d | %.4d | %.4d | %.4d\n\r", adc_samples[8], adc_samples[9], 
                                                                                  adc_samples[10], adc_samples[11], 
                                                                                  adc_samples[12], adc_samples[13], 
                                                                                  adc_samples[14], adc_samples[15]);
        printf(" 12 | %.4d | %.4d | %.4d | %.4d | %.4d | %.4d | %.4d | %.4d\n\r", adc_samples[16], adc_samples[17], 
                                                                                  adc_samples[18], adc_samples[19], 
                                                                                  adc_samples[20], adc_samples[21], 
                                                                                  adc_samples[22], adc_samples[23]);
        printf("------------------------------------------------------------\n\r");
    }
}


/* [] END OF FILE */
