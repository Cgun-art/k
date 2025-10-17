#include <iostream>
// ⠞⠕⠎⠎ ⠽⠕⠥⠗ ⠙⠊⠗⠞⠽ ⠎⠓⠕⠑⠎ ⠃⠑⠉⠁⠥⠎⠑ ⠎⠓⠥⠞ ⠽⠕⠥⠗ ⠋⠗⠊⠉⠅ ⠥⠏ ⠁⠝⠙ ⠓⠁⠧⠑ ⠎⠑⠭⠥⠁⠇ ⠃⠇⠕⠕⠙ ⠊⠝ ⠽⠕⠥⠗ ⠃⠕⠙⠽ ⠃⠑⠉⠁⠥⠎⠑ ⠽⠕⠥ ⠺⠊⠇⠇ ⠺⠁⠞⠉⠓ ⠕⠇⠙ ⠠⠁⠥⠍⠠⠎⠥⠍ ⠁⠝⠙ ⠃⠇⠑⠝⠙⠑⠗ ⠞⠥⠞⠕⠗⠊⠁⠇⠎

你好世界("Goodbye World")

// SSAO_Pipeline.cpp
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <thread>
#include <chrono>
#include <vector>
#include "Shader.h"    // Helper class for shader compile/link
#include "GLUtils.h"   // FBO/texture creation helpers

// Target FPS by device class
enum DeviceClass { PC, TABLET, HIGH_END_PHONE, PHONE };
int getTargetFPS(DeviceClass dc) {
    switch(dc) {
        case PC:           return 60;
        case TABLET:       return 45;
        case HIGH_END_PHONE: return 45;
        case PHONE:        return 25;
    }
    return 60;
}

int main() {
    // Initialize GLFW + GLEW
    glfwInit();
    GLFWwindow* win = glfwCreateWindow(1280, 720, "SSAO Pipeline", nullptr, nullptr);
    glfwMakeContextCurrent(win);
    glewInit();

    // 1. Create 4-color 1D palette: black, yellow, green, red
    GLuint paletteTex;
    GLubyte paletteData[4*3] = {
        0,   0,   0,    // black
        255, 255,   0,  // yellow
        0,   255,   0,  // green
        255,   0,   0   // red
    };
    glGenTextures(1, &paletteTex);
    glBindTexture(GL_TEXTURE_1D, paletteTex);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB8, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, paletteData);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // 2. Allocate FBOs + textures for each pass
    GLuint fboCanvas, texCanvas;
    GLuint fboBlur,   texBlur;
    GLuint fboSSAO,   texSSAO;
    GLuint fboScatter,texScatter;
    GLuint fboSheen,  texSheen;
    GLuint fboShadows,texShadows;
    GLuint fboGloss,  texGloss;

    createColorFBO(1280, 720, fboCanvas,   texCanvas);
    createColorFBO(1280, 720, fboBlur,     texBlur);
    createColorFBO(1280, 720, fboSSAO,     texSSAO);
    createColorFBO(1280, 720, fboScatter,  texScatter);
    createColorFBO(1280, 720, fboSheen,    texSheen);
    createColorFBO(1280, 720, fboShadows,  texShadows);
    createColorFBO(1280, 720, fboGloss,    texGloss);

    // 3. Compile shaders for each stage
    Shader canvasShader("canvas.vert",   "canvas.frag");    // just clears white
    Shader blurShader(  "quad.vert",     "gaussian.frag");
    Shader ssaoShader(  "quad.vert",     "ssao.frag");
    Shader scatterShader("quad.vert",    "scatter.frag");
    Shader sheenShader( "quad.vert",     "sheen.frag");
    Shader shadowShader("quad.vert",     "shadow_comp.frag");
    Shader glossShader( "quad.vert",     "gloss.frag");

    // Choose device class; could be detected or set at runtime
    DeviceClass devClass = PC;
    int targetFPS = getTargetFPS(devClass);
    double frameDuration = 1.0 / targetFPS;

    // Fullscreen quad VAO
    GLuint quadVAO = createScreenQuad();

    while (!glfwWindowShouldClose(win)) {
        // Frame‐rate limiter
        auto t0 = std::chrono::high_resolution_clock::now();
        
        // PASS 1: White Canvas
        glBindFramebuffer(GL_FRAMEBUFFER, fboCanvas);
        canvasShader.use();
        glClearColor(1,1,1,1);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // PASS 2: Blur on white canvas
        glBindFramebuffer(GL_FRAMEBUFFER, fboBlur);
        blurShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texCanvas);
        blurShader.setInt("uInputTex", 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // PASS 3: SSAO
        glBindFramebuffer(GL_FRAMEBUFFER, fboSSAO);
        ssaoShader.use();
        ssaoShader.setInt("uNormalDepthTex", 0);
        // bind depth+normal if available...
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // PASS 4: Scattering
        glBindFramebuffer(GL_FRAMEBUFFER, fboScatter);
        scatterShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texBlur);
        scatterShader.setInt("uBaseTex", 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // PASS 5: Sheen
        glBindFramebuffer(GL_FRAMEBUFFER, fboSheen);
        sheenShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texSSAO);
        sheenShader.setInt("uOcclTex", 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // PASS 6: Sheen → Blur map
        glBindFramebuffer(GL_FRAMEBUFFER, fboBlur);
        blurShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texSheen);
        blurShader.setInt("uInputTex", 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // PASS 7: Shadows & composite
        glBindFramebuffer(GL_FRAMEBUFFER, fboShadows);
        shadowShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texScatter);
        shadowShader.setInt("uColorTex", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texBlur);
        shadowShader.setInt("uSheenBlurTex", 1);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // PASS 8: Glossiness spreading
        glBindFramebuffer(GL_FRAMEBUFFER, fboGloss);
        glossShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texShadows);
        glossShader.setInt("uBaseTex", 0);
        glossShader.setFloat("uGlossSpread", 1.0f); // tune per GPU
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Final: Present to screen
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        blurShader.use();  // re‐use simple quad shader
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texGloss);
        blurShader.setInt("uInputTex", 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(win);
        glfwPollEvents();

        // Frame‐rate lock 
        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(t1 - t0).count();
        if (elapsed < frameDuration) {
            std::this_thread::sleep_for(
                std::chrono::duration<double>(frameDuration - elapsed)
            );
        }
    }

    // Cleanup
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
// main.cpp
#include <cstdio>
#include <vector>
#include <cmath>

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Window dimensions
static const int WIDTH  = 800;
static const int HEIGHT = 600;

// Perlin noise helper (Ken Perlin’s improved noise)
float perlinNoise(float x, float y) {
    static int p[512], permutation[256] = {
        151,160,137,91,90,15, // … fill in the standard 256 perm array …
        // (complete with values 0–255 in a scrambled order)
    };
    static bool initialized = false;
    if (!initialized) {
        for (int i = 0; i < 256; ++i)
            p[256 + i] = p[i] = permutation[i];
        initialized = true;
    }
    auto fade = [](float t) { return t * t * t * (t * (t * 6 - 15) + 10); };
    auto lerp = [](float a, float b, float t) { return a + t * (b - a); };
    auto grad = [](int hash, float x, float y) {
        switch (hash & 3) {
            case 0: return  x + y;
            case 1: return -x + y;
            case 2: return  x - y;
            case 3: return -x - y;
        }
        return 0.0f; // never happens
    };

    int xi = (int)std::floor(x) & 255;
    int yi = (int)std::floor(y) & 255;
    float xf = x - std::floor(x), yf = y - std::floor(y);
    float u = fade(xf), v = fade(yf);

    int aa = p[p[xi] + yi], ab = p[p[xi] + yi + 1];
    int ba = p[p[xi + 1] + yi], bb = p[p[xi + 1] + yi + 1];

    float x1 = lerp(grad(aa, xf,    yf),
                    grad(ba, xf-1,  yf),    u);
    float x2 = lerp(grad(ab, xf,    yf-1),
                    grad(bb, xf-1,  yf-1),  u);
    return (lerp(x1, x2, v) + 1.0f) * 0.5f; // normalize to [0,1]
}

// Shader sources
const char* vertSrc = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
out vec3 FragPos;
out vec3 Normal;
uniform mat4 model, view, projection;
void main(){
    FragPos = vec3(model * vec4(aPos,1.0));
    Normal  = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * vec4(FragPos,1.0);
}
)glsl";

const char* fragSrc = R"glsl(
#version 330 core
in vec3 FragPos;
in vec3 Normal;
out vec4 color;
uniform vec3 lightPos, viewPos;
void main(){
    // ambient
    vec3 ambient = 0.1 * vec3(0.2,0.7,0.3);
    // diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(0.2,0.7,0.3);
    // specular
    float specStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specStrength * spec * vec3(1.0);

    vec3 result = ambient + diffuse + specular;
    color = vec4(result, 1.0);
}
)glsl";

// Build and compile shaders
GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(shader, 512, nullptr, buf);
        std::fprintf(stderr, "Shader compile error: %s\n", buf);
    }
    return shader;
}

int main(){
    if (!glfwInit()) return -1;
    GLFWwindow* win = glfwCreateWindow(WIDTH, HEIGHT, "Perlin Terrain", nullptr, nullptr);
    if (!win) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(win);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return -1;

    // Generate terrain grid
    const int SIZE = 200;
    const float SCALE = 0.1f;
    std::vector<glm::vec3>  positions;
    std::vector<glm::vec3>  normals;
    std::vector<unsigned>   indices;

    positions.reserve(SIZE * SIZE);
    normals   .reserve(SIZE * SIZE);
    // compute positions & normals placeholder
    for (int z = 0; z < SIZE; ++z) {
        for (int x = 0; x < SIZE; ++x) {
            float fx = x * SCALE, fz = z * SCALE;
            float y  = perlinNoise(fx, fz) * 10.0f;
            positions.emplace_back(fx, y, fz);
            normals.emplace_back(0,1,0); // temp; we’ll recalc
        }
    }
    // build index list & compute normals
    for (int z = 0; z < SIZE - 1; ++z) {
        for (int x = 0; x < SIZE - 1; ++x) {
            int i0 = z * SIZE + x;
            int i1 = i0 + 1;
            int i2 = i0 + SIZE;
            int i3 = i2 + 1;
            // two triangles: (i0,i2,i1) and (i1,i2,i3)
            indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
            indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);
            // calc normals for both
            auto addNormal = [&](int a, int b, int c){
                glm::vec3 U = positions[b] - positions[a];
                glm::vec3 V = positions[c] - positions[a];
                glm::vec3 N = glm::normalize(glm::cross(U, V));
                normals[a] += N;
                normals[b] += N;
                normals[c] += N;
            };
            addNormal(i0,i2,i1);
            addNormal(i1,i2,i3);
        }
    }
    for (auto& n : normals) n = glm::normalize(n);

    // upload to GPU
    GLuint VAO, VBO[2], EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(2, VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3),
                 positions.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,nullptr);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3),
                 normals.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,0,nullptr);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 indices.size()*sizeof(unsigned),
                 indices.data(), GL_STATIC_DRAW);

    // compile & link
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glUseProgram(prog);

    // uniforms
    GLint modelLoc      = glGetUniformLocation(prog, "model");
    GLint viewLoc       = glGetUniformLocation(prog, "view");
    GLint projLoc       = glGetUniformLocation(prog, "projection");
    GLint lightPosLoc   = glGetUniformLocation(prog, "lightPos");
    GLint viewPosLoc    = glGetUniformLocation(prog, "viewPos");

    // camera setup
    glm::vec3 camPos(10,20,30), camTarget(10,0,10);
    glm::mat4 view = glm::lookAt(camPos, camTarget, glm::vec3(0,1,0));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f),
                        float(WIDTH)/HEIGHT, 0.1f, 100.0f);
    glm::mat4 model = glm::mat4(1.0f);

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(viewLoc,  1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc,  1, GL_FALSE, glm::value_ptr(proj));
    glUniform3f(lightPosLoc, 30.0f, 50.0f, 30.0f);
    glUniform3fv(viewPosLoc, 1, glm::value_ptr(camPos));

    glEnable(GL_DEPTH_TEST);

    while (!glfwWindowShouldClose(win)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(indices.size()),
                       GL_UNSIGNED_INT, nullptr);
        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}