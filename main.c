// hunter egeland & austin chandler
// project 2: pager - a virtual memory manager
// ricardo citro
// cst-315

// the goal of this project was to add some building blocks of an            algorithm for a virtual memory manager to our already existing            unix/linux shell.

#include <stdio.h>
#include <alloca.h>
#include <string.h>
#include <stdlib.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <sys/utsname.h>

// define files for address and backing store for memory management
FILE *addresses;
FILE *backing_store;

// define bool type enum
typedef enum {
    false,
    true
} bool;

struct dummyProcess {
    int id;
    int state;
    int calculationsRemaining;
    /*
    0. NEW
    1. READY
    2. WAITING
    3. RUNNING
    4. TERMINATED
    */
};

// define buffer constant as 64
#define BUFFER 64
// define max number of letters supported for user input
#define MAXCOM 1000
// define max number of commands supported for user input
#define MAXLIST 100
// define buffer size for reading a line
#define BUFF_SIZE 10
// define mask for all of logical_address except the address
#define ADDRESS_MASK 0xFFFF
// define mask for the offset
#define OFFSET_MASK 0xFF
// 16 entries in the translation lookaside buffer
#define TLB_SIZE 16
// page table of size 128
#define PAGE_TABLE_SIZE 128
// upon page fault, read in 256-byte page from BACKING_STORE
#define PAGE 256 
// define size of each frame
#define FRAME_SIZE 256  

FILE *fptr;

int TLBEntries = 0; // number of translation lookaside buffer entries
int hits = 0; // counter for translation lookaside buffer hits
int faults = 0; // counter for page faults
int currPage = 0; // number of pages
int logical_address; // store logical address
int TLBpages[TLB_SIZE]; // page numbers in translation lookaside buffer
bool pagesRef[PAGE_TABLE_SIZE]; // reference bits for page numbers in translation lookaside buffer
int pageTableNums[PAGE_TABLE_SIZE]; // page numbers in page table
char currAddress[BUFF_SIZE]; // addresses
signed char fromBS[PAGE]; // reads from BACKING_STORE
signed char byte; // value of physical memory at frame number/offset
int physMem[PAGE_TABLE_SIZE][FRAME_SIZE]; // physical memory array of 32,678 bytes

void getPage(int logicaladdress);
int backingStore(int pageNum);
void TLBInsert(int pageNum, int frameNum);

char *read_command(void); // function to read the command
char **parse_line(char *line); // function to pase through the lines
int execute(char **arguments); // function to execute arguments
int status; // status variable

char *comm = NULL; // command variable
char **args; // arguments variable
int on; // status variable

// functions for short-term scheduler portion
void runInterrupt();
int runDummyProcess(struct dummyProcess *process);
void scheduler(int Nprocesses, int tQNum, int printDetailed);

// variables for short-term scheduler portion
struct dummyProcess *processes;
int tQuantum;
int numProcesses;
int interrupt = 0; // 0 no interrupt, 1 interrupt

/*
----------------------------------------------------------------------------------------------------------------------------------------------------
--------------------------------------------------------------------------
*/

// function to read the command
char *read_command(void) {
//   char *comm = NULL;  // initialize command variable as NULL
//   ssize_t buffer = 0; // initialize the buffer to 0;
//   //getline(&comm, &buffer, stdin); // get user input
//   scanf("%[^\n]", comm); // Get user input for command
//   return comm; // return comm variable
    char *comm; // Define string char array
    char temp; // Define temporary variable for clearing buffer

    comm = malloc(200 * sizeof(char)); // Set str char array to 200
    scanf("%[^\n]", comm); // Get user input for command
    scanf("%c", &temp); // Clear buffer
    fflush(stdin); // Flush standard input
    fflush(stdout); // Flush standard ouput
    return comm;
}

/*
----------------------------------------------------------------------------------------------------------------------------------------------------
--------------------------------------------------------------------------
*/


// function to parse through the input command
char **parse_line(char *line) // 
{
  int buffer = BUFFER; // set temp buffer equal to the constant buffer from above
  int pos = 0; // initialize position as 0
  char **tokens = malloc(buffer * sizeof(char*)); // allocate memory to the buffer times the size of the input char
  char *token; // define token pointer
  if (!tokens) { // if there is no tokens
    printf("Error allocating memory with malloc\n"); // error message
    exit(0); // exit the program
  }
  token = strtok(line, " \t\r\n\a"); // create tokens from the string input into the parse function
  while(token != NULL) { // while the token isn't NULL, i.e. stepping through all the tokens
    tokens[pos] = token; // add the token to the tokens array
    pos++; // increment the position

    if (pos >= buffer) { // if the position is greater than or equal to the buffer
      buffer += BUFFER; // add the constant BUFFER from above onto the buffer variable declared in this function
      tokens = realloc(tokens, buffer * sizeof(char*)); // reallocate and resize the memory block of the tokens in the tokens array 
      if (!tokens) { // if there's no tokens
        printf("Error reallocating memory!\n"); // error message
        exit(0); // exit the program
      }
    }
    token = strtok(NULL, " \t\r\n\a"); // create tokens from the string input into the parse function if the position is less than the buffer
  }
  
  return tokens; // return the tokens
}


/*
----------------------------------------------------------------------------------------------------------------------------------------------------
--------------------------------------------------------------------------
*/

int execute(char **arguments) // function to execute commands based on arguments
{
  int pid, waitPid, status; // variables for the program id, the wait function, and status
  pid = fork(); // this creates the parent and child processes, which run the same program after being instructed

  if(pid == 0) { // child process
    if (execvp(arguments[0], arguments) == -1) // execvp() takes control of the program, running the command instead, and if it returns -1 it means the command failed
    perror("Error with EXECVP\n"); // error message
  } else if (pid < 0) { // if program id is less than 0 (error)
    perror("Error PID < 0\n"); // error message
  } else { // parent process
    do {
      waitPid = waitpid(pid, &status, WUNTRACED); // the waitpid() function suspends execution of the process until the child changes state.
    } while (!WIFEXITED(status) && !WIFSIGNALED(status)); // WIFEXITED is true if the waitpid() function exits normally, otherwise it gives false. WIFSIGNALED() returns true if the child process signaled and did not exit normally.
  }
  return 1;
}

/*
----------------------------------------------------------------------------------------------------------------------------------------------------
--------------------------------------------------------------------------
*/

// help command
int open_help() {
  // print out all available functions
  printf("Available functions are:\n"
  "1. exit\n"
  "2. df\n"
  "3. help\n"
  "4. hello\n"
  "5. getdir\n"
  "6. top\n"
  "7. du\n"
  "8. clear\n"
  "9. whoami\n"
  "10. ls\n"
  "11. sproc\n");
  
  return 0;
}

/*
----------------------------------------------------------------------------------------------------------------------------------------------------
--------------------------------------------------------------------------
*/

// runs input commands
int command_handler(char* comm)
{
  int numCommands = 5, i, switchArg = 0; // number of commands and swithc argument initialization
  char* commandList[numCommands]; // command list array
  char* username; // username string
  char path[BUFFER]; // path string
  char dirName[10]; // directory name string
  char **parsed = parse_line(comm); // parsed line chunks array
  int x, y, z;

  // list of custom commands
  commandList[0] = "exit";
  commandList[1] = "help";
  commandList[2] = "hello";
  commandList[3] = "getdir";
  commandList[4] = "sproc";

  // loop to determine which case
  for (i = 0; i < numCommands; i++) {
    if (strcmp(parsed[0], commandList[i]) == 0) {
      switchArg = i + 1;
      break;
    }
  }

  // case choices
  switch (switchArg) {
  case 1: // exit()
    printf("\nProgram has been exited.\n");
    exit(0);
  case 2: // help()
    open_help();
    return 1;
  case 3: // hello()
    username = getenv("USER");
    printf("\nHello %s.\nUse help to know more..\n", username);
    return 1;
  case 4: // getdir()
    getcwd(path, BUFFER); // get the directory, set it equal to path
    printf(path); // print the directory
    printf("\n");
    return 1;
 
    printf("'");
    printf(parsed[0]);
    printf("'");
    printf(" is not a valid command.\n");
    break;
  case 5:
	scheduler(8, 4, 0);
  }
  
  return 0;
} 

/*
----------------------------------------------------------------------------------------------------------------------------------------------------
--------------------------------------------------------------------------
*/

// is the input a valid command, same as command handler, just checks if a custom command is read by seeing if this returns 1 or 0. 1 being yes 0 being no.
int valid_command(char* comm) {
  int numCommands = 5, i, switchArg = 0;
  char* commandList[numCommands];
  char* username;
  char** parsed = parse_line(comm);

  
  commandList[0] = "exit";
  commandList[1] = "help";
  commandList[2] = "hello";
  commandList[3] = "getdir";
  commandList[4] = "sproc";
  
  for (i = 0; i < numCommands; i++) {
    if (strcmp(parsed[0], commandList[i]) == 0) {
      switchArg = i + 1;
      break;
    }
  }
  
  switch (switchArg) {
  case 1:
    return 1;
  case 2:
    return 1;
  case 3:
    return 1;
  case 4:
    return 1;
  case 5:
    return 1;
  case 6:
    return 1;

  default:
    break;
  }
  
  return 0;
}

/*
----------------------------------------------------------------------------------------------------------------------------------------------------
--------------------------------------------------------------------------
*/

// function to get arguments
int do_all() {
  comm = read_command(); // get user input
    if (valid_command(comm) == 1) {
      command_handler(comm); //parse_line(comm));
      return 0;
    } else {
      if (strcmp(comm, "\n") == 0) { // if the input is empty, error
        perror("Please type a command. "); // error message
      }
  
        int buffer = BUFFER; // set buffer
        char **tokens = malloc(buffer * sizeof(char*)); // allocate memory to the tokens
        tokens = parse_line(comm);  // set the tokens array equal to the parse_line return  when the commands are entered into it.

        // debug print command
        /*printf("command: ");
        printf(tokens[0]);
        printf("\n");*/

       execute(tokens); // execute the commands

    }
  return 0;
}

/*
----------------------------------------------------------------------------------------------------------------------------------------------------
--------------------------------------------------------------------------
*/

// get the page from the logical address
void getPage(int logical_address) {
  
	// initialize frameNum to -1
  // mask leftmost 16 bits & shift right 8 bits to extract page number      // offset is just the rightmost bits look through translation lookaside      buffer
  
  int frameNum = -1;
  int pageNum = ((logical_address & ADDRESS_MASK) >> 8);
  int offset = (logical_address & OFFSET_MASK);
    
	// if translation lookaside buffer hit extract frame number increase         number of hits
  
  for (int i = 0; i < TLB_SIZE; i++) {
    if (TLBpages[i] == pageNum) {
      frameNum = i;
      hits++;
    }
  }

	// if the frame number was not found in the translation lookaside            buffer:
    // loop through all the pages 
  // if page number found in page table:
    // extract change reference bit 
  // if frame number is -1
    // read from BACKING_STORE
    // increase the number of page faults
    // change frame number to first available frame number

  if (frameNum == -1) {
    for (int i = 0; i < currPage; i++) {
      if (pageTableNums[i] == pageNum) {
        frameNum = i;
        pagesRef[i] = true;
      }
    }

    if (frameNum == -1) {
      int count = backingStore(pageNum);
      faults++;
      frameNum = count;
    }
  }

	// insert page number and frame number into translation lookaside buffer
	// assign the value of the signed char to byte
	// output the virtual address, physical address and byte of the signed       char to the console

  TLBInsert(pageNum, frameNum);
  byte = physMem[frameNum][offset];
  printf("Virtual address: %d Physical address: %d Value: %d\n",       logical_address, (frameNum << 8) | offset, byte);
  
}

/*
----------------------------------------------------------------------------------------------------------------------------------------------------
--------------------------------------------------------------------------
*/

// read from backing store
int backingStore(int pageNum) {

  // initialize counter variable
  int counter = 0;

	// position to read from pageNum
	// read from beginning of file to find backing store
  
  if (fseek(backing_store, pageNum * PAGE, SEEK_SET) != 0) {
    fprintf(stderr, "Error seeking in backing store\n");
  }
  
  if (fread(fromBS, sizeof(signed char), PAGE, backing_store) == 0) {
    fprintf(stderr, "Error reading from backing store\n");
  }

	// search until specific page is found
	// if reference bit is 0: 
    // replace page
    // set search to false to end loop
	// else if reference bit is 1
    // set reference bit to 0

  bool search = true;
  
  while (search) {
    if (currPage == PAGE_TABLE_SIZE) {
      currPage = 0;
    }
    if (pagesRef[currPage] == false) {
      pageTableNums[currPage] = pageNum;
      search = false;
    } else {
      pagesRef[currPage] = false;
    }
    currPage++;
  }
  
    // load the contents into physical memory
    for (int i = 0; i < PAGE; i++) {
      physMem[currPage - 1][i] = fromBS[i];
    }

    // set counter to previous page and return
    counter = currPage - 1;
    return counter;
}

/*
----------------------------------------------------------------------------------------------------------------------------------------------------
--------------------------------------------------------------------------
*/

// insert the page into the translation lookaside buffer (TLB)
void TLBInsert(int pageNum, int frameNum) {
	
	// search for the entry in the translation lookaside buffer
  
  int i;
  for (i = 0; i < TLBEntries; i++) {
    if (TLBpages[i] == pageNum) {
      break;
    }
  }

	// if the number of entries is equal to the index:
    // if the TLB is not full
		  // insert the page with FIFO replacement
		// else:
			// shift everything over
			// replace using first in first out
		// else the number of entries is not equal to the index:
			// loop over everything to move the number of entries back 1
			// if still room in TLB:
				// insert page at the end
			// else if TLB is full:
				// place page at number of entries - 1
  
  if (i == TLBEntries) {
    if (TLBEntries < TLB_SIZE) {
      TLBpages[TLBEntries] = pageNum;
    }
    else {
      for (i = 0; i < TLB_SIZE - 1; i++) {
        TLBpages[i] = TLBpages[i + 1];
    }
      
      TLBpages[TLBEntries - 1] = pageNum;
    }
  } else {
      for (i = i; i < TLBEntries - 1; i++) {
        TLBpages[i] = TLBpages[i + 1];
      }
    
      if (TLBEntries < TLB_SIZE) { 
        TLBpages[TLBEntries] = pageNum;
      } else {
          TLBpages[TLBEntries - 1] = pageNum;
        }
    }

    // if TLB is still not full, increment the number of entries
    if (TLBEntries < TLB_SIZE) {
        TLBEntries++;
    }
}

/*
----------------------------------------------------------------------------------------------------------------------------------------------------
--------------------------------------------------------------------------
*/

// runner for the thread
void *runner(void *arg) {

  // convert arg to string and run it
  system((char *)arg);
	return NULL;
  
}

/*
----------------------------------------------------------------------------------------------------------------------------------------------------
--------------------------------------------------------------------------
*/

// grabs the file from the inputted value
void fromFile(char *fileName) {
	
	// create variables for: file name, chars in the line, length of the         list of chars, read length

  FILE *fp;
  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  // open the inputted file
	// while the file is open
		// split string by ;
		// create threads for each token
		// split the string by ; in each thread
		// join all the threads together

  fp = fopen(fileName, "r");

  while ((read = getline(&line, &len, fp)) != -1) {
    char *token = strtok(line, ";");
    pthread_t tid[10];
    int i = 0;

    while (token != NULL) {
      pthread_create(&tid[i++], NULL, runner, token);
      token = strtok(NULL, ";");
    }

    for (int j = 0; j < i; j++) {
      pthread_join(tid[j], NULL);
    }
  }

	// close the file 
	// free the line on the console

  fclose(fp);
  
  if (line) {
    free(line);
  }
}

/*
----------------------------------------------------------------------------------------------------------------------------------------------------
--------------------------------------------------------------------------
*/

// SHORT-TERM SCHEDULER PORTION

/*
--------------------------------------------------------------------------
*/

void runInterrupt() { // Simluation of I/O interrupt
  
  printf("i/o interrupt\n");
  sleep(1);
  interrupt = 0;
  
}

/*
--------------------------------------------------------------------------
*/

int runDummyProcess(struct dummyProcess *process) { // simulation of process doing calculations
  
  if (process -> calculationsRemaining > 0) {
    process -> calculationsRemaining--; // subtract from total
  }
  
  if (process -> calculationsRemaining <= 0) { // if the process is completed
    return 1;
  } else { // else the process not Completed
    return 0;
  }
  
}

/*
--------------------------------------------------------------------------
*/

void *printResults() { // information display
  
  printf("ID: STATE:\n");
  
  while(processes[numProcesses - 1].state != 4) {
    for (int i = 0; i < numProcesses; i++) {
      printf("%i %i\n", processes[i].id, processes[i].state);
    }
    printf("---------\n");
    sleep(1);
  }
  
  printf("All processes Completed!\n");
  for (int i = 0; i < numProcesses; i++) {
    printf("%i %i\n", processes[i].id, processes[i].state);        
  }

	return NULL;
  
}

/*
--------------------------------------------------------------------------
*/

void *printResultsDetailed() { // more detailed information display with amount of time running and calculations remaining
  
  time_t timeRunning;
  printf("ID: STATE: Calculations Remaining:\n");
  
  while(processes[numProcesses - 1].state != 4) {
    time(&timeRunning);
    printf("Time Elapsed: %li\n", timeRunning);
    
    for (int i = 0; i < numProcesses; i++) {
      printf("%i %i %i\n", processes[i].id, processes[i].state, processes[i].calculationsRemaining);
    }
    printf("---------\n");
    sleep(1);
  }
    
  printf("All processes Completed!\n");
  for (int i = 0; i < numProcesses; i++) {
    printf("%i %i\n", processes[i].id, processes[i].state);        
  }

	return NULL;
}

/*
--------------------------------------------------------------------------
*/

void *interruptInput() { // simple i/o simulation by user typing in console
  
  while(processes[numProcesses - 1].state != 4) {
    getchar();
    interrupt = 1;
  }

	return NULL;
  
}

/*
--------------------------------------------------------------------------
*/

void *RR() {
  
  // uses global dummyProcess struct array to simulate processes with states
    
  // declare variables
  int numRunning = 0;
  int numCompleted = 0;
  time_t timer, starttime;

  // set processes to default values
  for(int i = 0; i < numProcesses; i++) {
    processes[i].id = i; // gives each process an ID
    processes[i].state = 0; // sets default state to NEW
    processes[i].calculationsRemaining = 1000000; // sets a default amount of calculations for processes to use
  }

  while(numCompleted != numProcesses) { // while all processes not completed
    for (int i = 0; i < numProcesses; i++) {
      if (processes[i].state != 4) { // only run processes that are not completed
                
        // if process is NEW, set to READY
        if (processes[i].state == 0) {
          processes[i].state = 1;
        }
        
        // if process is READY and no processes are running, set to RUNNING
        if (processes[i].state == 1 && numRunning == 0) {
          processes[i].state = 3;
        }
        
        // WAIT FOR INTERRUPT IF THERE IS ONE
        if (interrupt) { // i/o interrupt
          processes[i].state = 2; // set to WAITING
          runInterrupt();
          if (numRunning == 0) { // if no other processes running, go directly to running state
            processes[i].state = 3;
          } else { // else go to ready state
            processes[i].state = 1;
          }
        }
        
        // if running state
        if (processes[i].state == 3) { 
          numRunning++;
          time(&starttime);
          do {
            time(&timer);
          } while (runDummyProcess(&processes[i]) == 0 && difftime(timer,starttime) <= tQuantum); // while runProcess is still returning not completed and time is not at tQuantum yet
                    
          // if process has finished processing, set it to terminated
          if (processes[i].calculationsRemaining <= 0) {
            processes[i].state = 4;
            numCompleted++;
          } else {
            processes[i].state = 1;
          }
        
          numRunning--;
        }
      }
    }
  }

	return NULL;
  
}

/*
--------------------------------------------------------------------------
*/

void scheduler(int Nprocesses, int tQNum, int printDetailed) { 
  
  // set dummy process variables
  numProcesses = Nprocesses;
  tQuantum = tQNum;
  processes = malloc(numProcesses * sizeof(struct dummyProcess));

  // create threads
  pthread_t thread, printThread, interruptThread;
  pthread_create(&thread, NULL, &RR, NULL);
    
  // set which print method depending on user input
  switch(printDetailed) {
    case 1:
      pthread_create(&printThread, NULL, &printResultsDetailed, NULL);
      break;     
    default:
      pthread_create(&printThread, NULL, &printResults, NULL);
      break;
    }
  
  // run interrupt simulation thread
  pthread_create(&interruptThread, NULL, &interruptInput, NULL);
    
  // join/cancel threads and free up memory
  pthread_join(thread, NULL);
  pthread_join(printThread, NULL);
  pthread_cancel(interruptThread); // needs to be canceled to break out of loop early
  free(processes);
  
}

/*
----------------------------------------------------------------------------------------------------------------------------------------------------
--------------------------------------------------------------------------
*/

int main() {

  do { // start the shell
    printf("lopeshell> "); // display the text the shell displays to show where to type your commands

    // get arguments
    do_all();

    // parse the line
	  parse_line(comm);
    
  } while(1); // loop infinitely while shell is on

}