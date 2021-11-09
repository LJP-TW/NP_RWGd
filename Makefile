NP_SIMPLE = np_simple
NP_SINGLE_PROC = np_single_proc
NP_MULTI_PROC = np_multi_proc

CC = gcc
CFLAGS = -std=gnu11 -Wall

ifeq ($(DEBUG),1)
   CFLAGS += -DDEBUG
endif

INC_COMMON_ALL     := common_all/include
SRC_COMMON_ALL     := $(wildcard common_all/src/*.c)
INC_COMMON_UP      := common_userpipe/include
SRC_COMMON_UP      := $(wildcard common_userpipe/src/*.c)
INC_NP_SIMPLE      := np_simple_dir/include
SRC_NP_SIMPLE      := $(wildcard np_simple_dir/src/*.c)
INC_NP_SINGLE_PROC := np_single_proc_dir/include
SRC_NP_SINGLE_PROC := $(wildcard np_single_proc_dir/src/*.c)
INC_NP_MULTI_PROC  := np_multi_proc_dir/include
SRC_NP_MULTI_PROC  := $(wildcard np_multi_proc_dir/src/*.c)

RM = rm -f

all: $(NP_SIMPLE) $(NP_SINGLE_PROC) $(NP_MULTI_PROC)

$(NP_SIMPLE):
	@echo "Compiling" $@ "..."
	$(CC) $(CFLAGS) -I $(INC_COMMON_ALL) -I $(INC_NP_SIMPLE) \
	    $(SRC_COMMON_ALL) $(SRC_NP_SIMPLE) -o $@

$(NP_SINGLE_PROC):
	@echo "Compiling" $@ "..."
	$(CC) $(CFLAGS) -I $(INC_COMMON_ALL) -I $(INC_COMMON_UP) -I $(INC_NP_SINGLE_PROC) \
		$(SRC_COMMON_ALL) $(SRC_COMMON_UP) $(SRC_NP_SINGLE_PROC) -o $@

$(NP_MULTI_PROC):
	@echo "Compiling" $@ "..."
	$(CC) $(CFLAGS) -I $(INC_COMMON_ALL) -I $(INC_COMMON_UP) -I $(INC_NP_MULTI_PROC) \
	    $(SRC_COMMON_ALL) $(SRC_COMMON_UP) $(SRC_NP_MULTI_PROC) -o $@

.PHONY: remake
remake: remove $(NP_SIMPLE) $(NP_SINGLE_PROC) $(NP_MULTI_PROC)
	@echo "Remake done."

.PHONY: remove
remove: 
	@$(RM) $(NP_SIMPLE) $(NP_SINGLE_PROC) $(NP_MULTI_PROC)
