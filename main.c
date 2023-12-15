#include <stdio.h>
#include "input.h"
int main(void);
static void splash_screen(void);

int main(void)
{
	splash_screen();
	input_loop();
	return 0;
}

void splash_screen(void)
{
	printf("\n--------------------------------------------------------------------------------\n");
	printf("Welcome to Secret in the Forest\n");
	printf("--------------------------------------------------------------------------------\n");
	printf("What lies in the mysterious forest on the edge of a small village? Myths and\n");
	printf("legends have been told for centuries, but no-one has yet figured it out. Will\n");
	printf("you be the one to finally solve this riddle? Good luck!\n");
	printf("--------------------------------------------------------------------------------\n");
	printf("To learn how to play, type: help\n");
	printf("--------------------------------------------------------------------------------\n");
}
