NP_SIMPLE = np_simple

INCDIR = include
SRCDIR = src

CC = gcc
CFLAGS = -std=gnu11 -Wall -I $(INCDIR) -I $(SRCDIR)

SOURCES_NP_SHELL := $(wildcard $(SRCDIR)/npshell/*.c)
SOURCES_NP_SIMPLE := $(wildcard $(SRCDIR)/server/np_simple/*.c)

RM = rm -f

all: $(NP_SIMPLE)

$(NP_SIMPLE):
	@echo "Compiling" $@ "..."
	$(CC) $(CFLAGS) $(SOURCES_NP_SHELL) $(SOURCES_NP_SIMPLE) -o $@

.PHONY: remake
remake: remove $(NP_SIMPLE)
	@echo "Remake done."

.PHONY: remove
remove: 
	@$(RM) $(NP_SIMPLE)
