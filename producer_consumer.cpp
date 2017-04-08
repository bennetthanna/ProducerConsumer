#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <vector>

struct Item {
	int ID;
	int sleep_time;
};

pthread_mutex_t cant_touch_this = PTHREAD_MUTEX_INITIALIZER;
std::vector<Item*>buffer;
sem_t producer_semaphore;
sem_t consumer_semaphore;
int num_producers;
int num_consumers;
int buffer_size;
int num_items;
int producer_sem_value;
int consumer_sem_value;

void get_producer_sem_value() {
	sem_getvalue(&producer_semaphore, &producer_sem_value);
	printf("Producer semaphore value = %i\n", producer_sem_value);
}

void get_consumer_sem_value() {
	sem_getvalue(&consumer_semaphore, &consumer_sem_value);
    printf("Consumer semaphore value = %i\n", consumer_sem_value);
}

void *Produce(void *arg) {

	// producer semaphore will post and push to buffer vector as they produce new items
	// producer semaphore will wait if buffer == buffer_size
	// inside producer, post consumer semaphore (shows consumer that there are items to consume)

	int thread_ID = (intptr_t) arg;

	//divide the items based on the number of threads
	int multiplier = num_items/num_producers;
	//set the start of each thread's responsibility to its ID * multiplier
	int start = thread_ID * multiplier;
	int end;
	
	//if it is the last thread then set the end of the thread's responsibility to the last item
	if (thread_ID == num_producers-1) {
		end = num_items;
	//else set it to the next threads start
	} else {
		end = (thread_ID + 1) * multiplier;
	}

	printf("Thread #%i start = %i\n", thread_ID, start);
	printf("Thread #%i end = %i\n", thread_ID,end);

	for (int i = start; i < end; ++i) {
		sem_wait(&producer_semaphore);
		usleep(rand() % 701 + 300);
		Item *item;
		item = new Item;
		if (item == NULL) {
			fprintf(stderr, "Failed to allocate memory\n");
			exit(1);
		}

		item->ID = i;
		item->sleep_time = rand() % 701 + 200;
		pthread_mutex_lock(&cant_touch_this);
		buffer.push_back(item);
		printf("Producer Thread #%i : Item #%i : Sleep Time: %i\n", thread_ID, item->ID, item->sleep_time);
		pthread_mutex_unlock(&cant_touch_this);
		sem_post(&consumer_semaphore);
		get_producer_sem_value();
		get_consumer_sem_value();
	}

	return NULL;
} 

void *Consume(void *arg) {
	while(1) {
		sem_wait(&consumer_semaphore);
		int thread_ID = (intptr_t) arg;
		Item *item;
		item = buffer.front();
		usleep(item->sleep_time);
		printf("Consumer Thread #%i : Item #:%i : Sleep Time: %i\n", thread_ID, item->ID, item->sleep_time);
		pthread_mutex_lock(&cant_touch_this);
		buffer.erase(buffer.begin());
		pthread_mutex_unlock(&cant_touch_this);
		sem_post(&producer_semaphore);
		// consumer semaphore will wait while buffer > 0 (decrement/consume items in buffer)
		get_producer_sem_value();
		get_consumer_sem_value();
	}
	return NULL;

}

int main(int argc, char **argv) {
	if (argc != 5) {
		fprintf(stderr, "usage: ./hw1 num_prod num_cons buf_size num_items\n");
		exit(1);
	} 

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

	num_producers = atoi(argv[1]);
	num_consumers = atoi(argv[2]);
	buffer_size = atoi(argv[3]);
	num_items = atoi(argv[4]);

	if (sem_init(&producer_semaphore, 0, buffer_size) == -1) {
	    perror("sem_init");
	} else {
		sem_getvalue(&producer_semaphore, &producer_sem_value);
		printf("Producer semaphore value = %i\n", producer_sem_value);
	}

	if (sem_init(&consumer_semaphore, 0, 0) == -1) {
        perror("sem_init");
    } else {
    	sem_getvalue(&consumer_semaphore, &consumer_sem_value);
    	printf("Consumer semaphore value = %i\n", consumer_sem_value);
    }

	pthread_t *producer_threads;
	pthread_t *consumer_threads;
	
	producer_threads = (pthread_t*)malloc(sizeof(pthread_t)*num_producers);
	if (producer_threads == NULL) {
		fprintf(stderr, "Failed to allocate memory\n");
		exit(1);
	}
	consumer_threads = (pthread_t*)malloc(sizeof(pthread_t)*num_consumers);
	if (consumer_threads == NULL) {
		fprintf(stderr, "Failed to allocate memory\n");
		exit(1);
	}

	for (int i = 0; i < num_consumers; ++i) {
		pthread_create(&consumer_threads[i], NULL, Consume, (void *) i);
	}

	for (int i = 0; i < num_producers; ++i) {
		pthread_create(&producer_threads[i], NULL, Produce, (void *) i);
	}

	for (int i = 0; i < num_producers; ++i) {
		pthread_join(producer_threads[i], NULL);
	}	

	fprintf(stderr, "DONE PRODUCING!\n");

	for (int i = 0; i < num_consumers; ++i) {
		pthread_join(consumer_threads[i], NULL);
	}
}