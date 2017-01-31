/*
 * Helianthus21_utils.c
 *
 *  Created on: May 2, 2015
 *      Author: Andrei
 */

#include "chip.h"
#include <cr_section_macros.h>
#define TICKRATE_HZ1 (3) /* 10 ticks per second */
#define TICKRATE_HZ2 (4) /* 11 ticks per second */
#define TEST_CCAN_BAUD_RATE 125000

#define MPPT_RECEIVE 0x300


#define Tamura_slope 0.142579
#define Tamura_b 2.524332

#define CHECK_AVG_NUM 10
#define MAIN_AVG_NUM 30

#define DYNAMIC_PERIODIC_DELAY 24
//Resistors used in voltage dividers
#define R6 220
#define R7 15.4
#define R8 220
#define R9 5.11 //used to be 5.62, I changed the resistor so I can read higher voltages -AB
#define R16 150
#define R17 57.6

#define MPPT_TRACK_TRANSMIT_ID 0x410 //MPPT#1
//#define MPPT_TRACK_TRANSMIT_ID 0x420 //MPPT#2
//#define MPPT_TRACK_TRANSMIT_ID 0x430 //MPPT#3
//#define MPPT_TRACK_TRANSMIT_ID 0x440 //MPPT#4

#define CAN_SEND_WAIT 5


extern uint16_t max_duty_cycle;
extern uint8_t min_duty_cycle;
extern const uint16_t coutVoltage_limit;// 130V max output voltage
extern const uint16_t cinVoltage_limit;// 50V max input voltage -AB 6/17/2015
extern const uint16_t Current_limit ;//Protect the 8A fuse
extern uint8_t Track;


extern uint8_t Track;

extern uint16_t duty_cycle;

typedef enum {output=1, input, V_12} eVoltagetype ;
typedef enum {ORANGE=1, BLUE, YELLOW, GREEN, RED} LED_COLOR ;
typedef enum {STATIC_=0,DYNAMIC_=1, IV_TRACE=2, OPEN=3} MPPT_Mode;


//uint8_t toggleLED[5]; //ORANGE,BLUE,YELLOW,GREEN,RED


extern uint8_t data[8];
extern uint8_t receive_data[8];
extern uint8_t wait_for_tx_finished;
//extern CCAN_MSG_OBJ_T receive_obj;
CCAN_MSG_OBJ_T receive_obj;
extern CCAN_MSG_OBJ_T transmit_obj;
static ADC_CLOCK_SETUP_T ADCSetup;

extern uint16_t int_input_voltage;
extern float f_input_voltage;
extern uint16_t int_input_current;
extern float f_input_current;

extern uint16_t int_output_voltage;
extern float f_output_voltage;
extern uint16_t int_output_current;
extern float f_output_current;

extern uint16_t int_12V_voltage;
extern float f_12V_voltage;
extern uint16_t int_12V_current;
extern float f_12V_current;

extern uint8_t Arrayside_over;
extern uint8_t Batteryside_over;

extern uint8_t MPPT_mode;
extern uint8_t message_num;

extern uint8_t dlc;

extern uint8_t CAN_send_count;

extern uint8_t Dynamic_count;
//extern uint8_t dynamic_periodic_delay;






uint8_t delay_flag;
/*
 * Initialize CLKOUT pin
 * returns nothing
 */



void Set_PWM_Duty_Cycle(uint16_t duty_cycle)
{
	Chip_TIMER_SetMatch(LPC_TIMER32_1, 0, (max_duty_cycle-duty_cycle) );
}

void Config_CLKOUTpin_(void)
{
	Chip_GPIO_SetPinDIROutput(LPC_GPIO, 0, 1);
	Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO0_1, FUNC1); // choose CLKOUT function -AB 1/7/2015

	//***** Setup CLKOUT pin -AB 1/7/2015
	LPC_SYSCTL->CLKOUTSEL=0x3;// choose main clock
	LPC_SYSCTL->CLKOUTDIV= 1;
	LPC_SYSCTL->CLKOUTUEN= 0; // no change
	LPC_SYSCTL->CLKOUTUEN= 1; // change
	//*****

	return ;
}

void Config_CT32B0(void)
{
	Chip_TIMER_Init(LPC_TIMER32_0);
	Chip_TIMER_Reset(LPC_TIMER32_0);

	/*enable interrupt*/
	Chip_TIMER_MatchEnableInt(LPC_TIMER32_0,0);
	NVIC_EnableIRQ(TIMER_32_0_IRQn);


	Chip_TIMER_PrescaleSet(LPC_TIMER32_0,1);//48MHz/1= 48MHz
	Chip_TIMER_SetMatch(LPC_TIMER32_0, 0, 0xB71B00);//48MHz/12M = 4 Hz


	Chip_TIMER_ResetOnMatchEnable(LPC_TIMER32_0, 0);
	Chip_TIMER_Enable(LPC_TIMER32_0);
	Chip_TIMER_ClearMatch(LPC_TIMER32_0, 0);
}

/* @brief Setup CT16B0 for delay function
 * @param none
 * @return none
 */
void Config_CT16B0(void)
{
	//***** Set Prescale counter, match value -AB 1/7/2014
	Chip_TIMER_Init(LPC_TIMER16_0);
	Chip_TIMER_Reset(LPC_TIMER16_0);
	Chip_TIMER_PrescaleSet(LPC_TIMER16_0,0xFFFF);// 48Mhz/65535= 733 Hz input to the timer block
	Chip_TIMER_SetMatch(LPC_TIMER16_0, 0, 7 );// 733/7= ~105, LED will blink at ~105 Hz ,10ms timer
	Chip_TIMER_ResetOnMatchEnable(LPC_TIMER16_0, 0); // enable reset when Timer counter == Match value
	//*****
}

/* @brief control the PWM duty cycle using CT32B1_MAT0
 * @param duty cycle from 1% to 100%
 * @return nothing
 */
void Config_PWM_CT32B1_MAT0(uint16_t duty_cycle)
{
	Chip_TIMER_Init(LPC_TIMER32_1);
	Chip_TIMER_Reset(LPC_TIMER32_1);
	//Chip_TIMER_PrescaleSet(LPC_TIMER32_1,8);// 48Mhz/8=6Mhz
	Chip_TIMER_PrescaleSet(LPC_TIMER32_1,1);// 48Mhz/1=48Mhz
	Chip_TIMER_SetMatch(LPC_TIMER32_1, 0, (max_duty_cycle-duty_cycle) ); //duty cycle is 0% to 100%
	Chip_TIMER_SetMatch(LPC_TIMER32_1, 2, max_duty_cycle);// Match timer 2 sets the length, 48MHz/max_duty_cycle PWM frequency
	//Chip_TIMER_SetMatch(LPC_TIMER32_1, 0, (400-duty_cycle) ); //duty cycle is 0 to 800 Chip_TIMER_SetMatch
	//Chip_TIMER_SetMatch(LPC_TIMER32_1, 2, 800);// Match timer 2 sets the length, 48MHz/800= 60,000 KHz PWM frequency
	LPC_TIMER32_1->MCR   |= 1<<7; //Set MATCH 2 to define the length
	//LPC_TIMER32_1->EMR |= 0x2<<6; //Set MAT2 on match
	LPC_TIMER32_1->PWMC|=1  ; //Set PWMEN0 for PWM mode
	Chip_TIMER_Enable(LPC_TIMER32_1);
}

void Config_ADCs(void)
{

	Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO1_0, FUNC2); //AD1 reads input voltage
	Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO1_10, FUNC1); //AD6 read input current
	Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO1_2, FUNC2); //AD3 read output voltage
	Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO0_11, FUNC2); //AD0 read output current
	Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO1_11, FUNC1); //AD7 read 12V voltage
	Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO1_4, FUNC1); //AD5 read 12V current
	Chip_ADC_Init(LPC_ADC, &ADCSetup);

	/*
		 * input voltage= channel 1
		 * input current= channel 6
		 * output voltage= channel 3
		 * output current= channel 0
		 * 12V voltage=channel 7
		 * 12V current=channel 5
		 * -AB 1/7/2014
		 */


}

void Config_Watchdog(void)
{
	uint32_t wdtFreq;
	/* Initialize WWDT (also enables WWDT clock) */
	Chip_WWDT_Init(LPC_WWDT);
	/* Prior to initializing the watchdog driver, the clocking for the
	   watchdog must be enabled. This example uses the watchdog oscillator
	   set at a 50KHz (1Mhz / 20) clock rate. 50KHz=> 20us   */
	Chip_SYSCTL_PowerUp(SYSCTL_POWERDOWN_WDTOSC_PD); //It seems like I do not use PDRUNCFG on the ADC peripheral -AB 4/18/2015
	Chip_Clock_SetWDTOSC(WDTLFO_OSC_1_05, 20);

	/* The WDT divides the input frequency into it by 4 */
	wdtFreq = Chip_Clock_GetWDTOSCRate() / 4;

	/* LPC1102/4, LPC11XXLV, and LPC11CXX devices select the watchdog
	   clock source from the SYSCLK block, while LPC11AXX, LPC11EXX, and
	   LPC11UXX devices select the clock as part of the watchdog block. */
	Chip_Clock_SetWDTClockSource(SYSCTL_WDTCLKSRC_WDTOSC, 1);

	/* Set watchdog feed time constant to approximately 2s
	   */
	Chip_WWDT_SetTimeOut(LPC_WWDT, wdtFreq * 2);

	/* Setup Systick to feed the watchdog timer. This needs to be done
		 * at a rate faster than the WDT warning. */
	SysTick_Config(Chip_Clock_GetSystemClockRate() / 50);

	/* Clear watchdog warning and timeout interrupts */
	Chip_WWDT_ClearStatusFlag(LPC_WWDT, WWDT_WDMOD_WDTOF);

	/* Clear watchdog interrupt */
	NVIC_ClearPendingIRQ(WDT_IRQn);
	/*
	 * when will this interrupt fire? Manual says the flag will be set when the
	 * Watchdog counter reaches the value specified by WDWARNINT  -AB 4/18/2015
	 */


	/*enable watchdog interrupt */
	NVIC_EnableIRQ(WDT_IRQn);


	Chip_WWDT_Start(LPC_WWDT);
}

/* @brief configure the GPIO
 * @param none
 * @return none
 */
void Config_GPIO(void)

{
	Chip_GPIO_Init(LPC_GPIO);


	//Orange LED
	Chip_GPIO_SetPinDIROutput(LPC_GPIO,0,2);
	Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO0_2, FUNC0);
	//Blue LED
	Chip_GPIO_SetPinDIROutput(LPC_GPIO,1,8);
	Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO1_8, FUNC0);
	//Yellow LED
	Chip_GPIO_SetPinDIROutput(LPC_GPIO,2,6);
	Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO2_6, FUNC0);
	//Green LED
	Chip_GPIO_SetPinDIROutput(LPC_GPIO,2,7);
	Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO2_7, FUNC0);
	//Red LED
	Chip_GPIO_SetPinDIROutput(LPC_GPIO,2,8);
	Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO2_8, FUNC0);

	//PWM
	Chip_GPIO_SetPinDIROutput(LPC_GPIO,1,1);
	Chip_IOCON_PinMuxSet(LPC_IOCON,IOCON_PIO1_1,FUNC3); //choose CT32B1_MAT0, PWM


	//Relay
	Chip_GPIO_SetPinDIROutput(LPC_GPIO,3,0);
	Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO3_0, FUNC0);

}

/* @brief Setup the periodic sending of CAN messages to CCS
 * @param none
 * @return none
 */
void Config_CT16B1(void)
{
	//TODO: Design this timer for CAN sending
	Chip_TIMER_Init(LPC_TIMER16_1);
	Chip_TIMER_Reset(LPC_TIMER16_1);
	Chip_TIMER_PrescaleSet(LPC_TIMER16_1,0xFFFF);// 48Mhz/65535= 733 Hz input to the timer block
	//Chip_TIMER_SetMatch(LPC_TIMER16_1, 0, 184 );// 733/184= ~4, LED will blink at ~4 Hz ,250ms timer
	Chip_TIMER_SetMatch(LPC_TIMER16_1, 0, 730 ); //use every 1s
	Chip_TIMER_ResetOnMatchEnable(LPC_TIMER16_1, 0); // enable reset when Timer counter == Match value
	/*enable interrupt*/
	Chip_TIMER_MatchEnableInt(LPC_TIMER16_1,0);
	NVIC_EnableIRQ(TIMER_16_1_IRQn);

	Chip_TIMER_Enable(LPC_TIMER16_1);
	Chip_TIMER_ClearMatch(LPC_TIMER16_1, 0);

}

void Configure_MPPT(uint16_t duty_cycle)
{
	 //48MHz System clock at CLKOUT pin -AB 1/7/2015
	 Config_CLKOUTpin_();

//	  I/O configurations for peripherals (not including ADC) -AB 1/7/2015
//	 Config_Peripheral_pins_();

	 //Setup the delay function -AB 6/21/2015
	 Config_CT16B0();


	 //Set the ADCs
	 Config_ADCs();

	 //Set and enable the initial PWM duty cycle -AB 1/7/2015
	Config_PWM_CT32B1_MAT0(duty_cycle);

	 //Config_Watchdog();

	 Config_CAN();

	 Config_GPIO();

	 //This is for the LEDs

	 //I will use Config_CT32B0 as timer for dynamic algorithm, time for every 3 minutes.


	 Config_CT32B0();//Toggle the LEDs and setup the timer for dynamic algorithm

	 Set_PWM_Duty_Cycle(duty_cycle);// Set the guess duty cycle -AB 1/8/2015

	 LED_OFF(BLUE);
	 LED_OFF(RED);
	 LED_OFF(GREEN);
	 LED_OFF(ORANGE);
	 LED_OFF(YELLOW);

	 //Setup the periodic CAN messages to CCS
	 //Config_CT16B1();

}


/* @brief Open the relay, this will disconnect the resistor to the + cap
 * @param None
 * @return Nothing
 */
void Relay_OPEN(void)
{
	Chip_GPIO_SetPinState(LPC_GPIO, 3, 0, TRUE);
}

/* @brief close the relay, this will connect the resistor to the + cap
 * @param None
 * @return Nothing
 */
void Relay_CLOSE(void)
{
	Chip_GPIO_SetPinState(LPC_GPIO, 3, 0, FALSE);
}

/* @brief turn on an LED
 * @param LED color
 * @return Nothing
 */
void LED_ON(LED_COLOR color)
{
	switch (color)
	{
		case ORANGE:
			Chip_GPIO_SetPinState(LPC_GPIO, 0, 2, TRUE);
			break;
		case BLUE:
			Chip_GPIO_SetPinState(LPC_GPIO, 1, 8, TRUE);
			break;
		case YELLOW:
			Chip_GPIO_SetPinState(LPC_GPIO, 2, 6, TRUE);
			break;
		case GREEN:
			Chip_GPIO_SetPinState(LPC_GPIO, 2, 7, TRUE);
			break;
		case RED:
			Chip_GPIO_SetPinState(LPC_GPIO, 2, 8, TRUE);
			break;
	}
}

/* @brief turn off an LED
 * @param LED color
 * @return Nothing
 */
void LED_OFF(LED_COLOR color)
{
	switch (color)
		{
			case ORANGE:
				Chip_GPIO_SetPinState(LPC_GPIO, 0, 2, FALSE);
				break;
			case BLUE:
				Chip_GPIO_SetPinState(LPC_GPIO, 1, 8, FALSE);
				break;
			case YELLOW:
				Chip_GPIO_SetPinState(LPC_GPIO, 2, 6, FALSE);
				break;
			case GREEN:
				Chip_GPIO_SetPinState(LPC_GPIO, 2, 7, FALSE);
				break;
			case RED:
				Chip_GPIO_SetPinState(LPC_GPIO, 2, 8, FALSE);
				break;
		}
}

/* @brief toggle an LED
 * @param LED color
 * @return Nothing
 */
void LED_Toggle (LED_COLOR color)
{
	//TODO: is there something wrong with this function?
	switch (color)
		{
			case ORANGE:
				Chip_GPIO_SetPinState(LPC_GPIO, 0, 2, ~Chip_GPIO_GetPinState(LPC_GPIO, 0, 2));
				break;
			case BLUE:
				Chip_GPIO_SetPinState(LPC_GPIO, 1, 8,  ~Chip_GPIO_GetPinState(LPC_GPIO, 1, 8));
				break;
			case YELLOW:
				Chip_GPIO_SetPinState(LPC_GPIO, 2, 6,  ~Chip_GPIO_GetPinState(LPC_GPIO, 2, 6));
				break;
			case GREEN:
				Chip_GPIO_SetPinState(LPC_GPIO, 2, 7,  ~Chip_GPIO_GetPinState(LPC_GPIO, 2, 7));
				break;
			case RED:
				Chip_GPIO_SetPinState(LPC_GPIO, 2, 8,  ~(Chip_GPIO_GetPinState(LPC_GPIO, 2, 8)));
				break;

		}
}

void Read_ADC_Channel(uint8_t channel, uint16_t *dataADC)
{
	/*
	 * input voltage= channel 1
	 * input current= channel 6
	 * output voltage= channel 3
	 * output current= channel 0
	 * 12V voltage=channel 7
	 * 12V current=channel 5
	 * -AB 1/7/2014
	 */
	Chip_ADC_EnableChannel(LPC_ADC, channel,ENABLE);
	Chip_ADC_SetStartMode(LPC_ADC, ADC_START_NOW, ADC_TRIGGERMODE_RISING);
	while (Chip_ADC_ReadStatus(LPC_ADC, channel, ADC_DR_DONE_STAT) != SET) {}
	Chip_ADC_ReadValue(LPC_ADC, channel, dataADC);
	Chip_ADC_EnableChannel(LPC_ADC, channel,DISABLE);

}

unsigned long absdifference(unsigned long a, unsigned long b)
{
	if(a>=b)
		return a-b;
	else
		return b-a;
}

float fabsdifference(float a, float b)
{
	if(a>=b)
			return a-b;
		else
			return b-a;
}


/* @brief Calculates the real analog current
 * @param pointer to value of current from ADC
 * @param pointer to value of real analog value
 */


void ConvertADCcurrent_to_realcurrent(uint16_t* fake_current, float* real_current)
{

	//*real_current=(((*fake_current*3.3)/1023)-Tamura_b)/Tamura_slope;
	*real_current=3;//Temporary
}


/* @brief Calculates the real analog voltage
 * @param pointer to value of voltage from ADC
 * @param pointer to value of real analog value
 */
void ConvertADCvoltage_to_realvoltage(uint16_t* fake_voltage,float* real_voltage, eVoltagetype V_type)
{

	switch (V_type)
	{
		case output:
			*real_voltage=((R8+R9)*3.3*(*fake_voltage))/(R9*1023);
			break;
		case input:
			*real_voltage=((R6+R7)*3.3*(*fake_voltage))/(R7*1023);
			break;
		case V_12:
			*real_voltage=((R16+R17)*3.3*(*fake_voltage))/(R17*1023);
			break;
		//default:
			//TODO: error

	}

}


void Check_Voltage_Current_at_Input_Output(void)
{
	uint16_t int_input_voltage_check=0;
	float f_input_voltage_check=0;


	uint16_t int_input_current=0;
	float f_input_current=0;


	uint16_t int_output_voltage_check=0;
	float f_output_voltage_check=0;


	uint16_t int_output_current=0;
	float f_output_current=0;

	float f_avg_input_voltage_check=0;
	float f_avg_output_voltage_check=0;
	float f_avg_input_current=0;
	float f_avg_output_current=0;

	uint8_t i=0;

	uint16_t check_initial_duty_cycle=duty_cycle;
//
//	uint16_t int_12V_voltage=0;
//	float f_12V_voltage=0;
//	uint16_t int_12V_current=0;
//	float f_12V_current=0;

	for(i=0; i<CHECK_AVG_NUM;i++)
	{

		//Check output voltage -AB 6/17/2015
		Read_ADC_Channel(ADC_CH3,&int_output_voltage_check);
		ConvertADCvoltage_to_realvoltage(&int_output_voltage_check, &f_output_voltage_check, output);

		//Check input voltage - AB 6/17/2015
		Read_ADC_Channel(ADC_CH1,&int_input_voltage_check);
		ConvertADCvoltage_to_realvoltage(&int_input_voltage_check, &f_input_voltage_check, input);

		//Check output current - AB 6/17/2015
		Read_ADC_Channel(ADC_CH0,&int_output_current);
		ConvertADCcurrent_to_realcurrent(&int_output_current, &f_output_current);

		//Check input current - AB 6/17/2015
		Read_ADC_Channel(ADC_CH6,&int_input_current);
		ConvertADCcurrent_to_realcurrent(&int_input_current, &f_input_current);

		f_avg_input_voltage_check+=f_input_voltage_check;
		f_avg_input_current+=f_input_current;
		f_avg_output_voltage_check+=f_output_voltage_check;
		f_avg_output_current+=f_output_current;

	}

	f_avg_input_voltage_check=f_avg_input_voltage_check/CHECK_AVG_NUM;
	f_avg_input_current=f_avg_input_current/CHECK_AVG_NUM;
	f_avg_output_voltage_check=f_avg_output_voltage_check/CHECK_AVG_NUM;
	f_avg_output_current=f_avg_output_current/CHECK_AVG_NUM;

	uint16_t AB_probe4=int_input_current;


	//Arrayside_over=0;
	//Batteryside_over=0;
 	if((uint16_t)f_avg_output_voltage_check>coutVoltage_limit || (uint16_t)f_avg_input_voltage_check>cinVoltage_limit || (uint16_t)f_avg_output_current>Current_limit || (uint16_t)f_avg_input_current>Current_limit)
	//if((uint16_t)f_output_voltage_check>coutVoltage_limit || (uint16_t)f_input_voltage_check>cinVoltage_limit)
	{
		//TODO: LET CCS know which caused the error
		if((uint16_t)f_avg_input_voltage_check>cinVoltage_limit)
		{
			Arrayside_over=1;
		}

		if((uint16_t)f_avg_input_current>Current_limit)
		{
			Arrayside_over=2;
		}

		if((uint16_t)f_avg_input_voltage_check>cinVoltage_limit && (uint16_t)f_avg_input_current>Current_limit)
		{
			Arrayside_over=3;
		}

		if((uint16_t)f_avg_output_voltage_check>coutVoltage_limit)
		{
			Batteryside_over=1;
		}

		if((uint16_t)f_avg_output_current>Current_limit)
		{
			Batteryside_over=2;
		}

		if((uint16_t)f_avg_output_voltage_check>coutVoltage_limit && (uint16_t)f_avg_output_current>Current_limit)
		{
			Batteryside_over=3;
		}


		Set_PWM_Duty_Cycle(0); // FET is open so array is on Voc -AB 1/9/2015
		LED_ON(RED);
		Track=0;	//Stop tracking
	}
	else
	{
		LED_OFF(RED);
		Track=1;
		//TODO: must setup duty cycle again
		Set_PWM_Duty_Cycle(check_initial_duty_cycle);//Set a good guess here -AB 6/18/2015
		Batteryside_over=0;
		Arrayside_over=0;
	}

}


/*	@brief Calculates the appropriate value for CLKDIVVAL depending on required baud rate
 * 	@param baud rate required
 * 	@param Holder for correct CLKDIVVAL result
 * 	@return Nothing
*/
void baudrateCalculate(uint32_t baud_rate, uint32_t *can_api_timing_cfg)
{
	uint32_t pClk, div, quanta, segs, seg1, seg2, clk_per_bit, can_sjw;
	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_CAN);
	pClk = Chip_Clock_GetMainClockRate();

	clk_per_bit = pClk / baud_rate;

	for (div = 0; div <= 15; div++) {
		for (quanta = 1; quanta <= 32; quanta++) {
			for (segs = 3; segs <= 17; segs++) {
				if (clk_per_bit == (segs * quanta * (div + 1))) {
					segs -= 3;
					seg1 = segs / 2;
					seg2 = segs - seg1;
					can_sjw = seg1 > 3 ? 3 : seg1;
					can_api_timing_cfg[0] = div;
					can_api_timing_cfg[1] =
						((quanta - 1) & 0x3F) | (can_sjw & 0x03) << 6 | (seg1 & 0x0F) << 8 | (seg2 & 0x07) << 12;
					return;
				}
			}
		}
	}
}

void send_fucking_CAN(void)
{
	if(CAN_send_count==CAN_SEND_WAIT)
	{
		uint8_t i=0;
		CAN_send_count=0;
		float avg_voltage=0;
		float avg_current=0;
		//Status message
		if(message_num==0)
		{
			data[0]=MPPT_mode;
			data[1]=Arrayside_over;
			data[2]=Batteryside_over;
			data[3]=0;
			data[4]=0;
			data[5]=0;
			data[6]=0;
			data[7]=0;

			CANsend(data, &dlc, &message_num);

		}

		//Voltage input, current input
		if(message_num==1)
		{
			for(i=0; i<MAIN_AVG_NUM; i++)
			{
				Read_ADC_Channel(ADC_CH1,&int_input_voltage);//Input Voltage
				ConvertADCvoltage_to_realvoltage(&int_input_voltage, &f_input_voltage, input);


				Read_ADC_Channel(ADC_CH6, &int_input_current);//Input Current
				ConvertADCcurrent_to_realcurrent(&int_input_current, &f_input_current);

				avg_voltage+=f_input_voltage;
				avg_current+=f_input_current;
			}

			avg_voltage=avg_voltage/MAIN_AVG_NUM;
			avg_current=avg_current/MAIN_AVG_NUM;

			turnFloatIntoChar(data, avg_voltage);
			turnFloatIntoChar(&data[4], avg_current);

			CANsend(data, &dlc, &message_num);

		}
		float test_voltage_in=f_input_voltage;
		//Voltage output, current output
		if(message_num==2)
		{
			for(i=0; i<MAIN_AVG_NUM; i++)
			{
				Read_ADC_Channel(ADC_CH3,&int_output_voltage);//Output Voltage
				ConvertADCvoltage_to_realvoltage(&int_output_voltage, &f_output_voltage, output);

				Read_ADC_Channel(ADC_CH0, &int_output_current);//Output Current
				ConvertADCcurrent_to_realcurrent(&int_output_current, &f_output_current);

				avg_voltage+=f_output_voltage;
				avg_current+=f_output_current;
			}

			avg_voltage=avg_voltage/MAIN_AVG_NUM;
			avg_current=avg_current/MAIN_AVG_NUM;

			turnFloatIntoChar(data, avg_voltage);
			turnFloatIntoChar(&data[4], avg_current);

			CANsend(data, &dlc, &message_num);

			//message_num=0;//go back to sending the status message, useful for Helianthus REV 2.2 only
		}
		//float test_voltage_out=f_output_voltage;

		//This last message is for the board voltage and board current, this will not be implemented for Helianthus REV 2.2
	//	if(message_num==3)
	//	{
	//
	//	}



		message_num++;



		if(message_num==3)
		{
					message_num=0;//IV trace does not send voltage output and current output
		}
	}

}

union FloatCharTranslator
	{
	   float floatData;
	   char charData[4];
	}dataUnion;

/* @brief turns a 32 bit float into a 32 bit char
 * @param pointer to an array of 4 chars
 * @param float value to be converted
 * Note: Make sure that data has enough space (at least 4)
 */
void turnFloatIntoChar(uint8_t* data_, const float value)
{

   //FloatCharTranslator dataUnion;
   dataUnion.floatData = value;
   uint8_t i = 0;
   for (i = 0; i < 4; i++)
   {
	   data_[i] = dataUnion.charData[i];
   }
}

/*  @brief transmit the message to CAN bus
 *  @param pointer to data array
 *	@param pointer to length of data field
 *	@return nothing
 */
void CANsend(uint8_t* data_, uint8_t* dlc_, uint8_t* message_num)
{
	wait_for_tx_finished=2;

	transmit_obj.mode_id=(CAN_MSGOBJ_STD|MPPT_TRACK_TRANSMIT_ID)+*message_num;
	transmit_obj.dlc=*dlc_ ;
	transmit_obj.data[0] =data_[0];
	transmit_obj.data[1] =data_[1];
	transmit_obj.data[2] =data_[2];
	transmit_obj.data[3] =data_[3];
	transmit_obj.data[4] =data_[4];
	transmit_obj.data[5] =data_[5];
	transmit_obj.data[6] =data_[6];
	transmit_obj.data[7] =data_[7];

	//wait_for_tx_finished=1;
	LPC_CCAN_API->can_transmit(&transmit_obj);

	while(wait_for_tx_finished)
	{
		if(Chip_GPIO_GetPinState(LPC_GPIO, 2, 8))
			{
				LED_OFF(YELLOW);
			}
			else LED_ON(YELLOW);
	}


}
/*	CAN transmit callback */
/*	Function is executed by the Callback handler after
    a CAN message has been transmitted */
void CAN_tx(uint8_t msg_obj_num)
{

//	if(Chip_GPIO_GetPinState(LPC_GPIO, 2, 8))
//	{
//		LED_OFF(YELLOW);
//	}
//	else LED_ON(YELLOW);
	//		Reset the flag used by main to wait for transmission finished
	if (wait_for_tx_finished == msg_obj_num)
	{
		wait_for_tx_finished=0;
	}



	return;

}

/*	CAN error callback */
/*	Function is executed by the Callback handler after
    an error has occured on the CAN bus */
void CAN_error(uint32_t error_info)
{
	//TODO: Blink the RED LED here
	//TODO: Determine which error came up and solve appropriately
	//
	uint8_t andrei_b_testing=0;
	andrei_b_testing=1;
}

/*	CAN receive callback */
/*	Function is executed by the Callback handler after
    a CAN message has been received */
void CAN_rx(uint8_t msg_obj_num)
{


	/* Determine which CAN message has been received */
	//receive_obj.msgobj = msg_obj_num;
	/* Now load up the receive_obj structure with the CAN message */
	//LPC_CCAN_API->can_receive(&receive_obj);

	//uint8_t AB_probe=msg_obj_num;
	if (msg_obj_num == receive_obj.msgobj)
	{
		LPC_CCAN_API->can_receive(&receive_obj);
		receive_data[0]=receive_obj.data[0];
		//uint8_t AB_probe2=receive_obj.data[0];
		receive_data[1]=receive_obj.data[1];
		receive_data[2]=receive_obj.data[2];
		receive_data[3]=receive_obj.data[3];
		receive_data[4]=receive_obj.data[4];
		receive_data[5]=receive_obj.data[5];
		receive_data[6]=receive_obj.data[6];
		receive_data[7]=receive_obj.data[7];


	}
}
void Config_CAN(void)
{

		uint32_t CanApiClkInitTable[2];
	  	/* Publish CAN Callback Functions */
		CCAN_CALLBACKS_T callbacks = {
			&CAN_rx,
			&CAN_tx,
			&CAN_error,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
		};

	    baudrateCalculate(TEST_CCAN_BAUD_RATE, CanApiClkInitTable);

	    /*@brief CAN initialization of CLKDIVVAL and CAN_BTR
	     *@param argument is applied to CANCLKDIV-contains divisor to be used to divide PCLK to produce CAN_CLK
	     *@param argument is applied to CAN_BTR- enables interrupts on the CAN controller level. Set to FALSE for polled communication
	     *@return Nothing
	    */
	    LPC_CCAN_API->init_can(&CanApiClkInitTable[0], TRUE);

		 /*
		  *  @brief Configure the CAN callback functions
		  *  @param callback struct containing the callback functions
		  *  @return Nothing
		  */
	  	LPC_CCAN_API->config_calb(&callbacks);

	  	/*	@brief
	  	 * 	@param
	  	 * 	@return Nothing
	  	 */

		transmit_obj.mode_id=CAN_MSGOBJ_STD|MPPT_TRACK_TRANSMIT_ID;
		transmit_obj.mask=0x0;
		transmit_obj.msgobj=2;


	  	receive_obj.mode_id=CAN_MSGOBJ_STD|MPPT_RECEIVE;
	  	receive_obj.mask=0x3FF;//
	  	receive_obj.msgobj=1;
	  	LPC_CCAN_API->config_rxmsgobj(&receive_obj);



	  	NVIC_EnableIRQ(CAN_IRQn);


}



/* @brief Will delay operations of the microcontroller to allow transients to dissipate
 * @param none
 * @return none
 */
void Delay(void)
{
	delay_flag=1;
	Chip_TIMER_ClearMatch(LPC_TIMER16_0, 0);
	Chip_TIMER_MatchEnableInt(LPC_TIMER16_0,0);
	NVIC_EnableIRQ(TIMER_16_0_IRQn);
	Chip_TIMER_Enable(LPC_TIMER16_0);
	while(delay_flag){}

}

void LED_TEST(void)
{
	LED_ON(ORANGE);
	LED_ON(YELLOW);
	LED_ON(RED);
	LED_ON(BLUE);
	LED_ON(GREEN);
}

void Send_Status_to_CCS(void)
{
	uint8_t dlc=8;
	uint8_t message_num=0;

	data[0]=MPPT_mode;
	data[1]=Arrayside_over;
	data[2]=Batteryside_over;
	data[3]=0;
	data[4]=0;
	data[5]=0;
	data[6]=0;
	data[7]=0;

	CANsend(data, &dlc, &message_num);


	while (wait_for_tx_finished)
	{

	}

}

void Send_Data_to_CCS(MPPT_Mode modyyy)
{
//	uint16_t int_input_voltage=0;
//	float f_input_voltage=0;
//	uint16_t int_input_current=0;
//	float f_input_current=0;
//
//	uint16_t int_output_voltage=0;
//	float f_output_voltage=0;
//	uint16_t int_output_current=0;
//	float f_output_current=0;
//
//	uint16_t int_12V_voltage=0;
//	float f_12V_voltage=0;
//	uint16_t int_12V_current=0;
//	float f_12V_current=0;




//	if(modyyy==STATIC_ || modyyy== DYNAMIC_ || modyyy==OPEN)
//	{
//		Delay();
//		Delay();
//		Read_ADC_Channel(ADC_CH1,&int_input_voltage);//Input Voltage
//		ConvertADCvoltage_to_realvoltage(&int_input_voltage, &f_input_voltage, input);
//		turnFloatIntoChar(data, f_input_voltage);
//
//		Read_ADC_Channel(ADC_CH6, &int_input_current);//Input Current
//		//ConvertADCvoltage_to_realcurrent(int_input_current, f_input_current);
//		//turnFloatIntoChar(data[4], f_input_current);
//
//		//This is temporary until I get the Tamura Current Sensors! -AB 6/17/2015
//		data[4]='1';
//		data[5]=0;
//		data[6]=0;
//		data[7]=0;
//		message_num=1;
//		CANsend(data, &dlc, &message_num);
//		Delay();
//		Delay();
//		while (wait_for_tx_finished)
//		    		{
//			data[7]=0;
//		    		}
//		//******************************************************
//		Read_ADC_Channel(ADC_CH3,&int_output_voltage);//Output Voltage
//		ConvertADCvoltage_to_realvoltage(&int_output_voltage, &f_output_voltage, output);
//		turnFloatIntoChar(data, f_output_voltage);
//
//
//		//Read_ADC_Channel(ADC_CH0, &int_output_current);//Output Current
//		//ConvertADCvoltage_to_realcurrent(int_output_current, f_output_current);
//		//turnFloatIntoChar(data[4], float_val);
//		//This is temporary until I get the Tamura Current Sensors! -AB 6/17/2015
//		data[4]='2';
//		data[5]=0;
//		data[6]=0;
//		data[7]=0;
//		message_num=2;
//		CANsend(data, &dlc, &message_num);
//		Delay();
//		Delay();
//		while (wait_for_tx_finished)
//		    		{
//
//		    		}
//		//	//******************************************************
//		//Read_ADC_Channel(ADC_CH7, &int_12V_voltage);//12V Voltage
////		ConvertADCvoltage_to_realvoltage(&int_12V_voltage, &f_12V_voltage, V_12);
////		turnFloatIntoChar(data, f_12V_voltage);
//
//		//Read_ADC_Channel(ADC_CH5, &int_12V_current);//12V Current
//		//ConvertADCvoltage_to_realcurrent(int_12V_current, f_12V_current);
//		//turnFloatIntoChar(data[4], f_12V_current);
//		//This is temporary until I get the Tamura Current Sensors! -AB 6/17/2015
////		data[0]='T';
////				data[1]='E';
////				data[2]='S';
////				data[3]='T';
////				data[4]='3';
////		data[4]=0;
////		data[5]=0;
////		data[6]=0;
////		data[7]=0;
////		message_num=3;
////		CANsend(data, &dlc, &message_num);
////		Delay();
////		Delay();
////		while (wait_for_tx_finished)
////		    		{
////
////		    		}
//	}
//
//
//	if(modyyy==IV_TRACE)
//	{
//		Read_ADC_Channel(ADC_CH1,&int_input_voltage);//Input Voltage
//		ConvertADCvoltage_to_realvoltage(&int_input_voltage, &f_input_voltage, input);
//		turnFloatIntoChar(data, f_input_voltage);
//
//		Read_ADC_Channel(ADC_CH6,&int_input_current);//Input Current
//		//ConvertADCvoltage_to_realcurrent(int_val, float_val);
//		//turnFloatIntoChar(data[4], float_val);
//		//This is temporary until I get the Tamura Current Sensors! -AB 6/17/2015
//		data[4]=0;
//		data[5]=0;
//		data[6]=0;
//		data[7]=0;
//		message_num=6;
//		CANsend(data, &dlc, &message_num);
//		while (wait_for_tx_finished)
//		    		{
//		    		}
//	}
//
//
//


}


//***************************************************************************************************************************************************
//ALL IRQ HANDLERS ARE HERE
//***************************************************************************************************************************************************



void SysTick_Handler(void)
{
	Chip_WWDT_Feed(LPC_WWDT);
}

/**
 * @brief	CCAN Interrupt Handler
 * @return	Nothing
 * @note	The CCAN interrupt handler must be provided by the user application.
 *	It's function is to call the isr() API located in the ROM. After the isr() is called,
 *	appropriate action according to the data received and the status detected on the CAN bus
 */
void CAN_IRQHandler(void)
{
	LPC_CCAN_API->isr();
}

/* @brief clears the delay flag after x the delay function is initiated
 * @param none
 * @return none
 */
void TIMER16_0_IRQHandler(void)
{

	delay_flag=0;
	Chip_TIMER_ClearMatch(LPC_TIMER16_0, 0);
	NVIC_DisableIRQ(TIMER_16_0_IRQn);
}

/* @brief periodically blinks the LED, this may be a temporary interrupt
 * @param none
 * @return none
 */
void TIMER32_0_IRQHandler (void)
{

	//Clear the appropriate bit after this functions is done
	Chip_TIMER_ClearMatch(LPC_TIMER32_0, 0);
	Dynamic_count++;

	if(Dynamic_count>(DYNAMIC_PERIODIC_DELAY+1))
		Dynamic_count=0;
	switch(MPPT_mode)
	{
		case 0://Static algorithm
			LED_ON(BLUE);
			if(Chip_GPIO_GetPinState(LPC_GPIO, 2, 8))
			{
				LED_OFF(GREEN);
			}
			else LED_ON(GREEN);
			LED_OFF(ORANGE);
			LED_OFF(RED);
			LED_OFF(YELLOW);
			break;
		case 1: //Dynamic algorithm
			LED_ON(ORANGE);
			if(Chip_GPIO_GetPinState(LPC_GPIO, 2, 8))
			{
				LED_OFF(GREEN);
			}
			else LED_ON(GREEN);
			LED_OFF(BLUE);
			LED_OFF(RED);
			LED_OFF(YELLOW);
			break;
		case 2: //IV_trace
			LED_ON(YELLOW);
			LED_OFF(BLUE);
			LED_OFF(RED);
			LED_OFF(ORANGE);
			LED_OFF(BLUE);
		case 3: //Error state
			if(Chip_GPIO_GetPinState(LPC_GPIO, 2, 8))
			{
				LED_OFF(RED);
			}
			else LED_ON(RED);
			LED_OFF(BLUE);
			LED_OFF(YELLOW);
			LED_OFF(ORANGE);
			LED_OFF(BLUE);
			break;

	}

//	//Toggle the appropriate LED color
//
////	if(toggleLED[0])
////	{
////		LED_Toggle(RED);
////	}
////	else LED_OFF(RED);
////
////	if(toggleLED[1])
////	{
////		LED_Toggle(GREEN);
////	}
////	else LED_OFF(GREEN);
////
////	if(toggleLED[2])
////	{
////		LED_Toggle(YELLOW);
////		toggleLED[2]=0;
////	}
////	else LED_OFF(YELLOW);
////
////	if(toggleLED[3])
////	{
////		LED_Toggle(BLUE);
////	}
////	else LED_OFF(BLUE);
////
////	if(toggleLED[4])
////	{
////		LED_Toggle(ORANGE);
////	}
////	else LED_OFF(ORANGE);
//
//			if(Chip_GPIO_GetPinState(LPC_GPIO, 2, 8))
//			{
//				LED_OFF(RED);
//			}
//			else LED_ON(RED);
//
//
//			//TODO: LED_Toggle not working :( -AB 6/6/2015
//			//LED_Toggle(RED);
//
////			LED_Toggle(GREEN);
////
////			LED_Toggle(YELLOW);
////
////			LED_Toggle(BLUE);
////
////			LED_Toggle(ORANGE);
//
//			if(Chip_GPIO_GetPinState(LPC_GPIO, 2, 8))
//						{
//							LED_OFF(GREEN);
//						}
//						else LED_ON(GREEN);
//
//			if(Chip_GPIO_GetPinState(LPC_GPIO, 2, 8))
//						{
//							LED_OFF(YELLOW);
//						}
//						else LED_ON(YELLOW);
//
//			if(Chip_GPIO_GetPinState(LPC_GPIO, 2, 8))
//						{
//							LED_OFF(BLUE);
//						}
//						else LED_ON(BLUE);
//
//			if(Chip_GPIO_GetPinState(LPC_GPIO, 2, 8))
//						{
//							LED_OFF(ORANGE);
//						}
//						else LED_ON(ORANGE);

}

void	TIMER16_1_IRQHandler (void)
{
	CAN_send_count++;
	//TODO: CAN send function
	Chip_TIMER_ClearMatch(LPC_TIMER16_1, 0);
//	if(CAN_send_count==CAN_SEND_WAIT)
//	{
//		CAN_send_count=0;
//		//Status message
//		if(message_num==0)
//		{
//			data[0]=MPPT_mode;
//			data[1]=Arrayside_over;
//			data[2]=Batteryside_over;
//			data[3]=0;
//			data[4]=0;
//			data[5]=0;
//			data[6]=0;
//			data[7]=0;
//
//			CANsend(data, &dlc, &message_num);
//
//		}
//
//		//Voltage input, current input
//		if(message_num==1)
//		{
//			Read_ADC_Channel(ADC_CH1,&int_input_voltage);//Input Voltage
//			ConvertADCvoltage_to_realvoltage(&int_input_voltage, &f_input_voltage, input);
//			turnFloatIntoChar(data, f_input_voltage);
//
//			Read_ADC_Channel(ADC_CH6, &int_input_current);//Input Current
//			ConvertADCcurrent_to_realcurrent(&int_input_current, &f_input_current);
//			turnFloatIntoChar(&data[4], f_input_current);
//
//			CANsend(data, &dlc, &message_num);
//
//		}
//		float test_voltage_in=f_input_voltage;
//		//Voltage output, current output
//		if(message_num==2)
//		{
//			Read_ADC_Channel(ADC_CH3,&int_output_voltage);//Output Voltage
//			ConvertADCvoltage_to_realvoltage(&int_output_voltage, &f_output_voltage, output);
//			turnFloatIntoChar(data, f_output_voltage);
//
//			Read_ADC_Channel(ADC_CH0, &int_output_current);//Output Current
//			ConvertADCcurrent_to_realcurrent(&int_output_current, &f_output_current);
//			turnFloatIntoChar(&data[4], f_output_current);
//
//			CANsend(data, &dlc, &message_num);
//
//			//message_num=0;//go back to sending the status message, useful for Helianthus REV 2.2 only
//		}
//		//float test_voltage_out=f_output_voltage;
//
//		//This last message is for the board voltage and board current, this will not be implemented for Helianthus REV 2.2
//	//	if(message_num==3)
//	//	{
//	//
//	//	}
//
//		message_num++;
//
//
//
//		if(message_num==3)
//		{
//					message_num=0;//IV trace does not send voltage output and current output
//		}
//		while (wait_for_tx_finished)
//			{
//
//			}
//	}

}


