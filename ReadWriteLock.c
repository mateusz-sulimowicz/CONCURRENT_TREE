#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "ReadWriteLock.h"
#include "err.h"

/**
 * Read-write lock implementation.
 */
struct RWLock {
    size_t wait_wr;
    size_t wait_rd;
    size_t work_wr;
    size_t work_rd;
    size_t cascade_counter;
    pthread_cond_t to_read;
    pthread_cond_t to_write;
    pthread_mutex_t mutex;
};

RWLock *rwlock_new() {
    RWLock *r = malloc(sizeof(RWLock));
    if (!r) syserr("memory alloc failed!");

    pthread_mutex_init(&r->mutex, 0);
    pthread_cond_init(&r->to_write, 0);
    pthread_cond_init(&r->to_read, 0);

    r->wait_wr = 0;
    r->wait_rd = 0;
    r->work_wr = 0;
    r->work_rd = 0;
    r->cascade_counter = 0;
    return r;
}

// Acquire read lock.
int rwlock_rd_lock(RWLock *lock) {
    pthread_mutex_lock(&lock->mutex);
    ++lock->wait_rd;
    if (lock->cascade_counter > 0 || lock->wait_wr > 0 || lock->work_wr > 0) {
        // reader should wait
        do {
            pthread_cond_wait(&lock->to_read, &lock->mutex);
        } while (lock->work_wr > 0 || lock->cascade_counter == 0);
        --lock->cascade_counter;
    }

    --lock->wait_rd;
    ++lock->work_rd;
    pthread_mutex_unlock(&lock->mutex);
    return 0;
}

// Release read lock.
int rwlock_rd_unlock(RWLock *lock) {
    pthread_mutex_lock(&lock->mutex);
    --lock->work_rd;
    if (lock->work_rd == 0 && lock->wait_wr > 0) {
        pthread_cond_signal(&lock->to_write);
    } else if (lock->work_rd == 0 && lock->wait_rd > 0) {
        lock->cascade_counter = lock->wait_rd;
        pthread_cond_broadcast(&lock->to_read);
    }
    pthread_mutex_unlock(&lock->mutex);
    return 0;
}

// Acquire write lock.
int rwlock_wr_lock(RWLock *lock) {
    pthread_mutex_lock(&lock->mutex);
    ++lock->wait_wr;
    while (lock->work_rd > 0 || lock->work_wr > 0 || lock->cascade_counter > 0) {
        // writer should wait
        pthread_cond_wait(&lock->to_write, &lock->mutex);
    }
    --lock->wait_wr;
    ++lock->work_wr;
    pthread_mutex_unlock(&lock->mutex);
    return 0;
}

// Release write lock.
int rwlock_wr_unlock(RWLock *lock) {
    pthread_mutex_lock(&lock->mutex);
    --lock->work_wr;
    // always at most only one writer working
    if (lock->wait_rd > 0) {
        lock->cascade_counter = lock->wait_rd;
        pthread_cond_broadcast(&lock->to_read);
    } else if (lock->wait_wr > 0) {
        pthread_cond_signal(&lock->to_write);
    }
    pthread_mutex_unlock(&lock->mutex);
    return 0;
}

int rwlock_free(RWLock *lock) {
    pthread_cond_destroy(&lock->to_read);
    pthread_cond_destroy(&lock->to_write);
    pthread_mutex_destroy(&lock->mutex);
    free(lock);
    return 0;
}