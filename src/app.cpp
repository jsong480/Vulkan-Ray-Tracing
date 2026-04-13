#include "app.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

static std::string getExeDir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string dir(buf);
    return dir.substr(0, dir.find_last_of("\\/") + 1);
#else
    return "";
#endif
}

static std::string getShaderPath(const std::string& name) {
    return getExeDir() + "shaders/" + name;
}

static std::string getAssetPath(const std::string& name) {
    return getExeDir() + "assets/" + name;
}

// ─── Window + Input ─────────────────────────────────────────────────────────

void App::framebufferResizeCallback(GLFWwindow* w, int, int) {
    reinterpret_cast<App*>(glfwGetWindowUserPointer(w))->framebufferResized = true;
}

void App::cursorPosCallback(GLFWwindow* w, double xpos, double ypos) {
    auto* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(w));
    if (!app->mouseCaptured) return;

    if (app->firstMouse) {
        app->lastMouseX = xpos;
        app->lastMouseY = ypos;
        app->firstMouse = false;
        return;
    }
    float dx = static_cast<float>(xpos - app->lastMouseX);
    float dy = static_cast<float>(app->lastMouseY - ypos);
    app->lastMouseX = xpos;
    app->lastMouseY = ypos;
    app->camera.processMouse(dx, dy);
}

void App::mouseButtonCallback(GLFWwindow* w, int button, int action, int) {
    auto* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(w));
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS && !ImGui::GetIO().WantCaptureMouse) {
            app->mouseCaptured = true;
            app->firstMouse = true;
            glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else if (action == GLFW_RELEASE) {
            app->mouseCaptured = false;
            glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

void App::keyCallback(GLFWwindow* w, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(w, true);
}

void App::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(kWidth, kHeight,
                              "Vulkan Ray Tracing - Cornell Box", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetKeyCallback(window, keyCallback);
}

// ─── Vulkan setup ───────────────────────────────────────────────────────────

void App::initVulkan() {
    ctx.init(window);
    swapchain.init(ctx, window);
    commands.init(ctx.device, ctx.queueFamilies.graphicsFamily.value());
    commandBuffers = commands.allocate(ctx.device, kMaxFramesInFlight);
    createSyncObjects();
    initStorageImage();
    initScene();
    initRTPipeline();
    initDenoisePipeline();
    initImGui();
}

void App::initStorageImage() {
    auto destroy = [&](AllocatedImage& img) {
        if (img.image) destroyImage(ctx.allocator, ctx.device, img);
    };
    destroy(storageImage);
    destroy(accumImage);
    destroy(gbufferImage);
    destroy(denoisePing);
    destroy(denoisePong);

    auto ext = swapchain.extent;
    auto fmt = VK_FORMAT_R32G32B32A32_SFLOAT;
    storageImage = createStorageImage(ctx.allocator, ctx.device, ext, fmt);
    accumImage   = createStorageImage(ctx.allocator, ctx.device, ext, fmt);
    gbufferImage = createStorageImage(ctx.allocator, ctx.device, ext, fmt);
    denoisePing  = createStorageImage(ctx.allocator, ctx.device, ext, fmt);
    denoisePong  = createStorageImage(ctx.allocator, ctx.device, ext, fmt);

    auto cmd = commands.beginSingleTime(ctx.device);
    AllocatedImage* images[] = {
        &storageImage, &accumImage, &gbufferImage, &denoisePing, &denoisePong
    };
    for (auto* img : images) {
        cmdTransitionImageLayout(cmd, img->image,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            0, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT);
    }
    commands.endSingleTime(ctx.device, ctx.graphicsQueue, cmd);

    sampleCount = 0;
}

void App::initScene() {
    scene.buildCornellBox();
    scene.loadBunny(getAssetPath("bunny.obj"));
    scene.upload(ctx.device, ctx.allocator, commands, ctx.graphicsQueue);
    scene.buildAccel(ctx.device, ctx.allocator, commands, ctx.graphicsQueue);
    updateCamera();
}

void App::updateCamera() {
    float aspect = static_cast<float>(swapchain.extent.width)
                 / static_cast<float>(swapchain.extent.height);
    CameraUBO ubo = camera.ubo(aspect);

    void* mapped;
    vmaMapMemory(ctx.allocator, scene.uniformBuffer.allocation, &mapped);
    std::memcpy(mapped, &ubo, sizeof(ubo));
    vmaUnmapMemory(ctx.allocator, scene.uniformBuffer.allocation);
}

void App::updateBunnyTransform() {
    float rad = glm::radians(bunnyAngle);
    float c = std::cos(rad), s = std::sin(rad);

    VkTransformMatrixKHR xform{};
    xform.matrix[0][0] =  c; xform.matrix[0][2] = s; xform.matrix[0][3] = 0;
    xform.matrix[1][1] =  1; xform.matrix[1][3] = -1;
    xform.matrix[2][0] = -s; xform.matrix[2][2] = c; xform.matrix[2][3] = 0.35f;

    void* mapped;
    vmaMapMemory(ctx.allocator, scene.tlas.instanceBuffer.allocation, &mapped);
    auto* instances = static_cast<VkAccelerationStructureInstanceKHR*>(mapped);
    instances[1].transform = xform;
    vmaUnmapMemory(ctx.allocator, scene.tlas.instanceBuffer.allocation);
}

void App::initRTPipeline() {
    VkShaderStageFlags rtStages =
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    descriptorLayout = DescriptorLayoutBuilder()
        .addBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, rtStages)
        .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .addBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, rtStages)
        .addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
        .addBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
        .addBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
        .addBinding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
        .addBinding(7, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .addBinding(8, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .build(ctx.device);

    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    pcRange.offset     = 0;
    pcRange.size       = sizeof(RTPushConstants);

    VkPipelineLayoutCreateInfo plci{};
    plci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount         = 1;
    plci.pSetLayouts            = &descriptorLayout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges    = &pcRange;
    VK_CHECK(vkCreatePipelineLayout(ctx.device, &plci, nullptr, &pipelineLayout));

    auto rgen   = loadShaderModule(ctx.device, getShaderPath("raygen.rgen.spv"));
    auto rmiss  = loadShaderModule(ctx.device, getShaderPath("miss.rmiss.spv"));
    auto shadow = loadShaderModule(ctx.device, getShaderPath("shadow.rmiss.spv"));
    auto rchit  = loadShaderModule(ctx.device, getShaderPath("closesthit.rchit.spv"));

    RTPipelineBuilder builder;
    builder.addRayGenShader(rgen);
    builder.addMissShader(rmiss);
    builder.addMissShader(shadow);
    builder.addHitGroup(rchit);
    rtPipeline = builder.build(ctx.device, pipelineLayout, 1);

    vkDestroyShaderModule(ctx.device, rgen, nullptr);
    vkDestroyShaderModule(ctx.device, rmiss, nullptr);
    vkDestroyShaderModule(ctx.device, shadow, nullptr);
    vkDestroyShaderModule(ctx.device, rchit, nullptr);

    sbt.build(ctx.device, ctx.allocator, rtPipeline,
              ctx.rtPipelineProperties, 1, 2, 1);

    descriptorPool = createDescriptorPool(ctx.device, {
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4},
    }, 1);
    descriptorSet = allocateDescriptorSet(ctx.device, descriptorPool, descriptorLayout);

    writeDescriptors();
}

void App::writeDescriptors() {
    VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
    asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    asInfo.accelerationStructureCount = 1;
    asInfo.pAccelerationStructures    = &scene.tlas.accel.handle;

    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageView   = storageImage.view;
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo accumInfo{};
    accumInfo.imageView   = accumImage.view;
    accumInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo gbInfo{};
    gbInfo.imageView   = gbufferImage.view;
    gbInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorBufferInfo uboInfo{};
    uboInfo.buffer = scene.uniformBuffer.buffer;
    uboInfo.range  = sizeof(CameraUBO);

    VkDescriptorBufferInfo vertInfo = {scene.vertexBuffer.buffer,        0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo idxInfo  = {scene.indexBuffer.buffer,         0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo miInfo   = {scene.materialIndexBuffer.buffer, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo matInfo  = {scene.materialBuffer.buffer,      0, VK_WHOLE_SIZE};

    std::array<VkWriteDescriptorSet, 9> w{};
    for (auto& wd : w) {
        wd.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wd.dstSet          = descriptorSet;
        wd.descriptorCount = 1;
    }

    w[0].pNext          = &asInfo;
    w[0].dstBinding     = 0;
    w[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    w[1].dstBinding     = 1;
    w[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    w[1].pImageInfo     = &imgInfo;

    w[2].dstBinding     = 2;
    w[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w[2].pBufferInfo    = &uboInfo;

    w[3].dstBinding = 3; w[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; w[3].pBufferInfo = &vertInfo;
    w[4].dstBinding = 4; w[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; w[4].pBufferInfo = &idxInfo;
    w[5].dstBinding = 5; w[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; w[5].pBufferInfo = &miInfo;
    w[6].dstBinding = 6; w[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; w[6].pBufferInfo = &matInfo;

    w[7].dstBinding     = 7;
    w[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    w[7].pImageInfo     = &accumInfo;

    w[8].dstBinding     = 8;
    w[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    w[8].pImageInfo     = &gbInfo;

    vkUpdateDescriptorSets(ctx.device,
        static_cast<uint32_t>(w.size()), w.data(), 0, nullptr);
}

void App::initDenoisePipeline() {
    denoiseDescLayout = DescriptorLayoutBuilder()
        .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
        .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
        .addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
        .addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
        .addBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
        .build(ctx.device);

    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcRange.offset     = 0;
    pcRange.size       = sizeof(int32_t) * 3;

    VkPipelineLayoutCreateInfo plci{};
    plci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount         = 1;
    plci.pSetLayouts            = &denoiseDescLayout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges    = &pcRange;
    VK_CHECK(vkCreatePipelineLayout(ctx.device, &plci, nullptr, &denoisePipeLayout));

    auto compModule = loadShaderModule(ctx.device, getShaderPath("denoise.comp.spv"));

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = compModule;
    stage.pName  = "main";

    VkComputePipelineCreateInfo ci{};
    ci.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    ci.stage  = stage;
    ci.layout = denoisePipeLayout;
    VK_CHECK(vkCreateComputePipelines(ctx.device, VK_NULL_HANDLE, 1, &ci, nullptr, &denoisePipeline));

    vkDestroyShaderModule(ctx.device, compModule, nullptr);

    denoiseDescPool = createDescriptorPool(ctx.device, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 5},
    }, 1);
    denoiseDescSet = allocateDescriptorSet(ctx.device, denoiseDescPool, denoiseDescLayout);

    writeDenoiseDescriptors();
}

void App::writeDenoiseDescriptors() {
    VkDescriptorImageInfo infos[5]{};
    AllocatedImage* images[] = {
        &accumImage, &denoisePing, &denoisePong, &gbufferImage, &storageImage
    };
    for (int i = 0; i < 5; i++) {
        infos[i].imageView   = images[i]->view;
        infos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    }

    std::array<VkWriteDescriptorSet, 5> w{};
    for (int i = 0; i < 5; i++) {
        w[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[i].dstSet          = denoiseDescSet;
        w[i].dstBinding      = static_cast<uint32_t>(i);
        w[i].descriptorCount = 1;
        w[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        w[i].pImageInfo      = &infos[i];
    }
    vkUpdateDescriptorSets(ctx.device,
        static_cast<uint32_t>(w.size()), w.data(), 0, nullptr);
}

// ─── ImGui ──────────────────────────────────────────────────────────────────

void App::initImGui() {
    // Render pass: TRANSFER_DST → PRESENT_SRC (overlays ImGui onto blit result)
    VkAttachmentDescription attachment{};
    attachment.format         = swapchain.imageFormat;
    attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout  = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    attachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    VkRenderPassCreateInfo rpci{};
    rpci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = 1;
    rpci.pAttachments    = &attachment;
    rpci.subpassCount    = 1;
    rpci.pSubpasses      = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies   = &dep;
    VK_CHECK(vkCreateRenderPass(ctx.device, &rpci, nullptr, &imguiRenderPass));

    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10};
    VkDescriptorPoolCreateInfo dpci{};
    dpci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    dpci.maxSets       = 10;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes    = &poolSize;
    VK_CHECK(vkCreateDescriptorPool(ctx.device, &dpci, nullptr, &imguiDescPool));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding   = 6.0f;
    style.FrameRounding    = 4.0f;
    style.GrabRounding     = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.Alpha            = 0.92f;

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance       = ctx.instance;
    initInfo.PhysicalDevice = ctx.physicalDevice;
    initInfo.Device         = ctx.device;
    initInfo.QueueFamily    = ctx.queueFamilies.graphicsFamily.value();
    initInfo.Queue          = ctx.graphicsQueue;
    initInfo.DescriptorPool = imguiDescPool;
    initInfo.RenderPass     = imguiRenderPass;
    initInfo.MinImageCount  = swapchain.imageCount();
    initInfo.ImageCount     = swapchain.imageCount();
    initInfo.MSAASamples    = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&initInfo);

    createImGuiFramebuffers();
}

void App::createImGuiFramebuffers() {
    destroyImGuiFramebuffers();
    imguiFramebuffers.resize(swapchain.imageCount());
    for (uint32_t i = 0; i < swapchain.imageCount(); i++) {
        VkFramebufferCreateInfo fbci{};
        fbci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbci.renderPass      = imguiRenderPass;
        fbci.attachmentCount = 1;
        fbci.pAttachments    = &swapchain.imageViews[i];
        fbci.width           = swapchain.extent.width;
        fbci.height          = swapchain.extent.height;
        fbci.layers          = 1;
        VK_CHECK(vkCreateFramebuffer(ctx.device, &fbci, nullptr, &imguiFramebuffers[i]));
    }
}

void App::destroyImGuiFramebuffers() {
    for (auto fb : imguiFramebuffers)
        if (fb) vkDestroyFramebuffer(ctx.device, fb, nullptr);
    imguiFramebuffers.clear();
}

// ─── Sync ───────────────────────────────────────────────────────────────────

void App::createSyncObjects() {
    VkSemaphoreCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < kMaxFramesInFlight; i++) {
        VK_CHECK(vkCreateSemaphore(ctx.device, &sci, nullptr, &imageAvailableSems[i]));
        VK_CHECK(vkCreateSemaphore(ctx.device, &sci, nullptr, &renderFinishedSems[i]));
        VK_CHECK(vkCreateFence(ctx.device, &fci, nullptr, &inFlightFences[i]));
    }
}

// ─── Swapchain recreation ───────────────────────────────────────────────────

void App::recreateSwapchain() {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    while (w == 0 || h == 0) { glfwGetFramebufferSize(window, &w, &h); glfwWaitEvents(); }

    vkDeviceWaitIdle(ctx.device);
    swapchain.cleanup(ctx.device);
    swapchain.init(ctx, window);
    initStorageImage();
    updateCamera();
    createImGuiFramebuffers();

    if (descriptorSet != VK_NULL_HANDLE) {
        VkDescriptorImageInfo imgInfo{};
        imgInfo.imageView   = storageImage.view;
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkDescriptorImageInfo accumInfo{};
        accumInfo.imageView   = accumImage.view;
        accumInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkDescriptorImageInfo gbInfo{};
        gbInfo.imageView   = gbufferImage.view;
        gbInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        std::array<VkWriteDescriptorSet, 3> wr{};
        for (auto& w : wr) {
            w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet          = descriptorSet;
            w.descriptorCount = 1;
            w.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        }
        wr[0].dstBinding = 1; wr[0].pImageInfo = &imgInfo;
        wr[1].dstBinding = 7; wr[1].pImageInfo = &accumInfo;
        wr[2].dstBinding = 8; wr[2].pImageInfo = &gbInfo;

        vkUpdateDescriptorSets(ctx.device,
            static_cast<uint32_t>(wr.size()), wr.data(), 0, nullptr);
    }

    if (denoiseDescSet != VK_NULL_HANDLE) {
        writeDenoiseDescriptors();
    }
}

// ─── Command recording ─────────────────────────────────────────────────────

void App::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK(vkBeginCommandBuffer(cmd, &bi));

    if (rtPipeline != VK_NULL_HANDLE) {
        // ── TLAS update (bunny rotation) ─────────────────────────────
        if (bunnyRotating) {
            cmdUpdateTLAS(cmd, scene.tlas.accel, scene.tlas.instanceBuffer,
                          scene.tlas.scratchBuffer, 2);
        }

        // ── Ray trace ────────────────────────────────────────────────
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
            pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        RTPushConstants rtPC{sampleCount, maxBounces, lightIntensity, glassIOR};
        vkCmdPushConstants(cmd, pipelineLayout,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(rtPC), &rtPC);

        rtCmdTraceRays(cmd,
            &sbt.raygenRegion, &sbt.missRegion,
            &sbt.hitRegion,    &sbt.callableRegion,
            storageImage.extent.width, storageImage.extent.height, 1);

        // ── Barrier: RT write → compute read ─────────────────────────
        VkMemoryBarrier rtToCompute{};
        rtToCompute.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        rtToCompute.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        rtToCompute.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 1, &rtToCompute, 0, nullptr, 0, nullptr);

        // ── Denoise / tone mapping ──────────────────────────────────
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, denoisePipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
            denoisePipeLayout, 0, 1, &denoiseDescSet, 0, nullptr);

        uint32_t gx = (storageImage.extent.width  + 15) / 16;
        uint32_t gy = (storageImage.extent.height + 15) / 16;

        int passes = denoiserEnabled ? denoisePasses : 0;
        if (passes == 0) {
            struct { int32_t stepSize, passIndex, isLastPass; } dpc = {0, 0, 1};
            vkCmdPushConstants(cmd, denoisePipeLayout,
                VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(dpc), &dpc);
            vkCmdDispatch(cmd, gx, gy, 1);
        } else {
            VkMemoryBarrier compBarrier{};
            compBarrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            compBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            compBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            for (int pass = 0; pass < passes; pass++) {
                struct { int32_t stepSize, passIndex, isLastPass; } dpc = {
                    1 << pass, pass, (pass == passes - 1) ? 1 : 0
                };
                vkCmdPushConstants(cmd, denoisePipeLayout,
                    VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(dpc), &dpc);
                vkCmdDispatch(cmd, gx, gy, 1);

                if (pass < passes - 1) {
                    vkCmdPipelineBarrier(cmd,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        0, 1, &compBarrier, 0, nullptr, 0, nullptr);
                }
            }
        }

        // ── Blit storageImage → swapchain ───────────────────────────
        cmdTransitionImageLayout(cmd, storageImage.image,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);

        cmdTransitionImageLayout(cmd, swapchain.images[imageIndex],
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, VK_ACCESS_TRANSFER_WRITE_BIT);

        VkImageBlit blit{};
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.srcOffsets[1]  = {(int32_t)storageImage.extent.width,
                               (int32_t)storageImage.extent.height, 1};
        blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.dstOffsets[1]  = {(int32_t)swapchain.extent.width,
                               (int32_t)swapchain.extent.height, 1};
        vkCmdBlitImage(cmd,
            storageImage.image,           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            swapchain.images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit, VK_FILTER_NEAREST);

        cmdTransitionImageLayout(cmd, storageImage.image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT);

    } else {
        cmdTransitionImageLayout(cmd, swapchain.images[imageIndex],
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, VK_ACCESS_TRANSFER_WRITE_BIT);
        VkClearColorValue cc = {{0.02f, 0.02f, 0.04f, 1.0f}};
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(cmd, swapchain.images[imageIndex],
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &cc, 1, &range);
    }

    // ── ImGui overlay (both branches leave swapchain in TRANSFER_DST) ────
    {
        VkRenderPassBeginInfo rpbi{};
        rpbi.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpbi.renderPass  = imguiRenderPass;
        rpbi.framebuffer = imguiFramebuffers[imageIndex];
        rpbi.renderArea  = {{0, 0}, swapchain.extent};
        vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
        vkCmdEndRenderPass(cmd);
    }

    VK_CHECK(vkEndCommandBuffer(cmd));
}

// ─── Draw / Loop / Lifecycle ────────────────────────────────────────────────

void App::drawFrame() {
    vkWaitForFences(ctx.device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(ctx.device, swapchain.swapchain, UINT64_MAX,
        imageAvailableSems[currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) { recreateSwapchain(); return; }

    vkResetFences(ctx.device, 1, &inFlightFences[currentFrame]);
    vkResetCommandBuffer(commandBuffers[currentFrame], 0);
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{};
    si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = &imageAvailableSems[currentFrame];
    si.pWaitDstStageMask    = &waitStage;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &commandBuffers[currentFrame];
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = &renderFinishedSems[currentFrame];
    VK_CHECK(vkQueueSubmit(ctx.graphicsQueue, 1, &si, inFlightFences[currentFrame]));

    VkPresentInfoKHR pi{};
    pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores    = &renderFinishedSems[currentFrame];
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &swapchain.swapchain;
    pi.pImageIndices      = &imageIndex;
    result = vkQueuePresentKHR(ctx.presentQueue, &pi);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapchain();
    }

    sampleCount++;
    currentFrame = (currentFrame + 1) % kMaxFramesInFlight;
}

void App::mainLoop() {
    lastFrameTime = static_cast<float>(glfwGetTime());

    while (!glfwWindowShouldClose(window)) {
        float now = static_cast<float>(glfwGetTime());
        float dt  = now - lastFrameTime;
        lastFrameTime = now;

        glfwPollEvents();

        // ── ImGui frame ──────────────────────────────────────────────
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
        ImGui::Begin("Ray Tracing Controls");

        ImGui::Text("FPS: %.1f  |  SPP: %u", displayFPS, sampleCount);
        ImGui::Separator();

        bool rtChanged = false;

        ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "Rendering");
        if (ImGui::SliderInt("Max Bounces", &maxBounces, 1, 16)) rtChanged = true;
        if (ImGui::SliderFloat("Light Intensity", &lightIntensity, 0.1f, 5.0f)) rtChanged = true;

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "Glass");
        if (ImGui::SliderFloat("IOR", &glassIOR, 1.0f, 2.5f)) rtChanged = true;

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "Denoiser");
        ImGui::Checkbox("Enable##denoiser", &denoiserEnabled);
        ImGui::SliderInt("Passes", &denoisePasses, 1, 5);

        ImGui::Separator();
        ImGui::Checkbox("Bunny Rotation", &bunnyRotating);

        if (rtChanged) sampleCount = 0;

        ImGui::End();
        // ─────────────────────────────────────────────────────────────

        if (mouseCaptured) {
            glm::vec3 f = camera.front();
            glm::vec3 r = glm::normalize(glm::cross(f, glm::vec3(0, 1, 0)));
            float v = camera.speed * dt;

            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { camera.position += f * v; camera.moved = true; }
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { camera.position -= f * v; camera.moved = true; }
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { camera.position -= r * v; camera.moved = true; }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { camera.position += r * v; camera.moved = true; }
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)      { camera.position.y += v; camera.moved = true; }
            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) { camera.position.y -= v; camera.moved = true; }
        }

        if (bunnyRotating) {
            bunnyAngle += 45.0f * dt;
            updateBunnyTransform();
            sampleCount = 0;
        }

        if (camera.moved) {
            updateCamera();
            sampleCount = 0;
            camera.moved = false;
        }

        ImGui::Render();
        drawFrame();

        titleFrames++;
        titleTimer += dt;
        if (titleTimer >= 0.5f) {
            displayFPS = titleFrames / titleTimer;
            char title[256];
            std::snprintf(title, sizeof(title),
                "Vulkan RT | %.1f FPS | %u SPP | Right-click: camera, ESC: quit",
                displayFPS, sampleCount);
            glfwSetWindowTitle(window, title);
            titleFrames = 0;
            titleTimer  = 0;
        }
    }
    vkDeviceWaitIdle(ctx.device);
}

void App::run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

void App::cleanup() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    destroyImGuiFramebuffers();
    if (imguiDescPool)   vkDestroyDescriptorPool(ctx.device, imguiDescPool, nullptr);
    if (imguiRenderPass) vkDestroyRenderPass(ctx.device, imguiRenderPass, nullptr);

    scene.cleanup(ctx.device, ctx.allocator);
    sbt.cleanup(ctx.allocator);

    if (denoisePipeline)   vkDestroyPipeline(ctx.device, denoisePipeline, nullptr);
    if (denoisePipeLayout) vkDestroyPipelineLayout(ctx.device, denoisePipeLayout, nullptr);
    if (denoiseDescPool)   vkDestroyDescriptorPool(ctx.device, denoiseDescPool, nullptr);
    if (denoiseDescLayout) vkDestroyDescriptorSetLayout(ctx.device, denoiseDescLayout, nullptr);

    if (rtPipeline)       vkDestroyPipeline(ctx.device, rtPipeline, nullptr);
    if (pipelineLayout)   vkDestroyPipelineLayout(ctx.device, pipelineLayout, nullptr);
    if (descriptorPool)   vkDestroyDescriptorPool(ctx.device, descriptorPool, nullptr);
    if (descriptorLayout) vkDestroyDescriptorSetLayout(ctx.device, descriptorLayout, nullptr);

    destroyImage(ctx.allocator, ctx.device, denoisePong);
    destroyImage(ctx.allocator, ctx.device, denoisePing);
    destroyImage(ctx.allocator, ctx.device, gbufferImage);
    destroyImage(ctx.allocator, ctx.device, accumImage);
    destroyImage(ctx.allocator, ctx.device, storageImage);

    for (int i = 0; i < kMaxFramesInFlight; i++) {
        vkDestroySemaphore(ctx.device, imageAvailableSems[i], nullptr);
        vkDestroySemaphore(ctx.device, renderFinishedSems[i], nullptr);
        vkDestroyFence(ctx.device, inFlightFences[i], nullptr);
    }
    commands.cleanup(ctx.device);
    swapchain.cleanup(ctx.device);
    ctx.cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
}
