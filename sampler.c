/*******************************************************************************
* File Name: sampler.c
*
*  Description: This file contains the implementation of the ADC helper
*   designed as a wrapper of the SAR ADC.
*
******************************************************************************
* (c) 2023, Cypress Semiconductor Corporation. All rights reserved.
*******************************************************************************
* This software, including source code, documentation and related materials
* ("Software"), is owned by Cypress Semiconductor Corporation or one of its
* subsidiaries ("Cypress") and is protected by and subject to worldwide patent
* protection (United States and foreign), United States copyright laws and
* international treaty provisions. Therefore, you may use this Software only
* as provided in the license agreement accompanying the software package from
* which you obtained this Software ("EULA").
*
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software source
* code solely for use in connection with Cypress's integrated circuit products.
* Any reproduction, modification, translation, compilation, or representation
* of this Software except as specified above is prohibited without the express
* written permission of Cypress.
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
* including Cypress's product in a High Risk Product, the manufacturer of such
* system or application assumes all risk of such use and in doing so agrees to
* indemnify Cypress against all liability.
*****************************************************************************/

#include "sampler.h"
#include "cyhal.h"

/*******************************************************************************
* Constants
*******************************************************************************/

/*******************************************************************************
* Local Functions
*******************************************************************************/

/*******************************************************************************
* Global Variables
*******************************************************************************/
cy_stc_dma_descriptor_t sampler_dma_descriptor;

const cy_stc_tcpwm_counter_config_t sampler_timer_config = 
{
    .period = 32768,
    .clockPrescaler = CY_TCPWM_COUNTER_PRESCALER_DIVBY_1,
    .runMode = CY_TCPWM_COUNTER_CONTINUOUS,
    .countDirection = CY_TCPWM_COUNTER_COUNT_UP,
    .compareOrCapture = CY_TCPWM_COUNTER_MODE_COMPARE,
    .compare0 = 16384,
    .compare1 = 16384,
    .enableCompareSwap = false,
    .interruptSources = CY_TCPWM_INT_NONE,
    .captureInputMode = 0x3U,
    .captureInput = CY_TCPWM_INPUT_0,
    .reloadInputMode = 0x3U,
    .reloadInput = CY_TCPWM_INPUT_0,
    .startInputMode = 0x3U,
    .startInput = CY_TCPWM_INPUT_0,
    .stopInputMode = 0x3U,
    .stopInput = CY_TCPWM_INPUT_0,
    .countInputMode = 0x3U,
    .countInput = CY_TCPWM_INPUT_1,
};

const cy_stc_dma_descriptor_config_t sampler_dma_descriptor_config = 
{
    .retrigger = CY_DMA_RETRIG_IM,
    .interruptType = CY_DMA_1ELEMENT,
    .triggerOutType = CY_DMA_1ELEMENT,
    .channelState = CY_DMA_CHANNEL_ENABLED,
    .triggerInType = CY_DMA_1ELEMENT,
    .dataSize = CY_DMA_HALFWORD,
    .srcTransferSize = CY_DMA_TRANSFER_SIZE_WORD,
    .dstTransferSize = CY_DMA_TRANSFER_SIZE_DATA,
    .descriptorType = CY_DMA_1D_TRANSFER,
    .srcAddress = NULL,
    .dstAddress = NULL,
    .srcXincrement = 0,
    .dstXincrement = 1,
    .xCount = 2,
    .srcYincrement = 0,
    .dstYincrement = 0,
    .yCount = 1,
    .nextDescriptor = &sampler_dma_descriptor,
};

const cy_stc_dma_channel_config_t sampler_dma_channel_config = 
{
    .descriptor = &sampler_dma_descriptor,
    .preemptable = false,
    .priority = 3,
    .enable = false,
    .bufferable = false,
};

/*******************************************************************************
* Function Name: Sampler_Init
********************************************************************************
* Summary:
*   Initialize an Sampler object using the given SAR ADC and timer. The timer
*   has to trigger the SAR ADC conversion on overflow. It also assumes the timer
*   runs based on the maximum peripheral clock frequency, typically 100 MHz.
*   The SAR ADC has to be initialized by the application with at least one channel. 
*   This middleware only look at the first channel of the SAR ADC. Any additional
*   SAR ADC channel shall be handled externally.
*
* Parameters:
*   sampler: sampler object
*   sar: SAR ADC base pointer
*   timer: TCPWM base pointer
*   timer_ch: TCPWM channel
*
* Return:
*   If initialized correctly, returns SUCCESS, otherwise ERROR.
*
*******************************************************************************/
en_sampler_status_t Sampler_Init(sampler_t *sampler, SAR_Type *sar, 
                                 TCPWM_Type *timer, uint8_t timer_chan)
{
    if (sampler == NULL || sar == NULL || timer == NULL)
    {
        return SAMPLER_ERROR;
    }

    /* Initialize the timer */
    if ( CY_TCPWM_SUCCESS != Cy_TCPWM_Counter_Init(timer, timer_chan, &sampler_timer_config))
    {
        return SAMPLER_ERROR;
    }

    /* Set to default initial values */
    sampler->num_channels = 0;
    sampler->dma_base = NULL;
    sampler->samples_ptr = NULL;

    /* Set values based on the arguments */
    sampler->sar_base = sar;
    sampler->timer_base = timer;
    sampler->timer_chan = timer_chan;

    return SAMPLER_SUCCESS;
}

/*******************************************************************************
* Function Name: Sampler_Deinit
********************************************************************************
* Summary:
*   De-initialize an sampler object.
*
* Parameters:
*   sampler_t: Sampler object
*
*******************************************************************************/
void Sampler_Deinit(sampler_t *sampler)
{
    if (sampler == NULL || sampler->timer_base == NULL)
    {
        return;
    }

    /* Deinit internal timer */
    Cy_TCPWM_Counter_Disable(sampler->timer_base, sampler->timer_chan);
    Cy_TCPWM_Counter_DeInit(sampler->timer_base, sampler->timer_chan, &sampler_timer_config);

    /* Denit any DMA channel used */
    if (sampler->dma_base != NULL)
    {
        Cy_DMA_Channel_Disable(sampler->dma_base, sampler->dma_chan);
        Cy_DMA_Channel_DeInit(sampler->dma_base, sampler->dma_chan);
    }

    /* Set to default initial values */
    sampler->num_channels = 0;
    sampler->dma_base = NULL;
    sampler->samples_ptr = NULL;
    sampler->timer_base = NULL;

}

/*******************************************************************************
* Function Name: Sampler_SetScanRate
********************************************************************************
* Summary:
*   Set how often to trigger the SAR ADC. It configures the internal timer to
*   trigger the SAR ADC at this rate. The acquisition time is also provided to
*   tell how long it takes to acquire one sample. This information can be 
*   extracted from the device configurator in the SAR ADC personality. This 
*   information is used in the timer, which generates an external signal for 
*   a analog mux, for example. 
*
* Parameters:
*   sampler: sampler object
*   scan_rate_hz: scan rate in hertz
*   acq_time_ns: acquisition time in nanoseconds
*
* Return:
*   If set correctly, returns SUCCESS, otherwise ERROR.
*
*******************************************************************************/
en_sampler_status_t Sampler_SetScanRate(sampler_t *sampler, uint32_t scan_rate_hz, 
                                                            uint32_t acq_time_ns)
{
    uint32_t timer_clk_hz;
    uint32_t timer_period;
    uint32_t timer_compare;

    if (sampler == NULL || sampler->sar_base == NULL || sampler->timer_base == NULL)
    {
        return SAMPLER_ERROR;
    }   
     
    /* Get peripheral clock frequency (assumed to be same as timer clock) */
    timer_clk_hz = cyhal_clock_get_frequency(&CYHAL_CLOCK_PERI);

    if (timer_clk_hz == 0 || scan_rate_hz == 0)
    {
        return SAMPLER_ERROR;
    }

    timer_period = timer_clk_hz/scan_rate_hz;
    timer_compare = (((timer_clk_hz+500000)/1000000) * acq_time_ns)/1000;

    Cy_TCPWM_Counter_SetPeriod(sampler->timer_base, sampler->timer_chan, timer_period);
    Cy_TCPWM_Counter_SetCompare0(sampler->timer_base, sampler->timer_chan, timer_compare);

    return SAMPLER_SUCCESS;
}

/*******************************************************************************
* Function Name: Sampler_Configure
********************************************************************************
* Summary:
*   Configure the number of samples to acquire and where to place them.
*   The data is provided as 16-bits and the array storing should be allocated
*   by the application. It shall be large enough to accomodate the desired
*   number of channels.
*
* Parameters:
*   sampler: sampler object
*   num_channels: number of channels
*   samples: array with samples to be stored.
*
* Return:
*   If connected correctly, returns SUCCESS, otherwise ERROR.
*
*******************************************************************************/
en_sampler_status_t Sampler_Configure(sampler_t *sampler, uint8_t num_channels, 
                                                          int16_t *samples)
{
    if (sampler == NULL)
    {
        return SAMPLER_ERROR;
    } 

    if (num_channels > SAMPLER_MAX_NUM_CHANNELS)
    {
        return SAMPLER_ERROR;
    }

    sampler->samples_ptr = samples;
    sampler->num_channels = num_channels;

    return SAMPLER_SUCCESS;
}

/*******************************************************************************
* Function Name: Sampler_Start
********************************************************************************
* Summary:
*   Start sampling by enabling the internal timer and SAR ADC.
*
* Paramters:
*   sampler: sampler object
*
* Return:
*   If connected correctly, returns SUCCESS, otherwise ERROR.
*
*******************************************************************************/
en_sampler_status_t Sampler_Start(sampler_t *sampler)
{
    if (sampler == NULL || sampler->sar_base == NULL || sampler->timer_base == NULL)
    {
        return SAMPLER_ERROR;
    } 

    Cy_SAR_Enable(sampler->sar_base);
    Cy_DMA_Channel_Enable(sampler->dma_base, sampler->dma_chan);
    Cy_DMA_Enable(sampler->dma_base);
    Cy_TCPWM_Counter_SetCounter(sampler->timer_base, sampler->timer_chan, 0);
    Cy_TCPWM_Counter_Enable(sampler->timer_base, sampler->timer_chan);
    Cy_TCPWM_TriggerStart_Single(sampler->timer_base, sampler->timer_chan);

    return SAMPLER_SUCCESS;
}

/*******************************************************************************
* Function Name: Sampler_Stop
********************************************************************************
* Summary:
*   Stop sampling by disabling the internal timer and SAR ADC.
*
* Paramters:
*   sampler_t: sampler object
*
* Return:
*   If disconnected correctly, returns SUCCESS, otherwise ERROR.
*
*******************************************************************************/
en_sampler_status_t Sampler_Stop(sampler_t *sampler)
{
    if (sampler == NULL || sampler->sar_base == NULL || sampler->timer_base == NULL)
    {
        return SAMPLER_ERROR;
    } 

    Cy_DMA_Channel_Disable(sampler->dma_base, sampler->dma_chan);
    Cy_SAR_Disable(sampler->sar_base);
    Cy_TCPWM_Counter_Disable(sampler->timer_base, sampler->timer_chan);

    return SAMPLER_SUCCESS;
}

/*******************************************************************************
* Function Name: Sampler_SetupDMA
********************************************************************************
* Summary:
*   Setup a DMA to change the AMux connections without the CPU. 
*   This function shall only be called after AMux_AddPort() was executed for 
*   all connections.
*
* Parameters:
*   amux: AMux object
*   dma_base: DW base
*   dma_chan: DW channel
*
* Return:
*   If setup correctly, returns SUCCESS, otherwise ERROR.
*
*******************************************************************************/
en_sampler_status_t Sampler_SetupDMA(sampler_t *sampler, DW_Type *dma_base, uint32_t dma_chan)
{
    if (sampler == NULL || sampler->sar_base == NULL || sampler->timer_base == NULL)
    {
        return SAMPLER_ERROR;
    } 

    sampler->dma_base = dma_base;
    sampler->dma_chan = dma_chan;

    /* Initialize the DMA Descriptor */
    Cy_DMA_Descriptor_Init(&sampler_dma_descriptor, &sampler_dma_descriptor_config);
    Cy_DMA_Descriptor_SetDstAddress(&sampler_dma_descriptor, (void *) sampler->samples_ptr);
    Cy_DMA_Descriptor_SetSrcAddress(&sampler_dma_descriptor, (void *) &sampler->sar_base->CHAN_RESULT[0]);
    Cy_DMA_Descriptor_SetXloopDataCount(&sampler_dma_descriptor, sampler->num_channels);

    /* Initialize the DMA channel */
    Cy_DMA_Channel_Init(dma_base, dma_chan, &sampler_dma_channel_config);

    return SAMPLER_SUCCESS;
}


/* [] END OF FILE */