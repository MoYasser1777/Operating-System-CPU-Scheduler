#include "headers.h"
#include "queue.h"
#include "HPF.h"
#include "minHeap.c"

#define MemorySize 1024

/***************************************************************Variables***************************************************************/
// for shared memory
int msgqid, sigshmid;
int *sigshmaddr;

// for message queue
int processmsgqid;
struct message msg;

// for PCBs in the scheduler
struct PCB *processTable;
struct PCB *originProcessTable;
Queue waitingQueue;
pid_t *pids;
int pidCounter = 0;

int processesNumber;
minHeap priorityQueue;

int remainingProcesses;

FILE *sl;
FILE *sp;
FILE *ml;
int TE;
double TWTA, TW, TSTD;

Queue queue; // for round robin
int algorithm, quantum;

// for the current PCB running
struct PCB *HPF_current_PCB;
struct PCB RR_current_PCB;

// for SRTN
struct PCB *currentPCB;
int runningProcess = -1, prevTime = 0;
bool interrupt = 0;
void proccesEnd(struct PCB *process)
{
    process->end = getClk();
    process->turnaroundTime = process->end - process->fileInfo.arrival;
    process->waitingTime = process->turnaroundTime - process->fileInfo.runtime;
    process->executionTime = process->end - process->start;
}
// for Buddy
typedef struct buddyNode
{
    struct buddyNode *child[2];
    struct buddyNode *parent;
    int size;
    int id;
    int start;
    int smallest;
    bool full;
} node;
node Parent;
node **proccessNode;

// for memory
int memoryAlgorithm = -1;
int memory[MemorySize];

/***************************************************************Memory Algorithms***************************************************************/
bool First_Fit_Allocation(int id)
{
    processTable = originProcessTable;
    bool flag;
    int i;
    int start, end;
    for (i = 0; i < MemorySize; i++)
    {
        if (memory[i] == -1)
        {
            flag = true;
            for (int j = i; j < i + processTable[id].fileInfo.memSize; j++)
            {
                if (memory[j] != -1)
                {
                    flag = false;
                    break;
                }
            }

            if (flag)
            {
                for (int j = i; j < i + processTable[id].fileInfo.memSize; j++)
                    memory[j] = processTable[id].fileInfo.id;

                start = i;
                end = i + processTable[id].fileInfo.memSize - 1;
                break;
            }
        }
    }

    if (i == MemorySize)
    {
        return true;
    }

    fprintf(ml, "At time %d allocated %d bytes for process %d from %d to %d\n", getClk(), processTable[id].fileInfo.memSize, processTable[id].fileInfo.id, start, end);
    return false;
}

void First_Fit_Deallocation(int id)
{
    processTable = originProcessTable;

    int start, end;
    for (int i = 0; i < MemorySize; i++)
    {
        if (memory[i] == processTable[id].fileInfo.id)
        {
            for (int j = i; j < i + processTable[id].fileInfo.memSize; j++)
                memory[j] = -1;

            start = i;
            end = i + processTable[id].fileInfo.memSize - 1;
            break;
        }
    }
    fprintf(ml, "At time %d freed %d bytes for process %d from %d to %d\n", getClk(), processTable[id].fileInfo.memSize, processTable[id].fileInfo.id, start, end);

    struct Node *temp = waitingQueue.front;
    bool full;
    while (temp)
    {
        int tempid = temp->data.fileInfo.id - 1;
        full = First_Fit_Allocation(tempid);
        if (!full)
        {
            processTable = originProcessTable;
            pids[tempid] = fork();
            if (pids[tempid] == 0)
            {
                char runtime_str[10];   // assuming the maximum number of digits for runtime is 10
                char quantum_str[10];   // assuming the maximum number of digits for runtime is 10
                char algorithm_str[10]; // assuming the maximum number of digits for runtime is 10
                sprintf(algorithm_str, "%d", algorithm);
                sprintf(runtime_str, "%d", processTable[tempid].fileInfo.runtime);
                if (algorithm == 1)
                    quantum = processTable[tempid].fileInfo.runtime;
                sprintf(quantum_str, "%d", quantum);
                execl("./process.out", "process.out", runtime_str, quantum_str, algorithm_str, NULL);
            }
            else if (pids[tempid] == -1)
            {
                printf("error in fork\n");
            }
            kill(pids[tempid], SIGSTOP); // stop all the processes
            processTable[tempid].pid = pids[tempid];
            switch (algorithm)
            {
            case 1:
                penqueue(&processTable[tempid], processTable[tempid].fileInfo.priority);
                break;
            case 2:
                processTable[tempid].heapPriority = processTable[tempid].remainingTime;
                insertValue(&processTable[tempid], &priorityQueue);
                break;
            case 3:
                enqueue(&queue, processTable[tempid]);
                break;
            }
            dequeueid(&waitingQueue, temp->data.fileInfo.id);
        }
        temp = temp->next;
    }
}

// 256
void printRange(node *current, bool flag, int id)
{
    processTable = originProcessTable;
    if (flag == 1)
        fprintf(ml, "At time %d allocated %d bytes for process %d from %d to %d\n", getClk(), processTable[id].fileInfo.memSize, processTable[id].fileInfo.id, current->start, current->start + current->size - 1);
    else
        fprintf(ml, "At time %d freed %d bytes for process %d from %d to %d\n", getClk(), processTable[id].fileInfo.memSize, processTable[id].fileInfo.id, current->start, current->start + current->size - 1);
}

void setNode(node *current, node *parent, int id, int start, bool full)
{
    current->child[0] = current->child[1] = NULL;
    current->full = full;
    current->id = id;
    current->parent = parent;
    current->size = parent->size / 2;
    current->start = start;
    if (id != -1)
    {
        proccessNode[id] = current;
    }
}

void checkFullNode(node *current)
{
    if (current->child[0] != NULL && current->child[1] != NULL && current->child[0]->full && current->child[1]->full)
        current->full = 1;
}

node *findMIN(node *current, int size)
{
    node *ret = NULL;
    for (int i = 0; i < 2; i++)
    {
        if (current->child[i] && !current->child[i]->full)
        {
            node *temp = findMIN(current->child[i], size);
            if (temp != NULL && (ret == NULL || temp->size < ret->size))
                ret = temp;
        }
        else if (current->child[i] == NULL && current->size / 2 >= size && ret == NULL)
        {
            ret = current;
        }
    }

    return ret;
}

bool BUDDY_MEMORY_ALLOCATION(node *current, int id) // spliting function only
{
    processTable = originProcessTable;
    int size = processTable[id].fileInfo.memSize;
    if (current->size / 4 < size)
    {
        if (current->child[0] == NULL)
        {
            current->child[0] = malloc(sizeof(node));
            setNode(current->child[0], current, id, current->start, true);
            printRange(current->child[0], 1, id);
        }
        else if (current->child[1] == NULL)
        {
            current->child[1] = malloc(sizeof(node));
            setNode(current->child[1], current, id, current->start + current->size / 2, true);
            printRange(current->child[1], 1, id);
        }
        else
            return 0;

        checkFullNode(current);
        return 1;
    }
    for (int i = 0; i < 2; i++)
    {
        if (current->child[i] == NULL)
        {
            current->child[i] = malloc(sizeof(node));
            setNode(current->child[i], current, -1, current->start + i * current->size / 2, false);
        }
        if (!current->child[i]->full && BUDDY_MEMORY_ALLOCATION(current->child[i], id))
        {
            checkFullNode(current);
            return 1;
        }
    }

    return 0;
}

bool buddyMemoryAllocation(int id)
{
    node *mn = findMIN(&Parent, originProcessTable[id].fileInfo.memSize);
    if (mn == NULL)
        return 1;
    BUDDY_MEMORY_ALLOCATION(mn, id);
    return 0;
}

void BUDDY_MEMORY_DEALLOCATION(node *current, bool flag, int id)
{
    if (flag)
        printRange(current, 0, id);

    node *parent = current->parent;
    if (parent->child[0] == current)
    {
        free(parent->child[0]);
        parent->child[0] = NULL;
    }
    else
    {
        free(parent->child[1]);
        parent->child[1] = NULL;
    }

    while (true)
    {
        if (parent->parent != NULL && parent->child[0] == NULL && parent->child[1] == NULL)
        {
            BUDDY_MEMORY_DEALLOCATION(parent, 0, id);
            break;
        }
        parent->full = false;
        if (parent->parent == NULL)
            break;
        parent = parent->parent;
    }
    if (flag)
    {

        struct Node *temp = waitingQueue.front;
        bool full;
        while (temp)
        {
            int tempid = temp->data.fileInfo.id - 1;
            full = buddyMemoryAllocation(tempid);
            if (!full)
            {
                processTable = originProcessTable;
                pids[tempid] = fork();
                if (pids[tempid] == 0)
                {
                    char runtime_str[10];   // assuming the maximum number of digits for runtime is 10
                    char quantum_str[10];   // assuming the maximum number of digits for runtime is 10
                    char algorithm_str[10]; // assuming the maximum number of digits for runtime is 10
                    sprintf(algorithm_str, "%d", algorithm);
                    sprintf(runtime_str, "%d", processTable[tempid].fileInfo.runtime);
                    if (algorithm == 1)
                        quantum = processTable[tempid].fileInfo.runtime;
                    sprintf(quantum_str, "%d", quantum);
                    execl("./process.out", "process.out", runtime_str, quantum_str, algorithm_str, NULL);
                }
                else if (pids[tempid] == -1)
                {
                    printf("error in fork\n");
                }
                kill(pids[tempid], SIGSTOP); // stop all the processes
                processTable[tempid].pid = pids[tempid];
                switch (algorithm)
                {
                case 1:
                    penqueue(&processTable[tempid], processTable[tempid].fileInfo.priority);
                    break;
                case 2:
                    processTable[tempid].heapPriority = processTable[tempid].remainingTime;
                    insertValue(&processTable[tempid], &priorityQueue);
                    break;
                case 3:
                    enqueue(&queue, processTable[tempid]);
                    break;
                }
                dequeueid(&waitingQueue, temp->data.fileInfo.id);
            }
            temp = temp->next;
        }
    }
}

/***************************************************************Send/Recieve Handler***************************************************************/
// Handler for receiving signal from process generator when a process is arrived
void handler1(int signo)
{
    processTable = originProcessTable;

    if (algorithm == 2 && runningProcess != -1)
    {
        interrupt = 1;
        kill(runningProcess, SIGUSR1);
        int currentTime = getClk();
        if (prevTime < currentTime)
        {
            currentPCB->remainingTime--;
            currentPCB->heapPriority--;
        }
        if (currentPCB->remainingTime > 0)
            insertValue(currentPCB, &priorityQueue);
        else
        {
            remainingProcesses--;
            proccesEnd(currentPCB);
        }
    }

    struct process temp;
    int tempProcesses = *sigshmaddr;
    int id;
    for (int i = 0; i < tempProcesses; i++)
    {
        msgrcv(msgqid, &temp, sizeof(struct process), 0, !IPC_NOWAIT);
        printf("Scheduler:: Recieve process-> id: %d, arrival: %d, runtime: %d, priority: %d, memSize: %d\n", temp.id, temp.arrival, temp.runtime, temp.priority, temp.memSize);
        id = temp.id - 1;
        processTable[id].fileInfo = temp;
        processTable[id].state = 0;
        processTable[id].remainingTime = temp.runtime;
        processTable[id].start = -1;

        bool memoryIsFull;
        switch (memoryAlgorithm)
        {
        case 1:
            memoryIsFull = First_Fit_Allocation(id);
            break;
        case 2:
            memoryIsFull = buddyMemoryAllocation(id);
            break;
        }

        if (!memoryIsFull)
        {
            processTable = originProcessTable;
            pids[id] = fork();
            if (pids[id] == 0)
            {
                char runtime_str[10];   // assuming the maximum number of digits for runtime is 10
                char quantum_str[10];   // assuming the maximum number of digits for runtime is 10
                char algorithm_str[10]; // assuming the maximum number of digits for runtime is 10
                sprintf(algorithm_str, "%d", algorithm);
                sprintf(runtime_str, "%d", processTable[id].fileInfo.runtime);
                if (algorithm == 1)
                    quantum = processTable[id].fileInfo.runtime;
                sprintf(quantum_str, "%d", quantum);
                execl("./process.out", "process.out", runtime_str, quantum_str, algorithm_str, NULL);
            }
            else if (pids[id] == -1)
            {
                printf("error in fork\n");
            }
            kill(pids[id], SIGSTOP); // stop all the processes
            processTable[id].pid = pids[id];
            switch (algorithm)
            {
            case 1:
                penqueue(&processTable[id], processTable[id].fileInfo.priority);
                break;
            case 2:
                processTable[id].heapPriority = processTable[id].remainingTime;
                insertValue(&processTable[id], &priorityQueue);
                break;
            case 3:
                enqueue(&queue, processTable[id]);
                break;
            }
        }
        else
        {
            enqueue(&waitingQueue, processTable[id]);
        }
        pidCounter++;
    }
}

/***************************************************************Scheduler Algorithms***************************************************************/
// Function for HPF implementation
void HPF_Algo()
{
    while (remainingProcesses > 0)
    {
        while (pisempty())
        {
        }

        /*Switch to the next PCB*/
        HPF_current_PCB = ppeek();
        pdequeue();
        kill(HPF_current_PCB->pid, SIGCONT);

        /*Set Start Time*/
        HPF_current_PCB->start = getClk();
        HPF_current_PCB->state = 1;
        fprintf(sl, "At time %d process %d started arr %d total %d remain %d wait %d\n", getClk(), HPF_current_PCB->fileInfo.id, HPF_current_PCB->fileInfo.arrival, HPF_current_PCB->fileInfo.runtime, HPF_current_PCB->fileInfo.runtime, HPF_current_PCB->start - HPF_current_PCB->fileInfo.arrival);

        /*Wait untill process finish its runtime*/
        while (msgrcv(processmsgqid, &msg, sizeof(struct message), 1001, !IPC_NOWAIT) == -1)
        {
        }

        /*Some Calculations*/
        HPF_current_PCB->end = getClk();
        HPF_current_PCB->waitingTime = HPF_current_PCB->start - HPF_current_PCB->fileInfo.arrival;
        HPF_current_PCB->turnaroundTime = HPF_current_PCB->end - HPF_current_PCB->fileInfo.arrival;
        HPF_current_PCB->executionTime = HPF_current_PCB->end - HPF_current_PCB->start;
        HPF_current_PCB->state = 0;
        remainingProcesses--;
        TE += HPF_current_PCB->fileInfo.runtime;
        TWTA += HPF_current_PCB->turnaroundTime * 1.0 / HPF_current_PCB->fileInfo.runtime;
        TW += HPF_current_PCB->waitingTime;

        switch (memoryAlgorithm)
        {
        case 1:
            First_Fit_Deallocation(HPF_current_PCB->fileInfo.id - 1);
            break;
        case 2:
            BUDDY_MEMORY_DEALLOCATION(proccessNode[HPF_current_PCB->fileInfo.id - 1], 1, HPF_current_PCB->fileInfo.id - 1);
            break;
        }

        fprintf(sl, "At time %d process %d finished arr %d total %d remain %d wait %d TA %d WTA %.2f\n", getClk(), HPF_current_PCB->fileInfo.id, HPF_current_PCB->fileInfo.arrival, HPF_current_PCB->fileInfo.runtime, 0, HPF_current_PCB->start - HPF_current_PCB->fileInfo.arrival, HPF_current_PCB->turnaroundTime, HPF_current_PCB->turnaroundTime * 1.0 / HPF_current_PCB->fileInfo.runtime);
    }
}

// Function for RR implementation
void RR_Algo()
{
    while (remainingProcesses > 0)
    {
        while (is_empty(&queue))
        {
        }

        RR_current_PCB = dequeue(&queue);
        kill(RR_current_PCB.pid, SIGCONT);
        RR_current_PCB.state = 1;

        /*Set Start Time*/
        if (RR_current_PCB.start == -1)
        {
            RR_current_PCB.start = getClk();
            RR_current_PCB.remainingTime = RR_current_PCB.fileInfo.runtime;
            fprintf(sl, "At time %d process %d started arr %d total %d remain %d wait %d\n", getClk(), RR_current_PCB.fileInfo.id, RR_current_PCB.fileInfo.arrival, RR_current_PCB.fileInfo.runtime, RR_current_PCB.remainingTime, getClk() - (RR_current_PCB.fileInfo.runtime - RR_current_PCB.remainingTime) - RR_current_PCB.fileInfo.arrival);
        }
        else
            fprintf(sl, "At time %d process %d resumed arr %d total %d remain %d wait %d\n", getClk(), RR_current_PCB.fileInfo.id, RR_current_PCB.fileInfo.arrival, RR_current_PCB.fileInfo.runtime, RR_current_PCB.remainingTime, getClk() - (RR_current_PCB.fileInfo.runtime - RR_current_PCB.remainingTime) - RR_current_PCB.fileInfo.arrival);

        /*Wait untill process finish its quantum*/
        while (msgrcv(processmsgqid, &msg, sizeof(struct message), 1001, !IPC_NOWAIT) == -1)
        {
        }

        if (msg.status == 1)
        {
            /*Some Calculations*/
            RR_current_PCB.remainingTime = 0;
            RR_current_PCB.end = getClk();
            RR_current_PCB.turnaroundTime = RR_current_PCB.end - RR_current_PCB.fileInfo.arrival;
            RR_current_PCB.executionTime = RR_current_PCB.end - RR_current_PCB.start;
            RR_current_PCB.waitingTime = RR_current_PCB.turnaroundTime - RR_current_PCB.fileInfo.runtime;
            RR_current_PCB.state = 0;
            remainingProcesses--;
            TE += RR_current_PCB.executionTime;
            TWTA += RR_current_PCB.turnaroundTime * 1.0 / RR_current_PCB.fileInfo.runtime;
            TW += RR_current_PCB.waitingTime;

            switch (memoryAlgorithm)
            {
            case 1:
                First_Fit_Deallocation(RR_current_PCB.fileInfo.id - 1);
                break;
            case 2:
                BUDDY_MEMORY_DEALLOCATION(proccessNode[RR_current_PCB.fileInfo.id - 1], 1, RR_current_PCB.fileInfo.id - 1);
                break;
            }

            fprintf(sl, "At time %d process %d finished arr %d total %d remain %d wait %d TA %d WTA %.2f\n", getClk(), RR_current_PCB.fileInfo.id, RR_current_PCB.fileInfo.arrival, RR_current_PCB.fileInfo.runtime, 0, getClk() - (RR_current_PCB.fileInfo.runtime - RR_current_PCB.remainingTime) - RR_current_PCB.fileInfo.arrival, RR_current_PCB.turnaroundTime, RR_current_PCB.turnaroundTime * 1.0 / RR_current_PCB.fileInfo.runtime);
        }
        else
        {
            RR_current_PCB.remainingTime -= quantum;
            RR_current_PCB.state = 0;
            fprintf(sl, "At time %d process %d stopped arr %d total %d remain %d wait %d\n", getClk(), RR_current_PCB.fileInfo.id, RR_current_PCB.fileInfo.arrival, RR_current_PCB.fileInfo.runtime, RR_current_PCB.remainingTime, getClk() - (RR_current_PCB.fileInfo.runtime - RR_current_PCB.remainingTime) - RR_current_PCB.fileInfo.arrival);
            enqueue(&queue, RR_current_PCB);
        }
    }
}

// Function for SRTN implementation
void SRTN_Algo()
{
    while (remainingProcesses > 0)
    {
        while (isEmpty(&priorityQueue))
        {
        }
        currentPCB = heapExtractMin(&priorityQueue);
        runningProcess = currentPCB->pid;
        kill(currentPCB->pid, SIGCONT);

        prevTime = getClk();

        currentPCB->state = 1;
        int rem = currentPCB->remainingTime;
        while (msgrcv(processmsgqid, &msg, sizeof(struct message), 1001, !IPC_NOWAIT) == -1 && !interrupt)
        {
        }
        if (interrupt)
        {
            interrupt = 0;
            continue;
        }

        int currentTime = getClk();
        if (currentPCB->start == -1)
        {
            currentPCB->start = prevTime;
            fprintf(sl, "At time %d process %d started arr %d total %d remain %d wait %d\n", prevTime, currentPCB->fileInfo.id, currentPCB->fileInfo.arrival, currentPCB->fileInfo.runtime, rem, prevTime - (currentPCB->fileInfo.runtime - currentPCB->remainingTime) - currentPCB->fileInfo.arrival);
        }
        else
            fprintf(sl, "At time %d process %d resumed arr %d total %d remain %d wait %d\n", prevTime, currentPCB->fileInfo.id, currentPCB->fileInfo.arrival, currentPCB->fileInfo.runtime, rem, prevTime - (currentPCB->fileInfo.runtime - currentPCB->remainingTime) - currentPCB->fileInfo.arrival);
        currentPCB->state = 0;

        if (msg.status == 1)
        {
            switch (memoryAlgorithm)
            {
            case 1:
                First_Fit_Deallocation(currentPCB->fileInfo.id - 1);
                break;
            case 2:
                BUDDY_MEMORY_DEALLOCATION(proccessNode[currentPCB->fileInfo.id - 1], 1, currentPCB->fileInfo.id - 1);
                break;
            }
            proccesEnd(currentPCB);
            remainingProcesses--;
            currentPCB->remainingTime = 0;
            TE += currentPCB->executionTime;
            TWTA += currentPCB->turnaroundTime * 1.0 / currentPCB->fileInfo.runtime;
            TW += currentPCB->waitingTime;

            fprintf(sl, "At time %d process %d finished arr %d total %d remain %d wait %d TA %d WTA %.2f\n", getClk(), currentPCB->fileInfo.id, currentPCB->fileInfo.arrival, currentPCB->fileInfo.runtime, 0, getClk() - (currentPCB->fileInfo.runtime - currentPCB->remainingTime) - currentPCB->fileInfo.arrival, currentPCB->turnaroundTime, currentPCB->turnaroundTime * 1.0 / currentPCB->fileInfo.runtime);
        }
        else
        {
            currentPCB->remainingTime -= getClk() - prevTime;
            currentPCB->heapPriority -= getClk() - prevTime;
            runningProcess = -1;
            fprintf(sl, "At time %d process %d stopped arr %d total %d remain %d wait %d\n", getClk(), currentPCB->fileInfo.id, currentPCB->fileInfo.arrival, currentPCB->fileInfo.runtime, currentPCB->remainingTime, currentTime - (currentPCB->fileInfo.runtime - currentPCB->remainingTime) - currentPCB->fileInfo.arrival);
            insertValue(currentPCB, &priorityQueue);
        }
    }
}

/***************************************************************MAIN***************************************************************/
int main(int argc, char *argv[])
{
    signal(SIGUSR1, handler1);

    TE = TSTD = TW = TWTA = 0;

    sl = fopen("scheduler.log", "w");

    if (sl == NULL)
    {
        printf("Error opening file\n");
        return 1;
    }

    fclose(sl);

    sl = fopen("scheduler.log", "a");

    if (sl == NULL)
    {
        printf("Error opening file\n");
        return 1;
    }

    fprintf(sl, "#At time x process y state arr w total z remain y wait k\n");

    sp = fopen("scheduler.perf", "w");

    if (sp == NULL)
    {
        printf("Error opening file\n");
        return 1;
    }

    ml = fopen("memory.log", "w");

    if (ml == NULL)
    {
        printf("Error opening file\n");
        return 1;
    }
    fclose(ml);

    ml = fopen("memory.log", "a");

    if (ml == NULL)
    {
        printf("Error opening file\n");
        return 1;
    }
    fprintf(ml, "#At time x allocated y bytes for process z from I to j\n");

    if (argc != 5)
    {
        printf("ERROR, few arguments\n");
    }

    algorithm = atoi(argv[1]);
    quantum = atoi(argv[2]);
    processesNumber = atoi(argv[3]);
    memoryAlgorithm = atoi(argv[4]);

    processTable = (struct PCB *)malloc(processesNumber * sizeof(struct PCB));
    proccessNode = malloc(processesNumber * sizeof(node *));
    memset(proccessNode, 0, sizeof(proccessNode)); // setting array of pointers to NULL

    originProcessTable = processTable;
    pids = (pid_t *)malloc(processesNumber * sizeof(pid_t));
    remainingProcesses = processesNumber;

    Parent.child[0] = Parent.child[1] = NULL;
    Parent.parent = NULL;
    Parent.id = -1;
    Parent.size = MemorySize;
    Parent.start = 0;
    Parent.smallest = MemorySize / 2;

    // initalize memory
    for (int i = 0; i < MemorySize; i++)
        memory[i] = -1;

    if (memoryAlgorithm == 2)
        proccessNode = malloc(processesNumber * sizeof(node *));

    if (algorithm == 2)
        initialize(processesNumber, &priorityQueue);
    if (algorithm == 3)
        init_queue(&queue);

    // for message queue of receiving from process generator
    key_t key = ftok("key", 'p');
    msgqid = msgget(key, 0666 | IPC_CREAT);

    // shared memory of receiving from process generator for special case of coming many processes in same time
    key_t sigkey = ftok("key", 'n');
    sigshmid = shmget(sigkey, 4, 0666 | IPC_CREAT);
    sigshmaddr = (int *)shmat(sigshmid, (void *)0, 0);

    // for message queue of receiving from process
    key_t pKey = ftok("key", 'i');
    processmsgqid = msgget(pKey, 0666 | IPC_CREAT);

    init_queue(&waitingQueue);
    initClk();

    switch (algorithm)
    {
    case 1:
        HPF_Algo();
        break;
    case 2:
        SRTN_Algo();
        break;
    case 3:
        RR_Algo();
        break;
    default:
        break;
    }

    msgctl(processmsgqid, IPC_RMID, NULL);
    processTable = originProcessTable;

    for (int i = 0; i < processesNumber; i++)
        TSTD += pow((processTable[i].turnaroundTime * 1.0 / processTable[i].fileInfo.runtime - TWTA), 2);

    fprintf(sp, "CPU utilization = %.2f%%\nAvg WTA = %.2f\nAvg Waiting = %.2f\nStd WTA = %.2f\n", ((TE * 1.0 / getClk()) * 100.0), TWTA / processesNumber, TW / processesNumber, sqrt(TSTD / (double)processesNumber));
    fclose(ml);
    fclose(sp);
    fclose(sl);
    free(originProcessTable);
    if (memoryAlgorithm == 2)
        free(proccessNode);
    if (algorithm == 2)
        freeHeap(&priorityQueue);

    printf("\n........DONE........\n");
    // destroyClk(true);
}