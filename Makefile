all:
	gcc -o crss net.c main_window.c crss.c parse.c -lexpat -lcurses -g
