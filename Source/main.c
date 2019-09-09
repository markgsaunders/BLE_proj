/***************************************************************************//**
* \file main.c
* \version 1.0
*
* \brief
* Template for development with the CY8CPROTO-063-BLE kit.
*
********************************************************************************
* \copyright
* Copyright 2017-2019 Cypress Semiconductor Corporation
* SPDX-License-Identifier: Apache-2.0
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include "cy_device_headers.h"
#include "cy_pdl.h"
#include "cycfg.h"

#include "cy_ble_gatt.h"
#include "cycfg_ble.h"

#include "stdio.h"


/* Convenient defines for interrupt levels */
#define ISR_PRIORITY_LOW		3
#define ISR_PRIORITY_MEDIUM		2
#define ISR_PRIORITY_HIGH		1

cy_stc_scb_uart_context_t KIT_UART_context;

static cy_stc_ble_conn_handle_t conn_handle;

void button_isr( void )
{
	/* Clear the interrupt */
	Cy_GPIO_ClearInterrupt( KIT_BTN1_PORT, KIT_BTN1_PIN );
	NVIC_ClearPendingIRQ( KIT_BTN1_IRQ );

	if( Cy_BLE_GetConnectionState( conn_handle ) == CY_BLE_CONN_STATE_CONNECTED )
	{
		uint8_t battery_level;
		uint8_t notify;

		/* Read the current battery level from GATT */
		Cy_BLE_BASS_GetCharacteristicValue( CY_BLE_BATTERY_SERVICE_INDEX, CY_BLE_BAS_BATTERY_LEVEL, sizeof( battery_level ), &battery_level );

		/* Decrement the level */
		battery_level--;
		if( battery_level == (uint8_t)(-1) )
			battery_level = 100;

		/* Write the new level back to GATT */
		Cy_BLE_BASS_SetCharacteristicValue( CY_BLE_BATTERY_SERVICE_INDEX, CY_BLE_BAS_BATTERY_LEVEL, sizeof( battery_level ), &battery_level );

		/* Is notification enabled? If yes, tell the central */
		Cy_BLE_BASS_GetCharacteristicDescriptor( conn_handle, CY_BLE_BATTERY_SERVICE_INDEX, CY_BLE_BAS_BATTERY_LEVEL, CY_BLE_BAS_BATTERY_LEVEL_CCCD, sizeof( notify ), &notify );
		if( notify )
		{
			Cy_BLE_BASS_SendNotification( conn_handle, CY_BLE_BATTERY_SERVICE_INDEX, CY_BLE_BAS_BATTERY_LEVEL, sizeof( battery_level ), &battery_level );
		}
	}
}

void alert( uint8_t level )
{
    Cy_SCB_UART_PutString( KIT_UART_HW, "Alert " );

	switch( level )
	{
		case CY_BLE_NO_ALERT:		/* Both LEDs off */
			Cy_SCB_UART_PutString( KIT_UART_HW, "OFF\r\n" );
			Cy_GPIO_Set( KIT_LED1_PORT, KIT_LED1_PIN );
			Cy_GPIO_Set( KIT_LED2_PORT, KIT_LED2_PIN );
		break;

		case CY_BLE_MILD_ALERT:		/* LED1 off, LED2 on */
			Cy_SCB_UART_PutString( KIT_UART_HW, "MILD\r\n" );
			Cy_GPIO_Set( KIT_LED1_PORT, KIT_LED1_PIN );
			Cy_GPIO_Clr( KIT_LED2_PORT, KIT_LED2_PIN );
		break;

		case CY_BLE_HIGH_ALERT:		/* LED1 on, LED2 off */
			Cy_SCB_UART_PutString( KIT_UART_HW, "HIGH\r\n" );
			Cy_GPIO_Clr( KIT_LED1_PORT, KIT_LED1_PIN );
			Cy_GPIO_Set( KIT_LED2_PORT, KIT_LED2_PIN );
		break;

		default:					/* Both LEDs on - should never occur */
			Cy_SCB_UART_PutString( KIT_UART_HW, "ERROR\r\n" );
			Cy_GPIO_Clr( KIT_LED1_PORT, KIT_LED1_PIN );
			Cy_GPIO_Clr( KIT_LED2_PORT, KIT_LED2_PIN );
		break;
	}
}

void stack_handler( uint32_t event, void* eventParam )
{
	char passkey_str[50];

    switch( event )
    {
        case CY_BLE_EVT_STACK_ON:
            Cy_SCB_UART_PutString( KIT_UART_HW, "Stack on\r\n" );
            Cy_BLE_GAPP_StartAdvertisement( CY_BLE_ADVERTISING_FAST, CY_BLE_PERIPHERAL_CONFIGURATION_0_INDEX );
        break;

        case CY_BLE_EVT_GATT_CONNECT_IND:
            Cy_SCB_UART_PutString( KIT_UART_HW, "Connected\r\n" );
            conn_handle = *(cy_stc_ble_conn_handle_t *)eventParam;
        break;

        case CY_BLE_EVT_GAP_DEVICE_DISCONNECTED:
            Cy_SCB_UART_PutString( KIT_UART_HW, "Disconnected\r\n" );
            Cy_BLE_GAPP_StartAdvertisement( CY_BLE_ADVERTISING_FAST, CY_BLE_PERIPHERAL_CONFIGURATION_0_INDEX );
            alert( CY_BLE_NO_ALERT );
            conn_handle.attId = 0;
            conn_handle.bdHandle = 0;
        break;

		case CY_BLE_EVT_GAP_AUTH_REQ:
			Cy_SCB_UART_PutString( KIT_UART_HW, "Authenticating\r\n" );

			/* Send the authentication settings set by the BLE Configurator - GAP Settings */
			Cy_BLE_GAPP_AuthReqReply( &cy_ble_config.authInfo[CY_BLE_SECURITY_CONFIGURATION_0_INDEX] );
		break;

		case CY_BLE_EVT_GAP_PASSKEY_DISPLAY_REQUEST:
			sprintf( passkey_str, "Passkey requested\tKey = %06ld\r\n", ( (cy_stc_ble_gap_auth_pk_info_t*)eventParam )->passkey );
			Cy_SCB_UART_PutString( KIT_UART_HW, passkey_str );
		break;

		case CY_BLE_EVT_GAP_AUTH_COMPLETE:
			Cy_SCB_UART_PutString( KIT_UART_HW, "Authentication complete\r\n" );
		break;

		case CY_BLE_EVT_GAP_AUTH_FAILED:
			Cy_SCB_UART_PutString( KIT_UART_HW, "Authentication fail\r\n" );
		break;

        default:
            /* Ignore the event */
        break;
    }
}

void IAS_handler( uint32_t event, void* eventParam )
{
	uint8_t alertLevel;

	/* Alert Level Characteristic write event */
	if( event == CY_BLE_EVT_IASS_WRITE_CHAR_CMD )
	{
		/* Read the updated Alert Level value from the GATT database */
		Cy_BLE_IASS_GetCharacteristicValue( CY_BLE_IAS_ALERT_LEVEL, sizeof( alertLevel ), &alertLevel );

		alert( alertLevel );
	}
}

void BAS_handler( uint32_t event, void* eventParam )
{
	Cy_SCB_UART_PutString( KIT_UART_HW, "Notification " );
	Cy_SCB_UART_PutString( KIT_UART_HW, ( CY_BLE_EVT_BASS_NOTIFICATION_ENABLED == event ) ? "on\r\n" : "off\r\n" );
}

int main(void)
{
	/* Set up the device based on configurator selections */
	init_cycfg_all();

	Cy_SCB_UART_Init( KIT_UART_HW, &KIT_UART_config, &KIT_UART_context );
	Cy_SCB_UART_Enable( KIT_UART_HW );
	Cy_SCB_UART_PutString( KIT_UART_HW, "\r\n\n*** Application Started ***\r\n" );

	/* Turn on the button interrupt */
	const cy_stc_sysint_t button_intr_config = { KIT_BTN1_IRQ, ISR_PRIORITY_LOW };

	Cy_SysInt_Init( &button_intr_config, button_isr );
	NVIC_EnableIRQ(  button_intr_config.intrSrc );

	const cy_stc_sysint_t ble_intr_config = { bless_interrupt_IRQn, ISR_PRIORITY_HIGH };
	Cy_SysInt_Init( &ble_intr_config, Cy_BLE_BlessIsrHandler );
	NVIC_EnableIRQ(  ble_intr_config.intrSrc );

	Cy_BLE_RegisterEventCallback( stack_handler );

	Cy_BLE_IAS_RegisterAttrCallback( IAS_handler );

	Cy_BLE_BAS_RegisterAttrCallback( BAS_handler );

	/* Initialize and start BLE */
	Cy_BLE_Init( &cy_ble_config );
	Cy_BLE_Enable();

	__enable_irq();

	for( ; ; )
	{
	    Cy_BLE_ProcessEvents();
	}
}
