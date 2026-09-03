#include "../../shared.h"

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

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

void build_sierpinsky_mesh(const float *base_vertices, const unsigned int &max_points, std::vector<float> &vertices)
{
    vertices.clear();

    Point3 a{base_vertices[0], base_vertices[1], base_vertices[2]};
    Point3 b{base_vertices[3], base_vertices[4], base_vertices[5]};
    Point3 c{base_vertices[6], base_vertices[7], base_vertices[8]};

    vertices.push_back(a.x);
    vertices.push_back(a.y);
    vertices.push_back(a.z);

    vertices.push_back(b.x);
    vertices.push_back(b.y);
    vertices.push_back(b.z);

    vertices.push_back(c.x);
    vertices.push_back(c.y);
    vertices.push_back(c.z);

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

    unsigned int shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
        0.0f, 0.5f, 0.0f
    };

    std::vector<float> sierpinsky_vertices;
    const unsigned int max_points = 1e3;
    build_sierpinsky_mesh(vertices, max_points, sierpinsky_vertices);

    unsigned int VAO, VBO;
    createVertexArray(sierpinsky_vertices, VAO, VBO);

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
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}
