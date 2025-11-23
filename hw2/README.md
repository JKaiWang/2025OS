# OS Lab2 — Mini Shell Implementation  
NCKU CSIE 2025

This project implements a simplified Unix shell that supports:

- Built-in commands (`cd`)
- Execution of external programs
- Input/output redirection (`<`, `>`)
- Pipelines (`|`)

Requirements 2.1–2.4 are documented below.

---

# 2.1 Built-in Command: `cd`

## Overview
`cd` is implemented as a built-in command because directory changes must occur inside the shell process itself.

## Behavior
- `cd <path>` changes to the specified directory.
- `cd` with no argument changes to `$HOME`.
- All errors are printed using `perror("cd")`.

## Code

```c
int cd(char **args)
{
    if (args[1] == NULL) {
        char *home = getenv("HOME");
        if (chdir(home) != 0)
            perror("cd");
        return 1;
    }

    if (chdir(args[1]) != 0)
        perror("cd");

    return 1;
}
```

---

# 2.2 Executing External Commands (`spawn_proc`)

## Overview
External commands are executed with the following sequence:

1. `fork()` a child process  
2. Child calls `redirection()`  
3. Child executes the program using `execvp()`  
4. Parent waits for the child  

## Code

```c
int spawn_proc(struct cmd_node *p)
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        redirection(p);
        execvp(p->args[0], p->args);
        perror("execvp");
        exit(1);
    }

    int status;
    waitpid(pid, &status, 0);
    return 1;
}
```

---

# 2.3 Input/Output Redirection (`<`, `>`)

## Overview
Redirection is handled with `open()` and `dup2()`:

- `< infile` → set `STDIN`
- `> outfile` → set `STDOUT`
- Also integrates with pipe in/out (for 2.4)

## Code

```c
void redirection(struct cmd_node *p)
{
    if (p->in_file != NULL) {
        int fd = open(p->in_file, O_RDONLY);
        if (fd < 0) { perror("open infile"); exit(1); }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (p->out_file != NULL) {
        int fd = open(p->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) { perror("open outfile"); exit(1); }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    if (p->in != 0) {
        dup2(p->in, STDIN_FILENO);
        close(p->in);
    }
    if (p->out != 1) {
        dup2(p->out, STDOUT_FILENO);
        close(p->out);
    }
}
```

---

# 2.4 Pipeline (`|`) — Bonus

## Overview
Pipelines are implemented using a linked list of `cmd_node`s.

For a command such as:

```
cmd1 | cmd2 | cmd3
```

- Each command is stored in its own `cmd_node`
- `pipe_num` indicates how many commands exist
- A pipe is created between each adjacent pair
- Each command runs in its own child process

## Code

```c
int fork_cmd_node(struct cmd *cmd)
{
    int num = cmd->pipe_num;
    struct cmd_node *cur = cmd->head;

    int pipefd[num - 1][2];

    for (int i = 0; i < num - 1; i++)
        pipe(pipefd[i]);

    int pidx = 0;

    while (cur != NULL) {

        pid_t pid = fork();

        if (pid < 0) { perror("fork"); exit(1); }

        if (pid == 0) {

            if (pidx != 0)
                cur->in = pipefd[pidx - 1][0];

            if (pidx != num - 1)
                cur->out = pipefd[pidx][1];

            redirection(cur);

            for (int i = 0; i < num - 1; i++) {
                close(pipefd[i][0]);
                close(pipefd[i][1]);
            }

            execvp(cur->args[0], cur->args);
            perror("execvp");
            exit(1);
        }

        cur = cur->next;
        pidx++;
    }

    for (int i = 0; i < num - 1; i++) {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }

    int status;
    for (int i = 0; i < num; i++)
        wait(&status);

    return 1;
}
```

---

# Examples

## Redirection
```
$ cat demo.txt > out.txt
$ cat out.txt
```

## Single Pipe
```
$ cat demo.txt | grep os
```

## Multiple Pipes
```
$ cat demo.txt | grep Day | wc -l
```

---

# Summary

| Requirement | Description | Status |
|------------|-------------|--------|
| **2.1** | Built-in `cd` | ✔ Completed |
| **2.2** | External commands (`fork` + `execvp`) | ✔ Completed |
| **2.3** | Redirection `<` `>` | ✔ Completed |
| **2.4** | Pipeline `|` | ★ Bonus Completed |

This shell now supports built-ins, external commands, redirection, and pipelines.

---

# Build and Run

```
make
./my_shell
```

---

# File Structure

```
/include
    builtin.h
    command.h
    shell.h

/src
    builtin.c
    command.c
    shell.c

demo.txt
my_shell.c
makefile
```

---

