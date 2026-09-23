#define STB_IMAGE_IMPLEMENTATION

#include "../shared.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <limits>
#include <unordered_map>

namespace fs = std::filesystem;

struct SceneVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

struct Material {
    glm::vec3 ambient{0.2f};
    glm::vec3 diffuse{0.8f};
    glm::vec3 specular{0.0f};
    float shininess = 32.0f;
    std::string diffuseMap;
    unsigned int texture = 0;
};

struct Submesh {
    size_t firstVertex = 0;
    size_t vertexCount = 0;
    std::string materialName;
};

struct Scene {
    std::vector<SceneVertex> vertices;
    std::vector<Submesh> submeshes;
    std::unordered_map<std::string, Material> materials;
    glm::vec3 minBounds{std::numeric_limits<float>::max()};
    glm::vec3 maxBounds{std::numeric_limits<float>::lowest()};
};

struct FaceIndex {
    int position = 0;
    int texCoord = 0;
    int normal = 0;
};

static std::string trim(const std::string &value) {
    const size_t first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos)
        return {};
    const size_t last = value.find_last_not_of(" \t\r");
    return value.substr(first, last - first + 1);
}

static std::string normalizePath(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

static int resolveIndex(int index, size_t count) {
    if (index > 0)
        return index - 1;
    if (index < 0)
        return static_cast<int>(count) + index;
    return -1;
}

static FaceIndex parseFaceIndex(const std::string &token) {
    FaceIndex result;
    std::stringstream stream(token);
    std::string part;
    std::getline(stream, part, '/');
    result.position = part.empty() ? 0 : std::stoi(part);
    if (std::getline(stream, part, '/'))
        result.texCoord = part.empty() ? 0 : std::stoi(part);
    if (std::getline(stream, part, '/'))
        result.normal = part.empty() ? 0 : std::stoi(part);
    return result;
}

static bool loadMaterials(const fs::path &filename, std::unordered_map<std::string, Material> &materials) {
    std::ifstream input(filename);
    if (!input) {
        std::cerr << "No se pudo abrir el MTL: " << filename << '\n';
        return false;
    }

    Material *current = nullptr;
    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream stream(line);
        std::string directive;
        stream >> directive;
        if (directive == "newmtl") {
            std::string name;
            stream >> name;
            current = &materials[name];
        } else if (current != nullptr && directive == "Ka")
            stream >> current->ambient.r >> current->ambient.g >> current->ambient.b;
        else if (current != nullptr && directive == "Kd")
            stream >> current->diffuse.r >> current->diffuse.g >> current->diffuse.b;
        else if (current != nullptr && directive == "Ks")
            stream >> current->specular.r >> current->specular.g >> current->specular.b;
        else if (current != nullptr && directive == "Ns")
            stream >> current->shininess;
        else if (current != nullptr && directive == "d") {
            float opacity;
            stream >> opacity;
        } else if (current != nullptr && directive == "map_Kd") {
            std::string texturePath;
            std::getline(stream, texturePath);
            current->diffuseMap = normalizePath(trim(texturePath));
        }
    }
    return true;
}

static void beginSubmesh(Scene &scene, const std::string &materialName) {
    if (!scene.submeshes.empty() && scene.submeshes.back().materialName == materialName)
        return;
    scene.submeshes.push_back({scene.vertices.size(), 0, materialName});
}

static void appendTriangle(Scene &scene, const std::array<FaceIndex, 3> &indices,
                           const std::vector<glm::vec3> &positions, const std::vector<glm::vec2> &texCoords,
                           const std::vector<glm::vec3> &normals, const std::string &materialName) {
    glm::vec3 normal(0.0f);
    const int p0 = resolveIndex(indices[0].position, positions.size());
    const int p1 = resolveIndex(indices[1].position, positions.size());
    const int p2 = resolveIndex(indices[2].position, positions.size());
    if (p0 < 0 || p1 < 0 || p2 < 0 || p0 >= static_cast<int>(positions.size()) ||
        p1 >= static_cast<int>(positions.size()) || p2 >= static_cast<int>(positions.size()))
        return;

    normal = glm::normalize(glm::cross(positions[p1] - positions[p0], positions[p2] - positions[p0]));
    beginSubmesh(scene, materialName);
    for (const FaceIndex &index : indices) {
        const int positionIndex = resolveIndex(index.position, positions.size());
        const int texCoordIndex = resolveIndex(index.texCoord, texCoords.size());
        const int normalIndex = resolveIndex(index.normal, normals.size());
        SceneVertex vertex;
        vertex.position = positions[positionIndex];
        vertex.texCoord = texCoordIndex >= 0 && texCoordIndex < static_cast<int>(texCoords.size())
                              ? texCoords[texCoordIndex]
                              : glm::vec2(0.0f);
        vertex.normal =
            normalIndex >= 0 && normalIndex < static_cast<int>(normals.size()) ? normals[normalIndex] : normal;
        scene.vertices.push_back(vertex);
        scene.minBounds = glm::min(scene.minBounds, vertex.position);
        scene.maxBounds = glm::max(scene.maxBounds, vertex.position);
        ++scene.submeshes.back().vertexCount;
    }
}

static bool loadObj(const fs::path &filename, Scene &scene) {
    std::ifstream input(filename);
    if (!input) {
        std::cerr << "No se pudo abrir el OBJ: " << filename << '\n';
        return false;
    }

    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> texCoords;
    std::vector<glm::vec3> normals;
    std::string currentMaterial;
    std::string line;

    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream stream(line);
        std::string directive;
        stream >> directive;
        if (directive == "mtllib") {
            std::string materialFile;
            stream >> materialFile;
            loadMaterials(filename.parent_path() / normalizePath(materialFile), scene.materials);
        } else if (directive == "v") {
            glm::vec3 position;
            stream >> position.x >> position.y >> position.z;
            positions.push_back(position);
        } else if (directive == "vt") {
            glm::vec2 texCoord;
            stream >> texCoord.x >> texCoord.y;
            texCoords.push_back(texCoord);
        } else if (directive == "vn") {
            glm::vec3 normal;
            stream >> normal.x >> normal.y >> normal.z;
            normals.push_back(glm::normalize(normal));
        } else if (directive == "usemtl")
            stream >> currentMaterial;
        else if (directive == "f") {
            std::vector<FaceIndex> face;
            std::string token;
            while (stream >> token)
                face.push_back(parseFaceIndex(token));
            for (size_t i = 1; i + 1 < face.size(); ++i)
                appendTriangle(scene, {face[0], face[i], face[i + 1]}, positions, texCoords, normals, currentMaterial);
        }
    }
    return !scene.vertices.empty();
}

static unsigned int createSolidTexture(const glm::vec3 &color) {
    const unsigned char pixel[] = {static_cast<unsigned char>(glm::clamp(color.r, 0.0f, 1.0f) * 255.0f),
                                   static_cast<unsigned char>(glm::clamp(color.g, 0.0f, 1.0f) * 255.0f),
                                   static_cast<unsigned char>(glm::clamp(color.b, 0.0f, 1.0f) * 255.0f), 255};
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return texture;
}

static unsigned int loadTexture(const fs::path &filename, const glm::vec3 &fallbackColor) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load(filename.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!data) {
        std::cerr << "No se pudo cargar la textura " << filename << ": " << stbi_failure_reason() << '\n';
        return createSolidTexture(fallbackColor);
    }

    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    stbi_image_free(data);
    return texture;
}

static const char *vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
void main()
{
    vec4 worldPosition = model * vec4(aPos, 1.0);
    FragPos = worldPosition.xyz;
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord;
    gl_Position = projection * view * worldPosition;
}
)";

static const char *fragmentShaderSource = R"(
#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
out vec4 FragColor;
uniform vec3 cameraPos;
uniform vec3 materialKa;
uniform vec3 materialKd;
uniform vec3 materialKs;
uniform float materialNs;
uniform sampler2D diffuseMap;
uniform bool hasDiffuseMap;
void main()
{
    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(vec3(4.0, 6.0, 4.0) - FragPos);
    vec3 fillLightDir = normalize(vec3(-4.0, 2.0, -3.0) - FragPos);
    vec3 viewDir = normalize(cameraPos - FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float diffuse = max(dot(normal, lightDir), 0.0) * 0.75;
    diffuse += max(dot(normal, fillLightDir), 0.0) * 0.25;
    float specular = pow(max(dot(normal, halfwayDir), 0.0), max(materialNs, 1.0));
    vec3 baseColor = hasDiffuseMap ? texture(diffuseMap, TexCoord).rgb : materialKd;
    vec3 ambient = baseColor * 0.18 + materialKa * 0.35;
    vec3 color = ambient + baseColor * diffuse + materialKs * specular;
    FragColor = vec4(color, 1.0);
}
)";

static glm::vec3 cameraPosition(0.0f, 0.0f, 4.0f);
static float yaw = -90.0f;
static float pitch = 0.0f;
static bool dragging = false;
static double lastMouseX = 0.0;
static double lastMouseY = 0.0;

static void mouseButtonCallback(GLFWwindow *window, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT)
        return;
    dragging = action == GLFW_PRESS;
    if (dragging)
        glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
}

static void cursorCallback(GLFWwindow *, double x, double y) {
    if (!dragging)
        return;
    const float sensitivity = 0.15f;
    yaw += static_cast<float>(x - lastMouseX) * sensitivity;
    pitch -= static_cast<float>(y - lastMouseY) * sensitivity;
    pitch = glm::clamp(pitch, -89.0f, 89.0f);
    lastMouseX = x;
    lastMouseY = y;
}

int main() {
    const fs::path objPath = "fireplace_room/fireplace_room.obj";
    Scene scene;
    if (!loadObj(objPath, scene))
        return 1;

    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow *window = glfwCreateWindow(1000, 800, "Wavefront OBJ/MTL Scene", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorCallback);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwTerminate();
        return 1;
    }

    const fs::path materialDirectory = objPath.parent_path();
    for (auto &[name, material] : scene.materials) {
        material.texture = material.diffuseMap.empty()
                               ? createSolidTexture(material.diffuse)
                               : loadTexture(materialDirectory / material.diffuseMap, material.diffuse);
    }
    Material defaultMaterial;
    defaultMaterial.texture = createSolidTexture(defaultMaterial.diffuse);
    scene.materials.emplace("", defaultMaterial);

    std::vector<float> gpuVertices;
    gpuVertices.reserve(scene.vertices.size() * 8);
    for (const SceneVertex &vertex : scene.vertices) {
        gpuVertices.insert(gpuVertices.end(), {vertex.position.x, vertex.position.y, vertex.position.z, vertex.normal.x,
                                               vertex.normal.y, vertex.normal.z, vertex.texCoord.x, vertex.texCoord.y});
    }

    unsigned int vao;
    unsigned int vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, gpuVertices.size() * sizeof(float), gpuVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void *>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void *>(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    const unsigned int shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "diffuseMap"), 0);
    const glm::vec3 center = (scene.minBounds + scene.maxBounds) * 0.5f;
    const glm::vec3 sceneExtent = scene.maxBounds - scene.minBounds;
    const float extent = std::max({sceneExtent.x, sceneExtent.y, sceneExtent.z});
    const float sceneScale = extent > 0.0f ? 3.0f / extent : 1.0f;
    cameraPosition = glm::vec3(0.0f, 0.0f, 4.5f);
    glEnable(GL_DEPTH_TEST);
    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        const float currentFrame = static_cast<float>(glfwGetTime());
        const float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(front);
        const float cameraSpeed = 1.0f * deltaTime;
        const float rotationSpeed = 90.0f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            cameraPosition += front * cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            cameraPosition -= front * cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            yaw -= rotationSpeed;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            yaw += rotationSpeed;

        int width;
        int height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.04f, 0.05f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);
        const glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(sceneScale)) *
                    glm::translate(glm::mat4(1.0f), -center);
        const glm::mat4 view = glm::lookAt(cameraPosition, cameraPosition + front, glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::mat4 projection =
            glm::perspective(glm::radians(45.0f), static_cast<float>(width) / height, 0.01f, 100.0f);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(shaderProgram, "cameraPos"), 1, glm::value_ptr(cameraPosition));

        glBindVertexArray(vao);
        for (const Submesh &submesh : scene.submeshes) {
            const auto materialIt = scene.materials.find(submesh.materialName);
            const Material &material =
                materialIt != scene.materials.end() ? materialIt->second : scene.materials.at("");
            glUniform3fv(glGetUniformLocation(shaderProgram, "materialKa"), 1, glm::value_ptr(material.ambient));
            glUniform3fv(glGetUniformLocation(shaderProgram, "materialKd"), 1, glm::value_ptr(material.diffuse));
            glUniform3fv(glGetUniformLocation(shaderProgram, "materialKs"), 1, glm::value_ptr(material.specular));
            glUniform1f(glGetUniformLocation(shaderProgram, "materialNs"), material.shininess);
            glUniform1i(glGetUniformLocation(shaderProgram, "hasDiffuseMap"), !material.diffuseMap.empty());
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, material.texture);
            glUniform1i(glGetUniformLocation(shaderProgram, "diffuseMap"), 0);
            glDrawArrays(GL_TRIANGLES, static_cast<GLint>(submesh.firstVertex),
                         static_cast<GLsizei>(submesh.vertexCount));
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    for (const auto &[name, material] : scene.materials)
        glDeleteTextures(1, &material.texture);
    glDeleteProgram(shaderProgram);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
