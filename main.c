#include "HashMap.h"
#include "path_utils.h"
#include "Tree.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <asm-generic/errno.h>


#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <stdbool.h>

bool my_path_valid(const char *path) {
    Tree *tree = tree_new();
    int ret = tree_create(tree, path);
    tree_free(tree);
    return ret != EINVAL;
}

char* fill_with_component(char *s, size_t len) {
    s[0] = '/';
    for (size_t i = 1; i <= len; ++i)
        s[i] = 'a';
    s[len + 1] = '/';
    s[len + 2] = '\0';
    return s + len + 1;
}

int f(int err) { if (-20 <= err && err <= -1) return -1; return err; }

int main() {
    printf("PIZDA");
    Tree *tree = tree_new();
    assert(f(tree_create(tree, "/c/c/")) == ENOENT);
    assert(f(tree_create(tree, "/")) == EEXIST);
    assert(f(tree_move(tree, "/c/a/", "/b/b/")) == ENOENT);
    assert(f(tree_remove(tree, "/b/c/c/c/")) == ENOENT);
    assert(f(tree_move(tree, "/", "/b/c/")) == EBUSY);
    assert(f(tree_create(tree, "/b/c/a/")) == ENOENT);
    assert(f(tree_move(tree, "/c/b/", "/a/c/")) == ENOENT);
    assert(f(tree_move(tree, "/c/c/", "/a/b/")) == ENOENT);
    assert(f(tree_move(tree, "/", "/")) == EBUSY);
    assert(f(tree_move(tree, "/b/", "/c/a/")) == ENOENT);
    assert(f(tree_remove(tree, "/b/b/")) == ENOENT);
    assert(f(tree_remove(tree, "/")) == EBUSY);
    assert(f(tree_remove(tree, "/a/")) == ENOENT);
    assert(f(tree_move(tree, "/a/a/c/", "/a/c/")) == ENOENT);
    assert(f(tree_move(tree, "/c/b/b/a/", "/b/a/")) == ENOENT);
    assert(f(tree_remove(tree, "/a/a/c/")) == ENOENT);
    assert(f(tree_remove(tree, "/")) == EBUSY);
    assert(f(tree_move(tree, "/c/b/", "/")) == EEXIST);
    assert(f(tree_remove(tree, "/")) == EBUSY);
    assert(f(tree_move(tree, "/", "/")) == EBUSY);
    assert(f(tree_create(tree, "/")) == EEXIST);
    assert(f(tree_move(tree, "/b/b/", "/")) == EEXIST);
    assert(f(tree_move(tree, "/b/b/a/", "/a/a/c/a/")) == ENOENT);
    assert(f(tree_remove(tree, "/c/a/a/a/")) == ENOENT);
    assert(f(tree_create(tree, "/a/a/c/b/")) == ENOENT);
    assert(f(tree_create(tree, "/b/")) == 0);
    assert(f(tree_move(tree, "/c/c/", "/a/a/c/")) == ENOENT);
    assert(f(tree_remove(tree, "/a/b/c/")) == ENOENT);
    assert(f(tree_remove(tree, "/a/c/a/")) == ENOENT);
    assert(f(tree_create(tree, "/c/b/b/a/")) == ENOENT);
    assert(f(tree_move(tree, "/a/", "/b/")) == ENOENT);
    assert(f(tree_move(tree, "/b/c/a/c/", "/b/b/c/a/")) == ENOENT);
    assert(f(tree_create(tree, "/a/c/b/a/")) == ENOENT);
    assert(f(tree_create(tree, "/c/b/")) == ENOENT);
    assert(f(tree_remove(tree, "/c/b/")) == ENOENT);
    assert(f(tree_remove(tree, "/c/")) == ENOENT);
    assert(f(tree_remove(tree, "/c/")) == ENOENT);
    assert(f(tree_remove(tree, "/c/b/")) == ENOENT);
    assert(f(tree_remove(tree, "/c/a/c/b/")) == ENOENT);
    assert(f(tree_move(tree, "/b/b/a/", "/c/a/")) == ENOENT);
    assert(f(tree_create(tree, "/")) == EEXIST);
    assert(f(tree_move(tree, "/a/b/c/b/", "/a/")) == ENOENT);
    assert(f(tree_remove(tree, "/")) == EBUSY);
    assert(f(tree_remove(tree, "/b/a/b/a/")) == ENOENT);
    assert(f(tree_remove(tree, "/c/b/")) == ENOENT);
    assert(f(tree_move(tree, "/c/c/b/a/", "/c/b/a/")) == ENOENT);
    assert(f(tree_remove(tree, "/c/a/")) == ENOENT);
    assert(f(tree_create(tree, "/b/b/")) == 0);
    assert(f(tree_move(tree, "/a/a/", "/c/c/c/")) == ENOENT);
    assert(f(tree_move(tree, "/", "/b/b/c/c/")) == EBUSY);
    assert(f(tree_remove(tree, "/b/c/b/")) == ENOENT);
    assert(f(tree_move(tree, "/c/", "/c/")) == ENOENT);
    assert(f(tree_remove(tree, "/c/c/c/c/")) == ENOENT);
    assert(f(tree_remove(tree, "/b/b/a/a/")) == ENOENT);
    assert(f(tree_create(tree, "/c/b/")) == ENOENT);
    assert(f(tree_move(tree, "/b/b/b/", "/c/c/a/a/")) == ENOENT);
    assert(f(tree_remove(tree, "/a/b/b/a/")) == ENOENT);
    assert(f(tree_remove(tree, "/c/a/")) == ENOENT);
    assert(f(tree_create(tree, "/a/b/")) == ENOENT);
    assert(f(tree_remove(tree, "/c/c/")) == ENOENT);
    assert(f(tree_create(tree, "/")) == EEXIST);
    assert(f(tree_move(tree, "/a/b/c/b/", "/b/a/")) == ENOENT);
    assert(f(tree_move(tree, "/a/a/", "/c/c/")) == ENOENT);
    assert(f(tree_create(tree, "/a/c/a/")) == ENOENT);
    assert(f(tree_move(tree, "/", "/b/a/b/")) == EBUSY);
    assert(f(tree_remove(tree, "/c/c/b/")) == ENOENT);
    assert(f(tree_create(tree, "/b/c/c/")) == ENOENT);
    assert(f(tree_create(tree, "/")) == EEXIST);
    assert(f(tree_create(tree, "/")) == EEXIST);
    assert(f(tree_remove(tree, "/b/")) == ENOTEMPTY);
    assert(f(tree_move(tree, "/", "/c/c/a/a/")) == EBUSY);
    assert(f(tree_move(tree, "/b/b/a/", "/b/")) == ENOENT);
    assert(f(tree_remove(tree, "/c/b/c/c/")) == ENOENT);
    assert(f(tree_move(tree, "/b/c/", "/a/c/c/b/")) == ENOENT);
    assert(f(tree_create(tree, "/c/")) == 0);
    assert(f(tree_remove(tree, "/c/b/c/")) == ENOENT);
    assert(f(tree_move(tree, "/a/b/c/b/", "/c/b/b/a/")) == ENOENT);
    assert(f(tree_move(tree, "/b/c/", "/")) == EEXIST);
    assert(f(tree_remove(tree, "/c/c/c/")) == ENOENT);
    assert(f(tree_remove(tree, "/b/b/b/b/")) == ENOENT);
    assert(f(tree_create(tree, "/a/")) == 0);
    assert(f(tree_move(tree, "/c/a/", "/b/a/a/c/")) == ENOENT);
    assert(f(tree_move(tree, "/c/a/a/", "/")) == EEXIST);
    assert(f(tree_remove(tree, "/a/a/")) == ENOENT);
    assert(f(tree_remove(tree, "/")) == EBUSY);
    assert(f(tree_remove(tree, "/a/a/c/")) == ENOENT);
    assert(f(tree_create(tree, "/a/a/b/")) == ENOENT);
    assert(f(tree_create(tree, "/a/")) == EEXIST);
    assert(f(tree_move(tree, "/a/b/b/a/", "/b/")) == ENOENT);

}
