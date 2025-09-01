#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>

using namespace std;

// Simple 2D vector
struct Vec2 {
    float x, y;
};

// Particle
struct Particle {
    Vec2 pos;
    Vec2 vel;
};

const int width = 800, height = 600;
const float G = 200.0f, M = 2000.0f;
const int numParticles = 200;
const float centerX = (float)width / 2;
const float centerY = (float)height / 2;

// Gravitational acceleration toward origin (black hole at center)
Vec2 gravity(Vec2 pos, float G, float M) {
    float dx = pos.x - centerX; // computer x displacement
    float dy = pos.y - centerY; // computer y displacement
    float r2 = dx*dx + dy*dy;
    float r = sqrt(r2);
    if (r < 5.0f) r = 5.0f; // prevent singularity
    float F = G * M / r2; // F = magnitude of acceleration
    return { -F * dx / r, -F * dy / r }; // we create an acceleration vecotr towards the blackhole
}

// Update particle
void updateParticle(Particle &p, float dt, float G, float M) {
    Vec2 a = gravity(p.pos, G, M); // create the acceleration vector
    p.vel.x += a.x * dt; // add one unit of acceleration to velocities
    p.vel.y += a.y * dt;
    p.pos.x += p.vel.x * dt; // update positions using velocity
    p.pos.y += p.vel.y * dt;
}

// Shader sources
const char* vertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform float uScreenWidth;
uniform float uScreenHeight;
void main() {
    float x = (aPos.x / uScreenWidth) * 2.0 - 1.0;
    float y = (aPos.y / uScreenHeight) * 2.0 - 1.0;
    gl_Position = vec4(x, y, 0.0, 1.0);
    gl_PointSize = 2.0;
}
)";

const char* fragmentShaderSrc = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;
void main() {
    FragColor = vec4(uColor, 1.0);
}
)";

// Compile shader helper
GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(shader, 512, nullptr, info);
        cerr << "Shader compilation error: " << info << endl;
    }
    return shader;
}

// Create shader program helper
GLuint createProgram(const char* vertSrc, const char* fragSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

int main() {
    srand(time(nullptr));

    // Initialize GLFW
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(width, height, "Black Hole OpenGL", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    // Initialize particles
    vector<Particle> particles;
    for (int i = 0; i < numParticles; ++i) {
        float angle = (rand() % 360) * 3.14159f / 180.0f; // random angle on circle around blackhole
        float radius = 50 + rand() % 250; // random dist from blackhole

        // stable orbit velocity: = sqrt(GM/r)
        float orbitalSpeed = sqrt((G * M) / radius) * 0.9f;
        float xVel = -1 * sin(angle) * orbitalSpeed;
        float yVel = cos(angle) * orbitalSpeed;

        particles.push_back({ 
            { width/2 + radius * cos(angle), height/2 + radius * sin(angle) }, 
            {xVel, yVel} 
        });
    }

    // Create VBO and VAO
    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glGenVertexArrays(1, &VAO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, numParticles * 2 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    GLuint shaderProgram = createProgram(vertexShaderSrc, fragmentShaderSrc);
    glUseProgram(shaderProgram);

    GLint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
    GLint widthLoc = glGetUniformLocation(shaderProgram, "uScreenWidth");
    GLint heightLoc = glGetUniformLocation(shaderProgram, "uScreenHeight");

    glUniform1f(widthLoc, (float)width);
    glUniform1f(heightLoc, (float)height);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        // Update particle positions
        vector<float> data(numParticles * 2);
        for (int i = 0; i < numParticles; ++i) {
            updateParticle(particles[i], 0.01f, G, M);
            data[2*i] = particles[i].pos.x;
            data[2*i+1] = particles[i].pos.y;
        }

        // Upload data to GPU
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, data.size() * sizeof(float), data.data());

        // Draw particles
        glUniform3f(colorLoc, 1.0f, 1.0f, 1.0f);
        glBindVertexArray(VAO);
        glDrawArrays(GL_POINTS, 0, numParticles);

        // Draw black hole at center
        float bh[2] = { width/2.0f, height/2.0f };
        glBufferSubData(GL_ARRAY_BUFFER, 0, 2 * sizeof(float), bh);
        glUniform3f(colorLoc, 1.0f, 0.0f, 0.0f);
        glDrawArrays(GL_POINTS, 0, 1);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
