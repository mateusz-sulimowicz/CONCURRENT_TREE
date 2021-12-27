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

    // TODO: check if succeded
    d->subdirs = hmap_new();
    d->lock = rwlock_new();
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
    Directory *subdir = dir_new();
    hmap_insert(d->subdirs, subdir_name, subdir);
    return 0; // TODO: check if succedded.
}

// -----------------------

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

// finds and rd-locks the found directory
Directory *tree_find_rd(Tree *tree, const char *path) {
    assert(tree != NULL && is_path_valid(path));

    if (strcmp(path, "/") == 0) {
        // root dir
        rwlock_rd_lock(tree->root->lock);
        return tree->root;
    }

    char component[MAX_FOLDER_NAME_LENGTH + 1];
    const char *subpath = path;

    rwlock_rd_lock(tree->root->lock);
    Directory *current = tree->root;
    Directory *next = NULL;

    while ((subpath = split_path(subpath, component))) {
        next = hmap_get(current->subdirs, component);

        if (!next) {
            // subdirectory of current dir not found.
            rwlock_rd_unlock(current->lock);
            return NULL;
        }

        rwlock_rd_lock(next->lock);
        rwlock_rd_unlock(current->lock);
        current = next;
    }

    return current;
}

// finds and wr-locks the found directory
Directory *tree_find_wr(Tree *tree, const char *path) {
    assert(tree != NULL && is_path_valid(path));
    Directory *dir = NULL;

    if (strcmp(path, "/") == 0) {
        // root dir
        rwlock_wr_lock(tree->root->lock);
        return tree->root;
    }

    char component[MAX_FOLDER_NAME_LENGTH + 1];
    char *parent_path = make_path_to_parent(path, component);

    Directory *parent = tree_find_rd(tree, parent_path);

    if (!parent) {
        free(parent_path);
        return NULL;
    }

    dir = hmap_get(parent->subdirs, component);

    if (!dir) {
        // directory not found
        rwlock_rd_unlock(parent->lock);
        return NULL;
    }

    // TODO: check. If the lock/unlock were in opposite order,
    // thread may try to lock a rwlock of removed directory.
    rwlock_wr_lock(dir->lock);
    rwlock_rd_unlock(parent->lock);

    return dir;
}

int tree_create(Tree *tree, const char *path) {
    assert(tree != NULL);

    if (!is_path_valid(path)) {
        return EINVAL;
    }

    char *component = malloc(sizeof(char) * (MAX_FOLDER_NAME_LENGTH + 1));
    char *parent_path = make_path_to_parent(path, component);
    Directory *parent = tree_find_wr(tree, parent_path);

    if (!parent) {
        free(component);
        free(parent_path);
        return ENOENT;
    }

    if (hmap_get(parent->subdirs, component)) {
        rwlock_wr_unlock(parent->lock);
        free(component);
        free(parent_path);
        return EEXIST;
    }

    int res = dir_create(parent, component);
    rwlock_wr_unlock(parent->lock);
    free(parent_path);
    return res;
}

char *tree_list(Tree *tree, const char *path) {
    assert(tree != NULL);

    if (!is_path_valid(path)) {
        return NULL;
    }

    Directory *d = tree_find_rd(tree, path);

    if (!d) {
        return NULL;
    }

    char *res = dir_list(d);
    rwlock_rd_unlock(d->lock);
    return res;
}

int tree_remove(Tree *tree, const char *path) {
    assert(tree != NULL);

    if (strcmp(path, "/") == 0) {
        return EBUSY;
    }

    if (!is_path_valid(path)) {
        return EINVAL;
    }

    char component[(MAX_FOLDER_NAME_LENGTH + 1)];
    char *parent_path = make_path_to_parent(path, component);
    Directory *parent = tree_find_wr(tree, parent_path);

    if (!parent) {
        free(parent_path);
        return ENOENT;
    }

    if (!hmap_get(parent->subdirs, component)) {
        rwlock_wr_unlock(parent->lock);
        free(parent_path);
        return ENOENT;
    }

    Directory *dir = hmap_get(parent->subdirs, component);

    rwlock_wr_lock(dir->lock);

    if (hmap_size(dir->subdirs) > 0) {
        return ENOTEMPTY;
    }

    // TODO: what to do with waiting threads when directory is removed?

    hmap_remove(parent->subdirs, component);
    dir_free(dir);
    return 0;
}

/*int tree_move(Tree *tree, const char *source, const char *target) {
    assert(tree != NULL);

    if (strcmp(source, "/") == 0) {
        return EBUSY;
    }

    if (!is_path_valid(source) || !is_path_valid(target)) {
        return EINVAL;
    }

    Directory *source_dir = tree_find_wr()


}*/

void tree_free(Tree *tree) {
    assert(tree != NULL);
    dir_free(tree->root);
    free(tree);
}








