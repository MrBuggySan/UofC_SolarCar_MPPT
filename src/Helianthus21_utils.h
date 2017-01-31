/*
 * Helianthus21_utils.h
 *
 *  Created on: May 2, 2015
 *      Author: Andrei
 */

#ifndef AB_TEST_UTILS_H_
#define AB_TEST_UTILS_H_

typedef enum {output=1, input, V_12} eVoltagetype ;
typedef enum {ORANGE=1, BLUE, YELLOW, GREEN, RED} LED_COLOR ;
typedef enum {STATIC_=1,DYNAMIC_=2, IV_TRACE=3, OPEN=4} MPPT_Mode;


void Config_CLKOUTpin_(void);
void Config_Peripheral_pins_(void);
void Config_CT32B0(void);
void Config_CT16B0(void);
void Config_PWM_CT32B1_MAT1(uint16_t duty_cycle);
void Config_ADCs(void);
void Config_Watchdog(void);
void Config_CAN(void);
void Config_GPIO(void);
void Config_CT16B1(void);
void Configure_MPPT(uint16_t duty_cycle);
void Relay_OPEN(void);
void send_fucking_CAN(void);
void Relay_CLOSE(void);
void LED_ON(LED_COLOR color);
void LED_OFF(LED_COLOR color);
void LED_Toggle (LED_COLOR color);
void LED_TEST(void);
void Set_PWM_Duty_Cycle(uint16_t duty_cycle);
void Read_ADC_Channel(uint8_t channel, uint16_t *dataADC);
void Check_Voltage_Current_at_Input_Output(void);
unsigned long absdifference(unsigned long a, unsigned long b);
float fabsdifference(float a, float b);
void ConvertADCcurrent_to_realcurrent(uint16_t* current);
void ConvertADCvoltage_to_realvoltage(uint16_t* fake_voltage,float* real_voltage, eVoltagetype V_type);
void baudrateCalculate(uint32_t baud_rate, uint32_t *can_api_timing_cfg);
void CANsend(uint8_t* data_, uint8_t* dlc_, uint8_t* message_num);
void CAN_tx(uint8_t msg_obj_num);
void CAN_error(uint32_t error_info);
void CAN_rx(uint8_t msg_obj_num);
void Delay(void);//10ms delay
void Send_Status_to_CCS(void);
void Send_Data_to_CCS(MPPT_Mode modyyy);


#endif /* AB_TEST_UTILS_H_ */


