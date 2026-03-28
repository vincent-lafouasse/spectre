# SPDX-License-Identifier: GPL-3.0-or-later
NAME        = spectre
CC          = gcc
C_VERSION   = c11
CFLAGS      = -Wall -Wextra -std=$(C_VERSION) -O3
RM          = rm -rf

ROOT        = $(PWD)
SRCS_DIR    = src
BUILD_DIR   = build

FFTW_VER    = 3.3.10
FFTW_DIR    = third_party/fftw-$(FFTW_VER)
FFTW_INST   = $(FFTW_DIR)/install_dir
FFTW_LIB    = $(FFTW_INST)/lib/libfftw3f.a
# NOTE: NEON is ARM, x86 AVX TODO
FFTW_CONFIG = --prefix=$(ROOT)/$(FFTW_INST) --enable-float --enable-neon --enable-static --disable-shared --with-pic --disable-fortran

KISS_FFT_VER   = 131.2.0
KISS_FFT_DIR   = third_party/kissfft-$(KISS_FFT_VER)
KISS_FFT_LIB   = $(KISS_FFT_DIR)/libkissfft-float.a

RAYLIB_VER  = 5.5
RAYLIB_DIR  = third_party/raylib-$(RAYLIB_VER)/src
RAYLIB_LIB  = $(RAYLIB_DIR)/libraylib.a

INCS        = -I$(SRCS_DIR) -I$(SRCS_DIR)/common -I$(RAYLIB_DIR) -I$(FFTW_INST)/include -I$(KISS_FFT_DIR)
LIBS        = $(RAYLIB_LIB) $(FFTW_LIB) $(KISS_FFT_LIB) -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL -lm

SRCS        = $(shell find src -name '*.c')
OBJS        = $(SRCS:%.c=$(BUILD_DIR)/%.o)

.PHONY: all
all: $(NAME)

$(NAME): $(RAYLIB_LIB) $(FFTW_LIB) $(KISS_FFT_LIB) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIBS) -o $(NAME)
	@echo "\033[32mLinked $(NAME) successfully\033[0m"

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCS) -c $< -o $@
	@echo "\033[34mCompiled $<\033[0m"

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# configure then build fftw
$(FFTW_LIB):
	$(RM) $(FFTW_DIR)/build_tmp
	mkdir -p $(FFTW_DIR)/build_tmp
	@echo "\033[33mConfiguring FFTW ...\033[0m"
	cd $(FFTW_DIR)/build_tmp && ../configure $(FFTW_CONFIG)
	@echo "\033[33mBuilding and Installing...\033[0m"
	$(MAKE) -C $(FFTW_DIR)/build_tmp -j$(shell sysctl -n hw.ncpu)
	$(MAKE) -C $(FFTW_DIR)/build_tmp install
	@echo "\033[32mFFTW installed successfully.\033[0m"
	$(RM) $(FFTW_DIR)/build_tmp

$(KISS_FFT_LIB):
	KISSFFT_DATATYPE=float KISSFFT_STATIC=1 KISSFFT_TOOLS=0 make all -C $(KISS_FFT_DIR) 

$(RAYLIB_LIB):
	RAYLIB_LIBTYPE=STATIC make -C $(RAYLIB_DIR)

.PHONY: clean
clean:
	$(RM) $(BUILD_DIR) $(NAME)

.PHONY: distclean
distclean: clean
	$(MAKE) -C $(RAYLIB_DIR) clean
	$(MAKE) -C $(KISS_FFT_DIR) clean
	$(RM) $(FFTW_INST)

.PHONY: re
re: clean all

.PHONY: update
update: clean
	$(MAKE) all
	bear --output $(BUILD_DIR)/compile_commands.json -- $(MAKE) re

