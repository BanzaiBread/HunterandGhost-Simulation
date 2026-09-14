CC = gcc
CFLAGS = -W -pthread
TARGET = proj

# List of source files
SRCS = main.c ghost.c helpers.c house.c hunter.c room.c evidence.c 

# Automatically generate a list of object files
OBJS = $(SRCS:.c=.o)

# Default target
all: $(TARGET)

# Link the object files to create the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Compile .c files into .o files
# We depend on defs.h and helpers.h so that if headers change, we recompile
%.o: %.c defs.h helpers.h
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up old build files
clean:
	rm -f $(OBJS) $(TARGET)