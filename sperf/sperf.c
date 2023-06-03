#define _GNU_SOURCE 
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>

char* get_syscall(const char* line) {
  regex_t re;
  int err;
  if ((err = regcomp(&re, "[a-z0-9]+\\(", REG_EXTENDED)) != 0) {
    perror("Regcomp error");
    exit(EXIT_FAILURE);
  }
  regmatch_t match;
  char* buf = NULL;
  if (regexec(&re, line, 1, &match, 0) == 0) {
    buf = (char*)malloc((match.rm_eo) * sizeof(char));
    for (int i = 0; i < match.rm_eo - 1; ++i) {
      buf[i] = line[i];
    }
    buf[match.rm_eo - 1] = '\0';
  }
  regfree(&re);
  return buf;
}

double get_time(const char* line) {
  regex_t re;
  if (regcomp(&re, "<0.[0-9]+>", REG_EXTENDED) != 0) {
    perror("Regcomp error");
    exit(EXIT_FAILURE);
  }
  regmatch_t match;
  char buf[10];
  if (regexec(&re, line, 1, &match, 0) == 0) {
    for (int i = match.rm_so + 1; i < match.rm_eo - 1; ++i) {
      buf[i - match.rm_so - 1] = line[i];
    }
    buf[match.rm_eo - match.rm_so - 2] = '\0';
  }
  regfree(&re);
  return strtod(buf, NULL);
}

struct Info {
  char* name;
  double value;
};

void insert(struct Info* infos, int i, int k, int* ids) {
  for (int j = 0; j < k; ++j) {
    if (ids[j] >= 0 && infos[ids[j]].value < infos[i].value) {
      // Found a smaller value.
      for (int m = k; m >= j; --m) {
        ids[m+1] = ids[m];
      }
      ids[j] = i;
      return;
    }
  }
  // Not found, put it at end or ignore it.
  for (int j = 0; j < k; ++j) {
    if (ids[j] < 0) {
      ids[j] = i;
      return;
    }
  }
}

void print_result(struct Info* infos, int count, int k) {
  if (k > count) {
    k = count;
  }
  int ids[k+1];
  for (int i = 0; i < k + 1; ++i) {
    ids[i] = -1;
  }
  int cur = 0;
  for (int i = 0; i < count; ++i) {
    insert(infos, i, k, ids);
  }
  double total = 0;
  for (int i = 0; i < k; ++i) {
    total += infos[ids[i]].value;
  }
  for (int i = 0; i < k; ++i) {
    printf("%s, %f, (%d%%)\n", infos[ids[i]].name, infos[ids[i]].value, (int)(infos[ids[i]].value / total * 100));
  }
}

int main(int argc, char *argv[], char *envp[]) {
  if (argc < 2) {
    perror("Usage: sperf COMMAND ARGS\n");
    return -1;
  }
  // Get the PATH environment variable.
  const char* pre = "PATH=";
  const char* strace_str = "strace";
  char* path = NULL;
  for (char** str = envp; *str; ++str) {
    if (strncmp(pre, *str, strlen(pre)) == 0) {
      path = (char*)malloc(sizeof(char) * (strlen(*str) + 1));
      strcpy(path, *str);
    }
  }

  // Setup the exec_argv.
  char* exec_argv[argc + 3];
  exec_argv[0] = "strace";
  exec_argv[1] = "-T";
  for (int i = 1; i < argc; ++i) {
    exec_argv[i + 1] = argv[i];
  }
  exec_argv[argc + 1] = ">/dev/null";
  exec_argv[argc + 2] = NULL;

  // Create the pipe.
  int pipefd[2];
  if (pipe(pipefd) != 0) {
    perror("Pipe error\n");
    return -1;
  }

  // Create a child process to execve strace.
  int pid = fork();

  if (pid == 0) {
    close(pipefd[0]);
    for (char* str = path; ; str = NULL) {
      char* token = strtok(str, ":");
      if (!token) {
        break;
      }
      // Try to execve with token + "/strace"
      char* filename = (char*)malloc(sizeof(char) * strlen(token) + strlen(strace_str) + 2);
      strcpy(filename, token);
      strcat(filename, "/");
      strcat(filename, strace_str);
      dup2(pipefd[1], fileno(stderr));
      execve(filename, exec_argv, envp);
      fflush(stderr);
      free(filename);
    }
  } else {
    close(pipefd[1]);
    FILE* file = fdopen(pipefd[0], "r");
    if (file == NULL) {
      exit(EXIT_FAILURE);
    }
    char* line = NULL;
    size_t len = 0;

    struct Info infos[1024];
    int count = 0;
    while (getline(&line, &len, file) != -1) {
      char* name = get_syscall(line);
      const double value = get_time(line);
      if (!name) {
        continue;
      }
      int found = 0;
      for (int i = 0; i < count; ++i) {
        if (strcmp(infos[i].name, name) == 0) {
          infos[i].value += value;
          found = 1;
          break;
        }
      }
      if (!found) {
        infos[count].name = name;
        infos[count].value = value;
        ++count;
      }
    }

    print_result(infos, count, 10);
    free(path);
    for (int i = 0; i < count; ++i) {
      free(infos[i].name);
    }
  }
}
