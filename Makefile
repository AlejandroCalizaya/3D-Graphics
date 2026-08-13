APP_NAME := test
DOCKERFILE := Dockerfile
WORKSPACE_DIR := $(CURDIR)

DISPLAY ?= :0

RUN_FILE := $(word 2,$(MAKECMDGOALS))

ifeq ($(RUN_FILE),)
	RUN_FILE := main.cpp
endif

RUN_EXE := a.out

.PHONY: build run bash clean

# =========================================================
# Construir imagen
# =========================================================

build:
	docker build \
		-f $(DOCKERFILE) \
		-t $(APP_NAME) .

# =========================================================
# Ejecutar programa
# =========================================================

run:
	xhost +SI:localuser:root >/dev/null 2>&1 || true

	docker run --rm -it --gpus all \
		-e DISPLAY=$(DISPLAY) \
		-e NVIDIA_DRIVER_CAPABILITIES=compute,utility,graphics \
		-v /tmp/.X11-unix:/tmp/.X11-unix \
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
				/tmp/glad/src/glad.c \
				-I/tmp/glad/include \
				$$(pkg-config --cflags --libs glfw3) \
				-lGL \
				-ldl \
				-o $(RUN_EXE); \
			./$(RUN_EXE)'

# =========================================================
# Entrar al contenedor
# =========================================================

bash:
	docker run --rm -it --gpus all \
		-e DISPLAY=$(DISPLAY) \
		-e NVIDIA_DRIVER_CAPABILITIES=compute,utility,graphics \
		-v /tmp/.X11-unix:/tmp/.X11-unix \
		-v "$(WORKSPACE_DIR)":/workspace \
		-w /workspace \
		$(APP_NAME) \
		bash

# =========================================================
# Limpiar
# =========================================================

clean:
	docker rmi -f $(APP_NAME) || true
	rm -f $(RUN_EXE)