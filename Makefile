# SPDX-License-Identifier: GPL-3.0-or-later
NAME        = spectre
CC          = gcc
CFLAGS      = -Wall -Wextra -Werror -std=c99 -O3
RM          = rm -rf

SRCS_DIR    = src
BUILD_DIR   = obj

FFTW_VER    = 3.3.10
FFTW_DIR    = third_party/fftw-$(FFTW_VER)
FFTW_INST   = $(FFTW_DIR)/install_dir
FFTW_LIB    = $(FFTW_INST)/lib/libfftw3f.a
FFTW_CONFIG = --prefix=$(PWD)/$(FFTW_INST) --enable-float --enable-neon --enable-static --disable-shared --with-pic

INCS        = -I$(SRCS_DIR) -I$(FFTW_INST)/include
LIBS        = $(FFTW_LIB) -lm

SRCS        = $(SRCS_DIR)/main.c
OBJS        = $(SRCS:$(SRCS_DIR)/%.c=$(BUILD_DIR)/%.o)

.PHONY: all
all: $(NAME)

$(NAME): $(FFTW_LIB) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIBS) -o $(NAME)
	echo "\033[32mLinked $(NAME) successfully\033[0m"

$(BUILD_DIR)/%.o: $(SRCS_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCS) -c $< -o $@
	echo "\033[34mCompiled $<\033[0m"

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# configure then build fftw
$(FFTW_LIB):
	@mkdir -p $(FFTW_DIR)/build_tmp
	@echo "Configuring FFTW in temp directory..."
	cd $(FFTW_DIR) && ./configure $(FFTW_CONFIG)
	@$(MAKE) -C $(FFTW_DIR)/build_tmp install
	@rm -rf $(FFTW_DIR)/build_tmp

.PHONY: clean
clean:
	$(RM) $(BUILD_DIR)
	echo "\033[31mRemoved object files\033[0m"

.PHONY: fclean
fclean: clean
	$(RM) $(NAME)
	echo "\033[31mRemoved $(NAME)\033[0m"

# Clean everything including the FFTW build artifacts
.PHONY: distclean
distclean: fclean
	if [ -d "$(FFTW_DIR)" ]; then \
		$(MAKE) -C $(FFTW_DIR) distclean || true; \
	fi
	$(RM) $(FFTW_INST)
	echo "\033[31mDeep cleaned FFTW vendor folder\033[0m"

re: fclean all

