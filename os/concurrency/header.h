# include <stdlib.h>
# include <stdio.h>
# include <semaphore.h>
# include <pthread.h>
# include <stdbool.h>
# include <unistd.h>
# include <sys/types.h>
# include <stdatomic.h>
# include <errno.h>

typedef int buffer_item;

void init_buffer(int sz);
void rm_buf();
int put_item(buffer_item item);
int rm_item(buffer_item* item);

