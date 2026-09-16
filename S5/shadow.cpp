#include "../shared.h"

using namespace std;
const unsigned int SCR_WIDTH = 1000;
const unsigned int SCR_HEIGHT = 800;
const unsigned int SHADOW_WIDTH = 1024;
const unsigned int SHADOW_HEIGHT = 1024;
glm::vec3 cameraPos(4.0f, 3.0f, 6.0f);
glm::vec3 cameraFront(-0.45f, -0.25f, -0.85f);
glm::vec3 cameraUp(0.0f, 1.0f, 0.0f);
float deltaTime = 0.0f;
float lastFrame = 0.0f;

const char *vertexShaderDepth = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 lightSpaceMatrix;
uniform mat4 model;
void main()
{
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
}
)";

const char *fragmentShaderDepth = R"(
#version 330 core
void main()
{
}
)";

const char *vertexShaderMain = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 lightSpaceMatrix;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec3 FragPos;
out vec4 FragPosLightSpace;
void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;
    FragPosLightSpace = lightSpaceMatrix * worldPos;
    gl_Position = projection * view * worldPos;
}
)";
const char *fragmentShaderMain = R"(
#version 330 core
in vec3 FragPos;
in vec4 FragPosLightSpace;
out vec4 FragColor;
uniform sampler2D shadowMap;
uniform vec3 lightPos;
uniform vec3 cameraPos;
uniform vec3 color;

float shadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float bias = max(0.04 * (1.0 - max(dot(normal, lightDir), 0.0)), 0.002);
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float closestDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += projCoords.z - bias > closestDepth ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}
void main()
{
    vec3 dx = dFdx(FragPos);
    vec3 dy = dFdy(FragPos);
    vec3 normal = normalize(cross(dx, dy));
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 viewDir = normalize(cameraPos - FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float diffuse = max(dot(normal, lightDir), 0.0);
    float specular = pow(max(dot(normal, halfwayDir), 0.0), 48.0);
    float shadow = shadowCalculation(FragPosLightSpace, normal, lightDir);
    vec3 lighting = 0.18 * color + (1.0 - shadow) * (0.75 * diffuse * color + 0.35 * specular * vec3(1.0));
    FragColor = vec4(lighting, 1.0);
}
)";

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float cameraSpeed = 3.5f * deltaTime;
    glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        cameraPos -= cameraSpeed * right;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        cameraPos += cameraSpeed * right;
}

void createPlaneBuffers(unsigned int &VAO, unsigned int &VBO, unsigned int &EBO)
{
    vector<Vertex> vertices = {
        {-5.0f, -1.0f, -5.0f, 0.0f, 0.0f},
        { 5.0f, -1.0f, -5.0f, 5.0f, 0.0f},
        { 5.0f, -1.0f,  5.0f, 5.0f, 5.0f},
        {-5.0f, -1.0f,  5.0f, 0.0f, 5.0f}};
    vector<unsigned int> indices = {0, 2, 1, 0, 3, 2};
    createMeshBuffers(vertices, indices, VAO, VBO, EBO);
}

void drawObject(unsigned int shader, unsigned int VAO, GLsizei indexCount, const glm::mat4 &model)
{
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
}

int main()
{
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Shadow Mapping", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    vector<Vertex> sphereVertices;
    vector<CHE> sphereHalfEdges;
    vector<unsigned int> sphereIndices;
    vector<unsigned int> sphereEdgeIndices;
    buildSphere(64, 64, sphereVertices, sphereHalfEdges, sphereIndices, sphereEdgeIndices);

    unsigned int sphereVAO, sphereVBO, sphereEBO;
    unsigned int planeVAO, planeVBO, planeEBO;
    createMeshBuffers(sphereVertices, sphereIndices, sphereVAO, sphereVBO, sphereEBO);
    createPlaneBuffers(planeVAO, planeVBO, planeEBO);

    unsigned int depthShader = createShaderProgram(vertexShaderDepth, fragmentShaderDepth);
    unsigned int mainShader = createShaderProgram(vertexShaderMain, fragmentShaderMain);

    unsigned int depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);

    unsigned int depthMap;
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        glDeleteFramebuffers(1, &depthMapFBO);
        glDeleteTextures(1, &depthMap);
        return -1;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glUseProgram(mainShader);
    glUniform1i(glGetUniformLocation(mainShader, "shadowMap"), 0);

    glm::mat4 sphereModel = glm::mat4(1.0f);
    glm::mat4 planeModel = glm::mat4(1.0f);
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        processInput(window);

        float lightAngle = currentFrame * 0.65f;
        glm::vec3 lightPos(3.5f * cos(lightAngle), 4.5f, 3.5f * sin(lightAngle));
        glm::mat4 lightProjection = glm::ortho(-6.0f, 6.0f, -6.0f, 6.0f, 0.5f, 12.0f);
        glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f, -0.5f, 0.0f), cameraUp);
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        glUseProgram(depthShader);
        glUniformMatrix4fv(glGetUniformLocation(depthShader, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
        drawObject(depthShader, sphereVAO, static_cast<GLsizei>(sphereIndices.size()), sphereModel);
        drawObject(depthShader, planeVAO, 6, planeModel);

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glClearColor(0.08f, 0.10f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(mainShader);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glUniformMatrix4fv(glGetUniformLocation(mainShader, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
        glUniformMatrix4fv(glGetUniformLocation(mainShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(mainShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(mainShader, "lightPos"), 1, glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(mainShader, "cameraPos"), 1, glm::value_ptr(cameraPos));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depthMap);

        glUniform3f(glGetUniformLocation(mainShader, "color"), 0.18f, 0.45f, 0.85f);
        drawObject(mainShader, sphereVAO, static_cast<GLsizei>(sphereIndices.size()), sphereModel);
        glUniform3f(glGetUniformLocation(mainShader, "color"), 0.55f, 0.57f, 0.60f);
        drawObject(mainShader, planeVAO, 6, planeModel);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteFramebuffers(1, &depthMapFBO);
    glDeleteTextures(1, &depthMap);
    glDeleteVertexArrays(1, &sphereVAO);
    glDeleteBuffers(1, &sphereVBO);
    glDeleteBuffers(1, &sphereEBO);
    glDeleteVertexArrays(1, &planeVAO);
    glDeleteBuffers(1, &planeVBO);
    glDeleteBuffers(1, &planeEBO);
    glDeleteProgram(depthShader);
    glDeleteProgram(mainShader);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
