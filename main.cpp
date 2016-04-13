#include "task_switcher.h"
#include <avr/interrupt.h>

int task1();
int task2();

int main()
{
	init_task((int)task1);
	init_task((int)task2);
	start();
}

//Task1
int task1()
{
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
int task2()
{
	long b = 0;
	while(1){
		b++;
		//PORTB = (1 << PINB5);
		b = 0;
	}
}