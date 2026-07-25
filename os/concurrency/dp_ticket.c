# include <stdio.h>
# include <pthread.h>
# include <stdbool.h>
# include <unistd.h>

typedef struct 
{
    enum{thinking,eating,hungry}state[5];
    pthread_cond_t self[5]; 
    pthread_mutex_t monitor_lck;
    
    int tickets[5];
    long ticket_counter;
}monitor;
monitor m;

void init_monitor(monitor *m)
{
    pthread_mutex_init(&m->monitor_lck,NULL);
    m->ticket_counter=0;
    for(int i=0;i<5;i++)
    {
        pthread_cond_init(&m->self[i],NULL);
        m->state[i]=thinking;
        m->tickets[i]=0; 
    }
}
void test(monitor *m,int i);
void acquire(monitor *m,int i)
{
    pthread_mutex_lock(&m->monitor_lck);
    m->state[i]=hungry;
    m->ticket_counter++;
    m->tickets[i]=m->ticket_counter;

    test(m,i);
    while(m->state[i]!=eating)//gurads against spurrious wakeups
    {
        pthread_cond_wait(&m->self[i],&m->monitor_lck);
    }
    pthread_mutex_unlock(&m->monitor_lck);

}

void release(monitor *m,int i)
{
    pthread_mutex_lock(&m->monitor_lck);
    m->state[i]=thinking;
    //investigate neighbours as only they might wait for me
    test(m,(i+4)%5);
    test(m,(i+1)%5);
    pthread_mutex_unlock(&m->monitor_lck);
}

void test(monitor *m,int i)
{
    int left=(i+4)%5;
    int right=(i+1)%5;
    if(m->state[i]==hungry && m->state[left]!=eating && m->state[right]!=eating)
    {
        //even if chopstickts available step back if neighbours had been waiting longer
        bool l_wait=m->state[left]==hungry && m->tickets[left]<m->tickets[i];
        bool r_wait=m->state[right]==hungry && m->tickets[right]<m->tickets[i];
        if(!l_wait && !r_wait)
        {
            m->state[i]=eating;
            m->tickets[i]=0; //discard ticket since am eating
            pthread_cond_signal(&m->self[i]);
        }
    }
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
    sleep(50);
    return 0;
}

//problem is with counter when max reached and it overflows
//{check how linux kernel manages a similar situation}

//0 releases
//test(1) fails 2 has older ticket[3 was eating]
//test(4) fails 4 not hungry

//1,2 asleep
//3 exits 2 is signalled

//once a process signals another process even though signal and continue
//that condition still holds because of state management
//so while only guards against spurrious wakeups[here]


//starvation due to death
//if i dies while eating then its state stays eatinng 
//neigbours wait forever

//if it dies while hungry it will hold oldest ticket(in this case) and starve neighbours
//dies while holding mutex global starvation scenario