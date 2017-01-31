/*
===============================================================================
 Name        : AB_Test.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
*/


#include "chip.h"

// TODO: insert other include files here
#include <cr_section_macros.h>
#include "Helianthus21_utils.h"
#include "Helianthus21_modes.h"


// TODO: insert other definitions and declarations here
#define TICKRATE_HZ1 (3) /* 10 ticks per second */
#define TICKRATE_HZ2 (4) /* 11 ticks per second */
//#define TEST_CCAN_BAUD_RATE 500000
#define TEST_CCAN_BAUD_RATE 125000
//user must define unique MPPT_ID for each MPPT
//#define MPPT_TRACK_TRANSMIT_ID 0x410 //MPPT#1
//#define MPPT_RECEIVE 0x300
#define CAN_SEND_WAIT 5


const uint16_t coutVoltage_limit=135;// 135V max output voltage
const uint16_t cinVoltage_limit=50;// 50V max input voltage -AB 6/17/2015
const uint16_t Current_limit=6 ;//Protect the 8A fuse



uint8_t CAN_send_count=0;
uint8_t Dynamic_count=0;

uint16_t max_duty_cycle=400;
uint8_t min_duty_cycle=0;
uint16_t delta_voltage= 3;//50 corresponds to 2.465V at the input
uint8_t go_towards_higher_current=1;
uint8_t fail_count=0;
uint8_t tracking_speed=0;


uint8_t Track=1;
uint8_t data[8];
uint8_t receive_data[8];
uint8_t wait_for_tx_finished=0;


//CCAN_MSG_OBJ_T receive_obj;
CCAN_MSG_OBJ_T transmit_obj;
uint16_t duty_cycle=600;// Think of a good guess here -AB 1/8/2015

uint16_t int_input_voltage=0;
float f_input_voltage=0;
uint16_t int_input_current=0;
float f_input_current=0;

uint16_t int_output_voltage=0;
float f_output_voltage=0;
uint16_t int_output_current=0;
float f_output_current=0;

uint16_t int_12V_voltage=0;
float f_12V_voltage=0;
uint16_t int_12V_current=0;
float f_12V_current=0;

uint8_t Arrayside_over=0;
uint8_t Batteryside_over=0;

uint8_t MPPT_mode=0;
uint8_t message_num=0;
uint8_t dlc=8;

uint16_t Dynamic_iteration=0;
uint16_t dynamic_max_duty_power=0;
unsigned long dynamic_max_previous_power=0;
#define DYNAMIC_PERIODIC_DELAY 24
//uint8_t dynamic_periodic_delay=12;



int main(void)
{

//	receive_obj.mode_id=CAN_MSGOBJ_STD|MPPT_RECEIVE;
//	receive_obj.mask=0x4;
//	receive_obj.msgobj=1;

//	transmit_obj.mode_id=CAN_MSGOBJ_STD|MPPT_TRACK_TRANSMIT_ID;
//	transmit_obj.mask=0x0;
//	transmit_obj.msgobj=2;

    SystemCoreClockUpdate(); //System clock at 48 MHz -AB 1/7/2015

    duty_cycle=max_duty_cycle*0.25;
    Configure_MPPT(duty_cycle);

    //float test_message=65.65;

    wait_for_tx_finished=0;

    //Relay_OPEN();

    receive_data[0]=0x0;//This is the default value so we can have static alg as default -AB 6/17/2015

    //TODO: compare the difference between this code and ccan_romc code

    Check_Voltage_Current_at_Input_Output();
   	while(1)
   	{

   		Check_Voltage_Current_at_Input_Output();

    	if(Track==1)
    	{

    		//Send_Status_to_CCS(receive_data[0]);
    		//Send_Status_to_CCS();
    		switch(receive_data[0])
    		{
    			case 0:
    				MPPT_mode=0;

    				track_MPP_static();
    				//Send_Data_to_CCS(STATIC_);

      				break;
    			case 1:
    				MPPT_mode=1;
    				if (Dynamic_count>DYNAMIC_PERIODIC_DELAY)
    				{
						Dynamic_iteration=1;
    				}
    				track_MPP_dynamic();
    				//Send_Data_to_CCS(DYNAMIC_);
    				break;
    			case 2:
    				MPPT_mode=2;
    				//Disable the CAN sending interrupt
    				//NVIC_DisableIRQ(TIMER_16_1_IRQn);
    				IV_trace();
    				//Enable the CAN sending interrupt
    				//NVIC_EnableIRQ(TIMER_16_1_IRQn);
    				receive_data[0]=0;
    				duty_cycle=0.75*max_duty_cycle;
    				Set_PWM_Duty_Cycle(duty_cycle);
    				break;
    			case 3:
    				//OPEN mode
    				MPPT_mode=3;
    				duty_cycle=0;
					Set_PWM_Duty_Cycle(duty_cycle);
					break;
    			case 7:
    				//Boost efficiency test mode
    				MPPT_mode=7;
    				NVIC_DisableIRQ(TIMER_16_1_IRQn);
    				Boost_Efficiency();
    				NVIC_EnableIRQ(TIMER_16_1_IRQn);
    				break;
    			case 9:
    				//Static voltage mode
    				MPPT_mode=9;
    				//duty_cycle=0.25*max_duty_cycle;

    				duty_cycle=0;
    				Set_PWM_Duty_Cycle(duty_cycle);
    				break;
    			case 10:
    				//Static voltage mode
    				MPPT_mode=10;
    				duty_cycle=0.25*max_duty_cycle;
    				Set_PWM_Duty_Cycle(duty_cycle);
    				break;

    		}


    	}
    	else
    	{
    		MPPT_mode=3;
    		Check_Voltage_Current_at_Input_Output();
    		duty_cycle=0;
    		Set_PWM_Duty_Cycle(duty_cycle);
//    		Send_Status_to_CCS(receive_data[0]);
//    		Send_Data_to_CCS(OPEN);
    	}

    	//TODO: how long is the appropriate delay?
    	uint8_t i=0;
    	for(i=0; i<25 ; i++)
    	{
    		//Delay();
    		Check_Voltage_Current_at_Input_Output();
    	}

    	Delay();


    	send_fucking_CAN();
    	CAN_send_count++;

    	//LED_TEST();

    }


    return 0;
}
