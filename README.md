# Pictures-Recognition-Alg.
A parallel and distributed pictures recognition algorithm using OpenMP and MPI

# Features & Strategy

 - [x]  Using Open Multi-Processing interface (OpenMP) and Message Passing Interface (MPI).
 - [x]  Avg. runtime using 2 computers and 3 processes (where one is the master) is 1-2 seconds.
 - [x]  About the implementation strategy:
 
 I chose the classic master-slave method for this project.
 Master process first reads and initializes picture list object list and matching value from "input.txt" text file.
 The master sends each slave the object list and matching value for later computing.
 Slaves communicate with master process using MPI_Probe, that way they can declare their intentions whether it is for getting
 new task from master's task pool (asking for a new picture to work on) or whether it's to notify master that they finished
 last task they handled.
 Slaves task is- get a picture from master if there are any left in pool, search objects from object list in this picture,
 first object to be found (according to the predetermined calculation) is sent back to the master (object no. x found in
 picture no. y in indexes(a,b)).
 If no object was found notify master and ask for another job.
 All slaves searches objects in picture using parallelization of omp.
 Master sends each free process a picture and retrieves information about result and prints whatever's relevant into
 "output.txt" text file. If there are any tasks free, free processes will get a new task to work on.
 Once all the slaves finish their jobs, each process including master frees memory allocated for pictures (only master) and for
 object list (everyone).
 
- [x] Hopefully my implementation will meet your expectations, I worked very hard on it and I'm happy to say that I am satisfied
with the result. First my implementation ran 4.5 minutes, then I improved it and it ran 1.5 minutes, improved it again and got
to 22 seconds and finally I was able to get it to this point – less than 1 second on 2 computers.



