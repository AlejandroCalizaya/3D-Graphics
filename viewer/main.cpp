#include "Camera.h"
#include "Renderer.h"
#include "Ui.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <iostream>

int main()
{
    if (!glfwInit())
    {
        std::cerr << "No se pudo inicializar GLFW.\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(1280, 800, "3D Graphics Viewer", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "No se pudo inicializar GLAD.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    ViewerRenderer renderer;
    if (!renderer.initialize())
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    std::vector<SceneObject> objects;
    ViewerCamera camera;
    RenderSettings settings;
    ViewerUiState ui;
    ViewerUi interface;
    bool dragging = false;
    double lastX = 0.0;
    double lastY = 0.0;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (!io.WantCaptureMouse)
        {
            const bool leftPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            double x;
            double y;
            glfwGetCursorPos(window, &x, &y);
            if (leftPressed && !dragging)
            {
                int framebufferWidth;
                int framebufferHeight;
                glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
                selectFastMarchingVertex(objects, ui, camera, framebufferWidth, framebufferHeight, x, y);
            }
            if (leftPressed && dragging)
                camera.orbit(static_cast<float>(x - lastX), static_cast<float>(y - lastY));
            dragging = leftPressed;
            lastX = x;
            lastY = y;

            const double scrollY = io.MouseWheel;
            if (scrollY != 0.0)
                camera.zoom(static_cast<float>(scrollY));
        }

        int width;
        int height;
        glfwGetFramebufferSize(window, &width, &height);
        renderer.render(objects, camera, settings, width, height);
        if (interface.draw(objects, ui, settings, camera))
            glfwSetWindowShouldClose(window, true);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    for (SceneObject &object : objects)
        object.gpu.destroy();
    renderer.destroy();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}