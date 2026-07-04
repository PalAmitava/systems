# include <stdio.h>
# include <sys/types.h>
# include <unistd.h>
# include <signal.h>
# include <sys/wait.h>
# define SIG_TERM_LOOP SIGUSR1


volatile sig_atomic_t flag=1; 

void handler(int signal)
{
    flag=0;
    const char buf[]="Inside the handler\n";
    write(STDOUT_FILENO,buf,sizeof(buf)-1); //not print the null at the end
}
int main()
{
    pid_t pid;
    pid=fork();
    if(pid<0)
    {
        perror("Fork is failure");
        return 1;
    }
    else if(pid==0)
    {
        //signal(SIG_TERM_LOOP,handler); //signal handler installed(old standard)
        struct sigaction s;
        s.sa_handler=handler;
        sigemptyset(&s.sa_mask);
        s.sa_flags=0;
        if(sigaction(SIG_TERM_LOOP,&s,NULL)==-1)
        {
            perror("sigaction erred");
            _exit(2);
        }
        while(flag)
        {
            fprintf(stdout,"I am the child with pid: %d\n",getpid());
            sleep(1);
        }
        fprintf(stdout,"Exited loop and killing the child\n");
        fflush(stdout);
        _exit(37); //may not flush buffer so above message might not appear hence the flush

    }
    else
    {
        int status;
        sleep(5);
        if(kill(pid,SIG_TERM_LOOP)==-1)
        {
            perror("Error with kill\n");
            return 1;
        }
        waitpid(pid,&status,0);
        //decoding the status interger retuned by kernel using macros

        if(WIFEXITED(status))
        {
            printf("Child exited normally\n"); //using return exit() or _exit
            printf("%d\n",WEXITSTATUS(status));//exit code jodi WIFEXITED true hoy
        }
        else if(WIFSIGNALED(status)) //killed because of a signal
        {
            printf("Died due to signal\n");
            printf("%d\n",WTERMSIG(status)); //kon signal terminate korlo child ke
        }
        return 0;

    }

}