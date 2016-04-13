#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

// the total number of pipe pairs
#define NUM_PIPES	 	5

// size we will send and receive through pipes
#define BUFFER_SIZE 1000

// filename for output file
const char OUTPUT_NAME[] = "output.txt";

double get_current_time() {
	/* acquire and return the current time in milliseconds */
	struct timeval current;
	long mtime, seconds, useconds;

	gettimeofday(&current, NULL);

	seconds  = current.tv_sec;
	useconds = current.tv_usec;

	mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
	return mtime / 1000.0;
}

void iterative_process(int process, int pipe, double start_time) {

	// write buffer variable
	char writebuffer[BUFFER_SIZE];

	// loop until 30 seconds has passed then process will break out
	for (int i = 0;; i++)
	{
		// set writebuffer to all null characters (this is important when printing the buffer)
		memset(writebuffer, '\0', sizeof(writebuffer));

		// print formatted string to the buffer
		snprintf(writebuffer, sizeof(writebuffer), "%5.3lf: Child %d message %d, ",
							get_current_time() - start_time, process, i);

		// write buffer to pipe
		write(pipe, writebuffer, strlen(writebuffer));

		//sleep for a random time of 0, 1, 2 seconds between messages
		int i = rand()%3;
		sleep(i);

		//Determine how long process has been running. if >= 30 sec. break out
		double running = get_current_time() - start_time;
		if(running >= 30.0)
			break;
	}
}

void stdin_process(int process, int pipe, double start_time) {
	// write buffer variable
	char writebuffer[BUFFER_SIZE];
	while(1){
		// set writebuffer to all null characters (this is important when printing the buffer)
		memset(writebuffer, '\0', sizeof(writebuffer));
		printf("Enter user input: ");
		char userInput[100];
		fgets(userInput, 100, stdin);
		userInput[strcspn(userInput, "\r\n")] = '\0'; // just cleaning newline
		// print formatted string to the buffer
		snprintf(writebuffer, sizeof(writebuffer), "%5.3lf: Child %d message: %s, ", get_current_time() - start_time, process, userInput);

		// write buffer to pipe
		write(pipe, writebuffer, strlen(writebuffer));
		if(get_current_time() - start_time >= 30.0)
			break;
	}
}

int main(int argc, char const *argv[]) {

	pid_t pid;
	pid_t child_pids[5];
	int pipes[2 * NUM_PIPES];

	double start_time = get_current_time();

	// create pipes
	for(int i = 0; i < NUM_PIPES; i++) {
		if(pipe(pipes+(i * 2))) {
			fprintf(stderr, "Pipe failed.\n");
			return EXIT_FAILURE;
		}
	}

	//creaing writable file connection
	FILE *output;
	char *mode = "w";
	output = fopen(OUTPUT_NAME, mode);

	// checks for file connection error
	if(output == NULL){
		fprintf(stderr, "Can't open output file %s! \n", OUTPUT_NAME);
		return EXIT_FAILURE;
	}

	// this will let us set the process number for the child before it forks
	int process;

	// create child processes
	for (int i = 0; i < 5; i++){
		// assign process number for child
		process = i;

		// fork child
		pid = fork();

		// print error if fork fails
		if(pid < 0) {
			fprintf(stderr, "Fork failed.");
			return EXIT_FAILURE;
		}

		// if child break out of this loop
		if(pid == 0) break;
		// if parent, add the child pid to our array
		else child_pids[i] = pid;
	}

	// children
	if(pid == (pid_t) 0) {
		// assign random seed
		srand(time(NULL) ^ (getpid()<<16));

		// assign read and write pipe variables
		int read_pipe = pipes[process * 2];
		int write_pipe = pipes[(process * 2) + 1];

		// close read for process pipe
		close(read_pipe);

		// children 1 - 4
		if(process < 4) {
			iterative_process(process, write_pipe, start_time);
		}

		// child 5 (stdin)
		else {
			stdin_process(process, write_pipe, start_time);
		}

		// close write pipe and return
		close(write_pipe);
		return EXIT_SUCCESS;
	}

	// parent
	else if(pid > (pid_t) 0) {
		// close write pipes for parent
		for (int i = 0; i < NUM_PIPES; i += 2) close(pipes[i + 1]);

		// select loop
		while(1){
			// select variables
			fd_set rfds;
			struct timeval tv;
			int retval;

			// add pipes to set
			FD_ZERO(&rfds);
			int running_children = 0;
			for (int i = 0; i < 5; i++) {
				// check if child is still alive before adding pipe
				int status;
				pid_t result = waitpid(child_pids[i], &status, WNOHANG);
				// if result is 0, child is still alive; add pipe
				if (result == 0) {
					FD_SET(pipes[i * 2], &rfds);
					running_children++;
				}
			}

			// check if no children left running
			if (!running_children) break;

			// set timemout to 5 seconds
			tv.tv_sec = 5;
			tv.tv_usec = 0;

			// get return value of select
			// retval = select(20, &rfds, NULL, NULL, &tv);
			retval = select(pipes[(NUM_PIPES * 2 - 1)] + 1, &rfds, NULL, NULL, &tv);

			// if the return value is -1, error
			if (retval == -1)
				fprintf(stderr, "Select failed.");
			// any other non-zero means a file is readable
			else if (retval) {
				// will remove; debug output to visibly see different select cycles
				// printf("Data is available now.\n");
				// loop through pipes to find the readable ones
				for (int i = 0; i < NUM_PIPES * 2; i += 2) {
					// FD_ISSET returns non-zero when the FD is readable
					if (FD_ISSET(pipes[i], &rfds)) {
						char readbuffer[BUFFER_SIZE];
						memset(readbuffer, '\0', sizeof(readbuffer));
						read(pipes[i], readbuffer, sizeof(readbuffer)-1);
						//writes to output file
						if (readbuffer[0] != '\0') fprintf(output, "%5.3lf %s\n", get_current_time() - start_time, readbuffer);
					}
				}
			}
			// else nothing was readable by the timeout period
			else {
				printf("No data within five seconds.\n");
			}
		}
	}
	// fork error
	else {
		fprintf(stderr, "Fork failed.");
		return EXIT_FAILURE;
	}

	//let user know that file is ready
	printf("Output file %s is ready!\n", OUTPUT_NAME);

	//close file connection
	fclose(output);

	// return at end of process
	return EXIT_SUCCESS;
}
