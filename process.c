#include "headers.h"

/* Modify this file as needed*/
int remainingTime;
bool interrupt = 0;
bool sec = 0;
void handler(int signum)
{
    // printf("interupted");
    interrupt = 1;
    signal(signum, handler);
}
int previousTime;
int main(int agrc, char *argv[])
{
    initClk();
    if (agrc != 4)
    {
        exit(-1);
    }
    // printf("start");
    // TODO it needs to get the remaining time from somewhere
    remainingTime = atoi(argv[1]);
    int quantum = atoi(argv[2]);
    int algorithm = atoi(argv[3]);
    key_t pKey = ftok("key", 'i');
    int processmsgqid = msgget(pKey, 0666 | IPC_CREAT);

    int finished = 0;
    struct message msg;
    msg.mtype = 1001;

    signal(SIGUSR2,handler);
    while (1)
    {

        previousTime = getClk();
        if (algorithm == 2)
        {

            while (getClk() - previousTime < remainingTime && !interrupt)
            {
            }
            interrupt=0;
            int time=getClk();
            if (time - previousTime != 0)
            {
                remainingTime -= time - previousTime;
                if(!remainingTime)
                    msg.status = 1;
                msgsnd(processmsgqid, &msg, sizeof(struct message), 0);
                if(!remainingTime)
                        exit(0);
            }
            else
                printf("interupted\n");
            kill(getpid(), SIGSTOP);
            continue;
        }
        if (remainingTime <= quantum)
        {
            quantum = remainingTime;
            finished = 1;
        }

        int count = quantum;
        while (count > 0)
        {
            if (getClk() - previousTime == 1)
            {
                count--;
                remainingTime--;
                previousTime = getClk();
                sec = 1;
            }
            if (interrupt)
                break;
        }

        if (sec && finished == 1)
        {
            msg.status = 1;
            msgsnd(processmsgqid, &msg, sizeof(struct message), 0);
            exit(0);
        }
        else
        {
            msg.status = 0;
            if (!interrupt)
                msgsnd(processmsgqid, &msg, sizeof(struct message), 0);
            interrupt = 0;
            kill(getpid(), SIGSTOP);
        }
        sec = 0;
    }


    return 0;
}
