#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/errno.h>
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
    pthread_cond_t to_read;
    pthread_cond_t to_write;
    pthread_mutex_t mutex;
};

RWLock *rwlock_new() {
    int err;
    RWLock *r = malloc(sizeof(RWLock));
    if (!r) {
        syserr("", EMEMORY);
    }

    if ((err = pthread_mutex_init(&r->mutex, 0)) != 0) syserr("", err);
    if (pthread_cond_init(&r->to_write, 0) != 0) syserr("", err);
    if (pthread_cond_init(&r->to_read, 0) != 0) syserr("", err);

    r->wait_wr = 0;
    r->wait_rd = 0;
    r->work_wr = 0;
    r->work_rd = 0;
    return r;
}

// Acquire read lock.
int rwlock_rd_lock(RWLock *lock) {
    int err;
    if ((err = pthread_mutex_lock(&lock->mutex)) != 0) syserr("", err);

    ++lock->wait_rd;
    if (lock->wait_wr > 0 || lock->work_wr > 0) { // TODO: check
        // reader should wait
        do {
            if ((err = pthread_cond_wait(&lock->to_read, &lock->mutex)) != 0)
                syserr("", err);
        } while (lock->work_wr > 0);
    }
    --lock->wait_rd;

    ++lock->work_rd;
    return pthread_mutex_unlock(&lock->mutex);
}

// Release read lock.
int rwlock_rd_unlock(RWLock *lock) {
    int err;
    if ((err = pthread_mutex_lock(&lock->mutex)) != 0) syserr("", err);

    --lock->work_rd;
    if (lock->work_rd == 0 && lock->wait_wr > 0) {
        if ((err = pthread_cond_signal(&lock->to_write)) != 0)
            syserr("", err);
    } else if (lock->work_rd == 0 && lock->wait_rd > 0) {
        if ((err = pthread_cond_broadcast(&lock->to_read)) != 0)
            syserr("", err);
    }
    return pthread_mutex_unlock(&lock->mutex);
}

// Acquire write lock.
int rwlock_wr_lock(RWLock *lock) {
    int err;
    if ((err = pthread_mutex_lock(&lock->mutex)) != 0) syserr("", err);

    ++lock->wait_wr;
    while (lock->work_rd > 0 || lock->work_wr > 0) {
        // writer should wait
        if ((err = pthread_cond_wait(&lock->to_write, &lock->mutex)) != 0)
            syserr("", err);
    }
    --lock->wait_wr;
    ++lock->work_wr;
    return pthread_mutex_unlock(&lock->mutex);
}

// Release write lock.
int rwlock_wr_unlock(RWLock *lock) {
    int err;
    if ((err = pthread_mutex_lock(&lock->mutex)) != 0) syserr("", err);

    --lock->work_wr;
    // always at most only one writer working
    if (lock->wait_rd > 0) {
        if ((err = pthread_cond_broadcast(&lock->to_read)) != 0)
            syserr("", err);
    } else if (lock->wait_wr > 0) {
        if ((err = pthread_cond_signal(&lock->to_write)) != 0)
            syserr("", err);
    }
    return pthread_mutex_unlock(&lock->mutex);
}

int rwlock_free(RWLock *lock) {
    int err;

    if ((err = pthread_cond_destroy(&lock->to_read)) != 0) syserr("", err);
    if ((err = pthread_cond_destroy(&lock->to_write)) != 0) syserr("", err);
    if ((err = pthread_mutex_destroy(&lock->mutex)) != 0) syserr("", err);
    free(lock);
    return 0;
}