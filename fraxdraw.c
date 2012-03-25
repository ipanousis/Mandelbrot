#include "mandel.h"
#include "math/complex.h"
#include "colortables/colortables.h"
#include <GL/glut.h>
#include <unistd.h>

#define SIZE 1024

pthread_t redisplayThread;
pthread_t glutThread;

int displayWindow;
short int drawingrow;

short int computeGPU = 1;
short int computeMT  = 0;

// Mandel OpenCL constructs
ocl_constructs * mandelCl;

// Mandel data/settings
mandelbrot_section_params * mandel;
GLfloat* newcolorarray;
GLfloat* colorarray;
GLfloat* vertexarray;

// Interactiveness
double panOffset  = 0.3; // for panning
double zoomOffset = 0.25; // for centre region focus & for step scaling

// Processing/Displaying sync
short int   active = 1;
short int * readyrows;
short int   drawing = 1;
pthread_cond_t  cv_drawing = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mx_drawing = PTHREAD_MUTEX_INITIALIZER;

// Multi-threaded draw
void* mandelbrotDraw(void* params){
  mandelbrot_section_params* castedparams = (mandelbrot_section_params*)params;

  //printf("Started rows %d-%d...\n",castedparams->startIndex.y,(castedparams->startIndex.y)+(castedparams->dimensions.y)-1);

  GLdouble escapedN;
  GLfloat* marray     = vertexarray;
  point_d  startPoint = castedparams->startPoint;
  point_i  startIndex = castedparams->startIndex;
  point_i  dimensions = castedparams->dimensions;

  mandel_state* current_state = new_mandel_state();
  double step = castedparams->step;

  int palleteindex;
  int rowsdone = 0;
  int row = startIndex.y;
  int col;
  int i = 0;
  int rowjump = dimensions.y-1;
  while(rowsdone < dimensions.y){

    if(row < (startIndex.y+dimensions.y) && readyrows[row]==0){

      current_state->MANDEL_C.x = startPoint.x + startIndex.x * step;
      current_state->MANDEL_C.y = startPoint.y - row * step;

      for (col = startIndex.x; col < startIndex.x + dimensions.x; col++){

        current_state->MANDEL_Z->x = 0;
        current_state->MANDEL_Z->y = 0;
        current_state->MANDEL_CUR_DEPTH = 0;

        escapedN = (GLfloat)mandel_escape(current_state);

        // Use pallete colors
        palleteindex = (int)(escapedN * 4.15f) + 25;
        marray[i * 6 + 3] = table1[palleteindex][0] / 256.0f;
        marray[i * 6 + 4] = table1[palleteindex][2] / 256.0f;
        marray[i * 6 + 5] = table1[palleteindex][1] / 256.0f;
        
        current_state->MANDEL_C.x += step;
      }

      readyrows[row] = 1;
      rowsdone++;
    }
    if(row >= (startIndex.y + dimensions.y - 1)){
      rowjump = rowjump / 2;
      row = startIndex.y;
    }
    else{
      row += rowjump;
    }
    usleep(5000);
  }

  //printf("Finished rows %d-%d.\n",castedparams->startIndex.y,(castedparams->startIndex.y)+(castedparams->dimensions.y)-1);

  free(current_state);
  free(castedparams);

  return NULL;
}

// GPU-accelerated draw
void mandelbrotDrawGPU(){

  int fail = mandelbrotGPU(mandel, mandelCl);

  if(fail){
    fprintf(stderr, "mandelbrotGPU failed\n");
    return;
  }

  int* escapedArray = mandel->data->mandelarray_i;

  int i, palleteindex;
  for(i = 0; i < mandel->dimensions.y * mandel->dimensions.x; i++){

    // Use pallete colors, make max use of the pallete colours
    palleteindex = (int)(escapedArray[i] * 4.15f) + 25;
    newcolorarray[i * 3]     = table1[palleteindex][0] / 255.0f;
    newcolorarray[i * 3 + 1] = table1[palleteindex][1] / 255.0f;
    newcolorarray[i * 3 + 2] = table1[palleteindex][2] / 255.0f;

  }

  GLfloat* swap = colorarray;
  colorarray = newcolorarray;
  newcolorarray = swap;
  
}

// Call a mandelbrot draw
void* doMandel(void* params){

  while(active){

    if(drawing){

      int j;
      for(j = 0; j < mandel->dimensions.y; j++)
        readyrows[j] = 0;

      // Multi-threaded
      //mandelbrot(mandelArray,startPoint,dimensions,step,mandelpow,mandelbrotDraw);

      // GPU
      mandelbrotDrawGPU();

      drawing = 0;

    }

    // Use the wait functionality of conditional variables, not actual locking
    // (but conform to the required locking/unlocking of course)
    pthread_mutex_lock(&mx_drawing);

    pthread_cond_wait(&cv_drawing, &mx_drawing);

    pthread_mutex_unlock(&mx_drawing);
  }
  
  return NULL;
}

void handleKeypress(unsigned char key, int x, int y){

  // Handle W-A-S-D for panning
  // Q-E for zoom
  // Call doMandel with new settings

  short int change  = 0;
  double yOffset    = 0.0f;
  double xOffset    = 0.0f;
  double stepOffset = 0.0f;

  double range = mandel->dimensions.x * mandel->step;

  switch(key){

  // Pan up
  case 'W':
    change = 1;
    yOffset = range * panOffset;
    break;
  // Pan left
  case 'A':
    change = 1;
    xOffset = -range * panOffset;
    break;
  // Pan down
  case 'S':
    change = 1;
    yOffset = -range * panOffset;
    break;
  // Pan right
  case 'D':
    change = 1;
    xOffset = range * panOffset;
    break;
  // Zoom out
  case 'Q':
    change = 1;
    xOffset    = -range * zoomOffset * 2;
    yOffset    =  range * zoomOffset * 2;
    stepOffset =  mandel->step;
    break;
  // Zoom in
  case 'E':
    change = 1;
    xOffset    =  range * zoomOffset / 2;
    yOffset    = -range * zoomOffset / 2;
    stepOffset = -mandel->step * zoomOffset;
    break;
  // Exit
  case 'X':
    active = 0;
    change = 1;
    drawing = 0;
  default: break;

  }

  if(change && !drawing){

    mandel->startPoint.x += xOffset;
    mandel->startPoint.y += yOffset;
    mandel->step         += stepOffset;

    //printf("New startPoint = {%f,%f}, step = %f\n", mandel->startPoint.x, mandel->startPoint.y, mandel->step);

    drawing = 1;

    // Notify waiting thread
    pthread_mutex_lock(&mx_drawing);
    
    pthread_cond_signal(&cv_drawing);
    
    pthread_mutex_unlock(&mx_drawing);

  }

}

void initRendering(){

  glEnable(GL_DEPTH_TEST);

}

// Called when window is resized
void handleResize(int w,int h){
  // Tell OpenGL how to convert from coordinates to pixel values
  glViewport(0,0,w,h);

  // Set the camera perspective
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity(); //Reset the camera

  // Camera angle, width-to-height ratio, near z clipping coord, far z clipping coord
  gluPerspective(45.0,(float)w/(float)h,1.0,600.0);
}

 // Draws the 3D scene
void drawScene(){

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_MODELVIEW);

  glLoadIdentity();

  glBegin(GL_LINE_STRIP);
  glColor3f(1.0f, 1.0f, 1.0f);

  glVertex3f(-2.0f, 2.0f, -5.0f);
  glVertex3f( 2.0f, 2.0f, -5.0f);
  glVertex3f( 2.0f,-2.0f, -5.0f);
  glVertex3f(-2.0f,-2.0f, -5.0f);
  glVertex3f(-2.0f, 2.0f, -5.0f);
  glEnd();

  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_COLOR_ARRAY);

  /*if(computeMT){
    for(drawingrow = 0; drawingrow < mandel->dimensions.y; drawingrow++){
      if(readyrows[drawingrow]){
        glVertexPointer(3, GL_FLOAT, 6 * sizeof(GLfloat), &(vertexarray[drawingrow * mandel->dimensions.x * 6]));
        glColorPointer(3, GL_FLOAT, 6 * sizeof(GLfloat), &(vertexarray[drawingrow * mandel->dimensions.x * 6 + 3]));
        glDrawArrays(GL_POINTS, 0, mandel->dimensions.x);
      }
    }
  }
  else if(computeGPU){*/

  glVertexPointer(3, GL_FLOAT, 3 * sizeof(GLfloat), &(vertexarray[0]));
  glColorPointer(3, GL_FLOAT, 3 * sizeof(GLfloat), &(colorarray[0]));
  glDrawArrays(GL_POINTS, 0, mandel->dimensions.x * mandel->dimensions.y);

  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_COLOR_ARRAY);

  glutSwapBuffers(); // Sends 3D scene
  glFlush();
}

void dataInit(){

  point_i dimensions = {SIZE, SIZE};
  point_d startPoint = {-2, 2};
  point_i startIndex = { 0, 0};
  double step = 4.0f / SIZE;
  int mandelpow = 2;

  int bytes = sizeof(GLfloat) * dimensions.x * dimensions.y * 6;
  printf("Allocating... %d bytes - %d KB - %d MB\n", bytes, bytes / 1024, bytes / (1024 * 1024));

  mandelarray * mandelArray = (mandelarray*)malloc(sizeof(mandelarray));

  mandelArray->mandelarray_i = (int*)malloc(sizeof(int) * dimensions.y * dimensions.x);

  mandel = new_mandelbrot_section_params(mandelArray, 
                                         &startPoint, 
                                         &startIndex,
                                         &dimensions, 
                                         step, mandelpow);

  vertexarray = (GLfloat*)malloc(sizeof(GLfloat) * dimensions.y * dimensions.x * 3);
  colorarray  = (GLfloat*)malloc(sizeof(GLfloat) * dimensions.y * dimensions.x * 3);
  newcolorarray = (GLfloat*)malloc(sizeof(GLfloat) * dimensions.y * dimensions.x * 3);

  int i;
  for(i = 0; i < mandel->dimensions.x * mandel->dimensions.y; i++){

      vertexarray[i * 3]     = -2 + (i % mandel->dimensions.x) * mandel->step;
      vertexarray[i * 3 + 1] =  2 - (i / mandel->dimensions.x) * mandel->step;
      vertexarray[i * 3 + 2] = -5.0f;

  }

  readyrows = (short int*)malloc(sizeof(short int) * dimensions.y);

  short int clGlSharing = 1;

  mandelCl = newOclConstructs(0,0,clGlSharing);

  initMandelbrotGPU(mandel, mandelCl);
}

void* redisplay(int value){

  if(!active) return NULL;
  
  glutPostRedisplay();

  glutTimerFunc(10, (void*)redisplay, 0);

  return NULL;

}

void* startGlutMainLoop(void* params){

  // Initialise GLUT
  int argc = 0;
  glutInit(&argc, NULL);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  glutInitWindowSize(SIZE, SIZE); //Set the window size

  // Create the window
  displayWindow = glutCreateWindow("Mandel-Mandel");
  initRendering(); // init rendering

  glutDisplayFunc(drawScene);
  glutKeyboardFunc(handleKeypress);
  glutReshapeFunc(handleResize);
  glutTimerFunc(20, (void*)redisplay, 0);

  glutMainLoop();

  return NULL;
}

int main(int argc,char** argv){

  dataInit();

  // Start GLUT in a separate thread
  pthread_create(&glutThread, NULL, (void*)startGlutMainLoop, (void*)NULL);

  // Start the mandel loop
  doMandel(NULL);

  return 0;
}

