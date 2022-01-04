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

/*
 * IMPORTANT:
 * When traversing the tree,
 * parent node's lock is being held by the thread,
 * until child is successfully locked by the thread.
 * Then, parent is unlocked and traversal continues.
 *
 * Type of lock a thread puts on a Directory
 * is specified by "Tree traversal lock type" comment
 * before each traversal function.
 *
 * The described method ensures that
 * if a thread is waiting to lock a node,
 * the node won't be destroyed in the meantime.
 * PROOF:
 * If thread is waiting to lock a node,
 * it is holding a lock on the node's parent.
 * Thus, when thread is trying to remove a node
 * and it has already write-locked the node's parent,
 * there are no other nodes waiting to lock the node.
 * Hence, when the thread successfully
 * write-locks the node (still write-locking it's parent),
 * no other nodes are waiting on the node's lock,
 * or working on the node.
 */

/*
 * Single directory of the tree.
 */
typedef struct Directory Directory;

struct Directory {
    RWLock *lock;
    HashMap *subdirs;
    Directory *parent;
};

Directory *dir_new(Directory *parent) {
    Directory *d = malloc(sizeof(Directory));
    if (!d) syserr("", EMEMORY);

    d->subdirs = hmap_new();
    if (!d->subdirs) syserr("", EMEMORY);

    d->lock = rwlock_new();
    if (!d->lock) syserr("", EMEMORY);

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

// Write-locks root's subtree.
int dir_wr_lock(Directory *root) {
    assert(root != NULL);
    rwlock_wr_lock(root->lock);
    const char *subdir_name;
    Directory *subdir;
    HashMapIterator it = hmap_iterator(root->subdirs);
    while (hmap_next(root->subdirs, &it, &subdir_name, (void **) &subdir)) {
        dir_wr_lock(subdir);
    }
    return 0;
}

// Write-unlocks root's subtree.
int dir_wr_unlock(Directory *root) {
    assert(root != NULL);
    const char *subdir_name;
    Directory *subdir;
    HashMapIterator it = hmap_iterator(root->subdirs);
    while (hmap_next(root->subdirs, &it, &subdir_name, (void **) &subdir)) {
        dir_wr_unlock(subdir);
    }
    rwlock_wr_unlock(root->lock);
    return 0;
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

int dir_move(Directory *source_parent, Directory *target_parent,
             const char *source_dir_name, const char *target_dir_name) {
    // assert that source_parent AND target_parent are write-locked.

    int err = 0;
    Directory *moved = NULL;
    moved = hmap_get(source_parent->subdirs, source_dir_name);
    if (!moved) err = ENOENT;
    if (!err && hmap_get(target_parent->subdirs, target_dir_name)) err = EEXIST;

    if (!err) {
        dir_wr_lock(moved);
        hmap_remove(source_parent->subdirs, source_dir_name);
        hmap_insert(target_parent->subdirs, target_dir_name, moved);
        moved->parent = target_parent;
        rwlock_wr_unlock(target_parent->lock);
        if (source_parent != target_parent) {
            rwlock_wr_unlock(source_parent->lock);
        }
        dir_wr_unlock(moved);
    } else {
        rwlock_wr_unlock(source_parent->lock);
        if (source_parent != target_parent) {
            rwlock_wr_unlock(target_parent->lock);
        }
    }
    return err;
}

// Finds directory and read-locks it's parent.
// Tree traversal lock type: READ.
int dir_find_rdlock_parent(Directory **out, Directory *root, const char *path) {
    assert(root != NULL && is_path_valid(path));
    char child_name[MAX_FOLDER_NAME_LENGTH + 1];
    const char *subpath = path;
    Directory *parent = root->parent;
    rwlock_rd_lock(parent->lock);
    Directory *child = root;
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

// Finds directory and write-locks it.
// Tree traversal lock type: WRITE.
int dir_find_wrlock(Directory **out, Directory *root, const char *path, bool unlock_root) {
    assert(root != NULL && is_path_valid(path));
    // assert that root is wrlocked. // TODO:
    if (strcmp("/", path) == 0) {
        *out = root;
        return 0;
    }

    char child_name[MAX_FOLDER_NAME_LENGTH + 1];
    const char *subpath = path;
    Directory *parent = root;
    Directory *child = NULL;
    while ((subpath = split_path(subpath, child_name))) {
        child = hmap_get(parent->subdirs, child_name);
        if (!child) {
            if (unlock_root || parent != root) rwlock_wr_unlock(parent->lock);
            return ENOENT;
        }
        rwlock_wr_lock(child->lock);
        if (unlock_root || parent != root) rwlock_wr_unlock(parent->lock);
        parent = child;
    }
    *out = child;
    return 0;
}

// Finds last common ancestor and read-locks it's parent.
// Tree traversal lock type: READ.
int dir_find_common(Directory **out, Directory *root, const char *path1, const char *path2) {
    assert(root != NULL && is_path_valid(path1) && is_path_valid(path2));
    char *common_path = make_common_path(path1, path2);
    if (!common_path) syserr("", EMEMORY);

    int err = dir_find_rdlock_parent(out, root, common_path);
    free(common_path);
    return err;
}

// Finds and write-locks out1 & out2's common ancestor,
// then finds and write-locks out1 & out2,
// If the ancestor is not *out1 or *out2, it is unlocked
// before locking the *out2 node
// Tree traversal lock type: WRITE.
int dir_find_wr_lock2(Directory **out1, Directory **out2, Directory *root,
                      char *path1, char *path2) {
    assert(root && path1 && path2);
    int err;
    Directory *common = NULL;
    err = dir_find_common(&common, root, path1, path2);
    if (err) return err;

    rwlock_wr_lock(common->lock);
    rwlock_rd_unlock(common->parent->lock);

    if (strcmp(path1, path2) == 0) {
        *out1 = common;
        *out2 = common;
        return err;
    }

    char *subpath1 = path1;
    char *subpath2 = path2;
    split_common_path(&subpath1, &subpath2);

    if (is_subpath(path1, path2)) {
        *out2 = common;
        err = dir_find_wrlock(out1, common, subpath1, false);
        if (err) {
            rwlock_wr_unlock(common->lock);
            return err;
        }
    } else if (is_subpath(path2, path1)) {
        *out1 = common;
        err = dir_find_wrlock(out2, common, subpath2, false);
        if (err) {
            rwlock_wr_unlock(common->lock);
            return err;
        }
    } else {
        err = dir_find_wrlock(out2, common, subpath2, false);
        if (err) {
            rwlock_wr_unlock(common->lock);
            return err;
        }
        err = dir_find_wrlock(out1, common, subpath1, true);
        if (err) {
            rwlock_wr_unlock((*out2)->lock);
            return err;
        }
    }
    return err;
}

// ----------------------------------------------

struct Tree {
    Directory *root;
};

Tree *tree_new() {
    Tree *t = malloc(sizeof(Tree));
    if (!t) syserr("", EMEMORY);

    Directory *dummy = dir_new(NULL);
    if (!dummy) syserr("", EMEMORY);

    t->root = dir_new(dummy);
    if (!t->root) syserr("", EMEMORY);
    return t;
}

// Finds directory and read-locks its parent.
// Tree traversal lock type: READ.
int tree_find(Directory **out, Tree *tree, const char *path) {
    assert(tree != NULL && is_path_valid(path));
    return dir_find_rdlock_parent(out, tree->root, path);
}

// Creates new directory.
// Let V be the directory that will become parent
// of newly created directory.
// First, V's parent is found.
// (Tree traversal lock type: READ.)
// Then, V is write-locked and it's parent is released.
// Then the new directory is created.
int tree_create(Tree *tree, const char *path) {
    assert(tree != NULL);
    if (!is_path_valid(path)) return EINVAL;
    if (strcmp(path, "/") == 0) return EEXIST;

    int err;
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

// Return content of directory at given path.
// Tree traversal lock type: READ.
char *tree_list(Tree *tree, const char *path) {
    assert(tree != NULL);
    if (!is_path_valid(path)) return NULL;

    Directory *d = NULL;
    int err = tree_find(&d, tree, path);
    if (err) return NULL;

    rwlock_rd_lock(d->lock);
    rwlock_rd_unlock(d->parent->lock);
    char *res = dir_list(d);
    rwlock_rd_unlock(d->lock);
    return res;
}

// Finds parent of the to-be-removed directory,
// write-locks it and write-locks the to-be-removed directory.
// Then the directory is removed.
int tree_remove(Tree *tree, const char *path) {
    assert(tree != NULL);
    if (!is_path_valid(path)) return EINVAL;
    if (strcmp(path, "/") == 0) return EBUSY;

    int err;
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

// To prevent deadlocks:
// - finds and write-locks the last common ancestor of source & target
// - finds and write-locks source and target directories.
// then, whole subtree of moved directory is write-locked
// and it is moved to the new location.
int tree_move(Tree *tree, const char *source, const char *target) {
    assert(tree && source && target);
    if (!is_path_valid(source) || !is_path_valid(target)) return EINVAL;
    if (strcmp(source, "/") == 0) return EBUSY;
    if (strcmp(target, "/") == 0) return EEXIST;
    if (is_subpath(target, source)) return EMOVE;

    int err;
    char source_dir_name[MAX_FOLDER_NAME_LENGTH + 1];
    char target_dir_name[MAX_FOLDER_NAME_LENGTH + 1];
    char *source_parent_path = make_path_to_parent(source, source_dir_name);
    char *target_parent_path = make_path_to_parent(target, target_dir_name);
    Directory *source_parent = NULL;
    Directory *target_parent = NULL;

    err = dir_find_wr_lock2(&source_parent, &target_parent, tree->root,
                            source_parent_path, target_parent_path);
    if (err) {
        free(source_parent_path);
        free(target_parent_path);
        return err;
    }

    err = dir_move(source_parent, target_parent,
                   source_dir_name, target_dir_name);

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
