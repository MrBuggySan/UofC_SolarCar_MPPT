/*
 * Helianthus21_modes.c
 *
 *  Created on: May 2, 2015
 *      Author: Andrei
 */
#include "chip.h"
#include <cr_section_macros.h>
#include "Helianthus21_utils.h"

extern uint16_t max_duty_cycle;
extern uint8_t min_duty_cycle;
extern uint16_t delta_voltage;
extern uint8_t Track;
extern uint8_t go_towards_higher_current;
extern uint8_t Track;
extern uint8_t fail_count;
extern uint16_t duty_cycle;
extern uint8_t data[8];
extern uint8_t Dynamic_count;
extern uint16_t Dynamic_iteration;
extern unsigned long dynamic_max_previous_power;
extern uint16_t dynamic_max_duty_power;
extern uint8_t receive_data[8];
extern uint8_t wait_for_tx_finished;


#define BOOST_REQ_CURRENT_1A 230 //This is 1A
//#define BOOST_REQ_CURRENT_1A 900 //This is 1A

#define BOOST_REQ_CURRENT_2A 323 //This is 2A
#define BOOST_REQ_CURRENT_3A 410 //This is 3A
#define BOOST_REQ_CURRENT_4A 500 //This is 4A
#define BOOST_REQ_CURRENT_5A 1004 //This is 5A

#define ALG_AVG 40


#define DYNAMIC_PERIOD 10
//static ADC_CLOCK_SETUP_T ADCSetup;


//TODO: This function is low priority
void Boost_Efficiency(void)
{
	uint16_t Req_input_current=BOOST_REQ_CURRENT_4A; //holder for the CAN message input requirement -AB 4/18/2015
	uint16_t voltage, current;
	//get the required input current here -AB 4/18/2015
	uint8_t message_num=0;

	while(1)
	{
		Read_ADC_Channel(ADC_CH6,&current);
		//within +/-5% value of the required current?
		if(current<Req_input_current)
		{
			duty_cycle+=1;
			Set_PWM_Duty_Cycle(duty_cycle);
		}

		if(current>Req_input_current)
		{
			duty_cycle-=1;
			Set_PWM_Duty_Cycle(duty_cycle);
		}


		uint8_t i=0;
		for(i=0; i<5 ; i++)
		{
			Delay();
			Check_Voltage_Current_at_Input_Output();
		 }
		if(receive_data[0] !=7)
			return;
		Read_ADC_Channel(ADC_CH1,&voltage);//Input Voltage
		Read_ADC_Channel(ADC_CH6, &current);//Input Current
		data[0]=voltage;
		data[2]=current;
		Read_ADC_Channel(ADC_CH3,&voltage);//Output Voltage
		Read_ADC_Channel(ADC_CH0, &current);//Output Current
		data[4]=voltage;
		data[6]=current;
		uint8_t dlc=8;
		message_num++;
		CANsend(data, &dlc, &message_num);

//		while (wait_for_tx_finished)
//			{
//
//			}

	};


}


void IV_trace(void)
{
	uint16_t current=0;
	uint16_t voltage=0;
	uint8_t dlc=8;
	uint8_t message_num=0;
	for(duty_cycle=min_duty_cycle; duty_cycle<=max_duty_cycle;	duty_cycle++)
	{

		Set_PWM_Duty_Cycle(duty_cycle);
		Delay();
		Delay();
		Delay();
		Delay();
		Check_Voltage_Current_at_Input_Output();
		if(Track == 0)
			return;

		Read_ADC_Channel(ADC_CH1,&voltage);//Input Voltage
		Read_ADC_Channel(ADC_CH6, &current);//Input Current
		data[0]=voltage;
		data[1]=voltage>>8;
		data[2]=current;
		data[3]=current>>8;
		Read_ADC_Channel(ADC_CH3,&voltage);//Output Voltage
		Read_ADC_Channel(ADC_CH0, &current);//Output Current
		data[4]=voltage;
		data[5]=voltage>>8;
		data[6]=current;
		data[7]=current>>8;
		uint8_t AB_probe3=data[4];
		uint8_t AB_probe4=data[5];
		CANsend(data, &dlc, &message_num);
		Delay();
		Delay();
		Delay();
		Delay();
		message_num++;

	}

}



//TODO: look over this function
// Track the MPP
void track_MPP_static(void)
{
	/* Pseudo-code:
	 * -save initial duty cycle before any changes to duty cycle is done
	 * -take readings of input voltage and current then average
	 * -calculate the initial power
	 * -while the absolute delta voltage (abs difference between init V and V after the change of duty cycle)
	 *                                       resulting from duty cycle change has not been reached
	 * 		-if go towards higher current =1 then increase duty cycle
	 * 		-if go towards higher current =0 then decrease duty cycle
	 * 		-re-read the input voltage to be used for abs difference calculation
	 * -delay to let the power change to take effect
	 * -take readings of input voltage and current then average
	 * -calculate the final power
	 * -compare the initial and final power
	 * 		-if final power is lesser than initial power then set go towards higher current to opposite of what it was
	 * 		-else set go towards higher current as it was
	 *
	 *
	 */

	uint8_t tracking_speed;
	// Variables needed to for this function to work
	unsigned char i = 0;
	uint16_t temp_current=0; // Used to store the current
	uint16_t current=0;
	uint16_t temp_voltage=0; // Store the value of voltage
	uint16_t voltage=0;
	uint16_t initial_duty_cycle = duty_cycle;

	unsigned long beginning_power;
	uint16_t beginning_current;
	uint16_t beginning_voltage;
	unsigned long end_power;
	uint16_t end_current;
	uint16_t end_voltage;

	// Increment the PWM
	// Also reduce the rate of oscillation when near the MPP
	if (fail_count == 0)
		tracking_speed = 4;
	else if (fail_count == 1)
		tracking_speed = 2;
	else
		tracking_speed = 1;

	// Measure the input current and voltage


	// Get four sets of current and voltage values, then calculate the average.
	for (i=0; i<ALG_AVG;i++)
	{
		// Get current data. This function needs to be written.
		//get_current_temp();

		Read_ADC_Channel(ADC_CH1, &voltage);
		Read_ADC_Channel(ADC_CH6, &current);
		Check_Voltage_Current_at_Input_Output();
		if (Track == 0)
		{
			return;
		}
		// Store input and current voltages. Still have to define data_array
		temp_current = temp_current + current;
		temp_voltage = temp_voltage + voltage;

	}

	// Calculate the initial power
 	beginning_current = temp_current/ALG_AVG;
	beginning_voltage = temp_voltage/ALG_AVG;

	beginning_power = beginning_voltage * beginning_current;

	//test duty cycle
	uint16_t duty_cycle__=duty_cycle;

	Check_Voltage_Current_at_Input_Output();
	if (Track == 0)
	{
		return;
	}

	//TODO: we may have to change delta voltage as we arrive the MPP
	while ( absdifference(voltage,beginning_voltage) < delta_voltage )
	{

		if (go_towards_higher_current ==1) // Increase duty cycle
			//duty_cycle = duty_cycle + tracking_speed; // Adjust tracking speed.
			duty_cycle = duty_cycle + 1;
		else if (go_towards_higher_current == 0) // Decrease duty cycle in this scenario
			//duty_cycle = duty_cycle - tracking_speed; // Adjust tracking speed.
			duty_cycle = duty_cycle - 1;
		duty_cycle__=duty_cycle;

		if (duty_cycle >= min_duty_cycle && duty_cycle <= max_duty_cycle)
			// Must be in this range for the following to take effect
			Set_PWM_Duty_Cycle(duty_cycle);
		// Check if updated duty cycle is out of range and undo change if necessary
		else
		{
			go_towards_higher_current = !go_towards_higher_current;
			duty_cycle = initial_duty_cycle;
			return;
		}

		// Re measure current
		Read_ADC_Channel(ADC_CH1, &voltage);
		//Read_ADC_Channel(ADC_CH6, &current);
//		Check_Voltage_Current_at_Input_Output();
//		if(Track == 0)
//			return;
//		Check_Voltage_Current_at_Input_Output();
//		if (Track == 0)
//		{
//			return;
//		}

	}

	for (i=0; i<10; i++)
	{
		Delay();
		Check_Voltage_Current_at_Input_Output();
				if (Track == 0)
				{
					return;
				}
	}

	// Re measure input current and voltage
	temp_current = 0;
	temp_voltage = 0;

	//Calculate the average of 4 samples.
	for (i=0; i<ALG_AVG; i++)
	{

		Read_ADC_Channel(ADC_CH1, &voltage);
		Read_ADC_Channel(ADC_CH6, &current);
		Check_Voltage_Current_at_Input_Output();
		if(Track == 0)
			return;
		// Store input current and voltage values.
		temp_current = temp_current + current;
		temp_voltage = temp_voltage + voltage;

	}

	end_current = temp_current/ALG_AVG;
	end_voltage = temp_voltage/ALG_AVG;

	end_power = end_voltage * end_current;

// Check if change in duty cycle was correct, undo if necessary.

	//TODO: This may be a good place to change delta_voltage
	if(end_power < beginning_power)
	{
		// Undo change in duty cycle by setting it to the initial value.
		duty_cycle = initial_duty_cycle;
		Set_PWM_Duty_Cycle(duty_cycle);
		// Change the direction we're moving in.
		go_towards_higher_current = !go_towards_higher_current;

		// Increase fail count to account for oscillations.
		if (fail_count < 8);
		fail_count++;

	}
	// When guesses are finally correct, decrease fail count. We'll get to this point eventually :D
	else
	{	if (fail_count > 0)
			fail_count--;
	}
	// AND WE ARE DONE. Good job team

	return;
}

void track_MPP_dynamic(void)
{
	/*Pseudo-code:
	 * Operate the power increment technique every 3 minutes
	 * 	-ten iterations
	 * 		-first iteration
	 * 			-duty cycle=0.10*max duty cycle
	 * 		-remaining iterations
	 * 			-duty cycle+= max duty cycle/10
	 * 		-check if the duty cycle is within the limits
	 * 		-Set the duty cycle
	 * 		-Measure input voltage
	 * 		-Measure input current
	 * 		-calculate input power
	 * 		-keep this current duty cycle if it produces a power greater than the previous duty cycle
	 * 	-P&O mode until the 3 minute timer runs out
	 *
	 */






	if(Dynamic_iteration>0 && Dynamic_iteration<DYNAMIC_PERIOD)
	{
		uint16_t temp_voltage=0;
		uint16_t temp_current=0;
		uint16_t voltage=0;
		uint16_t current=0;
		unsigned char i=0;
		unsigned long dynamic_tentative_power=0;


		//TODO: I can optimize the code by picking specific duty cycles
		duty_cycle=Dynamic_iteration*0.10*max_duty_cycle;
		if (duty_cycle >= min_duty_cycle && duty_cycle <= max_duty_cycle)
					// Must be in this range for the following to take effect
					Set_PWM_Duty_Cycle(duty_cycle);
		else return;


		for (i=0; i<3; i++)
		{
			Read_ADC_Channel(ADC_CH1,&voltage);//Input Voltage
			Read_ADC_Channel(ADC_CH6, &current);//Input Current
			temp_voltage+=voltage;
			temp_current+=current;
		}

		temp_voltage=temp_voltage/3;
		temp_current=temp_current/3;
		dynamic_tentative_power=temp_current*temp_voltage;


		if(dynamic_tentative_power>=dynamic_max_previous_power)
		{
			dynamic_max_duty_power=duty_cycle;
			dynamic_max_previous_power=dynamic_tentative_power;
		}
		for (i=0; i<30; i++)
			{
				Delay();
				Check_Voltage_Current_at_Input_Output();
						if (Track == 0)
						{
							return;
						}
			}
		Dynamic_iteration++;
		Check_Voltage_Current_at_Input_Output();
		if(Track == 0)
				return;
		duty_cycle=dynamic_max_duty_power;
		//main_check=0;
	}
	//P&0 mode
	if(Dynamic_iteration>=DYNAMIC_PERIOD)
	{
		//main_check=1;
		track_MPP_static();
	}



}


////TODO: look over this function
//void track_MPP_dynamic(void)
//{
//
//		// Variables needed to for this function to work
//
//        extern uint32_t i; // Used to store the iteration number
//     	uint32_t current_condition;
//    	uint32_t voltage_condition;
//
//    	// From AB_Test_utils.c . Include file when SAFE - MT
//    	extern const uint32_t max_duty_cycle;
//    	extern const uint32_t min_duty_cycle;
//
//    	uint16_t D1 = 1;
//    	uint16_t D2 = 75;
//    	uint16_t D3 = 99;
//    	extern uint32_t lower_duty;
//    	extern uint32_t upper_duty;
//    	extern uint32_t middle_duty;
//       	// We don't need the initial_duty_cycle variable
//    	// unsigned long initial_duty_cycle = duty_cycle;
//
//    	// Check if uint16_t would be a better option.
//    	extern uint32_t temp_current [3]; // Used to store the current
//    	extern uint32_t current;
//    	extern uint32_t temp_voltage [3]; // Store the value of voltage
//    	extern uint32_t voltage;
//       	extern uint32_t beginning_current;
//    	extern uint32_t beginning_voltage;
//    	extern uint32_t beginning_power[3]; // Store the first power values
//    	extern uint32_t end_power;
//    	extern uint32_t end_current;
//    	extern uint32_t end_voltage;
//    	extern uint32_t first_power;
//        uint32_t new_power;
//
//
//    	// Set the Duty Cycles for the first iteration
//    	lower_duty = Set_PWM_duty_cycle(D1);
//    		temp_voltage[1] = Read_ADC_Channel(ADC_CH7, &voltage);
//    		temp_current[1] = Read_ADC_Channel(ADC_CH5, &current);
//
//    	// Take these measurements for the next two Duty Cycles.
//    	upper_duty = Set_PWM_duty_cycle(D3);
//    		temp_voltage[3] = Read_ADC_Channel(ADC_CH7, &voltage);
//    	    temp_current[3] = Read_ADC_Channel(ADC_CH5, &current);
//
//    	middle_duty = Set_PWM_duty_cycle(D2);
//    		temp_voltage[2] = Read_ADC_Channel(ADC_CH7, &voltage);
//    	    temp_current[2] = Read_ADC_Channel(ADC_CH5, &current);
//
//    	//Calculate conditional values based on the two formulae given in DIRECT algorithm paper.
//    	current_condition = (temp_current[1] - temp_current[2])/ temp_current[1];
//    	voltage_condition = (temp_voltage[3] - temp_voltage[2])/ temp_voltage[3];
//
//    	// Check conditions against given values in the paper
//    	while ((current_condition >= 0.1) && (voltage_condition >= 0.2))
//    	{
//    	// Calculate the power for the stored current and voltage values
//    	 for (i=0; i<=3; i++)
//    	 {
//    		 beginning_power[i] = temp_voltage[i] * temp_current[i];
//    	  	}
//    	// Compare the calculated power values.
//    	if ((beginning_power[1] > beginning_power[2]) && (beginning_power[1] > beginning_power[3]))
//    	{
//    		first_power = beginning_power[1];
//    	// Set new range for duty cycle - Would it be better to have a function reading the ADC values for each Duty Cycles? Would make this code more shorter
//    	// Read the Current and Voltage values from the ADC.
//    	// Also check if the duty cycle is in the range
//    		if (((D1-10) >= min_duty_cycle) && ((D1+10) <= max_duty_cycle))
//    	    {
//    			upper_duty = Set_PWM_duty_cycle(D1+10);
//
//    			temp_voltage[3] = Read_ADC_Channel(ADC_CH7, &voltage);
//    	        temp_current[3] = Read_ADC_Channel(ADC_CH5, &current);
//
//    	        lower_duty = Set_PWM_duty_cycle(D1-10);
//    	    	temp_voltage[1] = Read_ADC_Channel(ADC_CH7, &voltage);
//    	    	temp_current[1] = Read_ADC_Channel(ADC_CH5, &current);
//
//    	        middle_duty = Set_PWM_duty_cycle(D1);
//    	    	temp_voltage[2] = Read_ADC_Channel(ADC_CH7, &voltage);
//    	    	temp_current[2] = Read_ADC_Channel(ADC_CH5, &current);
//    	     }
//    	     else
//    	     return 0;
//
//    	     }
//    	     else if ((beginning_power[2] > beginning_power[1]) && (beginning_power[2] > beginning_power[3]))
//    	     {
//    	    	 first_power = beginning_power[2];
//    	    	 if (((D2-10) >= min_duty_cycle) && ((D2+10) <= max_duty_cycle))
//    	        {
//    	    		 upper_duty = Set_PWM_duty_cycle(D2+10);
//
//    	        	 temp_voltage[3] = Read_ADC_Channel(ADC_CH7, &voltage);
//    	        	 temp_current[3] = Read_ADC_Channel(ADC_CH5, &current);
//
//    	        	 lower_duty = Set_PWM_duty_cycle(D2-10);
//    	        	 temp_voltage[1] = Read_ADC_Channel(ADC_CH7, &voltage);
//    	             temp_current[1] = Read_ADC_Channel(ADC_CH5, &current);
//
//   	    			 middle_duty = Set_PWM_duty_cycle(D1);
//    	        	 temp_voltage[2] = Read_ADC_Channel(ADC_CH7, &voltage);
//    	        	 temp_current[2] = Read_ADC_Channel(ADC_CH5, &current);
//    	         }
//    	       }
//    	       else
//    	       first_power = beginning_power[3];
//    	       if (((D3-10) >= min_duty_cycle) && ((D3+10) <= max_duty_cycle))
//    	       {
//    	    	   upper_duty = Set_PWM_duty_cycle(D3+10);
//    	           temp_voltage[3] = Read_ADC_Channel(ADC_CH7, &voltage);
//    	           temp_current[3] = Read_ADC_Channel(ADC_CH5, &current);
//     			   lower_duty = Set_PWM_duty_cycle(D2-10);
//    	           temp_voltage[1] = Read_ADC_Channel(ADC_CH7, &voltage);
//    	           temp_current[1] = Read_ADC_Channel(ADC_CH5, &current);
//
//    	           middle_duty = Set_PWM_duty_cycle(D1);
//    	           temp_voltage[2] = Read_ADC_Channel(ADC_CH7, &voltage);
//    	           temp_current[2] = Read_ADC_Channel(ADC_CH5, &current);
//    	       }
//
//    	       return 0;
//    	       }
//}


