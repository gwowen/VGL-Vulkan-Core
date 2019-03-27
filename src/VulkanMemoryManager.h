/*********************************************************************
Copyright 2018 VERTO STUDIO LLC.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
***************************************************************************/

#pragma once

#include <vector>
#include <map>
#include <list>
#include <mutex>
#include "vulkan.h"

namespace vgl
{
  namespace core
  {
    ///This class will get more advanced as time passes... 
    class VulkanMemoryManager
    {
    public:
      enum AllocationStrategy
      {
        AS_MOBILE_CONVERVATIVE,
        AS_DESKTOP
      };

      enum FreeMode
      {
        FM_MANUAL,
        FM_SYNCHRONOUS
      };

      VulkanMemoryManager(VkPhysicalDevice physicalDevice, VkDevice device);
      ~VulkanMemoryManager();

    protected:
      enum AllocationType
      {
        AT_SMALL = 0, AT_MED, AT_LARGE, AT_DEDICATED, AT_UNKNOWN
      };

      struct Subregion
      {
        uint32_t startPage, size;
        bool free;
        uint64_t id;

        //Subregion *next = nullptr, *prev = nullptr;
        //static Subregion *create();
        //void destroy();
      };

      struct FreeRegionComparator 
      {
        bool operator()(const Subregion *a, const Subregion *b) const;
      };

      struct Allocation
      {
        VkDeviceMemory memory;
        uint32_t size; //in pages
        uint32_t suballocationCount;
        uint32_t memoryType;
        uint64_t id;
        AllocationType type;
        bool imageOptimal; //for now, the easiest way to deal with bufferImageGranularity & aliasing
        std::list<Subregion> regions;
        std::map<Subregion *, std::list<Subregion>::iterator, FreeRegionComparator> freeRegions;
      };

    public:
      static const uint64_t Any = 0;

      ///Allocation & Freeing of suballocations /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      struct Suballocation
      {
      public:
        VkDeviceMemory memory;
        VkDeviceSize offset;

        uint32_t size; //size in pages
        uint32_t memoryType;
        uint64_t allocationId, subregionId;
        std::list<Subregion>::iterator subregionIt;
        
        Suballocation() = default;
        Suballocation(nullptr_t null) : memory(VK_NULL_HANDLE) {}
        void operator =(nullptr_t null) { memory = VK_NULL_HANDLE; }
        operator bool() const { return (memory != nullptr); }
        bool operator !() const { return (memory == nullptr); }
      };

      Suballocation allocate(uint32_t typeFilter, VkMemoryPropertyFlags properties, VkDeviceSize requiredSize, VkDeviceSize requiredAlignment=0, bool imageOptimal=false, uint64_t allocationId=Any);
      //Suballocation allocateDedicated(uint32_t typeFilter, VkMemoryPropertyFlags properties, VkDeviceSize requiredSize, VkDeviceSize requiredAlignment=0, bool imageOptimal=false);
      void free(const Suballocation &suballocation);

      ///If free mode is set to AT_MANUAL, you'll need to call this before any actual Vulkan allocations are freed.  
      ///Use caution when calling during event loop however, this can be a very expensive operation.
      void reclaimMemory();

      ///When using this function, the caller is entirely responsible for the returned memory object
      VkDeviceMemory allocateDirect(uint32_t memoryType, VkDeviceSize requiredSize, bool imageOptimal=false);   
      std::pair<VkDeviceMemory, uint64_t> allocateDedicated(uint32_t typeFilter, VkMemoryPropertyFlags properties, VkDeviceSize requiredSize, bool allowSuballocation=true, bool imageOptimal=false);

      ///Utilities to determine what kind of memory properties a suballocation has //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

      ///If an allocation is coherent, it is not necessary to manually flush for CPU-made changes to reflect on the GPU
      bool isAllocationCoherent(const Suballocation &suballocation);
      bool isAllocationCoherent(uint64_t allocationId);
      bool isAllocationHostCached(const Suballocation &suballocation);
      bool isAllocationHostCached(uint64_t allocationId);
      bool isAllocationHostVisible(const Suballocation &suballocation);
      bool isAllocationHostVisible(uint64_t allocationId);
      bool isAllocationDeviceLocal(const Suballocation &suballocation);
      bool isAllocationDeviceLocal(uint64_t allocationId);

      ///Returns the size in bytes of a given suballocation
      VkDeviceSize getAllocationSize(const Suballocation &suballocation);

    protected:
      static const int pageSize = 4096;

      static AllocationStrategy allocationStrategy;
      static FreeMode freeMode;
      static VkDeviceSize allocSizeBoundaries[2][3];

      //calls to all public methods will be synchronized across threads
      std::mutex managerLock;

      uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
      VkDeviceMemory makeNewAllocation(uint32_t memoryType, AllocationType type, bool imageOptimal);
      Suballocation findSuballocation(uint32_t memoryType, VkDeviceSize requiredSize, VkDeviceSize requiredAlignment, AllocationType type, bool imageOptimal, uint64_t allocationId);
      void cleanupAllocations();

      std::pair<VkDeviceMemory, uint64_t> allocateDedicated(uint32_t memoryType, VkDeviceSize requiredSize, bool allowSuballocation = true, bool imageOptimal = false);

      VkPhysicalDevice physicalDevice;
      VkPhysicalDeviceMemoryProperties memoryProperties;

      std::list<Subregion>::iterator divideSubregion(Allocation &allocation, std::list<Subregion>::iterator region, uint32_t sizeInPages);
      void mergeSubregions(Allocation &allocation, std::list<Subregion>::iterator region1, std::list<Subregion>::iterator region2);

      Allocation *allocationForId(uint64_t allocationId);

      VkDevice device;
      Allocation *allocationsPool;
      std::vector<Allocation *> allocations[VK_MAX_MEMORY_TYPES];
      std::map<uint64_t, uint64_t> allocationsMap[VK_MAX_MEMORY_TYPES];
      uint64_t lowMemoryFlags = 0;
      size_t allocatedBytes = 0; //across all heaps
      //std::vector<Subregion> subregionPools[VK_MAX_MEMORY_TYPES];
      uint64_t allocationIds = 1, subregionIds = 1, ai = 0, invalidatedSubregionIds = 0;
    };
  }
}