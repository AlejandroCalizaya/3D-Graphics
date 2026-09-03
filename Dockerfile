FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# =========================================================
# Herramientas de compilación
# =========================================================

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    pkg-config \
    python3 \
    python3-pip \
    python3-dev \
    python3-jinja2 \
    git \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# =========================================================
# GLFW + OpenGL + X11 (Con soporte de renderizado por software)
# =========================================================

RUN apt-get update && apt-get install -y --no-install-recommends \
    libglm-dev \
    libglfw3-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    mesa-common-dev \
    libx11-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    xauth \
    mesa-utils \
    xvfb \
    && rm -rf /var/lib/apt/lists/*

# =========================================================
# Instalar FFMPEG
# =========================================================

RUN apt-get update && apt-get install -y ffmpeg && rm -rf /var/lib/apt/lists/*

# =========================================================
# Instalar el generador GLAD 1 que tenemos en /opt/glad
# =========================================================

COPY glad /opt/glad

WORKDIR /opt/glad

RUN python3 -m pip install \
    --break-system-packages \
    .

# =========================================================
# Workspace
# =========================================================

WORKDIR /workspace

CMD ["/bin/bash"]
