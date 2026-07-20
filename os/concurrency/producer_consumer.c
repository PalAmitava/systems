# include "header.h"
# define PTHREAD_BARRIER_SERIAL_THREAD -1 
# define MY_RAND_MAX 150
//abv defn of barrier in thread lib prevent error squiggles

void* producer(void* param);
void* consumer(void* param);

int *ids1,*ids2; //for cleanup associated with atexit
pthread_t *tid_p,*tid_c;

static pthread_barrier_t start_line; //producers and consumer methods have access
atomic_bool running=true;

void cleanup()
{
    free(tid_p);
    free(tid_c);
    free(ids1);
    free(ids2);
    pthread_barrier_destroy(&start_line);
    rm_buf();
}

int main(int argc,char* argv[])
{
    atexit(cleanup);
    if(argc!=5)
    {
        fprintf(0,"Usage: ./main <sleep_before_exit> <no. of producers> <no. of consumers> <buffer_sz>\n");
        return 1;
    }
    int sleep_time=atoi(argv[1]);
    int producers=atoi(argv[2]);
    int consumers=atoi(argv[3]);
    int buf_sz=atoi(argv[4]);

    init_buffer(buf_sz);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr,PTHREAD_SCOPE_SYSTEM); //default for linux
    
    int total=producers+consumers;
    pthread_barrier_init(&start_line,NULL,total); //NULL def. attribut

    tid_p=(pthread_t*)malloc(producers*sizeof(pthread_t));
    tid_c=(pthread_t*)malloc(consumers*sizeof(pthread_t));
    if(tid_p==NULL || tid_c == NULL)
    {
        perror("Malloc main\n");
        exit(EXIT_FAILURE);
    }
    //creating threads
    *ids1=(int*)malloc(producers*sizeof(int));
    if(ids1==NULL)
    {
        perror("Malloc ids1\n");
        exit(EXIT_FAILURE);
    }
    for(int i=0;i<producers;i++)
    {
        ids1[i]=100+i;
        if(pthread_create(&tid_p[i],&attr,producer,&ids1[i])==-1)
        {
            printf("Failed to create producer thread\n");
            return 1; //exit program
        }
    }
    *ids2=(int*)malloc(consumers*sizeof(int));
    if(ids2==NULL)
    {
        perror("Malloc ids2\n");
        exit(EXIT_FAILURE);
    }
    for(int i=0;i<consumers;i++)
    {
        ids2[i]=500+i;
        if(pthread_create(&tid_c[i],&attr,consumer,&ids2[i])==-1)
        {
            printf("Failed to create consumer threads\n");
            return 1;
        }
    }
    sleep(sleep_time);
    running=false;
    for(int i=0;i<producers;i++)
    {
        
        void *retval;
        pthread_join(tid_c[i],&retval);
        if(retval==(void*)1)
        {
            printf("producer %d had ended abnormally\n",ids1[i]);
        }
    }
    for(int i=0;i<consumers;i++)
    {
        void *retval;
        pthread_join(tid_c[i],&retval);
        if(retval==(void*)1)
        {
            printf("consumer %d had ended abnormally\n",ids2[i]);
        }
    }
    exit(EXIT_SUCCESS);

}
void* consumer(void* param)
{
    int ucid=*(int*)param;
    int status=pthread_barrier_wait(&start_line);
    if(status==PTHREAD_BARRIER_SERIAL_THREAD) //this special value returned to exactly one thread rest receive 0 guranteeing messagae printed once
    {
        printf("All producers and consumers ready!. Starting execution...\n");
        
        printf("Last thread to be ready was consumer:%d",ucid);
    }
    buffer_item item;
    while(true && running)
    {
        usleep(rand()%MY_RAND_MAX);
        if(rm_item(&item))
        {
           fprintf(stderr,"Error condition rm_item\n");
           pthread_exit((void*)1);//expects a void*
        }
        else
        {
            printf("consumer with %lu tid and serial id :%d consumed:%d\n",(unsigned long)pthread_self(),ucid,item);
        }
      
    }
    return NULL;
}

void* producer(void* param)
{
    int upid=*(int*)param;
    int status=pthread_barrier_wait(&start_line);
    if(status==PTHREAD_BARRIER_SERIAL_THREAD)
    {
        printf("All producers and consumers ready!. Starting execution...\n");
        printf("Last thread to be ready was producer:%d",upid);
    }
    while(true && running)
    {
        usleep(rand()%MY_RAND_MAX);
        buffer_item item= rand()%MY_RAND_MAX;
        if(put_item(item))
        {
            fprintf(stderr,"Error in put-item\n");
            pthread_exit((void*)1); //wont reach here technically 
        }
        else
        {
            printf("producer with %lu tid and serial id :%d produced:%d\n",(unsigned long)pthread_self(),upid,item);
        }
    }
    return NULL;


}