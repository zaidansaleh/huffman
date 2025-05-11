#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FREQ_SIZE 128

typedef struct Node {
    char symbol;
    size_t count;
    struct Node *left;
    struct Node *right;
} Node;

void node_pprint(const Node *node, int indent, FILE *stream) {
    for (int i = 0; i < 2 * indent; ++i) {
        fputc(' ', stream);
    }

    fprintf(stream, "%c %ld\n", node->symbol, node->count);
    if (node->left && node->right) {
        node_pprint(node->left, indent + 1, stream);
        node_pprint(node->right, indent + 1, stream);
    }
}

typedef struct {
    int (*comparator)(const Node *, const Node *);
    size_t capacity;
    size_t length;
    Node data[];
} Heap;

size_t heap_left_child(size_t i) {
    return 2 * i + 1;
}

size_t heap_right_child(size_t i) {
    return 2 * i + 2;
}

size_t heap_parent(size_t i) {
    return (i - 1) / 2;
}

Heap *heap_new(size_t capacity, int (*comparator)(const Node *, const Node *)) {
    Heap *heap = malloc(sizeof(Heap) + sizeof(Node) * capacity);
    if (!heap) {
        return NULL;
    }
    heap->comparator = comparator;
    heap->capacity = capacity;
    heap->length = 0;
    memset(heap->data, 0, sizeof(Node) * capacity);
    return heap;
}

int heap_insert(Heap *heap, Node value) {
    if (heap->length >= heap->capacity) {
        return -1;
    }
    Node *current = &heap->data[heap->length++];
    *current = value;
    if (heap->length == 1) {
        return 0;
    }
    Node *parent = &heap->data[heap_parent(current - heap->data)];
    while ((*heap->comparator)(current, parent) < 0 || current == heap->data) {
        Node tmp = *current;
        *current = *parent;
        *parent = tmp;
    }
    return 0;
}

void heap_free(Heap *heap) {
    free(heap);
}

int compare(const Node *a, const Node *b) {
    return (int)a->count - (int)b->count;
}

int main(void) {
    const char *s = "hello";
    size_t len = strlen(s);
    char freq[FREQ_SIZE] = {0};
    size_t symbol_count = 0;

    for (size_t i = 0; i < len; ++i) {
        char *element = &freq[(size_t)s[i]];
        if (*element == 0) {
            symbol_count++;
        }
        *element += 1;
    }

    Heap *pq = heap_new(2 * symbol_count - 1, compare);
    for (uint8_t ch = 0; ch < FREQ_SIZE; ++ch) {
        if (freq[ch] == 0) {
            continue;
        }
        Node node = {
            .symbol = ch,
            .count = freq[ch],
            .left = NULL,
            .right = NULL,
        };
        if (heap_insert(pq, node) < 0) {
            fprintf(stderr, "error: heap insert failed\n");
            heap_free(pq);
            return 1;
        }
    }

    for (size_t i = 0; i < pq->length; ++i) {
        node_pprint(&pq->data[i], 0, stdout);
    }

    heap_free(pq);
    return 0;
}

