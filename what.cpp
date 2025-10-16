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