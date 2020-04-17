#pragma once
#include "common.hpp"
#include "instance.hpp"
#include "memory.hpp"

class Descriptors{
public:

    void init(Instance *instance){
        this->instance = instance;
        create_texture_sampler();
        create_uniform_buffers();
        create_descriptor_set_layout();
        create_descriptor_pool();
    }

    void destroy()
    {
        vkDestroyDescriptorPool(instance->device, descriptor_pool, nullptr);
        vkDestroyDescriptorSetLayout(instance->device, descriptor_set_layout, nullptr);
        
        for(int i = 0; i < uniform_buffers.size(); i++) uniform_buffers[i].destroy();
        vkDestroySampler(instance->device, texture_sampler, nullptr);
    }

    VkSampler texture_sampler;
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    std::vector<Buffer> uniform_buffers;
    std::vector<Buffer> dynamic_uniform_buffers;
    std::vector<VkDescriptorSet> descriptor_sets;

    void bind_diffuse_image(Image *image){ diffuse = image; }
    void bind_enviroment_image(Image *image){ enviroment = image; }
    // specular +
    // roughness +

    void create_descriptor_sets(){
        std::vector<VkDescriptorSetLayout> layouts(instance->surface.image_count, descriptor_set_layout);

        VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        allocInfo.descriptorPool = this->descriptor_pool;
        allocInfo.descriptorSetCount = (uint32_t)instance->surface.image_count;
        allocInfo.pSetLayouts = layouts.data();

        descriptor_sets.resize(instance->surface.image_count);
        if (vkAllocateDescriptorSets(instance->device, &allocInfo, descriptor_sets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < instance->surface.image_count; i++) {

            VkDescriptorBufferInfo bufferInfo = {};
            bufferInfo.buffer = uniform_buffers[i].buffer;
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);
            
            VkDescriptorImageInfo imageInfo = {};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = diffuse->image_view; 
            imageInfo.sampler = texture_sampler;

            VkDescriptorImageInfo enviromentInfo = {};
            enviromentInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            enviromentInfo.imageView = enviroment->image_view; 
            enviromentInfo.sampler = texture_sampler;

            VkDescriptorBufferInfo materialInfo = {};
            materialInfo.buffer = dynamic_uniform_buffers[i].buffer;
            materialInfo.offset = 0;
            materialInfo.range = sizeof(Material);
            
            std::array<VkWriteDescriptorSet, 4> descriptorWrites = {};

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = descriptor_sets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &bufferInfo;
            
            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = descriptor_sets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfo;
            
            descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[2].dstSet = descriptor_sets[i];
            descriptorWrites[2].dstBinding = 2;
            descriptorWrites[2].dstArrayElement = 0;
            descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pImageInfo = &enviromentInfo;

            descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[3].dstSet = descriptor_sets[i];
            descriptorWrites[3].dstBinding = 3;
            descriptorWrites[3].dstArrayElement = 0;
            descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC; // !dyn
            descriptorWrites[3].descriptorCount = 1;
            descriptorWrites[3].pBufferInfo = &materialInfo;
            
            vkUpdateDescriptorSets(instance->device, (uint32_t)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
        }
        printf("Descriptor sets ready \n");
    }
    
private:

    Image *enviroment;
    //-- PBR textures --
    Image *diffuse; // color
    Image *normal;  // direction
    Image *orm;
    /*
    Image *occlusion; // R
    Image *rougness;  // G
    Image *metalic;   // B
    */
    //------------------
    
    Instance *instance;

    void create_texture_sampler()
    {
        VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        samplerInfo.magFilter = VK_FILTER_LINEAR; // or VK_FILTER_NEAREST
        samplerInfo.minFilter = VK_FILTER_LINEAR; // or VK_FILTER_NEAREST

        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 16;

        samplerInfo.unnormalizedCoordinates = VK_FALSE; // use normalized [0, 1]

        // https://developer.nvidia.com/gpugems/gpugems/part-ii-lighting-and-shadows/chapter-11-shadow-map-antialiasing
        samplerInfo.compareEnable = VK_FALSE; // for shadow mapping
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        if (vkCreateSampler(instance->device, &samplerInfo, nullptr, &texture_sampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }

    void create_uniform_buffers(){

        VkDeviceSize size;
        
        size = sizeof(UniformBufferObject);
        uniform_buffers.resize(instance->surface.image_count);

        for (size_t i = 0; i < instance->surface.image_count; i++) 
        {
            uniform_buffers[i].init(this->instance);
            uniform_buffers[i].create_buffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        }

        size = sizeof(Material);
        dynamic_uniform_buffers.resize(instance->surface.image_count);

        for (size_t i = 0; i < instance->surface.image_count; i++) 
        {
            dynamic_uniform_buffers[i].init(this->instance);
            dynamic_uniform_buffers[i].create_buffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        }

        printf("created uniform buffers \n");
    }

    void create_descriptor_set_layout()
    {
        // define:
        //  descriptor type
        //  where to use (shader stage)
        //  tell binding for shader program

        VkDescriptorSetLayoutBinding uboLayoutBinding = {};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        
        VkDescriptorSetLayoutBinding skyboxLayoutBinding = {};
        skyboxLayoutBinding.binding = 2;
        skyboxLayoutBinding.descriptorCount = 1;
        skyboxLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        skyboxLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        
        VkDescriptorSetLayoutBinding dynamicMaterial = {};
        dynamicMaterial.binding = 3;
        dynamicMaterial.descriptorCount = 1;
        dynamicMaterial.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC; // !dyn
        dynamicMaterial.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
       
        
        std::array<VkDescriptorSetLayoutBinding, 4> bindings = {
            uboLayoutBinding, 
            samplerLayoutBinding, 
            skyboxLayoutBinding,
            dynamicMaterial,
        };

        VkDescriptorSetLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        createInfo.bindingCount = (uint32_t)bindings.size();
        createInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(this->instance->device, &createInfo, nullptr, &descriptor_set_layout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
         printf("Created descriptor set layouts \n");
    }

    void create_descriptor_pool()
    {
        std::array<VkDescriptorPoolSize, 4> pool_sizes = {};
        pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_sizes[0].descriptorCount = (uint32_t)instance->surface.image_count;
        pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_sizes[1].descriptorCount = (uint32_t)instance->surface.image_count;
        pool_sizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_sizes[2].descriptorCount = (uint32_t)instance->surface.image_count;
        pool_sizes[3].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC; // !dyn
        pool_sizes[3].descriptorCount = (uint32_t)instance->surface.image_count;

        VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        poolInfo.poolSizeCount = (uint32_t)pool_sizes.size();
        poolInfo.pPoolSizes = pool_sizes.data();
        poolInfo.maxSets = (uint32_t)instance->surface.image_count;

        if (vkCreateDescriptorPool(instance->device, &poolInfo, nullptr, &this->descriptor_pool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }

        printf("Created descriptor pool \n");
    }
};