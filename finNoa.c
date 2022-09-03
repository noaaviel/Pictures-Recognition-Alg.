#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "CalcUtils.h"
#include <omp.h>

int main(int argc, char *argv[]) {
	int my_rank; /* rank of process */
	int p; /* number of processes */
	int source; /* rank of sender */
	int dest; /* rank of receiver */
	int tag = 0; /* tag for messages */
	char message[100]; /* storage for message */

	MPI_Status status; /* return status for receive */

	Object **allObjects;
	Picture **allPics; 

	int numberOfPictures = 0, numberOfObjects = 0;
	double start,finish;
	double matchingValue;
	int id, init;


	/* start up MPI */

	MPI_Init(&argc, &argv);

	/* find out process rank */
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

	/* find out number of processes */
	MPI_Comm_size(MPI_COMM_WORLD, &p);
	start= MPI_Wtime();

	if (my_rank != 0) {
		/*each proc that isn't 0 needs to receive all the objects to be found in pics*/
		MPI_Bcast(&init, 1, MPI_INT, 0, MPI_COMM_WORLD);
		if (init == 0) {
			recvAllObjects(&numberOfObjects, &allObjects);
			MPI_Bcast(&matchingValue, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

			MPI_Bcast(&init, 1, MPI_INT, 0, MPI_COMM_WORLD);

		}
		slave(&allObjects, &numberOfObjects, &matchingValue);
	} else {
		char *fileName = "input.txt";
		readFromFile(&matchingValue, &numberOfPictures, &numberOfObjects,
				&allObjects, &allPics, fileName);
		init = 0;
		MPI_Bcast(&init, 1, MPI_INT, 0, MPI_COMM_WORLD);
		sendAllObjects(&numberOfObjects, allObjects);
		MPI_Bcast(&matchingValue, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

		init = 1;
		MPI_Bcast(&init, 1, MPI_INT, 0, MPI_COMM_WORLD);
		master(&allPics, &p, &numberOfPictures);
		
		//freeAllPics
		freeAllPictures(&allPics,&numberOfPictures);
		finish= MPI_Wtime();
		printf("%.8f seconds\n",(finish-start));

	}
	/* shut down MPI */
	freeAllObjects(&allObjects, &numberOfObjects);
	
	
	MPI_Finalize();

	return 0;
}
//dynamic master slave loading
//each proc asks for a picture at first!
//each proc searches an object from allObjects in all possible positions
//if found-> notify proc 0 which object in what picture and what position
//else if didn't match any position-> proceed to next object
//if finished all objects search on pic --> ask proc 0 for another pic
//if there are more pics in 0 -> send this proc another pic
void master(Picture ***allPictures, int *numberOfProc, int *numOfPics) {

	MPI_Status stat, stat2;
	int slavesWorking = *numberOfProc - 1; //all the processes that searches objects not including 0
	int counter = *numOfPics; //number of pictures left to process
	int picIndex = 0;
	int found = 0;
	int picToPrintIndex, objToPrintIndex, posX, posY;
	int stopped = 0;
	FILE *fp;
	char *fileName = "output.txt";
	if ((fp = fopen(fileName, "w")) == 0) {
		printf("cannot open file %s for reading\n", fileName);
		exit(0);
	}
	/* there are jobs unprocessed  ||  there are slaves still working on jobs */
	while (counter > 0 || slavesWorking < *numberOfProc - 1) {
		
		// Wait for any incoming message from any of the processes running
		MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &stat);

		// Store rank of receiver into slave_rank
		int slave_rank = stat.MPI_SOURCE;
	
		// Decide according to the tag which type of message we have got and handle it accordingly
		if (stat.MPI_TAG == TAG_ASK_FOR_JOB) {
			//the recv here is just to accept which slave_rank sent a request
			MPI_Recv(&found, 1, MPI_INT, slave_rank, TAG_ASK_FOR_JOB,
					MPI_COMM_WORLD, &stat2);
			/*if there are unprocessed jobs*/
			if (counter > 0) {
				counter = counter - 1; // send picture to process
				slavesWorking = slavesWorking - 1;
				// pack data of job
				sendApic(allPictures, &picIndex, &slave_rank);
				picIndex = picIndex + 1;
				//MPI_Send(pic, /*...*/, slave_rank, TAG_JOB_DATA, MPI_COMM_WORLD);
				
			} else {
				// send stop msg to slave
				int stopWork = 1;
				MPI_Send(&stopWork, 1, MPI_INT, slave_rank, TAG_STOP,
						MPI_COMM_WORLD);
			}
		} else {
			slavesWorking = slavesWorking + 1;//slave finished his job and now we have 1 more free slave
			// We got a result message and the slave is not working now
			MPI_Recv(&found, 1, MPI_INT, slave_rank, TAG_RESULT, MPI_COMM_WORLD,
					&stat2);
			
			//deal with the result
			if (found == 1) { //Check If Found And Print To File
				MPI_Recv(&picToPrintIndex, 1, MPI_INT, slave_rank, TAG_RESULT,
						MPI_COMM_WORLD, &stat2);
				MPI_Recv(&objToPrintIndex, 1, MPI_INT, slave_rank, TAG_RESULT,
						MPI_COMM_WORLD, &stat2);
				MPI_Recv(&posX, 1, MPI_INT, slave_rank, TAG_RESULT,
						MPI_COMM_WORLD, &stat2);
				MPI_Recv(&posY, 1, MPI_INT, slave_rank, TAG_RESULT,
						MPI_COMM_WORLD, &stat2);

				fprintf(fp, "Picture #%d: found object #%d at (%d,%d)\n",
						picToPrintIndex, objToPrintIndex, posX, posY);
				// mark slave with rank slave_rank as stopped

			} else {
				MPI_Recv(&picToPrintIndex, 1, MPI_INT, slave_rank, TAG_RESULT,
						MPI_COMM_WORLD, &stat2);
				fprintf(fp, "No Objects Found In Picture #%d\n",
						picToPrintIndex);
			}
			if(counter == 0){
				stopped = 1;
				MPI_Send(&stopped, 1, MPI_INT, slave_rank, TAG_STOP,
						MPI_COMM_WORLD);
			}
				
		}
	}
	
}

void slave(Object ***allObjects, int *numberOfObjects, double *matchingValue) {
	int stopped = 0;
	MPI_Status stat, stat2;
	Picture *picToWork;
	int found = 0;
	int flagGetPic;

	int picToPrintIndex, objToPrintIndex=-1;
	int printX=0,printY=0;
	int posX = 0, posY = 0;
	do {
		// Here we send a message to the master asking for a job
		MPI_Send(&found, 1, MPI_INT, 0, TAG_ASK_FOR_JOB, MPI_COMM_WORLD);
		MPI_Probe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &stat); //wait to recv TAG from master
		if (stat.MPI_TAG == TAG_JOB_DATA) {
			// Retrieve job data from master into msg_buffer
			flagGetPic=getPicFromMaster(&picToWork, stat);
			if(flagGetPic!=1){
				exit(0);
			}
			picToPrintIndex = picToWork->id;
			//printf("in proc: pic: %d dim %d\n", picToWork->id, picToWork->dim);

			//iterate over all objects
			int i;
			int breakFlag=1;
#pragma omp parallrl for shared(breakFlag,objToPrintIndex,found,printX,printY)			
			for (i = 0; i < *numberOfObjects; i++) {
				if(breakFlag){		
				//calculation for each object
					found = processPicture(&posX, &posY, picToWork,
							allObjects[0][i], *matchingValue);
					if (found == 1&& breakFlag) {
						objToPrintIndex = i; 
						printX=posX;
						printY=posY;
						breakFlag=0;
						//break;	
					}
				}	
			}
			if(objToPrintIndex>=0 && objToPrintIndex<*numberOfObjects){
				found=1;
				
			}else{
				found=0;
				MPI_Send(&found,1,MPI_INT,0,TAG_RESULT,MPI_COMM_WORLD);
				MPI_Send(&picToPrintIndex,1,MPI_INT,0,TAG_RESULT,MPI_COMM_WORLD);
			
			}
			
			
			// send result to master found or not
			MPI_Send(&found, 1, MPI_INT, 0, TAG_RESULT, MPI_COMM_WORLD);
			if (found == 1) {
				objToPrintIndex = allObjects[0][objToPrintIndex]->id;
				MPI_Send(&picToPrintIndex, 1, MPI_INT, 0, TAG_RESULT,
						MPI_COMM_WORLD);
				MPI_Send(&objToPrintIndex, 1, MPI_INT, 0, TAG_RESULT,
						MPI_COMM_WORLD);
				MPI_Send(&printX, 1, MPI_INT, 0, TAG_RESULT, MPI_COMM_WORLD);
				MPI_Send(&printY, 1, MPI_INT, 0, TAG_RESULT, MPI_COMM_WORLD);
				
			}
		} else { //stop working
				 // We got a stop message we have to retrieve it by using MPI_Recv
				 // But we can ignore the data from the MPI_Recv call
			
			MPI_Recv(&stopped, 1, MPI_INT, 0, TAG_STOP, MPI_COMM_WORLD, &stat2);
		
			stopped = 1;
		}

	} while (stopped == 0);
}
void firstLoadPicToProc(int *numOfProc, int *numberOfPictures,
		Picture **allPics) {
	for (int i = 1; (i < (*numOfProc)) || (i < *numberOfPictures); i++) {
		MPI_Send(&allPics[i - 1]->id, 1, MPI_INT, i, TAG_FIRST_LOAD,
				MPI_COMM_WORLD);
		MPI_Send(&allPics[i - 1]->dim, 1, MPI_INT, i, TAG_FIRST_LOAD,
				MPI_COMM_WORLD);
		for (int j = 0; j < (allPics[i - 1]->dim); j++) {
			MPI_Send(allPics[i - 1]->pic[j], (allPics[i - 1]->dim), MPI_INT, i,
					TAG_FIRST_LOAD, MPI_COMM_WORLD);
		}
	}
}

int processPicture(int *posX, int *posY, Picture *picture, Object *obj,
		double matchingValue) {
	int loopCounter = (picture->dim) - (obj->dim) + 1;

	if ((picture->dim) < (obj->dim)) {
		return -1;
	}
	double accumulatedDiff = 0.0;
	
	for (int i = 0; i < loopCounter; i++) {
		for (int j = 0; j < loopCounter; j++) {
			//between pos(i,j) to pos(k,l) (k!=i, j!=l) accumulatedDiff=0
			accumulatedDiff = 0;
			for (int n = i; n < i + (obj->dim); n++) {
				for (int o = j; o < j + (obj->dim); o++) {
					accumulatedDiff += (abs(
							picture->pic[n][o] - obj->obj[n - i][o - j]))
							/ (double) (picture->pic[n][o]);
				}
				if(matchingValue < accumulatedDiff){
					break;
				}
				
			}
			if (matchingValue > accumulatedDiff) {
				*posX = i;
				*posY = j;
				return 1;
			}
		}

	}
	*posX = (-1);
	*posY = (-1);
	return 0;
}
int calculateRelativeDiff(int p, int o) {
	return abs((p - o) / p);
}
void sendApic(Picture ***allPics, int *pos, int *sender) {
//sends the picture in index to sender
	MPI_Send(&(allPics[0][*pos]->id), 1, MPI_INT, *sender, TAG_JOB_DATA,
			MPI_COMM_WORLD);
	MPI_Send(&(allPics[0][*pos]->dim), 1, MPI_INT, *sender, TAG_JOB_DATA,
			MPI_COMM_WORLD);

	for (int j = 0; j < (allPics[0][*pos]->dim); j++) {
		MPI_Send(allPics[0][*pos]->pic[j], (allPics[0][*pos]->dim), MPI_INT,
				*sender, TAG_JOB_DATA, MPI_COMM_WORLD);
	}
}

int getPicFromMaster(Picture **retPic, MPI_Status status) {
	retPic[0] = (Picture*) malloc(sizeof(Picture));
	if (!retPic[0]) {
		printf("malloc failed getPicFromMaster\n");
		exit(0);
	}
	MPI_Recv(&retPic[0]->id, 1, MPI_INT, 0, TAG_JOB_DATA, MPI_COMM_WORLD,
			&status);

	MPI_Recv(&retPic[0]->dim, 1, MPI_INT, 0, TAG_JOB_DATA, MPI_COMM_WORLD,
			&status);

	retPic[0]->pic = (int**) malloc(sizeof(int*) * (retPic[0]->dim));
	if (!retPic[0]->pic) {
		printf("malloc failed getPicFromMaster\n");
		exit(0);
	}

	for (int j = 0; j < (retPic[0]->dim); j++) {
		retPic[0]->pic[j] = (int*) malloc(sizeof(int) * (retPic[0]->dim));
		if (!retPic[0]->pic[j]) {
			printf("malloc failed getPicFromMaster\n");
			exit(0);
		}
		MPI_Recv(retPic[0]->pic[j], (retPic[0]->dim), MPI_INT, 0, TAG_JOB_DATA,
				MPI_COMM_WORLD, &status);
	}
	
	return 1;

}

void sendAllObjects(int *numberOfObjects, Object **allObjects) {
	MPI_Bcast(numberOfObjects, 1, MPI_INT, 0, MPI_COMM_WORLD);
	for (int i = 0; i < *numberOfObjects; i++) {
		MPI_Bcast(&(allObjects[i]->id), 1, MPI_INT, 0, MPI_COMM_WORLD);
		MPI_Bcast(&(allObjects[i]->dim), 1, MPI_INT, 0, MPI_COMM_WORLD);
		for (int j = 0; j < (allObjects[i]->dim); j++) {
			MPI_Bcast(allObjects[i]->obj[j], (allObjects[i]->dim), MPI_INT, 0,
					MPI_COMM_WORLD);
		}
	}
}
void recvAllObjects(int *numberOfObjects, Object ***allObjects) {
	MPI_Bcast(numberOfObjects, 1, MPI_INT, 0, MPI_COMM_WORLD);
	allObjects[0] = (Object**) malloc(sizeof(Object*) * (*numberOfObjects));
	if (!allObjects) {
		printf("malloc failed recv all objects1\n");
		exit(0);
	}
	for (int i = 0; i < *numberOfObjects; i++) {
		allObjects[0][i] = (Object*) malloc(sizeof(Object));
		if (!allObjects[0][i]) {
			printf("malloc failed recv all objects2\n");
			exit(0);
		}
		MPI_Bcast(&(allObjects[0][i]->id), 1, MPI_INT, 0, MPI_COMM_WORLD);
		MPI_Bcast(&(allObjects[0][i]->dim), 1, MPI_INT, 0, MPI_COMM_WORLD);

		allObjects[0][i]->obj = (int**) malloc(
				sizeof(int*) * allObjects[0][i]->dim);
		if (!allObjects[0][i]->obj) {
			printf("malloc failed recv all objects\n3");
			exit(0);
		}

		for (int j = 0; j < allObjects[0][i]->dim; j++) {
			allObjects[0][i]->obj[j] = (int*) malloc(
					(sizeof(int) * (allObjects[0][i]->dim)));
			if (!allObjects[0][i]->obj[j]) {
				printf("malloc failed recv all objects4 \n");
				exit(0);
			}
		}

		for (int j = 0; j < (allObjects[0][i]->dim); j++) {
			MPI_Bcast(allObjects[0][i]->obj[j], (allObjects[0][i]->dim),
					MPI_INT, 0, MPI_COMM_WORLD);
		}
	}
}

void readFromFile(double *matchingValue, int *numberOfPictures,
		int *numberOfObjects, Object ***allObjects, Picture ***allPics,
		char *fileName) {
	FILE *fp;
	if ((fp = fopen(fileName, "r")) == 0) {
		printf("cannot open file %s for reading\n", fileName);
		exit(0);
	}
//matching value
	fscanf(fp, "%lf", matchingValue);
	printf("%lf\n", *matchingValue);
//number of pictures
	fscanf(fp, "%d", numberOfPictures);
	printf("%d\n", *numberOfPictures);
	allPics[0] = (Picture**) malloc(sizeof(Picture*) * (*numberOfPictures));
	if (!allPics[0]) {
		printf("error malloc readFromFile all pictures\n");
		exit(0);
	}
	for (int i = 0; i < (*numberOfPictures); i++) {
		allPics[0][i] = (Picture*) malloc(sizeof(Picture));
		if (!allPics[0][i]) {
			printf("error malloc readFromFile all pictures[i]\n");
			exit(0);
		}
//read id
		fscanf(fp, "%d", &allPics[0][i]->id);
//read dim
		fscanf(fp, "%d", &allPics[0][i]->dim);
//init matrix
		allPics[0][i]->pic = (int**) malloc(sizeof(int*) * allPics[0][i]->dim);
		if (!allPics[0][i]->pic) {
			printf("error malloc readFromFile picture[i]->matrix\n");
			exit(0);
		}
		for (int k = 0; k < allPics[0][i]->dim; k++) {
			allPics[0][i]->pic[k] = (int*) malloc(
					sizeof(int) * allPics[0][i]->dim);
			if (!allPics[0][i]->pic[k]) {
				printf("error malloc readFromFile picture[i][k]\n");
				exit(0);
			}
			for (int t = 0; t < allPics[0][i]->dim; t++) {
				fscanf(fp, "%d", &allPics[0][i]->pic[k][t]);
			}
		}
	}
//read objects
//number of objects
	fscanf(fp, "%d", numberOfObjects);
	allObjects[0] = (Object**) malloc(sizeof(Object*) * (*numberOfObjects));
	if(!allObjects[0]){
		printf("error malloc readFromFile picture[i][k]\n");
		exit(0);
		
	}
	for (int i = 0; i < (*numberOfObjects); i++) {
		allObjects[0][i] = (Object*) malloc(sizeof(Object));
		if (!allObjects[0][i]) {
			printf("error malloc readFromFile all objects[i]\n");
			exit(0);
		}
//read id
		fscanf(fp, "%d", &(allObjects[0][i]->id));
//read dim
		fscanf(fp, "%d", &(allObjects[0][i]->dim));
//init matrix
		allObjects[0][i]->obj = (int**) malloc(
				sizeof(int*) * (allObjects[0][i]->dim));
		if (!allObjects[0][i]->obj) {
			printf("error malloc readFromFile obj[i]->matrix\n");
			exit(0);
		}
		for (int k = 0; k < (allObjects[0][i]->dim); k++) {
			allObjects[0][i]->obj[k] = (int*) malloc(
					sizeof(int) * (allObjects[0][i]->dim));
			if (!allObjects[0][i]->obj[k]) {
				printf("error malloc readFromFile obj[i][k]\n");
				exit(0);
			}
			for (int t = 0; t < (allObjects[0][i]->dim); t++) {
				fscanf(fp, "%d", &allObjects[0][i]->obj[k][t]);
			}
		}
	}
	fclose(fp);
}

void freeAllObjects(Object ***allObjects, int *numberOfObjects) {
	if (allObjects[0] != NULL) {
		for (int i = 0; i < (*numberOfObjects); i++) {
			if (allObjects[0][i] != NULL) {
				for (int j = 0; j < allObjects[0][i]->dim; j++) {
					free(allObjects[0][i]->obj[j]);
				}
			}
			free(allObjects[0][i]->obj);
			free(allObjects[0][i]);
		}
		free(allObjects[0]);
	}
}

void freeAllPictures(Picture ***allPictures, int *numberOfPictures) {
	if (allPictures[0] != NULL) {
		for (int i = 0; i < (*numberOfPictures); i++) {
			freeApic(&allPictures[0][i]);
			free(allPictures[0][i]->pic);
			free(allPictures[0][i]);
		}
		free(allPictures[0]);
	}
}

void freeApic(Picture **picture) {
	if (picture[0] != NULL) {
		for (int j = 0; j < picture[0]->dim; j++) {
			free(picture[0]->pic[j]);
		}
	}
}
