/*******************************************************************************
* File Name: amux.c
*
*  Description: This file contains the implementation of the Analog mux (amux)
*   designed to work with ADC or other analog components.
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

#include "amux.h"

/*******************************************************************************
* Constants
*******************************************************************************/

/*******************************************************************************
* Local Functions
*******************************************************************************/

/*******************************************************************************
* Global Variables
*******************************************************************************/
const uint32_t amux_all_zero = 0x00000000;

cy_stc_dma_descriptor_t amux_dma_descriptor[2*AMUX_MAX_NUM_CONNECTIONS];

const cy_stc_dma_descriptor_config_t amux_dma_descriptor_config = 
{
    .retrigger = CY_DMA_RETRIG_IM,
    .interruptType = CY_DMA_1ELEMENT,
    .triggerOutType = CY_DMA_DESCR,
    .channelState = CY_DMA_CHANNEL_ENABLED,
    .triggerInType = CY_DMA_DESCR_CHAIN,
    .dataSize = CY_DMA_WORD,
    .srcTransferSize = CY_DMA_TRANSFER_SIZE_WORD,
    .dstTransferSize = CY_DMA_TRANSFER_SIZE_WORD,
    .descriptorType = CY_DMA_SINGLE_TRANSFER,
    .srcAddress = NULL,
    .dstAddress = NULL,
    .srcXincrement = 0,
    .dstXincrement = 0,
    .xCount = 1,
    .srcYincrement = 0,
    .dstYincrement = 0,
    .yCount = 1,
    .nextDescriptor = NULL,
};

const cy_stc_dma_channel_config_t amux_dma_channel_config = 
{
    .descriptor = &amux_dma_descriptor[0],
    .preemptable = false,
    .priority = 3,
    .enable = false,
    .bufferable = false,
};

/*******************************************************************************
* Function Name: AMux_Init
********************************************************************************
* Summary:
*   Initialize an AMux object based on the given global analog mux.
*
* Parameters:
*   amux: AMux object
*   amux_sel: global amux.
*
* Return:
*   If initialized correctly, returns SUCCESS, otherwise ERROR.
*
*******************************************************************************/
en_amux_status_t AMux_Init(amux_t *amux, en_amux_select_t amux_sel)
{
    if (amux == NULL)
    {
        return AMUX_ERROR;
    }

    /* Set some structure variables to their initial values */
    amux->num_conn = 0;
    amux->curr_conn = AMUX_CONN_UNKNOWN;
    amux->dma_base = NULL;
    amux->dma_en = false;

    /* Check if Amux selection is correct */
    if ((amux_sel != AMUX_A) && (amux_sel != AMUX_B))
    {
        return AMUX_ERROR;
    }
    amux->amux_sel = amux_sel;

    return AMUX_SUCCESS;
}

/*******************************************************************************
* Function Name: AMux_Deinit
********************************************************************************
* Summary:
*   De-initialize an AMux object.
*
* Parameters:
*   amux: AMux object
*
*******************************************************************************/
void AMux_Deinit(amux_t *amux)
{
    if (amux == NULL)
    {
        return;
    }

    /* Disconnect all current pins from the AMux */
    AMux_DisconnectAll(amux);

    /* Denit any DMA channel used */
    if (amux->dma_base != NULL)
    {
        Cy_DMA_Channel_Disable(amux->dma_base, amux->dma_chan);
        Cy_DMA_Channel_DeInit(amux->dma_base, amux->dma_chan);
    }

    /* Set some structure variables to their initial values */
    amux->num_conn = 0;
    amux->curr_conn = AMUX_CONN_UNKNOWN;
    amux->dma_base = NULL;
    amux->dma_en = false;
}

/*******************************************************************************
* Function Name: AMux_AddPort
********************************************************************************
* Summary:
*   Add the pins from the given port to the list of connections to the analog 
*   mux.
*   If some of the pins in this port is not used, the pin must be configured to
*   work as a GPIO. No peripheral connection is allowed. 
*
* Parameters:
*   amux: AMux object
*   port: Port base to connect
*   mask: 8-bit mask indicating which pins from the port.
*
* Return:
*   If added correctly, returns SUCCESS, otherwise ERROR.
*
*******************************************************************************/
en_amux_status_t AMux_AddPort(amux_t *amux, GPIO_PRT_Type *port, uint8_t mask)
{
    uint32_t portNum;
    HSIOM_PRT_Type* portAddrHSIOM;

    if (amux == NULL || port == NULL || amux->dma_en == true)
    {
        return AMUX_ERROR;
    }   
     
    /* Extract the port number and port HSIOM address */
    portNum = ((uint32_t)(port) - CY_GPIO_BASE) / GPIO_PRT_SECTION_SIZE;
    portAddrHSIOM = (HSIOM_PRT_Type*)(CY_HSIOM_BASE + (HSIOM_PRT_SECTION_SIZE * portNum));

    /* Check the mask argument for which bits to connect */
    for (uint8_t pinNum = 0; pinNum < CY_GPIO_PINS_MAX; pinNum++)
    {
        /* Check if reached the number of connections*/
        if (amux->num_conn < AMUX_MAX_NUM_CONNECTIONS)
        {  
            /* Add the connections to the list */
            if ((mask & (1 << pinNum)) != 0)
            {
                /* Check which HSIOM register (SEL0 or SEL1) to use */
                if(pinNum < CY_GPIO_PRT_HALF)
                {
                    /* Set the HSIOM port address */
                    amux->connect_port[amux->num_conn] = (volatile uint32_t) &portAddrHSIOM->PORT_SEL0;
                    /* Set the value to write to HSIOM */
                    amux->connect_pin[amux->num_conn] = (amux->amux_sel << (8*pinNum));
                }
                else
                {
                    /* Set the HSIOM port address */
                    amux->connect_port[amux->num_conn] = (volatile uint32_t) &portAddrHSIOM->PORT_SEL1;
                    /* Set the value to write to HSIOM */
                    amux->connect_pin[amux->num_conn] = amux->amux_sel << (8*(pinNum-4));                
                }

                /* Increment the number of connections */
                amux->num_conn++;
            }
        }
        else 
        {
            /* No more connections allowed */
            return AMUX_ERROR;
        }   
    }

    /* Clear all connections from the given port to the analog mux */
    portAddrHSIOM->PORT_SEL0 = 0;
    portAddrHSIOM->PORT_SEL1 = 0;

    return AMUX_SUCCESS;
}

/*******************************************************************************
* Function Name: AMux_Connect
********************************************************************************
* Summary:
*   Connect the given pin to the Analog mux. Disconnect the current one.
*   The pin index is based on the order the pins were added with AMux_AddPort().
*
* Parameters:
*   amux: AMux object
*   index: index connection
*
* Return:
*   If connected correctly, returns SUCCESS, otherwise ERROR.
*
*******************************************************************************/
en_amux_status_t AMux_Connect(amux_t *amux, uint8_t index)
{
    if (amux == NULL || amux->dma_en == true)
    {
        return AMUX_ERROR;
    } 

    if (amux->curr_conn != AMUX_CONN_UNKNOWN)
    {
        /* Disconnect current pin */
        CY_SET_REG32(amux->connect_port[amux->curr_conn], 0);
    }
    else
    {
        /* If connection is unknown, disconnect all */
        AMux_DisconnectAll(amux);
    }

    /* Connect the given pin */
    CY_SET_REG32(amux->connect_port[index], amux->connect_pin[index]);

    /* Update current connection */
    amux->curr_conn = index;

    return AMUX_SUCCESS;
}

/*******************************************************************************
* Function Name: AMux_ConnectNext
********************************************************************************
* Summary:
*   Disconnect the current pin and connect the next one in the list.
*
* Paramters:
*   amux: AMux object
*
* Return:
*   If connected correctly, returns SUCCESS, otherwise ERROR.
*
*******************************************************************************/
en_amux_status_t AMux_ConnectNext(amux_t *amux)
{
    if (amux == NULL || amux->dma_en == true)
    {
        return AMUX_ERROR;
    } 

    /* If connection is unknown, disconnect all */
    if (amux->curr_conn == AMUX_CONN_UNKNOWN)
    {
        AMux_DisconnectAll(amux);
    }
    else
    {
        /* Disconnect current pin */
        CY_SET_REG32(amux->connect_port[amux->curr_conn], 0);
    }

    /* Update to the next pin and connect it */
    amux->curr_conn = (amux->curr_conn + 1) % amux->num_conn;
    CY_SET_REG32(amux->connect_port[amux->curr_conn], 
                 amux->connect_pin[amux->curr_conn]);


    return AMUX_SUCCESS;
}

/*******************************************************************************
* Function Name: AMux_DisconnectAll
********************************************************************************
* Summary:
*   Disconnect all the pins from AMux.
*
* Paramters:
*   amux: AMux object
*
* Return:
*   If disconnected correctly, returns SUCCESS, otherwise ERROR.
*
*******************************************************************************/
en_amux_status_t AMux_DisconnectAll(amux_t *amux)
{
    volatile uint32_t previous_port = 0;

    if (amux == NULL)
    {
        return AMUX_ERROR;
    } 

    /* Go over every connection and clear the port */
    for (int i = 0; i < amux->num_conn; i++)
    {
        /* Skip if previous port is the same */
        if (previous_port != amux->connect_port[i])
        {
            CY_SET_REG32(amux->connect_port[i], 0);
            previous_port = amux->connect_port[i];
        }
    }

    /* Set to the last pin, so the next is the first in the list*/
    amux->curr_conn = (amux->num_conn - 1);

    return AMUX_SUCCESS;
}

/*******************************************************************************
* Function Name: AMux_SetupDMA
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
en_amux_status_t AMux_SetupDMA(amux_t *amux, DW_Type *dma_base, uint32_t dma_chan)
{
    if (amux == NULL || dma_base == NULL || amux->dma_en == true)
    {
        return AMUX_ERROR;
    } 

    amux->dma_base = dma_base;
    amux->dma_chan = dma_chan;

    /* Initialize the DMA descriptor settings for each pin connection. 
     * Each pin requires two descriptors, one to clear and one to set */
    for (uint32_t i = 0; i < amux->num_conn; i++)
    {
        /* Setup the DMA descriptor to clear the connection */
        Cy_DMA_Descriptor_Init(&amux_dma_descriptor[2*i], &amux_dma_descriptor_config);
        if (i == 0)
        {
            Cy_DMA_Descriptor_SetDstAddress(&amux_dma_descriptor[2*i], (void *) amux->connect_port[amux->num_conn-1]);
        }
        else
        {
            Cy_DMA_Descriptor_SetDstAddress(&amux_dma_descriptor[2*i], (void *) amux->connect_port[i-1]);
        }
        Cy_DMA_Descriptor_SetSrcAddress(&amux_dma_descriptor[2*i], &amux_all_zero);
        Cy_DMA_Descriptor_SetNextDescriptor(&amux_dma_descriptor[2*i], &amux_dma_descriptor[2*i+1]);

        /* Setup the DMA descriptor to set the connection */
        Cy_DMA_Descriptor_Init(&amux_dma_descriptor[2*i+1], &amux_dma_descriptor_config);
        Cy_DMA_Descriptor_SetDstAddress(&amux_dma_descriptor[2*i+1], (void *) amux->connect_port[i]);
        Cy_DMA_Descriptor_SetSrcAddress(&amux_dma_descriptor[2*i+1], &amux->connect_pin[i]);
        Cy_DMA_Descriptor_SetTriggerInType(&amux_dma_descriptor[2*i+1], CY_DMA_1ELEMENT);
        if (i == (amux->num_conn - 1))
        {
            Cy_DMA_Descriptor_SetNextDescriptor(&amux_dma_descriptor[2*i+1], &amux_dma_descriptor[0]);
        }
        else
        {
            Cy_DMA_Descriptor_SetNextDescriptor(&amux_dma_descriptor[2*i+1], &amux_dma_descriptor[2*(i+1)]);
        }           
    }

    /* Disconnect all pins from the mux */
    AMux_DisconnectAll(amux);

    amux->curr_conn = AMUX_CONN_UNKNOWN;

    /* Initialize the DMA channel */
    Cy_DMA_Channel_Init(dma_base, dma_chan, &amux_dma_channel_config);

    return AMUX_SUCCESS;
}

/*******************************************************************************
* Function Name: AMux_StartDMA
********************************************************************************
* Summary:
*   Enable the DMAs internally to perform the mux. It setups to start from the
*   initial pin added to the analog mux.
*
* Paramters:
*   amux: AMux object
*
* Return:
*   If started correctly, returns SUCCESS, otherwise ERROR.
*
*******************************************************************************/
en_amux_status_t AMux_StartDMA(amux_t *amux)
{
    if (amux == NULL || amux->dma_base == NULL)
    {
        return AMUX_ERROR;
    } 

    amux->dma_en = true;

    Cy_DMA_Channel_SetDescriptor(amux->dma_base, amux->dma_chan, 
                                &amux_dma_descriptor[0]);
    Cy_DMA_Channel_Enable(amux->dma_base, amux->dma_chan);
    Cy_DMA_Enable(amux->dma_base);

    return AMUX_SUCCESS;
}

/*******************************************************************************
* Function Name: AMux_StopDMA
********************************************************************************
* Summary:
*   Disable the DMAs internally.
*
* Paramters:
*   amux: AMux object
*
* Return:
*   If stopped correctly, returns SUCCESS, otherwise ERROR.
*
*******************************************************************************/
en_amux_status_t AMux_StopDMA(amux_t *amux)
{
    if (amux == NULL || amux->dma_base == NULL)
    {
        return AMUX_ERROR;
    } 

    amux->dma_en = false;

    Cy_DMA_Channel_Disable(amux->dma_base, amux->dma_chan);

    return AMUX_SUCCESS;
}


/* [] END OF FILE */