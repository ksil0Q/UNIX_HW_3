#include <sys/types.h> 
#include <sys/wait.h> 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>

#include "myinit.h"

// file existance status macros
#define FILE_EXISTS 1
#define FILE_DOES_NOT_EXISTS 0

// file path macros
#define ABSOLUTE_PATH 1
#define RELATIVE_PATH 0

// pids
#define MAXPROC 5
pid_t pid_list[MAXPROC];
int pid_count=0;

// log file
char* log_file_name = "/tmp/myinit.log";

char* config_name;
// log message types
#define INFO "Info"
#define ERROR "Error"


void log_message(char* message_type, char* message)
{
    FILE* log_file = fopen(log_file_name, "a+");
    if (log_file == NULL)
    {
        printf("Error: can`t open log file\n");
        exit(EXIT_FAILURE);
    }
    fprintf(log_file, "%s: %s", message_type, message);
    fclose(log_file);
}


int digits_count(int num)
{
    if (num < 10)
        return 1;
    return floor(log10(abs(num))) + 1; // :(
}


int get_lines_count_in_file(FILE* file)
{
    int lines_count = 0;
    while (!feof(file)){
        if (fgetc(file) == '\n')
            lines_count++;
    }
    fseek(file, 0, SEEK_SET);
    return lines_count + 1;
}

char** read_from_config_file(FILE* config, int* tasks_count)
{
    if (!config){
        log_message(ERROR, "file is closed\n");
        exit(EXIT_FAILURE);
    }
    int lines_count = get_lines_count_in_file(config);

    if (lines_count == 0){
        log_message(ERROR, "config file is empty\n");
        exit(EXIT_FAILURE);
    }
    int readed;
    size_t len = 0;
    char* line = NULL;
    char** lines = (char**)malloc(lines_count * sizeof(char*));
    int i = 0;
    while ((readed = getline(&line, &len, config)) != -1){
        line[strcspn(line, "\n")] = 0; // remove \n

        lines[i] = (char*)malloc(len);
        if (!(lines[i] = (char*)malloc(len + 1))){
            while (i--){
                free(lines[i]);
                log_message(ERROR, "smth went wrong in memory allocation\n");
                exit(EXIT_FAILURE);
            }
        }
        strcpy(lines[i], line);
        i++;
    }
    free(line);
    *tasks_count = lines_count;
    return lines;
}

void change_process_dir_to_root()
{
    if (!chdir("/")){
        log_message(INFO, "Process dir changed to root\n");
        return;
    }
    log_message(ERROR, "Process dir cannot changed to root\n");
    exit(EXIT_FAILURE);
}

int check_absolute_path(char* file_path)
{
    if (file_path[0] == '.' || file_path[0] != '/'){
        return RELATIVE_PATH;
    }
    return ABSOLUTE_PATH;
}

int check_file_exists(char* file_name)
{
    if (check_absolute_path(file_name) == ABSOLUTE_PATH)
        if (access(file_name, F_OK) == 0)
            return FILE_EXISTS;
        else
            return FILE_DOES_NOT_EXISTS;
    else
        return FILE_DOES_NOT_EXISTS; // only absolute path are allowed
}

Task* make_task_from_str(char* task_s)
{
    Task* task = (Task*)malloc(sizeof(Task));
    task->executable = NULL;
    task->args = NULL;
    task->_stdin = NULL;
    task->_stdout = NULL;
    task->is_empty = 1; // by default

    if (!task_s){
        log_message(ERROR, "Empty line was read from config -> skipped\n");
        return task;
    }

    char* token;
    if ((token = strtok(task_s, " ")) != NULL){ // split by space, get path to executable
        if (check_absolute_path(token) == RELATIVE_PATH)
            return task; // .is_empty = 1
        task->executable = (char*)malloc(strlen(token) + 1);
        strcpy(task->executable, token);
    }

    while ((token = strtok(NULL, " ")) != NULL){
        if (token[0] == '/' || token[0] == '.'){ // trying to distinguish keys from stdin/stdout, skipping relative path
            break; // end of reading keys
        }
        else{
            if (task->args == NULL){
                task->args = (char*)malloc(strlen(token) + 1);
                strcpy(task->args, token);
            }
            else {
                task->args = (char*)realloc(task->args, strlen(task->args) + strlen(token) + 2); // 2 cause space between keys 
                strcat(task->args, " ");
                strcat(task->args, token);
            }
        }
    }

    if (token != NULL)
        if (check_absolute_path(token) == ABSOLUTE_PATH){
            task->_stdin = (char*)malloc(strlen(token) + 1);
            strcpy(task->_stdin, token);
        }
        else{
            char* err_mes = "Got relative path - '', expected absolute path\n"; 
            err_mes = (char*)malloc(strlen(err_mes) + strlen(token) + 1);
            sprintf(err_mes, "Got relative path - '%s', expected absolute path\n", token);
            log_message(ERROR, err_mes);
            return task;
        }
    else{
        log_message(ERROR, "Incorrect args in configuration file, make sure that all pathes in absolute form\n");
        return task;
    }

    if ((token = strtok(NULL, " ")) != NULL)
        if (check_absolute_path(token) == ABSOLUTE_PATH){
            task->_stdout = (char*)malloc(strlen(token) + 1);
            strcpy(task->_stdin, token);
        }
        else {
            char* err_mes = "Got relative path - '', expected absolute path\n"; 
            err_mes = (char*)malloc(strlen(err_mes) + strlen(token) + 1);
            sprintf(err_mes, "Got relative path - '%s', expected absolute path\n", token);
            log_message(ERROR, err_mes);
            return task;
        }
    else{
        log_message(ERROR, "Incorrect args in configuration file, make sure that all pathes in absolute form\n");
        return task;
    }

    task->is_empty = 0;
    return task;
}

void free_task(Task* task)
{
    if (task)
    {
        if(task->executable != NULL)
            free(task->executable);

        if(task->args != NULL)
            free(task->args);

        if(task->_stdin != NULL)
            free(task->_stdin);

        if(task->_stdout != NULL)
            free(task->_stdout);
        
        free(task);
    }
}

void _start_child(Task* task, int i)
{
    pid_t cpid;

    cpid = fork();
    switch (cpid) {
        case -1:
            log_message(ERROR, "Fork failed\n");
            return;
        case 0:
            cpid = getpid();
            freopen(task->_stdin, "r+", stdin);
            freopen(task->_stdout, "r+", stdout);
            char* task_name = (char*)malloc(strlen(task->executable) + 2);
            sprintf(task_name, "%s%d", task->executable, i);
            execl(task->executable, task_name, task->args, NULL);
            exit(0);
        default:
            task->pid = cpid;
            pid_list[i] = cpid;
            pid_count++;
    }
    char* mes = "Task with pid= started\n";
    mes = (char*)malloc(strlen(mes) + digits_count(task->pid) + 1);
    sprintf(mes, "Task with pid=%d started\n", task->pid);
    log_message(INFO, mes);
}
void start_childs(char** tasks_s, int tasks_count, Task** tasks)
{
    for (int i = 0; i < tasks_count; i++){
        Task* task = make_task_from_str(tasks_s[i]);
        if (task->is_empty) {
            free_task(task);
            continue;
        }
        tasks[i] = task;
        _start_child(task, i);
    }
}

void sighup_handler(int signal)
{
    if (signal == SIGHUP)
    {
        for (int i = 0; i < pid_count; i++)
        {
            kill(pid_list[i], SIGKILL);
            exit(0);
        }
    }

    FILE* config = fopen(config_name, "r");
    if (config == NULL){
            char* err_mess = "Cannot open file '' for reading";
            err_mess = (char*)malloc(strlen(err_mess) + strlen(config_name + 1));
            sprintf(err_mess, "Cannot open file '%s' for reading", config_name);
            log_message(ERROR, err_mess);
            exit(EXIT_FAILURE);
        }

    int tasks_count = 0;    
    char** tasks_s = read_from_config_file(config, &tasks_count);
    Task** tasks = (Task**)malloc(sizeof(Task*) * tasks_count); 
    start_childs(tasks_s, tasks_count, tasks);
    return;
}

int main(int argc, char* argv[])
{
    signal(SIGHUP, sighup_handler);
    change_process_dir_to_root();
    int arg = 0;
    char* _config_name;
    // args parsing
    while ((arg = getopt(argc, argv, "p:")) != -1)
    {
        switch (arg)
        {
            case 'p':
                _config_name = (char*)malloc(strlen(optarg) + 1);
                strcpy(_config_name, optarg);
                char* mess = "Init process got configuration file with name: \n";
                mess = (char*)malloc(strlen(mess) + strlen(_config_name) + 1);
                sprintf(mess, "Init process got configuration file with name: %s\n", _config_name);
                log_message(INFO, mess);
                config_name = _config_name;
        }
    }
    log_message(INFO, "myinit started\n");
    if (check_file_exists(config_name) == FILE_DOES_NOT_EXISTS)
    {
        char* err_mess = " doesn`t exists\n";
        err_mess = (char*)malloc(strlen(err_mess) + strlen(config_name + 1));
        sprintf(err_mess, "%s doesn`t exists\n", config_name);
        log_message(ERROR, err_mess);
        exit(EXIT_FAILURE);
    }
    else
    {
        FILE* config = fopen(config_name, "r");
        if (config == NULL){
            char* err_mess = "Cannot open file '' for reading";
            err_mess = (char*)malloc(strlen(err_mess) + strlen(config_name + 1));
            sprintf(err_mess, "Cannot open file '%s' for reading", config_name);
            log_message(ERROR, err_mess);
            exit(EXIT_FAILURE);
        } 

        int tasks_count = 0;
        char** tasks_s = read_from_config_file(config, &tasks_count);
        Task** tasks = (Task**)malloc(sizeof(Task*) * tasks_count); 
        start_childs(tasks_s, tasks_count, tasks);

        pid_t cpid;
        while (pid_count)
        {
            cpid=waitpid(-1, NULL, 0);
            for (int i=0; i<MAXPROC; i++)
            {
                if(pid_list[i]==cpid)
                {
                    char* mess = "Child number  pid  finished\n";
                    mess = (char*)malloc(strlen(mess) + digits_count(i) + digits_count(cpid) + 1);
                    sprintf(mess, "Child number %d pid %d finished\n", i, cpid);
                    log_message(INFO, mess);

                    _start_child(tasks[i], i);
                    pid_list[i]=0;
                    pid_count--;
                }
            }
        }
    }

    return 0;
}