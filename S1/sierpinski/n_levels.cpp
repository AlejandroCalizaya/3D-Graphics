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

void append_triangle(const Point3 &a, const Point3 &b, const Point3 &c, std::vector<float> &vertices, std::vector<unsigned int> &indices)
{
    unsigned int base_index = static_cast<unsigned int>(vertices.size() / 3);

    vertices.push_back(a.x);
    vertices.push_back(a.y);
    vertices.push_back(a.z);

    vertices.push_back(b.x);
    vertices.push_back(b.y);
    vertices.push_back(b.z);

    vertices.push_back(c.x);
    vertices.push_back(c.y);
    vertices.push_back(c.z);

    indices.push_back(base_index);
    indices.push_back(base_index + 1);
    indices.push_back(base_index + 2);
}

void subdivide_triangle(const Point3 &a, const Point3 &b, const Point3 &c, unsigned int depth, std::vector<float> &vertices, std::vector<unsigned int> &indices)
{
    if (depth == 0)
    {
        append_triangle(a, b, c, vertices, indices);
        return;
    }

    Point3 ab_mid = midpoint(a, b);
    Point3 ac_mid = midpoint(a, c);
    Point3 bc_mid = midpoint(b, c);

    subdivide_triangle(a, ab_mid, ac_mid, depth - 1, vertices, indices);
    subdivide_triangle(ab_mid, b, bc_mid, depth - 1, vertices, indices);
    subdivide_triangle(ac_mid, bc_mid, c, depth - 1, vertices, indices);
}

void build_sierpinsky_mesh(const float *base_vertices, int depth, std::vector<float> &vertices, std::vector<unsigned int> &indices)
{
    vertices.clear();
    indices.clear();

    Point3 a{base_vertices[0], base_vertices[1], base_vertices[2]};
    Point3 b{base_vertices[3], base_vertices[4], base_vertices[5]};
    Point3 c{base_vertices[6], base_vertices[7], base_vertices[8]};

    subdivide_triangle(a, b, c, depth, vertices, indices);
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

    const unsigned int depth = 5;
    std::vector<float> subdivided_vertices;
    std::vector<unsigned int> subdivided_indices;
    build_sierpinsky_mesh(vertices, depth, subdivided_vertices, subdivided_indices);

    unsigned int VAO, VBO, EBO;
    createVertexArrayWithIndices(subdivided_vertices, subdivided_indices, VAO, VBO, EBO);
    
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    while (!glfwWindowShouldClose(window))
    {
        processInput(window);

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        
        glDrawElements(GL_TRIANGLES, subdivided_indices.size(), GL_UNSIGNED_INT, 0);
        
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
