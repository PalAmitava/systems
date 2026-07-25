#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct 
{
    int buffer[5];
    int head;
    int tail;
} queue;

typedef struct 
{
    enum {thinking, eating, hungry} state[5];
    pthread_cond_t cond;          // A single condition variable
    pthread_mutex_t monitor_lck;
    queue q;
} monitor;

monitor m;

void init_monitor(monitor *m)
{
    pthread_mutex_init(&m->monitor_lck, NULL);
    pthread_cond_init(&m->cond, NULL); 
    
    for(int i = 0; i < 5; i++)
    {
        m->state[i] = thinking;
    }
    m->q.head = 0;
    m->q.tail = 0;
}

void acquire(monitor *m, int i)
{
    pthread_mutex_lock(&m->monitor_lck);
    m->state[i] = hungry;
    
    //Enqueue the philosopher 
    m->q.buffer[m->q.tail] = i;
    m->q.tail = (m->q.tail + 1) % 5;

    //wait for turn
    // I must be at the head of the queue AND my neighbors cannot be eating
    while (m->q.buffer[m->q.head] != i || 
           m->state[(i + 4) % 5] == eating || 
           m->state[(i + 1) % 5] == eating) 
    {
        pthread_cond_wait(&m->cond, &m->monitor_lck);
    }
    m->state[i] = eating;
    
    //Dequeue myself so the next person in line becomes the head
    m->q.head = (m->q.head + 1) % 5;
    
    //Broadcast to let the new head of the queue check if they can eat concurrently
    pthread_cond_broadcast(&m->cond);
    
    pthread_mutex_unlock(&m->monitor_lck);
}

void release(monitor *m, int i)
{
    pthread_mutex_lock(&m->monitor_lck);
    
    m->state[i] = thinking;
    
    // Broadcast to wake up everyone. 
    // Only the philosopher at the head of the queue will actually proceed.
    pthread_cond_broadcast(&m->cond);
    
    pthread_mutex_unlock(&m->monitor_lck);
}

void* philosopher(void* args)
{
    int index = *(int*)args;
    while(true){
        usleep(500); 
        
        acquire(&m, index);
        printf("Philosopher %d is eating\n", index);
        
        sleep(1); 
        
        release(&m, index);
        printf("Philosopher %d is thinking\n", index);
    }
    return NULL;
}

int main()
{
    init_monitor(&m);
    int ids[5];
    pthread_t phtid[5];
    
    for(int i = 0; i < 5; i++)
    {
        ids[i] = i;
    }
    
    for(int i = 0; i < 5; i++)
    {
        pthread_create(&phtid[i], NULL, philosopher, &ids[i]);
    }
    
    // Keep main thread alive
    sleep(50);
    return 0;
}



//strict fifo bad
//say 0 eating 1 blocked
//now 3 eating 2 wants and 2 blocked
//when 3 finishes 2 can eat but with strict fifo 2 starved unnecesarrily

