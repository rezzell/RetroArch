TARGETS := http_method_match_test task_http_harness_test

CORE_DIR := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

HTTP_METHOD_MATCH_SOURCES := http_method_match_test.c
TASK_HTTP_HARNESS_SOURCES := \
	task_http_harness_test.c \
	../task_harness.c \
	$(CORE_DIR)/tasks/task_http.c \
	$(LIBRETRO_COMM_DIR)/queues/task_queue.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c

HTTP_METHOD_MATCH_OBJS := $(HTTP_METHOD_MATCH_SOURCES:.c=.o)
TASK_HTTP_HARNESS_OBJS := $(TASK_HTTP_HARNESS_SOURCES:.c=.o)

CFLAGS += -Wall -pedantic -std=gnu99 -g -O0
TASK_HTTP_HARNESS_CFLAGS = \
	$(CFLAGS) \
	-DHAVE_NETWORKING \
	-DTASK_HTTP_TEST \
	-I$(CORE_DIR) \
	-I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGETS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(CORE_DIR)/tasks/task_http.o: $(CORE_DIR)/tasks/task_http.c
	$(CC) -c -o $@ $< $(TASK_HTTP_HARNESS_CFLAGS)

../task_harness.o: ../task_harness.c
	$(CC) -c -o $@ $< $(TASK_HTTP_HARNESS_CFLAGS)

$(LIBRETRO_COMM_DIR)/queues/task_queue.o: $(LIBRETRO_COMM_DIR)/queues/task_queue.c
	$(CC) -c -o $@ $< $(TASK_HTTP_HARNESS_CFLAGS)

$(LIBRETRO_COMM_DIR)/compat/compat_strl.o: $(LIBRETRO_COMM_DIR)/compat/compat_strl.c
	$(CC) -c -o $@ $< $(TASK_HTTP_HARNESS_CFLAGS)

$(LIBRETRO_COMM_DIR)/compat/compat_strldup.o: $(LIBRETRO_COMM_DIR)/compat/compat_strldup.c
	$(CC) -c -o $@ $< $(TASK_HTTP_HARNESS_CFLAGS)

$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.o: $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c
	$(CC) -c -o $@ $< $(TASK_HTTP_HARNESS_CFLAGS)

$(LIBRETRO_COMM_DIR)/lists/string_list.o: $(LIBRETRO_COMM_DIR)/lists/string_list.c
	$(CC) -c -o $@ $< $(TASK_HTTP_HARNESS_CFLAGS)

$(LIBRETRO_COMM_DIR)/string/stdstring.o: $(LIBRETRO_COMM_DIR)/string/stdstring.c
	$(CC) -c -o $@ $< $(TASK_HTTP_HARNESS_CFLAGS)

task_http_harness_test.o: task_http_harness_test.c
	$(CC) -c -o $@ $< $(TASK_HTTP_HARNESS_CFLAGS)

http_method_match_test: $(HTTP_METHOD_MATCH_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

task_http_harness_test: $(TASK_HTTP_HARNESS_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGETS) $(HTTP_METHOD_MATCH_OBJS) $(TASK_HTTP_HARNESS_OBJS)

.PHONY: clean
