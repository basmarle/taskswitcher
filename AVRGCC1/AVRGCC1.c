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
volatile int current_task = 0;
volatile int firstrun = 1;

typedef struct{
	int taskClockCount;
} kernel_settings_t;

typedef struct{
	int id;
	int sph;
	int spl;
	int flags;
	int pcl;
	int pch;
	void * address;
	int firsttime;
	
} task_table_t;

task_table_t task_table[MAX_TASKS];
kernel_settings_t kernel_settings;

ISR (TIMER1_COMPA_vect) __attribute__ ((naked));

void * task1(){
	long a = 0;
	while(1){
		a++;
		//if(a == 5000){
			PORTB = (0 << PINB5);
			a = 0;
		//}
		
	}
}

void * task2(){
	long b = 0;
	while(1){
		b++;
		//if(b == 5000){
			PORTB = (1 << PINB5);
			b = 0;
		//}
	}
}

void initTask(void * taskAddress){
	static int taskCount = 0;
	
	//Set ID of task
	task_table[taskCount].id = taskCount;
	//Start address of the task
	task_table[taskCount].address = taskAddress;
	//Split into 2 bytes
	task_table[taskCount].pcl = ((int)task_table[taskCount].address & 0x00FF);
	task_table[taskCount].pch = ((int)task_table[taskCount].address & 0xFF00) >> 8;
	//Set taskpointer
	task_table[taskCount].spl = (0x7D0 - (taskCount * 0x64)) & 0x00FF;
	task_table[taskCount].sph = ((0x7D0 - (taskCount * 0x64)) & 0xFF00) >> 8;
	task_table[taskCount].firsttime = 1;
	
	taskCount++;
}

int main(void)
{
	cli();//stop interrupts
	
	//All pins in PORTD are outputs
	DDRB = 0b11111111;    
	//Set the tik count for each task
	kernel_settings.taskClockCount = 100;

	TCCR1A = 0;// set entire TCCR1A register to 0
	TCCR1B = 0;// same for TCCR1B
	TCNT1  = 0;//initialize counter value to 0
	OCR1A = kernel_settings.taskClockCount;
	// turn on CTC mode
	TCCR1B |= (1 << WGM12);
	// Set bit for prescaler
	TCCR1B |= (1 << CS10);
	// enable timer compare interrupt
	TIMSK1 |= (1 << OCIE1A);
	
	
	initTask(task1);
	initTask(task2);

	sei();//allow interrupts
	
	while (1)
	{
		// we have a working Timer
		counter++;
	}
}



ISR (TIMER1_COMPA_vect)
{
	//Disable interrupts
	cli();
	
	//Do not switch context on first run
	if(firstrun){
		//Only run once
		firstrun = 0;
		//Set stack pointer
		asm volatile("MOV r0, %[lowAddress] " :: [lowAddress] "r" (task_table[0].spl));
		asm volatile("OUT __SP_L__, r0");
		asm volatile("MOV r0, %[highAddress] " :: [highAddress] "r" (task_table[0].sph));
		asm volatile("OUT __SP_H__, r0");
		
		asm("MOV R18, %[lowAdress]" :: [lowAdress] "r" (task_table[0].pcl));
		asm("PUSH R18");
		asm("MOV R18, %[highAdress]" :: [highAdress] "r" (task_table[0].pch));
		asm("PUSH 18");
		task_table[0].firsttime = 0;
		
	} else {
			
		//Save context
		asm volatile (
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
		"push  r18                   \n\t" \
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
		"push  r31                   \n\t");
			
		//Store stackpointer in TCB
		asm volatile("in    r0, __SP_L__");
		asm volatile("MOV %[lowAdress], r0 ": [lowAdress] "=r" (task_table[current_task].spl) : );
		asm volatile("in    r0, __SP_H__");
		asm volatile("MOV %[highAdress], r0 ": [highAdress] "=r" (task_table[current_task].sph) : );
		
		//Select the current task
		if(current_task == 0){
			current_task = 1;
			}else if(current_task == 1){
			current_task = 0;
		}
		
		if(task_table[current_task].firsttime){
			asm volatile("MOV r0, %[lowAddress] " :: [lowAddress] "r" (task_table[current_task].spl));
			asm volatile("OUT __SP_L__, r0");
			asm volatile("MOV r0, %[highAddress] " :: [highAddress] "r" (task_table[current_task].sph));
			asm volatile("OUT __SP_H__, r0");
			
			asm("MOV R18, %[lowAdress]" :: [lowAdress] "r" (task_table[current_task].pcl));
			asm("PUSH R18");
			asm("MOV R18, %[highAdress]" :: [highAdress] "r" (task_table[current_task].pch));
			asm("PUSH 18");
			task_table[current_task].firsttime = 0;
		} else {
			//Load stack pointers
			asm volatile("MOV r0, %[lowAddress] " :: [lowAddress] "r" (task_table[current_task].spl));
			asm volatile("OUT __SP_L__, r0");
			asm volatile("MOV r0, %[highAddress] " :: [highAddress] "r" (task_table[current_task].sph));
			asm volatile("OUT __SP_H__, r0");
						
			//Load context
			asm volatile (
			"pop  r31                    \n\t" \
			"pop  r30                    \n\t" \
			"pop  r29                    \n\t" \
			"pop  r28                    \n\t" \
			"pop  r27                    \n\t" \
			"pop  r26                    \n\t" \
			"pop  r25                    \n\t" \
			"pop  r24                    \n\t" \
			"pop  r23                    \n\t" \
			"pop  r22                    \n\t" \
			"pop  r21                    \n\t" \
			"pop  r20                    \n\t" \
			"pop  r19                    \n\t" \
			"pop  r18                    \n\t" \
			"pop  r17                    \n\t" \
			"pop  r16                    \n\t" \
			"pop  r15                    \n\t" \
			"pop  r14                    \n\t" \
			"pop  r13                    \n\t" \
			"pop  r12                    \n\t" \
			"pop  r11                    \n\t" \
			"pop  r10                    \n\t" \
			"pop  r9                     \n\t" \
			"pop  r8                    \n\t" \
			"pop  r7                     \n\t" \
			"pop  r6                     \n\t" \
			"pop  r5                     \n\t" \
			"pop  r4                     \n\t" \
			"pop  r3                     \n\t" \
			"pop  r2                     \n\t" \
			"pop  r1                     \n\t" \
			"pop  r0                     \n\t" \
			"out  __SREG__, r0           \n\t" \
			"pop  r0                     \n\t" \
			);
		}
	}

	//Set clocktimer
	TCNT1  = 0;
	TIFR1 = 1 << 1;
	//Enable interrupts
	sei();
	//Return to task
	reti();
}

//Save context of the current task
void saveContext(int current_task){
	//Save all registers
	asm volatile (						\
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
	"push  r31                   \n\t");
	
	//Store stackpointer in TCB
	asm volatile("in    r0, __SP_L__");
	asm volatile("MOV %[lowAdress], r0 ": [lowAdress] "=r" (task_table[current_task].spl) : );
	asm volatile("in    r0, __SP_H__");
	asm volatile("MOV %[highAdress], r0 ": [highAdress] "=r" (task_table[current_task].sph) : );
}

void restoreContext(){
	
}