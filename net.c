#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>

#ifdef SSL_SUPPORT
#include <openssl/ssl.h>
#endif

#include <time.h>

#include <netdb.h>

#include "crss.h"
//#include "crss_main.h"
#include "parse.h"

#define CRSS_NET_BUFSIZE 4096

struct parse_url {
  char *service;
  char *host;
  char *port;
  char *get;
};

#ifdef SSL_SUPPORT
SSL_CTX * get_ssl_ctx() {
  static SSL_CTX *ssl_ctx = NULL;
  if (ssl_ctx == NULL) {
    SSL_library_init();
    ssl_ctx = SSL_CTX_new(SSLv23_client_method());
  }
  return ssl_ctx;
}
#endif

int get_line(int sockfd, char *buf, size_t len) {

  ssize_t rlen;
  int i;
  char c;

  if (len < 1)
    return 0;

  for (i = 0; i < len; ++i) {
    rlen = recv(sockfd, &c, 1, 0);
    if (rlen <= 0) {
      break;
    } else if (c == '\n') {
      break;
    } else if (c == '\r') {
      rlen = recv(sockfd, &c, 1, MSG_PEEK);
      if (c == '\n')
        recv(sockfd, &c, 1, 0);
      break;
    }
    buf[i] = c;
  };

  buf[i] = '\0';

  return i;

}

int get_status(const char *line) {

  return atoi(line + 9);

}

// return the index in string a where string b begins
// if b is not found in a, returns -1
int strindex(const char *a, const char *b) {

  int i;
  int size_a = strlen(a);
  int size_b = strlen(b);

  if (size_b > size_a || !size_b || !size_a) return -1;

  for (i = 0; i < size_a; ++i) {
    if (a[i] == b[0]) {
      int j;
      int found = 1;
      for (j = 0; j < size_b && i + j < size_a; ++j) {
        if (a[i+j] != b[j]) {
          found = 0;
          break;
        }
      }
      if (found) {
        return i;
      }
    }
  }

  return -1;

}

void parse_url_n(struct parse_url *parse_url, const char *url) {

  int state;
  int i;

  int strn = strlen(url);

  int service = 0;
  int host = 0;
  int port = 0;
  int get = 0;

  state = 0;
  for (i = 0; i < strn; ++i) {

    switch(state) {
      case 0:
        //looking for ':'
        if (url[i] == ':') {
          if (url[i+1] == url[i+2] && url[i+1] == '/') {
            i += 2;
            host = i + 1;
            state = 1;
          }
        }
        break;
      case 1:
        //looking for ':' or '/'
        if (url[i] == ':') {
          port = i + 1;
          state = 2;
        } else if (url[i] == '/') {
          get = i;
          state = 3;
        }
        break;
      case 2:
        //looking for '/'
        if (url[i] == '/') {
          get = i;
          state = 3;
        }
        break;
      case 3:
        break;
    }

  }

  //printf("s->%i, h->%i, p->%i, g->%i\n", service, host, port, get);

  parse_url->service = malloc(host - 3  + 1);
  strncpy(parse_url->service, url, host - 3 + 1);
  parse_url->service[host - 3] = '\0';

  parse_url->host = malloc(get - host + 1);
  strncpy(parse_url->host, url + host, get - host + 1);
  parse_url->host[get - host] = '\0';

  parse_url->get = malloc(strn - get + 1);
  strncpy(parse_url->get, url + get, strn - get + 1);
  parse_url->get[strn - get] = '\0';

}

void parse_url(struct parse_url *pu, const char *url) {

  int i;
  int t;

  int service, host, port, get;
  service = host = port = get = -1;

  t = strindex(url, "://");
  if (t > 0) {
    service = 0;
    host = t + 3;
    pu->service = malloc(t+1);
    strncpy(pu->service, url, t);
  } else {
    t = 0;
  }

  if (t > 0) t += 3;
  
  for (i = host; i < strlen(url+host); ++i) {
    if (url[i] == '/' && get < 0) {
      get = i + 1;
    } else if (url[i] == ':' && port < 0) {
      port = i + 1;
    }
  }

  if (strlen(url+t) > 0) {
    pu->host = malloc(strlen(url+t)+1);
    strcpy(pu->host, url+t);
  }

  //printf("service: %i\nhost: %i\nport: %i\nget: %i\n",
   //service, host, port, get);

}

int sn_build_get_request(char *str, size_t size, const char *get, const char **headers) {

  const char **header;
  int requestlen;

  requestlen = 0;
  requestlen += snprintf(str + requestlen, size - requestlen,
   "GET %s HTTP/1.1\r\n", get);
  
  for (header = headers; *header; header += 2) {
    requestlen += snprintf(str + requestlen, size - requestlen, "%s: %s\r\n",
     *header, *(header + 1));
  }

  requestlen += snprintf(str + requestlen, size - requestlen, "\r\n");

  return requestlen;

}

void free_parse_url(struct parse_url *parse_url) {

  if (parse_url->service)
    free(parse_url->service);

  if (parse_url->host)
    free(parse_url->host);

  if (parse_url->get)
    free(parse_url->get);

  parse_url->service = NULL;
  parse_url->host = NULL;
  parse_url->get = NULL;

}

void file_download(const char *filename, const char *url) {

  int fd, sd, status, readlen, writelen;
  long int content_length, total_write;
  struct parse_url parsed_url;
  struct addrinfo *ai;
  char buf[CRSS_NET_BUFSIZE];

  const char *headers[] = {"Host", NULL, NULL};

  bzero(&parsed_url, sizeof(struct parse_url));
  parse_url_n(&parsed_url, url);

  if (!parsed_url.host)
    exit(1);

  headers[1] = parsed_url.host;

  if(getaddrinfo(parsed_url.host, parsed_url.service, NULL, &ai)) {
    perror("getaddrinfo");
    exit(1);
  }

  sd = socket(AF_INET, SOCK_STREAM, 0);
  if (sd < 0) {
    perror("socket");
    exit(1);
  }

  if (connect(sd, ai->ai_addr, ai->ai_addrlen)) {
    perror("connect");
    exit(1);
  }

  freeaddrinfo(ai);

  writelen = sn_build_get_request(buf, CRSS_NET_BUFSIZE, parsed_url.get, headers);
  writelen = send(sd, buf,  writelen, 0);

  readlen = get_line(sd, buf, CRSS_NET_BUFSIZE);
  if (!readlen) {
    fprintf(stderr, "get line\n");
    exit(1);
  }

  status = get_status(buf);

  if (status != 200 && status != 301 && status != 302) {
    fprintf(stderr, "status: %i\n", status);
  }

  content_length = 0;
  do {
    readlen = get_line(sd, buf, CRSS_NET_BUFSIZE);
    if (!readlen)
      break;
    if (0 == strindex(buf, "Content-Length:")) {
      printf("content-length: %i\n", atoi(buf + 16));
      content_length = atol(buf + 16);
    }
    if (0 == strindex(buf, "Location:")) {
      printf("redirecting to %s\n", buf + 10);
      if (status == 301 || status == 302) {
        close(sd);
        free_parse_url(&parsed_url);
        file_download(filename, buf + 10);
        return;
      }
    }
    //printf("%s\n", buf);
  } while (readlen > 0);

  // open file
  fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
  if (fd < 0) {
    perror("open");
    exit(1);
  }

  do {
    readlen = recv(sd,  buf, CRSS_NET_BUFSIZE, 0);
    if (readlen < 0) {
      perror("recv");
      break;
    }
    writelen = write(fd, buf, readlen);
    if (writelen < 0) {
      perror("write");
      break;
    }
  } while (readlen > 0);

  close(sd);
  free_parse_url(&parsed_url);
  close(fd);

}

void init_data() {
  struct crss_menu *crss_m = crss_get_main_menu();
  // This will ultimately need to read a podcast list from a file, for now add podcasts here to test
  append_menu_item(crss_m, create_menu_item("Test Podcast", "https://testpodcasturl/test.xml"));
}

void set_title(const char *data) {
  //printf("Channel title: %s\n", data);
}

void set_item_title(const char *data) {
  append_menu_item(crss_get_main_menu()->sel->sub_menu, create_menu_item(data, NULL));
}

void set_item_url(const char *data) {
  crss_get_main_menu()->sel->sub_menu->last->url = malloc(strlen(data) + 1);
  strcpy(crss_get_main_menu()->sel->sub_menu->last->url, data);
}

void get(const char *url) {

  struct addrinfo hints;
  struct addrinfo *ai;
  int sockfd;

  #ifdef SSL_SUPPORT
  SSL *ssl;
  #endif

  struct parse_url parsed_url;
  bzero(&parsed_url, sizeof(struct parse_url));
  parse_url_n(&parsed_url, url);

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  ai = NULL;
  getaddrinfo(parsed_url.host, parsed_url.service, &hints, &ai);

  uint8_t *addr_shift;
  int i;

  time_t now = time(NULL);
  struct tm *tm = gmtime(&now);
  struct tm testm;
  testm.tm_sec = 0;
  testm.tm_min = 30;
  testm.tm_hour = 6;
  testm.tm_mday = 28;
  testm.tm_mon = 4;
  testm.tm_year = 112;
  testm.tm_wday = 2;
  testm.tm_yday = 147;
  testm.tm_isdst = 0;
  testm.tm_gmtoff = 0;
  testm.tm_zone = "GMT";
  char modified_since[32];
  tm = &testm;
  strftime(modified_since, 32, "%a, %d %b %Y %T %Z", tm);
  const char *headers[] = {"Host", parsed_url.host, "If-Modified-Since", modified_since, NULL};

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    fprintf(stderr, "socket could not be created\n");
    exit(1);
  }

  if (connect(sockfd, ai->ai_addr, ai->ai_addrlen)) {
    perror("could not connect");
    exit(1);
  }

  if (strindex(parsed_url.service, "https") != -1) {
    #ifdef SSL_SUPPORT
    ssl = SSL_new(get_ssl_ctx());
    SSL_set_fd(ssl, sockfd);
    if (SSL_connect(ssl)) {
      perror("ssl socket connection failure");
      exit(1);
    }
    #else
    perror("ssl not supported");
    #endif
  }

  char buf[256];
  int reqlen = sn_build_get_request(buf, 256, parsed_url.get, headers);
  //fprintf(stderr, "%s", buf);
  int total_size = 0;

  if (send(sockfd, buf, reqlen, 0) < 0) {
    perror("bad send");
    exit(1);
  }

  ssize_t readlen;
  int nline;

  nline = get_line(sockfd, buf, 128);
  int status = get_status(buf);
  //printf("STATUS: %i\n", status);

  while (nline) {
    nline = get_line(sockfd, buf, 128);
    //printf("line: %s\n", buf);
  }

  if (status != 200) exit(1);

  char c;
  do {
    readlen = recv(sockfd, &c, 1, MSG_PEEK);
    if (readlen < 1 || c == '<')
      break;
    recv(sockfd, &c, 1, 0);
  } while (readlen);

  init_tags();
  register_tag_handler("/rss/channel/title", set_title);
  register_tag_handler("item/title", set_item_title);
  register_tag_handler("/rss/channel/item/enclosure@url", set_item_url);

  parse(sockfd);

  free_tags();

  close(sockfd);

}

