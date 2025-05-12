#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_INITIAL_CAPACITY 128
#define SYMBOL_SIZE 128
#define UINT8_WIDTH 8

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
    Heap *heap = malloc(sizeof(Heap) + sizeof(Node *) * capacity);
    if (!heap) {
        return NULL;
    }
    heap->comparator = comparator;
    heap->capacity = capacity;
    heap->length = 0;
    memset(heap->data, 0, sizeof(Node *) * capacity);
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
    for (size_t i = 0; i < heap->length; ++i) {
        Node *node = heap->data[i];
        // Avoid calling node_free to prevent recursive free since
        // heap->data stores all the nodes.
        free(node);
    }
    free(heap);
}

int compare(const Node *a, const Node *b) {
    return (int)a->count - (int)b->count;
}

typedef struct {
    uint32_t bits;
    uint8_t length;
} Code;

void code_append(Code *code, uint8_t bit) {
    code->bits = code->bits << 1 | bit;
    code->length++;
}

void code_print(const Code *code, FILE *stream) {
    for (int i = code->length - 1; i >= 0; --i) {
        uint8_t bit = (code->bits >> i) & 1;
        char ch = bit ? '1' : '0';
        fputc(ch, stream);
    }
}

typedef struct {
    Node *node;
    Code code;
} StackElement;

typedef struct {
    size_t capacity;
    size_t length;
    StackElement data[];
} Stack;

Stack *stack_new(size_t capacity) {
    Stack *stack = malloc(sizeof(Stack) + sizeof(StackElement) * capacity);
    if (!stack) {
        return NULL;
    }
    stack->capacity = capacity;
    stack->length = 0;
    memset(stack->data, 0, capacity);
    return stack;
}

int stack_push(Stack *stack, StackElement element) {
    if (stack->length >= stack->capacity) {
        return -1;
    }
    stack->data[stack->length++] = element;
    return 0;
}

StackElement stack_pop(Stack *stack) {
    if (stack->length == 0) {
        StackElement el = {0};
        return el;
    }
    return stack->data[--stack->length];
}

void stack_free(Stack *stack) {
    free(stack);
}

void freq_table_build(size_t *freq, const char *input, size_t *char_count, size_t *symbol_count, size_t *node_count) {
    *char_count = strlen(input);
    *symbol_count = 0;

    for (size_t i = 0; i < *char_count; ++i) {
        size_t *element = &freq[(size_t)input[i]];
        if (*element == 0) {
            *symbol_count += 1;
        }
        *element += 1;
    }

    *node_count = 2 * *symbol_count - 1;
}

Node *huffman_tree_build(const size_t *freq, size_t node_count) {
    Node *root = NULL;

    Heap *pq = heap_new(node_count, compare);
    if (!pq) {
        goto cleanup;
    }

    for (uint8_t ch = 0; ch < SYMBOL_SIZE; ++ch) {
        if (freq[ch] == 0) {
            continue;
        }
        Node *node = node_new_leaf(ch, freq[ch]);
        if (!node) {
            goto cleanup;
        }
        if (heap_insert(pq, node) < 0) {
            goto cleanup;
        }
    }

    while (pq->length > 1) {
        Node *a = heap_pop(pq);
        Node *b = heap_pop(pq);
        Node *node = node_new_internal(a, b);
        if (!node) {
            // a and b were removed from pq by heap_pop, so they must be freed manually.
            free(a);
            free(b);
            goto cleanup;
        }
        if (heap_insert(pq, node) < 0) {
            free(a);
            free(b);
            goto cleanup;
        }
    }
    root = heap_pop(pq);

cleanup:
    if (pq) {
        heap_free(pq);
    }
    return root;
}

int code_table_build(Code *table, Node *root, size_t node_count) {
    int retcode = 0;
    Stack *st = stack_new(node_count);
    if (!st) {
        retcode = -1;
        goto cleanup;
    }

    StackElement root_element = {
        .node = root,
        .code = {.bits = 0, .length = 0},
    };
    if (stack_push(st, root_element) < 0) {
        retcode = -1;
        goto cleanup;
    }
    while (st->length > 0) {
        StackElement element = stack_pop(st);
        if (!element.node) {
            retcode = -1;
            goto cleanup;
        }

        Node *node = element.node;
        if (!node->left && !node->right) {
            table[(size_t)node->symbol] = element.code;
        }

        if (node->left) {
            Code code = element.code;
            code_append(&code, 0);
            StackElement el = {.node = node->left, .code = code};
            if (stack_push(st, el) < 0) {
                retcode = -1;
                goto cleanup;
            }
        }
        if (node->right) {
            Code code = element.code;
            code_append(&code, 1);
            StackElement el = {.node = node->right, .code = code};
            if (stack_push(st, el) < 0) {
                retcode = -1;
                goto cleanup;
            }
        }
    }

cleanup:
    if (st) {
        stack_free(st);
    }
    return retcode;
}

int compress(const char *input, size_t char_count, const Code *table, FILE *stream) {
    Code byte = {.bits = 0, .length = 0};
    for (size_t i = 0; i < char_count; ++i) {
        char ch = input[i];
        const Code *code = &table[(size_t)ch];
        for (int i = code->length - 1; i >= 0; --i) {
            uint8_t bit = (code->bits >> i) & 1;
            code_append(&byte, bit);
            if (byte.length == UINT8_WIDTH) {
                if (!fwrite(&byte.bits, 1, 1, stream)) {
                    return -1;
                }
                byte.bits = 0;
                byte.length = 0;
            }
        }
    }
    if (byte.length != UINT8_WIDTH) {
        uint8_t remaining = UINT8_WIDTH - byte.length;
        byte.bits = byte.bits << remaining;
        if (!fwrite(&byte.bits, 1, 1, stream)) {
            return -1;
        }
    }

    return 0;
}

char *input_new(FILE *stream) {
    size_t capacity = INPUT_INITIAL_CAPACITY;
    size_t length = 0;
    char *input = malloc(sizeof(char) * capacity);
    if (!input) {
        return NULL;
    }

    int ch;
    while ((ch = fgetc(stream)) != EOF) {
        // Make sure length + 1 bytes are available (actual length plus null terminator).
        if (length + 1 >= capacity) {
            capacity *= 2;
            void *tmp = realloc(input, sizeof(char) * capacity);
            if (!tmp) {
                free(input);
                return NULL;
            }
            input = tmp;
        }
        input[length++] = ch;
    }
    input[length] = '\0';

    return input;
}

void input_free(char *input) {
    free(input);
}

int main(int argc, const char *argv[]) {
    int retcode = 0;
    FILE *input_file = NULL;
    FILE *output_file = NULL;
    char *input = NULL;
    Node *root = NULL;

    if (argc < 2) {
        input_file = stdin;
    } else {
        const char *pathname = argv[1];
        input_file = fopen(pathname, "r");
        if (!input_file) {
            fprintf(stderr, "error: input file '%s' open failed\n", pathname);
            retcode = 1;
            goto cleanup;
        }
    }

    if (argc < 3) {
        output_file = stdout;
    } else {
        const char *pathname = argv[2];
        output_file = fopen(pathname, "w");
        if (!output_file) {
            fprintf(stderr, "error: output file '%s' open failed\n", pathname);
            retcode = 1;
            goto cleanup;
        }
    }

    input = input_new(input_file);
    if (!input) {
        fprintf(stderr, "error: input contents read failed\n");
        retcode = 1;
        goto cleanup;
    }

    size_t freq[SYMBOL_SIZE] = {0};
    size_t char_count;
    size_t symbol_count;
    size_t node_count;
    freq_table_build(freq, input, &char_count, &symbol_count, &node_count);

    root = huffman_tree_build(freq, node_count);
    if (!root) {
        fprintf(stderr, "error: huffman tree build failed\n");
        retcode = 1;
        goto cleanup;
    }

    Code table[SYMBOL_SIZE] = {0};
    if (code_table_build(table, root, node_count) < 0) {
        fprintf(stderr, "error: code table build failed\n");
        retcode = 1;
        goto cleanup;
    }

    if (compress(input, char_count, table, output_file) < 0) {
        fprintf(stderr, "error: compress failed\n");
        retcode = 1;
        goto cleanup;
    }

cleanup:
    if (input_file) {
        fclose(input_file);
    }
    if (output_file) {
        fclose(output_file);
    }
    if (input) {
        input_free(input);
    }
    if (root) {
        node_free(root);
    }
    return retcode;
}

