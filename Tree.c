#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include "path_utils.h"
#include "HashMap.h"
#include "ReadWriteLock.h"
#include "Tree.h"

// ---- single directory of the tree ----

typedef struct Directory Directory;

struct Directory {
    RWLock *lock;
    HashMap *subdirs;
};

Directory *dir_new() {
    Directory *d = malloc(sizeof(Directory));

    if (d == NULL) {
        return NULL;
    }

    if (!(d->subdirs = hmap_new())) {
        free(d);
        return NULL;
    }

    if (!(d->lock = rwlock_new())) {
        hmap_free(d->subdirs);
        free(d);
        return NULL;
    }

    return d;
}

void dir_free(Directory *d) {
    if (d == NULL) {
        return;
    }

    const char *subdir_name;
    Directory *subdir;
    HashMapIterator it = hmap_iterator(d->subdirs);
    while (hmap_next(d->subdirs, &it, &subdir_name, (void **) &subdir)) {
        dir_free(subdir);
    }

    rwlock_free(d->lock);
    hmap_free(d->subdirs);
    free(d);
}


char *dir_list(Directory *d) {
    assert(d != NULL);
    return make_map_contents_string(d->subdirs);
}

int dir_create(Directory *d, char *subdir_name) {
    assert(d != NULL && subdir_name != NULL);
    Directory *subdir;

    if (!(subdir = dir_new())) {
        return -1;
    }

    if (!(hmap_insert(d->subdirs, subdir_name, subdir))) {
        dir_free(subdir);
        return -1;
    }

    return 0;
}

// ----------------------------------------------

struct Tree {
    Directory *root;
};

Tree *tree_new() {
    Tree *t = malloc(sizeof(Tree));
    if (t == NULL) {
        return NULL;
    }

    t->root = dir_new();
    return t;
}

// Finds and rd-locks the found directory.
// On failure, returns NULL.
int tree_find_rd_lock(Directory **d, Tree *tree, const char *path) {
    assert(tree != NULL && is_path_valid(path));
    int err;

    if (strcmp(path, "/") == 0) {
        // root dir
        *d = tree->root;
        return rwlock_rd_lock(tree->root->lock);
    }

    char subdir_name[MAX_FOLDER_NAME_LENGTH + 1];
    const char *subpath = path;

    if ((err = rwlock_rd_lock(tree->root->lock)) != 0) return err;
    Directory *current = tree->root;
    Directory *next = NULL;

    while ((subpath = split_path(subpath, subdir_name))) {
        next = hmap_get(current->subdirs, subdir_name);

        if (!next) {
            // subdirectory of current dir not found.
            rwlock_rd_unlock(current->lock);
            return ENOENT;
        }

        // todo check
        rwlock_rd_lock(next->lock);
        rwlock_rd_unlock(current->lock);
        current = next;
    }

    *d = current;
    return 0;
}

// Finds and wr-locks the found directory.
// On failure, returns NULL.
int tree_find_wr_lock(Directory **d, Tree *tree, const char *path) {
    assert(tree != NULL && is_path_valid(path));
    Directory *dir = NULL;
    int err;

    if (strcmp(path, "/") == 0) {
        // Root directory
        *d = tree->root;
        return rwlock_wr_lock(tree->root->lock);
    }

    char subdir_name[MAX_FOLDER_NAME_LENGTH + 1];
    char *parent_path = make_path_to_parent(path, subdir_name);

    Directory *parent = NULL;
    if ((err = tree_find_rd_lock(&parent, tree, parent_path) != 0)) {
        free(parent_path);
        return err;
    }

    if (!(dir = hmap_get(parent->subdirs, subdir_name))) {
        // Directory not found.
        rwlock_rd_unlock(parent->lock);
        free(parent_path);
        return ENOENT;
    }

    // TODO: check. If the lock/unlock were in opposite order,
    // thread may try to lock a rwlock of removed directory.
    if ((err = rwlock_wr_lock(dir->lock)) != 0) return err;
    if ((err = rwlock_rd_unlock(parent->lock)) != 0) return err;
    free(parent_path);

    *d = dir;
    return 0;
}

int tree_create(Tree *tree, const char *path) {
    assert(tree != NULL);
    int err;

    if (!is_path_valid(path)) return EINVAL;

    char subdir_name[MAX_FOLDER_NAME_LENGTH + 1];
    char *parent_path = make_path_to_parent(path, subdir_name);
    Directory *parent = NULL;
    if ((err = tree_find_wr_lock(&parent, tree, parent_path)) != 0) {
        free(parent_path);
        return err;
    }

    if (hmap_get(parent->subdirs, subdir_name)) {
        // subdir already exists
        rwlock_wr_unlock(parent->lock);
        free(parent_path);
        return EEXIST;
    }

    err = dir_create(parent, subdir_name);
    err = err == 0 ? rwlock_wr_unlock(parent->lock) : err;
    free(parent_path);

    return err;
}

char *tree_list(Tree *tree, const char *path) {
    assert(tree != NULL);

    if (!is_path_valid(path)) return NULL;

    Directory *d = NULL;
    if ((tree_find_rd_lock(&d, tree, path)) != 0) return NULL;

    char *res = dir_list(d);
    rwlock_rd_unlock(d->lock);
    return res;
}

int tree_remove(Tree *tree, const char *path) {
    assert(tree != NULL);
    int err;

    if (!is_path_valid(path)) {
        return EINVAL;
    }

    if (strcmp(path, "/") == 0) {
        return EBUSY;
    }

    char subdir_name[MAX_FOLDER_NAME_LENGTH + 1];
    char *parent_path = make_path_to_parent(path, subdir_name);
    Directory *parent = NULL;
    if ((err = tree_find_wr_lock(&parent, tree, parent_path)) != 0) {
        free(parent_path);
        return err;
    }

    Directory *dir = NULL;
    if (!(dir = hmap_get(parent->subdirs, subdir_name))) {
        rwlock_wr_unlock(parent->lock);
        free(parent_path);
        return ENOENT;
    }

    if ((err = rwlock_wr_lock(dir->lock)) != 0) {
        rwlock_wr_unlock(parent->lock);
        free(parent_path);
        return err;
    };

    if (hmap_size(dir->subdirs) > 0) {
        // directory is not empty
        rwlock_wr_unlock(parent->lock);
        rwlock_wr_unlock(dir->lock);
        return ENOTEMPTY;
    }

    hmap_remove(parent->subdirs, subdir_name);
    err = rwlock_wr_unlock(parent->lock);
    dir_free(dir);

    // TODO: nie dziala wspolbieznie.
    return err;
}

int tree_move(Tree *tree, const char *source, const char *target) {
    assert(tree != NULL);
    int err;

    if (!is_path_valid(source) || !is_path_valid(target)) {
        return EINVAL;
    }

    if (strcmp(source, "/") == 0) {
        return EBUSY;
    }

    char source_dir_name[MAX_FOLDER_NAME_LENGTH + 1];
    char *source_parent_path = make_path_to_parent(source, source_dir_name);
    Directory *source_parent_dir = NULL;

    char target_dir_name[MAX_FOLDER_NAME_LENGTH + 1];
    char *target_parent_path = make_path_to_parent(source, target_dir_name);
    Directory *target_parent_dir = NULL;

    // Lock the directory of lexicographically
    // smaller path first (to prevent deadlocks).
    if (strcmp(source, target) < 0) {
        if ((err = tree_find_wr_lock(&source_parent_dir, tree,
                                     source_parent_path)) != 0) {
            free(source_parent_path);
            free(target_parent_path);
            return err;
        }

        if ((err = tree_find_wr_lock(&target_parent_dir, tree,
                                     target_parent_path)) != 0) {
            rwlock_wr_unlock(source_parent_dir->lock);
            free(source_parent_path);
            free(target_parent_path);
            return err;
        }
    } else {
        if ((err = tree_find_wr_lock(&target_parent_dir, tree,
                                     target_parent_path)) != 0) {
            free(source_parent_path);
            free(target_parent_path);
            return err;
        }

        if ((err = tree_find_wr_lock(&source_parent_dir, tree,
                                     source_parent_path)) != 0) {
            rwlock_wr_unlock(target_parent_dir->lock);
            free(source_parent_path);
            free(target_parent_path);
            return err;
        }
    }

    Directory *to_move = NULL;
    if (!(to_move = hmap_get(source_parent_dir->subdirs, source_dir_name))) {
        // Source dir does not exist.
        rwlock_wr_unlock(source_parent_dir->lock);
        rwlock_wr_unlock(target_parent_dir->lock);
        free(source_parent_path);
        free(target_parent_path);
        return ENOENT;
    }

    if (hmap_get(target_parent_dir->subdirs, target_dir_name)) {
        // Target dir already exists.
        rwlock_wr_unlock(source_parent_dir->lock);
        rwlock_wr_unlock(target_parent_dir->lock);
        free(source_parent_path);
        free(target_parent_path);
        return EEXIST;
    }

    hmap_remove(source_parent_dir->subdirs, source_dir_name);
    hmap_insert(target_parent_dir->subdirs, source_dir_name, to_move);

    err = rwlock_wr_unlock(source_parent_dir->lock);
    err = err == 0 ? rwlock_wr_unlock(target_parent_dir->lock) : err;
    free(source_parent_path);
    free(target_parent_path);
    return err;
}

void tree_free(Tree *tree) {
    assert(tree != NULL);
    dir_free(tree->root);
    free(tree);
}








