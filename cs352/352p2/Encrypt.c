#include "encrypt-module.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <ctype.h>


pthread_mutex_t mx;  // Mutex for input and output.
sem_t encrypt_sem_empty, encrypt_sem_full;
sem_t input_sem_empty, input_sem_full;
sem_t output_sem_empty, output_sem_full;
sem_t write_sem_empty, write_sem_full;
sem_t counter_full_sem, counter_empty_sem;
pthread_cond_t inputCV, inputCV2, outputCV, outputCV2; // condition variables
char *input_buffer, *output_buffer;     // buffer for input and output
int count_for_input = 0;       // count the input
int count_for_output = 0;      // count the output
int inputBS;            // input buffer size the user gonna type in
int outputBS;           // output buffer size the user gonna type in

/*
 * print the current input counts and key.
 */
void print_input(){
    printf("Total input count with current key is %d\n", get_input_total_count());
    for(int i = 0 ; i < 26 ; i++){
        printf("%c: %d ",i+'A', get_input_count(i));
    }
}

/*
 * print the current output counts and key.
 */
void print_output(){
    printf("\n");
    printf("Total output count with current key is %d\n", get_output_total_count());
    for(int i = 0 ; i < 26 ; i++){
        printf("%c: %d ",i+'A',get_output_count(i));
    }
}

/*
 * call print_input, printing same format as the example.
 */
void reset_requested(){
    sem_wait(&counter_empty_sem);
    sem_wait(&counter_full_sem);
    printf("\n");
    printf("\nReset requested.\n");
    print_input();
}

/*
 * call print_output, printing same format as the example.
 */
void reset_finished(){
    print_output();
    printf("\nReset finished.\n");
    sem_post(&counter_empty_sem);
    sem_wait(&counter_full_sem);
}

/*
 * Runs on a thread, it can read the input file.
 */
void *reading(arg){
    char ch;
    int iterator = 0;
    while((ch = read_input()) != EOF){  // read the input
        sem_wait(&input_sem_empty);
        sem_wait(&output_sem_empty);
        // check the input count and the input buffer size, if full then wait
        if(count_for_input == inputBS)
            pthread_cond_wait(&inputCV, &mx);
        input_buffer[iterator] = ch;
        iterator = (iterator + 1) % inputBS;
        if(count_for_input != 0)
            pthread_cond_signal(&inputCV2);
        sem_post(&counter_empty_sem);
        sem_post(&counter_full_sem);
        count_for_input++;
    }
}

/*
 * Runs on a thread, it can count the input.
 */
void *counting_in(arg) {
    int i = 0;
    while(1){
        sem_post(&counter_empty_sem);
        sem_wait(&counter_full_sem);
        if(input_buffer[i % inputBS] == EOF)
            break;
        count_input(input_buffer[i % inputBS]);
        i++;
    }
}

/*
 * Runs on a thread, it can encrypt the input file.
 */
void *encrypting(arg){
    int out = 0;
    int in = 0;
    char ch;
    while(1){
        sem_wait(&input_sem_full);

        pthread_mutex_lock(&mx); // ---------------------Lock
        if(count_for_input == 0)
            pthread_cond_wait(&inputCV2, &mx);
        ch = input_buffer[out];
        out = (out + 1) % outputBS;
        count_for_input--;
        // check the input count and input buffer size
        if(count_for_input != inputBS)
            pthread_cond_signal(&inputCV);
        pthread_mutex_unlock(&mx); // ------------------Unlock

        sem_post(&input_sem_empty);
        ch = caesar_encrypt(ch); // ----------------------------Encrypt
        if(ch == EOF)
            break;
        sem_wait(&output_sem_empty);

        pthread_mutex_lock(&mx); // -------------------Lock
        sem_wait(&counter_full_sem);
        if(count_for_output == outputBS)
            pthread_cond_wait(&outputCV, &mx);
        output_buffer[in] = ch;
        in = (in + 1) % inputBS;
        count_for_output++;
        if(count_for_output != 0)
            pthread_cond_signal(&outputCV2);
        sem_post(&output_sem_empty);
        pthread_mutex_unlock(&mx); // -----------------Unlock

        sem_post(&output_sem_full);
    }
}

/*
 * Runs on a thread, it can write the encrypted file to the output file.
 */
void *writing(arg){
    int i = 0;
    char ch = '\0';
    while(ch != EOF){
        sem_wait(&write_sem_full);
        if(count_for_output == 0)
            pthread_cond_wait(&outputCV2, &mx);
        ch = output_buffer[i];
        i = (i + 1) % outputBS;
        count_for_output--;
        // check the output count and the output buffer size
        if(count_for_output != outputBS)
            pthread_cond_signal(&outputCV);
        sem_post(&write_sem_empty);
        write_output(ch);
    }
}

/*
 * Runs on a thread, it can count the output.
 */
void *counting_out(arg){
    int i = 0;
    while(1){
        sem_post(&counter_full_sem);
        sem_wait(&counter_empty_sem);
        if(output_buffer[i % outputBS] == EOF)
            break;
        count_output(output_buffer[i % outputBS] );
        i++;
    }
}

/*
 * free semaphores at the end.
 */

main(int argc, char *argv[]) {
    /*
     * There should be three command line parameters
     */
    if (argc != 3) {
        printf("Invalid Command! try: ./encrypt352 inputFile outputFile.\n");
        exit(1);
    }

    printf("Enter the input buffer size: \n");
    scanf("%d", &inputBS);
    /*
     * check if input_buffer > 1. if not -> exit.
     */
    if(inputBS <= 1){
        printf("Input buffer should be greater than 1!\n");
        exit(1);
    }
    printf("Enter the output buffer size: \n");
    scanf("%d", &outputBS);
    /*
     * check if output_buffer > 1. if not -> exit.
     */
    if(outputBS <= 1){
        printf("Output buffer should be greater than 1!\n");
        exit(1);
    }


    // --------------- must call init in main function
    init(argv[1], argv[2]);

    input_buffer = malloc(sizeof(char)*inputBS);
    output_buffer = malloc(sizeof(char)*outputBS);

    sem_init(&input_sem_empty, 0, inputBS);
    sem_init(&input_sem_full, 0, 0);
    sem_init(&output_sem_empty, 0, outputBS);
    sem_init(&output_sem_full, 0, 0);
    sem_init(&write_sem_empty, 0, 0);
    sem_init(&write_sem_full, 0, 0);
    sem_init(&encrypt_sem_empty, 0, 0);
    sem_init(&encrypt_sem_full, 0, 0);

    pthread_cond_init(&inputCV, NULL);
    pthread_cond_init(&inputCV2, NULL);
    pthread_cond_init(&outputCV, NULL);
    pthread_cond_init(&outputCV2, NULL);

    pthread_t r, in_c, e, w, out_c;
    pthread_create(&r, NULL, reading, NULL);
    pthread_create(&in_c, NULL, counting_in, NULL);
    pthread_create(&e, NULL, encrypting, NULL);
    pthread_create(&w, NULL, writing, NULL);
    pthread_create(&out_c, NULL, counting_out, NULL);

    pthread_join(r, NULL);
    pthread_join(in_c, NULL);
    pthread_join(e, NULL);
    pthread_join(w, NULL);
    pthread_join(out_c, NULL);

    sem_destroy(&input_sem_empty);
    sem_destroy(&input_sem_full);
    sem_destroy(&output_sem_empty);
    sem_destroy(&output_sem_full);
    sem_destroy(&write_sem_empty);
    sem_destroy(&write_sem_full);
    sem_destroy(&encrypt_sem_empty);
    sem_destroy(&encrypt_sem_full);

    pthread_cond_destroy(&inputCV);
    pthread_cond_destroy(&inputCV2);
    pthread_cond_destroy(&outputCV);
    pthread_cond_destroy(&outputCV2);

    print_input();
    print_output();
    printf("End of file reached.\n");
    return 0;
}
