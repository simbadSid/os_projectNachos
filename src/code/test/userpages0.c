#include "syscall.h"
//+b FoxTox 13.01.2016

void print_chars( void *arg )
{
	int c = *((int*) arg);
    int i;
    for (i = 0; i < 10; i++) {
    	c++;
    	PutInt(c);
    	PutChar('\n');
    }
}

int main()
{
	int arg[] = {1};
    int tid0 = UserThreadCreate(print_chars, (void *) arg);
    int tid1 = UserThreadCreate(print_chars, (void *) arg);
    int tid2 = UserThreadCreate(print_chars, (void *) arg);

    UserThreadJoin(tid0);
    UserThreadJoin(tid1);
    UserThreadJoin(tid2);

    Halt();
    return 0;

}
//+e FoxTox 13.01.2016
