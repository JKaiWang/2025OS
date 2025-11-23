#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "../include/command.h"
#include "../include/builtin.h"

// ======================= requirement 2.3 =======================
/**
 * @brief 
 * Redirect command's stdin and stdout to the specified file descriptor
 * If you want to implement ( < , > ), use "in_file" and "out_file" included the cmd_node structure
 * If you want to implement ( | ), use "in" and "out" included the cmd_node structure.
 *
 * @param p cmd_node structure
 * 
 */
void redirection(struct cmd_node *p)
{
    // 1. 處理 input redirection (< infile)
    if (p->in_file != NULL) {
        int fd = open(p->in_file, O_RDONLY);
        if (fd < 0) {
            perror("open infile");
            exit(1);
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            perror("dup2 infile");
            exit(1);
        }
        close(fd);
    }

    // 2. 處理 output redirection (> outfile)
    if (p->out_file != NULL) {
        int fd = open(p->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror("open outfile");
            exit(1);
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2 outfile");
            exit(1);
        }
        close(fd);
    }

    // 3. Pipe 用到的 in/out (p->in, p->out) —— 若不是預設，必須接上 pipe
    if (p->in != 0) {   // 如果 pipe 設定 stdin
        if (dup2(p->in, STDIN_FILENO) < 0) {
            perror("dup2 pipe in");
            exit(1);
        }
        close(p->in);
    }

    if (p->out != 1) {  // 如果 pipe 設定 stdout
        if (dup2(p->out, STDOUT_FILENO) < 0) {
            perror("dup2 pipe out");
            exit(1);
        }
        close(p->out);
    }
}

// ===============================================================

// ======================= requirement 2.2 =======================
/**
 * @brief 
 * Execute external command
 * The external command is mainly divided into the following two steps:
 * 1. Call "fork()" to create child process
 * 2. Call "execvp()" to execute the corresponding executable file
 * @param p cmd_node structure
 * @return int 
 * Return execution status
 */
int spawn_proc(struct cmd_node *p)
{
    pid_t pid = fork();

    // Fork failed
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    // Child process
    if (pid == 0) {

        // 先做 redirection (< > 或 pipe)
        redirection(p);

        // execvp：外部指令
        if (execvp(p->args[0], p->args) == -1) {
            perror("execvp");
            exit(1);   // child 必須結束
        }
    }

    // Parent process 等 child 做完
    int status;
    waitpid(pid, &status, 0);

    return 1;
}

// ===============================================================


// ======================= requirement 2.4 =======================
/**
 * @brief 
 * Use "pipe()" to create a communication bridge between processes
 * Call "spawn_proc()" in order according to the number of cmd_node
 * @param cmd Command structure  
 * @return int
 * Return execution status 
 */
int fork_cmd_node(struct cmd *cmd)
{
    int num = cmd->pipe_num;           // 指令數量
    struct cmd_node *cur = cmd->head;  // 從第一個 command 開始

    int pipefd[ num - 1 ][2 ];         // 為 N 個指令準備 N-1 個 pipe

    // Step 1: 建立所有 pipes
    for (int i = 0; i < num - 1; i++) {
        if (pipe(pipefd[i]) < 0) {
            perror("pipe");
            exit(1);
        }
    }

    int pidx = 0;  // pipe index
    while (cur != NULL) {

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(1);
        }

        // Child process
        if (pid == 0) {

            // 1. 若不是第一個 command → stdin 來自前一個 pipe
            if (pidx != 0) {
                cur->in = pipefd[pidx - 1][0];
            }

            // 2. 若不是最後一個 command → stdout 指向下一個 pipe
            if (pidx != (num - 1)) {
                cur->out = pipefd[pidx][1];
            }

            // 3. 做 redirection: 同時處理 < > 與 pipe in/out
            redirection(cur);

            // 4. 關掉 child 不需要的 pipe
            for (int i = 0; i < num - 1; i++) {
                close(pipefd[i][0]);
                close(pipefd[i][1]);
            }

            // 5. execvp 執行指令
            execvp(cur->args[0], cur->args);
            perror("execvp");
            exit(1);
        }

        // Parent: 下一個 command
        cur = cur->next;
        pidx++;
    }

    // Step 2: Parent 關掉所有 pipes
    for (int i = 0; i < num - 1; i++) {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }

    // Step 3: Parent 等所有 children 收屍
    int status;
    for (int i = 0; i < num; i++) {
        wait(&status);
    }

    return 1;
}

// ===============================================================


void shell()
{
	while (1) {
		printf(">>> $ ");
		char *buffer = read_line();
		if (buffer == NULL)
			continue;

		struct cmd *cmd = split_line(buffer);
		
		int status = -1;
		// only a single command
		struct cmd_node *temp = cmd->head;
		
		if(temp->next == NULL){
			status = searchBuiltInCommand(temp);
			if (status != -1){
				int in = dup(STDIN_FILENO), out = dup(STDOUT_FILENO);
				if( in == -1 | out == -1)
					perror("dup");
				redirection(temp);
				status = execBuiltInCommand(status,temp);

				// recover shell stdin and stdout
				if (temp->in_file)  dup2(in, 0);
				if (temp->out_file){
					dup2(out, 1);
				}
				close(in);
				close(out);
			}
			else{
				//external command
				status = spawn_proc(cmd->head);
			}
		}
		// There are multiple commands ( | )
		else{
			
			status = fork_cmd_node(cmd);
		}
		// free space
		while (cmd->head) {
			
			struct cmd_node *temp = cmd->head;
      		cmd->head = cmd->head->next;
			free(temp->args);
   	    	free(temp);
   		}
		free(cmd);
		free(buffer);
		
		if (status == 0)
			break;
	}
}
