#include "headers.h"
#include <string.h>

int algorithm = -1, memoryAlgorithm = -1, quantum = 0, processesNumber = 0;
pid_t pids[2];

int msgqid, sigshmid;
int *sigshmaddr;

void clearResources(int signum)
{
    // TODO Clears all resources in case of interruption
    // free malloc
    msgctl(msgqid, IPC_RMID, NULL);
    shmctl(sigshmid, IPC_RMID, 0);
    kill(pids[0], SIGKILL);
    kill(pids[1], SIGKILL);
    kill(getpid(), SIGKILL);
}

void readFile(struct process **processes_ptr, char *filename)
{
    FILE *file;
    char line[100];

    file = fopen(filename, "r");
    if (file == NULL)
    {
        printf("Failed to open file.");
        return;
    }

    while (fgets(line, sizeof(line), file))
    {
        if (strncmp(line, "#", 1) == 0)
        {
            continue; // Skip comment lines
        }
        processesNumber++;
    }

    rewind(file);

    *processes_ptr = (struct process *)malloc(processesNumber * sizeof(struct process));

    // #id arrival runtime priority
    int id, arrival, runtime, priority, memory;
    int i = 0;
    while (fgets(line, sizeof(line), file))
    {
        if (strncmp(line, "#", 1) == 0)
        {
            continue; // Skip comment lines
        }
        if (sscanf(line, "%d\t%d\t%d\t%d\t%d", &id, &arrival, &runtime, &priority, &memory) == 5)
        {
            (*processes_ptr + i)->id = id; // use *processes_ptr to access the allocated memory
            (*processes_ptr + i)->arrival = arrival;
            (*processes_ptr + i)->runtime = runtime;
            (*processes_ptr + i)->priority = priority;
            (*processes_ptr + i)->memSize = memory;
            i++;
        }
    }

    fclose(file);
}

void chooseAlgorithms(char *algoText, char *algo, char *quantaText, char *quanta, char *memText, char *mem)
{

    if (strcmp(algoText, "-sch") != 0)
    {
        printf("invalid -sch argument entry\nExiting...\n");
        exit(0);
    }

    algorithm = atoi(algo);
    if (algorithm == 3)
    {
        if (strcmp(quantaText, "-q") != 0)
        {
            printf("invalid -q argument entry\nExiting...\n");
            exit(0);
        }
        quantum = atoi(quanta);
        if (quantum <= 0)
        {
            printf("invalid quantum value entry\nExiting...\n");
            exit(0);
        }
    }
    if (algorithm < 1 || algorithm > 3)
    {
        printf("invalid algorithm value entry\nExiting...\n");
        exit(0);
    }
    if (strcmp(memText, "-mem") != 0)
    {
        printf("invalid -mem argument entry\nExiting...\n");
        exit(0);
    }
    memoryAlgorithm = atoi(mem);
    if (memoryAlgorithm < 1 || memoryAlgorithm > 2)
    {
        printf("invalid memory value entry\nExiting...\n");
        exit(0);
    }
    return;
}

void initiateChildren()
{
    for (int i = 0; i < 2; i++)
    {
        pids[i] = fork();
        if (pids[i] == -1)
        {
            printf("Error in fork\n");
            exit(-1);
        }
        else if (pids[i] == 0)
        {
            if (i == 0)
                execl("./clk.out", "clk.out", NULL);
            else
            {
                char alg_str[10];
                char qntm_str[10];
                char prnm_str[10];
                char memalg_str[10];
                sprintf(alg_str, "%d", algorithm);
                sprintf(qntm_str, "%d", quantum);
                sprintf(prnm_str, "%d", processesNumber);
                sprintf(memalg_str, "%d", memoryAlgorithm);
                execl("./scheduler.out", "scheduler.out", alg_str, qntm_str, prnm_str, memalg_str, NULL);
            }
        }
    }
}


//function used to sort the processes which has the same arrival time but different priorites
//so it sort it with highest priority to lowest
void insertionSort(struct process *arr, int n)
{
    int i, j;
    struct process key;
    for (i = 1; i < n; i++)
    {
        key = arr[i];
        j = i - 1;
        while (j >= 0 && arr[j].arrival == key.arrival && arr[j].priority > key.priority)
        {
            arr[j + 1] = arr[j];
            j = j - 1;
        }
        arr[j + 1] = key;
    }
}



int main(int argc, char *argv[])
{
    signal(SIGINT, clearResources);
    struct process *processes = NULL;
    // TODO Initialization
    // 1. Read the input files.
    if (argc != 8)
    {
        printf("Invalid number of arguments\nExiting...\n");
        exit(0);
    }
    int length = strlen(argv[1]);
    if (length > 4 && argv[1][length - 4] == '.' && argv[1][length - 3] == 't' && argv[1][length - 2] == 'x' && argv[1][length - 1] == 't')
        readFile(&processes, argv[1]);
    else
    {
        printf("The file is not .txt type\nExiting...\n");
        exit(0);
    }
    for (int i = 0; i < processesNumber; i++)
    {
        printf("when i = %d, id: %d, arrival: %d, runtime: %d, priority: %d, memSize:%d\n", i, (processes + i)->id, (processes + i)->arrival, (processes + i)->runtime, (processes + i)->priority, (processes + i)->memSize);
    }
    // 2. Ask the user for the chosen memory algorithm and scheduling algorithm and its parameters, if there are any.
    chooseAlgorithms(argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]);

    // 3. Initiate and create the scheduler and clock processes.
    initiateChildren();

    // 4. Use this function after creating the clock process to initialize clock

    key_t sigkey = ftok("key", 'n');
    sigshmid = shmget(sigkey, 4, 0666 | IPC_CREAT);
    sigshmaddr = (int *)shmat(sigshmid, (void *)0, 0);

    key_t key = ftok("key", 'p');
    msgqid = msgget(key, 0666 | IPC_CREAT);

    insertionSort(processes,processesNumber);

    // To get time use this
    initClk();

    int i = 0;
    while (i < processesNumber)
    {
        int currentTime = getClk();
        if ((processes + i)->arrival == currentTime)
        {
            int arrivedProcesses = 0;
            while ((processes + i)->arrival == currentTime)
            {
                printf("Process Generator:: Send process-> id: %d, arrival: %d, runtime: %d, priority: %d, memSize:%d\n", (processes + i)->id, (processes + i)->arrival, (processes + i)->runtime, (processes + i)->priority, (processes + i)->memSize);
                msgsnd(msgqid, &processes[i], sizeof(struct process), 0);
                i++;
                arrivedProcesses++;
            }

            *sigshmaddr = arrivedProcesses;
            kill(pids[1], SIGUSR1);
        }
    }

    // TODO Generation Main Loop
    // 5. Create a data structure for processes and provide it with its parameters.
    // 6. Send the information to the scheduler at the appropriate time.
    // 7. Clear clock resources
    int status;
    waitpid(pids[1], &status, 0);
    printf("Child process finished with status %d\n", status);

    destroyClk(true);
}
