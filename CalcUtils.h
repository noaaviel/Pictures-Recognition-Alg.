#ifndef CALCULATEUTILS_H_
#define CALCULATEUTILS_H_
#include <math.h>
#include "mpi.h"

#define TAG_FIRST_LOAD 4
#define TAG_ANOTHER_PIC 5
//#define TAG_FOUND 3
//#define TAG_CONTINUE_WORK 4

#define TAG_RESULT 0
#define TAG_ASK_FOR_JOB 1
#define TAG_JOB_DATA 2
#define TAG_STOP 3


int calculateRelativeDiff(int p, int o);

typedef struct {
	int id;
	int dim;
	int **pic;
} Picture;

typedef struct {
	int id;
	int dim;
	int **obj;
} Object;

void readFromFile(double *matchingValue, int *numberOfPictures,
		int *numberOfObjects, Object ***allObjects, Picture ***allPics,
		char *fileName);
void sendAllObjects(int *numberOfObjects, Object **allObjects);
void recvAllObjects(int *numberOfObjects, Object ***allObjects);
void firstLoadPicToProc(int *numOfProc, int *numberOfPictures,
		Picture **allPics);
int getPicFromMaster(Picture **retPic, MPI_Status status);
void sendApic(Picture ***allPics, int *pos, int *sender);
int processPicture(int *posX, int *posY, Picture *picture, Object *obj, double matchingValue);
void freeAllObjects(Object ***allObjects, int *numberOfObjects);
void freeApic(Picture **picture);
void freeAllPictures(Picture ***allPictures, int *numberOfPictures);
void master(Picture ***allPictures, int *numberOfProc, int *numOfPics);
void slave(Object ***allObjects, int *numberOfObjects, double *matchingValue);

#endif

