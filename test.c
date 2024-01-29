#include <stdio.h> //if you don't use scanf/printf change this include
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <math.h>

int main(int argc, char *argv[])
{
    for (int i = 0; i < 100; i++)
    {

        int pid = fork();
        if (pid == 0)
        {
            int pid2 = fork();
            if (pid2 == 0)
            {
                execl("./test_generator.out", "test_generator.out", NULL);
            }
            else
            {
                int status;
                waitpid(pid2, &status, 0);
                if (status != 0)
                {
                    printf("error");
                    exit(-1);
                }
                printf("made tests\n");
                execl("./process_generator.out", "process_generator.out", NULL);
            }
            exit(0);
        }
        else
        {
            sleep(35);
                printf("\n\n\nsuccuess %d \n\n\n",i);
        }
    }
    printf("\n\n\nfinish yeah baby\n\n");
}