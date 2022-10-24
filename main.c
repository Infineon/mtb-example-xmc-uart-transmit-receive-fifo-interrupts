/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the XMC MCU: UART Transmit Receive
*              FIFO Interrupts Example for ModusToolbox. This example shows 
*              how to use TX and RX FIFO limit interrupts and send data from
*              TX to RX. If reception is successful the onboard LED 1 will be
*              switched on, otherwise it will remains switched off. 
*
* Related Document: See README.md
*
******************************************************************************
*
* Copyright (c) 2015-2021, Infineon Technologies AG
* All rights reserved.                        
*                                             
* Boost Software License - Version 1.0 - August 17th, 2003
* 
* Permission is hereby granted, free of charge, to any person or organization
* obtaining a copy of the software and accompanying documentation covered by
* this license (the "Software") to use, reproduce, display, distribute,
* execute, and transmit the Software, and to prepare derivative works of the
* Software, and to permit third-parties to whom the Software is furnished to
* do so, all subject to the following:
* 
* The copyright notices in the Software and this entire statement, including
* the above license grant, this restriction and the following disclaimer,
* must be included in all copies of the Software, in whole or in part, and
* all derivative works of the Software, unless such copies or derivative
* works are solely in the form of machine-executable object code generated by
* a source language processor.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
* SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
* FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*                                                                              
*****************************************************************************/

#include "cybsp.h"
#include "cy_utils.h"
#include "xmc_gpio.h"
#include "xmc_uart.h"
#include "cycfg_peripherals.h"

/*******************************************************************************
* Defines
*******************************************************************************/
/* Bytes of data to be transmitted */
#define NUM_DATA                        9

/* Set interrupt priority for the USIC0_0_IRQn */
#define USIC0_0_IRQn_PRIORITY           63

/* Set interrupt priority for the USIC0_1_IRQn */
#define USIC0_1_IRQn_PRIORITY           62

#if (UC_SERIES == XMC14)
/* Set bit  */
#define GPIO_OUTPUT_LEVEL_HIGH          0x10000U 
/* Reset bit */
#define  GPIO_OUTPUT_LEVEL_LOW          0x1U
#endif

#if (UC_SERIES == XMC47)
/* Reset bit */
#define GPIO_OUTPUT_LEVEL_LOW           0x10000U 
/* Set bit  */  
#define  GPIO_OUTPUT_LEVEL_HIGH         0x1U 
#endif

/*******************************************************************************
*  Global Variables
*******************************************************************************/
/* TX buffer index */
uint32_t tx_index = 0;

/* RX buffer index */
uint32_t rx_index = 0;

/* Flag to set when Rx index equals the total data transmitted */
volatile uint32_t flag = 0;

/* Array for storing the data to be transmitted */
uint8_t tx_data[NUM_DATA];

/* Array for storing the received data */
uint8_t rx_data[NUM_DATA];

/*******************************************************************************
* Function Name: USIC0_0_IRQHandler
********************************************************************************
* Summary:
* Transmit IRQ Handler. The function called everytime the number of elements 
* in the TX FIFO reduces below TX FIFO Limit (set to one). The function is used 
* to fill the TX FIFO with the next element in the tx_data buffer.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void USIC0_0_IRQHandler(void)
{
    /* If still remaining data to be send */
    if(tx_index < NUM_DATA)
    {
        /* Wait if the TX FIFO is full */
        while(XMC_USIC_CH_TXFIFO_IsFull(CYBSP_DEBUG_UART_HW));
    
        /* Fill the TX FIFO with the next element in the tx_data buffer */
        XMC_UART_CH_Transmit(CYBSP_DEBUG_UART_HW, tx_data[tx_index]);
        tx_index++;
    }
    else
    {
        /* Disable the TX FIFO Event when all the data in the tx_data
         * buffer has been transmitted
         */
        XMC_USIC_CH_TXFIFO_DisableEvent(CYBSP_DEBUG_UART_HW, 
                                        XMC_USIC_CH_TXFIFO_EVENT_CONF_STANDARD);
        NVIC_DisableIRQ(USIC0_0_IRQn);
    }
}

/*******************************************************************************
* Function Name: USIC0_0_IRQHandler
********************************************************************************
* Summary:
* Receive handling IRQ . The function called everytime the number of elements
* in the RX FIFO exceeds above Rx FIFO Limit (set to seven). 
* The function is used to read the RX FIFO until it is empty and set a flag
* when all the data has been received.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void USIC0_1_IRQHandler(void) 
{
    /* Read the RX FIFO till it is empty */
    while(!XMC_USIC_CH_RXFIFO_IsEmpty(CYBSP_DEBUG_UART_HW))
    {
        rx_data[rx_index++] = XMC_UART_CH_GetReceivedData(CYBSP_DEBUG_UART_HW);
    }

    /* If all the data have been received */
    if(rx_index == (NUM_DATA))
    {
        flag = 1;
    }

    /* If the remaining data to be received is smaller than the initial
     * RX FIFO limit, then the RX FIFO limit is modified to the remaining data 
     * minus 1 in order to trigger interrupt when we have all the data received 
     */
    if((NUM_DATA - rx_index) < CYBSP_DEBUG_UART_RXFIFO_LIMIT)
    {
        XMC_USIC_CH_RXFIFO_SetSizeTriggerLimit(CYBSP_DEBUG_UART_HW, XMC_USIC_CH_FIFO_SIZE_8WORDS, (NUM_DATA - rx_index) - 1);
    }
}

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function. It performs the following tasks:
* 1. Initial setup of device.
* 2. Starts the UART peripheral
* 3. Fills the TX FIFO for the first time
* 4. Check if the data transmitted is equal to the data received.
*    LED is switched ON in case of successful reception.
*
* Parameters:
*  none
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
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Filling the TX array */
    for (uint32_t i = 0; i < NUM_DATA; i++)
    {
        tx_data[i] = i;
    }

    /* Configuring priority and enabling NVIC IRQ 
     * for the defined Service Request line number 
     */
    NVIC_SetPriority(USIC0_0_IRQn, USIC0_0_IRQn_PRIORITY);
    NVIC_EnableIRQ(USIC0_0_IRQn);
    NVIC_EnableIRQ(USIC0_1_IRQn);
    NVIC_SetPriority(USIC0_1_IRQn, USIC0_1_IRQn_PRIORITY);

    /* Start the UART peripheral */ 
    XMC_UART_CH_Start(CYBSP_DEBUG_UART_HW);

    /* Wait until the TX FIFO is has some space to accommodate more data */
    while(XMC_USIC_CH_TXFIFO_IsFull (CYBSP_DEBUG_UART_HW));

    /* Filling the the first time TX FIFO. Successive fillings will be done in the TX FIFO IRQ */
    XMC_UART_CH_Transmit(CYBSP_DEBUG_UART_HW, tx_data[tx_index++]);

    /* If the total data data to be transmitted is smaller than the initial RX FIFO Limit
     * then RX FIFO limit is modified to the total data to be transmitted minus 1 in order 
     * react when we have all the data received 
     */
    if((NUM_DATA - rx_index) < CYBSP_DEBUG_UART_RXFIFO_LIMIT)
    {
        XMC_USIC_CH_RXFIFO_SetSizeTriggerLimit(CYBSP_DEBUG_UART_HW, XMC_USIC_CH_FIFO_SIZE_8WORDS, (NUM_DATA - rx_index) - 1);
    }

    while(1)
    {
        /* Infinite loop */
        if (flag == 1)
        {
            /* Check if every received data match with the transmitted data */
            for (int tmp = 0; tmp < NUM_DATA; tmp++)
            {
                /* If reception fails stays in an infinite while loop and switch off the LED */
                if (tx_data[tmp] != rx_data[tmp])
                {
                    XMC_GPIO_SetOutputLevel(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN, GPIO_OUTPUT_LEVEL_LOW);
                }
                /* If reception is successful turn on the LED */
                else
                {
                    XMC_GPIO_SetOutputLevel(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN, GPIO_OUTPUT_LEVEL_HIGH);
                }
            }

            /* Reset the flag to zero */
            flag = 0;
        }
    }
}

/* [] END OF FILE */
