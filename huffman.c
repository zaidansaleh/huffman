/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Saleh Zaidan */

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILE_EXTENSION ".huff"

#define INPUT_INITIAL_CAPACITY 128
#define SYMBOL_SIZE 128
#define UINT8_WIDTH 8

#define DEBUG_FREQ (1 << 0)
#define DEBUG_TREE (1 << 1)
#define DEBUG_CODE (1 << 2)

const char *escape_char(char ch) {
    static char escape_char_buf[2];
    if (isprint(ch)) {
        escape_char_buf[0] = ch;
        escape_char_buf[1] = '\0';
        return escape_char_buf;
    }
    switch (ch) {
    case '\n': return "\\n";
    case '\t': return "\\t";
    case '\r': return "\\r";
    case '\b': return "\\b";
    case '\f': return "\\f";
    case '\v': return "\\v";
    case '\\': return "\\\\";
    case '\'': return "\\'";
    case '\"': return "\\\"";
    case '\0': return "\\0";
    }
    return NULL;
}

uint32_t uint32_be_read(uint8_t *buf) {
    return buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
}

void uint32_be_write(uint8_t *buf, uint32_t value) {
    buf[0] = (value >> 24) & 0xFF;
    buf[1] = (value >> 16) & 0xFF;
    buf[2] = (value >> 8) & 0xFF;
    buf[3] = value & 0xFF;
}

typedef struct Node {
    char symbol;
    size_t count;
    struct Node *left;
    struct Node *right;
} Node;

Node *node_new() {
    Node *node = malloc(sizeof(Node));
    if (!node) {
        return NULL;
    }
    memset(node, 0, sizeof(Node));
    return node;
}

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
    if (node->left && node->right) {
        fprintf(stream, "(%ld)", node->count);
    } else {
        fprintf(stream, "('%s': %ld)", escape_char(node->symbol), node->count);
    }
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
    char symbol;
    uint32_t bits;
    uint8_t length;
} Code;

void code_append(Code *code, uint8_t bit) {
    code->bits = code->bits << 1 | bit;
    code->length++;
}

void code_print(const Code *code, FILE *stream) {
    fprintf(stream, "'%s' -> ", escape_char(code->symbol));
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

typedef struct {
    size_t capacity;
    size_t length;
    size_t position;
    char data[];
} InputBuffer;

InputBuffer *input_buffer_new(FILE *stream) {
    InputBuffer *inbuf = malloc(sizeof(InputBuffer) + sizeof(char) * INPUT_INITIAL_CAPACITY);
    if (!inbuf) {
        return NULL;
    }
    inbuf->capacity = INPUT_INITIAL_CAPACITY;
    inbuf->length = 0;
    inbuf->position = 0;

    int ch;
    while ((ch = fgetc(stream)) != EOF) {
        // Make sure length + 1 bytes are available (actual length plus null terminator).
        if (inbuf->length + 1 >= inbuf->capacity) {
            size_t new_capacity = inbuf->capacity * 2;
            void *new_inbuf = realloc(inbuf, sizeof(InputBuffer) + sizeof(char) * new_capacity);
            if (!new_inbuf) {
                free(inbuf);
                return NULL;
            }
            inbuf = new_inbuf;
            inbuf->capacity = new_capacity;
        }
        inbuf->data[inbuf->length++] = ch;
    }
    inbuf->data[inbuf->length] = '\0';

    return inbuf;
}

void input_buffer_read(InputBuffer *inbuf, uint8_t *dest, size_t len) {
    memcpy(dest, &inbuf->data[inbuf->position], len);
    inbuf->position += len;
}

void input_buffer_free(InputBuffer *inbuf) {
    free(inbuf);
}

void freq_table_build(int debug, size_t *freq, InputBuffer *inbuf, size_t *symbol_count, size_t *node_count) {
    *symbol_count = 0;

    for (size_t i = 0; i < inbuf->length; ++i) {
        size_t *element = &freq[(size_t)inbuf->data[i]];
        if (*element == 0) {
            *symbol_count += 1;
        }
        *element += 1;
    }

    *node_count = 2 * *symbol_count - 1;

    if (debug & DEBUG_FREQ) {
        fprintf(stderr, "Freq table:\n");
        for (uint8_t ch = 0; ch < SYMBOL_SIZE; ++ch) {
            size_t value = freq[ch];
            if (value > 0) {
                fprintf(stderr, "'%s' -> %ld\n", escape_char(ch), value);
            }
        }
    }
}

Node *huffman_tree_from_freq(int debug, const size_t *freq, size_t node_count) {
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

    if (debug & DEBUG_TREE) {
        fprintf(stderr, "Huffman tree:\n");
        node_pprint(root, 0, stderr);
    }

cleanup:
    if (pq) {
        heap_free(pq);
    }
    return root;
}

typedef struct {
    size_t capacity;
    size_t length;
    Code data[];
} CodeTable;

CodeTable *code_table_new(size_t capacity) {
    CodeTable *table = malloc(sizeof(CodeTable) + sizeof(Code) * capacity);
    if (!table) {
        return NULL;
    }
    table->capacity = capacity;
    table->length = 0;
    memset(table->data, 0, sizeof(Code) * capacity);
    return table;
}

int code_table_insert(CodeTable *table, Code *code) {
    if (table->length >= table->capacity) {
        return -1;
    }
    table->data[table->length++] = *code;
    return 0;
}

Code *code_table_get(CodeTable *table, size_t index) {
    if (index >= table->length) {
        return NULL;
    }
    return &table->data[index];
}

Code *code_table_find(CodeTable *table, char symbol) {
    for (size_t i = 0; i < table->length; ++i) {
        Code *code = code_table_get(table, i);
        if (code && code->symbol == symbol) {
            return code;
        }
    }
    return NULL;
}

void code_table_free(CodeTable *table) {
    free(table);
}

CodeTable *code_table_from_huffman_tree(int debug, Node *root, size_t node_count) {
    bool error = false;
    CodeTable *table = NULL;
    Stack *st = NULL;

    table = code_table_new(node_count);
    if (!table) {
        error = true;
        goto cleanup;
    }

    st = stack_new(node_count);
    if (!st) {
        error = true;
        goto cleanup;
    }

    StackElement root_element = {
        .node = root,
        .code = {.symbol = root->symbol, .bits = 0, .length = 0},
    };
    if (stack_push(st, root_element) < 0) {
        goto cleanup;
    }
    while (st->length > 0) {
        StackElement element = stack_pop(st);
        if (!element.node) {
            error = true;
            goto cleanup;
        }

        Node *node = element.node;
        if (!node->left && !node->right) {
            if (code_table_insert(table, &element.code) < 0) {
                error = true;
                goto cleanup;
            }
        }

        if (node->left) {
            Code code = element.code;
            code.symbol = node->left->symbol;
            code_append(&code, 0);
            StackElement el = {.node = node->left, .code = code};
            if (stack_push(st, el) < 0) {
                error = true;
                goto cleanup;
            }
        }
        if (node->right) {
            Code code = element.code;
            code.symbol = node->right->symbol;
            code_append(&code, 1);
            StackElement el = {.node = node->right, .code = code};
            if (stack_push(st, el) < 0) {
                error = true;
                goto cleanup;
            }
        }
    }

    if (debug & DEBUG_CODE) {
        fprintf(stderr, "Code table:\n");
        for (size_t i = 0; i < table->length; ++i) {
            Code *code = code_table_get(table, i);
            if (code) {
                code_print(code, stderr);
                fputc('\n', stderr);
            }
        }
    }

cleanup:
    if (st) {
        stack_free(st);
    }
    if (error) {
        code_table_free(table);
        return NULL;
    }
    return table;
}

CodeTable *code_table_from_compressed(int debug, InputBuffer *inbuf, uint32_t *char_count) {
    bool error = false;
    CodeTable *table = NULL;

    uint8_t buf[4];
    input_buffer_read(inbuf, buf, sizeof(buf));
    *char_count = uint32_be_read(buf);

    uint8_t symbol_count;
    memcpy(&symbol_count, &inbuf->data[inbuf->position++], sizeof(uint8_t));

    table = code_table_new(symbol_count);
    if (!table) {
        error = true;
        goto cleanup;
    }

    Code cursor = {.symbol = '\0', .bits = 0, .length = 0};
    for (uint8_t i = 0; i < symbol_count; ++i) {
        Code current = {.symbol = '\0', .bits = 0, .length = 0};
        memcpy(&current.symbol, &inbuf->data[inbuf->position++], sizeof(char));
        memcpy(&current.length, &inbuf->data[inbuf->position++], sizeof(uint8_t));
        if (current.length > cursor.length) {
            cursor.bits <<= current.length - cursor.length;
            cursor.length = current.length;
        }
        current.bits = cursor.bits;
        cursor.bits += 1;
        if (code_table_insert(table, &current) < 0) {
            error = true;
            goto cleanup;
        }
    }

    if (debug & DEBUG_CODE) {
        fprintf(stderr, "Code table:\n");
        for (size_t i = 0; i < table->length; ++i) {
            Code *code = code_table_get(table, i);
            if (code) {
                code_print(code, stderr);
                fputc('\n', stderr);
            }
        }
    }

cleanup:
    if (error) {
        code_table_free(table);
        return NULL;
    }
    return table;
}

int code_table_compare(const void *a, const void *b) {
    const Code *code_a = (Code *)a;
    const Code *code_b = (Code *)b;
    if (code_a->length != code_b->length) {
        return (int)code_a->length - (int)code_b->length;
    }
    return (int)code_a->symbol - (int)code_b->symbol;
}

int code_table_canonicalize(int debug, CodeTable *table) {
    qsort(table->data, table->length, sizeof(Code), code_table_compare);

    Code cursor = {.symbol = '\0', .bits = 0, .length = 0};
    for (size_t i = 0; i < table->length; ++i) {
        Code *current = code_table_get(table, i);
        if (!current) {
            return -1;
        }
        if (current->length > cursor.length) {
            cursor.bits <<= current->length - cursor.length;
            cursor.length = current->length;
        }
        current->bits = cursor.bits;
        cursor.bits += 1;
    }

    if (debug & DEBUG_CODE) {
        fprintf(stderr, "Canonicalized code table:\n");
        for (size_t i = 0; i < table->length; ++i) {
            Code *code = code_table_get(table, i);
            if (code) {
                code_print(code, stderr);
                fputc('\n', stderr);
            }
        }
    }

    return 0;
}

Node *huffman_tree_from_code_table(int debug, CodeTable *table) {
    bool error = false;
    Node *root = NULL;

    root = node_new();
    if (!root) {
        error = true;
        goto cleanup;
    }

    for (size_t i = 0; i < table->length; ++i) {
        Node *current = root;
        Code *code = code_table_get(table, i);
        if (!code) {
            error = true;
            goto cleanup;
        }
        for (int i = code->length - 1; i >= 0; --i) {
            uint8_t bit = (code->bits >> i) & 1;
            switch (bit) {
            case 0:
                if (!current->left) {
                    current->left = node_new();
                }
                current = current->left;
                break;
            case 1:
                if (!current->right) {
                    current->right = node_new();
                }
                current = current->right;
                break;
            }
        }
        current->symbol = code->symbol;
    }

    if (debug & DEBUG_TREE) {
        fprintf(stderr, "Huffman tree:\n");
        node_pprint(root, 0, stderr);
    }

cleanup:
    if (error) {
        node_free(root);
        return NULL;
    }
    return root;
}

int compress(InputBuffer *inbuf, CodeTable *table, FILE *stream) {
    uint8_t char_count[4];
    uint32_be_write(char_count, inbuf->length);
    size_t written = fwrite(char_count, 1, sizeof(char_count), stream);
    if (written != sizeof(char_count)) {
        return -1;
    }
    if (fputc(table->length, stream) == EOF) {
        return -1;
    }
    for (size_t i = 0; i < table->length; ++i) {
        Code *code = code_table_get(table, i);
        if (!code) {
            return -1;
        }
        if (fputc(code->symbol, stream) == EOF) {
            return -1;
        }
        if (fputc(code->length, stream) == EOF) {
            return -1;
        }
    }

    Code byte = {.symbol = '\0', .bits = 0, .length = 0};
    for (size_t i = 0; i < inbuf->length; ++i) {
        char ch = inbuf->data[i];
        Code *code = code_table_find(table, ch);
        if (!code) {
            return -1;
        }
        for (int i = code->length - 1; i >= 0; --i) {
            uint8_t bit = (code->bits >> i) & 1;
            code_append(&byte, bit);
            if (byte.length == UINT8_WIDTH) {
                if (fputc(byte.bits, stream) == EOF) {
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
        if (fputc(byte.bits, stream) == EOF) {
            return -1;
        }
    }

    return 0;
}


int decompress(InputBuffer *inbuf, Node *root, uint32_t char_count, FILE *stream) {
    Node *current = root;
    uint32_t processed_count = 0;
    for (size_t i = inbuf->position; i < inbuf->length; ++i) {
        uint8_t byte = inbuf->data[i];
        for (int i = UINT8_WIDTH - 1; i >= 0 && processed_count < char_count; --i) {
            uint8_t bit = (byte >> i) & 1;
            switch (bit) {
            case 0:
                current = current->left;
                break;
            case 1:
                current = current->right;
                break;
            }
            if (!current->left && !current->right) {
                if (fputc(current->symbol, stream) == EOF) {
                    return -1;
                }
                current = root;
                processed_count += 1;
            }
        }
    }
    return 0;
}

typedef enum {
    MODE_COMPRESS,
    MODE_DECOMPRESS,
} Mode;

int main(int argc, const char *argv[]) {
    int retcode = 0;
    Mode mode = MODE_COMPRESS;
    bool show_help = false;
    int debug = 0;
    FILE *input_file = NULL;
    FILE *output_file = NULL;
    InputBuffer *inbuf = NULL;
    Node *root = NULL;
    CodeTable *table = NULL;

    int arg_cursor = 0;
    const char *program_name = argv[arg_cursor++];

    for (int i = 0; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "-c") == 0 || strcmp(arg, "--stdout") == 0) {
            output_file = stdout;
            arg_cursor += 1;
        } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            show_help = true;
            arg_cursor += 1;
        } else if (strcmp(arg, "-d") == 0 || strcmp(arg, "--decompress") == 0) {
            mode = MODE_DECOMPRESS;
            arg_cursor += 1;
        } else if (strcmp(arg, "--debug-") == 0) {
            const char *debug_level = arg + 8;
            if (strcmp(debug_level, "freq") == 0) {
                debug |= DEBUG_FREQ;
                arg_cursor += 1;
            }
            if (strcmp(debug_level, "tree") == 0) {
                debug |= DEBUG_TREE;
                arg_cursor += 1;
            }
            if (strcmp(debug_level, "code") == 0) {
                debug |= DEBUG_CODE;
                arg_cursor += 1;
            }
        }
    }

    if (show_help) {
        printf(
            "Usage: %s [options] [file]\n"
            "Compress input file using Huffman coding.\n"
            "The output is a new file with the same name as the input plus a .huff suffix.\n"
            "\n"
            "Options:\n"
            "  %-17s %s\n"
            "  %-17s %s\n"
            "  %-17s %s\n"
            "  %-17s %s\n"
            "  %-17s %s\n"
            "  %-17s %s\n",
            program_name,
            "-c, --stdout", "Write on standard output",
            "-d, --decompress", "Decompress input instead of compressing",
            "--debug-freq", "Print the frequency table to stderr",
            "--debug-tree", "Print the Huffman tree to stderr",
            "--debug-code", "Print the code table to stderr",
            "-h, --help", "Display this help message"
        );
        goto cleanup;
    }

    const char *input_pathname;
    size_t input_pathlen;
    if (arg_cursor >= argc) {
        input_file = stdin;
    } else if (argv[arg_cursor][0] == '-') {
        arg_cursor++;
        input_file = stdin;
    } else {
        input_pathname = argv[arg_cursor++];
        input_pathlen = strlen(input_pathname);
        bool is_suffix_huff = strcmp(input_pathname + input_pathlen - strlen(FILE_EXTENSION), FILE_EXTENSION) == 0;
        if (mode == MODE_COMPRESS && is_suffix_huff) {
            fprintf(stderr, "error: suffix already '%s'\n", FILE_EXTENSION);
            retcode = 1;
            goto cleanup;
        }
        if (mode == MODE_DECOMPRESS && !is_suffix_huff) {
            fprintf(stderr, "error: unknown suffix on '%s'\n", input_pathname);
            retcode = 1;
            goto cleanup;
        }
        input_file = fopen(input_pathname, "r");
        if (!input_file) {
            fprintf(stderr, "error: input file '%s' open failed\n", input_pathname);
            retcode = 1;
            goto cleanup;
        }
    }

    if (input_file == stdin) {
        output_file = stdout;
    } else {
        char output_pathname[4096] = {0};
        switch (mode) {
        case MODE_COMPRESS:
            memcpy(output_pathname, input_pathname, input_pathlen);
            strcat(output_pathname, FILE_EXTENSION);
            break;
        case MODE_DECOMPRESS:
            memcpy(output_pathname, input_pathname, input_pathlen - strlen(FILE_EXTENSION));
            break;
        }
        output_file = fopen(output_pathname, "w");
        if (!output_file) {
            fprintf(stderr, "error: output file '%s' open failed\n", output_pathname);
            retcode = 1;
            goto cleanup;
        }
    }

    inbuf = input_buffer_new(input_file);
    if (!inbuf) {
        fprintf(stderr, "error: input contents read failed\n");
        retcode = 1;
        goto cleanup;
    }

    switch (mode) {
    case MODE_COMPRESS: {
        size_t freq[SYMBOL_SIZE] = {0};
        size_t symbol_count;
        size_t node_count;
        freq_table_build(debug, freq, inbuf, &symbol_count, &node_count);

        root = huffman_tree_from_freq(debug, freq, node_count);
        if (!root) {
            fprintf(stderr, "error: huffman_tree_from_freq failed\n");
            retcode = 1;
            goto cleanup;
        }

        table = code_table_from_huffman_tree(debug, root, node_count);
        if (!table) {
            fprintf(stderr, "error: code_table_from_huffman_tree failed\n");
            retcode = 1;
            goto cleanup;
        }

        if (code_table_canonicalize(debug, table) < 0) {
            fprintf(stderr, "error: code_table_canonicalize failed\n");
            retcode = 1;
            goto cleanup;
        }

        if (compress(inbuf, table, output_file) < 0) {
            fprintf(stderr, "error: compress failed\n");
            retcode = 1;
            goto cleanup;
        }
        break;
    }
    case MODE_DECOMPRESS: {
        uint32_t char_count;
        table = code_table_from_compressed(debug, inbuf, &char_count);
        if (!table) {
            fprintf(stderr, "error: code_table_from_compressed failed\n");
            retcode = 1;
            goto cleanup;
        }

        root = huffman_tree_from_code_table(debug, table);
        if (!root) {
            fprintf(stderr, "error: huffman_tree_from_code_table failed\n");
            retcode = 1;
            goto cleanup;
        }

        if (decompress(inbuf, root, char_count, output_file) < 0) {
            fprintf(stderr, "error: decompress failed\n");
            retcode = 1;
            goto cleanup;
        }
        break;
    }
    }

cleanup:
    if (input_file) {
        fclose(input_file);
    }
    if (output_file) {
        fclose(output_file);
    }
    if (inbuf) {
        input_buffer_free(inbuf);
    }
    if (root) {
        node_free(root);
    }
    if (table) {
        code_table_free(table);
    }
    return retcode;
}

