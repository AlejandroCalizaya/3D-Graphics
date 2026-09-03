#include "../shared.h"

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

const char *vertexShaderSource = R"(
#version 330 core

layout (location = 0) in vec3 aPos;

void main()
{
    gl_Position = vec4(aPos, 1.0);
}
)";


const char *geometryShaderSource = R"(
#version 330 core

// Recibe un triángulo
layout (triangles) in;

// Generará hasta 9 vértices
layout (triangle_strip, max_vertices = 9) out;

// ------------------------------------------------------------
// Calcula el punto medio entre dos puntos
// ------------------------------------------------------------

vec3 midpoint(vec3 a, vec3 b)
{
    return (a + b) * 0.5;
}

// ------------------------------------------------------------
// Emite un triángulo
// ------------------------------------------------------------

void emit_triangle(vec3 a, vec3 b, vec3 c)
{
    gl_Position = vec4(a, 1.0);
    EmitVertex();

    gl_Position = vec4(b, 1.0);
    EmitVertex();

    gl_Position = vec4(c, 1.0);
    EmitVertex();

    EndPrimitive();
}

// ------------------------------------------------------------
// Geometry Shader
// ------------------------------------------------------------

void main()
{
    // Obtener los tres vértices del triángulo de entrada
    vec3 a = gl_in[0].gl_Position.xyz;
    vec3 b = gl_in[1].gl_Position.xyz;
    vec3 c = gl_in[2].gl_Position.xyz;

    // Calcular puntos medios
    vec3 ab = midpoint(a, b);
    vec3 ac = midpoint(a, c);
    vec3 bc = midpoint(b, c);

    // --------------------------------------------------------
    // Triángulo superior
    // --------------------------------------------------------

    emit_triangle(
        a,
        ab,
        ac
    );

    // --------------------------------------------------------
    // Triángulo inferior izquierdo
    // --------------------------------------------------------

    emit_triangle(
        ab,
        b,
        bc
    );

    // --------------------------------------------------------
    // Triángulo inferior derecho
    // --------------------------------------------------------

    emit_triangle(
        ac,
        bc,
        c
    );
}
)";


const char *fragmentShaderSource = R"(
#version 330 core

out vec4 FragColor;

void main()
{
    FragColor = vec4(
        1.0f,
        0.5f,
        0.2f,
        1.0f
    );
}
)";


int main()
{
    if (!glfwInit())
    {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(
        GLFW_OPENGL_PROFILE,
        GLFW_OPENGL_CORE_PROFILE
    );

    GLFWwindow *window = glfwCreateWindow(
        SCR_WIDTH,
        SCR_HEIGHT,
        "Sierpinski Triangle - Geometry Shader",
        NULL,
        NULL
    );

    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;

        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    glfwSetFramebufferSizeCallback(
        window,
        framebuffer_size_callback
    );

    if (!gladLoadGLLoader(
            (GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD"
                  << std::endl;

        glfwTerminate();
        return -1;
    }

    std::cout << "OpenGL version: "
              << glGetString(GL_VERSION)
              << std::endl;

    unsigned int vertexShader =
        glCreateShader(GL_VERTEX_SHADER);

    glShaderSource(
        vertexShader,
        1,
        &vertexShaderSource,
        NULL
    );

    glCompileShader(vertexShader);

    int success;
    char infoLog[512];

    glGetShaderiv(
        vertexShader,
        GL_COMPILE_STATUS,
        &success
    );

    if (!success)
    {
        glGetShaderInfoLog(
            vertexShader,
            512,
            NULL,
            infoLog
        );

        std::cout
            << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
            << infoLog
            << std::endl;
    }

    unsigned int geometryShader =
        glCreateShader(GL_GEOMETRY_SHADER);

    glShaderSource(
        geometryShader,
        1,
        &geometryShaderSource,
        NULL
    );

    glCompileShader(geometryShader);

    glGetShaderiv(
        geometryShader,
        GL_COMPILE_STATUS,
        &success
    );

    if (!success)
    {
        glGetShaderInfoLog(
            geometryShader,
            512,
            NULL,
            infoLog
        );

        std::cout
            << "ERROR::SHADER::GEOMETRY::COMPILATION_FAILED\n"
            << infoLog
            << std::endl;
    }

    unsigned int fragmentShader =
        glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(
        fragmentShader,
        1,
        &fragmentShaderSource,
        NULL
    );

    glCompileShader(fragmentShader);

    glGetShaderiv(
        fragmentShader,
        GL_COMPILE_STATUS,
        &success
    );

    if (!success)
    {
        glGetShaderInfoLog(
            fragmentShader,
            512,
            NULL,
            infoLog
        );

        std::cout
            << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n"
            << infoLog
            << std::endl;
    }

    unsigned int shaderProgram =
        glCreateProgram();

    glAttachShader(
        shaderProgram,
        vertexShader
    );

    glAttachShader(
        shaderProgram,
        geometryShader
    );

    glAttachShader(
        shaderProgram,
        fragmentShader
    );

    glLinkProgram(shaderProgram);

    glGetProgramiv(
        shaderProgram,
        GL_LINK_STATUS,
        &success
    );

    if (!success)
    {
        glGetProgramInfoLog(
            shaderProgram,
            512,
            NULL,
            infoLog
        );

        std::cout
            << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
            << infoLog
            << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(geometryShader);
    glDeleteShader(fragmentShader);

    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };

    unsigned int VAO;
    unsigned int VBO;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(
        GL_ARRAY_BUFFER,
        VBO
    );

    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(vertices),
        vertices,
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        3 * sizeof(float),
        (void *)0
    );

    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);

    while (!glfwWindowShouldClose(window))
    {
        processInput(window);
        
        glClearColor(
            0.2f,
            0.3f,
            0.3f,
            1.0f
        );

        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);

        glDrawArrays(
            GL_TRIANGLES,
            0,
            3
        );

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(
        1,
        &VAO
    );

    glDeleteBuffers(
        1,
        &VBO
    );

    glDeleteProgram(
        shaderProgram
    );

    glfwTerminate();

    return 0;
}