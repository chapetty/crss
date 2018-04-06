#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <expat.h>

#include "parse.h"
#include "crss.h"

const unsigned short BUF_LEN = 1024;
const unsigned short MAX_TAGS = 32;

enum {
  TAG_CHANNEL,
  TAG_ENCLOSURE,
  TAG_ITEM,
  TAG_RSS,
  TAG_TITLE,
  TAG_XML,
  TAGS, // just to keep track of the number of tags
  TAG_UNKNOWN

};

struct stack_item {
  int tag_id;
  char *match_str;
  struct stack_item *next;
};

struct list_item {
  char *title;
  char *url;
  struct list_item *next;
};

struct user_data {
  struct stack_item *top;
  int receiving_data;
  void *tmp_data;

  char match_str[128];
  int match_str_len;

  void (*handle_data)(void *);
};

struct tag_handler {
  struct tag_handler *next;
  char *tag_string;
  void (*tag_handler)(const char *);
};

struct tag_handler *th_first;

void register_tag_handler(const char *tag_string, void (*tag_handler)(const char *)) {

  int found = 0;
  struct tag_handler *th_tmp = th_first;
  struct tag_handler *th_last = NULL;

  while (th_tmp && !found) {
    if (!strcmp(th_tmp->tag_string, tag_string))
      found = 1;
    th_last = th_tmp;
    th_tmp = th_tmp->next;
  }

  if (found) {
    th_last->tag_handler = tag_handler;
  } else {
    th_tmp = malloc(sizeof(struct tag_handler));
    th_tmp->next = NULL;
    th_tmp->tag_string = malloc(strlen(tag_string) + 1);
    strcpy(th_tmp->tag_string, tag_string);
    th_tmp->tag_handler = tag_handler;
    if (th_last)
      th_last->next = th_tmp;
    else
      th_first = th_tmp;
  }

}

#ifdef DEBUG
void DEBUG_list_tags() {

  struct tag_handler *th_tmp = th_first;

  while (th_tmp != NULL) {
    fprintf(stderr, "registered %p for \"%s\"\n", th_tmp->tag_handler, th_tmp->tag_string);
    th_tmp = th_tmp->next;
  }

}
#endif

void match_tag_handlers(struct user_data *ud, const char *data) {

  struct tag_handler *th_tmp = th_first;

  while (th_tmp) {
    if (th_tmp->tag_string[0] == '/') {
      // compare to global match string
      if (!strcmp(ud->match_str, th_tmp->tag_string))
        th_tmp->tag_handler(data);
    } else {
      // try each match string until we hit root
      struct stack_item *si = ud->top;
      while (si) {
        if (!strcmp(si->match_str, th_tmp->tag_string))
          th_tmp->tag_handler(data);
        si = si->next;
      }
    }
    th_tmp = th_tmp->next;
  }

}

int has_tag_handlers(struct user_data *ud) {

  struct tag_handler *th_tmp = th_first;

  while (th_tmp) {
    if (th_tmp->tag_string[0] == '/') {
      if (!strcmp(ud->match_str, th_tmp->tag_string))
        return 1;
    } else {
      struct stack_item *si = ud->top;
      while (si) {
        if (!strcmp(si->match_str, th_tmp->tag_string))
          return 1;
        si = si->next;
      }
    }
    th_tmp = th_tmp->next;
  }

  return 0;

}

void *get_tag_handler(const char *tag_string) {

  struct tag_handler *th_tmp = th_first;

  while (th_tmp) {
    if (!strcmp(th_tmp->tag_string, tag_string))
      return th_tmp->tag_handler;
    th_tmp = th_tmp->next;
  }

  return NULL;

}

/*
void set_tag(int index, const char *tag) {

  if (index >= TAGS || index < 0)
    return;

  if (tags[index] != NULL)
    free(tags[index]);

  tags[index] = malloc(strlen(tag) + 1);
  strcpy(tags[index], tag);

}
*/

/*
int find_tag(const char *tag_string, int *tag) {

  int tag_id;
  int tag_start = 0;
  int tag_end = TAGS - 1;
  int range;
  int cmp;

  do {
    range = tag_end - tag_start + 1;
    tag_id = tag_start + (range >> 1);
    cmp = strcmp(tag_string, tags[tag_id]);
    if (cmp < 0)
      tag_end = tag_id - 1;
    else if (cmp > 0)
      tag_start = tag_id + 1;
    else
      break;
  } while (range > 0);

  if (range > 0) {
    *tag = tag_id;
    return 1;
  }

  return 0;

}
*/

/*
void init_tags() {

  tags = calloc(TAGS, sizeof(char *));
  set_tag(TAG_CHANNEL, "channel");
  set_tag(TAG_ENCLOSURE, "enclosure");
  set_tag(TAG_ITEM, "item");
  set_tag(TAG_RSS, "rss");
  set_tag(TAG_TITLE, "title");
  set_tag(TAG_XML, "xml");

}
*/

void test_tag_handler(const char *data) {
  fprintf(stderr, "test: %s\n", data);
}

void init_tags() {

  th_first = NULL;

}

void free_tags() {

  struct tag_handler *th_tmp = th_first;

  while (th_first) {
    th_tmp = th_first->next;
    free(th_first->tag_string);
    free(th_first);
    th_first = th_tmp;
  }

}

/*
void free_tags() {

  int i;

  for (i = 0; i < TAGS; ++i) {
    if (tags[i] != NULL)
      free(tags[i]);
  }

  free(tags);
  tags = NULL;

}
*/

void stack_push(struct user_data *ud, char type, const char *tag_name) {

  struct stack_item *si = malloc(sizeof(struct stack_item));

  // FIXME
  si->tag_id = TAG_UNKNOWN;

  si->next = NULL;
  // we want each stack item to append its tag name to the global match string,
  // but should, itself, point to the local part of the match string
  if (ud->top)
    si->match_str = ud->top->match_str + strlen(ud->top->match_str);
  else
    si->match_str = ud->match_str;
  si->match_str[0] = type;
  si->match_str += 1;
  strcpy(si->match_str, tag_name);

  if (!ud->top) {
    ud->top = si;
  } else {
    si->next = ud->top;
    ud->top = si;
  }

#ifdef DEBUG
  //fprintf(stderr, "debug: stack->top: %i, next: %p\n", ud->top->tag_id,
  // ud->top->next);
  fprintf(stderr, "debug: stack->top: %s\n", ud->top->match_str);
#endif

}

void stack_pop(struct user_data *s) {

  if (s->top) {
#ifdef DEBUG
    //fprintf(stderr, "debug: popping: %i\n", s->top->tag_id);
    fprintf(stderr, "debug: popping: %s\n", s->top->match_str);
#endif
    // replace the '/' that was added to the global match string to drop the
    // local match string
    *(s->top->match_str - 1) = '\0';
    struct stack_item *tmp = s->top->next;
    free(s->top);
    s->top = tmp;
  }

}

void stack_destroy(struct user_data *ud) {

  while (ud->top)
    stack_pop(ud);

  if (ud->tmp_data) {
    free(ud->tmp_data);
    ud->tmp_data = NULL;
  }

  ud->receiving_data = 0;

}


void startElement(void *userData, const char *name, const char **atts) {

  struct user_data *ud = (struct user_data *) userData;
  const char **att;
  int tag_id;

  //if (find_tag(name, &tag_id))
  //  stack_push(ud, tag_id);
  //else
  //  stack_push(ud, TAG_UNKNOWN);

  stack_push(ud, '/', name);

  att = atts;
  while (*att) {
    stack_push(ud, '@', *att);

    match_tag_handlers(ud, *(att+1));

    stack_pop(ud);
    att += 2;
  }

  ud->receiving_data = 0;

}

void endElement(void *userData, const char *name) {

  struct user_data *ud = (struct user_data *) userData;

  if (ud->receiving_data) {
    match_tag_handlers(ud, ud->tmp_data);
    ud->receiving_data = 0;
  }
  
  if (ud->tmp_data) {
    free(ud->tmp_data);
    ud->tmp_data = NULL;
  }

  stack_pop(ud);

}

void handleCharacterData(void *userData, const char *d, int len) {

  struct user_data *s = (struct user_data *) userData;

  if (!s->receiving_data) {
    if (has_tag_handlers(s)) {
      s->tmp_data = malloc(len + 1);
      strncpy(s->tmp_data, d, len);
      ((char *) s->tmp_data)[len] = 0;
      s->receiving_data = 1;
    }
  } else {
    int tmp_data_len = strlen(s->tmp_data);
    char *new_data = malloc(tmp_data_len + len + 1);
    strncpy(new_data, s->tmp_data, tmp_data_len);
    strncpy(new_data + tmp_data_len, d, len);
    new_data[tmp_data_len + len] = 0;
    free(s->tmp_data);
    s->tmp_data = new_data;
  }

}

void add_to_menu(void *userData) {

  struct user_data *ud = (struct user_data *) userData;
  //menu_append(ud->menu, ud->tmp_data);

#ifdef DEBUG
  fprintf(stderr, "the fuck do I do with \"%s\"?\n", (char *) ud->tmp_data);
#endif

}

void parse(int sockfd) {

  ssize_t len;
  char buf[BUF_LEN];

  struct user_data ud;
  ud.top = NULL;
  ud.receiving_data = 0;
  ud.tmp_data = NULL;

  XML_Parser parser = XML_ParserCreate(NULL);
  XML_SetUserData(parser, &ud);
  XML_SetElementHandler(parser, startElement, endElement);
  XML_SetCharacterDataHandler(parser, handleCharacterData);

  do {
    len = recv(sockfd, buf, BUF_LEN, 0);
    if (len < 0) {
      perror("?");
      break;
    }
    if (!XML_Parse(parser, buf, len, !len)) {
      fprintf(stderr, "bad data\n");
      break;
    }
  } while (len > 0);

  stack_destroy(&ud);
  XML_ParserFree(parser);

}

#ifdef TEST_PARSE
int main() {
#else
int parse_main() {
#endif

  int i;
  size_t len;
  char buf[BUF_LEN];
  struct user_data ud;
  ud.top = NULL;
  ud.receiving_data = 0;
  ud.tmp_data = NULL;
  ud.handle_data = add_to_menu;

  XML_Parser parser = XML_ParserCreate(NULL);
  XML_SetUserData(parser, &ud);
  XML_SetElementHandler(parser, startElement, endElement);
  XML_SetCharacterDataHandler(parser, handleCharacterData);

  FILE *f = fopen("zen.xml", "r");
  if (f == NULL) {
    XML_ParserFree(parser);
    exit(1);
  }

  init_tags();

  do {
    len = fread(buf, 1, BUF_LEN, f);
    if (!XML_Parse(parser, buf, len, (len != BUF_LEN))) {
      fprintf(stderr, "bad data\n");
      break;
    }
    //fprintf(stdout, "%i\n", len);
  } while (len == BUF_LEN);
  
  if (ud.top != NULL)
    fprintf(stderr, "broken stack!\n");

  while (ud.top != NULL) {
    printf("%i\n", ud.top->tag_id);
    stack_pop(&ud);
  }

  free_tags();
  fclose(f);
  XML_ParserFree(parser);

  return 0;

}
