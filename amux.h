/*****************************************************************************
* File Name  : amux.h
*
* Description: This file contains definitions of constants and structures for
*              the Analog mux (mux) implementation.
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
#ifndef AMUX_H_
#define AMUX_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "cy_pdl.h"

/*******************************************************************************
*                              Enumerated Types
*******************************************************************************/
typedef enum
{
    /** Select global analog mux A to establish the connections */
    AMUX_A = HSIOM_SEL_AMUXA,

    /** Select global analog mux B to establish the connections */
    AMUX_B = HSIOM_SEL_AMUXB,

} en_amux_select_t;

typedef enum
{
    /** Return success */
    AMUX_SUCCESS = 0u,

    /** Return error */
    AMUX_ERROR = 1u,

} en_amux_status_t;


/*******************************************************************************
*                                 API Constants
*******************************************************************************/
#ifndef AMUX_MAX_NUM_CONNECTIONS
    #define AMUX_MAX_NUM_CONNECTIONS       (32u)
#endif

#define AMUX_CONN_UNKNOWN              (0xFF)

/*******************************************************************************
*                              Type Definitions
*******************************************************************************/
/** Object Structure */
typedef struct
{
    en_amux_select_t amux_sel;
    volatile uint32_t connect_port[AMUX_MAX_NUM_CONNECTIONS];
    uint32_t connect_pin[AMUX_MAX_NUM_CONNECTIONS];
    uint8_t curr_conn;
    uint8_t num_conn;
    bool dma_en;
    DW_Type* dma_base;
    uint32_t dma_chan;
} amux_t;

/*******************************************************************************
*                            Function Prototypes
*******************************************************************************/
en_amux_status_t AMux_Init(amux_t *amux, en_amux_select_t amux_sel);
en_amux_status_t AMux_AddPort(amux_t *amux, GPIO_PRT_Type *port, uint8_t mask);
en_amux_status_t AMux_Connect(amux_t *amux, uint8_t index);
en_amux_status_t AMux_ConnectNext(amux_t *amux);
en_amux_status_t AMux_DisconnectAll(amux_t *amux);
en_amux_status_t AMux_SetupDMA(amux_t *amux, DW_Type *dma_base, uint32_t dma_chan);
en_amux_status_t AMux_StartDMA(amux_t *amux);
en_amux_status_t AMux_StopDMA(amux_t *amux);
void AMux_Deinit(amux_t *amux);


#endif /* AMUX_H_ */