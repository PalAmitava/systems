# define _GNU_SOURCE
# include <stdio.h>
# include <sched.h>
# include <stdint.h>
# include <stdatomic.h>
# include <stdalign.h>
# include <pthread.h>
# include <time.h>
# include <stdlib.h>
# include <stdbool.h>
# include <unistd.h>
# define SLOT_SIZE 4
# define ITERATIONS 1000000

typedef struct { alignas(64)
uint32_t data;
atomic_bool flag;
} deliverable;

deliverable slots[SLOT_SIZE];

pthread_barrier_t barrier;

void* producer(void* args)
{
    int slot_index=*(int*)args;
    pthread_barrier_wait(&barrier);
    for(int i=0;i<ITERATIONS;i++)
    {
        slots[slot_index].data=(uint32_t)(rand()%100);
        //flag default value is 0
        atomic_store_explicit(&slots[slot_index].flag,true,memory_order_release);
        while(atomic_load_explicit(&slots[slot_index].flag,memory_order_acquire)); //we wait for flag to be false 
        //consumer needs time to process the data
        
    }
    return NULL;

}

void* consumer(void* args)
{
    (void)args;
    pthread_barrier_wait(&barrier);
    while(true)
    {
        for(int i=0;i<4;i++)
            {
            if (atomic_load_explicit(&slots[i].flag, memory_order_acquire) == true) {
        
            //Safely read the non-atomic data.
             uint32_t data = slots[i].data;
             printf("Consumer consumed %d from producer %d\n", data, i); 
              // Use RELEASE so we don't flip the flag before we finish reading 'data'.
              atomic_store_explicit(&slots[i].flag, false, memory_order_release);
            }
            }
    }
    return NULL;
}

int main(void)
{
    int ids[SLOT_SIZE];
    pthread_t tid[SLOT_SIZE];
    pthread_t ctid;
    for(int i=0;i<SLOT_SIZE;i++)
    {
        ids[i]=i;
    }
    const unsigned int total=5;
    pthread_barrier_init(&barrier,NULL,total);
    for(int i=0;i<4;i++)
    {
        pthread_create(&tid[i],NULL,producer,&ids[i]);
        cpu_set_t set; //this a mask
        CPU_ZERO(&set);//set all bits to zero for this mask
        CPU_SET((size_t)i<<1,&set);//i alloted logical processor i*2
        //i is int it could i<<1 could cause bug if i were -ve
        //compiler assured everything ok
        pthread_setaffinity_np(tid[i],sizeof(cpu_set_t),&set);//mask applied to the thread
    }
    pthread_create(&ctid,NULL,consumer,NULL);
    cpu_set_t cset;
    CPU_ZERO(&cset);
    CPU_SET(1<<3,&cset);
    pthread_setaffinity_np(ctid,sizeof(cpu_set_t),&cset);
    sleep((unsigned int)50);

}

//no prior read/write reordered after release[used with store] i.e store buffer flushed to be visible to cache coherency protocol all reads completed
//no later read/write reordered before atomic load acquire executed also pending cache invalidation done as new data might be available later
//for simple atomics relaxed is sufficient(allows reordering compiler and hardware)
//default is seq_cst tries to ensure global timeline consistency and slaps performance

//cat /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size
//64
//cache line size is 64B whole slots array fits inside this
//which means producer 0 on say core 0 wants to write it wants ownership of that line an writes
//slots[1] lives in same box if producer 1 wants to write producer 0 copy invalidated 
//constany cycle of validation and invalidation
//so we add alignas(64) forcing compiler to add 56B of padding after flag

//i also pin the threads to different cores
//use lscpu for core and chip details
//ich habe 8 cores and 2 threads per sockets
//so core0 and 1 actually means same physical cores OS sees two different logical processors
//reality am on windows have 12 physical cores 4P*2(threads) 8E =16 total threads

//running htop verified cores 0,2,4,6 running at 100%
//consumer core close to 0% had a printf() statement that needs syscalls OS locks terminal format screen write to screen
//os may put it to sleeep so its core utilisaiton drops
//consumer was running terribly slow while producers were getting stuck in their while loops pegging core at 100%


//removed the printf statement consumer ran at 100% cores 0,2,4,6 at 0%
//now consumer will run at full speed
//it races with producers the producers finish and return NULL
//consumer stuck at while(true) pegging core to 100% utilisation


