#ifndef ROOMS_H
#define ROOMS_H

extern int room_id(void);
extern void look_room(void);
extern void move(char *direction);
extern int search_desc(void);
extern void location_move(int room, int dir, int loc);

#endif
