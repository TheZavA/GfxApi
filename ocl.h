#include <cassert>
#include "cl.hpp"

#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/foreach.hpp>

#include <iostream>
#include <fstream>


struct ocl_t
{
    std::vector<cl::Device> all_devices;
    std::vector<cl::Platform> all_platforms;
    cl::Device default_device;
    
    boost::shared_ptr<cl::Program> program;
    boost::shared_ptr<cl::Context> context;

        ocl_t(size_t platform_index, size_t device_index)
    {
        

        //get all platforms (drivers)
        cl::Platform::get(&all_platforms);
        if(all_platforms.size()==0){
            std::cout<<" No platforms found. Check OpenCL installation!\n";
            exit(1);
        }
        
        std::cout << "|all_platforms|: " << all_platforms.size() << std::endl;
        cl::Platform default_platform=all_platforms.at(platform_index);
        std::cout << "Using platform: "<<default_platform.getInfo<CL_PLATFORM_NAME>()<<"\n";
     
        //get default device of the default platform
        default_platform.getDevices(CL_DEVICE_TYPE_ALL, &all_devices);
        if(all_devices.size()==0){
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
        if(platforms.size()==0){
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
        if(platforms.size()==0){
            std::cout<<" No platforms found. Check OpenCL installation!\n";
            exit(1);
        }
        
        std::cout << "|platforms|: " << platforms.size() << std::endl;
        
        cl::Platform platform=platforms.at(platform_index);
        
        std::cout << "Using platform: "<<platform.getInfo<CL_PLATFORM_NAME>()<<"\n";
        
        std::vector<cl::Device> devices;

        //get default device of the default platform
        platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);
        if(devices.size()==0){
            std::cout<<" No devices found. Check OpenCL installation!\n";
            return;
        }
        
        std::cout << "|devices|: " << devices.size() << std::endl;
        
        BOOST_FOREACH(cl::Device device, devices)
        {
            std::cout<< "Device: "<<device.getInfo<CL_DEVICE_NAME>()<<"\n";
        }
    }
    
    void load_cl_source(const std::string& source_path)
    {
        
        std::string kernel_code;
        std::ifstream in(source_path);
        
        in.seekg(0, std::ios::end);   
        kernel_code.reserve(in.tellg());
        in.seekg(0, std::ios::beg);

        kernel_code.assign((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());

        cl::Program::Sources sources;
     
        sources.push_back(std::make_pair(kernel_code.c_str(),kernel_code.length()));
     
        std::vector<cl::Device> build_devices;
        build_devices.push_back(default_device);

        std::cout<< "building program" << std::endl << std::flush;
        program = boost::make_shared<cl::Program>(*context,sources);
        if(program->build(build_devices)!=CL_SUCCESS)
        {
            std::cout<<" Error building: "<<program->getBuildInfo<CL_PROGRAM_BUILD_LOG>(default_device)<<"\n";
            exit(1);
        }
    
        std::cout<< "program ready" << std::endl << std::flush;
    }

    typedef uint32_t word_t;

    void test(word_t* r, std::size_t rn)
    {
        //this is hardcoded in test, but you can ofc change this,
        // either making it dynamic in test, or rehardcoding it to something else
        std::size_t unit_size = 16;
        std::size_t units = rn / unit_size;
        assert(units*unit_size == rn);//rn must be divisible by unit_size
        
     
        // create buffers on the device
        cl::Buffer ocl_r(*context,CL_MEM_READ_WRITE,rn*sizeof(word_t));
     
        
     
        //create queue to which we will push commands for the device.
        cl::CommandQueue queue(*context,default_device);
     
        //write arrays A and B to the device
        queue.enqueueWriteBuffer(ocl_r/*buffer*/,CL_TRUE/*blocking*/, 0/*offset*/,rn*sizeof(word_t)/*size*/,r/*cpu ptr to copy*/);

     
        
        //run the kernel
        cl::make_kernel<cl::Buffer&> test(cl::Kernel(*program,"test"));
        
        cl::EnqueueArgs eargs(queue, cl::NDRange(units));
        //cl::KernelFunctor test(cl::Kernel(*program,"test"),
        //                                     queue,cl::NullRange,cl::NDRange(units),cl::NullRange);
        
      //  assert(rn==16);
        test(eargs, ocl_r /*other var/buffers go here*/);

        //read result C from the device to array C
        queue.enqueueReadBuffer(ocl_r,CL_TRUE,0,rn*sizeof(word_t), r);
        
    }

    void test2(float* r, std::size_t rn, float worldx, float worldy, float worldz, float res)
    {
        //this is hardcoded in test, but you can ofc change this,
        // either making it dynamic in test, or rehardcoding it to something else
        std::size_t unit_size = 1;
        std::size_t units = rn;
        
        
     
        // create buffers on the device
        cl::Buffer ocl_r(*context,CL_MEM_READ_WRITE,rn*rn*rn*sizeof(float));
     
        
     
        //create queue to which we will push commands for the device.
        cl::CommandQueue queue(*context,default_device);
     
        //write arrays A and B to the device
        queue.enqueueWriteBuffer(ocl_r/*buffer*/,CL_TRUE/*blocking*/, 0/*offset*/,rn*rn*rn*sizeof(float)/*size*/,r/*cpu ptr to copy*/);

     
        
        //run the kernel
        cl::make_kernel<cl::Buffer&, float, float, float, float> test(cl::Kernel(*program,"test"));
        
        cl::EnqueueArgs eargs(queue, cl::NDRange(units, units, units));
        //cl::KernelFunctor test(cl::Kernel(*program,"test"),
        //                                     queue,cl::NullRange,cl::NDRange(units),cl::NullRange);
        
      //  assert(rn==16);
        test(eargs, ocl_r /*other var/buffers go here*/, worldx, worldy, worldz, res);

        //read result C from the device to array C
        queue.enqueueReadBuffer(ocl_r,CL_TRUE,0,rn*rn*rn*sizeof(float), r);
        
    }

    void surfaceNormal(float* r, std::size_t rn, float worldx, float worldy, float worldz, float res)
    {
        //this is hardcoded in test, but you can ofc change this,
        // either making it dynamic in test, or rehardcoding it to something else
        std::size_t unit_size = 1;
        std::size_t units = rn;

        // create buffers on the device
        cl::Buffer ocl_r(*context,CL_MEM_READ_WRITE,rn*rn*rn*sizeof(float));

        //create queue to which we will push commands for the device.
        cl::CommandQueue queue(*context,default_device);
     
        //write arrays A and B to the device
        queue.enqueueWriteBuffer(ocl_r/*buffer*/,CL_TRUE/*blocking*/, 0/*offset*/,rn*rn*rn*sizeof(float)/*size*/,r/*cpu ptr to copy*/);

        //run the kernel
        cl::make_kernel<cl::Buffer&, float, float, float, float> surfaceNormal(cl::Kernel(*program,"surfaceNormal"));
        
        cl::EnqueueArgs eargs(queue, cl::NDRange(units, units, units));

        surfaceNormal(eargs, ocl_r, worldx, worldy, worldz, res);

        //read result C from the device to array C
        queue.enqueueReadBuffer(ocl_r,CL_TRUE,0,rn*rn*rn*sizeof(float), r);
        
    }
};