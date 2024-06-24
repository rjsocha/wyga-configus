#define EPOLL
#define HTTPSERVER_IMPL
#include "httpserver.h"
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

#if defined(__x86_64__) || defined(_M_X64)
  #define APPARCH "amd64"
#elif defined(__aarch64__) || defined(_M_ARM64)
  #define APPARCH "arm64/v8"
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
  #define APPARCH "386"
#elif defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__)
  #define APPARCH "arm/v6"
#elif defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
  #define APPARCH "arm/v7"
#else
  #define APPARCH "???"
#endif

#define max(a,b) ({ __typeof__ (a) _a = (a);  __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
#define min(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })

#define PONG      "PONG"
#define VERSION   "1.7.2"

#define CFGURL    "/cfg/"
#define DEFAULT_CONFIG_DIR "/depot"
#define MIN_OUTPUT_SIZE 16<<10
#define MAX_OUTPUT_SIZE 256<<10

int request_target_is(struct http_request_s* request, char const * target) {
  http_string_t url = http_request_target(request);
  int len = strlen(target);
  return len == url.len && memcmp(url.buf, target, url.len) == 0;
}

int allowed_filename(char const *name) {
char c, lc=0;
  while(c=*name++) {
    if(c>='0' && c<='9') {
      continue;
    }
    if(c>='a' && c<='z') {
      continue;
    }
    if(c>='A' && c<='Z') {
      continue;
    }
    switch(c) {
      case '_':
      case '-':
      case '.':
      case '@':
      case '/':
      case '%':
      case '(':
      case ')':
        continue;
    }
    return 1;
  }
  return 0;
}

char *macro_replace(const char *start, const char *end, const char *default_start, const char *default_end, char *output, long int *size, long  int output_size) {
int macro_len=(int)(end-start);
char *env_val;
char* env_var;
  ssize_t env_name_len = snprintf(NULL, 0, "%.*s",macro_len,start);
  if( env_var = malloc(env_name_len + 1) ) {
    snprintf(env_var, env_name_len + 1, "%.*s",macro_len,start);
    if( env_val = getenv(env_var) ) {
      int len = strlen(env_val);
      if( *size+len >= output_size ) len = output_size-*size-1;
      *size += len;
      memcpy(output, env_val, len);
      output += len;
    } else if (default_end - default_start ) {
      int len = default_end - default_start;
      if( *size+len >= output_size ) len = output_size-*size-1;
      *size += len;
      memcpy(output, default_start, len);
      output += len;
    }
    free(env_var);
  }
  return output;
}

// replaces @{{NAME-default}} with env variable ${<NAME>}
// based on https://github.com/bennoleslie/gettext/blob/master/gettext-runtime/src/envsubst.c
long int macro_engine(char *input, char *output, long int output_size) {
  char *result=input;
  long int size=0;
  const char *variable_start;
  const char *variable_end;
  const char *default_start;
  const char *default_end;
  char c;
  for (; *input != '\0';) {
    // copy proccessed input to output buffer
    if( result!=input ) {
      int to_copy=(input-result);
      if( size+to_copy >= output_size ) to_copy=output_size-size-1;
      if(to_copy) {
        memcpy(output, result, to_copy);
        output += to_copy;
        size += to_copy;
        result = input;
      }
    }
    // is this macro?
    if (*input == '@') {
      input++;
      if (*input++ != '{') continue;
      if (*input++ != '{') continue;

      variable_start = input;
      c = *input;
      if( (c >= 'A' && c <= 'Z') || c == '_' ) {
        do {
          c = *++input;
        } while ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_');
        variable_end = input;
        default_start = 0;
        default_end = 0;
        if ( *input == '-' ) {
          input++;
          default_start = input;
          do
            c=*++input;
          while ( c && c != '}' );
          default_end = input;
        }
        if (*input++ != '}') continue;
        if (*input++ != '}') continue;
        result=input;
        output=macro_replace(variable_start, variable_end, default_start, default_end, output, &size, output_size);
      }
    } else if (*input++ == '{') {
      // new macro format {{NAME=default}}
      if (*input++ != '{') continue;
      variable_start = input;
      c = *input;
      if( (c >= 'A' && c <= 'Z') ) {
        while ( (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ) {
          if ( c == '-' ) {
            *input = '_';
            c = '_';
          }
          c = *++input;
        }
        variable_end = input;
        default_start = 0;
        default_end = 0;
        if ( *input == '=' ) {
          input++;
          default_start = input;
          do
            c=*++input;
          while ( c && c != '}' );
          default_end = input;
        }
        if (*input++ != '}') continue;
        if (*input++ != '}') continue;
        result=input;
        output=macro_replace(variable_start, variable_end, default_start, default_end, output, &size, output_size);
      }
    }
  }
  if(result!=input) {
    int to_copy=(input-result);
    if( size+to_copy >= output_size ) to_copy=output_size-size-1;
    if( to_copy ) {
      memcpy(output,result,to_copy);
      size += to_copy;
    }
  }
  return size;
}

char *process_file(char const *file, long int *len) {
struct stat st;
char *buffer = NULL;
char *input = NULL;
char full_name[512];
char *config;
  *len=-1;
  if(strlen(file) < 1 || *file == '/' ) {
    return NULL;
  }
  if( allowed_filename(file) != 0 ) {
    return NULL;
  }
  memset(full_name,0,sizeof(full_name));
  config=getenv("CONFIGUS_DEPOT");
  if(config == NULL) {
    config=DEFAULT_CONFIG_DIR;
  }
  //printf("CONFIG: %s\n",config);
  strncpy(full_name,config,sizeof(full_name)-1);
  strncat(full_name,"/expose/",sizeof(full_name)-1-strlen(full_name));
  strncat(full_name,file,sizeof(full_name)-1-strlen(full_name));
  //printf("FULL PATH: %s %ld\n",full_name,strlen(full_name));
  if(stat(full_name,&st) == 0 ) {
    long int size = st.st_size;
    if(size>0) {
      int fd;
      if( (fd=open(full_name,0)) >= 0 ) {
        int buffer_size = max(MIN_OUTPUT_SIZE, min(size*4, MAX_OUTPUT_SIZE));
        if( (input=calloc(size+1,sizeof(char))) && (buffer=calloc(buffer_size,sizeof(char))) ) {
          if( read(fd,input,size) == size ) {
            input[size]=0;
            *len=macro_engine(input,buffer,buffer_size);
          }
          free(input);
        }
        close(fd);
      }
    } else {
      *len = 0;
    }
  }
  return buffer;
}

void handle_request(struct http_request_s* request) {
  struct sockaddr_in client;
  char ip[INET_ADDRSTRLEN];
  socklen_t len;
  int status = 200;
  time_t now;
  struct tm * timeinfo;
  char cfgfile[256];
  char *file = NULL;
  char date[40];
  int cfglen = strlen(CFGURL);

  http_request_connection(request, HTTP_CLOSE);

  len = sizeof(client);
  getpeername(request->socket,(struct sockaddr *)&client,&len);
  if (inet_ntop(AF_INET, &client.sin_addr, ip, sizeof(ip)) == NULL) {
     strcpy(ip,"?");
  }

  http_string_t method = http_request_method(request);
  http_string_t url = http_request_target(request);

  struct http_response_s* response = http_response_init();
  http_response_header(response, "Content-Type", "application/octet-stream");

  if (request_target_is(request, "/.ping")) {
    http_response_body(response, PONG, sizeof(PONG) - 1);
  } else if (request_target_is(request, "/.version")) {
    http_response_body(response, VERSION, sizeof(VERSION) - 1);
  } else if (memcmp(url.buf,CFGURL,cfglen) == 0) {
    int cpy;
    long int len;

    memset(cfgfile,0,sizeof(cfgfile));
    cpy=url.len-cfglen<sizeof(cfgfile) ? url.len-cfglen : sizeof(cfgfile)-1;
    strncpy(cfgfile,url.buf+cfglen,cpy);
    file=process_file(cfgfile,&len);
    if(len>=0) {
      if(file) {
        http_response_body(response, file, len);
      }
    } else {
      status=404;
    }
  } else {
    status=404;
  }
  now = time(NULL);
  timeinfo = localtime ( &now);
  strftime(date,sizeof(date),"%Y/%m/%d %H:%M:%S",timeinfo);
  printf("%s configus %s %d %.*s %.*s\n",date,ip,status,method.len,method.buf,url.len,url.buf);
  fflush(stdout);
  http_response_status(response, status);
  http_respond(request, response);
  if(file!=NULL) {
    free(file);
  }
}

struct http_server_s* server;

void handle_sigterm(int signum) {
  free(server);
  exit(0);
}
void show_banner() {
int fd,c;
char buf[256];
char banner_name[256];
char *config;
  memset(banner_name,0,sizeof(banner_name));
  config=getenv("CONFIGUS_DEPOT");
  if(config == NULL) {
    config=DEFAULT_CONFIG_DIR;
  }
  strncpy(banner_name,config,sizeof(banner_name)-1);
  strncat(banner_name,"/.banner",sizeof(banner_name)-1-strlen(banner_name));
  fd=open(banner_name,0);
  if( fd >= 0 ) {
    while ( (c=read(fd, &buf, sizeof(buf))) > 0) {
      write(1, &buf, c);
    }
    close(fd);
  }
}

int main() {
char *listen;
int port=80;
  // only port number supported
  listen=getenv("CONFIGUS_LISTEN");
  if(listen != NULL) {
    port=atoi(listen);
    if(port<1 || port > 65535) {
      port = 80;
    }
  }
  printf("CONFIGUS SERVIVCE VERSION %s READY AT 0.0.0.0:%d ON linux/%s (%d/%d)\n",VERSION,port,APPARCH,geteuid(),getegid());
  show_banner();
  fflush(stdout);
  server = http_server_init(port, handle_request);
  signal(SIGTERM, handle_sigterm);
  signal(SIGINT, handle_sigterm);
  http_server_listen(server);
  return 1;
}
