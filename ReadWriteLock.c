#include <stdlib.h>
#include <pthread.h>
#include <sys/errno.h>
#include "ReadWriteLock.h"

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
    RWLock *r = malloc(sizeof(RWLock));

    if (!r) {
        return NULL;
    }

    if (pthread_mutex_init(&r->mutex, 0) != 0) return NULL;

    if (pthread_cond_init(&r->to_write, 0) != 0) {
        pthread_mutex_destroy(&r->mutex);
        return NULL;
    }

    if (pthread_cond_init(&r->to_read, 0) != 0) {
        pthread_mutex_destroy(&r->mutex);
        pthread_cond_destroy(&r->to_read);
        return NULL;
    }
    return r;
}

int rwlock_rd_lock(RWLock *lock) {
    int err;
    if ((err = pthread_mutex_lock(&lock->mutex)) != 0) return err;

    ++lock->wait_rd;
    if (lock->wait_wr > 0 || lock->work_wr > 0) { // TODO: check
        // reader should wait
        do {
            if ((err = pthread_cond_wait(&lock->to_read, &lock->mutex)) != 0) return err;
        } while (lock->work_wr > 0);
    }
    --lock->wait_rd;

    ++lock->work_rd;
    return pthread_mutex_unlock(&lock->mutex);
}

int rwlock_rd_unlock(RWLock *lock) {
    int err;
    if ((err = pthread_mutex_lock(&lock->mutex)) != 0) return err;

    --lock->work_rd;
    if (lock->work_rd == 0 && lock->wait_wr > 0) {
        if ((err = pthread_cond_signal(&lock->to_write)) != 0) return err;
    } else if (lock->work_rd == 0 && lock->wait_rd > 0) {
        if ((err = pthread_cond_broadcast(&lock->to_read)) != 0) return err;
    }

    return pthread_mutex_unlock(&lock->mutex);
}

int rwlock_wr_lock(RWLock *lock) {
    int err;
    if ((err = pthread_mutex_lock(&lock->mutex)) != 0) return err;

    ++lock->wait_wr;
    while (lock->work_rd > 0 || lock->work_wr > 0) {
        // writer should wait
        if ((err = pthread_cond_wait(&lock->to_write, &lock->mutex)) != 0) return err;
    }
    --lock->wait_wr;

    ++lock->work_wr;
    return pthread_mutex_unlock(&lock->mutex);
}

int rwlock_wr_unlock(RWLock *lock) {
    int err;
    if ((err = pthread_mutex_lock(&lock->mutex)) != 0) return err;

    --lock->work_wr;
    // always at most only one writer working
    if (lock->wait_rd > 0) {
        if ((err = pthread_cond_broadcast(&lock->to_read)) != 0) return err;
    } else if (lock->wait_wr > 0) {
        if ((err = pthread_cond_signal(&lock->to_write)) != 0) return err;
    }

    return pthread_mutex_unlock(&lock->mutex);
}

int rwlock_free(RWLock *lock) {
    int err;

    if ((err = pthread_cond_destroy(&lock->to_read)) != 0) return err;
    if ((err = pthread_cond_destroy(&lock->to_write)) != 0) return err;
    if ((err = pthread_mutex_destroy(&lock->mutex)) != 0) return err;
    free(lock);

    return 0;
}