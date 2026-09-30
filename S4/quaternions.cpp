#include "../shared.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

const unsigned int SCR_WIDTH = 1000;
const unsigned int SCR_HEIGHT = 800;

class camera
{
public:
    explicit camera(float distance = 8.0f)
        : distance_(distance), orientation_(1.0f, 0.0f, 0.0f, 0.0f), target_(0.0f), dragging_(false),
          animated_(true), animationTime_(0.0f), viewportWidth_(SCR_WIDTH), viewportHeight_(SCR_HEIGHT)
    {
    }

    void setViewport(int width, int height)
    {
        viewportWidth_ = width;
        viewportHeight_ = height;
    }

    void beginDrag(double x, double y)
    {
        dragging_ = true;
        lastArcballPoint_ = arcballPoint(x, y);
    }

    void drag(double x, double y)
    {
        if (!dragging_ || animated_)
            return;

        const glm::vec3 currentPoint = arcballPoint(x, y);
        const glm::vec3 axis = glm::cross(lastArcballPoint_, currentPoint);
        const float dotProduct = glm::clamp(glm::dot(lastArcballPoint_, currentPoint), -1.0f, 1.0f);
        if (glm::length(axis) > 0.00001f)
            orientation_ = glm::normalize(glm::angleAxis(std::acos(dotProduct), glm::normalize(axis)) * orientation_);
        lastArcballPoint_ = currentPoint;
    }

    void endDrag()
    {
        dragging_ = false;
    }

    void zoom(double offset)
    {
        distance_ = glm::clamp(distance_ - static_cast<float>(offset) * 0.65f, 2.0f, 20.0f);
    }

    void update(float deltaTime)
    {
        if (animated_) {
            animationTime_ += deltaTime;
            orientation_ = glm::angleAxis(animationTime_ * 0.35f, glm::vec3(0.0f, 1.0f, 0.0f)) *
                           glm::angleAxis(glm::sin(animationTime_ * 0.65f) * 0.22f, glm::vec3(1.0f, 0.0f, 0.0f));
        }
    }

    void setAnimated(bool animated)
    {
        animated_ = animated;
        dragging_ = false;
    }

    void toggleAnimated()
    {
        setAnimated(!animated_);
    }

    bool isAnimated() const
    {
        return animated_;
    }

    glm::mat4 viewMatrix() const
    {
        return glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -distance_)) * glm::mat4_cast(orientation_) *
               glm::translate(glm::mat4(1.0f), -target_);
    }

    glm::vec3 position() const
    {
        return target_ + glm::inverse(orientation_) * glm::vec3(0.0f, 0.0f, distance_);
    }

private:
    glm::vec3 arcballPoint(double x, double y) const
    {
        const float normalizedX = static_cast<float>(2.0 * x / viewportWidth_ - 1.0);
        const float normalizedY = static_cast<float>(1.0 - 2.0 * y / viewportHeight_);
        glm::vec3 point(normalizedX, normalizedY, 0.0f);
        const float lengthSquared = point.x * point.x + point.y * point.y;
        if (lengthSquared <= 1.0f)
            point.z = glm::sqrt(1.0f - lengthSquared);
        else
            point = glm::normalize(point);
        return glm::normalize(point);
    }

    float distance_;
    glm::quat orientation_;
    glm::vec3 target_;
    glm::vec3 lastArcballPoint_;
    bool dragging_;
    bool animated_;
    float animationTime_;
    int viewportWidth_;
    int viewportHeight_;
};

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

void updateWindowTitle(GLFWwindow *window, const camera &activeCamera, bool perspectiveProjection)
{
    const char *projectionName = perspectiveProjection ? "Perspectiva" : "Ortografica";
    const char *cameraMode = activeCamera.isAnimated() ? "Animada" : "Arcball";
    std::string title = "Quaternions: ";
    title += cameraMode;
    title += " - ";
    title += projectionName;
    title += " [A camara, B proyeccion]";
    glfwSetWindowTitle(window, title.c_str());
}

int main()
{
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "S4 - Quaternions", nullptr, nullptr);
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

    camera activeCamera;
    glfwSetWindowUserPointer(window, &activeCamera);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, [](GLFWwindow *currentWindow, int button, int action, int) {
        camera *activeCamera = static_cast<camera *>(glfwGetWindowUserPointer(currentWindow));
        if (button != GLFW_MOUSE_BUTTON_LEFT)
            return;
        double x;
        double y;
        glfwGetCursorPos(currentWindow, &x, &y);
        if (action == GLFW_PRESS)
            activeCamera->beginDrag(x, y);
        else if (action == GLFW_RELEASE)
            activeCamera->endDrag();
    });
    glfwSetCursorPosCallback(window, [](GLFWwindow *currentWindow, double x, double y) {
        static_cast<camera *>(glfwGetWindowUserPointer(currentWindow))->drag(x, y);
    });
    glfwSetScrollCallback(window, [](GLFWwindow *currentWindow, double, double yOffset) {
        static_cast<camera *>(glfwGetWindowUserPointer(currentWindow))->zoom(yOffset);
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
    bool perspectiveProjection = true;
    bool projectionKeyWasPressed = false;
    bool cameraKeyWasPressed = false;
    float lastFrame = static_cast<float>(glfwGetTime());
    updateWindowTitle(window, activeCamera, perspectiveProjection);

    while (!glfwWindowShouldClose(window)) {
        const float currentFrame = static_cast<float>(glfwGetTime());
        activeCamera.update(currentFrame - lastFrame);
        lastFrame = currentFrame;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        const bool projectionKeyPressed = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;
        if (projectionKeyPressed && !projectionKeyWasPressed)
            perspectiveProjection = !perspectiveProjection;
        projectionKeyWasPressed = projectionKeyPressed;

        const bool cameraKeyPressed = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
        if (cameraKeyPressed && !cameraKeyWasPressed)
            activeCamera.toggleAnimated();
        cameraKeyWasPressed = cameraKeyPressed;

        int width;
        int height;
        glfwGetFramebufferSize(window, &width, &height);
        activeCamera.setViewport(width, height);
        updateWindowTitle(window, activeCamera, perspectiveProjection);
        glViewport(0, 0, width, height);
        glClearColor(0.06f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const float aspect = static_cast<float>(width) / static_cast<float>(height);
        const glm::mat4 projection = perspectiveProjection
                                         ? glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f)
                                         : glm::ortho(-5.0f * aspect, 5.0f * aspect, -5.0f, 5.0f, 0.1f, 100.0f);
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

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &sphereVAO);
    glDeleteBuffers(1, &sphereVBO);
    glDeleteBuffers(1, &sphereEBO);
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}