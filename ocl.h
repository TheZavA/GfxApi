#include <cassert>
#include "cl.hpp"

#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/foreach.hpp>

#include <iostream>
#include <fstream>

#include <streambuf>

#include "voxel/Chunkmanager.h"
#include "voxel/Chunk.h"
#include <algorithm>

// STUPID STUPID VS!!!!!
#pragma push_macro("max") 
#undef max

#define WORKGROUP_SIZE 256



struct ocl_t
{
    static const uint32_t GROUP_SIZE = 256;


    static const uint32_t NUM_BANKS = 16;

    uint32_t m_elementsAllocated;
    uint32_t m_levelsAllocated;

    uint32_t m_numVoxels;
    uint32_t m_numVoxels2Border;

    std::vector<cl::Device> all_devices;
    std::vector<cl::Platform> all_platforms;
    cl::Device default_device;
    
    boost::shared_ptr<cl::Program> program;
    boost::shared_ptr<cl::Context> context;

    boost::shared_ptr<cl::Buffer> m_density_buffer;
    boost::shared_ptr<cl::Buffer> m_edge_buffer;
    boost::shared_ptr<cl::Buffer> m_occupied_buffer;
    boost::shared_ptr<cl::Buffer> m_occupied_scan_buffer;

    boost::shared_ptr<cl::Buffer> m_buffer;

    std::vector<boost::shared_ptr<cl::Buffer>> m_scanPartialSums;

    cl::CommandQueue m_queue;


    ocl_t(size_t platform_index, size_t device_index)
    {

        //get all platforms (drivers)
        cl::Platform::get(&all_platforms);
        if(all_platforms.size()==0)
        {
            std::cout<<" No platforms found. Check OpenCL installation!\n";
            exit(1);
        }
        
        std::cout << "|all_platforms|: " << all_platforms.size() << std::endl;
        cl::Platform default_platform=all_platforms.at(platform_index);
        std::cout << "Using platform: " << default_platform.getInfo<CL_PLATFORM_NAME>() << "\n";
     
        //get default device of the default platform
        default_platform.getDevices(CL_DEVICE_TYPE_ALL, &all_devices);
        if(all_devices.size() == 0)
        {
            std::cout<<" No devices found. Check OpenCL installation!\n";
            exit(1);
        }
        
        std::cout << "|all_devices|: " << all_devices.size() << std::endl;
        default_device=all_devices.at(device_index);
        std::cout<< "Using device: "<<default_device.getInfo<CL_DEVICE_NAME>()<<"\n";
     
        std::vector<cl::Device> build_devices;
        build_devices.push_back(default_device);
     
        std::vector<cl::Device> devices = build_devices;
        context = boost::make_shared<cl::Context>(devices);
        
        
    }
    
    static void enumerate_platforms()
    {
        std::vector<cl::Platform> platforms;
        
        //get all platforms (drivers)
        cl::Platform::get(&platforms);
        if(platforms.size() == 0)
        {
            std::cout<<" No platforms found. Check OpenCL installation!\n";
            return;
        }
        
        std::cout << "|platforms|: " << platforms.size() << std::endl;
        BOOST_FOREACH(cl::Platform platform, platforms)
        {
            std::cout << "Platform: "<<platform.getInfo<CL_PLATFORM_NAME>()<<"\n";
        }
    }
    
    static void enumerate_devices(size_t platform_index)
    {
        std::vector<cl::Platform> platforms;
        
        //get all platforms (drivers)
        cl::Platform::get(&platforms);
        if(platforms.size() == 0)
        {
            std::cout<<" No platforms found. Check OpenCL installation!\n";
            exit(1);
        }
        
        std::cout << "|platforms|: " << platforms.size() << std::endl;
        
        cl::Platform platform=platforms.at(platform_index);
        
        std::cout << "Using platform: "<<platform.getInfo<CL_PLATFORM_NAME>()<<"\n";
        
        std::vector<cl::Device> devices;

        //get default device of the default platform
        platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);
        if(devices.size() == 0)
        {
            std::cout<<" No devices found. Check OpenCL installation!\n";
            return;
        }
        
        std::cout << "|devices|: " << devices.size() << std::endl;
        
        BOOST_FOREACH(cl::Device device, devices)
        {
            std::cout<< "Device: " << device.getInfo<CL_DEVICE_NAME>() << "\n";
        }
    }


    
    void load_cl_source(std::vector<std::string> source_paths)
    {
        
        m_numVoxels = ChunkManager::CHUNK_SIZE * ChunkManager::CHUNK_SIZE * ChunkManager::CHUNK_SIZE;
        m_numVoxels2Border = (ChunkManager::CHUNK_SIZE + 4) * (ChunkManager::CHUNK_SIZE + 4) * (ChunkManager::CHUNK_SIZE + 4);
        
        cl::Program::Sources sources;

        std::string str;

        std::vector<std::string> source_container;

        for(auto source_path : source_paths)

        {
            std::ifstream t(source_path);
            std::string str1 = std::string((std::istreambuf_iterator<char>(t)),
                               std::istreambuf_iterator<char>());

            source_container.push_back(str1);

            sources.push_back(std::make_pair(source_container.back().c_str(), source_container.back().length()));
        }
     
        std::vector<cl::Device> build_devices;
        build_devices.push_back(default_device);

        std::cout<< "building program" << std::endl << std::flush;
        program = boost::make_shared<cl::Program>(*context, sources);
        if(program->build(build_devices)!=CL_SUCCESS)
        {
            std::cout << " Error building: " << program->getBuildInfo<CL_PROGRAM_BUILD_LOG>(default_device) << "\n";
            exit(1);
        }
    
        std::cout<< "program ready" << std::endl << std::flush;

        m_queue = cl::CommandQueue(*context, default_device);

        m_edge_buffer = boost::make_shared<cl::Buffer>(*context, CL_MEM_READ_WRITE, m_numVoxels2Border*sizeof(cl_edge_info_t));
        m_occupied_buffer = boost::make_shared<cl::Buffer>(*context, CL_MEM_READ_WRITE, m_numVoxels2Border*sizeof(uint32_t));
        m_occupied_scan_buffer = boost::make_shared<cl::Buffer>(*context, CL_MEM_READ_WRITE, m_numVoxels2Border*sizeof(uint32_t));
        m_density_buffer = boost::make_shared<cl::Buffer>(*context, CL_MEM_READ_WRITE, m_numVoxels2Border*sizeof(float));


        m_queue.enqueueWriteBuffer(*m_edge_buffer, CL_TRUE, 0, m_numVoxels2Border*sizeof(uint32_t), nullptr);
        m_queue.enqueueWriteBuffer(*m_occupied_buffer, CL_TRUE, 0, m_numVoxels2Border*sizeof(uint32_t), nullptr);
        m_queue.enqueueWriteBuffer(*m_occupied_scan_buffer, CL_TRUE, 0, m_numVoxels2Border*sizeof(uint32_t), nullptr);
        m_queue.enqueueWriteBuffer(*m_density_buffer, CL_TRUE, 0, m_numVoxels2Border*sizeof(float), nullptr);
 
    }

    typedef uint32_t word_t;

    void density3d(std::size_t rn, float worldx, float worldy, float worldz, float res)
    {
        std::size_t units = rn;

        cl_float3_t pos; 
        pos.x = worldx;
        pos.y = worldy;
        pos.z = worldz;

        cl::Kernel density_kernel(*program, "test");
        density_kernel.setArg(0, *m_density_buffer);
        density_kernel.setArg(1, pos);
        density_kernel.setArg(2, res);

        m_queue.enqueueNDRangeKernel(density_kernel, cl::NullRange, cl::NDRange(units, units, units));

    }


    void classifyEdges(cl_edge_info_t* r, std::size_t rn, float worldx, float worldy, float worldz, float res, std::vector<cl_edge_info_t>& compactedEdges)
    {
        std::size_t units = rn;

        cl_float3_t pos; 
        pos.x = worldx;
        pos.y = worldy;
        pos.z = worldz;

        cl::Kernel classify_kernel(*program, "classifyEdges");
        classify_kernel.setArg(0, *m_density_buffer);
        classify_kernel.setArg(1, *m_edge_buffer);
        classify_kernel.setArg(2, *m_occupied_buffer);
        classify_kernel.setArg(3, pos);
        classify_kernel.setArg(4, res);

        m_queue.enqueueNDRangeKernel(classify_kernel, cl::NullRange, cl::NDRange(units, units, units));

        //read result C from the device to array C
        uint32_t count = rn*rn*rn;
        CreatePartialSumBuffers(count);

        PreScanBufferRecursive(*m_occupied_scan_buffer, *m_occupied_buffer, GROUP_SIZE, GROUP_SIZE, count, 0);

        int result = 0;
        m_queue.enqueueReadBuffer(*m_occupied_scan_buffer, CL_TRUE, (count - 1)*sizeof(uint32_t), sizeof(uint32_t), &result);

        m_levelsAllocated = 0;
        m_scanPartialSums.clear();
        m_elementsAllocated = 0;

        if(result > 0)
        {
            auto compact_buffer = boost::make_shared<cl::Buffer>(*context, CL_MEM_READ_WRITE, result*sizeof(cl_edge_info_t));
            cl::Kernel compact_kernel(*program, "compactEdges");
            compact_kernel.setArg(0, *m_occupied_buffer);
            compact_kernel.setArg(1, *m_occupied_scan_buffer);
            compact_kernel.setArg(2, *compact_buffer);
            compact_kernel.setArg(3, *m_edge_buffer);
            compact_kernel.setArg(4, count);

            m_queue.enqueueNDRangeKernel(compact_kernel, cl::NullRange, cl::NDRange(count));
            compactedEdges.resize(result);
            
            m_queue.enqueueReadBuffer(*compact_buffer, CL_TRUE, 0, result * sizeof(cl_edge_info_t), &compactedEdges[0]);

        }

    }

    void getCompactCorners(cl_int4_t* r, std::size_t rn)
    {
        std::size_t units = rn;

        auto tempBuffer = boost::make_shared<cl::Buffer>(*context, CL_MEM_READ_WRITE, rn*sizeof(uint32_t));

        m_queue.enqueueWriteBuffer(*tempBuffer, CL_TRUE, 0, rn*sizeof(cl_int4_t), r);
       
        cl::Kernel classify_kernel(*program, "computeCorners");
        classify_kernel.setArg(0, *m_density_buffer);
        classify_kernel.setArg(1, *tempBuffer);
        classify_kernel.setArg(2, (int)ChunkManager::CHUNK_SIZE + 2);

        m_queue.enqueueNDRangeKernel(classify_kernel, cl::NullRange, cl::NDRange(units));

        int result = 0;
        m_queue.enqueueReadBuffer(*tempBuffer, CL_TRUE, 0, rn*sizeof(cl_int4_t), r);
        
    }
    

    void generateZeroCrossing(std::vector<edge_t>& r, float worldx, float worldy, float worldz, float res)
    {
        std::size_t unit_size = 1;
        std::size_t units = r.size();

        // create buffers on the device
        cl::Buffer ocl_r(*context, CL_MEM_READ_WRITE, r.size()*sizeof(edge_t));
     
        //create queue to which we will push commands for the device.
        cl::CommandQueue queue(*context, default_device);
     
        queue.enqueueWriteBuffer(ocl_r, CL_TRUE, 0, r.size()*sizeof(edge_t), &r[0]);

        //run the kernel
        cl::make_kernel<cl::Buffer&, cl_float3_t, float> genZeroCross(cl::Kernel(*program, "genZeroCross"));
        
        cl::EnqueueArgs eargs(queue, cl::NDRange(units));

        cl_float3_t pos; 
        pos.x = worldx;
        pos.y = worldy;
        pos.z = worldz;

        genZeroCross(eargs, ocl_r, pos, res);

        //read result C from the device to array C
        queue.enqueueReadBuffer(ocl_r, CL_TRUE, 0, r.size()*sizeof(edge_t), &r[0]);
        
    }


    bool IsPowerOfTwo(int n)
    {
        return ((n&(n-1))==0) ;
    }

    int floorPow2(int n)
    {
        int exp;
        frexp((int)n, &exp);
        return 1 << (exp - 1);
    }

    int CreatePartialSumBuffers(unsigned int count)
    {
        m_elementsAllocated = count;

        unsigned int group_size = GROUP_SIZE;
        unsigned int element_count = count;

        int level = 0;

        do
        {       
            unsigned int group_count = (int)std::max(1, (int)ceil((int)element_count / (2.0f * group_size)));
            if (group_count > 1)
            {
                level++;
            }
            element_count = group_count;
        
        } while (element_count > 1);

        //ScanPartialSums = (cl_mem*) malloc(level * sizeof(cl_mem));
        m_levelsAllocated = level;
        //memset(ScanPartialSums, 0, sizeof(cl_mem) * level);
    
        element_count = count;
        level = 0;
    
        do
        {       
            unsigned int group_count = (int)std::max(1, (int)ceil((int)element_count / (2.0f * group_size)));
            if (group_count > 1) 
            {
                size_t buffer_size = group_count * sizeof(int);
                auto level_buffer = boost::make_shared<cl::Buffer>(*context, CL_MEM_READ_WRITE, buffer_size);
                m_scanPartialSums.push_back(level_buffer);
                //ScanPartialSums[level++] = clCreateBuffer(ComputeContext, CL_MEM_READ_WRITE, buffer_size, NULL, NULL);
            }

            element_count = group_count;

        } while (element_count > 1);

        return CL_SUCCESS;
    }

    int PreScanStoreSum(
        size_t *global, 
        size_t *local, 
        size_t shared, 
        cl::Buffer& output_data, 
        cl::Buffer& input_data, 
        cl::Buffer& partial_sums,
        unsigned int n,
        int group_index, 
        int base_index)
    {

        cl::Kernel classify_kernel(*program, "PreScanStoreSumKernel");
        classify_kernel.setArg(0, output_data);
        classify_kernel.setArg(1, input_data);
        classify_kernel.setArg(2, partial_sums);
        classify_kernel.setArg(3, shared, 0); // TODO maybe 0
        classify_kernel.setArg(4, group_index);
        classify_kernel.setArg(5, base_index);
        classify_kernel.setArg(6, n);

        m_queue.enqueueNDRangeKernel(classify_kernel, cl::NullRange, cl::NDRange(global[0]), cl::NDRange(local[0]));

        return CL_SUCCESS;
    }

    int PreScanStoreSumNonPowerOfTwo(
        size_t *global, 
        size_t *local, 
        size_t shared, 
        cl::Buffer& output_data, 
        cl::Buffer& input_data, 
        cl::Buffer& partial_sums,
        unsigned int n,
        int group_index, 
        int base_index)
    {
        cl::Kernel classify_kernel(*program, "PreScanStoreSumNonPowerOfTwoKernel");
        classify_kernel.setArg(0, output_data);
        classify_kernel.setArg(1, input_data);
        classify_kernel.setArg(2, partial_sums);
        classify_kernel.setArg(3, shared, 0); // TODO maybe 0
        classify_kernel.setArg(4, group_index);
        classify_kernel.setArg(5, base_index);
        classify_kernel.setArg(6, n);

        m_queue.enqueueNDRangeKernel(classify_kernel, cl::NullRange, cl::NDRange(global[0]), cl::NDRange(local[0]));

        return CL_SUCCESS;
    }

    int UniformAdd(
        size_t *global, 
        size_t *local, 
        cl::Buffer& output_data, 
        cl::Buffer& partial_sums, 
        unsigned int n, 
        unsigned int group_offset, 
        unsigned int base_index)
    {

        cl::Kernel classify_kernel(*program, "UniformAddKernel");
        classify_kernel.setArg(0, output_data);
        classify_kernel.setArg(1, partial_sums);
        classify_kernel.setArg(2, sizeof(int), 0);
        classify_kernel.setArg(3, group_offset); // TODO maybe 0
        classify_kernel.setArg(4, base_index);
        classify_kernel.setArg(5, n);

        m_queue.enqueueNDRangeKernel(classify_kernel, cl::NullRange, cl::NDRange(global[0]), cl::NDRange(local[0]));

        return CL_SUCCESS;
    }

    int PreScan(
        size_t *global, 
        size_t *local, 
        size_t shared, 
        cl::Buffer& output_data, 
        cl::Buffer& input_data, 
        unsigned int n,
        int group_index, 
        int base_index)
    {
        cl::Kernel classify_kernel(*program, "PreScanKernel");
        classify_kernel.setArg(0, output_data);
        classify_kernel.setArg(1, input_data);
        classify_kernel.setArg(2, shared, 0);
        classify_kernel.setArg(3, group_index); // TODO maybe 0
        classify_kernel.setArg(4, base_index);
        classify_kernel.setArg(5, n);

        m_queue.enqueueNDRangeKernel(classify_kernel, cl::NullRange, cl::NDRange(global[0]), cl::NDRange(local[0]));

        return CL_SUCCESS;
    }

    int PreScanNonPowerOfTwo(
        size_t *global, 
        size_t *local, 
        size_t shared, 
        cl::Buffer& output_data, 
        cl::Buffer& input_data, 
        unsigned int n,
        int group_index, 
        int base_index)
    {
        cl::Kernel classify_kernel(*program, "PreScanNonPowerOfTwoKernel");

        classify_kernel.setArg(0, output_data);
        classify_kernel.setArg(1, input_data);
        classify_kernel.setArg(2, shared, 0);
        classify_kernel.setArg(3, group_index); // TODO maybe 0
        classify_kernel.setArg(4, base_index);
        classify_kernel.setArg(5, n);

        m_queue.enqueueNDRangeKernel(classify_kernel, cl::NullRange, cl::NDRange(global[0]), cl::NDRange(local[0]));

        return CL_SUCCESS;
    }

    int PreScanBufferRecursive(cl::Buffer& output_data, cl::Buffer& input_data, int max_group_size, int max_work_item_count, int element_count, int level)
    {
        unsigned int group_size = max_group_size; 
        unsigned int group_count = (int)std::max(1, (int)(int)ceil((int)element_count / (2.0f * group_size)));
        unsigned int work_item_count = 0;

        if (group_count > 1)
            work_item_count = group_size;
        else if (IsPowerOfTwo(element_count))
            work_item_count = element_count / 2;
        else
            work_item_count = floorPow2(element_count);
        
        work_item_count = (work_item_count > max_work_item_count) ? max_work_item_count : work_item_count;

        unsigned int element_count_per_group = work_item_count * 2;
        unsigned int last_group_element_count = element_count - (group_count-1) * element_count_per_group;
        unsigned int remaining_work_item_count = (int)std::max(1, (int)(last_group_element_count / 2));
        remaining_work_item_count = (remaining_work_item_count > max_work_item_count) ? max_work_item_count : remaining_work_item_count;
        unsigned int remainder = 0;
        size_t last_shared = 0;

    
        if (last_group_element_count != element_count_per_group)
        {
            remainder = 1;

            if(!IsPowerOfTwo(last_group_element_count))
                remaining_work_item_count = floorPow2(last_group_element_count);    
        
            remaining_work_item_count = (remaining_work_item_count > max_work_item_count) ? max_work_item_count : remaining_work_item_count;
            unsigned int padding = (2 * remaining_work_item_count) / NUM_BANKS;
            last_shared = sizeof(int) * (2 * remaining_work_item_count + padding);
        }

        remaining_work_item_count = (remaining_work_item_count > max_work_item_count) ? max_work_item_count : remaining_work_item_count;
        size_t global[] = { (int)std::max(1, (int)(group_count - (int)remainder)) * work_item_count, 1 };
        size_t local[]  = { work_item_count, 1 };  

        unsigned int padding = element_count_per_group / NUM_BANKS;
        size_t shared = sizeof(int) * (element_count_per_group + padding);
    
        
        int err = CL_SUCCESS;
    
        if (group_count > 1)
        {
            cl::Buffer& partial_sums = *m_scanPartialSums[level];
            err = PreScanStoreSum(global, local, shared, output_data, input_data, partial_sums, work_item_count * 2, 0, 0);
            if(err != CL_SUCCESS)
                return err;
            
            if (remainder)
            {
                size_t last_global[] = { 1 * remaining_work_item_count, 1 };
                size_t last_local[]  = { remaining_work_item_count, 1 };  

                err = PreScanStoreSumNonPowerOfTwo(last_global, last_local, last_shared,
                                                   output_data, input_data, partial_sums,
                                                   last_group_element_count, group_count - 1, 
                                                   element_count - last_group_element_count);    
        
                if(err != CL_SUCCESS)
                    return err;			
			
            }

            err = PreScanBufferRecursive(partial_sums, partial_sums, max_group_size, max_work_item_count, group_count, level + 1);
            if(err != CL_SUCCESS)
                return err;
            
            err = UniformAdd(global, local, output_data, partial_sums,  element_count - last_group_element_count, 0, 0);
            if(err != CL_SUCCESS)
                return err;
        
            if (remainder)
            {
                size_t last_global[] = { 1 * remaining_work_item_count, 1 };
                size_t last_local[]  = { remaining_work_item_count, 1 };  

                err = UniformAdd(last_global, last_local, output_data, partial_sums, last_group_element_count, group_count - 1, element_count - last_group_element_count);
                
                if(err != CL_SUCCESS)
                    return err;
            }
        }
        else if (IsPowerOfTwo(element_count))
        {
            err = PreScan(global, local, shared, output_data, input_data, work_item_count * 2, 0, 0);
            if(err != CL_SUCCESS)
                return err;
        }
        else
        {
            err = PreScanNonPowerOfTwo(global, local, shared, output_data, input_data, element_count, 0, 0);
            if(err != CL_SUCCESS)
                return err;
        }

        return CL_SUCCESS;
    }

};