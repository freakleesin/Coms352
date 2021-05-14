#include <stdlib.h>
#include <stdio.h>
#include "device-controller.h"

typedef struct DeviceBuffer{
    int *buffer;
    int head;
    int tail;
    int max;
    int full;
}DeviceBuffer,*pDeviceBuffer;

DeviceBuffer db;

int is_full() {
    return db.full == 1? 0 : -1;
}

int is_empty() {
    if(db.full == 0 && db.head == db.tail){
        return 0;
    }
    else{
        return -1;
    }
}

void enqueue(int val) {
    if(is_full() == 0){
        db.tail = (db.tail + 1) % db.max;
    }
    db.buffer[db.head] = val;
    db.head = (db.head + 1) % db.max;
    if(db.tail ==  db.head){
        db.full = 1;
    }
}

int dequeue() {
    if(is_empty() == 0){
        return -1;
    }
    
    int val = db.buffer[db.tail];
    db.tail = (db.tail + 1) % db.max;
    if(db.tail ==  db.head){
        db.full = 0;
    }
    return val;
}

void read_interrupt(int c) {
    enqueue(c);
    if(c == '\n'){
        c = dequeue();
        write_device(c);
    }
}

void write_done_interrupt() {
    int a = dequeue();
    write_device(a);
}

int main(int argc, char* argv[]) {
    if(argc != 2){
        printf("The format is incorrect.\n");
        exit(1);
    }
    int n = atoi(*(argv+1));
    db.buffer = malloc(n*sizeof(char));
    db.max = n;
    db.full = 0;
    db.head = 0;
    db.tail = 0;
    start();
    free(db.buffer);
    return 0;
}

