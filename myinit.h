typedef struct {
    char* executable;
    char* args;
    char* _stdin;
    char* _stdout; // stdin/stdout names are macros in stdio.h
    int is_empty;
    int pid;
} Task;