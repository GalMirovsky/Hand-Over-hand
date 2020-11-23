#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "concurrent_list.h"

// All functions use a similar way to synchronize:
// after taking care of special cases we iterate over the list using hand over hand

struct node {
    int value;
    node *next;
    pthread_mutex_t lock;
    // add more fields
};

struct list {
    // add fields
    node *head;

    // this lock is used as the head predecessor lock. since we use hand over hand, the first node doesn't have
    // a predecessor so we use this lock instead. it does not lock the entire list!
    // in most functions we use a variable: int firstIteration. this variable tells us if we're in the first
    // iteration over the list and should release this lock (headLock)
    pthread_mutex_t headLock;
};

struct node *createNode(int value);

int compareValueAndLock(int value, node *next);

void releaseNodeMemory(struct node *node1);

void print_node(node *node) {
    // DO NOT DELETE
    if (node)
    {
        printf("%d ", node->value);
    }
}

list *create_list() {

    list *list1 = (list *) malloc(sizeof(list));
    if (!list1)
    {
        printf("malloc failed, exiting..\n");
        exit(-1);
    }

    list1->head = NULL;
    pthread_mutex_init(&(list1->headLock), NULL);

    return list1;
}

// here we get the first value in the list and then call to remove_value()
void delete_list(list *list) {

    node *node1, *temp;
    int value;

    if (!list)
        return;

    pthread_mutex_lock(&(list->headLock));
    while (list->head)
    {
        pthread_mutex_lock(&(list->head->lock));
        value = list->head->value;
        pthread_mutex_unlock(&(list->head->lock));
        pthread_mutex_unlock(&(list->headLock));

        remove_value(list, value);
        pthread_mutex_lock(&list->headLock);
    }

    pthread_mutex_unlock(&(list->headLock));
    pthread_mutex_destroy(&(list->headLock));
    free(list);
}

// We first take care of special cases and then iterate over the list using hand over hand.
// example: list is 1, 2, 3. insert 4 => we'll only lock 2 & 3
void insert_value(list *list, int value) {
    // add code here

    node *current, *newNode, *prev;
    int firstIteration = 1;

    if (!list)
        return;

    newNode = createNode(value);
    pthread_mutex_lock(&(list->headLock));

    if (!list->head)
    {
        list->head = newNode;
        pthread_mutex_unlock(&(list->headLock));
        return;
    }

    pthread_mutex_lock(&(list->head->lock));

    // value should be the first element. example: list is: {5,...}, value is 1
    if (list->head->value > value)
    {
        newNode->next = list->head;
        list->head = newNode;

        pthread_mutex_unlock(&(list->head->next->lock));
        pthread_mutex_unlock(&(list->headLock));
        return;
    }

    current = list->head;

    // list is {5}, value is 6
    if (!current->next)
    {
        current->next = newNode;
        pthread_mutex_unlock(&(current->lock));
        pthread_mutex_unlock(&(list->headLock));
        return;
    }

    // here list is {5, 7 ,...}, value > 5
    while (compareValueAndLock(value, current->next))
    {
        if (firstIteration)
        {
            pthread_mutex_unlock(&(list->headLock));
            firstIteration = 0;
        }
        else
            pthread_mutex_unlock(&(prev->lock));
        prev = current;
        current = current->next;
    }

    newNode->next = current->next;
    current->next = newNode;
    pthread_mutex_unlock(&(current->lock));
    if (firstIteration)
        pthread_mutex_unlock(&(list->headLock));
    else
        pthread_mutex_unlock(&(prev->lock));
}

// this func compares the value with next->value and if the next node still smaller, locks `next` and returns 1
int compareValueAndLock(int value, node *next) {

    if (!next)
        return 0;

    pthread_mutex_lock(&(next->lock));
    if (next->value <= value)
        return 1;

    pthread_mutex_unlock(&(next->lock));
    return 0;
}

struct node *createNode(int value) {

    struct node *newNode;

    newNode = (node *) malloc(sizeof(node));
    if (!newNode)
    {
        printf("malloc failed\n");
        exit(-1);
    }
    newNode->value = value;
    newNode->next = NULL;
    if (pthread_mutex_init(&(newNode->lock), NULL) != 0)
    {
        printf("Unable to initialize lock\n");
        exit(-1);
    }
    return newNode;
}

void releaseNodeMemory(struct node *node1) {

    if (!node1)
        return;

    pthread_mutex_destroy(&(node1->lock));
    free(node1);
}

// We first take care of special cases and then iterate over the list using hand over hand.
// example: list is 1, 2, 3. remove 3 => we'll only lock 2 & 3
void remove_value(list *list, int value) {

    node *current, *prev, *temp;
    int firstIteration = 1;

    if (!list)
        return;

    pthread_mutex_lock(&(list->headLock));

    if (!list->head)
    {
        pthread_mutex_unlock(&(list->headLock));
        return;
    }

    pthread_mutex_lock(&(list->head->lock));

    // here we should delete list->head
    if (list->head->value == value)
    {
        temp = list->head;
        list->head = list->head->next;
        pthread_mutex_unlock(&(temp->lock));
        releaseNodeMemory(temp);
        pthread_mutex_unlock(&(list->headLock));
        return;
    }

    current = list->head;

    while (current->next)
    {
        pthread_mutex_lock(&(current->next->lock));
        if (firstIteration)
        {
            pthread_mutex_unlock(&(list->headLock));
            firstIteration = 0;
        }
        else
            pthread_mutex_unlock(&(prev->lock));

        if (current->next->value == value)
        {
            temp = current->next;
            current->next = current->next->next;
            pthread_mutex_unlock(&(temp->lock));
            releaseNodeMemory(temp);
            pthread_mutex_unlock(&(current->lock));
            return;
        }
        prev = current;
        current = current->next;
    }

    pthread_mutex_unlock(&(current->lock));
    if (firstIteration)
        pthread_mutex_unlock(&(list->headLock));
    else
        pthread_mutex_unlock(&(prev->lock));
}

void print_list(list *list) {

    node *current, *prev;
    int firstIteration = 1;

    if (!list)
    {
        printf("\n");
        return;
    }

    pthread_mutex_lock(&(list->headLock));

    if (!list->head)
    {
        printf("\n");
        pthread_mutex_unlock(&(list->headLock));
        return;
    }

    pthread_mutex_lock(&(list->head->lock));
    current = list->head;
    print_node(current);

    while (current->next)
    {
        pthread_mutex_lock(&(current->next->lock));
        if (firstIteration)
        {
            pthread_mutex_unlock(&(list->headLock));
            firstIteration = 0;
        }
        else
            pthread_mutex_unlock(&(prev->lock));

        print_node(current->next);
        prev = current;
        current = current->next;
    }

    pthread_mutex_unlock(&(current->lock));
    if (firstIteration)
        pthread_mutex_unlock(&(list->headLock));
    else
        pthread_mutex_unlock(&(prev->lock));

    printf("\n"); // DO NOT DELETE
}

void count_list(list *list, int (*predicate)(int)) {

    int count = 0; // DO NOT DELETE
    int firstIteration = 1;
    node *current, *prev;

    if (!list)
    {
        printf("%d items were counted\n", count);
        return;
    }

    pthread_mutex_lock(&(list->headLock));

    if (!list->head)
    {
        printf("%d items were counted\n", count);
        pthread_mutex_unlock(&(list->headLock));
        return;
    }

    pthread_mutex_lock(&(list->head->lock));
    current = list->head;
    if (predicate(current->value))
        count++;

    while (current->next)
    {
        pthread_mutex_lock(&(current->next->lock));
        if (firstIteration)
        {
            pthread_mutex_unlock(&(list->headLock));
            firstIteration = 0;
        }
        else
            pthread_mutex_unlock(&(prev->lock));

        if (predicate(current->next->value))
            count++;

        prev = current;
        current = current->next;
    }

    pthread_mutex_unlock(&(current->lock));
    if (firstIteration)
        pthread_mutex_unlock(&(list->headLock));
    else
        pthread_mutex_unlock(&(prev->lock));

    printf("%d items were counted\n", count); // DO NOT DELETE
}
