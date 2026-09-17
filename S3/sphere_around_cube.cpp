#include "../shared.h"

using namespace std;

const unsigned int SCR_WIDTH = 1000;
const unsigned int SCR_HEIGHT = 800;
FILE *ffmpegIn = nullptr;

glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 12.0f);

glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);

glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

float deltaTime = 0.0f;
float lastFrame = 0.0f;

const char *vertexShaderSource = R"(
#version 330 core

layout (location = 0) in vec3 aPos;

uniform mat4 view;
uniform mat4 projection;

uniform float time;

// ========================================================
// OBJECT TYPE
//
// 0 = cube
// 1 = sphere
// ========================================================

uniform int objectType;

uniform float cubeRotationSpeedX;
uniform float cubeRotationSpeedY;

uniform float sphereOrbitRadius;
uniform float sphereOrbitSpeed;
uniform float sphereRotationSpeed;
uniform float sphereHeight;

out vec3 FragPos;

mat4 rotationX(float angle)
{
    float c = cos(angle);
    float s = sin(angle);

    return mat4(
        1.0, 0.0, 0.0, 0.0,

        0.0, c, -s, 0.0,

        0.0, s,  c, 0.0,

        0.0, 0.0, 0.0, 1.0
    );
}

mat4 rotationY(float angle)
{
    float c = cos(angle);
    float s = sin(angle);

    return mat4(
         c, 0.0, s, 0.0,

        0.0, 1.0, 0.0, 0.0,

        -s, 0.0, c, 0.0,

        0.0, 0.0, 0.0, 1.0
    );
}

mat4 rotationZ(float angle)
{
    float c = cos(angle);
    float s = sin(angle);

    return mat4(
        c, -s, 0.0, 0.0,

        s,  c, 0.0, 0.0,

        0.0, 0.0, 1.0, 0.0,

        0.0, 0.0, 0.0, 1.0
    );
}

mat4 rotationAxis(
    float angle,
    vec3 axis
)
{
    float c = cos(angle);
    float s = sin(angle);
    float t = 1.0 - c;

    return mat4(
        t * axis.x * axis.x + c, t * axis.x * axis.y - s * axis.z, t * axis.x * axis.z + s * axis.y, 0.0,

        t * axis.x * axis.y + s * axis.z, t * axis.y * axis.y + c, t * axis.y * axis.z - s * axis.x, 0.0,

        t * axis.x * axis.z - s * axis.y, t * axis.y * axis.z + s * axis.x, t * axis.z * axis.z + c, 0.0,

        0.0, 0.0, 0.0, 1.0
    );
}


mat4 translation(
    float x,
    float y,
    float z
)
{
    return mat4(
        1.0, 0.0, 0.0, 0.0,

        0.0, 1.0, 0.0, 0.0,

        0.0, 0.0, 1.0, 0.0,

        x, y, z, 1.0
    );
}


void main()
{
    vec4 localPosition =
        vec4(aPos, 1.0);

    vec4 worldPosition;


    if (objectType == 0)
    {
        float angleX =
            time *
            cubeRotationSpeedX;


        float angleY =
            time *
            cubeRotationSpeedY;


        mat4 rotateX =
            rotationX(angleX);

        mat4 rotateY =
            rotationY(angleY);


        worldPosition =
            rotateY *
            rotateX *
            localPosition;
    }
    else
    {
        float selfRotation =
            time *
            sphereRotationSpeed;
        
        vec3 axis =
            normalize(
                vec3(
                    0.5,
                    0.3,
                    0.2
                )
            );


        mat4 selfRotationMatrix =
            rotationAxis(
                selfRotation,
                axis
            );


        vec4 rotatedSphere =
            selfRotationMatrix *
            localPosition;


        float orbitAngle =
            time *
            sphereOrbitSpeed;


        vec4 orbitPosition = 
            vec4(
                sphereOrbitRadius * cos(orbitAngle),
                sphereOrbitRadius * sin(orbitAngle),
                0.0,
                1.0
            );
        
        float orbitTilt = radians(235.0);

        mat4 orbitTiltMatrix =
            rotationX(orbitTilt);

        orbitPosition =
            orbitTiltMatrix *
            orbitPosition;

        mat4 orbitTranslation =
            translation(
                orbitPosition.x,
                orbitPosition.y + sphereHeight,
                orbitPosition.z
            );


        worldPosition =
            orbitTranslation *
            rotatedSphere;
    }


    FragPos = worldPosition.xyz;


    gl_Position =
        projection *
        view *
        worldPosition;
}
)";

const char *fragmentShaderSource = R"(
#version 330 core

in vec3 FragPos;

out vec4 FragColor;

uniform vec3 cameraPos;

uniform int objectType;

void main()
{
    vec3 dx = dFdx(FragPos);
    vec3 dy = dFdy(FragPos);

    vec3 normal =
        normalize(
            cross(dx, dy)
        );


    vec3 lightDirection =
        normalize(
            cameraPos -
            FragPos
        );


    float diffuse =
        max(
            dot(
                normal,
                lightDirection
            ),
            0.0
        );


    float ambient =
        0.20;


    float lighting =
        ambient +
        diffuse * 0.80;


    vec3 baseColor;

    if (objectType == 0)
    {
        baseColor =
            vec3(
                0.85,
                0.45,
                0.15
            );
    }
    else
    {
        baseColor =
            vec3(
                0.15,
                0.45,
                0.85
            );
    }


    vec3 finalColor =
        baseColor *
        lighting;


    FragColor =
        vec4(
            finalColor,
            1.0
        );
}
)";

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float cameraSpeed = 4.0f * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;

    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
}

void startRecord(const char *nombreArchivo, int fps, int width, int height) {
#ifdef _WIN32
    const char *cmdTemplate =
        "_popen(\"ffmpeg -y -f rawvideo -pix_fmt rgb24 -s %dx%d -r %d -i - -c:v libx264 -pix_fmt yuv420p %s\", \"wb\")";
    char cmd[512];
    sprintf(cmd, "ffmpeg -y -f rawvideo -pix_fmt rgb24 -s %dx%d -r %d -i - -c:v libx264 -pix_fmt yuv420p \"%s\"", width,
            height, fps, nombreArchivo);
    ffmpegIn = _popen(cmd, "wb");
#else
    char cmd[512];
    sprintf(cmd, "ffmpeg -y -f rawvideo -pix_fmt rgb24 -s %dx%d -r %d -i - -c:v libx264 -pix_fmt yuv420p \"%s\"", width,
            height, fps, nombreArchivo);
    ffmpegIn = popen(cmd, "w");
#endif

    if (!ffmpegIn) {
        std::cerr << "Error: No se pudo iniciar FFmpeg. ¿Está instalado en el sistema?" << std::endl;
    }
}

void recordFrame(int width, int height) {
    if (!ffmpegIn)
        return;

    std::vector<unsigned char> pixels(width * height * 3);
    std::vector<unsigned char> flippedPixels(width * height * 3);

    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    for (int y = 0; y < height; ++y) {
        std::memcpy(&flippedPixels[y * width * 3], &pixels[(height - 1 - y) * width * 3], width * 3);
    }

    fwrite(flippedPixels.data(), 1, width * height * 3, ffmpegIn);
}

void endRecord() {
    if (ffmpegIn) {
#ifdef _WIN32
        _pclose(ffmpegIn);
#else
        pclose(ffmpegIn);
#endif
        ffmpegIn = nullptr;
        std::cout << "Video guardado exitosamente." << std::endl;
    }
}

int main() {
    if (!glfwInit()) {
        cerr << "Error inicializando GLFW" << endl;

        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "3D Scene", nullptr, nullptr);

    if (!window) {
        cerr << "Error creando ventana" << endl;

        glfwTerminate();

        return -1;
    }

    glfwMakeContextCurrent(window);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cerr << "Error inicializando GLAD" << endl;

        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    unsigned int shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    vector<Vertex> cubeVertices;

    vector<CHE> cubeHalfEdges;

    vector<unsigned int> cubeFaceIndices;

    vector<unsigned int> cubeEdgeIndices;

    buildCube(cubeVertices, cubeHalfEdges, cubeFaceIndices, cubeEdgeIndices);

    vector<Vertex> sphereVertices;

    vector<CHE> sphereHalfEdges;

    vector<unsigned int> sphereFaceIndices;

    vector<unsigned int> sphereEdgeIndices;

    buildSphere(20, 20, 0.5f, sphereVertices, sphereHalfEdges, sphereFaceIndices, sphereEdgeIndices);

    unsigned int cubeVAO;
    unsigned int cubeVBO;
    unsigned int cubeEBO;

    createMeshBuffers(cubeVertices, cubeFaceIndices, cubeVAO, cubeVBO, cubeEBO);

    unsigned int sphereVAO;
    unsigned int sphereVBO;
    unsigned int sphereEBO;

    createMeshBuffers(sphereVertices, sphereFaceIndices, sphereVAO, sphereVBO, sphereEBO);

    int viewLoc = glGetUniformLocation(shaderProgram, "view");

    int projectionLoc = glGetUniformLocation(shaderProgram, "projection");

    int cameraPosLoc = glGetUniformLocation(shaderProgram, "cameraPos");

    int timeLoc = glGetUniformLocation(shaderProgram, "time");

    int objectTypeLoc = glGetUniformLocation(shaderProgram, "objectType");

    int cubeSpeedXLoc = glGetUniformLocation(shaderProgram, "cubeRotationSpeedX");

    int cubeSpeedYLoc = glGetUniformLocation(shaderProgram, "cubeRotationSpeedY");

    int orbitRadiusLoc = glGetUniformLocation(shaderProgram, "sphereOrbitRadius");

    int orbitSpeedLoc = glGetUniformLocation(shaderProgram, "sphereOrbitSpeed");

    int sphereRotationSpeedLoc = glGetUniformLocation(shaderProgram, "sphereRotationSpeed");

    int sphereHeightLoc = glGetUniformLocation(shaderProgram, "sphereHeight");

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    int fpsVideo = 24;
    float tiempoSimulado = 0.0f;
    float frameTimeFijo = 1.0f / static_cast<float>(fpsVideo);

    startRecord("mi_animacion_3d.mp4", fpsVideo, width, height);

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = tiempoSimulado;
        deltaTime = frameTimeFijo;
        tiempoSimulado += frameTimeFijo;

        processInput(window);

        glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        glfwGetFramebufferSize(window, &width, &height);
        float aspect = static_cast<float>(width) / static_cast<float>(height);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), cameraUp);

        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(cameraPosLoc, 1, glm::value_ptr(cameraPos));

        glUniform1f(timeLoc, currentFrame);

        glUniform1f(cubeSpeedXLoc, glm::radians(20.0f));
        glUniform1f(cubeSpeedYLoc, glm::radians(30.0f));

        glUniform1f(orbitRadiusLoc, 4.0f);
        glUniform1f(orbitSpeedLoc, 0.8f);
        glUniform1f(sphereRotationSpeedLoc, 1.5f);
        glUniform1f(sphereHeightLoc, 0.0f);

        glUniform1i(objectTypeLoc, 0);
        glBindVertexArray(cubeVAO);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(cubeFaceIndices.size()), GL_UNSIGNED_INT, nullptr);

        glUniform1i(objectTypeLoc, 1);
        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(sphereFaceIndices.size()), GL_UNSIGNED_INT, nullptr);

        recordFrame(width, height);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    endRecord();

    glDeleteVertexArrays(1, &cubeVAO);

    glDeleteBuffers(1, &cubeVBO);

    glDeleteBuffers(1, &cubeEBO);

    glDeleteVertexArrays(1, &sphereVAO);

    glDeleteBuffers(1, &sphereVBO);

    glDeleteBuffers(1, &sphereEBO);

    glDeleteProgram(shaderProgram);

    glfwTerminate();

    return 0;
}