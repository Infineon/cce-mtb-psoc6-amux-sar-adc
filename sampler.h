/*****************************************************************************
* File Name  : sampler.h
*
* Description: This file contains definitions of constants and structures for
*              the ADC Sampler with timer implementation.
*
*******************************************************************************
* Copyright 2020-2022, Cypress Semiconductor Corporation (an Infineon company) or
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
#ifndef SAMPLER_H_
#define SAMPLER_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "cy_pdl.h"

/*******************************************************************************
*                              Enumerated Types
*******************************************************************************/
typedef enum
{
    /** Return success */
    SAMPLER_SUCCESS = 0u,

    /** Return error */
    SAMPLER_ERROR = 1u,

} en_sampler_status_t;


/*******************************************************************************
*                                 API Constants
*******************************************************************************/
#ifndef SAMPLER_MAX_NUM_CHANNELS
    #define SAMPLER_MAX_NUM_CHANNELS       (32u)
#endif

/*******************************************************************************
*                              Type Definitions
*******************************************************************************/

/** Object Structure */
typedef struct
{
    TCPWM_Type *timer_base;
    uint8_t timer_chan;
    SAR_Type *sar_base;
    uint8_t num_channels;
    void *samples_ptr;
    DW_Type* dma_base;
    uint8_t dma_chan;

} sampler_t;

/*******************************************************************************
*                            Function Prototypes
*******************************************************************************/
en_sampler_status_t Sampler_Init(sampler_t *sampler, SAR_Type *sar, TCPWM_Type *timer, uint8_t timer_chan);
en_sampler_status_t Sampler_SetScanRate(sampler_t *sampler, uint32_t scan_rate_hz, uint32_t acq_time_ns);
en_sampler_status_t Sampler_Configure(sampler_t *sampler, uint8_t num_channels, int16_t *samples);
en_sampler_status_t Sampler_Start(sampler_t *sampler);
en_sampler_status_t Sampler_Stop(sampler_t *sampler);
en_sampler_status_t Sampler_SetupDMA(sampler_t *sampler, DW_Type *dma_base, uint32_t dma_chan);
void Sampler_Deinit(sampler_t *sampler);


#endif /* SAMPLER_H_ */