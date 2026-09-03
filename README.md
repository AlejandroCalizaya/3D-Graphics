# 3D Graphics

This repository contains assignment implementations and coursework for the **3D Graphics** course, featuring graphics implementations using **OpenGL**, **GLFW**, and **GLAD**, fully containerized with **Docker** for consistent cross-platform development.

---

## 🛠️ Prerequisites & Setup

Before building and running the project, follow these initial steps to set up the required dependencies and resources locally.

### 1. Clone the GLAD Generator
Clone the official GLAD repository (master/v1 branch) directly into the root folder to allow Docker to install it during the build process:

```bash
git clone https://github.com/Dav1dde/glad.git
```

### 2. Download and Setup 3D Models

Some 3D models are required for specific rendering assignments.

* **Stanford Bunny:** [Download Stanford Bunny (.tar.gz)](http://graphics.stanford.edu/pub/3Dscanrep/bunny.tar.gz)

#### 📦 Asset Folder Setup

1. Extract `bunny.tar.gz`. By default, this creates a `bunny/` directory containing two subfolders: `data/` and `reconstruction/`.
2. Move all `.ply` files located inside `bunny/reconstruction/` directly into the root `bunny/` folder.
3. Delete both `data/` and `reconstruction/` directories.

The final structure of the `bunny/` folder should look like this:

```text
bunny/
├── bun_zipper.ply
├── bun_zipper_res2.ply
├── bun_zipper_res3.ply
└── ... (other .ply files)

```

---

## 🐳 Docker Setup & Environment

The project is containerized using an **Ubuntu 24.04** base image equipped with modern rendering libraries, software rasterization fallback, and X11 forwarding capabilities.

### Build the Docker Image

Run the following command in the project root to create the `3dgraphics` image:

```bash
make build
```

---

## 🚀 Running the Code

You can run any `.cpp` file directly inside the containerized environment using `make`.

### Cross-Platform Execution

The Makefile automatically detects your operating system:

* **Linux / macOS:** Uses default native display output (`DISPLAY=:0`).
* **Windows (WSL / Docker Desktop):** Automatically routes display output to XLaunch / VcXsrv via `host.docker.internal:0`.

> **Note for Windows Users:** Make sure your X Server (e.g., XLaunch or VcXsrv) is running with *"Disable access control"* checked before launching the container.

### Commands

* **Run a specific C++ file:**
```bash
make run S1/window.cpp
```


*(If no path is provided, it defaults to running `main.cpp`)*
* **Access the Container's Interactive Shell:**
```bash
make bash
```


* **Clean up Container Images and Executables:**
```bash
make clean
```



---

## 📁 Repository Structure

```text
.
├── glad/             # Embedded GLAD generator sources
├── bunny/            # Extracted .ply model files
├── S1/               # Week 1: Hello World (Window) & Sierpinski Triangle
├── shared.h          # Shared helper functions and C++ generators
├── shared.cpp        # Shared implementation file
├── Dockerfile        # Environment specification (OpenGL, GLFW, GLM, FFmpeg)
├── Makefile          # Cross-platform build & run scripts
└── README.md         # Project documentation
```
