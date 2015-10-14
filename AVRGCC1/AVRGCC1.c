/*
 * AVRGCC1.c
 *
 * Created: 22-9-2015 12:52:06
 *  Author: Bas van Marle
 */ 

#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#define MAX_TASKS 2

volatile int counter = 0;
volatile int current_task = 0;
volatile int firstrun = 1;

typedef struct{
	int taskClockCount;
} kernel_settings_t;

//Task Program Block
typedef struct{
	int id;
	//Stack pointer high
	int sph;
	//Stack pointer low
	int spl;
	int flags;
	//Program counter high
	int pcl;
	//Program counter low
	int pch;
	//First address of a task
	void * address;
	//Used so no context is loaded when a tasks runs for the first time
	int firsttime;
	//True if the task is finished and need to be removed
	int markedforremoval;
} task_table_t;

//Linked list for all the tasks
struct node{
	task_table_t * task;
	struct node * next;
};

struct node *root;
struct node *current_node;
struct node *previous_node;

task_table_t task_table[MAX_TASKS];
kernel_settings_t kernel_settings;

//Declare the ISR as a naked function
ISR (TIMER1_COMPA_vect) __attribute__ ((naked));

//Task1
void * task1(){
	long a = 0;
	while(1){
		a++;
		PORTB = (0 << PINB5);
		a = 0;
	}
}

//Task2
void * task2(){
	long b = 0;
	while(1){
		b++;
		PORTB = (1 << PINB5);
		b = 0;
	}
}

//Task3
void * task3(){
	current_node->task->markedforremoval = 1;
	//End of task, jump to ISR
	asm("ijmp" :: "z" (TIMER1_COMPA_vect));
	
	/*asm("MOV R18, %[highAddress]" :: [highAddress] "r" (((int)TIMER1_COMPA_vect & 0xFF00) >> 8));
	asm("PUSH R18");
	asm("MOV R18, %[lowAddress]" :: [lowAddress] "r" (((int)TIMER1_COMPA_vect & 0x00FF)));
	asm("PUSH R18");
	asm("RET");*/
}

void initTask(void * taskAddress){
	static int taskCount = 0;
	
	//Only use the root for the first task
	if(taskCount == 0){
		root->task = malloc(sizeof(task_table_t));
		root->next = 0;
	} else {
		current_node->next = malloc(sizeof(task_table_t));
		current_node = current_node->next;
		current_node->task = malloc(sizeof(task_table_t));
		current_node->next = 0;
	}
	
	//Set ID of task
	current_node->task->id = taskCount;
	//Start address of the task
	current_node->task->address = taskAddress;
	//Split into 2 bytes
	current_node->task->pcl = ((int)current_node->task->address & 0x00FF);
	current_node->task->pch = ((int)current_node->task->address & 0xFF00) >> 8;
	//Set task pointer
	current_node->task->spl = (0x7D0 - (taskCount * 0x64)) & 0x00FF;
	current_node->task->sph = ((0x7D0 - (taskCount * 0x64)) & 0xFF00) >> 8;
	//Always first time
	current_node->task->firsttime = 1;
	//Not suitable for removal
	current_node->task->markedforremoval = 0;
	
	taskCount++;
}

int main(void)
{
	//stop interrupts
	cli();
	
	//All pins in PORTD are outputs
	DDRB = 0b11111111;    
	//Set the tik count for each task
	kernel_settings.taskClockCount = 100;

	// set entire TCCR1A register to 0
	TCCR1A = 0;
	// same for TCCR1B
	TCCR1B = 0;
	//initialize counter value to 0
	TCNT1  = 0;
	//Count to
	OCR1A = kernel_settings.taskClockCount;
	// turn on CTC mode
	TCCR1B |= (1 << WGM12);
	// Set bit for prescaler
	TCCR1B |= (1 << CS10);
	// enable timer compare interrupt
	TIMSK1 |= (1 << OCIE1A);
	
	
	root = malloc(sizeof(struct node));
	current_node  = root;
	initTask(task1);
	initTask(task2);
	initTask(task3);
	current_node->next = root;
	current_node  = root;
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
		asm volatile("MOV r0, %[lowAddress] " :: [lowAddress] "r" (current_node->task->spl));
		asm volatile("OUT __SP_L__, r0");
		asm volatile("MOV r0, %[highAddress] " :: [highAddress] "r" (current_node->task->sph));
		asm volatile("OUT __SP_H__, r0");
		
		asm("MOV R18, %[lowAdress]" :: [lowAdress] "r" (current_node->task->pcl));
		asm("PUSH R18");
		asm("MOV R18, %[highAdress]" :: [highAdress] "r" (current_node->task->pch));
		asm("PUSH 18");
		current_node->task->firsttime = 0;
		
	} else {
		//Save context only if task is not marked for removal
		if(!current_node->task->markedforremoval) {
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
			asm volatile("MOV %[lowAdress], r0 ": [lowAdress] "=r" (current_node->task->spl) : );
			asm volatile("in    r0, __SP_H__");
			asm volatile("MOV %[highAdress], r0 ": [highAdress] "=r" (current_node->task->sph) : );
		}
	
		//Select the current task
		if(current_node->next->task->markedforremoval){
			//Remember the task for removal
			previous_node = current_node->next;
			//Skip the task for removal
			current_node->next = previous_node->next;
			current_node = current_node->next;
			
			//Delete the task to be removed
			free(previous_node->task);
			previous_node->next = 0;
			free(previous_node);
		} else {
			current_node = current_node->next;
		}
		
		
		if(current_node->task->firsttime){
			asm volatile("MOV r0, %[lowAddress] " :: [lowAddress] "r" (current_node->task->spl));
			asm volatile("OUT __SP_L__, r0");
			asm volatile("MOV r0, %[highAddress] " :: [highAddress] "r" (current_node->task->sph));
			asm volatile("OUT __SP_H__, r0");
			
			asm("MOV R18, %[lowAdress]" :: [lowAdress] "r" (current_node->task->pcl));
			asm("PUSH R18");
			asm("MOV R18, %[highAdress]" :: [highAdress] "r" (current_node->task->pch));
			asm("PUSH 18");
			current_node->task->firsttime = 0;
		} else {
			//Load stack pointers
			asm volatile("MOV r0, %[lowAddress] " :: [lowAddress] "r" (current_node->task->spl));
			asm volatile("OUT __SP_L__, r0");
			asm volatile("MOV r0, %[highAddress] " :: [highAddress] "r" (current_node->task->sph));
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
	//Reset interruptbit
	TIFR1 = 1 << 1;
	//Enable interrupts
	sei();
	//Return to task
	reti();
}