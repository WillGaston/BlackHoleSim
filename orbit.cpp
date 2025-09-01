#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <algorithm>

using namespace std;

// Simple 2D vector
struct Vec2 {
    float x, y;
};

// Particle
struct Particle {
    Vec2 pos;
    Vec2 vel;
    vector<Vec2> trail;
    float temp;
    size_t maxTrailLength = 50;
};

const int width = 800, height = 600;
const float G = 200.0f, M = 2000.0f;
const int numParticles = 100;
const float centerX = (float)width / 2;
const float centerY = (float)height / 2;
const float blackHoleRadius = 15.0f;
const float accretionDiskRadius = 80.0f;

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

float calcTemp(const Particle& p) {
    float speed = sqrt(p.vel.x * p.vel.x + p.vel.y * p.vel.y);
    float dist = sqrt((p.pos.x - centerX) * (p.pos.x - centerX) + (p.pos.y - centerY) * (p.pos.y - centerY));

    float temp = speed * 0.01f + (200.0f / max(dist, 10.0f));

    return min(temp, 3.0f);
}

// Update particle
void updateParticle(Particle &p, float dt, float G, float M) {

    p.trail.push_back(p.pos);
    if (p.trail.size() > p.maxTrailLength) {
        p.trail.erase(p.trail.begin());
    }

    Vec2 a = gravity(p.pos, G, M); // create the acceleration vector
    p.vel.x += a.x * dt; // add one unit of acceleration to velocities
    p.vel.y += a.y * dt;
    p.pos.x += p.vel.x * dt; // update positions using velocity
    p.pos.y += p.vel.y * dt;

    p.temp = calcTemp(p);
}


// to do: get colour based on temp and distance to blackhole

Vec2 getParticleColour(float temp, float dist) {
    Vec2 colour;
    
    // Accretion disk effect: particles close to black hole are very hot and bright
    if (dist < accretionDiskRadius) {
        // Very hot accretion disk particles: white to blue
        float diskFactor = 1.0f - (dist / accretionDiskRadius);
        colour.x = 0.8f + diskFactor * 0.2f;  // Red component
        colour.y = 0.9f + diskFactor * 0.1f;  // Green component
        return colour; // Blue will be 1.0 in shader
    }
    
    // Normal particles: color based on temperature
    if (temp > 2.0f) {
        // Very hot: white/blue
        colour.x = 1.0f;
        colour.y = 1.0f;
    } else if (temp > 1.0f) {
        // Hot: yellow/white
        colour.x = 1.0f;
        colour.y = 1.0f;
    } else {
        // Cool: red/orange
        colour.x = 1.0f;
        colour.y = 0.3f + temp * 0.4f;
    }
    
    return colour;
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
    gl_PointSize = 10.0;
}
)";

const char* fragmentShaderSrc = R"(
#version 330 core
out vec4 FragColor;                          // Output color
uniform vec3 uColor;                         // Input color from CPU

void main() {
    // Create circular particles instead of square points
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = length(coord);
    
    // Smooth falloff for glowing effect
    float alpha = 1.0 - smoothstep(0.0, 0.5, dist);
    
    FragColor = vec4(uColor, alpha);         // Use calculated alpha for glow
}
)";

// Trail vertex shader for rendering particle trails
const char* trailVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in float aAlpha;       // Trail fade factor
uniform float uScreenWidth;
uniform float uScreenHeight;
out float vAlpha;

void main() {
    float x = (aPos.x / uScreenWidth) * 2.0 - 1.0;
    float y = (aPos.y / uScreenHeight) * 2.0 - 1.0;
    gl_Position = vec4(x, y, 0.0, 1.0);
    vAlpha = aAlpha;
}
)";

// Trail fragment shader with fading effect
const char* trailFragmentShaderSrc = R"(
#version 330 core
in float vAlpha;
out vec4 FragColor;
uniform vec3 uColor;

void main() {
    FragColor = vec4(uColor * 0.8, vAlpha * 0.3);  // Dimmer, fading trails
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

    glEnable(GL_PROGRAM_POINT_SIZE);
    
    // Enable blending for transparency effects
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Initialize particles
    vector<Particle> particles;
    for (int i = 0; i < numParticles; ++i) {
        float angle = (rand() % 360) * 3.14159f / 180.0f; // random angle on circle around blackhole
        float radius = 50 + rand() % 250; // random dist from blackhole

        // stable orbit velocity: = sqrt(GM/r)
        float orbitalSpeed = sqrt((G * M) / radius) * 0.9f;
        float xVel = -1 * sin(angle) * orbitalSpeed;
        float yVel = cos(angle) * orbitalSpeed;

        Particle p;
        p.pos = { width/2 + radius * cos(angle), height/2 + radius * sin(angle) };
        p.vel = { xVel, yVel };
        p.temp = 1.0f;

        particles.push_back(p);
    }

    // Set up OpenGL buffers for rendering particles
    GLuint particleVBO, particleVAO;
    glGenBuffers(1, &particleVBO);
    glGenVertexArrays(1, &particleVAO);
    
    // Configure particle vertex array object
    glBindVertexArray(particleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    glBufferData(GL_ARRAY_BUFFER, numParticles * 2 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Set up OpenGL buffers for rendering trails
    GLuint trailVBO, trailVAO;
    glGenBuffers(1, &trailVBO);
    glGenVertexArrays(1, &trailVAO);
    
    // Configure trail vertex array object (position + alpha for fading)
    glBindVertexArray(trailVAO);
    glBindBuffer(GL_ARRAY_BUFFER, trailVBO);
    // Each trail point has 3 floats: x, y, alpha
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Create shader programs
    GLuint particleProgram = createProgram(vertexShaderSrc, fragmentShaderSrc);
    GLuint trailProgram = createProgram(trailVertexShaderSrc, trailFragmentShaderSrc);
    
    // Get uniform locations for particle shader
    GLint particleColorLoc = glGetUniformLocation(particleProgram, "uColor");
    GLint particleWidthLoc = glGetUniformLocation(particleProgram, "uScreenWidth");
    GLint particleHeightLoc = glGetUniformLocation(particleProgram, "uScreenHeight");
    
    // Get uniform locations for trail shader
    GLint trailColorLoc = glGetUniformLocation(trailProgram, "uColor");
    GLint trailWidthLoc = glGetUniformLocation(trailProgram, "uScreenWidth");
    GLint trailHeightLoc = glGetUniformLocation(trailProgram, "uScreenHeight");
    
    // Set screen dimensions in both shaders
    glUseProgram(particleProgram);
    glUniform1f(particleWidthLoc, (float)width);
    glUniform1f(particleHeightLoc, (float)height);
    
    glUseProgram(trailProgram);
    glUniform1f(trailWidthLoc, (float)width);
    glUniform1f(trailHeightLoc, (float)height);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Clear screen to black
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);  // Dark blue background for space effect
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Update all particle positions using physics simulation
        vector<float> particleData(numParticles * 2);  // Array for particle positions
        vector<float> trailData;                        // Dynamic array for all trail points
        
        for (int i = 0; i < numParticles; ++i) {
            // Update particle physics (smaller timestep for stability)
            updateParticle(particles[i], 0.008f, G, M);
            
            // Store particle position data for rendering
            particleData[2*i] = particles[i].pos.x;     // X coordinate
            particleData[2*i+1] = particles[i].pos.y;   // Y coordinate
            
            // Add trail points to trail data array
            for (size_t j = 0; j < particles[i].trail.size(); ++j) {
                trailData.push_back(particles[i].trail[j].x);  // X position
                trailData.push_back(particles[i].trail[j].y);  // Y position
                
                // Calculate alpha for fading effect (newer points are more opaque)
                float alpha = (float)j / particles[i].trail.size();
                trailData.push_back(alpha);
            }
        }
        
        // Render particle trails first (so they appear behind particles)
        if (!trailData.empty()) {
            glUseProgram(trailProgram);
            glBindVertexArray(trailVAO);
            glBindBuffer(GL_ARRAY_BUFFER, trailVBO);
            
            // Upload trail data to GPU
            glBufferData(GL_ARRAY_BUFFER, trailData.size() * sizeof(float), 
                        trailData.data(), GL_DYNAMIC_DRAW);
            
            // Set trail colour (dim white)
            glUniform3f(trailColorLoc, 0.8f, 0.8f, 1.0f);
            
            // Draw all trail points as lines
            glDrawArrays(GL_POINTS, 0, trailData.size() / 3);
        }
        
        // Render particles with temperature-based coloring
        glUseProgram(particleProgram);
        glBindVertexArray(particleVAO);
        glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
        
        // Upload particle positions to GPU
        glBufferSubData(GL_ARRAY_BUFFER, 0, particleData.size() * sizeof(float), particleData.data());
        
        // Draw each particle with individual color based on temperature and distance
        for (int i = 0; i < numParticles; ++i) {
            float distanceFromCenter = sqrt((particles[i].pos.x - centerX) * (particles[i].pos.x - centerX) + 
                                           (particles[i].pos.y - centerY) * (particles[i].pos.y - centerY));
            
            Vec2 colour = getParticleColour(particles[i].temp, distanceFromCenter);
            
            // Special bright coloring for accretion disk particles
            if (distanceFromCenter < accretionDiskRadius) {
                glUniform3f(particleColorLoc, colour.x, colour.y, 1.0f);  // Bright blue-white
            } else {
                glUniform3f(particleColorLoc, colour.x, colour.y, 0.2f);  // Apply calculated color
            }
            
            // Draw single particle
            glDrawArrays(GL_POINTS, i, 1);
        }

        float blackHole[2] = { centerX, centerY };
        glBufferSubData(GL_ARRAY_BUFFER, 0, 2 * sizeof(float), blackHole);
        glUniform3f(particleColorLoc, 0.8f, 0.2f, 0.0f);  // Orange-red black hole
        glDrawArrays(GL_POINTS, 0, 1);
        
        // Present rendered frame and handle window events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteBuffers(1, &particleVBO);
    glDeleteVertexArrays(1, &particleVAO);
    glDeleteBuffers(1, &trailVBO);
    glDeleteVertexArrays(1, &trailVAO);
    glDeleteProgram(particleProgram);
    glDeleteProgram(trailProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
