/*
 * Mutex.h
 *
 *  Created on: 17 апр. 2015
 *      Author: vberegovoy
 */

#ifndef MUTEX_H_
#define MUTEX_H_

#include <pthread.h>

class Mutex {
public:
	Mutex() {
		pthread_mutex_init(&mutex, NULL);
	}
	virtual ~Mutex() {
		pthread_mutex_destroy(&mutex);
	}

	void lock() {
		pthread_mutex_lock(&mutex);
	}
	void unlock() {
		pthread_mutex_unlock(&mutex);
	}
private:
	pthread_mutex_t mutex;
};


class Autolock {
public:
	Autolock(Mutex& mutex) : mutex(mutex) {
		mutex.lock();
	}
	virtual ~Autolock() {
		mutex.unlock();
	}

private:
	Mutex mutex;
};

#endif /* MUTEX_H_ */
