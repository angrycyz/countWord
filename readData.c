#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include "uthash/src/uthash.h"

#define WORD_MAX_LEN 100
#define DEBUG

struct hashtable {
    char name[WORD_MAX_LEN];
    int count;
    UT_hash_handle hh;
};

struct linked_list {
    struct list_node *head;
    struct list_node *tail;
};

struct list_node {
    char name[WORD_MAX_LEN];
    struct list_node *next;
};

struct arg_read {
    FILE* input;
    FILE* file1;
};

struct arg_write {
    int thread_idx;
    struct hashtable *s;
};

void *writeToLinkedList(struct arg_read* arg1);
void list_append(struct linked_list *list, struct list_node *node);
struct list_node* list_pop(struct linked_list *list);
int list_empty(struct linked_list *list);
void *writeToHashTable(struct arg_write *arg2);

int list_empty(struct linked_list *list) {
    return list->head == NULL;
}

struct list_node* list_pop(struct linked_list *list) {
    if (list->head == NULL) {
        return NULL;
    } else {
        struct list_node *node = list->head;
        list->head = node->next;
        if (list->head == NULL){
            list->tail = NULL;
        }
        return node;
    }
}

void list_append(struct linked_list *list, struct list_node *node) {
    node->next = NULL;
    if (list->head == NULL) {
        list->head = list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }
}

pthread_mutex_t ll_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ht_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t eof_lock = PTHREAD_MUTEX_INITIALIZER;
struct hashtable *total[8];
struct linked_list list;
int eof_flag = 0;

// assume that input is valid
// assume no specific order is required for the result
// assume unique word number will not exceed memory
void readData(char* path, int thread_num) {
    
    FILE *input = fopen(path, "r");
    if (input == NULL) {
        printf("Cannot open file %s\n", path);
        exit(EXIT_FAILURE);
    }
    
    FILE *file1 = fopen("File1.txt", "w");
    if (file1 == NULL) {
        printf("Cannot open File1\n");
        exit(EXIT_FAILURE);
    }
    
    FILE *file2 = fopen("File2.txt", "w");
    if (file2 == NULL) {
        printf("Cannot open File2\n");
        exit(EXIT_FAILURE);
    }
    struct hashtable *s;
    struct arg_read *arg1 = (struct arg_read*)malloc(sizeof(struct arg_read));
    arg1->input = input;
    arg1->file1 = file1;
    
    /* use master thread to read data to linked-list and save data to file1*/
    writeToLinkedList(arg1);
    
    /* create threads which only retrieve data from linked-list and put data to hashtable*/
    int i = 0;
    pthread_t write_thread[thread_num - 1];
    for (; i < thread_num - 1; i++) {
        struct arg_write *arg2 = (struct arg_write*)malloc(sizeof(struct arg_write));
        arg2->s = s;
        arg2->thread_idx = i;
        if (pthread_create(&write_thread[i], NULL, &writeToHashTable, arg2) != 0) {
            fprintf(stderr, "error: Cannot create write thread # %d\n", i);
            break;
        }
    }
    
    /* wait for all child threads to be done */
    i = 0;
    for (; i < thread_num - 1; i++) {
        pthread_join(write_thread[i], NULL);
    }
    
    /* merge all table into one hashtable */
#ifdef DEBUG
    printf("Start merging hashtable\n");
#endif
    struct hashtable *s_, *tmp, *table = NULL;
    
    i = 0;
    for (; i < thread_num - 1; i++) {
        HASH_ITER(hh, total[i], s_, tmp) {
            HASH_FIND_STR(table, s_->name, s);
            if (s) {
                s->count += s_->count;
            } else {
                HASH_ADD_STR(table, name, s_);
            }
        }
    }

#ifdef DEBUG
    printf("Start iterating over hashtable\n");
#endif
    /* iterate over the hash table to write to file2 */
    HASH_ITER(hh, table, s, tmp) {
        fprintf(file2, "%s %d\n", s->name, s->count);
    }
    
    free(s);
    fclose(file2);
    
    pthread_mutex_destroy(&ll_lock);
    pthread_mutex_destroy(&ht_lock);
}

void *writeToLinkedList(struct arg_read* arg1) {
    
    int line_num = 1;
    int word_count = 1;
    char character;
    char word[WORD_MAX_LEN];
    int index = 0;
    
    while ((character = fgetc(arg1->input))) {
        if (character == '\n' || character == ' ' || character == '\0' || character == '\t' || character == EOF) {
            if (index == 0) {
                if (character == EOF) {
                    break;
                }
                continue;
            }
            /* write line number and word count to file1 */
            if (character == '\n') {
                fprintf(arg1->file1, "%d %d\n", line_num, word_count);
                line_num++;
            }
            int i = 0;
            
            /* convert to lowercase */
            while (i < WORD_MAX_LEN && word[i] != '\0') {
                word[i] = tolower(word[i]);
                i++;
            }
            
            struct list_node *node = (struct list_node*)malloc(sizeof(struct list_node));
            strncpy(node->name, word, WORD_MAX_LEN);
            
            pthread_mutex_lock(&ll_lock);
            list_append(&list, node);
            pthread_mutex_unlock(&ll_lock);
            
            word_count++;
            word[0] = 0;
            index = 0;
        } else {
            word[index++] = character;
            word[index] = 0;
        }
    }
    
    pthread_mutex_lock(&eof_lock);
    eof_flag = 1;
    pthread_mutex_unlock(&eof_lock);
    
    fclose(arg1->input);
    fclose(arg1->file1);
    
    return 0;
}

void *writeToHashTable(struct arg_write *arg2) {
#ifdef DEBUG
    printf("Thread enters: %d\n", arg2->thread_idx);
#endif
//    struct hashtable *s;
    while (1) {
        pthread_mutex_lock(&eof_lock);
        pthread_mutex_lock(&ll_lock);
        if (eof_flag == 1 && list_empty(&list)) {
            pthread_mutex_unlock(&ll_lock);
            pthread_mutex_unlock(&eof_lock);
            break;
        }
//        if (list_empty(&list)) {
//            pthread_mutex_unlock(&ll_lock);
//            pthread_mutex_unlock(&eof_lock);
//            usleep(5000);
//            continue;
//        }
        struct list_node *node = list_pop(&list);
        pthread_mutex_unlock(&ll_lock);
        pthread_mutex_unlock(&eof_lock);
        
        char word[WORD_MAX_LEN];
        strncpy(word, node->name, WORD_MAX_LEN);
        
        /* HASH_FIND_STR find the char array, if not found, it will free s */
        HASH_FIND_STR(total[arg2->thread_idx], word, arg2->s);
        if (arg2->s) {
            arg2->s->count++;
        } else {
            arg2->s = (struct hashtable*)malloc(sizeof(struct hashtable));
            arg2->s->count = 1;
            strncpy(arg2->s->name, word, WORD_MAX_LEN);
            
            HASH_ADD_STR(total[arg2->thread_idx], name, arg2->s);
        }
    }
#ifdef DEBUG
//    struct hashtable *tmp;
//    HASH_ITER(hh, total[arg2->thread_idx], arg2->s, tmp) {
//        printf("thread: %d, iterate: %s\n, count: %d\n", arg2->thread_idx, arg2->s->name, arg2->s->count);
//    }
    printf("Thread exits: %d\n", arg2->thread_idx);
#endif
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        perror("Invalid argument");
    }
    struct timeval tv1, tv2;
    
    long int core_num = sysconf(_SC_NPROCESSORS_ONLN);
    core_num = 2;

    printf("Online core number: %ld\n", core_num);
    
    gettimeofday(&tv1, NULL);
    
    readData(argv[1], core_num);
    
    gettimeofday(&tv2, NULL);
    printf ("Execution time = %f seconds\n",
            (double) (tv2.tv_usec - tv1.tv_usec) / 1000000 +
            (double) (tv2.tv_sec - tv1.tv_sec));
    return 0;
}
