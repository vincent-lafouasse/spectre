NAME        = spectre
CC          = gcc
C_VERSION   = c11
CFLAGS      = -Wall -Wextra -std=$(C_VERSION) -O3
RM          = rm -rf

ROOT        = $(PWD)
SRCS_DIR    = src
BUILD_DIR   = build

KISS_FFT_VER   = 131.2.0
KISS_FFT_DIR   = third_party/kissfft-$(KISS_FFT_VER)
KISS_FFT_LIB   = $(KISS_FFT_DIR)/libkissfft-float.a

RAYLIB_VER  = 5.5
RAYLIB_DIR  = third_party/raylib-$(RAYLIB_VER)/src
RAYLIB_LIB  = $(RAYLIB_DIR)/libraylib.a

INCS        = -I$(SRCS_DIR) -I$(SRCS_DIR)/common -I$(RAYLIB_DIR) -I$(KISS_FFT_DIR)
LIBS        = $(RAYLIB_LIB) $(KISS_FFT_LIB) -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL -lm

SRCS        = $(shell find src -name '*.c')
OBJS        = $(SRCS:%.c=$(BUILD_DIR)/%.o)

.PHONY: all
all: $(NAME)

$(NAME): $(RAYLIB_LIB) $(KISS_FFT_LIB) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIBS) -o $(NAME)
	@echo "\033[32mLinked $(NAME) successfully\033[0m"

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCS) -c $< -o $@
	@echo "\033[34mCompiled $<\033[0m"

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(KISS_FFT_LIB):
	KISSFFT_DATATYPE=float KISSFFT_STATIC=1 KISSFFT_TOOLS=0 make all -C $(KISS_FFT_DIR) 

$(RAYLIB_LIB):
	RAYLIB_LIBTYPE=STATIC make -C $(RAYLIB_DIR)

.PHONY: clean
clean:
	$(RM) $(BUILD_DIR) $(NAME)

.PHONY: distclean
distclean: clean
	make -C $(RAYLIB_DIR) clean
	make -C $(KISS_FFT_DIR) clean

.PHONY: re
re: clean all

.PHONY: update
update: clean
	mkdir -p build
	bear --output $(BUILD_DIR)/compile_commands.json -- make all


# couple of aliases
.PHONY: b c dc u
b: all
c: clean
dc: distclean
u: update
