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

RAYLIB_VER  = 5.5
RAYLIB_DIR  = third_party/raylib-$(RAYLIB_VER)/src
RAYLIB_LIB  = $(RAYLIB_DIR)/libraylib.a

INCS        = -I$(SRCS_DIR) -I$(SRCS_DIR)/common -I$(RAYLIB_DIR) -I$(FFTW_INST)/include
LIBS        = $(RAYLIB_LIB) $(FFTW_LIB) -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL -lm

SRCS        = $(shell find src -name '*.c')
OBJS        = $(SRCS:%.c=$(BUILD_DIR)/%.o)

.PHONY: all
all: $(NAME)

$(NAME): $(RAYLIB_LIB) $(FFTW_LIB) $(OBJS)
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

$(RAYLIB_LIB):
	RAYLIB_LIBTYPE=STATIC make -C $(RAYLIB_DIR)

.PHONY: clean
clean:
	$(RM) $(BUILD_DIR)/**/*.o
	@echo "\033[31mRemoved object files\033[0m"

.PHONY: fclean
fclean: clean
	$(RM) $(NAME) $(BUILD_DIR)
	make clean -C $(RAYLIB_DIR)
	$(RM) $(RAYLIB_LIB)
	@echo "\033[31mRemoved $(NAME)\033[0m"

# Clean everything including the FFTW build artifacts
.PHONY: distclean
distclean: fclean
	if [ -d "$(FFTW_DIR)" ]; then \
		$(MAKE) -C $(FFTW_DIR) distclean || true; \
	fi
	$(RM) $(FFTW_INST)
	@echo "\033[31mDeep cleaned FFTW vendor folder\033[0m"

.PHONY: update
update: fclean
	mkdir -p build
	echo "\033[33mGenerating compilation database with bear...\033[0m"
	bear --output build/compile_commands.json -- $(MAKE) all
	echo "\033[32mUpdate complete: build/compile_commands.json generated.\033[0m"

re: clean all

