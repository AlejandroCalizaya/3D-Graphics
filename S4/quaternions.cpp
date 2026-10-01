#include "../shared.h"

const unsigned int SCR_WIDTH = 1000;
const unsigned int SCR_HEIGHT = 800;

const char *VIDEO_FILE = "quaternions.mp4";
const int VIDEO_FPS = 30;
const double VIDEO_SECONDS = 30.0;

const glm::vec3 SCENE_CENTER(0.0f, 0.0f, -4.75f);

const char *vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec3 FragPos;
void main()
{
    vec4 worldPosition = model * vec4(aPos, 1.0);
    FragPos = worldPosition.xyz;
    gl_Position = projection * view * worldPosition;
}
)";

const char *fragmentShaderSource = R"(
#version 330 core
in vec3 FragPos;
out vec4 FragColor;
uniform vec3 objectColor;
uniform vec3 cameraPosition;
void main()
{
    vec3 normal = normalize(cross(dFdx(FragPos), dFdy(FragPos)));
    vec3 lightDirection = normalize(vec3(4.0, 5.0, 4.0) - FragPos);
    vec3 viewDirection = normalize(cameraPosition - FragPos);
    float diffuse = max(dot(normal, lightDirection), 0.0);
    float specular = pow(max(dot(reflect(-lightDirection, normal), viewDirection), 0.0), 32.0);
    FragColor = vec4(objectColor * (0.2 + 0.8 * diffuse) + vec3(specular * 0.25), 1.0);
}
)";

void updateWindowTitle(GLFWwindow *window, const camera &activeCamera)
{
    const char *projectionName = activeCamera.isPerspective() ? "Perspectiva" : "Ortografica";
    const char *cameraMode = activeCamera.isAnimated() ? "Animada" : "Arcball";
    std::string title = "Quaternions: ";
    title += cameraMode;
    title += " - ";
    title += projectionName;
    title += " [A camara, B proyeccion, R reset, rueda zoom]";
    glfwSetWindowTitle(window, title.c_str());
}

int main()
{
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Quaternions", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    camera activeCamera(SCENE_CENTER, 14.0f);
    glfwSetWindowUserPointer(window, &activeCamera);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, [](GLFWwindow *currentWindow, int button, int action, int) {
        camera *activeCamera = static_cast<camera *>(glfwGetWindowUserPointer(currentWindow));
        if (button != GLFW_MOUSE_BUTTON_LEFT)
            return;
        double x;
        double y;
        glfwGetCursorPos(currentWindow, &x, &y);
        if (action == GLFW_PRESS) {
            activeCamera->beginDrag(x, y);
            updateWindowTitle(currentWindow, *activeCamera);
        } else if (action == GLFW_RELEASE)
            activeCamera->endDrag();
    });
    glfwSetCursorPosCallback(window, [](GLFWwindow *currentWindow, double x, double y) {
        static_cast<camera *>(glfwGetWindowUserPointer(currentWindow))->drag(x, y);
    });
    glfwSetScrollCallback(window, [](GLFWwindow *currentWindow, double, double yOffset) {
        static_cast<camera *>(glfwGetWindowUserPointer(currentWindow))->zoom(yOffset);
    });
    glfwSetKeyCallback(window, [](GLFWwindow *currentWindow, int key, int, int action, int) {
        if (action != GLFW_PRESS)
            return;
        camera *activeCamera = static_cast<camera *>(glfwGetWindowUserPointer(currentWindow));
        if (key == GLFW_KEY_ESCAPE)
            glfwSetWindowShouldClose(currentWindow, true);
        else if (key == GLFW_KEY_A)
            activeCamera->toggleAnimated();
        else if (key == GLFW_KEY_B)
            activeCamera->toggleProjection();
        else if (key == GLFW_KEY_R)
            activeCamera->reset();
        else
            return;
        updateWindowTitle(currentWindow, *activeCamera);
    });

    glEnable(GL_DEPTH_TEST);
    const unsigned int shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    std::vector<Vertex> sphereVertices;
    std::vector<CHE> sphereHalfEdges;
    std::vector<unsigned int> sphereFaces;
    std::vector<unsigned int> sphereEdges;
    buildSphere(32, 24, 0.65f, sphereVertices, sphereHalfEdges, sphereFaces, sphereEdges);

    unsigned int sphereVAO;
    unsigned int sphereVBO;
    unsigned int sphereEBO;
    createMeshBuffers(sphereVertices, sphereFaces, sphereVAO, sphereVBO, sphereEBO);

    const int modelLoc = glGetUniformLocation(shaderProgram, "model");
    const int viewLoc = glGetUniformLocation(shaderProgram, "view");
    const int projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    const int objectColorLoc = glGetUniformLocation(shaderProgram, "objectColor");
    const int cameraPositionLoc = glGetUniformLocation(shaderProgram, "cameraPosition");
    float lastFrame = static_cast<float>(glfwGetTime());
    updateWindowTitle(window, activeCamera);

    int width;
    int height;
    glfwGetFramebufferSize(window, &width, &height);
    VideoRecorder recorder;
    recorder.start(VIDEO_FILE, width, height, VIDEO_FPS, VIDEO_SECONDS);

    while (!glfwWindowShouldClose(window)) {
        const float currentFrame = static_cast<float>(glfwGetTime());
        activeCamera.update(currentFrame - lastFrame);
        lastFrame = currentFrame;

        int windowWidth;
        int windowHeight;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);
        activeCamera.setViewport(windowWidth, windowHeight);

        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.06f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const float aspect = static_cast<float>(width) / static_cast<float>(height);
        const glm::mat4 projection = activeCamera.projectionMatrix(aspect);
        const glm::mat4 view = activeCamera.viewMatrix();
        const glm::vec3 cameraPosition = activeCamera.position();

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(cameraPositionLoc, 1, glm::value_ptr(cameraPosition));
        glBindVertexArray(sphereVAO);

        const glm::mat4 nearSphereModel = glm::translate(
            glm::mat4(1.0f), glm::vec3(2.2f * glm::sin(currentFrame), 0.0f, -1.5f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(nearSphereModel));
        glUniform3f(objectColorLoc, 0.95f, 0.35f, 0.12f);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(sphereFaces.size()), GL_UNSIGNED_INT, nullptr);

        const glm::mat4 farSphereModel = glm::translate(
            glm::mat4(1.0f), glm::vec3(2.2f * glm::sin(currentFrame + glm::pi<float>()), 0.0f, -8.0f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(farSphereModel));
        glUniform3f(objectColorLoc, 0.12f, 0.45f, 0.95f);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(sphereFaces.size()), GL_UNSIGNED_INT, nullptr);

        recorder.capture(glfwGetTime());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    recorder.stop();
    glDeleteVertexArrays(1, &sphereVAO);
    glDeleteBuffers(1, &sphereVBO);
    glDeleteBuffers(1, &sphereEBO);
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}