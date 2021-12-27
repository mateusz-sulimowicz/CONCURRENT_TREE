#pragma once

typedef struct RWLock RWLock;

RWLock *rwlock_new();

void rwlock_rd_lock(RWLock *lock);

void rwlock_rd_unlock(RWLock *lock);

void rwlock_wr_unlock(RWLock *lock);

void rwlock_wr_lock(RWLock *lock);

void rwlock_free(RWLock *lock);