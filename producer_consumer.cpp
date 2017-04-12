#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <vector>
#include <time.h>

// item struct with two attributes
struct Item {
	int ID;
	useconds_t sleep_time;
};

pthread_mutex_t cant_touch_this = PTHREAD_MUTEX_INITIALIZER;
std::vector<Item*>buffer;
sem_t producer_semaphore;
sem_t consumer_semaphore;
int num_producers;
int num_consumers;
int buffer_size;
int num_items;
pthread_t *producer_threads;
pthread_t *consumer_threads;

void *Produce(void *arg) {

	// set thread ID to the argument passed to the thread
	int thread_ID = (intptr_t) arg;

	// divide the items based on the number of threads
	int multiplier = num_items/num_producers;
	// set the start of each thread's responsibility to its ID * multiplier
	int start = thread_ID * multiplier;
	int end;
	// if it is the last thread then set the end of the thread's responsibility to the last item
	if (thread_ID == num_producers-1) {
		end = num_items;
	//else set it to the next threads start
	} else {
		end = (thread_ID + 1) * multiplier;
	}

	// for each item in the threads scope of responsibility
	for (int i = start; i < end; ++i) {
		// decrement producer semaphore to show there is one less available space for items
		// or block if buffer is full (semaphore is 0)
		sem_wait(&producer_semaphore);

		// sleep for random amount of time between 300 and 700
		useconds_t usecs = (rand() % 401 + 300);
		usleep(usecs);

		// create a new item
		Item *item;
		item = new Item;
		// error check creation
		if (item == NULL) {
			fprintf(stderr, "Failed to allocate memory\n");
			exit(1);
		}

		// set ID to item number based on i
		item->ID = i;
		// set sleep time to random number between 200 and 900
		useconds_t usec = (rand() % 701 + 200);
		item->sleep_time = usec;

		// lock mutex around the buffer
		pthread_mutex_lock(&cant_touch_this);
		// push back item into buffer
		buffer.push_back(item);	
		// release mutex
		pthread_mutex_unlock(&cant_touch_this);

		// increment consumer semaphore to show there are item(s) in the buffer to consume
		sem_post(&consumer_semaphore);
	}

	return NULL;
} 

void *Consume(void *arg) {

	while(1) {
		// decrement consumer semaphore to show there is one less item in the buffer to consume
		// or wait if there are no items to consume
		sem_wait(&consumer_semaphore);

		// set thread ID to the argument passed to the thread
		int thread_ID = (intptr_t) arg;
		Item *item;

		// lock mutex before altering buffer
		pthread_mutex_lock(&cant_touch_this);

		// grab the frist item in the buffer
		item = buffer.front();
		// sleep for the time determined by the item
		usleep(item->sleep_time);
		// print consumer number and item number
		printf("Consumer Thread #%i : Consuming Item #:%i\n", thread_ID, item->ID);
		// flush stdout to ensure proper printing
		fflush(stdout);
		// erase the item from the buffer
		buffer.erase(buffer.begin());
		// free the memory taken up by the item
		delete(item);

		// join the thread to ensure it's completion and no hanging threads
	 	pthread_join(consumer_threads[thread_ID], NULL);
	 	// release the mutex
		pthread_mutex_unlock(&cant_touch_this);
		// increment the producer semaphore to show that there is more available space for items to produce
		sem_post(&producer_semaphore);
	}

	return NULL;

}

int main(int argc, char **argv) {

	// seed time to ensure random numbers
	srand(time(0));

	// check for the correct number of command line arguments
	if (argc != 5) {
		fprintf(stderr, "usage: ./hw1 num_prod num_cons buf_size num_items\n");
		exit(1);
	} 

	// check that each argument is greater than 0
	for (int i = 0; i < argc; ++i) {
		if (atoi(argv[i]) <= 0) {
			if (i == 1) {
				fprintf(stderr, "num_prod must be greater than 0\n");
				exit(1);
			}
			if (i == 2) {
				fprintf(stderr, "num_cons must be greater than 0\n");
				exit(1);
			}
			if (i == 3) {
				fprintf(stderr, "buf_size must be greater than 0\n");
				exit(1);
			}
			if (i == 4) {
				fprintf(stderr, "num_items must be greater than 0\n");
				exit(1);
			}
		}
	}

	// set variables to given arguments
	num_producers = atoi(argv[1]);
	num_consumers = atoi(argv[2]);
	buffer_size = atoi(argv[3]);
	num_items = atoi(argv[4]);

	// initialize producer semaphore to buffer size to show that there are that many items available to produce
	// each time an item is produced, the producer semaphore will decrement
	// when the buffer is full, it will be 0 and the semaphore will wait
	if (sem_init(&producer_semaphore, 0, buffer_size) == -1) {
	    perror("Could not initialize producer semaphore");
	}

	// initialize consumer semaphore to 0 to show that there are no items available to consume
	// each time an item is consumed, the consumer semaphore will decrement
	// when the buffer is empty, it will be 0 and the semaphore will wait
	if (sem_init(&consumer_semaphore, 0, 0) == -1) {
        perror("Could not initialize consumer semaphore");
    }
	
	// allocate memory for the producer threads based on number of producers provided by user
	producer_threads = (pthread_t*)malloc(sizeof(pthread_t)*num_producers);
	// error check the allocation of memory
	if (producer_threads == NULL) {
		fprintf(stderr, "Failed to allocate memory\n");
		exit(1);
	}

	// allocate memory for the consumer threads based on number of consumers provided by user
	consumer_threads = (pthread_t*)malloc(sizeof(pthread_t)*num_consumers);
	// error check the allocation of memory
	if (consumer_threads == NULL) {
		fprintf(stderr, "Failed to allocate memory\n");
		exit(1);
	}

	// create each consumer thread
	// create consumer threads first so they can be there waiting to consume
	for (int i = 0; i < num_consumers; ++i) {
		if (pthread_create(&consumer_threads[i], NULL, Consume, (void *) i) != 0) {
			fprintf(stderr, "Error creating consumer thread %i\n", i);
		}
	}

	// create each producer thread
	for (int i = 0; i < num_producers; ++i) {
		if (pthread_create(&producer_threads[i], NULL, Produce, (void *) i) != 0) {
			fprintf(stderr, "Error creating producer thread %i\n", i);
		}
	}

	// join each producer thread
	for (int i = 0; i < num_producers; ++i) {
		void *status;
		int t = pthread_join(producer_threads[i], &status);
		if (t != 0) {
			fprintf(stderr, "Error in thread join: %i\n", t);
		}
	}

	// alert user that they are done producing
	fprintf(stderr, "DONE PRODUCING\n");

	// wait for SIGINT
	while(1) {

	}
}