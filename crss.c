#include <stdlib.h>
#include <string.h>

#include "crss.h"

void append_menu_item(struct crss_menu *menu, struct crss_menu_item *menu_item) {

  struct crss_menu_item *tmp;

  menu->size += 1;
  
  if (NULL == menu->first) {
    menu->first = menu_item;
    menu->last = menu_item;
    menu->sel = menu_item;
    return;
  }

  menu->last->next = menu_item;
  menu_item->last = menu->last;
  menu->last = menu_item;

}

struct crss_menu *create_menu() {

  struct crss_menu *menu;

  menu = malloc(sizeof(struct crss_menu));
  menu->sel = NULL;
  menu->first = NULL;
  menu->last = NULL;
  menu->size = 0;

  return menu;

}

void free_menu(struct crss_menu *menu) {

  struct crss_menu_item *tmp;

  if (NULL == menu) return;

  while (menu->first != NULL) {
    tmp = menu->first->next;
    free_menu_item(menu->first);
    menu->first = tmp;
  }

  free(menu);

}

struct crss_menu_item *create_menu_item(const char *title, const char *url) {

  int i;
  struct crss_menu_item *menu_item;

  menu_item = malloc(sizeof(struct crss_menu_item));

  if (title) {
    menu_item->title = malloc(strlen(title) + 1);
    strcpy(menu_item->title, title);
  }

  if (url) {
    menu_item->url = malloc(strlen(url) + 1);
    strcpy(menu_item->url, url);
  }

  menu_item->sub_menu = NULL;
  menu_item->next = NULL;
  menu_item->last = NULL;

  return menu_item;

}

void free_menu_item(struct crss_menu_item *menu_item) {

  if (NULL == menu_item) return;

  if (menu_item->title)
    free(menu_item->title);

  if (menu_item->url)
    free(menu_item->url);

  if (menu_item->sub_menu)
    free_menu(menu_item->sub_menu);

  free(menu_item);

}
