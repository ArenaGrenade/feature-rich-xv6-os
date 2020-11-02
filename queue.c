/* Circular queue implementation in an array */
#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"

void push(Queue* queue, struct proc* proc) {
    // cprintf("Pushing now ");
    if ((queue->front == 0 && queue->rear == NPROC - 1) ||
        (queue->front == queue->rear + 1)) {
            // cprintf("Queue %d has overflown\n", queue->queue_id);
            return;
    }

    if (queue->front == -1) {
        // This is the case when the queue is empty
        queue->front = 0;
        queue->rear = 0;
    } else {
        if (queue->rear == NPROC - 1) // Last element to be isnerted
            queue->rear = 0;
        else 
            queue->rear = queue->rear + 1;
    }
    queue->arr[queue->rear] = proc;

    // cprintf(" - %d \n", queue->arr[queue->rear]->pid);
    return;
}

struct proc* pop(Queue* queue) {
    if (queue->front == -1) {
        // cprintf("Queue %d has underflown\n", queue->queue_id);
        return 0;
    }

    int ret_ind = queue->front;

    if (queue->front == queue->rear) {
        // There is only one element in queue
        queue->front = -1;
        queue->rear = -1;
    } else {
        if (queue->front == NPROC - 1)
            queue->front = 0;
        else
            queue->front = queue->front + 1;
    }

    return queue->arr[ret_ind];
}

void display(Queue* queue) {
    int front_pos = queue->front;
    int rear_pos = queue->rear;

    if (front_pos == -1) {
        cprintf("Queue is empty - Unable to display\n");
        return;
    }

    cprintf("Queue %d: \n", queue->queue_id);
    if (front_pos <= rear_pos)
        while(front_pos <= rear_pos) {
            cprintf("%d ", queue->arr[front_pos]->pid);
            front_pos++;
        }
    else {
        while (front_pos <= MLFQSIZE - 1) {
            cprintf("%d ", queue->arr[front_pos]->pid);
            front_pos++;
        }
        front_pos = 0;
        while (front_pos <= rear_pos) {
            cprintf("%d ", queue->arr[front_pos]->pid);
            front_pos++;
        }
    }
    cprintf("\n");
    return;
}

int get_size(Queue* queue) {
    int front_pos = queue->front;
    int rear_pos = queue->rear;

    int size = 0;

    if (front_pos == -1) 
        return 0;

    if (front_pos <= rear_pos)
        while(front_pos <= rear_pos) {
            size++;
            front_pos++;
        }
    else {
        while (front_pos <= MLFQSIZE - 1) {
            size++;
            front_pos++;
        }
        front_pos = 0;
        while (front_pos <= rear_pos) {
            size++;
            front_pos++;
        }
    }
    return size;
}

