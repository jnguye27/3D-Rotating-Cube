/* 
Program: 3D Rotating Cube
Creator: Jessica Nguyen
Date: 2023-11-26
Purpose: Turns the number of points the user inputs into a moving image of a rotational cube.

3D Graphics using ASCII graphics
	- Original NCurses code from "Game Programming in C with the Ncurses Library"
	 https://www.viget.com/articles/game-programming-in-c-with-the-ncurses-library/
	 and from "NCURSES Programming HOWTO"
	 http://tldp.org/HOWTO/NCURSES-Programming-HOWTO/
*/ 

#define CL_USE_DEPRECATED_OPENCL_1_2_APIS // gets rid of the "warning: ‘clCreateCommandQueue’ is deprecated"
#define CL_TARGET_OPENCL_VERSION 120 // gets rid of "#pragma message: cl_version.h: CL_TARGET_OPENCL_VERSION is not defined."

#include<stdio.h>
#include<stdlib.h>
#include<math.h>
#include<string.h>
#ifndef NOGRAPHICS
   #include<unistd.h>
   #include<ncurses.h>
#endif
#ifdef MAC
   #include <OpenCL/cl.h>
#else
   #include <CL/cl.h>
#endif

#define DELAY 10000
// maximum screen size, both height and width
#define SCREENSIZE 100
// default number of iterations to run before exiting, only used
// when graphics are turned off
#define ITERATIONS 1000
#define PROGRAMFILE "transform.cl"
#define KERNELFUNC "transform"

// number of points
int pointCount;
// array of points before transformation
float **pointArray;
// array of points after transformation
float **drawArray;

// transformation matrix
float transformArray[4][4];
// display buffers
char frameBuffer[SCREENSIZE][SCREENSIZE];
float depthBuffer[SCREENSIZE][SCREENSIZE];

#ifndef NOGRAPHICS
	// maximum screen dimensions
   int max_y = 0, max_x = 0;
#endif

#ifndef NOGRAPHICS
   int drawPoints() {
      int c, i, j;
      float multx, multy;
      float point[4];

      // update screen maximum size
      getmaxyx(stdscr, max_y, max_x);

      // used to scale position of points based on screen size
      multx = (float)max_x / SCREENSIZE;
      multy = (float)max_y / SCREENSIZE;

      clear();

      // display points
      for(i=0; i<SCREENSIZE; i++) {
         for(j=0; j<SCREENSIZE; j++) {
            if (frameBuffer[i][j] == 'X') {
               mvprintw(i * multy, j*multx, "X");
            } else if (frameBuffer[i][j] == 'o') {
               mvprintw(i * multy, j*multx, "o");
            } else if (frameBuffer[i][j] == '.') {
               mvprintw(i * multy, j*multx, ".");
            }
         }
      }

      refresh();

      usleep(DELAY);

      // read keyboard and exit if 'q' pressed
      c = getch();
      if (c == 'q') {
         return(1);
      } else { 
         return(0);
      }
   }
#endif

// find a GPU/CPU associated with the first available platform
cl_device_id create_device() {
   // declaring variables
   int err;
   cl_device_id dev;
   cl_platform_id platform;

   // identify a platform
   err = clGetPlatformIDs(1, &platform, NULL);
   if (err < 0) {
      perror("Couldn't identify a platform");
      exit(1);
   } 

   // access a device
   err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &dev, NULL);
   if (err == CL_DEVICE_NOT_FOUND) {
      err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &dev, NULL);
   }
   if (err < 0) {
      perror("Couldn't access any devices");
      exit(1);   
   }

   return dev;
}

// create program from a file and compile it
cl_program build_program(cl_context ctx, cl_device_id dev, const char* filename) {
   cl_program program;
   FILE *program_handle;
   char *program_buffer, *program_log;
   size_t program_size, log_size;
   int err;

   // read program file and place content into buffer
   program_handle = fopen(filename, "r");
   if (program_handle == NULL) {
      perror("Couldn't find the program file");
      exit(1);
   }
   fseek(program_handle, 0, SEEK_END);
   program_size = ftell(program_handle);
   rewind(program_handle);
   program_buffer = (char*)malloc(program_size + 1);
   program_buffer[program_size] = '\0';
   fread(program_buffer, sizeof(char), program_size, program_handle);
   fclose(program_handle);

   // create program from file
   program = clCreateProgramWithSource(ctx, 1, (const char**)&program_buffer, &program_size, &err);
   if (err < 0) {
      perror("Couldn't create the program");
      exit(1);
   }
   free(program_buffer);

   // build program *** might change 0 to 1
   err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
   if (err < 0) {
      // find size of log and print to std output
      clGetProgramBuildInfo(program, dev, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
      program_log = (char*)malloc(log_size + 1);
      program_log[log_size] = '\0';
      clGetProgramBuildInfo(program, dev, CL_PROGRAM_BUILD_LOG, log_size + 1, program_log, NULL);
      printf("%s\n", program_log);
      free(program_log);
      exit(1);
   }

   return program;
}

// calculates the product of matrices b and c stores the result in matrix a
void matrixMult(float a[4][4], float b[4][4], float c[4][4]) {
   int row, col, element;

   for (row=0; row<4; row++) {
      for (col=0; col<4; col++) {
         a[row][col] = 0.0; 
         for (element=0; element<4; element++) {
            a[row][col] += b[row][element] * c[element][col]; 
         }
      }
   }
}

void allocateArrays() {
   int i;

   pointArray = malloc(sizeof(float *) * pointCount);
   for(i=0; i<pointCount; i++) {
      pointArray[i] = malloc(sizeof(float) * 4);
   }
   drawArray = malloc(sizeof(float *) * pointCount);
   for(i=0; i<pointCount; i++) {
      drawArray[i] = malloc(sizeof(float) * 4);
   }
}

void cubePointArray() {
   pointArray[0][0] = 0.5;
   pointArray[0][1] = 0.0;
   pointArray[0][2] = 0.5;
   pointArray[0][3] = 1.0;

   pointArray[1][0] = 0.5;
   pointArray[1][1] = 0.0;
   pointArray[1][2] = -0.5;
   pointArray[1][3] = 1.0;

   pointArray[2][0] = -0.5;
   pointArray[2][1] = 0.0;
   pointArray[2][2] = -0.5;
   pointArray[2][3] = 1.0;

   pointArray[3][0] = -0.5;
   pointArray[3][1] = 0.0;
   pointArray[3][2] = 0.5;
   pointArray[3][3] = 1.0;

   pointArray[4][0] = 0.5;
   pointArray[4][1] = 1.0;
   pointArray[4][2] = 0.5;
   pointArray[4][3] = 1.0;

   pointArray[5][0] = 0.5;
   pointArray[5][1] = 1.0;
   pointArray[5][2] = -0.5;
   pointArray[5][3] = 1.0;

   pointArray[6][0] = -0.5;
   pointArray[6][1] = 1.0;
   pointArray[6][2] = -0.5;
   pointArray[6][3] = 1.0;

   pointArray[7][0] = -0.5;
   pointArray[7][1] = 1.0;
   pointArray[7][2] = 0.5;
   pointArray[7][3] = 1.0;
}

void randomPointArray() {
   int i;
   float val;

   for (i=0; i<pointCount; i++) {
      val = (float) random() / 10000.0;
      pointArray[i][0] = 2.5 * ((val - trunc(val)) - 0.5);
      val = (float) random() / 10000.0;
      pointArray[i][1] = 2.5 * ((val - trunc(val)) - 0.5);
      val = (float) random() / 10000.0;
      pointArray[i][2] = 2.5 * ((val - trunc(val)) - 0.5);
      val = (float) random() / 10000.0;
      pointArray[i][3] = 2.5 * ((val - trunc(val)) - 0.5);
   }
}

void initTransform() {
   int i, j; 

   for (i=0; i<4; i++) {
      for (j=0; j<4; j++) {
         if (i == j) {
            transformArray[i][j] = 1.0;
         } else {
            transformArray[i][j] = 0.0;
         }
      }
   }
}

void xRot(int rot) {
   float oneDegree = 0.017453;
   float angle, sinAngle, cosAngle;
   float result[4][4];
   int i, j;
   float rotx[4][4]  = {1.0, 0.0, 0.0, 0.0, 
                        0.0, 1.0, 0.0, 0.0, 
                        0.0, 0.0, 1.0, 0.0, 
                        0.0, 0.0, 0.0, 1.0}; 

   angle = (float) rot * oneDegree;
   sinAngle = sinf(angle);
   cosAngle = cosf(angle);

   rotx[1][1] = cosAngle;
   rotx[2][2] = cosAngle;
   rotx[1][2] = -sinAngle;
   rotx[2][1] = sinAngle;

   matrixMult(result, transformArray, rotx);

	// copy result to transformation matrix
   for (i=0; i<4; i++) {
      for (j=0; j<4; j++) {
         transformArray[i][j] = result[i][j];
      }
   }
}

void yRot(int rot) {
   float oneDegree = 0.017453;
   float angle, sinAngle, cosAngle;
   float result[4][4];
   int i, j;
   float roty[4][4]  = {1.0, 0.0, 0.0, 0.0, 
                        0.0, 1.0, 0.0, 0.0, 
                        0.0, 0.0, 1.0, 0.0, 
                        0.0, 0.0, 0.0, 1.0}; 

   angle = (float) rot * oneDegree;
   sinAngle = sinf(angle);
   cosAngle = cosf(angle);

   roty[0][0] = cosAngle;
   roty[2][2] = cosAngle;
   roty[0][2] = sinAngle;
   roty[2][0] = -sinAngle;

   matrixMult(result, transformArray, roty);

	// copy result to transformation matrix
   for (i=0; i<4; i++) {
      for (j=0; j<4; j++) {
         transformArray[i][j] = result[i][j];
      }
   }
}

void zRot(int rot) {
   float oneDegree = 0.017453;
   float angle, sinAngle, cosAngle;
   float result[4][4];
   int i, j;
   float rotz[4][4]  = {1.0, 0.0, 0.0, 0.0, 
                        0.0, 1.0, 0.0, 0.0, 
                        0.0, 0.0, 1.0, 0.0, 
                        0.0, 0.0, 0.0, 1.0}; 

   angle = (float) rot * oneDegree;
   sinAngle = sinf(angle);
   cosAngle = cosf(angle);

   rotz[0][0] = cosAngle;
   rotz[1][1] = cosAngle;
   rotz[0][1] = -sinAngle;
   rotz[1][0] = sinAngle;

   matrixMult(result, transformArray, rotz);

	// copy result to transformation matrix
   for (i=0; i<4; i++) {
      for (j=0; j<4; j++) {
         transformArray[i][j] = result[i][j];
      }
   }
}

void translate(float x, float y, float z) {
   transformArray[3][0] = x;
   transformArray[3][1] = y;
   transformArray[3][2] = z;
}

void clearBuffers() {
   int i, j;

	// empty the frame buffer
	// set the depth buffer to a large distance
   for(i=0; i<SCREENSIZE; i++) {
      for(j=0; j<SCREENSIZE; j++) {
         frameBuffer[i][j] = ' ';
         depthBuffer[i][j] = -1000.0; 
      }
   }
}

void movePoints() {
   // declaring variables
   static int counter = 1;
   int x, y;

   // OpenCL structures
   cl_device_id device;
   cl_context context;
   cl_program program;
   cl_kernel kernel;
   cl_command_queue queue;
   cl_int i, j, k, err;
   cl_mem draw_mem, point_mem, transform_mem;
   float draw_data[pointCount*4], point_data[pointCount*4], transform_data[4*4];
   size_t local_size, global_size;

   // initializing variables
   device = NULL;

	// initialize transformation matrix
	// this needs to be done before the transformation is performed
   initTransform();

	// add some rotations to the transformation matrix
	// this needs to be done before the transformation is performed
   xRot(counter);
   yRot(counter);
   counter++;

	// OpenCL kernel: transform the points using the transformation matrix,
	// store the results of the transformation in the drawing array
   // turn our 2d arrays into temporary 1d arrays
   k = 0;
   for (i = 0; i < pointCount; i++) {
      for (j = 0; j < 4; j++) {
         draw_data[k] = drawArray[i][j];
         point_data[k] = pointArray[i][j];
         k++;
      }
   }
   k = 0;
   for (i = 0; i < 4; i++) {
      for (j = 0; j < 4; j++) {
         transform_data[k] = transformArray[i][j];
         k++;
      }
   }

   // create the device
   device = create_device();

   // create the context
   context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
   if (err < 0) {
      perror("Couldn't create a context");
      exit(1);   
   }
   
   // build the program
   program = build_program(context, device, PROGRAMFILE);

   // create memory buffer
   global_size = pointCount; // process the entire thing
   local_size = 1; // divide work items into groups #
   draw_mem = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, pointCount * 4 * sizeof(float), draw_data, &err);
   point_mem = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, pointCount * 4 * sizeof(float), point_data, &err);
   transform_mem = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 4 * 4 * sizeof(float), transform_data, &err);
   if (err < 0) {
      perror("Couldn't create a buffer");
      exit(1);   
   };

   // create a command queue
   queue = clCreateCommandQueue(context, device, 0, &err);
   if (err < 0) {
      perror("Couldn't create a command queue");
      exit(1);   
   };

   // create a kernel
   kernel = clCreateKernel(program, KERNELFUNC, &err);
   if (err < 0) {
      perror("Couldn't create a kernel");
      exit(1);
   };

   // create kernel arguments
   err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &draw_mem);
   err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &point_mem);
   err |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &transform_mem);
   if (err < 0) {
      perror("Couldn't create a kernel argument");
      exit(1);
   }

   // enqueue kernel
   err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL); 
   if (err < 0) {
      perror("Couldn't enqueue the kernel");
      exit(1);
   }

   // read the kernel's output
   err = clEnqueueReadBuffer(queue, draw_mem, CL_TRUE, 0, sizeof(draw_data), draw_data, 0, NULL, NULL);
   if (err < 0) {
      perror("Couldn't read the buffer");
      exit(1);
   }

   // save the 1D data back to our 2D global data
   k = 0;
   for (i = 0; i < pointCount; i++) {
      for (j = 0; j < 4; j++) {
         drawArray[i][j] = draw_data[k];
         pointArray[i][j] = point_data[k];
         k++;
      }
   }
   k = 0;
   for (i = 0; i < 4; i++) {
      for (j = 0; j < 4; j++) {
         transformArray[i][j] = transform_data[k];
         k++;
      }
   }

   // deallocate resources
   clReleaseKernel(kernel);
   clReleaseMemObject(draw_mem);
   clReleaseMemObject(point_mem);
   clReleaseMemObject(transform_mem);
   clReleaseCommandQueue(queue);
   clReleaseProgram(program);
   clReleaseContext(context);

	// clears buffers before drawing screen
   clearBuffers();

	// draw the screen
	// adds points to the frame buffer, use depth buffer to
	// sort points based upon distance from the viewer
   for (i=0; i<pointCount; i++) {
      x = (int) drawArray[i][0];
      y = (int) drawArray[i][1];
      if (depthBuffer[x][y] < drawArray[i][2]) { 
         if (drawArray[i][2] > 60.0) {
            frameBuffer[x][y] = 'X'; 
         } else if (drawArray[i][2] < 40.0) {
            frameBuffer[x][y] = '.'; 
         } else {
            frameBuffer[x][y] = 'o'; 
            depthBuffer[x][y] = drawArray[i][2];
         }
      }
   }
}

int main(int argc, char *argv[]) {
   int i, count;
   int argPtr;
   int drawCube, drawRandom;

	// set number of iterations, only used for timing tests 
	// not used in curses version
   count = ITERATIONS;

	// initialize drawing objects
   drawCube = 0;
   drawRandom = 0;

	// read command line arguments for number of iterations 
   if (argc > 1) {
      argPtr = 1;
      while(argPtr < argc) {
         if (strcmp(argv[argPtr], "-i") == 0) {
            sscanf(argv[argPtr+1], "%d", &count);
            argPtr += 2;
         } else if (strcmp(argv[argPtr], "-cube") == 0) {
            drawCube = 1;
            pointCount = 8;
            argPtr += 1;
         } else if (strcmp(argv[argPtr], "-points") == 0) {
            drawRandom = 1;
            sscanf(argv[argPtr+1], "%d", &pointCount);
            argPtr += 2;
         } else {
            printf("USAGE: %s <-i iterations> <-cube | -points #>\n", argv[0]);
            printf(" iterations -the number of times the population will be updated\n");
            printf("    the number of iterations only affects the non-curses program\n");
            printf(" the curses program exits when q is pressed\n");
            printf(" choose either -cube to draw the cube shape or -points # to\n");
            printf("    draw random points where # is an integer number of points to draw\n");
            exit(1);
         }
      }
   }

	// allocate space for arrays to store point position
   allocateArrays();
   if (drawCube == 1) {
      cubePointArray();
   } else if (drawRandom == 1) {
      randomPointArray();
   } else {
      printf("You must choose either <-cube> or <-points #> on the command line.\n");
      exit(1);
   }

   #ifndef NOGRAPHICS
      // initialize ncurses
      initscr();
      noecho();
      cbreak();
      timeout(0);
      curs_set(FALSE);
      
      // Global var `stdscr` is created by the call to `initscr()`
      getmaxyx(stdscr, max_y, max_x);
   #endif


   // draw and move points using ncurses 
   // do not calculate timing in this loop, ncurses will reduce performance
   #ifndef NOGRAPHICS
      while(1) {
         if (drawPoints() == 1) {
            break;
         }
         movePoints();
      }
   #endif

   // calculate movement of points but do not use ncurses to draw
   #ifdef NOGRAPHICS
      printf("Number of iterations %d\n", count);

      for(i=0; i<count; i++) {
         movePoints();
      }
   #endif

   #ifndef NOGRAPHICS
      // shut down ncurses
      endwin();
   #endif
}