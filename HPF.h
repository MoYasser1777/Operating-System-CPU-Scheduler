#ifndef HPF
#define HPF
#include "headers.h"

typedef struct node
{
    int priority;
    struct PCB *info;
    struct node *link;
} NODE;

NODE *front2 = NULL;

// insert method
void penqueue(struct PCB *data, int priority)
{
    NODE *temp, *q;

    temp = (NODE *)malloc(sizeof(NODE));
    temp->info = data;
    temp->priority = priority;
    // condition to check whether the first element is empty or the element to be inserted has more priority than the first element
    if (front2 == NULL || priority < front2->priority)
    {
        temp->link = front2;
        front2 = temp;
    }
    else
    {
        q = front2;
        while (q->link != NULL && q->link->priority <= priority)
            q = q->link;
        temp->link = q->link;
        q->link = temp;
    }
}

// delete method

struct PCB *ppeek()
{
    if (front2 != NULL)
        return front2->info;
    else
        return NULL;
}

void pdequeue()
{
    NODE *temp;
    // condition to check whether the Queue is empty or not
    if (front2 != NULL)
    {
        temp = front2;
        front2 = front2->link;
        free(temp);
    }
}

int pisempty()
{
    return (front2 == NULL);
}

void pdisplay()
{
    printf("printing queue: \n");
    NODE *temp = front2;
    while (temp)
    {
        printf("queue process, id: %d, start: %d, finish: %d, priority: %d\n", temp->info->fileInfo.id, temp->info->start, temp->info->end, temp->priority);
        temp = temp->link;
    }
}

#endif