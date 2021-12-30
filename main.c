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

int main() {
  /*  Tree *tree = tree_new();
    char *list_content = tree_list(tree, "/");
    assert(strcmp(list_content, "") == 0);
    free(list_content);
    assert(tree_list(tree, "/a/") == NULL);
    assert(tree_create(tree, "/a/") == 0);
    assert(tree_create(tree, "/a/b/") == 0);
    assert(tree_create(tree, "/a/b/") == EEXIST);
    assert(tree_create(tree, "/a/b/c/d/") == ENOENT);
    assert(tree_remove(tree, "/a/") == ENOTEMPTY);
    assert(tree_create(tree, "/b/") == 0);
    assert(tree_create(tree, "/a/c/") == 0);
    assert(tree_create(tree, "/a/c/d/") == 0);
    assert(tree_move(tree, "/a/c/", "/b/c/") == 0);
    assert(tree_remove(tree, "/b/c/d/") == 0);
    list_content = tree_list(tree, "/b/");
    assert(strcmp(list_content, "c") == 0);
    free(list_content);
    tree_free(tree);
*/

  char path1[] = "/a/b/c/e/";
  char path2[] = "/a/b/c/d/";
  char *sp1 = path1;
  char *sp2 = path2;
  split_common_path(&sp1, &sp2);
  printf("%s\n", sp1);
  printf("%s\n", sp2);

  printf("%s", make_common_path("/a/b/c/", "/a/b/d/"));

}
