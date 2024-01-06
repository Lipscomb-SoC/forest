#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "rooms.h"
#include "items.h"
#include "inter.h"
static void parse_input(char *line);
static void display_help(void);
static void quit_screen(void);
static void bad_ending(void);
static void good_ending(void);
int GAME_STATUS;

#define READLINE_MAX_LINE 80

static char *readline() {
    char buf[READLINE_MAX_LINE];
	size_t len;

	printf("command> ");
	fflush(stdout);
	if (fgets(buf,READLINE_MAX_LINE,stdin) == NULL) return NULL;
	len = strlen(buf);
	while(len && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
		len--;
		buf[len] = '\0';
	}
	return strdup(buf);
}

/* main input loop */
extern void input_loop(void)
{
	char *line;

	/* Start in room 0 and show it */
	room_name();
	look_room();
	GAME_STATUS = 0;

	/* main input loop */
	while(putchar('\n') && (line = readline()) != NULL) {
		if (line[0] != '\0') {
			parse_input(line);
			if (GAME_STATUS == -1) {
				quit_screen();
				break;
			} else if (GAME_STATUS == -2) {
				bad_ending();
				break;
			} else if (GAME_STATUS == -3) {
				good_ending();
				break;
			}
		}
		free(line);
	}
}

/* parse input and direct commands */
static void parse_input(char *line)
{
	int i;
	char allwords[100];
	char *words[8];

	/* copy the line and convert to lowercase */
	for (i = 0; i < 99 && line[i] != '\0'; i++) {
		allwords[i] = line[i];
		if (allwords[i] >= 'A' && allwords[i] <= 'Z')
			allwords[i] = allwords[i] + 'a' - 'A';
	}
	allwords[i] = '\0';

	/* split the line into separate words */
	*words = strtok(allwords, " \n");
	for(i = 1; i < 8; i++)
		*(words+i) = strtok(NULL, " \n");

	/* parse input */
	if (*words == NULL) {
		return;
	}
	
	/* user quit */
	else if (strcmp(*words,"quit") == 0 || strcmp(*words,"exit") == 0 || strcmp(*words,"q") == 0) {
		GAME_STATUS = -1;
	}

	/* user help */
	else if (strcmp(*words, "help") == 0 || strcmp(*words, "h") == 0) {
		display_help();
	}

	/* look at room */
	else if (strcmp(*words, "look") == 0 && *(words+1) == NULL) {
		look_room();
	}

	/* move */
	else if (strcmp(*words, "go") == 0 || strcmp(*words, "walk") == 0 || strcmp(*words, "move") == 0) {
		move(*(words+1));
	} else if (strcmp(*words, "north") == 0 || strcmp(*words, "east") == 0 || strcmp(*words, "south") == 0 ||
		strcmp(*words, "west") == 0) {
		move(*words);
	} else if (strcmp(*words, "n") == 0) {
		move("north");
	} else if (strcmp(*words, "e") == 0) {
		move("east");
	} else if (strcmp(*words, "s") == 0) {
		move("south");
	} else if (strcmp(*words, "w") == 0) {
		move("west");
	}

	/* item functions */
	else if (strcmp(*words,"take") == 0 || strcmp(*words,"get") == 0) {
		if (*(words+1) != NULL && strcmp(*(words+1),"the") == 0) {
			take_item(room_id(), *(words+2), *(words+3));
		} else {
			take_item(room_id(), *(words+1), *(words+2));
		}
	} else if (strcmp(*words,"drop") == 0) {
		if (*(words+1) != NULL && strcmp(*(words+1),"the") == 0) {
			drop_item(room_id(), *(words+2), *(words+3));
		} else {
			drop_item(room_id(), *(words+1), *(words+2));
		}
	} else if (strcmp(*words,"search") == 0 || strcmp(*words,"find") == 0) {
		search(room_id());
	} else if (strcmp(*words,"i") == 0 || strcmp(*words,"inventory") == 0 || strcmp(*words,"inv") == 0) {
		list_inv();
	} else if (strcmp(*words,"look") == 0) {
		if (strcmp(*(words+1),"at") == 0) {
			if (*(words+2) != NULL && strcmp(*(words+2),"the") == 0) {
				look_item(room_id(), *(words+3), *(words+4));
			} else {
				look_item(room_id(), *(words+2), *(words+3));
			}
		} else {
			look_item(room_id(), *(words+1), *(words+2));
		}
	}

	/* interactions */
	else if (strcmp(*words,"use") == 0) {
		use(words);
	}

	/* unknown command given */
	else {
		printf("\nUnknown command '%s",*words);
		for (i = 1; i < 4; i++) {
			if (*(words+i) != NULL) printf(" %s",*(words+i));
		}
		printf("'.\n");
	}
}

static void display_help(void)
{
	printf("\n--------------------------------------------------------------------------------\n");
	printf("HOW TO PLAY\n");
	printf("\tExplore the world and solve the mystery of the forest!\n");
	printf("\tTo perform an action, use the following commands:\n");
	printf("--------------------------------------------------------------------------------\n");
	printf("NAVIGATING THE WORLD\n");
	printf("\twalk direction (or just direction) - move in the direction specified\n");
	printf("\tlook - examine your surroundings\n");
	printf("--------------------------------------------------------------------------------\n");
	printf("MANAGING ITEMS\n");
	printf("\tinventory - list your belongings\n");
	printf("\ttake - pick up an item\n");
	printf("\tdrop - drop an item\n");
	printf("--------------------------------------------------------------------------------\n");
	printf("INTERACTING WITH THE WORLD\n");
	printf("\tsearch - look for hidden objects\n");
	printf("\tuse - use an item or items\n");
	printf("--------------------------------------------------------------------------------\n");
	printf("TO QUIT\n");
	printf("\tquit - give up in your search and go home\n");
	printf("--------------------------------------------------------------------------------\n");
}

/* Displayed upon exiting the game */
static void quit_screen(void)
{
	printf("\n--------------------------------------------------------------------------------\n");
	printf("GAME OVER: The mystery of the forest remains unsolved!\n");
	printf("--------------------------------------------------------------------------------\n\n");
}

/* bad ending */
static void bad_ending(void)
{
	printf("\n--------------------------------------------------------------------------------\n");
	printf("YOU GOT THE BAD ENDING\n");
	printf("--------------------------------------------------------------------------------\n");
	printf("How could you have known you were going to release an ancient horror upon the\n");
	printf("people of the world? Nobody can really blame you, but as the giant statue\n");
	printf("rampages across the world, killing all who oppose him, subjugating everyone\n");
	printf("else, you are trapped in the Hidden Temple with no way out.\n");
	printf("--------------------------------------------------------------------------------\n");
	printf("GAME OVER: BETTER LUCK NEXT TIME!\n");
	printf("--------------------------------------------------------------------------------\n\n");
}

/* good ending */
static void good_ending(void)
{
	printf("\n--------------------------------------------------------------------------------\n");
	printf("YOU GOT THE GOOD ENDING\n");
	printf("--------------------------------------------------------------------------------\n");
	printf("You have solved the secret of the Hidden Temple! By destroying the giant statue,\n");
	printf("you have uncovered a cache of riches beyond your wildest dreams! What will you\n");
	printf("do with your newfound wealth? Become a king? Become a god? The choice is up to\n");
	printf("you! But first, you need to find a cart...\n");
	printf("--------------------------------------------------------------------------------\n");
	printf("CONGRATULATIONS!\n");
	printf("--------------------------------------------------------------------------------\n\n");
}
