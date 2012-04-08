#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include "uart.h"

void setup(void);
void loop(void);

int main(void){
    setup();
    for(;;) loop();
}

void setup(){
    uart_init(UART_BAUD_SELECT_DOUBLE_SPEED(115201, F_CPU));
    //software PWM init FIXME
	DDRH |= 0x20;
	PORTH |= 0x20;
    sei();
}

//this scary construct is in place to make the compiler happy. if you know a better way, feel free to improve.
uint8_t _frameBuffer[] = {0,0,0,0};
uint8_t _secondFrameBuffer[] = {0,0,0,0};
uint8_t* frameBuffer = _frameBuffer;
uint8_t* secondFrameBuffer = _secondFrameBuffer;
char nbuf;
int state = 0;
int mode = 0; //for random: 1
uint8_t switch_last_state = 0;
int switch_debounce_timeout = 0;
uint8_t mcnt = 0;
uint8_t ccnt = 0;

uint8_t pwm_cycle = 0;
uint8_t pwm_val[4];

void swapBuffers(void){
    uint8_t* tmp = frameBuffer;
    frameBuffer = secondFrameBuffer;
    secondFrameBuffer = tmp;
}

void setLED(int num, int val){
    if(num<32){
        frameBuffer[num>>3] &= ~(1<<(num&7));
        if(val)
            frameBuffer[num>>3] |= 1<<(num&7);
    }
}

int parseHex(char* buf){
    int result = 0;
    int len = 2;
    for(int i=0; i<len; i++){
        char c = buf[len-i];
        int v = 0;
        if(c>='0' && c<='9'){
            v=(c-'0');
        }else if(c>='a' && c<= 'f'){
            v=(c-'a'+10);
        }else if(c>='A' && c<= 'F'){
            v=(c-'A'+10);
        }
        result |= v<<(4*i);
    }
    return result;
}

void loop(){ //one frame
    uint16_t receive_status = 1;
    do{ //Always empty the receive buffer since there are _delay_xxs in the following code and thus this might not run all that often.
        receive_status = uart_getc();
        char c = receive_status&0xFF;
        receive_status &= 0xFF00;
        //primitive somewhat messy state machine.
        //eats three commands: 0x73 led value                     sets led number [led] to [value]
        //                     0x62 buffer buffer buffer buffer   sets the whole frame buffer
        //                     0x72                               switch to random mode
        //                     0x61 meter value                   sets analog meter number [meter] to [value]
        if(!receive_status){
            switch(state){
                case 0:
                    switch(c){
                        case 's':
                            state = 2;
                            break;
                        case 'b':
                            nbuf = 0;
                            state = 4;
                            break;
                        case 'r':
                            mode = 1;
                            break;
                        case 'a':
                            state = 5;
                            nbuf = 0;
                            break;
                    }
                    break;
                case 2:
                    nbuf=c;
                    state = 3;
                    break;
                case 3:
                    setLED(nbuf, c);
                    mode = 0;
                    state = 0;
                    break;
                case 4:
                    secondFrameBuffer[(uint8_t) nbuf] = c;
                    nbuf++;
                    if(nbuf == 4){
                        swapBuffers();
                        mode = 0;
                        state = 0;
                    }
                    break;
                case 5:
                    nbuf = c;
                    state = 6;
                    if(nbuf >= 4) //prevent array overflows
                        nbuf = 0;
                    break;
                case 6:
                    pwm_val[(uint8_t) nbuf] = c;
                    mode = 0;
                    state = 0;
            }
        }
    }while(!receive_status);
	if(switch_debounce_timeout){
		_delay_us(50);
		switch_debounce_timeout--;
	}else{
		uint8_t switch_state = PINH & 0x20;
		if(switch_state != switch_last_state){
			if(switch_last_state){
				uart_puts_p(PSTR("herpderp\n"));
			}else{
				uart_puts_p(PSTR("kthxbye\n"));
			}
			switch_last_state = switch_state;
			switch_debounce_timeout = 10000;
		}
	}
}
