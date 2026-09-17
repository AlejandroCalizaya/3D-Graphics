#include "../../shared.h"
#include "fast_marching.h"

#include <chrono>
#include <limits>

using namespace std;

const unsigned int SCR_WIDTH = 1000;
const unsigned int SCR_HEIGHT = 800;

glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 2.5f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

float objectYaw = 0.0f;
float objectPitch = 0.0f;

bool isDragging = false;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

const int STEP = 50;
const float SLOWNESS = 1.0f;

const char *meshVertexSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;
out vec3 vColor;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    vColor = aColor;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

const char *meshFragmentSource = R"(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 vColor;

uniform vec3 cameraPos;

void main()
{
    vec3 dx = dFdx(FragPos);
    vec3 dy = dFdy(FragPos);
    vec3 normal = normalize(cross(dx, dy));

    vec3 lightDir = normalize(cameraPos - FragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    float diffuse = diff * 0.75;
    float ambient = 0.30;

    float lighting = ambient + diffuse;
    vec3 finalColor = vColor * lighting;
    FragColor = vec4(finalColor, 1.0f);
}
)";

const char *simpleVertexSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

const char *simpleFragmentSource = R"(
#version 330 core
out vec4 FragColor;

uniform vec3 color;

void main()
{
    FragColor = vec4(color, 1.0f);
}
)";

void processInput(GLFWwindow *window, int &anchor, int numVertices)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float cameraSpeed = 4.0f * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        anchor = min(numVertices - 1, anchor + STEP);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        anchor = max(0, anchor - STEP);
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
        {
            isDragging = true;
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            lastX = static_cast<float>(xpos);
            lastY = static_cast<float>(ypos);
        }
        else if (action == GLFW_RELEASE)
        {
            isDragging = false;
        }
    }
}

void cursor_position_callback(GLFWwindow *window, double xposIn, double yposIn)
{
    if (!isDragging)
        return;

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    float xoffset = xpos - lastX;
    float yoffset = ypos - lastY;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.4f;
    objectYaw += xoffset * sensitivity;
    objectPitch += yoffset * sensitivity;
}

int main()
{
    if (!glfwInit())
    {
        cerr << "Error inicializando GLFW" << endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Fast Marching", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        return -1;

    glEnable(GL_DEPTH_TEST);

    string filename = "bunny/bun_zipper_res4.ply";

    vector<Vertex> vertices;
    vector<CHE> halfEdges;
    vector<unsigned int> faceIndices;

    if (!loadPLY(filename, vertices, halfEdges, faceIndices))
    {
        cerr << "Error cargando la malla." << endl;
        return -1;
    }

    int numVertices = static_cast<int>(vertices.size());
    int numFaces = static_cast<int>(faceIndices.size() / 3);

    vector<vector<int>> adj;
    unordered_set<std::uint64_t> edges;
    FMM::buildTopology(halfEdges, faceIndices, adj, edges);

    size_t numEdges = 0;
    for (const auto &nbrs : adj)
        numEdges += nbrs.size();
    numEdges /= 2;

    unsigned int meshProgram = createShaderProgram(meshVertexSource, meshFragmentSource);
    unsigned int simpleProgram = createShaderProgram(simpleVertexSource, simpleFragmentSource);

    unsigned int meshVAO, meshVBO, meshEBO;
    glGenVertexArrays(1, &meshVAO);
    glGenBuffers(1, &meshVBO);
    glGenBuffers(1, &meshEBO);

    glBindVertexArray(meshVAO);

    glBindBuffer(GL_ARRAY_BUFFER, meshVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
    glEnableVertexAttribArray(0);

    vector<float> vertexColors(numVertices * 3, 0.0f);
    unsigned int colorVBO;
    glGenBuffers(1, &colorVBO);
    glBindBuffer(GL_ARRAY_BUFFER, colorVBO);
    glBufferData(GL_ARRAY_BUFFER, vertexColors.size() * sizeof(float), vertexColors.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, faceIndices.size() * sizeof(unsigned int), faceIndices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    unsigned int markerVAO, markerVBO;
    glGenVertexArrays(1, &markerVAO);
    glGenBuffers(1, &markerVBO);
    glBindVertexArray(markerVAO);
    glBindBuffer(GL_ARRAY_BUFFER, markerVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    int anchor = 0;
    int lastAnchor = -1;

    int meshModelLoc = glGetUniformLocation(meshProgram, "model");
    int meshViewLoc = glGetUniformLocation(meshProgram, "view");
    int meshProjLoc = glGetUniformLocation(meshProgram, "projection");
    int meshCamLoc = glGetUniformLocation(meshProgram, "cameraPos");

    int simpleModelLoc = glGetUniformLocation(simpleProgram, "model");
    int simpleViewLoc = glGetUniformLocation(simpleProgram, "view");
    int simpleProjLoc = glGetUniformLocation(simpleProgram, "projection");
    int simpleColorLoc = glGetUniformLocation(simpleProgram, "color");

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window, anchor, numVertices);

        if (anchor != lastAnchor)
        {
            lastAnchor = anchor;

            vector<int> sources = {anchor};
            FMM::FMMResult res = FMM::fastMarching(vertices, adj, edges, sources, SLOWNESS);

            float range = res.maxU - res.minU;
            if (range < 1e-9f)
                range = 1.0f;

            for (int i = 0; i < numVertices; ++i)
            {
                float t;
                if (res.U[i] >= numeric_limits<float>::infinity())
                    t = 0.0f;
                else
                    t = (res.U[i] - res.minU) / range;

                glm::vec3 c = FMM::jet(t);
                vertexColors[i * 3 + 0] = c.x;
                vertexColors[i * 3 + 1] = c.y;
                vertexColors[i * 3 + 2] = c.z;
            }

            glBindBuffer(GL_ARRAY_BUFFER, colorVBO);
            glBufferData(GL_ARRAY_BUFFER,
                         vertexColors.size() * sizeof(float),
                         vertexColors.data(),
                         GL_DYNAMIC_DRAW);

            vector<float> markerVerts = {
                vertices[anchor].x, vertices[anchor].y, vertices[anchor].z
            };
            glBindBuffer(GL_ARRAY_BUFFER, markerVBO);
            glBufferData(GL_ARRAY_BUFFER,
                         markerVerts.size() * sizeof(float),
                         markerVerts.data(),
                         GL_DYNAMIC_DRAW);
        }

        glClearColor(0.10f, 0.10f, 0.13f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float aspect = static_cast<float>(width) / static_cast<float>(height);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), cameraUp);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::rotate(model, glm::radians(objectPitch), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(objectYaw), glm::vec3(0.0f, 1.0f, 0.0f));

        glUseProgram(meshProgram);
        glUniformMatrix4fv(meshModelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(meshViewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(meshProjLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(meshCamLoc, 1, glm::value_ptr(cameraPos));

        glBindVertexArray(meshVAO);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(faceIndices.size()), GL_UNSIGNED_INT, 0);

        glUseProgram(simpleProgram);
        glUniformMatrix4fv(simpleModelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(simpleViewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(simpleProjLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3f(simpleColorLoc, 1.0f, 1.0f, 1.0f);

        glPointSize(14.0f);
        glBindVertexArray(markerVAO);
        glDrawArrays(GL_POINTS, 0, 1);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &meshVAO);
    glDeleteBuffers(1, &meshVBO);
    glDeleteBuffers(1, &colorVBO);
    glDeleteBuffers(1, &meshEBO);
    glDeleteVertexArrays(1, &markerVAO);
    glDeleteBuffers(1, &markerVBO);
    glDeleteProgram(meshProgram);
    glDeleteProgram(simpleProgram);

    glfwTerminate();
    return 0;
}
