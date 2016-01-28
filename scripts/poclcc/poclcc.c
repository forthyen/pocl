#include <stdio.h>
#include <stdlib.h>
#include <CL/opencl.h>
#include <assert.h>
#include <string.h>

char *kernelSource = NULL;
char *outputFile = NULL;
int openclDevice = CL_DEVICE_TYPE_DEFAULT;


int print_help(){
    printf("USAGE: poclcc [OPTION]... [FILE]\n");
    printf("\n");
    printf("OPTIONS:\n");
    printf("\t-d <device_type>\n");
    printf("\t\tSelect <device_type> as the device_type for clGetDeviceIDs. Default: CL_DEVICE_TYPE_DEFAULT\n");
    printf("\t-o <file>\n");
    printf("\t\tWrite output to <file>\n");
    return -1;
}

int poclcc_error(char *msg){
    printf("ERROR: %s", msg);
    return print_help();
}

char *kernel_load_file(const char * filename){
    char *buffer;
    size_t size, read_size;
    FILE *kern_file;

    kern_file = fopen(filename, "r");
    if (kern_file == NULL) {
        return NULL;
    }

    fseek(kern_file, 0, SEEK_END);
    size = ftell(kern_file);
    rewind(kern_file);

    buffer = malloc(size + 1);
    read_size = fread(buffer, 1, size, kern_file);
    if (read_size != size) {
        free(buffer);
        fclose(kern_file);
        return NULL;
    }
    fclose(kern_file);
    buffer[size] = '\0';

    return buffer;
}

int process_kernel_file(int arg, char **argv, int argc){
    if (arg >= argc)
        return poclcc_error("Incomplete argument for input file!\n");

    char *filename = argv[arg];
    char *ext = ".pocl";
    kernelSource = kernel_load_file(filename);
    if (outputFile == NULL){
        outputFile = malloc(strlen(filename)+strlen(ext));
        strcpy(outputFile, filename);
        strcat(outputFile, ext);
    }
    return 0;
}

int process_output(int arg, char **argv, int argc){
    if (arg >= argc)
        return poclcc_error("Incomplete argument for output file!\n");

    outputFile = argv[arg];
    return 0;
}

int process_opencl_device(int arg, char **argv, int argc){
    if (arg >= argc)
        return poclcc_error("Incomplete argument for device_type!\n");

    char *opencl_string = argv[arg];
    if (!strcmp(opencl_string, "CL_DEVICE_TYPE_CPU"))
        openclDevice = CL_DEVICE_TYPE_CPU;
    else if (!strcmp(opencl_string, "CL_DEVICE_TYPE_GPU"))
        openclDevice = CL_DEVICE_TYPE_GPU;
    else if (!strcmp(opencl_string, "CL_DEVICE_TYPE_ACCELERATOR"))
        openclDevice = CL_DEVICE_TYPE_ACCELERATOR;
    else if (!strcmp(opencl_string, "CL_DEVICE_TYPE_DEFAULT"))
        openclDevice = CL_DEVICE_TYPE_DEFAULT;
    else if (!strcmp(opencl_string, "CL_DEVICE_TYPE_ALL"))
        openclDevice = CL_DEVICE_TYPE_ALL;
    else { 
        printf("Invalid argument for device_type!\n");
        return print_help();
    }
    return 0;
}

int process_args(int *arg, char **argv, int argc){
    int prev_arg = *arg;
    char *current_arg = argv[*arg];
    if (!strcmp(current_arg, "-h") || !strcmp(current_arg, "--help")){
        return print_help();
    } else if (!strcmp(current_arg, "-d")) { 
        *arg = prev_arg+2;
        return process_opencl_device(prev_arg+1, argv, argc);
    } else if (!strcmp(current_arg, "-o")) { 
        *arg = prev_arg+2;
        return process_output(prev_arg+1, argv, argc);
    } else 
        return poclcc_error("Unknown argument!\n");
}

int main(int argc, char **argv) {
//MANAGEMENT OF ARGUMENTS
    int arg_num=1;
    if (argc < 2)
        return poclcc_error("Invalid argument!\n");

    while (arg_num < argc-1){
        if (process_args(&arg_num, argv, argc))
            return -1;
    }
    if (process_kernel_file(arg_num, argv, argc))
        return -1;

//OPENCL STUFF
    cl_platform_id cpPlatform;
    cl_device_id device_id;
    cl_context context;
    cl_program program;
    cl_int err;

    err = clGetPlatformIDs(1, &cpPlatform, NULL);
    err = clGetDeviceIDs(cpPlatform, openclDevice, 1, &device_id, NULL);
    assert(!err && "clGetDeviceIDs failed");

    context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);
    assert(context && "clCreateContext failed");

    program = clCreateProgramWithSource(context, 1, &kernelSource, NULL, &err);
    assert(program && "clCreateProgramWithSource failed");

    clBuildProgram(program, 0, NULL, NULL, NULL, NULL);  

//GENERATE FILE
    cl_int num_devices;
    size_t size_ret;
    int i;
    
    err = clGetProgramInfo(program, CL_PROGRAM_NUM_DEVICES, sizeof(cl_int), &num_devices, &size_ret);
    assert(!err);

    size_t *binary_sizes = malloc(num_devices*sizeof(size_t));
    err = clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, num_devices*sizeof(size_t), binary_sizes, &size_ret);
    assert(!err);
    
    unsigned char **binaries = malloc(num_devices*sizeof(unsigned char*));
    for (i=0; i<num_devices; i++)
        binaries[i] = malloc(binary_sizes[i]*sizeof(unsigned char));
        
    err = clGetProgramInfo(program, CL_PROGRAM_BINARIES, num_devices*sizeof(unsigned char*), binaries, &size_ret);
    assert(!err);

    FILE *fp=fopen(outputFile, "w"); 
    fprintf(fp, "poclbin\n"); //Magic string identifier
    fprintf(fp, "1\n"); //Format version identifier
    for (i=0; i<num_devices; i++){
        fprintf(fp, "%i ", binary_sizes[i], binaries[i]); //Binary size
        fwrite(binaries[i], sizeof(char), binary_sizes[i], fp); //Binary content
        fprintf(fp, "\n");
    }
    fclose(fp);

//RELEASE OPENCL STUFF
    clReleaseProgram(program);
    clReleaseContext(context);

    return 0;
}
