#include "csapp.h"
#include "cache.h"
#include <string.h>

/**  
 * @author Wei-Lin Tsai weilints@andrew.cmu.edu
 * 
 * @note
 *      1. Use reader/writer approach to implement cache 
 *      The same concept as text-book present 
 *      2. Use link-list for dynamic sized cache 
 **/

/** Static global variable */
static int read_cnt;            /// number of current reader 
static sem_t r_mutex, w_mutex;  /// mutex for read/writer permission

/** Static helper function */

/* @brief 
 *      increase all item's age
 * @note 
 *      it's not thread safe, so the semaphore lock/unlock 
 *      must be controlled by its caller
 * @param 
 *      cp: pointer to cache_t
 */
static void ages(cache_t *cp) {
    cache_item *ptr = cp->head;
    while (ptr) {
        ptr->age ++;
        ptr = ptr->next;
    }
}

/**
 * @brief
 *      given cache and tag, find if target cache_item 
 *      existed 
 *
 * @ret 1 if find, 0 otherwise
 * @param 
 *      cp: pointer to cache_t
 *      tag: to be macthed tag
 */
static int find_hit(cache_t *cp, const char *tag) {
    cache_item *ptr = cp->head;
    int find = 0;

    // update read_cnt
    P(&r_mutex);  
    read_cnt++;
    if (read_cnt == 1) { // first read 
        P(&w_mutex); // lock write
    }
    V(&r_mutex);  

    // begin finding
    while (ptr){
        if (!strcmp(ptr->tag, tag)){
            find = 1;
            break;
        } 
        ptr = ptr->next;
    }

    // update read_cnt
    P(&r_mutex);  
    read_cnt--;
    if (read_cnt == 0) { // last read 
        V(&w_mutex); // unlock write
    }
    V(&r_mutex); 

    return find;
}

/**
 * @brief
 *      givne cache and tag, get the data and size if a
 *      cache_item whose tag is tag in whole cache
 *
 * @note: 
 *      eventhough get failed, all cache's age 
 *      will increase by one, it's ok because it's fair
 * @param 
 *      cp: pointer to cache_t
 *      tag: to be macthed tag
 *      out_data: the data will be stored here and return 
 *      out_size: the size will be stored here and return
 * @ret 
 *      1 if get, 0 otherwise
 */
static int get_hit(cache_t *cp, const char *tag, char *out_data, \
            int *out_size){

    cache_item *ptr = cp->head;
    int get = 0;
    
    P(&w_mutex); // lock write

    // begin get 
    while (ptr){
        if (!strcmp(ptr->tag, tag)){
            get = 1;
            memcpy(out_data, ptr->data, ptr->size);
            *out_size = ptr->size;
            ptr->age = 1;
        } else {
            (ptr->age)++;
        } 
        ptr = ptr->next;
    }

    V(&w_mutex); // unlock write

    return get;
}

/* @brief 
 *      remove oldest one from cache's link-list
 * @note 
 *      it's not thread safe, so the semaphore lock/unlock 
 *      must be controlled by its caller
 * @param 
 *      cp: pointer to cache_t
 */
static void remove_oldest (cache_t *cp) {
    cache_item *ptr = cp->head;
    cache_item *pre_ptr = NULL;
    cache_item *oldest_ptr = NULL;
    cache_item *pre_oldest_prt = NULL;
    unsigned int oldest_age = 0;

    if ( NULL == ptr) {
        return;
    }

    // find oldest one
    while (ptr) {
        if (ptr->age > oldest_age) {
            pre_oldest_prt = pre_ptr;
            oldest_ptr = ptr;
            oldest_age = ptr->age;
        }
        pre_ptr = ptr;
        ptr = ptr->next;
    }
    if ( NULL == oldest_ptr ){
        return;  // safty purpose
    }

    /* remove */
    // relink 
    if ( cp->head == oldest_ptr ){ // it's head 
        cp->head = oldest_ptr->next;
    } else {
        pre_oldest_prt->next = oldest_ptr->next;
    }

    // update 
    cp->total_size -= oldest_ptr->size;
    (cp->cache_cnt)--;

    // free 
    Free(oldest_ptr->tag);
    Free(oldest_ptr->data);
    Free(oldest_ptr);
}

/**
 * @brief
 *      add a new cache-item into head of link-list with
 *      given tag and data
 *
 * @note
 *      The default age is 1 because we use unsign int 
 *      to store age so we need compare starting from 0
 *
 * @param 
 *      cp: pointer to cache_t
 *      tag: given tag to store
 *      data: given data to store
 *      size: given size to store
 */
static void add_to_cache_head(cache_t *cp, const char *tag,\
        const char *data, int size) {

    // creat 
    cache_item *item = Malloc(sizeof(cache_item));
    item->tag = Malloc(strlen(tag)+1);
    item->data = Malloc(size);
    // copy
    strcpy(item->tag, tag);
    memcpy(item->data, data, size);
    item->size = size;
    // update link
    item->next = cp->head;
    cp->head = item;
    cp->total_size += size;
    cp->cache_cnt ++;
    ages(cp);
    item->age = 1;
}

/** public function for other program to call */
/**
 * @brief
 *      initialize cache 
 *
 * @param 
 *      cp: pointer to cache_t
 */
void cache_init(cache_t *cp) {
    read_cnt = 0;
    Sem_init(&r_mutex, 0, 1);
    Sem_init(&w_mutex, 0, 1);
    cp->total_size = 0;
    cp->cache_cnt = 0;
    cp->head = NULL;
}

/**
 * @brief
 *      de-initialize cache 
 *
 * @param 
 *      cp: pointer to cache_t
 */
void cache_deinit(cache_t *cp) {
    cache_item *cur_ptr = cp->head;
    cache_item *next_ptr;
    while(cur_ptr){
        next_ptr = cur_ptr->next;
        Free(cur_ptr->tag);
        Free(cur_ptr->data);
        Free(cur_ptr);
        cur_ptr = next_ptr;
    }
}

/**
 * @brief
 *      read data from cache by given tag. 
 *
 * @param 
 *      cp: pointer to cache_t
 *      tag: to be macthed tag
 *      out_data: the data will be stored here and return 
 *      out_size: the size will be stored here and return
 * @ret 
 *      1 if get, 0 otherwise
 */
int read_cache(cache_t *cp, const char *tag, char *out_data, int *out_size) {
    if (find_hit(cp, tag)){
        return (get_hit(cp, tag, out_data, out_size));
    }
    return 0;
}

/**
 * @brief
 *      write data to cache by given tag/data/info
 *
 * @param 
 *      cp: pointer to cache_t
 *      tag: given tag to store 
 *      data: given data to store  
 *      size: given size to store
 * @ret 
 *      1 if get, 0 otherwise
 */
void write_cache(cache_t *cp, const char *tag, const char *data, int size) {
    if ( size > MAX_OBJECT_SIZE) {  // error proof
        return;   
    }

    P(&w_mutex);  // lock w

    // remove oldest one if space is not enough
    while ( size + cp->total_size > MAX_CACHE_SIZE) {
        remove_oldest(cp);
    }

    add_to_cache_head(cp, tag, data, size);
    
    V(&w_mutex);  // unlock w 
}

