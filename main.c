
// @AUTHOR: CYRUS MAJD AND KAYLA KAM
// JENSON-SHANNON DISTANCE MEASURE OF SOFTWARE SIMILARITY (MOSS) TOOL

// Dependencies
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <pthread.h>
#include <sys/queue.h>
#include <ctype.h>
#include <math.h>

enum {
    WALK_OK = 0,
    WALK_BADPATTERN,
    WALK_NAMETOOLONG,
    WALK_BADIO,
};

// Define macros
#define WS_NONE		0
#define WS_RECURSIVE	(1 << 0)
#define WS_DEFAULT	WS_RECURSIVE
#define WS_FOLLOWLINK	(1 << 1)	/* follow symlinks */
#define WS_DOTFILES	(1 << 2)	/* per unix convention, .file is hidden */
#define WS_MATCHDIRS	(1 << 3)	/* if pattern is used on dir names too */
#define QUEUESIZE 1000
#define STRINGSIZE 1000
#define REPOSITORYSIZE 1000
#define DEBUG_QUEUETEST 0
#define DEBUG_LLTEST 0
#define DEBUG_WFD 0
#define DEBUG 0
#define DEBUG_JSD 0
#define COMBINATIONGENERATOR 1
#define PRODUCTIONTEST 1

// Queue struct
struct queue {
    char data[QUEUESIZE][STRINGSIZE];
    unsigned head;  // index of first item in queue
    unsigned count;  // number of items in queue
    pthread_mutex_t lock;
    pthread_cond_t read_ready;  // wait for count > 0
    pthread_cond_t write_ready; // wait for count < QUEUESIZE
};

// WFDrepository struct
struct WFDrepository {
    struct Node * data[REPOSITORYSIZE];
    char fileNames[REPOSITORYSIZE][STRINGSIZE];
    unsigned head;  // index of first item in queue
    unsigned count;  // number of items in queue
    pthread_mutex_t lock;
    pthread_cond_t read_ready;  // wait for count > 0
    pthread_cond_t write_ready; // wait for count < REPOSITORYSIZE
};

// Linked List struct
struct Node {
    char data[100];
    long long wordCount;
    double frequency;
    struct Node* next;
};

// Method headers
// Basic utility helper methods
int walk_recur(char *dname, regex_t *reg, int spec, struct queue *Q);
int walk_dir(char *dname, char *pattern, int spec, struct queue *Q);
int traverseMain(struct queue *Q);
int queue_init(struct queue *Q);
int queue_add(struct queue *Q, char * item);
int queue_remove(struct queue *Q, char *item);
void queuePrint(struct queue *Q);
void destroyList(struct Node *head);
void sortedInsert(struct Node**, struct Node*);
void insertionSort(struct Node **head_ref);
void printList(struct Node *head);
void push(struct Node** head_ref, char* new_data, struct Node* head, int totalNumberOfWords);

// WFD Helper methods
struct Node * findWords(char *fileName, struct Node *WFD_LL, int totalNumberOfWords);
int findNumberOfWords(char * fileName);
void incrementWordCount(struct Node* head, char* new_data);
int elementExistsInLL(struct Node* head, char* new_data);
struct Node * WFDmain(char* fileName, struct Node *WFD_LL);
void calculateFrequency(struct Node* head, int totalNumberOfWords);
int WFDqueueinit(struct WFDrepository *Q);
int WFDqueue_add(struct WFDrepository *Q, struct Node * item, char * fileName);
int WFDqueue_remove(struct WFDrepository *Q, struct Node * item);
void WFDqueue_print(struct WFDrepository *Q);

// JSD Helper methods
double average(double frequencyOne, double frequencyTwo, int zeroFlag);
void traverseWordlist(struct Node *head);
double calculateKLDSection(double numerator, double denominator);
double calculateJSDValue(double KLD_1, double KLD_2);
int JSDhelper(struct Node *WFD_LL_1, struct Node *WFD_LL_2, char * file1, char * file2);
int JSDmain(char * file1, char * file2, struct Node * WFD_LL_1, struct Node * WFD_LL_2);

// ------------------------------- FILE TRAVERSAL HELPERS -------------------------------

int walk_recur(char *dname, regex_t *reg, int spec, struct queue *Q) {
    struct dirent *dent;
    DIR *dir;
    struct stat st;
    char fn[FILENAME_MAX];
    int res = WALK_OK;
    int len = strlen(dname);
    if (len >= FILENAME_MAX - 1)
        return WALK_NAMETOOLONG;

    strcpy(fn, dname);
    fn[len++] = '/';

    if (!(dir = opendir(dname))) {
        warn("can't open %s", dname);
        return WALK_BADIO;
    }

    errno = 0;
    while ((dent = readdir(dir))) {
        if (!(spec & WS_DOTFILES) && dent->d_name[0] == '.')
            continue;
        if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, ".."))
            continue;

        strncpy(fn + len, dent->d_name, FILENAME_MAX - len);
        if (lstat(fn, &st) == -1) {
            warn("Can't stat %s", fn);
            res = WALK_BADIO;
            continue;
        }

        /* don't follow symlink unless told so */
        if (S_ISLNK(st.st_mode) && !(spec & WS_FOLLOWLINK))
            continue;

        /* will be false for symlinked dirs */
        if (S_ISDIR(st.st_mode)) {
            /* recursively follow dirs */
            if ((spec & WS_RECURSIVE))
                walk_recur(fn, reg, spec, Q);

            if (!(spec & WS_MATCHDIRS)) continue;
        }

        /* pattern match */
        if (!regexec(reg, fn, 0, 0, 0)) {
//            puts(fn);
            queue_add(Q, fn);
        }
    }

    if (dir) closedir(dir);
    return res ? res : errno ? WALK_BADIO : WALK_OK;
}

int walk_dir(char *dname, char *pattern, int spec, struct queue *Q) {
    regex_t r;
    int res;
    if (regcomp(&r, pattern, REG_EXTENDED | REG_NOSUB))
        return WALK_BADPATTERN;
    res = walk_recur(dname, &r, spec, Q);
    regfree(&r);

    return res;
}

int traverseMain(struct queue *Q) {
    int r = walk_dir("test", ".\\.txt$", WS_DEFAULT|WS_MATCHDIRS, Q);
    switch(r) {
        case WALK_OK:		break;
        case WALK_BADIO:	err(1, "IO error");
        case WALK_BADPATTERN:	err(1, "Bad pattern");
        case WALK_NAMETOOLONG:	err(1, "Filename too long");
        default:
            err(1, "Unknown error?");
    }
    return 0;
}

// ------------------------------- END OF FILE TRAVERSAL HELPERS -------------------------------

// ------------------------------- QUEUE STRUCTURE -------------------------------

int queue_init(struct queue *Q)
{
    Q->head = 0;
    Q->count = 0;
    int i = pthread_mutex_init(&Q->lock, NULL);
    int j = pthread_cond_init(&Q->read_ready, NULL);
    int k = pthread_cond_init(&Q->write_ready, NULL);

    if (i != 0 || j != 0 || k != 0) {
        return EXIT_FAILURE;  // obtained from the init functions (code omitted)
    }

    return EXIT_SUCCESS;
}

int queue_add(struct queue *Q, char * item)
{
    pthread_mutex_lock(&Q->lock); // make sure no one else touches Q until we're done

    while (Q->count == QUEUESIZE) {
        // wait for another thread to dequeue
        pthread_cond_wait(&Q->write_ready, &Q->lock);
        // release lock & wait for a thread to signal write_ready
    }

    // at this point, we hold the lock & Q->count < QUEUESIZE

    unsigned index = Q->head + Q->count;
    if (index >= QUEUESIZE) index -= QUEUESIZE;

    strcpy(Q->data[index], item);
    ++Q->count;

    pthread_mutex_unlock(&Q->lock); // now we're done
    pthread_cond_signal(&Q->read_ready); // wake up a thread waiting to read (if any)

    return 0;
}

int queue_remove(struct queue *Q, char *item)
{
    pthread_mutex_lock(&Q->lock);

    while (Q->count == 0) {
        pthread_cond_wait(&Q->read_ready, &Q->lock);
    }

    // now we have exclusive access and queue is non-empty

    item = Q->data[Q->head];  // write value at head to pointer
    --Q->count;
    ++Q->head;
    if (Q->head == QUEUESIZE) Q->head = 0;

    pthread_mutex_unlock(&Q->lock);

    pthread_cond_signal(&Q->write_ready);

    return EXIT_SUCCESS;
}

void queuePrint(struct queue *Q) {
    int count = Q->count;
    for (int i = 0; i < count; i++) {
        printf("Value at %d is %s\n", i, Q->data[i]);
    }
}

// ------------------------------- END OF QUEUE STRUCTURE -------------------------------

// ------------------------------- WFD REPOSITORY QUEUE STRUCTURE -------------------------------

int WFDqueueinit(struct WFDrepository *Q)
{
    Q->head = 0;
    Q->count = 0;
    int i = pthread_mutex_init(&Q->lock, NULL);
    int j = pthread_cond_init(&Q->read_ready, NULL);
    int k = pthread_cond_init(&Q->write_ready, NULL);

    if (i != 0 || j != 0 || k != 0) {
        return EXIT_FAILURE;  // obtained from the init functions (code omitted)
    }

    return EXIT_SUCCESS;
}

int WFDqueue_add(struct WFDrepository *Q, struct Node * item, char * fileName)
{
    pthread_mutex_lock(&Q->lock); // make sure no one else touches Q until we're done

    while (Q->count == QUEUESIZE) {
        // wait for another thread to dequeue
        pthread_cond_wait(&Q->write_ready, &Q->lock);
        // release lock & wait for a thread to signal write_ready
    }

    // at this point, we hold the lock & Q->count < QUEUESIZE

    unsigned index = Q->head + Q->count;
    if (index >= QUEUESIZE) index -= QUEUESIZE;

    Q->data[index] = item;
    strcpy(Q->fileNames[index], fileName);
    ++Q->count;

    pthread_mutex_unlock(&Q->lock); // now we're done
    pthread_cond_signal(&Q->read_ready); // wake up a thread waiting to read (if any)

    return 0;
}

int WFDqueue_remove(struct WFDrepository *Q, struct Node * item)
{
    destroyList(Q->data[Q->count]);

    pthread_mutex_lock(&Q->lock);

    while (Q->count == 0) {
        pthread_cond_wait(&Q->read_ready, &Q->lock);
    }

    // now we have exclusive access and queue is non-empty

    item = Q->data[Q->head];  // write value at head to pointer
    --Q->count;
    ++Q->head;
    if (Q->head == QUEUESIZE) Q->head = 0;

    pthread_mutex_unlock(&Q->lock);

    pthread_cond_signal(&Q->write_ready);

    return EXIT_SUCCESS;
}

void WFDqueue_print(struct WFDrepository *Q) {
    int count = Q->count;
    for (int i = 0; i < count; i++) {
        printf("LOOKING AT FILE: %s\n", Q->fileNames[i]);
        printList(Q->data[i]);
        printf("\n");
    }
}

// ------------------------------- END OF WFD REPOSITORY QUEUE STRUCTURE -------------------------------

// ------------------------------- WFD LOCAL LL -------------------------------

void destroyList(struct Node *head) {
    struct Node *curr = head;
    while(curr != NULL)
    {
        struct Node *tmp = curr;
        curr = curr->next;
        free(tmp);
    }
}

// function to sort a singly linked list using insertion sort
void insertionSort(struct Node **head_ref)
{
    // Initialize sorted linked list
    struct Node *sorted = NULL;

    // Traverse the given linked list and insert every
    // node to sorted
    struct Node *current = *head_ref;
    while (current != NULL)
    {
        // Store next for next iteration
        struct Node *next = current->next;

        // insert current in sorted linked list
        sortedInsert(&sorted, current);

        // Update current
        current = next;
    }

    // Update head_ref to point to sorted linked list
    *head_ref = sorted;
}

// Function to insert a given node in a sorted linked list
/* function to insert a new_node in a list. Note that this
  function expects a pointer to head_ref as this can modify the
  head of the input linked list (similar to push())*/
void sortedInsert(struct Node** head_ref, struct Node* new_node)
{
    struct Node* current;
    /* Special case for the head end */
    // (*head_ref)->data >= new_node->data
    if (*head_ref == NULL || strcmp((*head_ref)->data, new_node->data) >= 0)
    {
        new_node->next = *head_ref;
        *head_ref = new_node;
    }
    else
    {
        /* Locate the node before the point of insertion */
        current = *head_ref;
        while (current->next!=NULL &&
               //current->next->data < new_node->data)
               strcmp(current->next->data, new_node->data) < 0)
        {
            current = current->next;
        }
        new_node->next = current->next;
        current->next = new_node;
    }
}

/* BELOW FUNCTIONS ARE JUST UTILITY TO TEST sortedInsert */

/* Function to print linked list */
void printList(struct Node *head)
{
    struct Node *temp = head;
    while(temp != NULL)
    {
        printf("WORD: %s\t\tWORD COUNT: %lld\tFREQUENCY: %f\n", temp->data, temp->wordCount, temp->frequency);
        temp = temp->next;
    }
}

int elementExistsInLL(struct Node* head, char* new_data) {
//    printf(" checking if %s exists in list... ", new_data);
//    printf("THE CURRENT ELEMENTS IN THE LIST: \n");
//    printList(head);
//    printf("------------\n");
    struct Node *temp = head;
    while(temp != NULL)
    {
//        printf(" .. COMPARING %s to %s .. ", temp->data, new_data);
        if (strcmp(temp->data, new_data) == 0) {
//            printf("%s EXISTS! \n", new_data);
            return 1;
        }
        temp = temp->next;
    }
//    printf("%s DOES NOT EXIST! \n", new_data);
    return 0;
}

void incrementWordCount(struct Node* head, char* new_data) {
    struct Node *temp = head;
    while(temp != NULL)
    {
        if (strcmp(temp->data, new_data) == 0) {
            temp->wordCount = temp->wordCount + 1;
//            printf("WC: %lld, WF: %f", temp->wordCount, temp->frequency);
        }
        temp = temp->next;
    }
}

void calculateFrequency(struct Node* head, int totalNumberOfWords) {
    struct Node *temp = head;
    while(temp != NULL)
    {
//        printf(" -CALCULATING FREQUENCY!- WORDCNT: %lld, TOTAL: %d",temp->wordCount, totalNumberOfWords);
        temp->frequency = (double)temp->wordCount / (double)totalNumberOfWords;
//        printf(", FREQUENCY: %f", temp->frequency);
        temp = temp->next;
    }
}

/* A utility function to insert a node at the beginning of linked list */
void push(struct Node** head_ref, char* new_data, struct Node* head, int totalNumberOfWords)
{
    // if the element already exists, just increment the value of wordcount. otherwise add the node.
    if (elementExistsInLL(head, new_data)) {
        incrementWordCount(head, new_data);
//        printf("INCREMENT!");
        calculateFrequency(head, totalNumberOfWords);
    }
    else {
        /* allocate node */
        struct Node* new_node = malloc(sizeof(struct Node));

        /* put in the data  */
        strcpy(new_node->data, new_data);

        // init wordCount to 1
        new_node->wordCount = 1;

        // init frequency
        new_node->frequency = (double) new_node->wordCount / (double) totalNumberOfWords;

        /* link the old list off the new node */
        new_node->next = (*head_ref);

        /* move the head to point to the new node */
        (*head_ref)    = new_node;
    }
}

// ------------------------------- END OF WFD LOCAL LL -------------------------------

// ------------------------------- WORD FREQUENCY ALGORITHM -------------------------------

struct Node * findWords(char *fileName, struct Node *WFD_LL, int totalNumberOfWords) {
    char word[100] = "";
    int endOfWordIndex = 0;
    size_t nbytes;
    ssize_t bytes_read;
    int fd;
    FILE *fp;
    char ch;
    int ENDWORDFLAG = 0;

    fp = fopen(fileName, "r");

    nbytes = 1;
    char letter[2];
    bytes_read = read(fd, letter, nbytes);

    while((ch = getc(fp)) != EOF) {
        if (isalnum(ch) || ch == '-') {
            ch = tolower(ch);
//            printf("%c ", ch);
            if (ENDWORDFLAG) { //new word
//                printf("%s\n", word);
                push(&WFD_LL, word, WFD_LL, totalNumberOfWords);
                endOfWordIndex = 0;
                ENDWORDFLAG = 0;
                strcpy(word, "");

                char tmp[2];
                tmp[0] = ch;
                tmp[1] = '\0';
                strcat(word, tmp);
                endOfWordIndex++;
            }
            else { //adding to a word
                char tmp[2];
                tmp[0] = ch;
                tmp[1] = '\0';
                strcat(word, tmp);
                endOfWordIndex++;
            }
        }
        else if (ch == '\''){
            continue;
        }
        else {
//            printf("!FLAG SET! ");
            ENDWORDFLAG = 1;
        }
    }
//    printf("%s\n", word);
    push(&WFD_LL, word, WFD_LL, totalNumberOfWords);
    fclose(fp);

//    insertionSort(&WFD_LL);
//    printf("================\n");
//    printList(WFD_LL);
    return WFD_LL;
//    destroyList(WFD_LL);
}

int findNumberOfWords(char * fileName) {
    char word[100] = "";
    int endOfWordIndex = 0;
    size_t nbytes;
    ssize_t bytes_read;
    int fd;
    FILE *fp;
    char ch;
    int ENDWORDFLAG = 0;
    int totalNumberOfWords = 0;

    fp = fopen(fileName, "r");

    nbytes = 1;
    char letter[2];
    bytes_read = read(fd, letter, nbytes);

    while((ch = getc(fp)) != EOF) {
        if (isalnum(ch) || ch == '-') {
            ch = tolower(ch);
//            printf("%c ", ch);
            if (ENDWORDFLAG) { //new word
                totalNumberOfWords++;
                endOfWordIndex = 0;
                ENDWORDFLAG = 0;
                strcpy(word, "");

                char tmp[2];
                tmp[0] = ch;
                tmp[1] = '\0';
                strcat(word, tmp);
                endOfWordIndex++;
            }
            else { //adding to a word
                char tmp[2];
                tmp[0] = ch;
                tmp[1] = '\0';
                strcat(word, tmp);
                endOfWordIndex++;
            }
        }
        else if (ch == '\''){
            continue;
        }
        else {
//            printf("!FLAG SET! ");
            ENDWORDFLAG = 1;
        }
    }
    totalNumberOfWords++;

    fclose(fp);

    return totalNumberOfWords;
}

struct Node * WFDmain(char* fileName, struct Node *WFD_LL) {
//    FILE *fp;
//    fp = fopen(fileName, "r");
//    fclose(fp);

    int meme = strcmp("2292992", fileName);
    printf("Reading file: %d \t\t. . . OK!\n", meme);
//    sleep(1);
//    printf("\t||NOW READING: %d||\n", meme);
//    fflush(stdout);
    char * tmp = fileName;

    // steps to WFD classify:
    //  1) obtain text from file.
    //  2) iterate through text character by character, make all lowercase. if space, set spaceFlag.
    //  3) so long as spaceFlag is not set, concat each read character into a word.
    //  4) print word (debugging)

    // returns total number of words in the file
    int totalNumberOfWords = findNumberOfWords(tmp);

//    printf(" ");
//    printf("\t||total number of words: %d||\n", totalNumberOfWords);

    // appends words to the linkedList of word frequencies
    return findWords(fileName, WFD_LL, totalNumberOfWords);

}

// ------------------------------- END OF WORD FREQUENCY ALGORITHM -------------------------------

// ------------------------------- JSD ALGORITHM -------------------------------

// calculates average of two doubles. zeroflag is set when the word is not found in both lists.
double average(double frequencyOne, double frequencyTwo, int zeroFlag) {
    if (zeroFlag) {
        return frequencyOne / (double) 2;
    }
    else {
        return (frequencyOne + frequencyTwo) / (double) 2;
    }
}

void traverseWordlist(struct Node *head) {
    struct Node *temp = head;
    while(temp != NULL)
    {
        printf("WORD: %s\t\tWORD COUNT: %lld\tFREQUENCY: %f\n", temp->data, temp->wordCount, temp->frequency);
        temp = temp->next;
    }
}

double calculateKLDSection(double numerator, double denominator) {
    double ratio = numerator/denominator;
    double logged = log2(ratio);
    double scaled = numerator * logged;
    return scaled;
}

double calculateJSDValue(double KLD_1, double KLD_2) {
    double leftPart = 0.5 * KLD_1;
    double rightPart = 0.5 * KLD_2;
    double sum = leftPart + rightPart;
    return sqrt(sum);
}

// calculates JSD between two files
int JSDhelper(struct Node *WFD_LL_1, struct Node *WFD_LL_2, char * file1, char * file2) {
//    printList(WFD_LL_1);
//    printf("\n");
//    printList(WFD_LL_2);

    double KLD_1 = 0.0;
    struct Node *temp = WFD_LL_1;
    while(temp != NULL) {
        char * currWord = temp->data;
        double temp2Frequency = 0.0;
        struct Node *temp2 = WFD_LL_2;
        double wordAverage = 0.0;
//        printf("FILE 1: WORD: %s\t\tWORD COUNT: %lld\tFREQUENCY: %f\n", temp->data, temp->wordCount, temp->frequency);
        while (temp2 != NULL) {
//            printf("FILE 2: WORD: %s\t\tWORD COUNT: %lld\tFREQUENCY: %f\n", temp2->data, temp2->wordCount, temp2->frequency);
            if (strcmp(currWord, temp2->data) == 0) {
//                printf("MATCH FOUND!\n");
                temp2Frequency = temp2->frequency;
                temp2 = NULL;
            }
            else {
                temp2 = temp2->next;
            }
        }
        if (temp2Frequency == 0.0) {
            wordAverage = average(temp->frequency, temp2Frequency, 1);
        }
        else {
            wordAverage = average(temp->frequency, temp2Frequency, 0);
        }
//        printf("AVERAGE FREQUENCY FOR WORD \"%s\" IN BOTH FILES IS %f. ACTUAL FREQUENCY: %f\n", temp->data, wordAverage, temp->frequency);
        double KLDsection = calculateKLDSection(temp->frequency, wordAverage);
        KLD_1 = KLD_1 + KLDsection;
        temp = temp->next;
    }

//    printf("\nKLD RESULT: %f\n", KLD_1);

//    printf("\n===================================\n\n");

    double KLD_2 = 0.0;
    struct Node *temp2b = WFD_LL_2;
    while(temp2b != NULL) {
        char * currWord = temp2b->data;
        double temp2Frequency = 0.0;
        struct Node *temp2 = WFD_LL_1;
        double wordAverage = 0.0;
//        printf("FILE 2: WORD: %s\t\tWORD COUNT: %lld\tFREQUENCY: %f\n", temp2b->data, temp2b->wordCount, temp2b->frequency);
        while (temp2 != NULL) {
//            printf("FILE 1: WORD: %s\t\tWORD COUNT: %lld\tFREQUENCY: %f\n", temp2->data, temp2->wordCount, temp2->frequency);
            if (strcmp(currWord, temp2->data) == 0) {
//                printf("MATCH FOUND!\n");
                temp2Frequency = temp2->frequency;
                temp2 = NULL;
            }
            else {
                temp2 = temp2->next;
            }
        }
        if (temp2Frequency == 0.0) {
            wordAverage = average(temp2b->frequency, temp2Frequency, 1);
        }
        else {
            wordAverage = average(temp2b->frequency, temp2Frequency, 0);
        }
//        printf("AVERAGE FREQUENCY FOR WORD \"%s\" IN BOTH FILES IS %f. ACTUAL FREQUENCY: %f\n", temp2b->data, wordAverage, temp2b->frequency);
        double KLDsection = calculateKLDSection(temp2b->frequency, wordAverage);
        KLD_2 = KLD_2 + KLDsection;
        temp2b = temp2b->next;
    }

//    printf("\nKLD RESULT: %f\n", KLD_2);

    //now that we have both KLDs stored in KLD_1 and KLD_2, we can calculate and return the JSD value.
    double JSD = calculateJSDValue(KLD_1, KLD_2);

    printf("%f %s %s\n", JSD, file1, file2);
}

int JSDmain(char * file1, char * file2, struct Node * WFD_LL_1, struct Node * WFD_LL_2) {

    JSDhelper(WFD_LL_1, WFD_LL_2, file1, file2);
//        printList(WFD_LL_1);
//        printf("\n");
//        printList(WFD_LL_2);

    return EXIT_SUCCESS;
}

// ------------------------------- END OF JSD ALGORITHM -------------------------------

int main() {

    // Queue
    // WARNING: fixed size queue, probably gonna bite me in the ass for big test cases, but look i got bigger problems right now.
    // please dont make directories more than ten thousand characters long :)
    if (PRODUCTIONTEST) {

        struct queue Q;
        queue_init(&Q);
        if (DEBUG_QUEUETEST) {
            queue_add(&Q, "69");
            queue_add(&Q, "1337");
            queue_add(&Q, "420");
            queue_add(&Q, "666");

            queuePrint(&Q);

            char* element = "66";
            queue_remove(&Q, element);

            queuePrint(&Q);
        }

        // Find all text files, bfs search.
        // WARNING: currently search directory is set to test/. Change this in final production.
        traverseMain(&Q);
        if (DEBUG) queuePrint(&Q);

        struct WFDrepository repo;
        WFDqueueinit(&repo);

        // READ IN THE QUEUE ONE ELEMENT AT A TIME (NOT MULTITHREADED) AND COMPUTE WFD
        int count = Q.count;
        for (int i = 0; i < count; i++) {
            struct Node *WFD_LL = NULL;
            WFD_LL = WFDmain(Q.data[i], WFD_LL);
            insertionSort(&WFD_LL);
//        printList(WFD_LL);
            WFDqueue_add(&repo, WFD_LL, Q.data[i]);
//        destroyList(WFD_LL);
//        printf("\n===================================================\n\n");
        }

//        WFDqueue_print(&repo);

        if (COMBINATIONGENERATOR) {
            if (DEBUG) {
                for (int i = 0; i < repo.count; i++) {
                    printf("%s\n", repo.fileNames[i]);
                }
            }

            printf("\n");

            int comboCounter = 0;
            int comboStart = 0;
            for (int i = 0; i < repo.count - 1; i++) {
                comboCounter = comboStart + 1;
//                printf("FILENAME: %s\n", repo.fileNames[i]);
                while (comboCounter < repo.count) {
//                    printf("\t compare to %s\n", repo.fileNames[comboCounter]);
                    JSDmain(repo.fileNames[i], repo.fileNames[comboCounter], repo.data[i], repo.data[comboCounter]);
                    comboCounter++;
                }
                comboStart++;
            }
        }

        // Clean up WFD repository
        int WFDdestroyer = repo.count;
        while(WFDdestroyer >= 0) {
            destroyList(repo.data[WFDdestroyer]);
            WFDdestroyer--;
        }
    }


    if (DEBUG_JSD) {
        char file1[100] = "test/jsdTest1.txt";
        char file2[100] = "test/jsdTest2.txt";

        struct Node *WFD_LL_1 = NULL;
        WFD_LL_1 = WFDmain(file1, WFD_LL_1);

        insertionSort(&WFD_LL_1);

        struct Node * WFD_LL_2 = NULL;
        WFD_LL_2 = WFDmain(file2, WFD_LL_2);

        insertionSort(&WFD_LL_2);

        JSDhelper(WFD_LL_1, WFD_LL_2, file1, file2);
//        printList(WFD_LL_1);
//        printf("\n");
//        printList(WFD_LL_2);

        destroyList(WFD_LL_1);
        destroyList(WFD_LL_2);
    }

    // DEBUG WFD
    if (DEBUG_WFD) {
        struct Node *WFD_LL = NULL;
        WFD_LL = WFDmain("test/textFile1.txt", WFD_LL);

        printList(WFD_LL);
        insertionSort(&WFD_LL);
        printf("================\n");
        printList(WFD_LL);
        destroyList(WFD_LL);
    }

    // Playing around with how a insertionsort linked list works
    if (DEBUG_LLTEST) {
        struct Node *a = NULL;
        push(&a, "apple", a, 5);
        push(&a, "zee", a, 5);
        push(&a, "dad", a,5);
        push(&a, "cow", a,5);
        push(&a, "zzz", a,5);
        push(&a, "zee", a,5);

        printf("Linked List before sorting \n");
        printList(a);

        insertionSort(&a);

        printf("\nLinked List after sorting \n");
        printList(a);

        destroyList(a);
    }

}