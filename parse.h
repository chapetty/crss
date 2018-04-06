#ifndef __PARSE_H__
#define __PARSE_H__

#include "crss.h"

void parse(int sockfd);
void register_tag_handler(const char *tag_string, void (*tag_handler)(const char *));
void free_tags();

#ifdef DEBUG
void DEBUG_list_tags();
#endif

#endif
