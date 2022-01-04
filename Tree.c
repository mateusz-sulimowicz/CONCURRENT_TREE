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


// Finds directory and read-locks its parent.
int dir_find(Directory **out, Directory *root, const char *path) {
    assert(root != NULL && is_path_valid(path));

    char child_name[MAX_FOLDER_NAME_LENGTH + 1];
    const char *subpath = path;

    Directory *parent = root->parent;
    Directory *child = root;
    while ((subpath = split_path(subpath, child_name))) {
        parent = child;
        child = hmap_get(parent->subdirs, child_name);
        if (!child) return ENOENT;
    }
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

// Finds directory and read-locks its parent.
int tree_find(Directory **out, Tree *tree, const char *path) {
    assert(tree != NULL && is_path_valid(path));

    char child_name[MAX_FOLDER_NAME_LENGTH + 1];
    const char *subpath = path;

    Directory *parent = tree->root->parent;
    rwlock_rd_lock(parent->lock);
    Directory *child = tree->root;
    while ((subpath = split_path(subpath, child_name))) {
        rwlock_rd_lock(child->lock);
        rwlock_rd_unlock(parent->lock);

        parent = child;
        child = hmap_get(parent->subdirs, child_name);
        if (!child) {
            rwlock_rd_unlock(parent->lock);
            return ENOENT;
        }
    }
    *out = child;
    return 0;
}

// Finds last common ancestor
int tree_find_common(Directory **out, Tree *tree, const char *path1, const char *path2) {
    assert(tree != NULL && is_path_valid(path1) && is_path_valid(path2));
    char *common_path = make_common_path(path1, path2);
    if (!common_path) return -1; // TODO: desc

    int err = tree_find(out, tree, common_path);
    free(common_path);
    return err;
}

int tree_find2_wrlock(Directory **common, Directory **out1, Directory **out2,
                      Tree *tree, char *path1, char *path2) {
    assert(tree != NULL && is_path_valid(path1) && is_path_valid(path2));

    int err = tree_find_common(common, tree, path1, path2);
    if (err) return err;
    *out1 = *common;
    *out2 = *common;

    rwlock_wr_lock((*common)->lock);
    rwlock_rd_unlock((*common)->parent->lock);

    err = split_common_path(&path1, &path2);
    if (err) {
        rwlock_wr_unlock((*common)->lock);
        return err;
    }

    char child_name[MAX_FOLDER_NAME_LENGTH + 1];
    const char *subpath = path1;

    Directory *parent = *common;
    Directory *child = NULL;
    while ((subpath = split_path(subpath, child_name))) {
        child = hmap_get(parent->subdirs, child_name);
        if (!child) {
            // subdir not found.
            rwlock_wr_unlock(parent->lock);
            if (parent != *common)
                rwlock_wr_unlock((*common)->lock);
            return ENOENT;
        }

        rwlock_wr_lock(child->lock);
        if (parent != *common) rwlock_wr_unlock(parent->lock);
        parent = child;
    }
    *out1 = parent;

    subpath = path2;
    parent = *common;
    child = NULL;
    while ((subpath = split_path(subpath, child_name))) {
        child = hmap_get(parent->subdirs, child_name);
        if (!child) {
            // subdir not found.
            rwlock_wr_unlock(parent->lock);
            if (parent != *common)
                rwlock_wr_unlock((*common)->lock);
            if (*out1 != *common)
                rwlock_wr_unlock((*out1)->lock);
            return ENOENT;
        }

        rwlock_wr_lock(child->lock);
        if (parent != *common) rwlock_wr_unlock(parent->lock);
        parent = child;
    }
    *out2 = parent;
    return 0;
}

int tree_wrlock(Directory *root) {
    assert(root != NULL);

    rwlock_wr_lock(root->lock);

    const char *subdir_name;
    Directory *subdir;
    HashMapIterator it = hmap_iterator(root->subdirs);
    while (hmap_next(root->subdirs, &it, &subdir_name, (void **) &subdir)) {
        tree_wrlock(subdir);
    }
    return 0;
}

int tree_wrlock_subtree(Directory *root) {
    assert(root != NULL);
    // we assume that root already is wrlocked

    const char *subdir_name;
    Directory *subdir;
    HashMapIterator it = hmap_iterator(root->subdirs);
    while (hmap_next(root->subdirs, &it, &subdir_name, (void **) &subdir)) {
        tree_wrlock(subdir);
    }
    return 0;
}

int tree_wr_unlock(Directory *root) {
    assert(root != NULL);

    const char *subdir_name;
    Directory *subdir;
    HashMapIterator it = hmap_iterator(root->subdirs);
    while (hmap_next(root->subdirs, &it, &subdir_name, (void **) &subdir)) {
        tree_wr_unlock(subdir);
    }

    rwlock_wr_unlock(root->lock);
    return 0;
}

int tree_create(Tree *tree, const char *path) {
    assert(tree != NULL);
    int err;

    if (!is_path_valid(path)) return EINVAL;
    if (strcmp(path, "/") == 0) return EEXIST;

    char subdir_name[MAX_FOLDER_NAME_LENGTH + 1];
    char *parent_path = make_path_to_parent(path, subdir_name);
    Directory *parent = NULL;

    err = tree_find(&parent, tree, parent_path);
    if (err) {
        free(parent_path);
        return err;
    }

    rwlock_wr_lock(parent->lock);
    rwlock_rd_unlock(parent->parent->lock);

    if (hmap_get(parent->subdirs, subdir_name)) {
        // subdir already exists
        err = EEXIST;
    } else {
        err = dir_create(parent, subdir_name);
    }

    rwlock_wr_unlock(parent->lock);
    free(parent_path);
    return err;
}

char *tree_list(Tree *tree, const char *path) {
    assert(tree != NULL);
    int err;

    if (!is_path_valid(path)) return NULL;

    Directory *d = NULL;
    err = tree_find(&d, tree, path);
    if (err) return NULL;

    rwlock_rd_lock(d->lock);
    rwlock_rd_unlock(d->parent->lock);

    char *res = dir_list(d);

    rwlock_rd_unlock(d->lock);
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

    rwlock_wr_lock(parent->lock);
    rwlock_rd_unlock(parent->parent->lock);

    Directory *dir = hmap_get(parent->subdirs, subdir_name);
    if (!dir) {
        // to-be-removed subdir does not exist
        rwlock_wr_unlock(parent->lock);
        free(parent_path);
        return ENOENT;
    }

    rwlock_wr_lock(dir->lock); // TODO
    if (hmap_size(dir->subdirs) > 0) {
        // to-be-removed subdir is not empty
        rwlock_wr_unlock(parent->lock);
        rwlock_wr_unlock(dir->lock);
        free(parent_path);
        return ENOTEMPTY;
    }

    hmap_remove(parent->subdirs, subdir_name);
    rwlock_wr_unlock(parent->lock);
    rwlock_wr_unlock(dir->lock);
    dir_free(dir);
    free(parent_path);

    return 0;
}

int tree_move(Tree *tree, const char *source, const char *target) {
    assert(tree && source && target);

    if (!is_path_valid(source) || !is_path_valid(target)) return EINVAL;
    if (strcmp(source, "/") == 0) return EBUSY;
    if (strcmp(target, "/") == 0) return EEXIST;
    if (is_subpath(target, source)) return -2; // TODO: describe error code.

    char source_dir_name[MAX_FOLDER_NAME_LENGTH + 1];
    char target_dir_name[MAX_FOLDER_NAME_LENGTH + 1];
    char *source_parent_path = make_path_to_parent(source, source_dir_name);
    char *target_parent_path = make_path_to_parent(target, target_dir_name);

    Directory *common = NULL;
    Directory *source_parent = NULL;
    Directory *target_parent = NULL;

    int err = tree_find_common(&common, tree, source_parent_path, target_parent_path);
    if (err) {
        free(source_parent_path);
        free(target_parent_path);
        return err;
    }

    tree_wrlock(common);
    rwlock_rd_unlock(common->parent->lock);

    char *sp1 = source_parent_path;
    char *sp2 = target_parent_path;
    err = split_common_path(&sp1, &sp2);

    if (!err) err = dir_find(&source_parent, common, sp1);
    if (!err) err = dir_find(&target_parent, common, sp2);

    Directory *moved = NULL;
    if (!err) {
        moved = hmap_get(source_parent->subdirs, source_dir_name);
        if (!moved) err = ENOENT;
    }

    if (!err && hmap_get(target_parent->subdirs, target_dir_name))
        err = EEXIST;

    if (!err) {
        hmap_remove(source_parent->subdirs, source_dir_name);
        hmap_insert(target_parent->subdirs, target_dir_name, moved);
        moved->parent = target_parent;
    }

    tree_wr_unlock(common);
    free(source_parent_path);
    free(target_parent_path);
    return err;
}

void tree_free(Tree *tree) {
    assert(tree != NULL);
    dir_free(tree->root->parent);
    dir_free(tree->root);
    free(tree);
}
