APP_NAME := 3dgraphics
DOCKERFILE := Dockerfile
WORKSPACE_DIR := $(CURDIR)


ifeq ($(OS),Windows_NT)
    IS_WINDOWS := 1
else
    IS_WSL := $(shell uname -r | grep -i microsoft)
endif

ifneq ($(strip $(IS_WINDOWS)$(IS_WSL)),)
    DISPLAY ?= host.docker.internal:0
else
    DISPLAY ?= :0
endif

RUN_FILE := $(word 2,$(MAKECMDGOALS))

ifeq ($(RUN_FILE),)
	RUN_FILE := main.cpp
endif


RUN_EXE := a.out


.PHONY: build run bash clean

build:
	docker build \
		-f $(DOCKERFILE) \
		-t $(APP_NAME) .

run:
	docker run --rm -it \
		-e DISPLAY=$(DISPLAY) \
		-e LIBGL_ALWAYS_SOFTWARE=1 \
		-v "$(WORKSPACE_DIR)":/workspace \
		-w /workspace \
		$(APP_NAME) \
		bash -lc '\
			set -e; \
			rm -rf /tmp/glad; \
			glad \
				--profile core \
				--api gl=3.3 \
				--generator c \
				--out-path /tmp/glad; \
			g++ -std=c++17 \
				$(RUN_FILE) \
				shared.cpp \
				/tmp/glad/src/glad.c \
				-I/tmp/glad/include \
				$$(pkg-config --cflags --libs glfw3) \
				-lGL \
				-ldl \
				-o $(RUN_EXE); \
			./$(RUN_EXE)'

bash:
	docker run --rm -it \
		-e DISPLAY=$(DISPLAY) \
		-e LIBGL_ALWAYS_SOFTWARE=1 \
		-v "$(WORKSPACE_DIR)":/workspace \
		-w /workspace \
		$(APP_NAME) \
		bash

clean:
	docker rmi -f $(APP_NAME) || true
	rm -f $(RUN_EXE)
