/*
 * @file : main.c
 * @author : Avinashee Tech
 * @brief : an implementation of custom Infrared Remote made to work with multiple device
 *          using ATtiny85 and Atmel Studio platform
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#define F_CPU 8000000                      //main cpu clock - 8Mhz

#include <util/delay.h>
#include <avr/cpufunc.h>

#define  AC_DEFAULT_TEMP    22             //default temperature when air conditioner turned on

//attiny85 pin bit map
typedef union{
	struct{
		uint8_t pin0 :1;
		uint8_t pin1 :1;
		uint8_t pin2 :1;
		uint8_t pin3 :1;
		uint8_t pin4 :1;
		uint8_t pin5 :1;
	};
	uint8_t pinB;
}pin_status_t;

//device enum
typedef enum{
	TV,
	AC,
}device_t;

//command enum
typedef enum{
	POWER_ON,
	POWER_OFF,
	PLUS,
	MINUS,
}ir_commands_t;

//variables
static uint8_t temperature = AC_DEFAULT_TEMP;
uint32_t millis = 0;
uint8_t last_button_state[3] = {1,1,1};           //by default all buttons are high
uint8_t current_button_state[3] = {1,1,1};
volatile uint8_t button_ispressed[3] = {0};

//function declaration
void high_pulse(void);
void low_pulse(void);
void send_one(void);
void send_zero(void);
void send_irdata(uint64_t data,int data_count);
void ir_start(void);
void ir_sequence_AC(uint64_t dataA, uint64_t dataB, int data_countA, int data_countB);
void ir_sequence_TV(uint16_t address, uint16_t command, uint16_t address_count, uint16_t command_count);
void ir_sequence_ACtemp(uint8_t temperature);
void ir_sequence(device_t device,ir_commands_t command);


/***********************************************************************
 *@brief : TIM1 ISR function 
 *@param : TIM OVF interrupt vector
 *@retval : None
 *@note : counter for milliseconds
************************************************************************/
ISR(TIM1_OVF_vect){
	millis++;
}

/***********************************************************************
 *@brief : main function 
 *@param : None
 *@retval : None
 *@note : perform remote functions (power on/off,temperature/volume minus,
  temperature/volume plus) based on button pressed and send respective
  commands 
************************************************************************/
int main(void)
{
    /* Replace with your application code */
	    pin_status_t portb;
	    device_t device;
		ir_commands_t command; 
		
		uint32_t button1_millis = 0;
		uint32_t button2_millis = 0;
		uint32_t button3_millis = 0;
		uint8_t debounceDelay = 60;    // the debounce time in ms
    
		/***************************Pinout**************************
		************************PB0 - Timer0************************
		******************PB2 - Toggle Switch(TV/AC)****************
		******************PB1 - Power Button(TV/AC)*****************
		*****************PB4 - Volume+(TV)/Temp+(AC)****************
		*****************PB3 - Volume-(TV)/Temp-(AC)****************
		***********************************************************/
		MCUCR &= ~(1<<PUD);
		DDRB &= ~(1<<DDB2) | ~(1<<DDB3) | ~(1<<DDB4) | ~(1<<DDB1) ;   //PB1 PB2 PB3 PB4 as input pins 
		PORTB |=  (1<<PB2);                                           //pullup enable for PB2 
		
		/*Timer 0 (PWM output)*/
		TCNT0 = 0;            //Timer0 counter init
		TCCR0A=0;             //Timer0 output compare init
		TCCR0B=0; 
		
		TCCR0A |=(1<<COM0A0); //Timer0 in toggle mode 
		TCCR0A |=(1<<WGM01);  //Start timer 0 in CTC mode
		TCCR0B |= (1 << CS00);
		
		/*Timer 1 (millis counter)*/
		TCNT1 = 0;
		TCCR1 |= (1<<CS12) | (1<<CS11);     //Prescaler - 32, ARR - 256
		cli();
		TIMSK |= (1<<TOIE1);                //Overflow interrupt 
		sei();
		
		
		while(1)
		{
		    static uint8_t power_button_state = 0;
			
			portb.pinB = (PINB);         //read gpio portB
			
			//select device to be controlled (TV or AC) PB2 toggle switch
 			if(portb.pin2==0){           
 				device = TV;  
 			}else{
 				device = AC;
 			}
			
			/******button sequence******/
			if(portb.pin1!=last_button_state[0]){           //PB1 button pressed
				button1_millis = millis;
				current_button_state[0] = 0;
			}else if(portb.pin3!=last_button_state[1]){     //PB3 button pressed
				button2_millis = millis;
				current_button_state[1] = 0;
			}else if(portb.pin4!=last_button_state[2]){     //PB4 button pressed
				button3_millis = millis;
				current_button_state[2] = 0;
			}else{
				_NOP();                                     
			}
			
			if((millis-button1_millis)>debounceDelay){
				
				if(current_button_state[0]==0){					
					button_ispressed[0] = 1;
				}
			}
			
			if((millis-button2_millis)>debounceDelay){
				
				if(current_button_state[1]==0){					
					button_ispressed[1] = 1;
				}
			}
			
			if((millis-button3_millis)>debounceDelay){
				
				if(current_button_state[2]==0){					
					button_ispressed[2] = 1;
				}
			}
			
			if(button_ispressed[0]==1){        //PB1 button
				//_delay_ms(100);
 				if(power_button_state==0){     //turn on command
  					command = POWER_ON;
  					ir_sequence(device,command);
  					
  					power_button_state=1;
				}else{                         //turn off command
  					command = POWER_OFF;
 					ir_sequence(device,command);
 					  					
  					power_button_state=0;      
				}
				
				button_ispressed[0] = 0;
				current_button_state[0] = 1;
		   }else if(button_ispressed[1]==1){     //PB3 button
				//_delay_ms(100);
			    command = MINUS;                 //temperature/volume minus
			    ir_sequence(device,command);
				
				button_ispressed[1] = 0;
				current_button_state[1] = 1;
		   }else if(button_ispressed[2]==1){     //PB4 button 
 				//_delay_ms(100);
				command = PLUS;                  //temperature/volume plus
				ir_sequence(device,command);	
				
				button_ispressed[2] = 0;
				current_button_state[2] = 1;
    	   }else{
				_NOP();
		   }
			
        }
	return 0;
}



/***********************************************************************
 *@brief : helper function to generate high pulse 
 *@param : None
 *@retval : None
************************************************************************/
void high_pulse(void){
	TCNT0 = 0;
	OCR0A=104; //CTC Compare value
}

/***********************************************************************
 *@brief : helper function to generate low pulse 
 *@param : None
 *@retval : None
************************************************************************/
void low_pulse(void){
	TCNT0 = 0;
	OCR0A=0; //CTC Compare value
}

/***********************************************************************
 *@brief : helper function to send one in IR data
 *@param : None
 *@retval : None
************************************************************************/
void send_one(void){
	high_pulse();     //mark
	_delay_us(560);
	low_pulse();    //space
	_delay_us(1680);
}

/***********************************************************************
 *@brief : helper function to send zero in IR data
 *@param : None
 *@retval : None
************************************************************************/
void send_zero(void){
	high_pulse();     //mark
	_delay_us(560);
	low_pulse();    //space
	_delay_us(560);
}

/***********************************************************************
 *@brief : function to send final prepared data using helper functions 
 *@param : prepared data as per protocol for device, size of data
 *@retval : None
************************************************************************/
void send_irdata(uint64_t data,int data_count){
	for(int i=0;i<data_count;i++){
		if(data&0x01){
			send_one();
		}else{
			send_zero();
		}
		
		data = data>>1;
	}
}

/***********************************************************************
 *@brief : IR start sequence function
 *@param : None
 *@retval : None
************************************************************************/
void ir_start(void){
	//start
	high_pulse();     //mark
	_delay_us(8992);
	low_pulse();    //space
	_delay_us(4496);
}

/***********************************************************************
 *@brief : function to implement protocol of AC
 *@param : first set of data, second set of data, size of first set of data,
  size of second set of data
 *@retval : None
 *@note : prepares the final data to be sent as per AC IR protocol 
************************************************************************/
void ir_sequence_AC(uint64_t dataA, uint64_t dataB, int data_countA, int data_countB){    
	DDRB |= (1<<DDB0);        //Enable PB0 as output
	
	ir_start();
	//Data - LSB first
	send_irdata(dataA,data_countA);
	//end
	high_pulse();     //mark
	_delay_us(560);
	//repeat
	low_pulse();     //space
	_delay_us(20654);
	high_pulse();     //mark
	_delay_us(560);
	low_pulse();     //space
	_delay_us(1680);
	//Data - LSB first
	send_irdata(dataB,data_countB);
	//end
	high_pulse();     //mark
	_delay_us(560);
	//repeat
	low_pulse();     //space
	
	DDRB &= ~(1<<DDB0);        //Disable PB0 as output

}

/***********************************************************************
 *@brief : function to implement NEC protocol of TV 
 *@param : device address, command to be sent, size of address, size of 
  command
 *@retval : None
 *@note : prepares the final data to be sent as per TV IR protocol
************************************************************************/
void ir_sequence_TV(uint16_t address, uint16_t command, uint16_t address_count, uint16_t command_count){    
	DDRB |= (1<<DDB0);        //Enable PB0 as output
	
	//start
	ir_start();
	//Address 
	send_irdata(address,address_count);
	//command 
	send_irdata(command,command_count);
	//end
	high_pulse();     //mark
	_delay_us(560);
	//repeat
	low_pulse();     //space
	_delay_us(41425);
	high_pulse();     //mark
	_delay_us(8992);
	low_pulse();     //space
	_delay_us(2250);
	high_pulse();     //mark
	_delay_us(560);
	//repeat again
	low_pulse();     //space
	_delay_us(96293);
	high_pulse();     //mark
	_delay_us(8992);
	low_pulse();     //space
	_delay_us(2250);
	high_pulse();     //mark
	_delay_us(562);
	low_pulse();     //space
	
	DDRB &= ~(1<<DDB0);        //Disable PB0 as output
	
}

/***********************************************************************
 *@brief : function to control air conditioner temperature
 *@param : temperature to be set
 *@retval : None
 *@note : sends command for the specific temperature
************************************************************************/
void ir_sequence_ACtemp(uint8_t temperature){
	switch(temperature){
		case 16:{
			       //Data - 0x250200079 LSB first, 0x30001008  LSB first
			       ir_sequence_AC(0x250200079,0x30001008,35,31);  
		        }break;
		case 17:{
			       //Data - 0x250200179 LSB first, 0x38001008  LSB first
			       ir_sequence_AC(0x250200179,0x38001008,35,31);
		        }break;
	    case 18:{
			       //Data - 0x250200279 LSB first, 0x40001008  LSB first
			       ir_sequence_AC(0x250200279,0x40001008,35,31);
		        }break;
		case 19:{
			       //Data - 0x250200379 LSB first, 0x48001008  LSB first
			       ir_sequence_AC(0x250200379,0x48001008,35,31);
		        }break;
	    case 20:{
			       //Data - 0x250200479 LSB first, 0x50001008  LSB first
			       ir_sequence_AC(0x250200479,0x50001008,35,31);
		        }break;
		case 21:{
			       //Data - 0x250200579 LSB first, 0x58001008  LSB first
			       ir_sequence_AC(0x250200579,0x58001008,35,31);
		        }break;
		case 22:{
			       //Data - 0x250200679 LSB first, 0x60001008  LSB first
			       ir_sequence_AC(0x250200679,0x60001008,35,31);
		        }break;
		case 23:{
			       //Data - 0x250200779 LSB first, 0x68001008  LSB first
			       ir_sequence_AC(0x250200779,0x68001008,35,31);
		        }break;
		case 24:{
			       //Data - 0x250200879 LSB first, 0x70001008  LSB first
			       ir_sequence_AC(0x250200879,0x70001008,35,31);
		        }break;
		case 25:{
			       //Data - 0x250200979 LSB first, 0x78001008  LSB first
			       ir_sequence_AC(0x250200979,0x78001008,35,31);
		        }break;
		case 26:{
			       //Data - 0x250200A79 LSB first, 0x60001008  LSB first
			       ir_sequence_AC(0x250200A79,0x60001008,35,31);
		        }break;
		case 27:{
			       //Data - 0x250200B79 LSB first, 0x8001008  LSB first
			       ir_sequence_AC(0x250200B79,0x8001008,35,31);
		        }break;
		case 28:{
			       //Data - 0x250200C79 LSB first, 0x10001008  LSB first
			       ir_sequence_AC(0x250200C79,0x10001008,35,31);
		        }break;
		case 29:{
			       //Data - 0x250200D79 LSB first, 0x18001008  LSB first
			       ir_sequence_AC(0x250200D79,0x18001008,35,31);
		        }break;
		case 30:{
			       //Data - 0x250200E79 LSB first, 0x20001008  LSB first
			       ir_sequence_AC(0x250200E79,0x20001008,35,31);
		        }break;
		default:
		         break;
	}
}

/***********************************************************************
 *@brief : function to call IR command sequence based on device selected
 *@param : device selected, command to perform
 *@retval : None
 *@note : send command based on device selected and button pressed
************************************************************************/
void ir_sequence(device_t device,ir_commands_t command){
	if(device==AC){
		if(command==POWER_ON){           //send power on for AC command
			//Data - 0x250200659 LSB first, 0x60001008  LSB first
			ir_sequence_AC(0x250200659,0x60001008,35,31);
		}else if(command==POWER_OFF){    //send power off for AC command
			//Data - 0x250200651 LSB first 0x20001008  LSB first
			ir_sequence_AC(0x250200651,0x20001008,35,31);
		}else if(command==PLUS){
			if(temperature<30){          //max temperature - 30 degree
				temperature++;
			}
			ir_sequence_ACtemp(temperature);
		}else{
			if(temperature>16){          //min temperature - 16 degree
				temperature--;
			}
			ir_sequence_ACtemp(temperature);
		}
		
	}else if(device==TV){
		if((command==POWER_ON) || (command==POWER_OFF)){
			//Data - address & it's inverse - 0x7F  0x00 LSB first, command & it's inverse - 0xF5  0x0A  LSB first 
			ir_sequence_TV(0x7F00,0xF50A,16,16);
		}else if(command==PLUS){
			//Data - address & it's inverse - 0x7F  0x00 LSB first, command & it's inverse - 0xA7  0x58  LSB first
			ir_sequence_TV(0x7F00,0xA758,16,16);
		}else{
			//Data - address & it's inverse - 0x7F  0x00 LSB first, command & it's inverse - 0xA4  0x5B  LSB first
			ir_sequence_TV(0x7F00,0xA45B,16,16);
		}
		
	}else{
		_NOP();
	}
}