#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <getopt.h>

typedef struct {
	char name[1024];
	pid_t pid;
	pid_t* children;
	int child_num;
} proc;

void PrintTree(proc* p, proc* procs, int depth, int pflag) {
	if (depth > 0) {
		printf("\n%*s |\n", (depth - 1) * 4, "");
		printf("%*s", (depth - 1) * 4, "");
		printf(" +--");
	}
	printf("%s", p->name);
	if (pflag) {
		printf("(%d)", p->pid);
	}
	for (int i = 0; i < p->child_num; ++i) {
		proc* tmp = procs;
		while (tmp != NULL) {
			if (tmp->pid == p->children[i]) {
				break;
			}
			++tmp;
		}
		PrintTree(tmp, procs, depth + 1, pflag);
	}
}

int main(int argc, char *argv[]) {
  // Get the arguments.
  char c;
  int nflag = 0, pflag = 0, vflag = 0;
  struct option long_options[] = {
	{"show-pids", no_argument, &pflag, 1},
	{"numeric-sort", no_argument, &nflag, 1},
	{"version", no_argument, &vflag, 1},
	{0, 0, 0, 0}
  };

    while ((c = getopt_long(argc, argv, "npV", long_options, NULL)) != -1) {
        switch (c) {     
            case 'n':
	  	nflag = 1;
	  	break;
	    case 'p':
	  	pflag = 1;
		break;
	    case 'V':
		vflag = 1;
		break;
            case 0:
                break;
            case '?':
                printf("Unknown option: %s\n", argv[optind-1]);
                return 1;
            default:
                printf("Unexpected option: %c\n", c);
                return 1;
        }
    }

  if (vflag) {
  	printf("A simple pstree by CY\n");
	return 0;
  }

  // Read max pid.
  FILE* pid_max_f = fopen("/proc/sys/kernel/pid_max", "r");
  if (pid_max_f == NULL) {
  	perror("No pid_max");
	return -1;
  }
  int pid_max;
  fscanf(pid_max_f, "%d", &pid_max);
  fclose(pid_max_f);

  char* base_path = "/proc";
  struct dirent* dent;
  DIR* srcdir = opendir(base_path);
  if (srcdir == NULL) {
  	perror("Open fail");
	return -1;
  }

  // Proc array.
  proc* procs;
  procs = malloc(sizeof(proc) * (pid_max + 1));
  pid_t* ppids;
  ppids = malloc(sizeof(pid_t) * pid_max);
  proc* p = procs;
  
  // Read proc directory.
  while ((dent = readdir(srcdir)) != NULL) {
	if (dent->d_name[0] < '0' || dent->d_name[0] > '9') {
		continue;
	}
	// Save the pids.
	p->pid = atoi(dent->d_name);
	
	// Read the stat to get name and ppid.
	char path[13 + strlen(dent->d_name) + 1], buf[1024];
	memset(path, 0, sizeof(path));
	strcat(path, base_path);
	strcat(path, "/");
	strcat(path, dent->d_name);
	strcat(path, "/status");
	FILE* f = fopen(path, "r");
	while ((fscanf(f, "%s", buf) != EOF)) {
		if (strcmp(buf, "Name:") == 0) {
			fscanf(f, "%s", p->name);
		}
		if (strcmp(buf, "PPid:") == 0) {
			fscanf(f, "%d", &ppids[p - procs]);
		}
	}
	fclose(f);
	++p;
  }
  int proc_count = p - procs;
  printf("Total proc: %d\n", proc_count);

  // Build the tree.
  for (int i = 0; i < proc_count; ++i) {
  	procs[i].children = malloc(sizeof(pid_t) * proc_count);
	procs[i].child_num = 0;
  }

  for (int i = 0; i < proc_count; ++i) {
	for (int j = 0; j < proc_count; ++j) {
		if (ppids[j] == procs[i].pid) {
			procs[i].children[procs[i].child_num++] = procs[j].pid;
		}
	}
  }

  for (int i = 0; i < proc_count; ++i) {
	  if (ppids[i] == 0) {
	  	PrintTree(&procs[i], procs, 0, pflag);
	  }
  }
  printf("\n");

  closedir(srcdir);
  return 0;
}
