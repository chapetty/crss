#ifndef __CRSS_H__
#define __CRSS_H__

struct crss_menu_item {
  char *title;
  char *url;
  struct crss_menu *sub_menu;
  struct crss_menu_item *next;
  struct crss_menu_item *last;
};

struct crss_menu {
  unsigned short size;
  struct crss_menu_item *sel;
  struct crss_menu_item *first;
  struct crss_menu_item *last;
};

struct crss_menu *crss_get_main_menu();
void crss_free_main_menu();

void append_menu_item(struct crss_menu *, struct crss_menu_item *);
void delete_menu_item(struct crss_menu *, struct crss_menu_item *);

struct crss_menu *create_menu();
void free_menu(struct crss_menu *);

struct crss_menu_item *create_menu_item(const char *, const char *);
void free_menu_item(struct crss_menu_item *);

#endif // __CRSS_H__
