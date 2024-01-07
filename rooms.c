#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rooms.h"
#include "rooms-desc.h"
#include "items.h"
static void room_exits(void);

/* room variable - start in room 0 */
static int current_room = 0;

/* current room_id */
extern int room_id(void)
{
	return current_room;
}

/* provide room description */
extern void look_room(void)
{
	printf("\n%s\n",locations[current_room].room_name);
	printf("\n%s\n",locations[current_room].room_desc);
	room_items(current_room);
	room_exits();
}

static void room_exits(void)
{
	int i;
	int count;
	char *dirs[4] = {"north","east","south","west"};
	printf("Exits:  ");
	for (count = 0, i = 0; i < 4; i++) {
		if (locations[current_room].walk_to[i] != -1) {
			printf("%s  ",dirs[i]);
			count++;
		}
	}
	if (count == 0) printf("none");
	putchar('\n');
}

extern void move(char *direction)
{
	int dir;

	putchar('\n');

	/* figure out direction */
	if (direction == NULL) {
		printf("Walk where?\n");
		return;
	} else if (strcmp(direction,"north") == 0) {
		dir = 0;
	} else if (strcmp(direction,"east") == 0) {
		dir = 1;
	} else if (strcmp(direction,"south") == 0) {
		dir = 2;
	} else if (strcmp(direction,"west") == 0) {
		dir = 3;
	} else {
		printf("Walk where?\n");
		return;
	}

	/* move */
	if (locations[current_room].walk_to[dir] != -1) {
		printf("%s\n",locations[current_room].walk_desc[dir]);
		current_room = locations[current_room].walk_to[dir];
		look_room();
	} else {
		printf("You cannot walk %s.\n",direction);
	}

}

extern int search_desc(void)
{
	if (locations[current_room].search_desc != NULL) {
		printf("%s ", locations[current_room].search_desc);
		return 0;
	} else {
		return 1;
	}

}

extern void location_move(int room, int dir, int loc)
{
	locations[room].walk_to[dir] = loc;
}
