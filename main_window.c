#include <ncurses.h>

#include <stdio.h>
#include <string.h>

#include "crss.h"

struct s_main_window {
  WINDOW *winleft;
  WINDOW *winright;
  WINDOW *wininfo;
  struct crss_menu *focus;
} main_window;

void print_menu(WINDOW *win, struct crss_menu *menu, bool focus) {

  int i, rows, cols;
  struct crss_menu_item *item;

  werase(win);
  getmaxyx(win, cols, rows);
  
  if (menu) {

    i = 1;
    item = menu->first;
    while (item != NULL) {
      if (item == menu->sel && focus) {
        wattron(win, A_REVERSE);
        mvwaddnstr(win, i, 2, item->title, cols);
        wattroff(win, A_REVERSE);
      } else {
        mvwaddnstr(win, i, 2, item->title, cols);
      }
      item = item->next;
      ++i;
    }

  }

  box(win, 0, 0);
  wrefresh(win);

}

void print_info(WINDOW *win, const char *info) {

  werase(win);

  if (info) {
    mvwaddstr(win, 1, 2, info);
  }

  box(win, 0, 0);
  wrefresh(win);

}

void main_window_init() {

  initscr();
  start_color();
  noecho();

  refresh();

  main_window.winleft = newwin(20, 40, 0, 0);
  main_window.winright = newwin(20, 40, 0, 40);
  main_window.wininfo = newwin(4, 80, 20, 0);

}

void main_window_free() {

  delwin(main_window.winleft);
  delwin(main_window.winright);
  delwin(main_window.wininfo);

  endwin();

}

void main_window_draw(struct crss_menu *menu) {

  bool cont;
  int c, i, sel;

  main_window.focus = menu;

  sel = 0;
  cont = true;
  while (cont) {

    refresh();
    print_menu(main_window.winleft, menu, main_window.focus == menu);
    print_menu(main_window.winright, menu->sel->sub_menu, main_window.focus != menu);
    //print_menu(main_window.wininfo, NULL);
    if (menu->sel->sub_menu) {
      print_info(main_window.wininfo, menu->sel->sub_menu->sel->title);
    } else {
      print_info(main_window.wininfo, NULL);
    }

    c = wgetch(main_window.winleft);
    switch (c) {
      case 'q':
        cont = false;
        break;
      case 'j':
        if (main_window.focus->sel->next) {main_window.focus->sel = main_window.focus->sel->next; print_menu(main_window.winright, menu->sel->sub_menu, true);}
        break;
      case 'k':
        if (main_window.focus->sel->last) {main_window.focus->sel = main_window.focus->sel->last; print_menu(main_window.winright, menu->sel->sub_menu, true);}
        break;
      case 'l':
        if (menu->sel && !menu->sel->sub_menu) {
          menu->sel->sub_menu = create_menu();
          get(menu->sel->url);
          print_menu(main_window.winright, menu->sel->sub_menu, true);
        }
        main_window.focus = menu->sel->sub_menu;
        break;
      case 'h':
        main_window.focus = menu;
        break;
      case 'd':
        file_download("test.mp3", menu->sel->sub_menu->sel->url);
        break;
      default:
        break;
    }

  }

}
