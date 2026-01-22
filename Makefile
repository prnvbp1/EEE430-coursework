CC=gcc
CFLAGS=-Wall -Wextra -O2 -pthread -IInclude

SRCS=src/main.c src/pc_globals.c src/pc_util.c src/pc_hist.c src/pc_queue.c src/pc_workers.c src/pc_runinfo.c

HDRS = Include/pc_globals.h Include/pc_hist.h Include/pc_queue.h Include/pc_runinfo.h Include/pc_types.h Include/pc_util.h Include/pc_workers.h


OUT=pc_model

all:
	$(CC) $(CFLAGS) $(SRCS) $(HDRS) -o $(OUT)

debug:
	$(CC) -Wall -Wextra -O0 -g -pthread -IInclude $(SRCS) -o $(OUT)

clean:
	rm -f $(OUT)

