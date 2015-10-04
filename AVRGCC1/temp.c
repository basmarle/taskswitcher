/*
 * temp.c
 *
 * Created: 24-9-2015 20:57:48
 *  Author: Encya
 */ 
/*
 * AVRGCC1.c
 *
 * Created: 22-9-2015 12:52:06
 *  Author: Encya
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>

#define MAX_TASKS 2

volatile int counter = 0;

typedef struct{
	int id;
	void * sph;
	void * spl;
	int flags;
	char * functionL;
	char * functionH;
	void * address;
	int running;
	
} task_table_t;

task_table_t task_table[MAX_TASKS];

void * task1(){
	long a = 0;
	while(1){
		a++;
	}
}

void * task2(){
	long b = 0;
	while(1){
		b++;
	}
}

void initTask(void * taskAddress){
	static int taskCount = 0;
	
	task_table[taskCount].id = taskCount;
	task_table[taskCount].address = taskAddress;
	task_table[taskCount].functionL = ((int)task_table[taskCount].address & 0x00FF);
	task_table[taskCount].functionH = ((int)task_table[taskCount].address & 0xFF00) >> 8;
	task_table[taskCount].running = 0;
	
	taskCount++;
}

int main(void)
{
	cli();//stop interrupts
	int * value = 0;
	int tikkertje = 0;

	//set timer1 interrupt at 1Hz
	TCCR1A = 0;// set entire TCCR1A register to 0
	TCCR1B = 0;// same for TCCR1B
	TCNT1  = 0;//initialize counter value to 0
	// set timer count for 1khz increments
	OCR1A = 1999;// = (16*10^6) / (1000*8) - 1
	//had to use 16 bit timer1 for this bc 1999>255, but could switch to timers 0 or 2 with larger prescaler
	// turn on CTC mode
	TCCR1B |= (1 << WGM12);
	// Set CS11 bit for 64 prescaler
	TCCR1B |= (1 << CS11);
	// enable timer compare interrupt
	TIMSK1 |= (1 << OCIE1A);
	
	
	initTask(task1);
	initTask(task2);

	sei();//allow interrupts
	
	while (1)
	{
		// we have a working Timer
		tikkertje++;
		counter++;
	}
}



ISR (TIMER1_COMPA_vect)
{
	//Disable interrupts
	cli();
	static int current_task = -1;
	
	//Select the current task
	if(current_task == 1 || current_task == -1){
		current_task = 0;
	}else if(current_task == 0){
		current_task = 1;
	}

	task_table[current_task].running = 1;
	int status = 72;
	asm("LDI R18, %[highAdress]" :: [highAdress] "r" (status));
	
	/*if(task_table[0].running){
		task_table[0].running = 0;
		task_table[1].running = 1;
		asm("LDI R18, 0x64");
	}else{
		task_table[1].running = 0;
		task_table[0].running = 1;
		asm("LDI R18, 0x52");
	}*/
	
	//Enable interrupts
	sei();
	//Return to task
	reti();
}

void saveContext(){
	asm volatile (	                     \
	"push  r0                    \n\t" \
	"in    r0, __SREG__          \n\t" \
	"cli                         \n\t" \
	"push  r0                    \n\t" \
	"push  r1                    \n\t" \
	"clr   r1                    \n\t" \
	"push  r2                    \n\t" \
	"push  r3                    \n\t" \
	"push  r4                    \n\t" \
	"push  r5                    \n\t" \
	"push  r6                    \n\t" \
	"push  r7                    \n\t" \
	"push  r8                    \n\t" \
	"push  r9                    \n\t" \
	"push  r10                   \n\t" \
	"push  r11                   \n\t" \
	"push  r12                   \n\t" \
	"push  r13                   \n\t" \
	"push  r14                   \n\t" \
	"push  r15                   \n\t" \
	"push  r16                   \n\t" \
	"push  r17                   \n\t" \
	"push  r19                   \n\t" \
	"push  r20                   \n\t" \
	"push  r21                   \n\t" \
	"push  r22                   \n\t" \
	"push  r23                   \n\t" \
	"push  r24                   \n\t" \
	"push  r25                   \n\t" \
	"push  r26                   \n\t" \
	"push  r27                   \n\t" \
	"push  r28                   \n\t" \
	"push  r29                   \n\t" \
	"push  r30                   \n\t" \
	"push  r31                   \n\t" \
	"lds   r26, pxCurrentTCB     \n\t" \
	"lds   r27, pxCurrentTCB + 1 \n\t" \
	"in    r0, __SP_L__          \n\t" \
	"st    x+, r0                \n\t" \
	"in    r0, __SP_H__          \n\t" \
	"st    x+, r0                \n\t" \
	);
}

void restoreContext(){
	
}