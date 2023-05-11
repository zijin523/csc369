// ------------
// This code is provided solely for the personal and private use of
// students taking the CSC369H5 course at the University of Toronto.
// Copying for purposes other than this use is expressly prohibited.
// All forms of distribution of this code, whether as given or with
// any changes, are expressly prohibited.
//
// Authors: Bogdan Simion
//
// All of the files in this directory and all subdirectories are:
// Copyright (c) 2019 Bogdan Simion
// -------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "executor.h"

extern struct executor tassadar;


/**
 * Populate the job lists by parsing a file where each line has
 * the following structure:
 *
 * <id> <type> <num_resources> <resource_id_0> <resource_id_1> ...
 *
 * Each job is added to the queue that corresponds with its job type.
 */
void parse_jobs(char *file_name) {
    int id;
    struct job *cur_job;
    struct admission_queue *cur_queue;
    enum job_type jtype;
    int num_resources, i;
    FILE *f = fopen(file_name, "r");

    /* parse file */
    while (fscanf(f, "%d %d %d", &id, (int*) &jtype, (int*) &num_resources) == 3) {

        /* construct job */
        cur_job = malloc(sizeof(struct job));
        cur_job->id = id;
        cur_job->type = jtype;
        cur_job->num_resources = num_resources;
        cur_job->resources = malloc(num_resources * sizeof(int));

        int resource_id; 
				for(i = 0; i < num_resources; i++) {
				    fscanf(f, "%d ", &resource_id);
				    cur_job->resources[i] = resource_id;
				    tassadar.resource_utilization_check[resource_id]++;
				}
				
				assign_processor(cur_job);

        /* append new job to head of corresponding list */
        cur_queue = &tassadar.admission_queues[jtype];
        cur_job->next = cur_queue->pending_jobs;
        cur_queue->pending_jobs = cur_job;
        cur_queue->pending_admission++;
    }

    fclose(f);
}

/*
 * Magic algorithm to assign a processor to a job.
 */
void assign_processor(struct job* job) {
    int i, proc = job->resources[0];
    for(i = 1; i < job->num_resources; i++) {
        if(proc < job->resources[i]) {
            proc = job->resources[i];
        }
    }
    job->processor = proc % NUM_PROCESSORS;
}


void do_stuff(struct job *job) {
    /* Job prints its id, its type, and its assigned processor */
    printf("%d %d %d\n", job->id, job->type, job->processor);
}



/**
 * TODO: Fill in this function
 *
 * Do all of the work required to prepare the executor
 * before any jobs start coming
 * 
 */
void init_executor() {
    int i;

    for(i = 0; i < NUM_RESOURCES; i++){
        //initialize resource_locks
        pthread_mutex_init(&tassadar.resource_locks[i], NULL);
        //initialize resource_utilization_check
        tassadar.resource_utilization_check[i] = 0;
    }

    for(i = 0; i < NUM_QUEUES; i++){
        //initialize admission_queues lock
        pthread_mutex_init(&(tassadar.admission_queues[i].lock), NULL);
        //initialize admission_queues conds
        pthread_cond_init(&(tassadar.admission_queues[i].admission_cv), NULL);
        pthread_cond_init(&(tassadar.admission_queues[i].execution_cv), NULL);

         //initialize admission_queues other variables
        tassadar.admission_queues[i].pending_jobs = NULL;
        tassadar.admission_queues[i].pending_admission = 0;
        tassadar.admission_queues[i].admitted_jobs = malloc(sizeof(struct job *) * QUEUE_LENGTH);
        tassadar.admission_queues[i].capacity = QUEUE_LENGTH;
        tassadar.admission_queues[i].num_admitted = 0;
        tassadar.admission_queues[i].head = 0;
        tassadar.admission_queues[i].tail = 0;
    }

    for(i = 0; i < NUM_PROCESSORS; i++){
        //initailize processor_record
        tassadar.processor_records[i].completed_jobs = NULL;
        tassadar.processor_records[i].num_completed = 0;
        //initailize processor_record lock
        pthread_mutex_init(&(tassadar.processor_records[i].lock), NULL);
    }
}


/**
 * TODO: Fill in this function
 *
 * Handles an admission queue passed in through the arg (see the executor.c file). 
 * Bring jobs into this admission queue as room becomes available in it. 
 * As new jobs are added to this admission queue (and are therefore ready to be taken
 * for execution), the corresponding execute thread must become aware of this.
 * 
 */
void *admit_jobs(void *arg) {
    struct admission_queue *q = arg;

    //If a job needs to be admited 
    while(q->pending_admission > 0){
        //admission lock
        pthread_mutex_lock(&(q->lock));
        //When the queue is full, need to execute the first
        while(q->num_admitted == q->capacity){
            pthread_cond_wait(&(q->admission_cv), &(q->lock));
        } 
        //The queue is not full, add the job into queue by FIFO
        //add the first job to the certain index
        q->admitted_jobs[q->tail] = q->pending_jobs;
        //move the next job to be the first job
        q->pending_jobs = q->pending_jobs->next;
        //update the index, move it pointed to the next position
        q->tail = (q->tail + 1) % QUEUE_LENGTH;
        //update the number of job admitted
        q->num_admitted ++;;
        //update the number of job waited to be admitted
        q->pending_admission --;
        //notify the corresponding execution thread
        pthread_cond_signal(&(q->execution_cv));
        //admission unlock
        pthread_mutex_unlock(&(q->lock));     
    }

    return NULL;
}

/**
Helper Function for qsort()
Compare the value of int pointer a and int point b.
Return the their value difference.
Qsort will swicth their order only if the return value is negative.
*/
int comp(const void *a,const void *b){
    return *(int *)a - *(int *)b;
}


/**
 * TODO: Fill in this function
 *
 * Moves jobs from a single admission queue of the executor. 
 * Jobs must acquire the required resource locks before being able to execute. 
 *
 * Note: You do not need to spawn any new threads in here to simulate the processors.
 * When a job acquires all its required resources, it will execute do_stuff.
 * When do_stuff is finished, the job is considered to have completed.
 *
 * Once a job has completed, the admission thread must be notified since room
 * just became available in the queue. Be careful to record the job's completion
 * on its assigned processor and keep track of resources utilized. 
 *
 * Note: No printf statements are allowed in your final jobs.c code, 
 * other than the one from do_stuff!
 */
void *execute_jobs(void *arg) {
    struct admission_queue *q = arg;
    int i;
    struct job *cur_job;
    int cur_resources;
    int processor;

    //Some job admitted or some jobs pending to be admitted, 
    while(q->num_admitted != 0 || q->pending_admission != 0){  
        //Admission lock
        pthread_mutex_lock(&(q->lock));
        //There is not job admitted but some job pending to be admitted
        while(q->num_admitted == 0){
            //wait for new jobs to be admitted 
            pthread_cond_wait(&(q->execution_cv), &(q->lock));
        }
        //Get the first job in the admission queue
        cur_job = q->admitted_jobs[q->head];
        //Sort the resources this job needs in ascending order
        qsort(cur_job->resources,cur_job->num_resources,sizeof(int),comp);
        //Acquired for the resources this job need
        for(i = 0; i < cur_job->num_resources; i++){
            //Lock the resource once it is being used
            pthread_mutex_lock(&(tassadar.resource_locks[cur_job->resources[i]]));
            //Get the reosurce that is being acquried
            cur_resources = cur_job->resources[i];
            //Keep track of resources utilized. 
            tassadar.resource_utilization_check[cur_resources]--;
        }
        //Move "head" points to the next job in admission queue
        q->head = (q->head + 1) % QUEUE_LENGTH;
        //Update the number of job in admission queue
        q->num_admitted --;
        //lock the record
        pthread_mutex_lock(&(tassadar.processor_records[cur_job->processor].lock));
        //excute do_stuff
        do_stuff(cur_job);
        //The job is completetd, then get the job's assigned processor.
        processor = cur_job->processor;
        //Add the current job to the front of the completed jobs
        cur_job->next = tassadar.processor_records[processor].completed_jobs;
        tassadar.processor_records[processor].completed_jobs = cur_job;
        //Update the numeber of jobs completed
        tassadar.processor_records[processor].num_completed ++;
        //Records unlock
        pthread_mutex_unlock(&(tassadar.processor_records[processor].lock));
        for(i = 0; i < cur_job->num_resources; i++){      
            pthread_mutex_unlock(&(tassadar.resource_locks[cur_job->resources[i]]));
        }
        //the room is available in the queue, then notify admission thread
        pthread_cond_signal(&(q->admission_cv));
        //admission unlock
        pthread_mutex_unlock(&(q->lock));
    }

    return NULL;
}
