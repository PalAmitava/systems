# define _GNU_SOURCE
# include <stdio.h>
# include <pthread.h>
# include <stdlib.h>
# include <unistd.h>
# include <stdbool.h>
# define WCHAIRS 5
typedef struct 
{
    int wait_area[WCHAIRS];
    int head;
    int tail;
    int waiting;
    
}queue;

typedef struct 
{
    int N;
    enum{sleeping,cutting}b_state;
    pthread_mutex_t monitor_lck;
    pthread_cond_t bcv; //barber waits
    pthread_cond_t ccv; //customers wait
    pthread_cond_t hs; //handshaking
    pthread_cond_t cutcv;//customers wait for haircut to actually finish
    pthread_cond_t chair_empty_cv; 
    queue q;
    int curr_serving;
    bool open;
    bool done; //prevent spurrious wakeups on cutcv
    //int finished;
    bool chair_empty;//state for chairemptycv
    
}monitor;

monitor m;
pthread_barrier_t tbarrier;//thread barrier
void init_monitor(monitor *m)
{
    (*m).N=WCHAIRS;
    (*m).b_state=sleeping;
    pthread_mutex_init(&m->monitor_lck,NULL);
    (*m).q.head=0;
    (*m).q.tail=0;
    (*m).q.waiting=0;
    (*m).curr_serving=-1;
    (*m).open=true;
    (*m).done=false;
    (*m).chair_empty=false;
    //(*m).finished=-1;
    pthread_cond_init(&m->ccv,NULL);
    pthread_cond_init(&m->bcv,NULL);
    pthread_cond_init(&m->hs,NULL);
    pthread_cond_init(&m->cutcv,NULL);
    pthread_cond_init(&m->chair_empty_cv,NULL);
}
void enter_shop(monitor *m,int i)
{
    pthread_mutex_lock(&m->monitor_lck);
    printf("Customer %d entered shop\n",i);
    if((*m).q.waiting == WCHAIRS)
    {
        pthread_mutex_unlock(&m->monitor_lck);
        pthread_exit((void*)1); 
    }
    (*m).q.wait_area[(*m).q.tail] = i;
    (*m).q.waiting++;
    (*m).q.tail = ((*m).q.tail + 1) % WCHAIRS;
    
    //Wake the barber if they were sleeping
    if((*m).b_state == sleeping)
    {
        pthread_cond_signal(&m->bcv);
    }
    while((*m).curr_serving!=i)
    pthread_cond_wait(&m->ccv,&m->monitor_lck);
    (*m).curr_serving=-1;
    (*m).done=false; //my hair not yet cut
    pthread_cond_signal(&m->hs);
    while(!(*m).done)
    pthread_cond_wait(&m->cutcv,&m->monitor_lck);
    (*m).chair_empty=true;
    pthread_cond_signal(&m->chair_empty_cv);
    pthread_mutex_unlock(&m->monitor_lck);
}

void cut_hair(monitor *m)
{
    do
    {
    pthread_mutex_lock(&m->monitor_lck);
    while((*m).q.waiting==0 && (*m).open)
    {
        (*m).b_state=sleeping;
        pthread_cond_wait(&m->bcv,&m->monitor_lck);
    }
    if(!(*m).open) 
    {
        pthread_mutex_unlock(&m->monitor_lck);
        return;
    }
    (*m).b_state=cutting;
    (*m).curr_serving=(*m).q.wait_area[(*m).q.head];
    //int cutting_curr=(*m).curr_serving;
    pthread_cond_broadcast(&m->ccv);
    (*m).q.head=((*m).q.head+1)%WCHAIRS;
    (*m).q.waiting--;
    
    while((*m).curr_serving!=-1)
    {
        pthread_cond_wait(&m->hs,&m->monitor_lck);
    }
    pthread_mutex_unlock(&m->monitor_lck);
    //haircut outside critical region
    //so customers get chance to enter shop and wait while hair is being cut
    for(int j=0;j<10000;j++); //hair cut
    
    pthread_mutex_lock(&m->monitor_lck);
    (*m).done=true;
    //(*m).finished=cutting_curr;
    (*m).chair_empty=false;
    pthread_cond_signal(&m->cutcv);
    while(!(*m).chair_empty)
    {
        pthread_cond_wait(&m->chair_empty_cv,&m->monitor_lck);
    }
    pthread_mutex_unlock(&m->monitor_lck);
    
    }while(1);
}

void* customer(void* args)
{
    int cid=*(int*)args;
    pthread_barrier_wait(&tbarrier);
    enter_shop(&m,cid);
    printf("Customer %d just finished haircut\n",cid);
    pthread_exit((void*)0);
}

void* barber(void* args)
{
    int bid=*(int*)args;
    pthread_barrier_wait(&tbarrier);
       
    cut_hair(&m);
    
    
    return NULL;
}

int main(int argc,char *argv[])
{
    if(argc!=2)
    {
        printf("Fatal: Usage ./main <no. of customer>\n");
        exit(EXIT_FAILURE);
    }
    init_monitor(&m);
    //if atoi fails it returns 0
    int noc=atoi(argv[1]);
    pthread_barrier_init(&tbarrier,NULL,(unsigned int)(noc+1));
    int *id=(int*)malloc((size_t)(noc+1)*sizeof(int));
    pthread_t *cid=(pthread_t*)malloc((size_t)noc*sizeof(pthread_t));
    for(int i=0;i<=noc;i++)
    {
        id[i]=i;
    }
    for(int i=0;i<noc;i++)
    {
        pthread_create(&cid[i],NULL,customer,&id[i]);
    }
    pthread_t bid;
    pthread_create(&bid,NULL,barber,&id[noc]);
    for(int i=0;i<noc;i++)
    {
        void *retval;
        pthread_join(cid[i],&retval);
        if(retval==((void*)1))
        {
            printf("CUstomer %d had to exit to lack of chairs\n",i);
        }
    }
    //by this time the customers have left and queue is empty barber blocked on bcv
    //better than cancelling barber thread[infinite loop] while it still holds mutex[bad practice]
    pthread_mutex_lock(&m.monitor_lck);
    m.open=false;
    pthread_cond_signal(&m.bcv);
    pthread_mutex_unlock(&m.monitor_lck);
    pthread_join(bid,NULL);
    pthread_barrier_destroy(&tbarrier);
    pthread_mutex_destroy(&m.monitor_lck);
    pthread_cond_destroy(&m.bcv);
    pthread_cond_destroy(&m.ccv);
    pthread_cond_destroy(&m.hs);
    pthread_cond_destroy(&m.cutcv);
    pthread_cond_destroy(&m.chair_empty_cv);
    free(id);
    free(cid);
}

//barber might reacquire mutex immediately after finishing a cut customer getting cut wasnt able to move head
//so barber tries to cuts customer 1 again hence it is barber who moves the head

//again barber thread cuts customer 1,2,3 so now currently_serving=3 now 1 is scheduled but it doesnt run
//so need some handshaking mechanism(extra overhead kills concurrency)

//problem is after handshake customer leaves and then barber cuts the hair outside critical region 
//introduce cutcv[customer wait here]

/*
waiting on cutcv without while loop guard was dangerous we introduced bool done
customer before signalling hs sets it to false
after finishing haircut barber enters monitor and sets it to true sets done=true


Barber finishes Customer A: The barber locks the mutex, sets done = true, signals cutcv, and unlocks the mutex.

The OS moves Customer A from the wait queue to the ready queue. But,the Barber thread loops back around and wins the race to re-acquire the mutex.

Barber starts Customer B: The barber pulls Customer B from the queue, sets curr_serving, broadcasts ccv, and goes to sleep on hs (releasing the mutex).

Customer B wakes up first: Customer B acquires the mutex, sees it is their turn, sets curr_serving = -1, and crucially, sets done = false, then goes to sleep on cutcv.

Customer A finally gets the mutex: Customer A wakes up from its cutcv wait and re-evaluates its while loop condition: while(!m->done).

The Deadlock: Because Customer B just set done = false, Customer A thinks its haircut isn't finished and goes back to sleep on cutcv. Customer A is now permanently stuck.
hence instead of bool done use int done prevents other customers overwriting

finished doesnt prevent overwriting by the barber
in short both bool and finished fail
we introduce along with done a chair_empty_cv[barber waits for customer to leave] exit handshake
these handshakes kill concurrency
earlier the barber was sigalling cutcv and exiting and grabbing next customer from waiting area
we make barber wait for customer to leave
*/

/*
OUTPUT:
Customer 1 entered shop
Customer 3 entered shop
Customer 5 entered shop
Customer 2 entered shop
Customer 4 entered shop
Customer 0 entered shop
Customer 1 just finished haircut
Customer 3 just finished haircut
Customer 5 just finished haircut
Customer 2 just finished haircut
Customer 4 just finished haircut
Customer 0 just finished haircut

Customers 1, 3, 5, 2, and 4 all successfully grab the mutex one after another and sit in the waiting room. 1 -> 2 -> 3 -> 4 -> 5. The waiting room is now completely full.
Before Customer 0 gets a chance to grab the mutex, the Barber thread gets CPU time. The Barber locks the mutex, sees people waiting, and pulls Customer 1 out of the waiting area.
sets(*q).waiting --;
*/