APP_NAME := 3dgraphics
DOCKERFILE := Dockerfile
WORKSPACE_DIR := $(CURDIR)

ifeq ($(OS),Windows_NT)
	IS_WINDOWS := 1
else
	IS_WSL := $(shell uname -r 2>/dev/null | grep -i microsoft)
endif

ifneq ($(strip $(IS_WINDOWS)$(IS_WSL)),)
    DISPLAY ?= host.docker.internal:0
else
    DISPLAY ?= :0
endif

FILE ?= main.cpp
RUN_EXE := a.out
VIEWER_SOURCES := viewer/main.cpp viewer/Mesh.cpp viewer/Camera.cpp viewer/Renderer.cpp viewer/Ui.cpp \
    imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/imgui_demo.cpp \
    imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp

BUILD_SOURCES = $(if $(filter viewer/main.cpp,$(FILE)),$(VIEWER_SOURCES) shared.cpp,$(FILE) shared.cpp)

.PHONY: build viewer run bash clean

build:
	docker build \
        -f $(DOCKERFILE) \
        -t $(APP_NAME) .

viewer: FILE := viewer/main.cpp
viewer: run

run:
	docker run --rm -it \
        -e DISPLAY=$(DISPLAY) \
        -e LIBGL_ALWAYS_SOFTWARE=1 \
        -v /tmp/.X11-unix:/tmp/.X11-unix:ro \
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
                $(BUILD_SOURCES) \
                /tmp/glad/src/glad.c \
                -I/tmp/glad/include \
                -Iimgui \
                -Iimgui/backends \
                $$(pkg-config --cflags --libs glfw3) \
                -lGL \
                -ldl \
                -o $(RUN_EXE); \
            ./$(RUN_EXE)'

bash:
	docker run --rm -it \
    	-e DISPLAY=$(DISPLAY) \
    	-e LIBGL_ALWAYS_SOFTWARE=1 \
    	-v /tmp/.X11-unix:/tmp/.X11-unix:ro \
    	-v "$(WORKSPACE_DIR)":/workspace \
    	-w /workspace \
    	$(APP_NAME) \
    	bash

clean:
	docker rmi -f $(APP_NAME) || true
	rm -f $(RUN_EXE)
