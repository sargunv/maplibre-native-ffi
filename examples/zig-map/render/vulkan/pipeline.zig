const std = @import("std");

const c = @import("../../c.zig").c;
const util = @import("util.zig");

pub const Pipeline = struct {
    allocator: std.mem.Allocator,
    render_pass: c.VkRenderPass,
    descriptor_set_layout: c.VkDescriptorSetLayout,
    pipeline_layout: c.VkPipelineLayout,
    handle: c.VkPipeline,
    sampler: c.VkSampler,
    descriptor_pool: c.VkDescriptorPool,
    descriptor_set: c.VkDescriptorSet,
    descriptor_image_view: c.VkImageView,

    pub fn init(
        allocator: std.mem.Allocator,
        device: c.VkDevice,
        swapchain_format: c.VkFormat,
    ) !Pipeline {
        var self = Pipeline{
            .allocator = allocator,
            .render_pass = null,
            .descriptor_set_layout = null,
            .pipeline_layout = null,
            .handle = null,
            .sampler = null,
            .descriptor_pool = null,
            .descriptor_set = null,
            .descriptor_image_view = null,
        };
        errdefer self.deinit(device);

        try self.createRenderPass(device, swapchain_format);
        try self.createDescriptorState(device);
        try self.createPipeline(device);
        return self;
    }

    pub fn deinit(self: *Pipeline, device: c.VkDevice) void {
        if (self.handle != null) c.vkDestroyPipeline(device, self.handle, null);
        self.handle = null;
        if (self.pipeline_layout != null) {
            c.vkDestroyPipelineLayout(device, self.pipeline_layout, null);
        }
        self.pipeline_layout = null;
        if (self.descriptor_pool != null) {
            c.vkDestroyDescriptorPool(device, self.descriptor_pool, null);
        }
        self.descriptor_pool = null;
        if (self.sampler != null) c.vkDestroySampler(device, self.sampler, null);
        self.sampler = null;
        if (self.descriptor_set_layout != null) {
            c.vkDestroyDescriptorSetLayout(device, self.descriptor_set_layout, null);
        }
        self.descriptor_set_layout = null;
        if (self.render_pass != null) c.vkDestroyRenderPass(device, self.render_pass, null);
        self.render_pass = null;
        self.descriptor_set = null;
        self.descriptor_image_view = null;
    }

    pub fn updateDescriptor(self: *Pipeline, device: c.VkDevice, image_view: c.VkImageView) void {
        const image_info = c.VkDescriptorImageInfo{
            .sampler = self.sampler,
            .imageView = image_view,
            .imageLayout = c.VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        const write = c.VkWriteDescriptorSet{
            .sType = c.VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = null,
            .dstSet = self.descriptor_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = c.VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &image_info,
            .pBufferInfo = null,
            .pTexelBufferView = null,
        };
        c.vkUpdateDescriptorSets(device, 1, &write, 0, null);
        self.descriptor_image_view = image_view;
    }

    fn createRenderPass(
        self: *Pipeline,
        device: c.VkDevice,
        swapchain_format: c.VkFormat,
    ) !void {
        const attachment = c.VkAttachmentDescription{
            .flags = 0,
            .format = swapchain_format,
            .samples = c.VK_SAMPLE_COUNT_1_BIT,
            .loadOp = c.VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = c.VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = c.VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = c.VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = c.VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = c.VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        };
        const color_ref = c.VkAttachmentReference{
            .attachment = 0,
            .layout = c.VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        const subpass = c.VkSubpassDescription{
            .flags = 0,
            .pipelineBindPoint = c.VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = 0,
            .pInputAttachments = null,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_ref,
            .pResolveAttachments = null,
            .pDepthStencilAttachment = null,
            .preserveAttachmentCount = 0,
            .pPreserveAttachments = null,
        };
        const dependency = c.VkSubpassDependency{
            .srcSubpass = c.VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = c.VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = c.VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = c.VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0,
        };
        const create_info = c.VkRenderPassCreateInfo{
            .sType = c.VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext = null,
            .flags = 0,
            .attachmentCount = 1,
            .pAttachments = &attachment,
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 1,
            .pDependencies = &dependency,
        };
        try util.expectVk(c.vkCreateRenderPass(
            device,
            &create_info,
            null,
            &self.render_pass,
        ));
    }

    fn createDescriptorState(self: *Pipeline, device: c.VkDevice) !void {
        const binding = c.VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = c.VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = c.VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = null,
        };
        const layout_info = c.VkDescriptorSetLayoutCreateInfo{
            .sType = c.VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = null,
            .flags = 0,
            .bindingCount = 1,
            .pBindings = &binding,
        };
        try util.expectVk(c.vkCreateDescriptorSetLayout(
            device,
            &layout_info,
            null,
            &self.descriptor_set_layout,
        ));

        const sampler_info = c.VkSamplerCreateInfo{
            .sType = c.VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = null,
            .flags = 0,
            .magFilter = c.VK_FILTER_LINEAR,
            .minFilter = c.VK_FILTER_LINEAR,
            .mipmapMode = c.VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = c.VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = c.VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = c.VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .mipLodBias = 0,
            .anisotropyEnable = c.VK_FALSE,
            .maxAnisotropy = 1,
            .compareEnable = c.VK_FALSE,
            .compareOp = c.VK_COMPARE_OP_ALWAYS,
            .minLod = 0,
            .maxLod = 0,
            .borderColor = c.VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            .unnormalizedCoordinates = c.VK_FALSE,
        };
        try util.expectVk(c.vkCreateSampler(device, &sampler_info, null, &self.sampler));

        const pool_size = c.VkDescriptorPoolSize{
            .type = c.VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
        };
        const pool_info = c.VkDescriptorPoolCreateInfo{
            .sType = c.VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = null,
            .flags = 0,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &pool_size,
        };
        try util.expectVk(c.vkCreateDescriptorPool(
            device,
            &pool_info,
            null,
            &self.descriptor_pool,
        ));
        const alloc_info = c.VkDescriptorSetAllocateInfo{
            .sType = c.VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = null,
            .descriptorPool = self.descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &self.descriptor_set_layout,
        };
        try util.expectVk(c.vkAllocateDescriptorSets(
            device,
            &alloc_info,
            &self.descriptor_set,
        ));
    }

    fn createPipeline(self: *Pipeline, device: c.VkDevice) !void {
        const vert = try createShader(
            self.allocator,
            device,
            @embedFile("shaders/fullscreen.vert.spv"),
        );
        defer c.vkDestroyShaderModule(device, vert, null);
        const frag = try createShader(
            self.allocator,
            device,
            @embedFile("shaders/sample.frag.spv"),
        );
        defer c.vkDestroyShaderModule(device, frag, null);

        const stages = [_]c.VkPipelineShaderStageCreateInfo{
            shaderStage(c.VK_SHADER_STAGE_VERTEX_BIT, vert),
            shaderStage(c.VK_SHADER_STAGE_FRAGMENT_BIT, frag),
        };
        const vertex_input = emptyVertexInputState();
        const input_assembly = triangleInputAssemblyState();
        const viewport_state = dynamicViewportState();
        const rasterizer = rasterizationState();
        const multisampling = multisampleState();
        const color_blend_attachment = colorBlendAttachmentState();
        const color_blending = c.VkPipelineColorBlendStateCreateInfo{
            .sType = c.VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = null,
            .flags = 0,
            .logicOpEnable = c.VK_FALSE,
            .logicOp = c.VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = &color_blend_attachment,
            .blendConstants = .{ 0, 0, 0, 0 },
        };
        const dynamic_states = [_]c.VkDynamicState{
            c.VK_DYNAMIC_STATE_VIEWPORT,
            c.VK_DYNAMIC_STATE_SCISSOR,
        };
        const dynamic_state = c.VkPipelineDynamicStateCreateInfo{
            .sType = c.VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext = null,
            .flags = 0,
            .dynamicStateCount = dynamic_states.len,
            .pDynamicStates = &dynamic_states,
        };
        const layout_info = c.VkPipelineLayoutCreateInfo{
            .sType = c.VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = null,
            .flags = 0,
            .setLayoutCount = 1,
            .pSetLayouts = &self.descriptor_set_layout,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = null,
        };
        try util.expectVk(c.vkCreatePipelineLayout(
            device,
            &layout_info,
            null,
            &self.pipeline_layout,
        ));
        const pipeline_info = c.VkGraphicsPipelineCreateInfo{
            .sType = c.VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = null,
            .flags = 0,
            .stageCount = stages.len,
            .pStages = &stages,
            .pVertexInputState = &vertex_input,
            .pInputAssemblyState = &input_assembly,
            .pTessellationState = null,
            .pViewportState = &viewport_state,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = null,
            .pColorBlendState = &color_blending,
            .pDynamicState = &dynamic_state,
            .layout = self.pipeline_layout,
            .renderPass = self.render_pass,
            .subpass = 0,
            .basePipelineHandle = null,
            .basePipelineIndex = -1,
        };
        try util.expectVk(c.vkCreateGraphicsPipelines(
            device,
            null,
            1,
            &pipeline_info,
            null,
            &self.handle,
        ));
    }
};

fn createShader(
    allocator: std.mem.Allocator,
    device: c.VkDevice,
    bytes: []const u8,
) !c.VkShaderModule {
    const code = try allocator.alloc(u32, bytes.len / 4);
    defer allocator.free(code);
    @memcpy(std.mem.sliceAsBytes(code), bytes);
    const create_info = c.VkShaderModuleCreateInfo{
        .sType = c.VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = null,
        .flags = 0,
        .codeSize = bytes.len,
        .pCode = code.ptr,
    };
    var shader: c.VkShaderModule = null;
    try util.expectVk(c.vkCreateShaderModule(device, &create_info, null, &shader));
    return shader;
}

fn shaderStage(
    stage: c.VkShaderStageFlagBits,
    module: c.VkShaderModule,
) c.VkPipelineShaderStageCreateInfo {
    return .{
        .sType = c.VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = null,
        .flags = 0,
        .stage = stage,
        .module = module,
        .pName = "main",
        .pSpecializationInfo = null,
    };
}

fn emptyVertexInputState() c.VkPipelineVertexInputStateCreateInfo {
    return .{
        .sType = c.VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = null,
        .flags = 0,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = null,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = null,
    };
}

fn triangleInputAssemblyState() c.VkPipelineInputAssemblyStateCreateInfo {
    return .{
        .sType = c.VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = null,
        .flags = 0,
        .topology = c.VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = c.VK_FALSE,
    };
}

fn dynamicViewportState() c.VkPipelineViewportStateCreateInfo {
    return .{
        .sType = c.VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = null,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = null,
        .scissorCount = 1,
        .pScissors = null,
    };
}

fn rasterizationState() c.VkPipelineRasterizationStateCreateInfo {
    return .{
        .sType = c.VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = null,
        .flags = 0,
        .depthClampEnable = c.VK_FALSE,
        .rasterizerDiscardEnable = c.VK_FALSE,
        .polygonMode = c.VK_POLYGON_MODE_FILL,
        .cullMode = c.VK_CULL_MODE_NONE,
        .frontFace = c.VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = c.VK_FALSE,
        .depthBiasConstantFactor = 0,
        .depthBiasClamp = 0,
        .depthBiasSlopeFactor = 0,
        .lineWidth = 1,
    };
}

fn multisampleState() c.VkPipelineMultisampleStateCreateInfo {
    return .{
        .sType = c.VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = null,
        .flags = 0,
        .rasterizationSamples = c.VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = c.VK_FALSE,
        .minSampleShading = 1,
        .pSampleMask = null,
        .alphaToCoverageEnable = c.VK_FALSE,
        .alphaToOneEnable = c.VK_FALSE,
    };
}

fn colorBlendAttachmentState() c.VkPipelineColorBlendAttachmentState {
    return .{
        .blendEnable = c.VK_FALSE,
        .srcColorBlendFactor = c.VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = c.VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = c.VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = c.VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = c.VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = c.VK_BLEND_OP_ADD,
        .colorWriteMask = c.VK_COLOR_COMPONENT_R_BIT |
            c.VK_COLOR_COMPONENT_G_BIT |
            c.VK_COLOR_COMPONENT_B_BIT |
            c.VK_COLOR_COMPONENT_A_BIT,
    };
}
