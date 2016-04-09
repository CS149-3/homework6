#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the total number of pipe pairs
#define NUM_PIPES	 	5

// size we will send and receive through pipes
#define BUFFER_SIZE 100

long get_current_time() {
	/* acquire and return the current time in milliseconds */
	return 0;
}

void iterative_process(int process, int pipe, long start_time) {
	/* example, replace this */

	// sleep 3 seconds
	sleep(3);
	// write buffer variable
	char writebuffer[BUFFER_SIZE];
	// loop through 5 iterative messages on a sleep cycle of 2
	for (int i = 0; i < 5; i++) {
		// set writebuffer to all null characters (this is important when printing the buffer)
		memset(writebuffer, '\0', sizeof(writebuffer));
		// print formatted string to the buffer
		snprintf(writebuffer, sizeof(writebuffer), "%lo: Child %d message %d", get_current_time() - start_time, process, i);
		// write buffer to pipe
		write(pipe, writebuffer, strlen(writebuffer));
		// sleep 2 seconds before repeating
		sleep(2);
	}
}

void stdin_process(int process, int pipe, long start_time) {
	/* example, replace this; this example does not get anything from stdin and mostly identical to iterative_process */

	// sleep 2 seconds
	sleep(2);
	// write buffer variable
	char writebuffer[BUFFER_SIZE];
	// loop through 5 iterative messages on a sleep cycle of 2
	for (int i = 0; i < 5; i++) {
		// set writebuffer to all null characters (this is important when printing the buffer)
		memset(writebuffer, '\0', sizeof(writebuffer));
		// print formatted string to the buffer
		snprintf(writebuffer, sizeof(writebuffer), "%lo: Child %d message %d", get_current_time() - start_time, process, i);
		// write buffer to pipe
		write(pipe, writebuffer, strlen(writebuffer));
		// sleep 2 seconds before repeating
		sleep(2);
	}
}

int main(int argc, char const *argv[]) {

	pid_t pid;
	pid_t child_pids[5];
	int pipes[2 * NUM_PIPES];

	long start_time = get_current_time();

	// create pipes
	for(int i = 0; i < NUM_PIPES; i++) {
		if(pipe(pipes+(i * 2))) {
			fprintf(stderr, "Pipe failed.\n");
			return EXIT_FAILURE;
		}
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
			if (!running_children) return EXIT_SUCCESS;

			// set timemout to 5 seconds
			tv.tv_sec = 5;
			tv.tv_usec = 0;

			// get return value of select
			// retval = select(20, &rfds, NULL, NULL, &tv);
			retval = select(pipes[(NUM_PIPES * 2 - 1)] + 1, &rfds, NULL, NULL, &tv);

			// if the return value is -1, error
			if (retval == -1)
				perror("Select failed.");
			// any other non-zero means a file is readable
			else if (retval) {
				// will remove; debug output to visibly see different select cycles
				printf("Data is available now.\n");
				// loop through pipes to find the readable ones
				for (int i = 0; i < NUM_PIPES * 2; i += 2) {
					// FD_ISSET returns non-zero when the FD is readable
					if (FD_ISSET(pipes[i], &rfds)) {
						char readbuffer[BUFFER_SIZE];
						memset(readbuffer, '\0', sizeof(readbuffer));
						read(pipes[i], readbuffer, sizeof(readbuffer)-1);
						if (readbuffer[0] != '\0') printf("%lo %s\n", get_current_time() - start_time, readbuffer);
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

	// return at end of process
	return EXIT_SUCCESS;
}
