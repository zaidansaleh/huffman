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

Node *node_new_leaf(char symbol, size_t count) {
    Node *node = malloc(sizeof(Node));
    if (!node) {
        return NULL;
    }
    node->symbol = symbol;
    node->count = count;
    node->left = NULL;
    node->right = NULL;
    return node;
}

Node *node_new_internal(Node *left, Node *right) {
    Node *node = malloc(sizeof(Node));
    if (!node) {
        return NULL;
    }
    node->symbol = '\0';
    node->count = left->count + right->count;
    node->left = left;
    node->right = right;
    return node;
}

void node_free(Node *node) {
    if (node->left && node->right) {
        node_free(node->left);
        node_free(node->right);
    }
    free(node);
}

void node_print(const Node *node, FILE *stream) {
    fprintf(stream, "('%c': %ld)", node->symbol, node->count);
}

void node_pprint(const Node *node, int indent, FILE *stream) {
    for (int i = 0; i < 2 * indent; ++i) {
        fputc(' ', stream);
    }

    node_print(node, stream);
    fputc('\n', stream);
    if (node->left && node->right) {
        node_pprint(node->left, indent + 1, stream);
        node_pprint(node->right, indent + 1, stream);
    }
}

typedef struct {
    int (*comparator)(const Node *, const Node *);
    size_t capacity;
    size_t length;
    Node *data[];
} Heap;

Node **heap_left_child(Heap *heap, Node **node) {
    size_t i = node - heap->data;
    if (i >= heap->length) {
        return NULL;
    }
    size_t j = 2 * i + 1;
    if (j >= heap->length) {
        return NULL;
    }
    return &heap->data[j];
}

Node **heap_right_child(Heap *heap, Node **node) {
    size_t i = node - heap->data;
    if (i >= heap->length) {
        return NULL;
    }
    size_t j = 2 * i + 2;
    if (j >= heap->length) {
        return NULL;
    }
    return &heap->data[j];
}

Node **heap_parent(Heap *heap, Node **node) {
    size_t i = node - heap->data;
    if (i == 0 || i >= heap->length) {
        return NULL;
    }
    return &heap->data[(i - 1) / 2];
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

int heap_insert(Heap *heap, Node *value) {
    if (heap->length >= heap->capacity) {
        return -1;
    }
    Node **hole = &heap->data[heap->length++];
    *hole = value;
    Node **parent;
    while ((parent = heap_parent(heap, hole)) && (*heap->comparator)(*hole, *parent) < 0) {
        Node *tmp = *hole;
        *hole = *parent;
        *parent = tmp;
        hole = parent;
    }
    return 0;
}

Node *heap_pop(Heap *heap) {
    Node **hole = &heap->data[0];
    Node *root = *hole;
    while (1) {
        Node **child;
        Node **left = heap_left_child(heap, hole);
        Node **right = heap_right_child(heap, hole);
        if (left && right && (*left)->count && (*right)->count) {
            child = (*heap->comparator)(*left, *right) < 0 ? left : right;
        } else if (left && (*left)->count) {
            child = left;
        } else if (right && (*right)->count) {
            child = right;
        } else {
            break;
        }
        Node *tmp = *hole;
        *hole = *child;
        *child = tmp;
        hole = child;
    }
    *hole = heap->data[--heap->length];
    return root;
}

void heap_pprint(const Heap *heap, FILE *stream) {
    for (size_t i = 0; i < heap->length; ++i) {
        node_print(heap->data[i], stream);
        fputc(' ', stream);
    }
    fprintf(stream, "<%ld empty>", heap->capacity - heap->length);
    fputc('\n', stream);
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
        Node *node = node_new_leaf(ch, freq[ch]);
        if (!node) {
            fprintf(stderr, "error: leaf node init failed\n");
            heap_free(pq);
            return 1;
        }
        if (heap_insert(pq, node) < 0) {
            fprintf(stderr, "error: heap insert failed\n");
            heap_free(pq);
            return 1;
        }
    }

    heap_pprint(pq, stdout);

    while (pq->length > 0) {
        printf("--------------------------------\n");
        Node *node = heap_pop(pq);
        node_free(node);
        heap_pprint(pq, stdout);
    }

    heap_free(pq);
    return 0;
}

