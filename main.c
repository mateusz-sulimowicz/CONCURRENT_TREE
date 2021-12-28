#include "HashMap.h"
#include "path_utils.h"
#include "Tree.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

void print_map(HashMap *map) {
    const char *key = NULL;
    void *value = NULL;
    printf("Size=%zd\n", hmap_size(map));
    HashMapIterator it = hmap_iterator(map);
    while (hmap_next(map, &it, &key, &value)) {
        printf("Key=%s Value=%p\n", key, value);
    }
    printf("\n");
}


int main(void) {
    HashMap *map = hmap_new();
    hmap_insert(map, "a", hmap_new());
    print_map(map);

    HashMap *child = (HashMap *) hmap_get(map, "a");
    hmap_free(child);
    hmap_remove(map, "a");
    print_map(map);

    hmap_free(map);


    Tree *t = tree_new();
    tree_create(t, "/a/");
    tree_create(t, "/b/");
    tree_create(t, "/a/c/");

    tree_remove(t, "/a/c/");

    printf("\nZawartosc a:\n");
    printf("\n%s", tree_list(t, "/a/"));
    printf("\nZawartosc b:\n");
    printf("\n%s", tree_list(t, "/b/"));
    printf("\nZawartosc c:\n");
    printf("\n%s", tree_list(t, "/c/"));


    tree_create(t, "/a/d/");

    tree_remove(t, "/a/c/");

    tree_create(t, "/a/a/");


    tree_free(t);

    return 0;
}