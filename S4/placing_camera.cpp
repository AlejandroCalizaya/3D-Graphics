#include "../shared.h"

const unsigned int SCR_WIDTH = 1000;
const unsigned int SCR_HEIGHT = 800;

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
    vec3 color = objectColor * (0.2 + 0.8 * diffuse) + vec3(1.0) * specular * 0.25;
    FragColor = vec4(color, 1.0);
}
)";

void updateWindowTitle(GLFWwindow *window, bool perspectiveProjection)
{
    const char *projectionName = perspectiveProjection ? "Perspectiva" : "Ortografica";
    std::string title = "Camara:";
    title += projectionName;
    title += " [B alternar, P perspectiva, O ortografica]";
    glfwSetWindowTitle(window, title.c_str());
}

int main()
{
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "S4 - Placing Camera", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    const unsigned int shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    std::vector<Vertex> sphereVertices;
    std::vector<CHE> sphereHalfEdges;
    std::vector<unsigned int> sphereFaces;
    std::vector<unsigned int> sphereEdges;
    buildSphere(24, 16, 0.65f, sphereVertices, sphereHalfEdges, sphereFaces, sphereEdges);

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
    bool projectionButtonWasPressed = false;
    updateWindowTitle(window, perspectiveProjection);

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        const bool previousProjection = perspectiveProjection;
        const bool projectionKeyPressed = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
        if (projectionKeyPressed && !projectionKeyWasPressed)
            perspectiveProjection = !perspectiveProjection;
        if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
            perspectiveProjection = false;
        if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
            perspectiveProjection = true;
        const bool projectionButtonPressed = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;
        if (projectionButtonPressed && !projectionButtonWasPressed) {
            perspectiveProjection = !perspectiveProjection;
        }
        if (perspectiveProjection != previousProjection)
            updateWindowTitle(window, perspectiveProjection);
        projectionKeyWasPressed = projectionKeyPressed;
        projectionButtonWasPressed = projectionButtonPressed;

        int width;
        int height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.06f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const float time = static_cast<float>(glfwGetTime());
        const glm::vec3 target(0.0f, 0.0f, -4.0f);
        const glm::vec3 cameraPosition(0.0f, 0.0f, 4.0f);
        const glm::mat4 view = glm::lookAt(cameraPosition, target, glm::vec3(0.0f, 1.0f, 0.0f));
        const float aspect = static_cast<float>(width) / static_cast<float>(height);
        const glm::mat4 projection = perspectiveProjection
                                         ? glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f)
                                         : glm::ortho(-5.0f * aspect, 5.0f * aspect, -5.0f, 5.0f, 0.1f, 100.0f);

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(cameraPositionLoc, 1, glm::value_ptr(cameraPosition));

        const glm::mat4 nearSphereModel = glm::translate(
            glm::mat4(1.0f), glm::vec3(2.2f * sin(time), 0.0f, -1.5f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(nearSphereModel));
        glUniform3f(objectColorLoc, 0.95f, 0.35f, 0.12f);
        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(sphereFaces.size()), GL_UNSIGNED_INT, nullptr);

        const glm::mat4 farSphereModel = glm::translate(
            glm::mat4(1.0f), glm::vec3(2.2f * sin(time + glm::pi<float>()), 0.0f, -8.0f));
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