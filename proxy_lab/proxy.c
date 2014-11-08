/**
 * proxty.c 
 *
 * @author Wei-Lin Tsai weilints@andrew.cmu.edu
 *
 * Basic flow
 *      1. create a fd to listen
 *      2. create a pool of pthread to deal with connection
 *      3. whenever new connection is accepted, insert to sbuf 
 *      4. pthread of pool will continuosly remove a fd from 
 *         sbuf and do the proxy job
 *         a. parse header 
 *         b. check if cache hit, it so, return data from cache 
 *         c. otherwise, establish connection to real host, get 
 *            data from host and send it back to client, also 
 *            write to cache it the size is less than MAX_OBJECT_SIZE
 * Used file:
 *      csapp.h/csapp.c: do a little hack for error handling
 *      sbuf.h/sbuf.c: from webside
 *      cache.h/cache.c: a reader/writer link-list based cache
 *
 * @note
 *      It's a basic pthread pool version as text presented 
 *
  */
#include <stdio.h>
#include "csapp.h"
#include "sbuf.h"
#include "cache.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* Thread pool size and buf size */
#define POOL_SIZE 32
#define SBUF_SIZE 400

/* Shared global variable */
sbuf_t sbuf;
cache_t cache;

/** Helper functions declarations */
void do_proxy(int fd);
void *thread(void *vargp);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);
int parse_request(int fd, char *method, char *uri, char *version, \
                  char *out_host, char *out_path, int *out_port);
int parse_uri(char *in_uri, char *out_host, char *out_path, int *out_port);
int validate_version(const char *version);
int read_and_refine_req_hdrs(rio_t *rp, char *out_buf, char *in_host);


/* You won't lose style points for including these long lines in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11;\
Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,\
application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";

int main(int argc, char **argv)
{
    int i, listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;
    pthread_t tid;

    clientlen = sizeof(clientaddr);

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(1);
    }

    // Handle signal
    Signal(SIGPIPE, SIG_IGN);

    port = atoi(argv[1]);
    sbuf_init(&sbuf, SBUF_SIZE);
    cache_init(&cache);

    listenfd = Open_listenfd(port);

    // create worker thread 
    for (i = 0; i < POOL_SIZE; i++){
        Pthread_create(&tid, NULL, thread, NULL);
    }
    
    while (1) {
	connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *)&clientlen);
        sbuf_insert(&sbuf, connfd);
    }
}

/** Helper functions */

/**
 * @brief
 *      do one proxy service
 *
 * The concept is described at the header of this file
 *  
 * @note 
 *      1. only support http 1.0 GET method
 * @param 
 *      fd: an newly accepted fd get from sbuf_remove
 */
void do_proxy(int fd) {
    rio_t rio;             /// CSAPP defined io structure
    char buf[MAXLINE];     /// tmp buffer 
    int tmp_len;           /// tmp len
    char hdr_buf[MAXBUF];  /// store whole modified request header
    char method[MAXLINE];  /// http 1.0 method, should be "GET"
    char uri[MAXLINE];
    char version[MAXLINE];
    /** uri can be http://hostname:port/path */ 
    char hostname[MAXLINE];  
    char path[MAXLINE];
    int port;
    /** created out going connection part */
    int to_real_host_fd;
    rio_t rio_to_real_host;
    /* for cache*/
    char tag[MAXLINE];
    char cache_data[MAX_OBJECT_SIZE];
    char *tmp_ptr = cache_data;
    int cache_data_size = 0;

    /* Handle request part */
    Rio_readinitb(&rio, fd);
    if ( Rio_readlineb(&rio, buf, MAXLINE) < 0) { // read firs line of header
        return;  // return on error
    }
    sscanf(buf, "%s %s %s", method, uri, version); 
    // parse and check correctness of request
    if (parse_request(fd, method, uri, version, hostname, path, &port)){
        return;
    }

    /* Prerare for out going access */
    if (read_and_refine_req_hdrs(&rio, hdr_buf, hostname)){
        return;  // return on error
    }

    /* Check if cache hit */
    sprintf(tag,"%s:%d%s", hostname, port, path);
    if (read_cache(&cache, tag, cache_data, &cache_data_size)){
        Rio_writen(fd, cache_data, cache_data_size);
        return;
    } 

    /* Now is cache miss */
    /* Establish connection to the real host */
    to_real_host_fd = Open_clientfd_r(hostname, port);
    Rio_readinitb(&rio_to_real_host, to_real_host_fd);
    
    /* Do the communication */ 
    // send request to real host
    sprintf(buf, "GET %s HTTP/1.0\r\n", path);
    Rio_writen(to_real_host_fd, buf, strlen(buf));
    Rio_writen(to_real_host_fd, hdr_buf, strlen(hdr_buf));
    // do the communication
    while ((tmp_len = Rio_readlineb(&rio_to_real_host, buf, MAXLINE)) > 0) {
        if ( tmp_len < 0 ){
            Close(to_real_host_fd);
            return;        // return on read error
        }
        cache_data_size += tmp_len;
        if ( cache_data_size <= MAX_OBJECT_SIZE) {
            memcpy(tmp_ptr, buf, tmp_len);
            tmp_ptr += tmp_len;
        }
        if ( Rio_writen(fd, buf, tmp_len)){
            Close(to_real_host_fd);
            return;        // return on write error
        }
    }

    // now update cache 
    if (cache_data_size <= MAX_OBJECT_SIZE){
        write_cache(&cache, tag, cache_data, cache_data_size);
    }

    Close(to_real_host_fd);
    return;
}

/**
 * @brief
 *      call with Pthread_create, detach from main thread
 *      then keep get fd from sbuf do the proxy service
 */
void *thread(void *vargp){
    int connfd;
    Pthread_detach(pthread_self());
    while(1) {
        connfd = sbuf_remove(&sbuf); // get a connfd from pool
        do_proxy(connfd);            // do proxy service
        Close(connfd);
    }
}

/**
 * clienterror - returns an error message to the client
 * copy from tiny.c
 */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Proxy server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

/**
 * @brief 
 *      parse headers from io and do some replacement as writeup 
 *      request
 * @param
 *      rp: the rio_t pointer, the source of data stream
 *      out_buf: the refined data to return for later use 
 *      in_host: the hostname to format a HOST header if client 
 *               not provide
 *
 * @ret
 *      0 if OK, -1 if error
 */
int read_and_refine_req_hdrs(rio_t *rp, char *out_buf, char *in_host) {
    char is_host_given = 0;
    char buf[MAXLINE];

    /** reading headers */
    while (1) {
        if ( Rio_readlineb(rp, buf, MAXLINE) < 0 ) {
            return -1;  // return on error
        }
        if(!strcmp(buf, "\r\n")){    // end of reading 
            break;
        }
        if(!strncasecmp(buf,"Host:", 5)){
            sprintf(out_buf, "%s", buf);
            is_host_given = 1;
        } else if (!strncasecmp(buf,"User-Agent:", 11)){
            // do nothing 
        } else if (!strncasecmp(buf,"Accept:", 7)){
            // do nothing 
        } else if (!strncasecmp(buf,"Accept-Encoding:", 16)){
            // do nothing
        } else if (!strncasecmp(buf,"Connection:", 11)){
            // do nothing
        } else if (!strncasecmp(buf,"Proxy-Connection:", 117)){
            // do nothing
        }else {
            sprintf(out_buf, "%s%s", out_buf, buf);
        }
    }

    /** attached required headers as writeup request */
    if (!is_host_given) {
        sprintf(out_buf, "%sHost: %s\r\n", out_buf, in_host);
    }
    // User-Agent, Accept, Accept-Enconding, Conncetion, Proxy-Connection
    sprintf(out_buf, "%s%s", out_buf, user_agent_hdr);
    sprintf(out_buf, "%s%s", out_buf, accept_hdr);
    sprintf(out_buf, "%s%s", out_buf, accept_encoding_hdr);
    sprintf(out_buf, "%s%s", out_buf, connection_hdr);
    sprintf(out_buf, "%s%s", out_buf, proxy_connection_hdr);

    sprintf(out_buf, "%s\r\n", out_buf);  // header terminator 

    return 0;
}

/**
 * @brief 
 *      parse request and  check if is valid 
 * @param
 *      fd: fd to send back error msg
 *      method: HTTP metod, only support "GET"
 *      uri: HTTP URI 
 *      version: HTTP version
 *      out_host: HTTP HOST
 *      out_path: HTTP path
 *      out_port: HTTP port 
 * @ret
 *      0 if OK, -1 if error
 */
int parse_request(int fd, char *method, char *uri, char *version,\
                  char *out_host, char *out_path, int *out_port){

    // check method
    if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Not Implemented",
                "Proxy server does not support this method");
        return -1;
    }

    // check version
    if (validate_version(version)){
        clienterror(fd, version, "501", "Not Implemented",
                "Proxy server does not support this version of HTTP");
        return -1;
    }

    // check uri
    if (parse_uri(uri, out_host, out_path, out_port)){
        clienterror(fd, uri, "400", "Bad Request",
                "Incorrect URI format");
        return -1;
    }

    return 0;                  
}


/**
 * @brief 
 *      parse uri to get host/path/port 
 * @param 
 *      in_uri: input uri to be parsed 
 *      out_host: store parsed host for later use 
 *      out_path: store parsed path for later use 
 *      out_port: store parsed port for later use
 *
 * @attention 
 *      we use strchr and strcpy/strncpy because it's thread safe 
 *
 * @ret
 *      0 if OK, -1 if error
 */
int parse_uri(char *in_uri, char *out_host, char *out_path, int *out_port){
    char *host_begin, *path_begin, *port_begin;
    int len;        /// tmp len for hostname

    // support http only, does not support https
    if (strncasecmp(in_uri, "http://", 7) != 0) {
        out_host[0] = '\0';
        return -1;
    }

    host_begin = in_uri + 7;
    path_begin = strchr(host_begin, '/');

    // get host 
    if (NULL == path_begin){
        strcpy(out_host, host_begin);
    } else {
        len = path_begin - host_begin;
        strncpy(out_host, host_begin, len);
        out_host[len] = '\0';
    }

    // get path 
    if (NULL == path_begin) {
        strcpy(out_path, "/");
    } else {
        strcpy(out_path, path_begin);
    }
    
    // get port
    port_begin = strchr(out_host, ':');
    if (NULL == port_begin) { 
        *out_port = 80;  // default
    } else {
        // no need to check if atoi success because browser did it for us
        *out_port = atoi(port_begin+1);
        // also refine host to remove port 
        *port_begin = '\0';
    }
    
    return 0;
}

/**
 * @brief 
 *      OK if version is HTTP/1.0 or HTTP/1.1, error otherwise
 * @param 
 *      version: string of version to be checked 
 * 
 * @ret
 *      0 if OK, -1 if error
 */
int validate_version(const char *version){
    if (strcasecmp(version, "HTTP/1.0") &&
        strcasecmp(version, "HTTP/1.1")){
        return -1;
    } 
    return 0;
}

