#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void processInput(GLFWwindow *window);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

const char *vertexShaderSource = "#version 330 core\n"
                                 "layout (location = 0) in vec3 aPos;\n"
                                 "void main()\n"
                                 "{\n"
                                 "   gl_Position = vec4(aPos, 1.0);\n"
                                 "}\0";

const char *fragmentShaderSource = "#version 330 core\n"
                                   "out vec4 FragColor;\n"
                                   "void main()\n"
                                   "{\n"
                                   "   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
                                   "}\n\0";

struct Point3
{
    float x;
    float y;
    float z;
};

Point3 midpoint(const Point3 &a, const Point3 &b)
{
    return {
        (a.x + b.x) * 0.5f,
        (a.y + b.y) * 0.5f,
        (a.z + b.z) * 0.5f,
    };
}

void build_sierpinsky_mesh(const float *base_vertices, const unsigned int &max_points, std::vector<float> &vertices)
{
    vertices.clear();

    Point3 a{base_vertices[0], base_vertices[1], base_vertices[2]};
    Point3 b{base_vertices[3], base_vertices[4], base_vertices[5]};
    Point3 c{base_vertices[6], base_vertices[7], base_vertices[8]};

    // add the initial triangle vertices
    vertices.push_back(a.x);
    vertices.push_back(a.y);
    vertices.push_back(a.z);

    vertices.push_back(b.x);
    vertices.push_back(b.y);
    vertices.push_back(b.z);

    vertices.push_back(c.x);
    vertices.push_back(c.y);
    vertices.push_back(c.z);

    // generate random 2D points inside the triangle
    float r1 = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    float r2 = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

    if (r1 + r2 > 1.0f)
    {
        r1 = 1.0f - r1;
        r2 = 1.0f - r2;
    }

    Point3 p;
    p.x = a.x * r1 + b.x * r2 + c.x * (1.0f - r1 - r2);
    p.y = a.y * r1 + b.y * r2 + c.y * (1.0f - r1 - r2);
    p.z = 0.0f;

    for (unsigned int i = 0; i < max_points; ++i)
    {
        // choose a random vertex of the base triangle and compute the midpoint with the random point
        int random_vertex = rand() % 3;
        Point3 chosen_vertex;
        if (random_vertex == 0)
        {
            chosen_vertex = a;
        }
        else if (random_vertex == 1)
        {
            chosen_vertex = b;
        }
        else
        {
            chosen_vertex = c;
        }

        Point3 midpoint_point = midpoint(chosen_vertex, p);

        vertices.push_back(midpoint_point.x);
        vertices.push_back(midpoint_point.y);
        vertices.push_back(midpoint_point.z);

        // replace the random point with the new midpoint for the next iteration
        p = midpoint_point;
    }
}


int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
                  << infoLog << std::endl;
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n"
                  << infoLog << std::endl;
    }

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
                  << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
        0.0f, 0.5f, 0.0f
    };

    std::vector<float> sierpinsky_vertices;
    const unsigned int max_points = 1e3;
    build_sierpinsky_mesh(vertices, max_points, sierpinsky_vertices);

    unsigned int VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(sierpinsky_vertices.size() * sizeof(float)),
                 sierpinsky_vertices.data(),
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);

    glPointSize(5.0f);

    while (!glfwWindowShouldClose(window))
    {
        processInput(window);

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);

        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(sierpinsky_vertices.size() / 3));

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}
