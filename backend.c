#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>

#ifndef _WIN32
#error "This program is Windows-only."
#endif

#include <direct.h>
#define MKDIR(path) _mkdir(path)

#define DATA_DIR "backend_data"
#define UNDO_DIR  DATA_DIR "/undo"
#define REDO_DIR  DATA_DIR "/redo"
#define CURRENT_FILE DATA_DIR "/current.txt"
#define META_FILE DATA_DIR "/meta.txt"

static void ensure_dirs() {
    struct stat st = {0};
    if (stat(DATA_DIR, &st) == -1) {
        MKDIR(DATA_DIR);
    }
    if (stat(UNDO_DIR, &st) == -1) {
        MKDIR(UNDO_DIR);
    }
    if (stat(REDO_DIR, &st) == -1) {
        MKDIR(REDO_DIR);
    }
}

static char *read_whole_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *) malloc(size + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    return buf;
}

static int write_whole_file(const char *path, const char *content) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fwrite(content, 1, strlen(content), f);
    fclose(f);
    return 1;
}

// meta keeps counters undo_count and redo_count
static void read_meta(int *undo_count, int *redo_count) {
    *undo_count = 0; *redo_count = 0;
    FILE *f = fopen(META_FILE, "r");
    if (!f) return;
    fscanf(f, "%d %d", undo_count, redo_count);
    fclose(f);
}
static void write_meta(int undo_count, int redo_count) {
    FILE *f = fopen(META_FILE, "w");
    if (!f) return;
    fprintf(f, "%d %d", undo_count, redo_count);
    fclose(f);
}

// stack functions
static int push_stack(const char *dir, const char *content, int *count) {
    (*count)++;
    char path[512];
    snprintf(path, sizeof(path), "%s/%d.txt", dir, *count);
    return write_whole_file(path, content);
}
static char *pop_stack(const char *dir, int *count) {
    if (*count <= 0) return NULL;
    char path[512];
    snprintf(path, sizeof(path), "%s/%d.txt", dir, *count);
    char *content = read_whole_file(path);

    remove(path);
    (*count)--;
    return content;
}

// In-memory stack for undo/redo
typedef struct MemStack {
    char **items;
    int size;
    int cap;
} MemStack;

static void memstack_init(MemStack *s) {
    s->items = NULL;
    s->size = 0;
    s->cap = 0;
}

static void memstack_free(MemStack *s) {
    if (!s) return;
    for (int i = 0; i < s->size; ++i) free(s->items[i]);
    free(s->items);
    s->items = NULL; s->size = 0; s->cap = 0;
}

static int memstack_push(MemStack *s, const char *content) {
    if (!s) return 0;
    if (s->size + 1 > s->cap) {
        int newcap = s->cap == 0 ? 8 : s->cap * 2;
        char **tmp = (char **)realloc(s->items, sizeof(char*) * newcap);
        if (!tmp) return 0;
        s->items = tmp;
        s->cap = newcap;
    }
    s->items[s->size++] = strdup(content ? content : "");
    return 1;
}

static char *memstack_pop(MemStack *s) {
    if (!s || s->size <= 0) return NULL;
    char *it = s->items[--s->size];
    s->items[s->size] = NULL;
    return it;
}

static void memstack_clear(MemStack *s) {
    if (!s) return;
    for (int i = 0; i < s->size; ++i) free(s->items[i]);
    s->size = 0;
}

static MemStack undo_stack, redo_stack;

// AVL Tree 
typedef struct AVLNode {
    char *word;
    int *positions;
    int pos_count;
    int pos_cap;
    int height;
    struct AVLNode *left;
    struct AVLNode *right;
} AVLNode;

// Get height of node
static int height(AVLNode *node) {
    if (node == NULL) return 0;
    return node->height;
}

// Get balance factor
static int get_balance(AVLNode *node) {
    if (node == NULL) return 0;
    return height(node->left) - height(node->right);
}

// Update height of node
static void update_height(AVLNode *node) {
    if (node == NULL) return;
    int left_height = height(node->left);
    int right_height = height(node->right);
    node->height = (left_height > right_height ? left_height : right_height) + 1;
}

// Right rotation
static AVLNode *rotate_right(AVLNode *y) {
    AVLNode *x = y->left;
    AVLNode *T2 = x->right;
    x->right = y;
    y->left = T2;
    update_height(y);
    update_height(x);
    return x;
}

// Left rotation
static AVLNode *rotate_left(AVLNode *x) {
    AVLNode *y = x->right;
    AVLNode *T2 = y->left;
    y->left = x;
    x->right = T2;
    update_height(x);
    update_height(y);
    return y;
}

// Create new node
static AVLNode *new_node(const char *word, int position) {
    AVLNode *node = (AVLNode *)malloc(sizeof(AVLNode));
    node->word = strdup(word);
    node->positions = (int *)malloc(sizeof(int) * 4);
    node->positions[0] = position;
    node->pos_count = 1;
    node->pos_cap = 4;
    node->height = 1;
    node->left = NULL;
    node->right = NULL;
    return node;
}

// Insert word into AVL tree
static AVLNode *insert(AVLNode *node, const char *word, int position) {
    if (node == NULL)
        return new_node(word, position);

    int cmp = strcmp(word, node->word);
    if (cmp < 0)
        node->left = insert(node->left, word, position);
    else if (cmp > 0)
        node->right = insert(node->right, word, position);
    else
    {
        // append position to this node
        if (node->pos_count + 1 > node->pos_cap) {
            int newcap = node->pos_cap * 2;
            int *tmp = (int *)realloc(node->positions, sizeof(int) * newcap);
            if (tmp) { node->positions = tmp; node->pos_cap = newcap; }
        }
        node->positions[node->pos_count++] = position;
        return node;
    }

    update_height(node);
    int balance = get_balance(node);

    // Left Left Case
    if (balance > 1 && strcmp(word, node->left->word) < 0)
        return rotate_right(node);

    // Right Right Case
    if (balance < -1 && strcmp(word, node->right->word) > 0)
        return rotate_left(node);

    // Left Right Case
    if (balance > 1 && strcmp(word, node->left->word) > 0) {
        node->left = rotate_left(node->left);
        return rotate_right(node);
    }

    // Right Left Case
    if (balance < -1 && strcmp(word, node->right->word) < 0) {
        node->right = rotate_right(node->right);
        return rotate_left(node);
    }

    return node;
}


static void free_tree(AVLNode *node) {
    if (node == NULL) return;
    free_tree(node->left);
    free_tree(node->right);
    free(node->word);
    free(node->positions);
    free(node);
}

// Build an AVL of words from text; store start indices of each whole-word occurrence
static AVLNode *build_avl_from_text(const char *text) {
    if (!text) return NULL;
    AVLNode *root = NULL;
    size_t n = strlen(text);
    size_t i = 0;
    while (i < n) {
        if (isalnum((unsigned char)text[i])) {
            size_t j = i;
            while (j < n && isalnum((unsigned char)text[j])) j++;
            size_t wlen = j - i;
            char *w = (char *)malloc(wlen + 1);
            memcpy(w, &text[i], wlen);
            w[wlen] = '\0';
            root = insert(root, w, (int)i);
            free(w);
            i = j;
        } else i++;
    }
    return root;
}

// Replace whole-word occurrences of `oldw` with `neww` in text, return newly allocated string
static char *replace_whole_words(const char *text, const char *oldw, const char *neww) {
    if (!text || !oldw || !neww) return strdup(text ? text : "");
    size_t n = strlen(text);
    size_t oldlen = strlen(oldw);
    size_t newlen = strlen(neww);
    // worst case size: replace every char with neww -> allocate n * (newlen+1) + 1
    size_t bufcap = n * (newlen + 1) + 1;
    char *out = (char *)malloc(bufcap);
    if (!out) return strdup(text);
    size_t oi = 0;
    size_t i = 0;
    while (i < n) {
        if (isalnum((unsigned char)text[i])) {
            size_t j = i;
            while (j < n && isalnum((unsigned char)text[j])) j++;
            size_t wlen = j - i;
            if (wlen == oldlen && strncmp(&text[i], oldw, oldlen) == 0) {
                // copy neww
                memcpy(&out[oi], neww, newlen); oi += newlen;
            } else {
                memcpy(&out[oi], &text[i], wlen); oi += wlen;
            }
            // copy following non-word chars
            i = j;
            while (i < n && !isalnum((unsigned char)text[i])) {
                out[oi++] = text[i++];
            }
        } else {
            out[oi++] = text[i++];
        }
    }
    out[oi] = '\0';
    // shrink to fit
    char *shr = (char *)realloc(out, oi + 1);
    return shr ? shr : out;
}

// Search word in AVL tree 
static void avl_search_and_print(const char *text, const char *pat) {
    // Use the existing whole-word scanner but keep signature correct.
    if (!text || !pat) { printf("Word not found!"); return; }
    size_t n = strlen(text);
    size_t m = strlen(pat);
    if (m == 0) { printf("Pattern empty."); return; }

    char *highlighted = (char *)malloc(n * 3 + 1);
    if (!highlighted) { printf("Internal error"); return; }
    size_t hi = 0;
    size_t i = 0;
    int found = 0;

    while (i < n) {
        if (isalnum((unsigned char)text[i])) {
            size_t j = i;
            while (j < n && isalnum((unsigned char)text[j])) j++;
            size_t wlen = j - i;
            if (wlen == m && strncmp(&text[i], pat, m) == 0) {
                const char *pre = "[HIGHLIGHT]";
                const char *post = "[/HIGHLIGHT]";
                memcpy(&highlighted[hi], pre, strlen(pre)); hi += strlen(pre);
                memcpy(&highlighted[hi], &text[i], wlen); hi += wlen;
                memcpy(&highlighted[hi], post, strlen(post)); hi += strlen(post);
                found = 1;
            } else {
                memcpy(&highlighted[hi], &text[i], wlen); hi += wlen;
            }
            i = j;
            while (i < n && !isalnum((unsigned char)text[i])) {
                highlighted[hi++] = text[i++];
            }
        } else {
            highlighted[hi++] = text[i++];
        }
    }
    highlighted[hi] = '\0';

    if (!found) printf("Word not found!");
    else printf("%s", highlighted);

    free(highlighted);
}


// Doubly-linked list

typedef struct CharNode {
    char ch;
    struct CharNode *prev;
    struct CharNode *next;
} CharNode;

typedef struct Buffer {
    CharNode *head;
    CharNode *tail;
    
    CharNode *cursor;
} Buffer;

static Buffer *buffer_create_from_string(const char *s) {
    Buffer *b = (Buffer *)malloc(sizeof(Buffer));
    if (!b) return NULL;
    b->head = b->tail = b->cursor = NULL;
    if (!s || s[0] == '\0') {
        b->cursor = NULL; 
        return b;
    }
    const char *p = s;
    while (*p) {
        CharNode *n = (CharNode *)malloc(sizeof(CharNode));
        n->ch = *p;
        n->prev = b->tail;
        n->next = NULL;
        if (b->tail) b->tail->next = n;
        else b->head = n;
        b->tail = n;
        p++;
    }
    b->cursor = NULL; 
    return b;
}

static void buffer_free(Buffer *b) {
    if (!b) return;
    CharNode *it = b->head;
    while (it) {
        CharNode *nx = it->next;
        free(it);
        it = nx;
    }
    free(b);
}

static char *buffer_to_string(Buffer *b) {
    if (!b) return strdup("");
    
    size_t n = 0;
    for (CharNode *it = b->head; it; it = it->next) n++;
    char *out = (char *)malloc(n + 1);
    size_t i = 0;
    for (CharNode *it = b->head; it; it = it->next) out[i++] = it->ch;
    out[i] = '\0';
    return out;
}

static char *buffer_to_string_with_cursor(Buffer *b) {
    if (!b) return strdup("|");
    size_t n = 0;
    for (CharNode *it = b->head; it; it = it->next) n++;

    char *out = (char *)malloc(n + 2);
    size_t oi = 0;
    
    CharNode *it = b->head;
    while (it && it != b->cursor) {
        out[oi++] = it->ch;
        it = it->next;
    }
    
    out[oi++] = '|';
    
    while (it) {
        out[oi++] = it->ch;
        it = it->next;
    }
    out[oi] = '\0';
    return out;
}

// insert a string 
static void buffer_insert_string(Buffer *b, const char *s) {
    if (!b || !s) return;
    // insert each character 
    for (const char *p = s; *p; ++p) {
        CharNode *n = (CharNode *)malloc(sizeof(CharNode));
        n->ch = *p;
        if (b->cursor == NULL) {
            
            n->next = NULL;
            n->prev = b->tail;
            if (b->tail) b->tail->next = n;
            else b->head = n;
            b->tail = n;
        } else {
            // insert before cursor node
            n->next = b->cursor;
            n->prev = b->cursor->prev;
            if (b->cursor->prev) b->cursor->prev->next = n;
            else b->head = n;
            b->cursor->prev = n;
        }
    }
}


static int buffer_delete_before_cursor(Buffer *b) {
    if (!b) return 0;
    CharNode *del = NULL;
    if (b->cursor == NULL) del = b->tail;
    else del = b->cursor->prev;
    if (!del) return 0;
    if (del->prev) del->prev->next = del->next;
    else b->head = del->next;
    if (del->next) del->next->prev = del->prev;
    else b->tail = del->prev;
    free(del);
    return 1;
}

static void buffer_move_left(Buffer *b) {
    if (!b) return;
    if (b->cursor == b->head) {
        
        return;
    }
    if (b->cursor == NULL) {
        
        b->cursor = b->tail;
    } else {
        if (b->cursor->prev) b->cursor = b->cursor->prev;
    }
}

static void buffer_move_right(Buffer *b) {
    if (!b) return;
    if (b->cursor == NULL) {
        
        return;
    }
    b->cursor = b->cursor->next;
}



static char *get_current_content() {
    char *cur = read_whole_file(CURRENT_FILE);
    if (!cur) {
        // create empty current
        write_whole_file(CURRENT_FILE, "");
        cur = strdup("");
    }
    return cur;
}

int main() {
    ensure_dirs();
    // initialize in-memory undo/redo stacks
    memstack_init(&undo_stack);
    memstack_init(&redo_stack);
    int undo_count = 0, redo_count = 0;
    // read_meta kept for compatibility but we will prefer in-memory stacks
    read_meta(&undo_count, &redo_count);
    
    size_t cap = 1024;
    size_t len = 0;
    char *raw = (char *)malloc(cap + 1);
    if (!raw) return 0;
    int ch;
    while ((ch = fgetc(stdin)) != EOF) {
        if (len + 1 >= cap) {
            cap *= 2;
            char *tmp = (char *)realloc(raw, cap + 1);
            if (!tmp) { free(raw); return 0; }
            raw = tmp;
        }
        raw[len++] = (char)ch;
    }
    raw[len] = '\0';
    
    while (len > 0 && (raw[len-1] == '\n' || raw[len-1] == '\r')) raw[--len] = '\0';
    if (len == 0) { free(raw); return 0; }
    

    
    char *cur_forbuf = read_whole_file(CURRENT_FILE);
    Buffer *buf = buffer_create_from_string(cur_forbuf ? cur_forbuf : "");
    if (cur_forbuf) free(cur_forbuf);
    if (strcmp(raw, "undo") == 0) {
        char *prev = memstack_pop(&undo_stack);
        if (!prev) {
            printf("Nothing to undo!");
        } else {
            char *current = get_current_content();
            memstack_push(&redo_stack, current);
            write_whole_file(CURRENT_FILE, prev);
            buffer_free(buf);
            buf = buffer_create_from_string(prev);
            printf("%s", prev);
            free(prev);
        }
        write_meta(undo_stack.size, redo_stack.size);
    }
    else if (strncmp(raw, "search:", 7) == 0) {
        const char *pattern = raw + 7;
        char *current = get_current_content();
        avl_search_and_print(current, pattern);
        free(current);
        
    }
    else if (strncmp(raw, "insert:", 7) == 0) {
        const char *s = raw + 7;
        // push previous content to undo and clear redo
        char *prev = buffer_to_string(buf);
        memstack_push(&undo_stack, prev);
        free(prev);
        memstack_clear(&redo_stack);
        // insert at cursor
        buffer_insert_string(buf, s);
        char *out = buffer_to_string(buf);
        // update CURRENT_FILE
        write_whole_file(CURRENT_FILE, out);
    // buffer already updated by insert operations; keep cursor semantics
        printf("%s", out);
        free(out);
        write_meta(undo_stack.size, redo_stack.size);
    }
    else if (strcmp(raw, "delete") == 0) {
        // push previous content
        char *prev = buffer_to_string(buf);
        memstack_push(&undo_stack, prev);
        free(prev);
        memstack_clear(&redo_stack);

        int ok = buffer_delete_before_cursor(buf);
        char *out = buffer_to_string(buf);
        write_whole_file(CURRENT_FILE, out);
        if (ok) printf("%s", out);
        else printf("Nothing to delete");
        free(out);
        write_meta(undo_stack.size, redo_stack.size);
    }
    else if (strcmp(raw, "cursor:left") == 0) {
        buffer_move_left(buf);
        char *out = buffer_to_string_with_cursor(buf);
        printf("%s", out);
        free(out);
    }
    else if (strcmp(raw, "cursor:right") == 0) {
        buffer_move_right(buf);
        char *out = buffer_to_string_with_cursor(buf);
        printf("%s", out);
        free(out);
    }
    else if (strcmp(raw, "showcursor") == 0) {
        char *out = buffer_to_string_with_cursor(buf);
        printf("%s", out);
        free(out);
    }
    else if (strcmp(raw, "new") == 0) {
        char *current = get_current_content();
        memstack_push(&undo_stack, current);
        free(current);
        memstack_clear(&redo_stack);
        write_whole_file(CURRENT_FILE, "");
        buffer_free(buf);
        buf = buffer_create_from_string("");
        printf("");
        write_meta(undo_stack.size, redo_stack.size);
    }
    
    else if (strncmp(raw, "save:", 5) == 0) {
        
        const char *p = raw + 5;
        const char *sep = strstr(p, "::");
        char filename[512];
        const char *content;
        if (sep) {
            size_t fnlen = sep - p;
            if (fnlen >= sizeof(filename)) fnlen = sizeof(filename)-1;
            strncpy(filename, p, fnlen);
            filename[fnlen] = '\0';
            content = sep + 2;
        } else {
        
            strcpy(filename, "document.txt");
            content = NULL;
        }

    char *current = get_current_content();
    memstack_push(&undo_stack, current);
    memstack_clear(&redo_stack);

        char *new_content;
        if (content) new_content = strdup(content);
        else new_content = strdup(current);
        free(current);

        
        write_whole_file(CURRENT_FILE, new_content);
        buffer_free(buf);
        buf = buffer_create_from_string(new_content);

        if (!write_whole_file(filename, new_content)) {
            printf("Failed to save to %s", filename);
        } else {
            printf("Saved to %s.", filename);
        }
        free(new_content);
        write_meta(undo_stack.size, redo_stack.size);
    }

    else if (strncmp(raw, "replace:", 8) == 0) {
        // format: replace:old::new
        const char *p = raw + 8;
        const char *sep = strstr(p, "::");
        if (!sep) {
            printf("Invalid replace format. Use replace:old::new");
        } else {
            size_t oldlen = sep - p;
            char oldw[512];
            if (oldlen >= sizeof(oldw)) oldlen = sizeof(oldw)-1;
            strncpy(oldw, p, oldlen); oldw[oldlen] = '\0';
            const char *neww = sep + 2;

            char *current = get_current_content();
            // Build AVL (not strictly required for replacement mechanics here but per requirement)
            AVLNode *root = build_avl_from_text(current);
            // If word not present, print and cleanup
            // We can search by scanning the AVL (simple search function)
            // quick search by traversing tree
            int found = 0;
            AVLNode *it = root;
            while (it) {
                int c = strcmp(oldw, it->word);
                if (c == 0) { found = 1; break; }
                it = c < 0 ? it->left : it->right;
            }
            if (!found) {
                printf("Word not found!");
                free_tree(root);
                free(current);
            } else {
                // push previous state to undo and clear redo
                memstack_push(&undo_stack, current);
                memstack_clear(&redo_stack);

                char *newtxt = replace_whole_words(current, oldw, neww);
                write_whole_file(CURRENT_FILE, newtxt);
                buffer_free(buf);
                buf = buffer_create_from_string(newtxt);
                printf("%s", newtxt);

                free(newtxt);
                free_tree(root);
                free(current);
                write_meta(undo_stack.size, redo_stack.size);
            }
        }
    }

    else if (strcmp(raw, "redo") == 0) {
        char *next = memstack_pop(&redo_stack);
        if (!next) {
            printf("Nothing to redo!");
        } else {
            char *current = get_current_content();
            memstack_push(&undo_stack, current);
            write_whole_file(CURRENT_FILE, next);
            buffer_free(buf);
            buf = buffer_create_from_string(next);
            printf("%s", next);
            free(next);
        }
        write_meta(undo_stack.size, redo_stack.size);
    }
    
    else if (strcmp(raw, "show") == 0) {
        char *current = get_current_content();
        printf("%s", current);
        free(current);
    }
    else {
        printf("Invalid command.");
    }

    
    buffer_free(buf);
    memstack_free(&undo_stack);
    memstack_free(&redo_stack);
    free(raw);
    return 0;
}