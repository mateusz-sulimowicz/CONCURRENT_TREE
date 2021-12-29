#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include "path_utils.h"
#include "HashMap.h"
#include "ReadWriteLock.h"
#include "Tree.h"
#include "err.h"

// ---- single directory of the tree ----

typedef struct Directory Directory;

struct Directory {
    RWLock *lock;
    RWLock *nondestroy;
    HashMap *subdirs;
    Directory *parent;
};

Directory *dir_new(Directory *parent) {
    Directory *d = malloc(sizeof(Directory));
    if (!d) return NULL;

    d->subdirs = hmap_new();
    if (!d->subdirs) {
        free(d);
        return NULL;
    }

    d->lock = rwlock_new();
    if (!d->lock) {
        hmap_free(d->subdirs);
        free(d);
        return NULL;
    }

    d->nondestroy = rwlock_new();
    if (!d->nondestroy) {
        hmap_free(d->subdirs);
        rwlock_free(d->lock);
        free(d);
        return NULL;
    }

    d->parent = parent;
    return d;
}

void dir_free(Directory *d) {
    assert(d);

    const char *subdir_name;
    Directory *subdir;
    HashMapIterator it = hmap_iterator(d->subdirs);
    while (hmap_next(d->subdirs, &it, &subdir_name, (void **) &subdir)) {
        dir_free(subdir);
    }

    rwlock_free(d->lock);
    rwlock_free(d->nondestroy);
    hmap_free(d->subdirs);
    free(d);
}

char *dir_list(Directory *d) {
    assert(d != NULL);
    return make_map_contents_string(d->subdirs);
}

int dir_create(Directory *d, char *subdir_name) {
    assert(d && subdir_name);
    Directory *subdir = NULL;

    subdir = dir_new(d);
    if (!subdir) return -1;

    if (!(hmap_insert(d->subdirs, subdir_name, subdir))) {
        dir_free(subdir);
        return -1;
    }
    return 0;
}

int dir_find(Directory **out, Directory *root, const char *path) {
    assert(root != NULL && is_path_valid(path));
    int err;

    char child_name[MAX_FOLDER_NAME_LENGTH + 1];
    const char *subpath = path;

    Directory *parent = root->parent;

    rwlock_rd_lock(parent->nondestroy);
    rwlock_rd_lock(parent->lock);

    Directory *child = root;
    // TODO
    while ((subpath = split_path(subpath, child_name))) {
        rwlock_rd_unlock(parent->lock);
        rwlock_rd_lock(child->nondestroy);
        rwlock_rd_unlock(parent->nondestroy);

        parent = child;
        rwlock_rd_lock(parent->lock);

        if (!(child = hmap_get(parent->subdirs, child_name))) {
            // subdir not found.
            rwlock_rd_unlock(parent->lock);
            rwlock_rd_unlock(parent->nondestroy);
            return ENOENT;
        }
    }
    rwlock_rd_unlock(parent->lock);

    *out = child;
    return 0;
}

// ----------------------------------------------

struct Tree {
    Directory *root;
};

Tree *tree_new() {
    Tree *t = malloc(sizeof(Tree));
    if (!t) return NULL;

    Directory *dummy = dir_new(NULL);
    if (!dummy) {
        free(t);
        return NULL;
    }

    t->root = dir_new(dummy);
    return t;
}

// Finds directory  and read-locks its parent.
int tree_find(Directory **out, Tree *tree, const char *path) {
    assert(tree != NULL && is_path_valid(path));
    return dir_find(out, tree->root, path);
}

int tree_create(Tree *tree, const char *path) {
    assert(tree != NULL);
    int err;
    printf("jestem tu!");

    if (!is_path_valid(path)) return EINVAL;

    char subdir_name[MAX_FOLDER_NAME_LENGTH + 1];
    char *parent_path = make_path_to_parent(path, subdir_name);
    Directory *parent = NULL;

    err = tree_find(&parent, tree, parent_path);
    if (err) {
        free(parent_path);
        return err;
    }

    rwlock_rd_lock(parent->nondestroy);
    rwlock_rd_unlock(parent->parent->nondestroy);
    rwlock_wr_lock(parent->lock);

    if (hmap_get(parent->subdirs, subdir_name)) {
        // subdir already exists
        err = EEXIST;
    } else {
        err = dir_create(parent, subdir_name);
    }

    rwlock_wr_unlock(parent->lock);
    rwlock_rd_unlock(parent->nondestroy);
    free(parent_path);
    return err;
}

char *tree_list(Tree *tree, const char *path) {
    assert(tree != NULL);
    int err;
    printf("jednak tu!");

    if (!is_path_valid(path)) return NULL;

    Directory *d = NULL;
    err = tree_find(&d, tree, path);
    if (err) return NULL;

    rwlock_rd_lock(d->nondestroy);
    rwlock_rd_unlock(d->parent->nondestroy);
    rwlock_rd_lock(d->lock);

    char *res = dir_list(d);
    rwlock_rd_unlock(d->lock);
    rwlock_rd_unlock(d->nondestroy);
    return res;
}

int tree_remove(Tree *tree, const char *path) {
    assert(tree != NULL);
    int err;

    if (!is_path_valid(path)) return EINVAL;

    if (strcmp(path, "/") == 0) return EBUSY;

    char subdir_name[MAX_FOLDER_NAME_LENGTH + 1];
    char *parent_path = make_path_to_parent(path, subdir_name);
    Directory *parent = NULL;

    err = tree_find(&parent, tree, parent_path);
    if (err) {
        free(parent_path);
        return err;
    }

    rwlock_wr_lock(parent->nondestroy);
    rwlock_rd_unlock(parent->parent->nondestroy);

    Directory *dir = hmap_get(parent->subdirs, subdir_name);
    if (!dir) {
        // to-be-removed subdir does not exist
        rwlock_wr_unlock(parent->nondestroy);
        free(parent_path);
        return ENOENT;
    }

    rwlock_wr_lock(dir->nondestroy); // TODO
    if (hmap_size(dir->subdirs) > 0) {
        // to-be-removed subdir is not empty
        rwlock_wr_unlock(dir->nondestroy);
        rwlock_wr_unlock(parent->nondestroy);
        free(parent_path);
        return ENOTEMPTY;
    }

    hmap_remove(parent->subdirs, subdir_name);
    rwlock_wr_unlock(parent->nondestroy);
    rwlock_wr_unlock(dir->nondestroy);
    dir_free(dir);
    free(parent_path);

    return 0;
}

int tree_move(Tree *tree, const char *source, const char *target) {
    return 0;
}

/*
int tree_move(Tree *tree, const char *source, const char *target) {
    assert(tree && source && target);

    if (!is_path_valid(source) || !is_path_valid(target))
        return EINVAL;

    if (strcmp(source, target) == 0) return 0;

    if (is_subpath(target, source)) return -2; // TODO: describe error code.

    if (is_subpath(source, target)) {

    // TODO


    } else {





    }


}
*/


/*
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
    char *target_parent_path = make_path_to_parent(target, target_dir_name);
    Directory *target_parent_dir = NULL;

    // Lock the directory of lexicographically
    // bigger path first (to prevent deadlocks).
    if (strcmp(source_parent_path, target_parent_path) > 0) {
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
    } else if (strcmp(source_parent_path, target_parent_path) < 0) {
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
    } else {
        if ((err = tree_find_wr_lock(&target_parent_dir, tree,
                                     target_parent_path)) != 0) {
            free(source_parent_path);
            free(target_parent_path);
            return err;
        }
        source_parent_dir = target_parent_dir;
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
    hmap_insert(target_parent_dir->subdirs, target_dir_name, to_move);

    if (source_parent_dir == target_parent_dir) {
        err = rwlock_wr_unlock(source_parent_dir->lock);
        free(source_parent_path);
        free(target_parent_path);
        return err;
    }

    err = rwlock_wr_unlock(source_parent_dir->lock);
    err = err == 0 ? rwlock_wr_unlock(target_parent_dir->lock) : err;
    printf("\ntu jestem! \n");
    free(source_parent_path);
    free(target_parent_path);
    return err;
}*/

void tree_free(Tree *tree) {
    assert(tree != NULL);
    dir_free(tree->root->parent);
    dir_free(tree->root);
    free(tree);
}
