#ifndef ITEMS_H
#define ITEMS_H

extern void room_items(int room_id);
extern void search(int room_id);
extern void list_inv(void);
extern void take_item(int room_id, char *word1, char *word2);
extern void take(int item_id);
extern int unique_item(int room_id, char *adj, char *name);
extern void drop_item(int room_id, char *word1, char *word2);
extern void look_item(int room_id, char *word1, char *word2);
extern void break_item(int item);
extern void create_item(int item_id, int room_id);

#endif
