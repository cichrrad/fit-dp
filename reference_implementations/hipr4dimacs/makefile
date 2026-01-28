# makefile for programs and generator

# compiler flags:
#
# CHECK_SOLUTION     check if at the end the sink is unreachable 
#                    from the source
# CUT_ONLY           define to compute min-cut but not max-flow
# PRINT_STAT         define to print detailed operation statistics
# PRINT_FLOW         define to print out flow value
# PRINT_CUT          define to print out the sink size of the cut
# INIT_UPDATE        perform initial global update
# FULL_RELABEL       increase distance label as much as possible
#                    instead of adding 1
# LASY_UPDATE        stop when all vertices with excess have been reached


CCOMP = gcc
CFLAGS = -O3 -DNDEBUG -DPRINT_STAT -DCHECK_SOLUTION -DFULL_RELABEL -Wall -DLASY_UPDATE
#CFLAGS = -g -DPRINT_STAT -DCHECK_SOLUTION -DFULL_RELABEL -Wall -lm -DLASY_UPDATE
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
all: hipr4
hipr4: $(OBJS)
	$(CCOMP) $(CFLAGS) $(OBJS) -o hipr4 -lm
%.o: %.c
	$(CCOMP) $(CFLAGS) -c $< -o $@
clean: 
	rm -f hipr4 *.o *~
