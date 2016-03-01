#include<stdio.h>
#include<stdlib.h>
#include<linux/unistd.h>
#include<sys/mman.h>

//functions below were yelling at me so I'm declaring the struct again
struct cs1550_sem{
	int value;
	struct sem_queue_node *first;
	struct sem_queue_node *last;
};

//handy wrapper functions to make these syscalls a bit easier to call
void down(struct cs1550_sem *sem) {
  syscall(__NR_cs1550_down, sem);
}
void up(struct cs1550_sem *sem) {
  syscall(__NR_cs1550_up, sem);
}

//need a shared variable among each producer/consumer for the in/out variable
struct prodcon_n{
	int value;
};

int main(int argc, char** argv)
{
	int num_prods;
	int num_cons;
	int buffsize;
	int prod_id = -1;
	int cons_id = -1;
	
	if (argc < 4)
	{
		printf("Correct format is: ./prodcons <number of producers> <number of consumers> <buffersize> \n");
		exit(0);
	}
	
	num_prods = (int)atoi(argv[1]);
	num_cons = (int) atoi(argv[2]);
	buffsize = (int) atoi(argv[3]);
	
	//the size of the three semaphores to be used (used for mmap)
	int size_sems = (sizeof(struct cs1550_sem)*3);
	int size_buff = (sizeof(int) * buffsize); //size of the buffer array
	int size_in_out =  (sizeof(struct prodcon_n)*2); //size of the two global index values "in" and "out"
	
	//mmap for the three semaphores
	void *sems = mmap(NULL, size_sems, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	
	//I decided to map the buffer array seperately just for convenience's sake
	void *buff = mmap(NULL, size_buff, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	
	//also need to include the in/out variables in shared memory
	void *inc = mmap(NULL, size_in_out, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	
	//declaration in memory of the three semaphores: empty, full, and mutex as discussed in lecture
	struct cs1550_sem *empty = ((struct cs1550_sem *) sems)+ 0;
	struct cs1550_sem *full = ((struct cs1550_sem *) sems) + 1;
	struct cs1550_sem *mutex = ((struct cs1550_sem *) sems)+ 2;
	
	int *prodcon_buffer = (int*) malloc(size_buff);
	prodcon_buffer = ((int*) buff) + 0;
	
	struct prodcon_n *in = ((struct prodcon_n *) inc) + 0;
	struct prodcon_n *out = ((struct prodcon_n *) inc)+ 1;
	
	//initialization of empty semaphore
	//assumed N = buffsize
	empty->value = buffsize;
	empty->first = NULL;
	empty->last = NULL;
	
	//initialization of full semaphore
	full->value = 0;
	full->first = NULL;
	full->last = NULL;
	
	//initialization of mutex semaphore
	mutex->value = 1;
	mutex->first = NULL;
	mutex->last = NULL;
	
	//set all the values of the buffer array to 0
	int i;
	for(i = 0; i < buffsize; i++)
	{
		prodcon_buffer[i] = 0;
	}
	
	in->value = 0;
	out->value = 0;
	
	pid_t prod;
	pid_t cons;
	
	//fork consumers
	for(i = 0; i < num_cons; i++)
	{
		//printf("forking a consumer\n");
		cons = fork();
		if(cons == 0)
		{
			cons_id = i;
			int cInt;
			while(1)
			{
				//printf("Consumer before down:   empty: %d, full: %d, mutex: %d\n", empty->value, full->value, mutex->value);
				//if(out->value = buffsize) return 0;
				down(full); //checks to see if the buffer is empty
				down(mutex); //enter critical region
				//printf("Consumer after down:   empty: %d, full: %d, mutex: %d\n", empty->value, full->value, mutex->value);
				cInt = prodcon_buffer[out->value];
				out->value = (out->value + 1) %buffsize;
				printf("Consumer #%d consumed: %d \n", cons_id, cInt);
				up(mutex); //leave critical region
				up(empty);
			}
		}			
	}
	
	//fork producers
	for(i = 0; i < num_prods; i++)
	{
		//printf("forking a producer \n");
		prod = fork();
		if(prod == 0) //current process is a producer
		{
			prod_id = i;
			//printf("we're in producer #%d for the first time!\n", prod_id);
			int pInt;
			while(1)
			{
				//printf("Producer before down:   empty: %d, full: %d, mutex: %d\n", empty->value, full->value, mutex->value);
				//if(in->value == buffsize) return 0;
				down(empty);//checks to see if the buffer is full
				down(mutex);//enter critical region
				//printf("Producer after down:   empty: %d, full: %d, mutex: %d\n", empty->value, full->value, mutex->value);
				//produces an int to put into the buffer
				//in this case it will just be the same as the value of "in"
				pInt = in->value;
				prodcon_buffer[in->value] = pInt;
				in->value = (in->value + 1) % buffsize;
				printf("Producer #%d produced: %d \n", prod_id, pInt);
				up(mutex);//exit critical region
				up(full);
			}
		}
	}
	
	//if there are child processes active, wait
	if(prod > 0 || cons > 0)
	{
		while(1);
	}
}