#pragma once

typedef struct RWLock RWLock;

RWLock *rwlock_new();

int rwlock_rd_lock(RWLock *lock);

int rwlock_rd_unlock(RWLock *lock);

int rwlock_wr_unlock(RWLock *lock);

int rwlock_wr_lock(RWLock *lock);

int rwlock_rm_lock(RWLock *lock);

int rwlock_rm_unlock(RWLock *lock);

int rwlock_free(RWLock *lock);