#ifndef __CACHE_H__
#define __CACHE_H__

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

struct cache_item {
    char *tag;         /// will be host:port/path
    char *data;
    int size;
    unsigned int age;  /// may be overflow, but don't care 
                       /// because it's not strict LRU
    struct cache_item *next;
};

typedef struct cache_item cache_item;

typedef struct {
    int total_size;    /// current usd cache size 
    int cache_cnt;     /// number of current caches
    struct cache_item *head; /// it's a linked list 
} cache_t;


void cache_init(cache_t *cp);
void cache_deinit(cache_t *cp);

/* return 1 if cache hit */
int read_cache(cache_t *cp, const char *tag, char *out_data, int *out_size);
/* write the target data to cache */
void write_cache(cache_t *cp, const char *tag, const char *data, int size);

#endif 
