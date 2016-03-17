/*
 * AVRGCC1.c
 *
 * Created: 22-9-2015 12:52:06
 *  Author: Bas van Marle
 */ 

#include <stdlib.h>
#include <avr/interrupt.h>

//Counter gebruikt voor algemene doeleinde
volatile int counter = 0;
//Verteld de scheduler dat het de eerste keer is
volatile int firstrun = 1;
//Geeft aan dat de scheduler is aangeroepen door een geblokkerde taak
volatile int returnFromBlocked = 0;
//Aantal taken in de ready lijst
volatile int amountReadyTasks = 0;
//Aantal geblokkerde taken
volatile int amountBlockedTasks = 0;
//Aantal taken
volatile int taskCount = 0;

//Settings for the kernel(ISR)
typedef struct{
	int isrTicks;
} kernel_settings_t;

//Task Control Block
typedef struct{
	int id;
	//Stack pointer high
	int * sph;
	//Stack pointer low
	int * spl;
	int flags;
	//Program counter high
	int pcl;
	//Program counter low
	int  pch;
	//First address of a task
	void * address;
	//Used so no context is loaded when a tasks runs for the first time
	int firsttime;
	//True if the task is finished and need to be removed
	int markedforremoval;
	//Sleep counter
	long sleepCounter;
} task_table_t;

//DoubleLinked list for all the tasks
struct node{
	task_table_t * task;
	struct node * next;
	struct node * previous;
};

struct node *root;
struct node *current_node_ready;
struct node *temp;
struct node *blockedQueue[10];

kernel_settings_t kernel_settings;

register unsigned char reg1 asm("r2");
register unsigned char reg2 asm("r3");

//Declare the ISR as a naked function
ISR (TIMER1_COMPA_vect) __attribute__ ((naked));
void * sleep(long time) __attribute__((naked));
//void * initTask(void * taskAddress) __attribute__ ((naked));
//Task1
void * task1(){
	long a = 0;
	while(1){
		a++;
		//
		PORTB = (0 << PINB5);
		a = 0;
		//
		sleep(20);
		//
		PORTB = (1 << PINB5);
		//
		sleep(20);
	}
}

//Task2
void * task2(){
	long b = 0;
	while(1){
		b++;
		//PORTB = (1 << PINB5);
		b = 0;
	}
}

//Task3
void * task3(){
	//
	current_node_ready->task->markedforremoval = 1;
	//End of task, jump to ISR
	asm("ijmp" :: "z" (TIMER1_COMPA_vect));
}

void initTask(void * taskAddress){
	
	//Only use the root for the first task
	if(taskCount == 0){
		current_node_ready->task = malloc(sizeof(task_table_t));
		current_node_ready->next = current_node_ready;
		current_node_ready->previous =  current_node_ready;
	} else {
		current_node_ready->next = malloc(sizeof(task_table_t));
		current_node_ready->next->previous = current_node_ready;
		current_node_ready = current_node_ready->next;
		current_node_ready->task = malloc(sizeof(task_table_t));
		current_node_ready->next = 0;
	}
	
	//Set ID of task
	current_node_ready->task->id = taskCount;
	//Start address of the task
	current_node_ready->task->address = taskAddress;
	//Split into 2 bytes
	current_node_ready->task->pcl = ((int)current_node_ready->task->address & 0x00FF);
	current_node_ready->task->pch = ((int)current_node_ready->task->address & 0xFF00) >> 8;
	//Set task pointer
	current_node_ready->task->spl = (0x7D0 - (taskCount * 0x64)) & 0x00FF;
	current_node_ready->task->sph = ((0x7D0 - (taskCount * 0x64)) & 0xFF00) >> 8;
	//Always first time
	current_node_ready->task->firsttime = 1;
	//Not suitable for removal
	current_node_ready->task->markedforremoval = 0;
	//Set sleep counter 0
	current_node_ready->task->sleepCounter = 0;
	//Amount of overall tasks
	taskCount++;
	//Amount of tasks in the ready queue
	amountReadyTasks++;
}

int main(void)
{
	//stop interrupts
	cli();
	//All pins in PORTD are outputs
	DDRB = 0b11111111;    
	//Set the tick count for the ISR
	kernel_settings.isrTicks = 8000;

	// set entire TCCR1A register to 0
	TCCR1A = 0;
	// same for TCCR1B
	TCCR1B = 0;
	//initialize counter value to 0
	TCNT1  = 0;
	//Count to
	OCR1A = kernel_settings.isrTicks;
	// turn on CTC mode
	TCCR1B |= (1 << WGM12);
	// Set bit for prescaler
	TCCR1B |= (1 << CS10);
	// enable timer compare interrupt
	TIMSK1 |= (1 << OCIE1A);
	
	//Create a root TCB
	current_node_ready = malloc(sizeof(struct node));
	root  = current_node_ready;
	initTask(task1);
	temp = current_node_ready;
	initTask(task2);
	initTask(task3);
	current_node_ready->next = root;
	root->previous = current_node_ready;
	current_node_ready  = root;

	//allow interrupts
	sei();
	
	while (1)
	{
		// we have a working Timer
		counter++;
	}
	
}


void * sleep(long time){
	//Disable interrupts
	cli();
	//Set the sleep time in the TCB
	current_node_ready->task->sleepCounter = (time * kernel_settings.isrTicks) * 2;
	//Remove the task from the ready list
	current_node_ready->previous->next = current_node_ready->next;
	current_node_ready->next->previous = current_node_ready->previous;
	//Add the task to the blocked queue
	blockedQueue[amountBlockedTasks] = current_node_ready;
	amountBlockedTasks++;
	//Tell the ISR we are comming back early because a task is blocked
	returnFromBlocked = 1;
	//Enable interrupts
	sei();
	//Jump to the ISR
	asm("ijmp" :: "z" (TIMER1_COMPA_vect));
}

ISR (TIMER1_COMPA_vect)
{
	static int dispatcher = 1;
	//Disable interrupts
	cli();
	
	//If the isr is called because a task has ended prematurely
	//we are going to switch tasks so we enable the dispatcher
	if(returnFromBlocked){
		dispatcher = 1;
		returnFromBlocked = 0;
	} else {		
		//Toggle dispatcher every 1 ms
		dispatcher ^= 1;
	}
	
	if(amountBlockedTasks > 0) {
		int amountBlockedTasksRemoved = 0;
		
		for(int i = 0; i <  amountBlockedTasks; i++){
			if(blockedQueue[i]->task->sleepCounter > 0){
				blockedQueue[i]->task->sleepCounter -= kernel_settings.isrTicks;
			} else {
				blockedQueue[i]->task->sleepCounter = 0;

				current_node_ready->next->previous = blockedQueue[i];
				blockedQueue[i]->next = current_node_ready->next;
				blockedQueue[i]->previous = current_node_ready;
				current_node_ready->next = blockedQueue[i];
				blockedQueue[i] = NULL;
				amountBlockedTasksRemoved++;
			}
		}
		amountBlockedTasks -= amountBlockedTasksRemoved;
	}


	//Do not switch context on first run
	if(firstrun){
		//Only run once
		firstrun = 0;
		//Set stack pointer
		asm volatile("MOV r0, %[lowAddress] " :: [lowAddress] "r" (current_node_ready->task->spl));
		asm volatile("OUT __SP_L__, r0");
		asm volatile("MOV r0, %[highAddress] " :: [highAddress] "r" (current_node_ready->task->sph));
		asm volatile("OUT __SP_H__, r0");
		
		asm("MOV R18, %[lowAdress]" :: [lowAdress] "r" (current_node_ready->task->pcl));
		asm("PUSH R18");
		asm("MOV R18, %[highAdress]" :: [highAdress] "r" (current_node_ready->task->pch));
		asm("PUSH 18");
		current_node_ready->task->firsttime = 0;
		
	}
	if(dispatcher) {
		//Save context only if task is not marked for removal
		if(!current_node_ready->task->markedforremoval) {
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
			
			asm volatile("in    r2, __SP_L__");
			current_node_ready->task->spl = reg1;
			//asm("lds r26,%0" :: "Q" (current_node->task->spl));
			//asm volatile("STS %0,R25", :: "=r" (a));
			//asm volatile("MOV %[lowAdress], r0 ": [lowAdress] "=r" (a) :);
			//current_node->task->spl = a;
			//current_node->task->spl = regReader;
			
			asm volatile("in    r3, __SP_H__");
			current_node_ready->task->sph = reg2;
			//asm volatile("MOV %[highAdress], r0 ": [highAdress] "=r" (b) : );
			//current_node->task->sph = b;
		}
	
		//Select the current task
		if(current_node_ready->next->task->markedforremoval){
			//Remember the task for removal
			temp = current_node_ready->next;
			//Skip the task for removal
			current_node_ready->next = current_node_ready->next->next;
			//Set the  previous node to the current node
			current_node_ready->next->previous = current_node_ready;
			//Select the next node
			current_node_ready = current_node_ready->next;
			
			//Delete the task to be removed
			free(temp->task);
			temp->next = 0;
			free(temp);
		} else {
			current_node_ready = current_node_ready->next;
		}
		
		//If the task is running for the first time we do not need to load the context from the stack
		if(current_node_ready->task->firsttime){
			asm volatile("MOV r0, %[lowAddress] " :: [lowAddress] "r" (current_node_ready->task->spl));
			asm volatile("OUT __SP_L__, r0");
			asm volatile("MOV r0, %[highAddress] " :: [highAddress] "r" (current_node_ready->task->sph));
			asm volatile("OUT __SP_H__, r0");
			
			asm("MOV R18, %[lowAdress]" :: [lowAdress] "r" (current_node_ready->task->pcl));
			asm("PUSH R18");
			asm("MOV R18, %[highAdress]" :: [highAdress] "r" (current_node_ready->task->pch));
			asm("PUSH 18");
			current_node_ready->task->firsttime = 0;
		} else {
			//Load stack pointers
			asm volatile("MOV r0, %[lowAddress] " :: [lowAddress] "r" (current_node_ready->task->spl));
			asm volatile("OUT __SP_L__, r0");
			asm volatile("MOV r0, %[highAddress] " :: [highAddress] "r" (current_node_ready->task->sph));
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
	//TCNT1  = 0;
	//Reset interruptbit
	//TIFR1 = 1 << 1;
	//Enable interrupts
	sei();
	//Return to task
	reti();
}