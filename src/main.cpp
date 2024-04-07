#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>
#include <string>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);

void mouse_callback(GLFWwindow *window, double xpos, double ypos);

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);

void processInput(GLFWwindow *window);

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);

unsigned int loadTexture(const char *path);

unsigned int loadCubemap(vector<std::string> faces);



// settings
void renderQuad();

// settings
const unsigned int SCR_WIDTH = 1000;
const unsigned int SCR_HEIGHT = 800;
bool bloom = true;
bool hdr = true;
bool hdrKeyPressed = false;
float exposure = 1.0f;

// camera

float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

struct DiffLight {
    glm::vec3 position;
    glm::vec3 diffuse;
    float constant;
    float linear;
    float quadratic;
    float maxDist;
    bool OnOff = false;
};

struct SpotLight{
    glm::vec3 position;
    glm::vec3 direction;
};

float dirOn = 0.0f;
float sptOn = 1.0f;
float simpleLightOn = 1.0f;
bool blinn = true;

struct ProgramState {
    glm::vec3 clearColor = glm::vec3(0);
    bool ImGuiEnabled = false;
    Camera camera;
    bool CameraMouseMovementUpdateEnabled = true;

    ProgramState()
            : camera(glm::vec3(0.0f, 0.0f, 3.0f)) {}

    void SaveToFile(std::string filename);

    void LoadFromFile(std::string filename);
};

void ProgramState::SaveToFile(std::string filename) {
    std::ofstream out(filename);
    out << clearColor.r << '\n'
        << clearColor.g << '\n'
        << clearColor.b << '\n'
        << ImGuiEnabled << '\n'
        << camera.Position.x << '\n'
        << camera.Position.y << '\n'
        << camera.Position.z << '\n'
        << camera.Front.x << '\n'
        << camera.Front.y << '\n'
        << camera.Front.z << '\n';
}

void ProgramState::LoadFromFile(std::string filename) {
    std::ifstream in(filename);
    if (in) {
        in >> clearColor.r
           >> clearColor.g
           >> clearColor.b
           >> ImGuiEnabled
           >> camera.Position.x
           >> camera.Position.y
           >> camera.Position.z
           >> camera.Front.x
           >> camera.Front.y
           >> camera.Front.z;
    }
}

ProgramState *programState;

void DrawImGui(ProgramState *programState);

int main() {
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Race about to start", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
    stbi_set_flip_vertically_on_load(false);

    programState = new ProgramState;
    programState->LoadFromFile("resources/program_state.txt");
    if (programState->ImGuiEnabled) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    // Init Imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;



    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // build and compile shaders
    // -------------------------
    Shader ourShader("resources/shaders/modelLighting.vs", "resources/shaders/modelLighting.fs");
    Shader transparentShader("resources/shaders/transparent.vs", "resources/shaders/transparent.fs");
    Shader skyboxShader("resources/shaders/skybox.vs", "resources/shaders/skybox.fs");
    Shader shaderBlur("resources/shaders/bloom.vs", "resources/shaders/bloom.fs");
    Shader hdrShader("resources/shaders/hdr.vs", "resources/shaders/hdr.fs");


    float transparentVertices[] = {
            // positions
            0.0f,  0.5f,  0.0f,  0.0f,  0.0f,
            0.0f, -0.5f,  0.0f,  0.0f,  1.0f,
            1.0f, -0.5f,  0.0f,  1.0f,  1.0f,

            0.0f,  0.5f,  0.0f,  0.0f,  0.0f,
            1.0f, -0.5f,  0.0f,  1.0f,  1.0f,
            1.0f,  0.5f,  0.0f,  1.0f,  0.0f
    };

    float skyboxVertices[] = {
            // positions
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f,  1.0f
    };

    unsigned int transparentVAO, transparentVBO;
    glGenVertexArrays(1, &transparentVAO);
    glGenBuffers(1, &transparentVBO);
    glBindVertexArray(transparentVAO);
    glBindBuffer(GL_ARRAY_BUFFER, transparentVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(transparentVertices), transparentVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);

    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);


    // load models
    // -----------

    Model track("resources/objects/track/track.obj");
    track.SetShaderTextureNamePrefix("material.");

    Model balloons("resources/objects/balloons/balloons.obj");
    balloons.SetShaderTextureNamePrefix("material.");

    Model car1("resources/objects/car1/car.obj");
    car1.SetShaderTextureNamePrefix("material.");

    Model car2("resources/objects/car2/car.obj");
    car2.SetShaderTextureNamePrefix("material.");

    Model man1("resources/objects/man1/man.obj");
    man1.SetShaderTextureNamePrefix("material.");

    Model man2("resources/objects/man2/man.obj");
    man2.SetShaderTextureNamePrefix("material.");


    Model fence("resources/objects/fence/fence.obj");
    fence.SetShaderTextureNamePrefix("material.");


    // point lights sa samo difuznom komponentom
    vector<DiffLight> diffLights;
    diffLights = {
            {glm::vec3(0.31f, 0.138f, 3.3967f),glm::vec3(5.0f, 1.0f, 1.0f),
             1.0f,1.0f,1.0f,0.007f, true},
            {glm::vec3(0.31f, 0.138f, 3.4125f),glm::vec3(5.0f, 1.0f, 1.0f),
             1.0f,1.0f,1.0f,0.007f, true},
            {glm::vec3(0.31f, 0.138f, 3.428f),glm::vec3(5.0f, 1.0f, 1.0f),
             1.0f,1.0f,1.0f,0.007f, true},
            {glm::vec3(-0.127f, 0.014f, 3.4957f),glm::vec3(8.0f, 2.0f, 2.0f),
             1.0f,1.0f,1.0f,0.005f},
            {glm::vec3(-0.127f, 0.014f, 3.5346f),glm::vec3(8.0f, 2.0f, 2.0f),
             1.0f,1.0f,1.0f,0.005f},
            {glm::vec3(-0.0697, 0.0218f, 3.378f),glm::vec3(10.0f, 2.0f, 2.0f),
             1.0f,1.0f,1.0f,0.0025f},
            {glm::vec3(-0.0697, 0.0218f, 3.375f), glm::vec3(10.0f, 2.0f, 2.0f),
             1.0f,1.0f,1.0f,0.0025f},
            {glm::vec3(-0.0697, 0.0218f, 3.353f), glm::vec3(10.0f, 2.0f, 2.0f),
             1.0f,1.0f,1.0f,0.0025f},
            {glm::vec3(-0.0697, 0.0218f, 3.35f), glm::vec3(10.0f, 2.0f, 2.0f),
             1.0f,1.0f,1.0f,0.0025f},

    };

    vector<SpotLight> spotLights = {
            {glm::vec3(-0.05f, 0.0167f, 3.4953f), glm::vec3 (1.0f, 0, 0)},
            {glm::vec3(-0.05f, 0.0167f, 3.534f), glm::vec3 (1.0f, 0, 0)},
            {glm::vec3(0.037, 0.0176f, 3.384f), glm::vec3 (1.0f, 0, 0)},
            {glm::vec3(0.037, 0.0176f, 3.3458f), glm::vec3 (1.0f, 0, 0)},
    };


    //tekstura za sorted blending
    unsigned int transparentTexture = loadTexture(FileSystem::getPath("resources/textures/fire.png").c_str());

    vector<glm::vec3> fires
            {
                    glm::vec3(-1.188f, 2.515f, -0.315f),
                    glm::vec3( 3.012f, 2.1f, 1.0f),
                    glm::vec3( -3.66f, 1.68f, -0.23f),
                    glm::vec3(0.6585f, 3.28f, -6.06f),
                    glm::vec3( 2.327f, 2.612f, -1.81f),
                    glm::vec3(-8.693f, 2.662f, -1.67f),
                    glm::vec3(2.74f, 2.34f, 8.987f),
                    glm::vec3(6.056f, 2.505f, 3.606f),
                    glm::vec3(7.295f, 2.31f, 0.135f),
                    glm::vec3(8.519f, 3.05f, 5.4f)
            };

    transparentShader.use();
    transparentShader.setInt("texture1", 0);


    //skybox
    vector<std::string> faces
            {
                    FileSystem::getPath("resources/textures/skybox/right.jpg"),
                    FileSystem::getPath("resources/textures/skybox/left.jpg"),
                    FileSystem::getPath("resources/textures/skybox/top.jpg"),
                    FileSystem::getPath("resources/textures/skybox/bottom.jpg"),
                    FileSystem::getPath("resources/textures/skybox/front.jpg"),
                    FileSystem::getPath("resources/textures/skybox/back.jpg")
            };

    unsigned int cubemapTexture = loadCubemap(faces);

    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);

    //hdr i bloom
    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    // create 2 floating point color buffers (1 for normal rendering, other for brightness threshold values)
    unsigned int colorBuffers[2];
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // attach texture to framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }
    // create and attach depth buffer (renderbuffer)
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    // tell OpenGL which color attachments we'll use (of this framebuffer) for rendering
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);
    // finally check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ping-pong-framebuffer for blurring
    unsigned int pingpongFBO[2];
    unsigned int pingpongColorbuffers[2];
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                        GL_CLAMP_TO_EDGE); // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "Framebuffer not complete!" << std::endl;
    }

    shaderBlur.use();
    shaderBlur.setInt("image", 0);
    hdrShader.use();
    hdrShader.setInt("hdrBuffer", 0);
    hdrShader.setInt("bloomBlur", 1);


    // draw in wireframe
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // render loop
    while (!glfwWindowShouldClose(window)) {
        // per-frame time logic
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        // input
        processInput(window);

        std::sort(fires.begin(), fires.end(),
                  [cameraPosition = programState->camera.Position](const glm::vec3& a, const glm::vec3& b) {
                      float d1 = glm::distance(a, cameraPosition);
                      float d2 = glm::distance(b, cameraPosition);
                      return d1 > d2;
                  });

        // render
        glClearColor(programState->clearColor.r, programState->clearColor.g, programState->clearColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //direkciona svetla
        ourShader.use();


        ourShader.setVec3("dirLight.direction", glm::vec3(0.0f, -1.0f, 0.0f));
        ourShader.setVec3("dirLight.ambient", dirOn*glm::vec3(0.2f, 0.2f, 0.2f));
        ourShader.setVec3("dirLight.diffuse",  dirOn*glm::vec3(0.5f,0.5f,0.5f));
        ourShader.setVec3("dirLight.specular", dirOn*glm::vec3(0.1f, 0.1f, 0.1f));

        //svetla na kolima pozadi i startna svetla
        for(int i = 0; i < diffLights.size(); i++) {

            // za smenjivanje startnih svetala:
            float OnOff = 1.0f;
            int x = (int)currentFrame;
            //svetla se pale piramidalno i gase u isto vreme
            if(x%4 < i+1 && diffLights[i].OnOff){
                OnOff = 0;
            }

            string tmp = "diffLights[" + to_string(i) + "].";
            ourShader.setVec3(tmp + "position", diffLights[i].position);
            ourShader.setVec3(tmp + "diffuse", OnOff * simpleLightOn * diffLights[i].diffuse);
            ourShader.setFloat(tmp + "constant", diffLights[i].constant);
            ourShader.setFloat(tmp + "linear", diffLights[i].linear);
            ourShader.setFloat(tmp + "quadratic", diffLights[i].quadratic);
            ourShader.setFloat(tmp + "maxDist", diffLights[i].maxDist);
        }

        ourShader.setVec3("viewPosition", programState->camera.Position);
        //spot svetla na kolima napred
        for(int i = 0; i < spotLights.size(); i++){
            string tmp = "spotLights[" + to_string(i) + "].";

            ourShader.setVec3(tmp + "position", spotLights[i].position);
            ourShader.setVec3(tmp + "direction", spotLights[i].direction);
            ourShader.setFloat(tmp + "cutOff", glm::cos(glm::radians(8.0f)));
            ourShader.setFloat(tmp + "outerCutOff", glm::cos(glm::radians(15.0f)));

            ourShader.setVec3(tmp + "ambient", sptOn*glm::vec3(0.1f, 0.1f, 0.1f));
            ourShader.setVec3(tmp + "diffuse", sptOn*glm::vec3(8.0f, 8.0f, 12.0f));
            ourShader.setVec3(tmp + "specular", sptOn*glm::vec3(2.0f, 2.0f, 2.0f));

            ourShader.setFloat(tmp + "constant", 1.0f);
            ourShader.setFloat(tmp + "linear", 0.8f);
            ourShader.setFloat(tmp + "quadratic", 0.8f);
        }
        ourShader.setFloat("material.shininess", 16.0f);

        ourShader.setBool("blinn", blinn);
        ourShader.setVec3("viewPosition", programState->camera.Position);
        // view/projection transformations
        glm::mat4 projection = glm::perspective(glm::radians(programState->camera.Zoom),
                                                (float) SCR_WIDTH / (float) SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = programState->camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        glm::mat4 model = glm::mat4(1.0f);

        glEnable(GL_CULL_FACE);

        model = glm::mat4(1.0f);
        model = glm::translate(model,glm::vec3(0, 0, 4.0f));
        model = glm::scale(model, glm::vec3(0.02f, 0.02f, 0.02f));
        ourShader.setMat4("model", model);
        track.Draw(ourShader);

        model = glm::mat4(1.0f);
        model = glm::rotate(model, float(glfwGetTime()/20.0f), glm::vec3(0, 1.0f, 0));
        model = glm::translate(model, glm::vec3(-4.0f, 1.0f, 8.0f));
        model = glm::scale(model, glm::vec3(0.02f, 0.02f, 0.02f));
        ourShader.setMat4("model", model);
        balloons.Draw(ourShader);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-0.08f, 0.0f, 3.515f));
        model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0, 1.0f, 0));
        model = glm::scale(model, glm::vec3(0.016f, 0.016f, 0.016f));
        ourShader.setMat4("model", model);
        car1.Draw(ourShader);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-0.01, 0.0f, 3.365f));
        model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0, 1.0f, 0));
        model = glm::scale(model, glm::vec3(0.03f, 0.03f, 0.03f));
        ourShader.setMat4("model", model);
        car2.Draw(ourShader);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-0.1875f, 0.097f, 2.995f));
        model = glm::scale(model, glm::vec3(0.02f, 0.02f, 0.02f));
        ourShader.setMat4("model", model);
        man1.Draw(ourShader);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.21f, 0.095f, 3.005f));
        model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0, 1.0f, 0));
        model = glm::scale(model, glm::vec3(0.02f, 0.02f, 0.02f));
        ourShader.setMat4("model", model);
        man2.Draw(ourShader);

        // Ograda na nekim mestima je jednostrana pa se sa jedne strane ne vidi kada je ukljucen Face culling
        glDisable(GL_CULL_FACE);

        model = glm::mat4(1.0f);
        model = glm::translate(model,glm::vec3(0, 0, 4.0f));
        model = glm::scale(model, glm::vec3(0.02f, 0.02f, 0.02f));
        ourShader.setMat4("model", model);
        fence.Draw(ourShader);

        //skybox
        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        view = glm::mat4(glm::mat3(programState->camera.GetViewMatrix()));
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS); // set depth function back to default

        //sorted blending
        //posle skybox-a jer se u suprotnom vidi background color
        transparentShader.use();
        projection = glm::perspective(glm::radians(programState->camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        view = programState->camera.GetViewMatrix();
        model = glm::mat4(1.0f);
        transparentShader.setMat4("projection", projection);
        transparentShader.setMat4("view", view);
        glBindVertexArray(transparentVAO);
        glBindTexture(GL_TEXTURE_2D, transparentTexture);
        for (auto fire : fires)
        {
            model = glm::mat4(1.0f);
            model = glm::rotate(model, float(glfwGetTime()/20.0f), glm::vec3(0, 1.0f, 0));
            model = glm::translate(model, fire);
            model = glm::scale(model, glm::vec3(0.018f));
            transparentShader.setMat4("model", model);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        //HDR i Bloom
        bool horizontal = true, first_iteration = true;
        unsigned int amount = 10;
        shaderBlur.use();
        for (unsigned int i = 0; i < amount; i++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
            shaderBlur.setInt("horizontal", horizontal);
            glBindTexture(GL_TEXTURE_2D, first_iteration ? colorBuffers[1] : pingpongColorbuffers[!horizontal]);  // bind texture of other framebuffer (or scene if first iteration)
            renderQuad();
            horizontal = !horizontal;
            if (first_iteration)
                first_iteration = false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        hdrShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]);
        hdrShader.setBool("hdr", hdr);
        hdrShader.setBool("bloom", bloom);
        hdrShader.setFloat("exposure", exposure);
        renderQuad();

        if (programState->ImGuiEnabled)
            DrawImGui(programState);



        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    programState->SaveToFile("resources/program_state.txt");
    delete programState;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}


unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
                // positions        // texture Coords
                -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
                -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
                1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(RIGHT, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS && !hdrKeyPressed)
    {
        hdr = !hdr;
        hdrKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_H) == GLFW_RELEASE)
    {
        hdrKeyPressed = false;
    }

    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    {
        if (exposure > 0.005f)
            exposure -= 0.005f;
        else
            exposure = 0.0f;
    }
    else if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    {
        exposure += 0.005f;
    }

}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    if (programState->CameraMouseMovementUpdateEnabled)
        programState->camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    programState->camera.ProcessMouseScroll(yoffset);
}

void DrawImGui(ProgramState *programState) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();


//    {
//        static float f = 0.0f;
//        ImGui::Begin("Hello window");
//        ImGui::Text("Hello text");
//        ImGui::SliderFloat("Float slider", &f, 0.0, 1.0);
//        ImGui::ColorEdit3("Background color", (float *) &programState->clearColor);
//
//
//        ImGui::End();
//    }

    {
        ImGui::Begin("Camera info");
        const Camera& c = programState->camera;
        ImGui::Text("Camera position: (%f, %f, %f)", c.Position.x, c.Position.y, c.Position.z);
        ImGui::Text("(Yaw, Pitch): (%f, %f)", c.Yaw, c.Pitch);
        ImGui::Text("Camera front: (%f, %f, %f)", c.Front.x, c.Front.y, c.Front.z);
        ImGui::Checkbox("Camera mouse update", &programState->CameraMouseMovementUpdateEnabled);
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        programState->ImGuiEnabled = !programState->ImGuiEnabled;
        if (programState->ImGuiEnabled) {
            programState->CameraMouseMovementUpdateEnabled = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }

    if (key == GLFW_KEY_T && action == GLFW_PRESS) {
        if (dirOn == 1.0f) {
            dirOn = 0;
        } else {
            dirOn = 1.0f;
        }
    }

    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        if (sptOn == 1.0f) {
            sptOn = 0;
        } else {
            sptOn = 1.0f;
        }
    }

    if (key == GLFW_KEY_P && action == GLFW_PRESS) {
        blinn = !blinn;
    }

    if (key == GLFW_KEY_O && action == GLFW_PRESS) {
        if (simpleLightOn == 1.0f) {
            simpleLightOn = 0;
        } else {
            simpleLightOn = 1.0f;
        }
    }

    if (key == GLFW_KEY_B && action == GLFW_PRESS) {
        bloom = !bloom;
    }
}

unsigned int loadTexture(char const * path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT); // for this tutorial: use GL_CLAMP_TO_EDGE to prevent semi-transparent borders. Due to interpolation it takes texels from next repeat
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

unsigned int loadCubemap(vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}