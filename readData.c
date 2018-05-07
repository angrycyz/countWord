#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include "uthash/src/uthash.h"

#define WORD_MAX_LEN 100
//#define lock_uthash

struct timeval tv1, tv2;

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

void writeToLinkedList(char* word, struct linked_list* list);
void list_append(struct linked_list *list, struct list_node *node);
int list_empty(struct linked_list *list);

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

// assume that input is valid
// assume no specific order is required for the result
void readData(char* path) {

#ifdef lock_uthash
    pthread_rwlock_t lock;
    if (pthread_rwlock_init(&lock,NULL) != 0) {
        printf("can't create rwlock");
        exit(EXIT_FAILURE);
    }
#endif
    
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
    
    struct hashtable *s, *tmp, *table = NULL;
    struct linked_list list;
    
    int line_num = 1;
    int word_count = 1;
    char character;
    char word[WORD_MAX_LEN];
    int index = 0;
    
    while ((character = fgetc(input)) != EOF) {
        
        if (character == '\n' || character == ' ' || character == '\0' || character == '\t') {

            /* write line number and word count to file1 */
            if (character == '\n') {
                fprintf(file1, "%d %d\n", line_num, word_count);
                line_num++;
            }
            int i = 0;
            
            /* convert to lowercase */
            while (i < WORD_MAX_LEN && word[i] != '\0') {
                word[i] = tolower(word[i]);
                i++;
            }

            writeToLinkedList(word, &list);
            
            /* HASH_FIND_STR find the char array, if not found, it will free the s */
#ifdef lock_uthash
            if (pthread_rwlock_rdlock(&lock) != 0) {
                printf("can't get rdlock");
                exit(EXIT_FAILURE);
            }
            HASH_FIND_STR(table, word, s);
            pthread_rwlock_unlock(&lock);
#endif
            if (s) {
                s->count++;
            } else {
                s = (struct hashtable*)malloc(sizeof(struct hashtable));
                s->count = 1;
                strncpy(s->name, word, WORD_MAX_LEN);
#ifdef lock_uthash
                if (pthread_rwlock_wrlock(&lock) != 0) {
                    printf("can't get wrlock");
                    exit(EXIT_FAILURE);
                }
                HASH_ADD_STR(table, name, s);
                pthread_rwlock_unlock(&lock);
#endif
            }
            word_count++;
            word[0] = 0;
            index = 0;
        } else {
            word[index++] = character;
            word[index] = 0;
        }
    }
    
    /* iterate over the hash table to write to file2 */
    HASH_ITER(hh, table, s, tmp) {
        fprintf(file2, "%s %d\n", s->name, s->count);
    }
    
    free(s);
    fclose(input);
    fclose(file1);
    fclose(file2);

#ifdef lock_uthash
    pthread_rwlock_destroy(&lock);
#endif
}

void writeToLinkedList(char word[], struct linked_list* list){
    struct list_node *node = (struct list_node*)malloc(sizeof(struct list_node));
    strncpy(node->name, word, WORD_MAX_LEN);
    list_append(list, node);
}
/*
void getFromLinkedList(){
 
}
*/
int main(int argc, char *argv[]) {
    if (argc != 2) {
        perror("Invalid argument");
    }
    
    long int core_num = sysconf(_SC_NPROCESSORS_ONLN);
    
    printf("Online core number: %ld\n", core_num);
    
    gettimeofday(&tv1, NULL);
    readData(argv[1]);
    gettimeofday(&tv2, NULL);
    printf ("Execution time = %f seconds\n",
            (double) (tv2.tv_usec - tv1.tv_usec) / 1000000 +
            (double) (tv2.tv_sec - tv1.tv_sec));
    
    return 0;
}
