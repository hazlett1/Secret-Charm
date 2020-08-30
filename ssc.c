
\
#ifndef F_CPU
#define F_CPU 8000000L //Sets the CPU to 8 Megahertz
#endif

#include <util/delay.h> //Lets me delay the program
#include <avr/io.h> //AVR library that I use
#include <stdlib.h> //Standard C library
										//        ATtiny85
#define MICROBUTTON PB3 //Microphone will i/o with Port 3 or pin 2    	         PB5/Pin 1|-----|VCC/Pin 8
#define RESETBUTTON PB1 //The reset button will input from Port 2 or pin 7       PB3/Pin 2|o    |PB2/Pin 7
#define LATCH PB0 //Latch will output the lock from Port 0 or Pin 5	         PB4/Pin 3|     |PB1/Pin 6
#define MICROPHONE PB2 //Microphone will i/o from Port 2 or Pin 7                GND/Pin 4|-----|PB0/Pin 5
#define TROUBLESHOOT PB4 //Will help me troubleshoot problems

#define RESET_PRESSED !(PINB & (1 << RESETBUTTON)) //Checks if reset button is pressed
#define MICRO_PRESSED !(PINB & (1 << MICROBUTTON)) //Checks if button is pressed

#define DEBOUNCE_TIME 25 //Used to make sure the button was supposed to be pressed
#define LOCK_INPUT_TIME 300 //Used so it will give a delay before pressing the button again
#define UNLOCK_TIME 5000 //The amount of time it takes to unlock the necklace

#define MAX_SIZE 8175 //The max size of the password -- to the highest number possible
#define VOICE_BUFFER 10 //Checks how different the numbers can be
#define BACKGROUND_SOUND_BUFFER 50//Buffer zone for background noise
#define MAX_ERRORS 10 //Buffer on how many times the numbers don't align
#define AVG_SIZE 10 //Take the average of x amount of raw data to be more accurate

//Declaring functions
void init_ADC();
void init_ports_mcu();
unsigned char microButton_state();
void micro_pressed(unsigned short *password, unsigned short password_peak, unsigned short *guess, unsigned char *unlocked);
void reset_pressed(unsigned short *password, unsigned short *new_password, unsigned short *password_peak);
void clean_array(unsigned short *arr, unsigned short size, unsigned short *peak_location);
unsigned short Read_ADC();
unsigned char closeTo(unsigned short guess, unsigned short low, unsigned short high);
void lock();
void unlock();
unsigned short avg(unsigned short *array);

void set_password(unsigned short *password, unsigned short *new_password, unsigned short size){ //Sets the password that will unlock the necklace

	unsigned short i;
	password = (unsigned short*) realloc(password, size * sizeof(short)); //Reshape the size of the password to the new password

	for(i = 0; i < size; i++){ //Copies the new password to the password array
		password[i] = new_password[i];
	}
}

unsigned short collect_phrase(unsigned short *arr){

        arr = (unsigned short*) realloc(arr, MAX_SIZE * sizeof(short)); //Make the array have the most possible slots
	unsigned short counter = 0, tmp_counter = 0; //Incrementer
	unsigned short peak = 0, peak_location = 0; //Used to find peak of the graph for comparison
	unsigned short tmp[AVG_SIZE]; //Array to hold raw data before average

	while(MICRO_PRESSED && counter < MAX_SIZE){ //Loops while the button is pressed or until storage is full
		PORTB |= (1 << TROUBLESHOOT);
		if(tmp_counter < AVG_SIZE){ //Takes the average of 10 raw data to be more presice
			arr[counter] = avg(tmp); //Sets it as the average

			if(arr[counter] > peak){ //Finds the peak of the graph and saves loctaion
				peak = arr[counter]; //Peak value
				peak_location = counter; //Peak location
			}

			counter++; //Increment
			tmp_counter = 0; //Reset tmp increment
		}

		tmp[tmp_counter] = Read_ADC(); //Read the analog data
		tmp_counter++; //Increment
	}
	PORTB &= ~(1 << TROUBLESHOOT);
	_delay_ms(1000);
	clean_array(arr, counter, &peak_location); //Clean the array

	return peak_location; //Return the peak
}

unsigned char is_phrase(unsigned short *password, unsigned short password_peak, unsigned short *Guess, unsigned short guess_peak){// Checks to see if the input is correct

/*
		Idea
		write code that detects the highest peaks of each of the passwords and
		then align them to eachother and compare hos similar the graphs are from there
*/
	unsigned short i, lowSize;
	unsigned char flag = 0;
	short peak_diff = password_peak - guess_peak;
	unsigned short shift_counter = 0;

	if(peak_diff > 0){ //Adds values to guess to shift array right to alogn the peaks
		unsigned short tmp[peak_diff]; //Temp array to shift values in the guess array
		Guess = (unsigned short*) realloc(Guess, (sizeof(Guess) + peak_diff) * sizeof(short)); //Resize the guess array to the new size
		for(i = 0; i < sizeof(Guess); i++){ //loops through the values
			if(i < peak_diff){ //Adds 0 to he start of array to shift he values right to align peaks
				tmp[i] = Guess[i]; //Temporarily holds the values
				Guess[i] = 0;
			}
			else{
				unsigned short tmp2 = Guess[i]; //Temporarily holds 1 values
				if(shift_counter == sizeof(tmp)) //Resets the shift incrementer if at the end of array
					shift_counter = 0;
				Guess[i] = tmp[shift_counter]; //Sets the shifted value in the right spot
				tmp[shift_counter] = tmp2; //Puts the old value in the tmp array so it can be shifte
			}
		}

	}
	else{ //Gets rid of values aat the start of array to align the peaks of the graph

		for(i = 0; i < sizeof(Guess); i++) //Shifts all the values left to align the peaks
			Guess[i] = Guess[i + abs(peak_diff)];

		Guess = (unsigned short*) realloc(Guess, (sizeof(Guess) + peak_diff) * sizeof(short)); //Resize the guess array to the new size
	}

	if(sizeof(password) > sizeof(Guess)) //Compares the sizes of the arrays to avoid compile error
		lowSize = sizeof(password);
	else
		lowSize = sizeof(Guess);


	for(i = 0; i < lowSize; i++){

		if(!closeTo(Guess[i], password[i] - VOICE_BUFFER, password[i] + VOICE_BUFFER)) //Checks to see if the values are close
			flag++; //Flag count

		if(flag > MAX_ERRORS) //If the numbers don't align past MAX_ERROR count it is false
			return 0;
	}
	return 1; //If it makes it to this point the passwords are similar
}

int main(void){

	init_ports_mcu(); //Sets the digital i/o
	init_ADC(); //Sets the analog input

	unsigned short password[MAX_SIZE]; //Array saved for the password
	unsigned short array[MAX_SIZE]; //Array used for guess or setting the new password
	unsigned char unlocked = 0; //True or False on if necklace is unlocked
	unsigned short password_peak;

	while(1){ //Constantly run the loop

		if(RESET_PRESSED){ //Reset button pressed

			_delay_ms(DEBOUNCE_TIME);
			reset_pressed(password, array, &password_peak); //Run the reset fuction
		}

		if(microButton_state()) //Checks to see if the button was pressed
			micro_pressed(password, password_peak, array, &unlocked);
	}
	return 0;
}

unsigned short Read_ADC(){

        uint8_t low; //Will collect the left hand bits 0 - 7
        uint16_t adc; //Will combine the right hand and left hand bits 9 & 8 and 7 - 0

        ADCSRA |= (1 << ADSC); //ADSC - Analog Digital Start Conversion
        while(ADCSRA & (1 << ADSC)); //Loop around until conversion is finished

        low = ADCL; //Set the left hand bits to low
        adc = ADCH << 8 | low; //Combine all the bits together

        return adc; //Returns the number 0 - 1024
}

void micro_pressed(unsigned short *password, unsigned short password_peak, unsigned short *guess, unsigned char *unlocked){
	unsigned short guess_peak;
	PORTB |= (1 << TROUBLESHOOT);
	if(!*unlocked){ //If the necklace is locked loop
		guess_peak = collect_phrase(guess); //Collects the data

		if(is_phrase(password, password_peak, guess, guess_peak)){ //If they are similar unlock the necklace
		        unlock();
                	*unlocked = 1; //Set unlocked to True
        	}
	}

        if(*unlocked && MICRO_PRESSED){ //If the button is pressed after unlocked lock the necklace again
        	lock();
		*unlocked = 0; //Set to False
	}

	_delay_ms(LOCK_INPUT_TIME); //Locks inputs so there is a hesitantion on next reading
}

void reset_pressed(unsigned short *password, unsigned short *new_password, unsigned short *password_peak){

	unsigned char button_held = 0, reset = 0;

	while(RESET_PRESSED){ //Runs a loop to track how long the button is being pressed
		button_held++; //counter
                _delay_ms(10);

			/*SET BACK TO 120*/
                if(button_held > 10){
                	reset = 1; //Rest button was held for 5 seconds
                        break;
                }

        }

        if(reset){ //If needed to reset
		while(MICRO_PRESSED == 0){
 	                PORTB |= (1 << TROUBLESHOOT); //Turn On LED ---- This is for troubleshooting
        	        _delay_ms(100);
			PORTB &= ~(1 << TROUBLESHOOT); //Turn off LED
              		_delay_ms(100);
		}
		*password_peak = collect_phrase(new_password); //Collect and set the new password
	        set_password(password, new_password, sizeof(new_password)); //Set new phrase
      	}
        reset = 0; //Set back to false
	_delay_ms(LOCK_INPUT_TIME); //Locks input so there is a hesitation on the next reading
}

void lock(){
	PORTB |= (1 << PB5); //Turn lock on
}

void unlock(){
	PORTB |= (1 << PB5); //Turn unlock on
}

unsigned char microButton_state(){

        if(MICRO_PRESSED){

                _delay_ms(DEBOUNCE_TIME); //Delays the next read of the button so it is accurate
                if(MICRO_PRESSED)
                        return 1; //Returns true;
        }
        return 0; //returns false
}

void clean_array(unsigned short *array, unsigned short released, unsigned short *peak_location){

	array = (unsigned short*) realloc(array, released * sizeof(short)); //Set the size of array to when the button was released
	unsigned short i = 0, start;
	PORTB |= (1 << TROUBLESHOOT);
//	_delay_ms(1000);
	while(1){
		PORTB &= ~(1 << TROUBLESHOOT);
		_delay_ms(100);
		if(array[i] > BACKGROUND_SOUND_BUFFER){ //If a sound is louder than the buffer then the array starts
			start = i;
			break;
		}
		i++;
		PORTB |= (1 << TROUBLESHOOT);
		_delay_ms(100);
	}
	PORTB &= ~(1 << TROUBLESHOOT);
	*peak_location -= start; //Changes where the peak location is saved in array

	for(i = 0; i < sizeof(array) - start; i++) //Copy where the sound starts to the front of the array
		array[i] = array[start + i];

	array = (unsigned short*) realloc(array, (sizeof(array) - start) * sizeof(short)); //Reshape the size to what is needed
}

unsigned short avg(unsigned short *array){
	unsigned short i, sum = 0, size = sizeof(array);

	for(i = 0; i < size; i++)
		sum += array[i];

	return sum / size;
}

unsigned char closeTo(unsigned short guess, unsigned short low, unsigned short high){

	if(guess > low && guess < high)
		return 1;
	return 0;

}

void init_ADC(){

        ADMUX =
                (0 << ADLAR) | //Turns off or on the ADC left align
                (0 << REFS1) |
                (0 << REFS0) |
                (0 << MUX3)  |
                (0 << MUX2)  |
                (0 << MUX1)  |
                (1 << MUX0)  ;
/*
	ADMUX --- REFS
	| REFS1 | REFS0 | Voltage Reference 			   |
	|   0   |   0   | AREF, Internal Vref = off 		   |
	|   0   |   1   | AVcc with external capacitor of AREF Pin |
	|   1   |   0   | Reserved				   |
	|   1   |   1   | Internal 1.1v (ATtiny85)		   |

	ADMUX --- MUX
	| Mux3 | MUX2 | MUX1 | MUX0 | Input |
	|  0   |  0   |  0   |  0   | ADC 0 |
	|  0   |  0   |  0   |  1   | ADC 1 |
	|  0   |  0   |  1   |  0   | ADC 2 |
	|  0   |  1   |  0   |  0   | ADC 3 |
*/

        ADCSRA =
                (1 << ADEN)  | //Enable ADC
                (1 << ADPS2) | //Set prescaler to 128, Bit 2
                (1 << ADPS1) | //Set prescaler to 128, Bit 1
                (1 << ADPS0) ; //Set prescaler to 128, Bit 0
/*
	ADCSRA --- ADPS
	| ADPS2 | ADPS1 | ADPS0 | Divsion Factor |
	|   0   |   0   |   0   |       2        |
	|   0   |   0   |   1   |       2	 |
	|   0   |   1   |   0   |	4	 |
	|   0   |   1   |   1   |	8	 |
	|   1   |   0   |   0   |	16	 |
	|   1   |   0   |   1   |	32	 |
	|   1   |   1   |   0   |	64	 |
	|   1   |   1   |   1   |	128	 |
*/
}

void init_ports_mcu(){

        DDRB = 0xFF; //Sets all the pins to output
        DDRB &= ~(1 << MICROBUTTON); //Sets button port to input
        DDRB &= ~(1 << RESETBUTTON); //Sets reset button port to input
        DDRB &= ~(1 << MICROPHONE);  //Sets the microphone pin to input

        PORTB = 0x00; //Sets all the Ports to LOW
        PORTB |= (1 << MICROBUTTON); //Sets the button pin to receive input
        PORTB |= (1 << RESETBUTTON); //Sets reset button pin to receive input
        PORTB |= (1 << MICROPHONE);  //Sets the microphone pin to receive input

}


