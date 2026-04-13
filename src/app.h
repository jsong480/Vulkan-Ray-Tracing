#pragma once

#include "vk_context.h"
#include "vk_swapchain.h"
#include "vk_commands.h"
#include "vk_buffer.h"
#include "vk_descriptors.h"
#include "vk_accel.h"
#include "vk_pipeline.h"
#include "scene.h"
#include "camera.h"

struct RTPushConstants {
    uint32_t frameCount;
    int32_t  maxBounces;
    float    lightIntensity;
    float    glassIOR;
};

class App {
public:
    void run();

private:
    static constexpr uint32_t kWidth  = 1280;
    static constexpr uint32_t kHeight = 720;
    static constexpr int kMaxFramesInFlight = 2;

    GLFWwindow*    window             = nullptr;
    bool           framebufferResized = false;
    VkContext      ctx;
    Swapchain      swapchain;
    CommandManager commands;

    std::vector<VkCommandBuffer> commandBuffers;
    VkSemaphore imageAvailableSems[kMaxFramesInFlight]{};
    VkSemaphore renderFinishedSems[kMaxFramesInFlight]{};
    VkFence     inFlightFences    [kMaxFramesInFlight]{};
    uint32_t    currentFrame = 0;

    AllocatedImage     storageImage;
    AllocatedImage     accumImage;
    AllocatedImage     gbufferImage;
    AllocatedImage     denoisePing;
    AllocatedImage     denoisePong;
    uint32_t           sampleCount = 0;

    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    VkDescriptorPool      descriptorPool   = VK_NULL_HANDLE;
    VkDescriptorSet       descriptorSet    = VK_NULL_HANDLE;
    VkPipelineLayout      pipelineLayout   = VK_NULL_HANDLE;
    VkPipeline            rtPipeline       = VK_NULL_HANDLE;
    ShaderBindingTable    sbt;

    VkDescriptorSetLayout denoiseDescLayout = VK_NULL_HANDLE;
    VkDescriptorPool      denoiseDescPool   = VK_NULL_HANDLE;
    VkDescriptorSet       denoiseDescSet    = VK_NULL_HANDLE;
    VkPipelineLayout      denoisePipeLayout = VK_NULL_HANDLE;
    VkPipeline            denoisePipeline   = VK_NULL_HANDLE;

    VkRenderPass               imguiRenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> imguiFramebuffers;
    VkDescriptorPool           imguiDescPool   = VK_NULL_HANDLE;

    Scene  scene;
    Camera camera;

    int   maxBounces      = 8;
    float lightIntensity  = 1.0f;
    float glassIOR        = 1.5f;
    int   denoisePasses   = 4;
    bool  denoiserEnabled = true;
    bool  bunnyRotating   = false;
    float bunnyAngle      = 0.0f;

    bool   mouseCaptured = false;
    bool   firstMouse    = true;
    double lastMouseX    = 0;
    double lastMouseY    = 0;
    float  lastFrameTime = 0;
    float  titleTimer    = 0;
    uint32_t titleFrames = 0;
    float  displayFPS    = 0;

    void initWindow();
    void initVulkan();
    void initStorageImage();
    void initScene();
    void initRTPipeline();
    void initDenoisePipeline();
    void initImGui();
    void createImGuiFramebuffers();
    void destroyImGuiFramebuffers();
    void writeDescriptors();
    void writeDenoiseDescriptors();
    void updateCamera();
    void updateBunnyTransform();
    void createSyncObjects();
    void mainLoop();
    void drawFrame();
    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);
    void recreateSwapchain();
    void cleanup();

    static void framebufferResizeCallback(GLFWwindow* w, int, int);
    static void cursorPosCallback(GLFWwindow* w, double x, double y);
    static void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods);
    static void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods);
};
