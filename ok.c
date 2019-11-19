#include <glob.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define TOK_EQ '?'
#define TOK_NE '!'
#define TOK_COMMENT '#'
#define TOK_INDENT '\t'


typedef struct func_args {
    char **data;
    int count;
} func_args;


int cmd_count_args(const char *line) {
    int count = 0;
    int i;

    bool in_quotes = false;
    bool in_dquotes = false;

    for (i = 0; i < strlen(line); i++) {
        switch (line[i]) {
            case ' ':
                if (!in_quotes && !in_dquotes){
                    count++;
                }
                break;
            case '\'':
                if (in_dquotes)
                    break;
                in_quotes = !in_quotes;
                break;
            case '"':
                if (in_quotes)
                    break;
                in_dquotes = !in_dquotes;
                break;
        }
    }
    return ++count; // extra for NULL terminator
}


func_args *cmd_split_line(const char *line) {
    func_args *fa;
    fa = (func_args*)malloc(sizeof(func_args));
    fa->count = cmd_count_args(line);
    fa->data = malloc((fa->count + 1) * sizeof(char *));
    int data_i = 0;

    char tmp[strlen(line)];
    int tmp_i = 0;

    int i;
    bool in_quotes = false;
    bool in_dquotes = false;

    for (i = 0; i < strlen(line); i++) {
        switch (line[i]) {
            case ' ':
                if (!in_quotes && !in_dquotes) {
                    tmp[tmp_i] = '\0';
                    fa->data[data_i++] = strndup(tmp, tmp_i);
                    tmp_i = 0;
                } else{
                    tmp[tmp_i++] = line[i];
                }
                break;
            case '\'':
                if (in_dquotes){
                    tmp[tmp_i++] = line[i];
                    break;
                }
                in_quotes = !in_quotes;
                break;
            case '"':
                if (in_quotes){
                    tmp[tmp_i++] = line[i];
                    break;
                }
                in_dquotes = !in_dquotes;
                break;
            case '\0':
                break;
            default:
                tmp[tmp_i++] = line[i];
                break;
        }
    }
    // Whatever is left in our buffer becomes an arg
    if (tmp_i > 0) {
        fa->data[data_i++] = strndup(tmp, tmp_i);
    }

    fa->data[data_i] = NULL;
    return fa;
}


void free_func_args(func_args *fa) {
    int i;
    for (i = 0; i < fa->count; i++) {
        free(fa->data[i]);
    }
    free(fa->data);
    free(fa);
}


int exec_cmd(char *command) {
    pid_t pid;
    int status;
    int exit_status = 1;
    func_args *fa;

    fa = cmd_split_line(command);

    if ((pid = fork()) == 0) {
        execvp(fa->data[0], fa->data);
        perror("fork");
        free_func_args(fa);
        exit(EXIT_FAILURE);
    }

    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        exit_status = WEXITSTATUS(status);
    }
    // printf("Child status: %d\n", exit_status);
    free_func_args(fa);
    return exit_status;
}


void parse_file(const char *filename) {
    FILE *stream;
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;
    bool allow_indent = false;

    stream = fopen(filename, "r");
    if (stream == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while ((nread = getline(&line, &len, stream)) != -1) {
        // printf("Retrieved line of length %zu:\n", nread);
        line[strcspn(line, "\n")] = '\0'; // no newlines here

        switch(line[0]) {
            case TOK_EQ:
                allow_indent = false;
                if (exec_cmd(line + 1) == 0) {
                    allow_indent = true;
                }
                break;
            case TOK_NE:
                allow_indent = false;
                if (exec_cmd(line + 1) != 0) {
                    allow_indent = true;
                }
                break;
            case TOK_INDENT:
                if (allow_indent) {
                    exec_cmd(line + 1);
                }
                break;
            case TOK_COMMENT:
                break;
            case '\0':
                allow_indent = false;
                // empty lines are fine
                break;
            default:
                printf("Not sure what this is '%s'\n", line);
                break;
        }
    }
    free(line);
    fclose(stream);
}


void iter_scripts(const char *script_path) {
    glob_t results;
    int ret, i;
    ret = glob(script_path, 0, NULL, &results);

    if (ret != 0) {
        globfree(&results);
        return;
    }
    for (i = 0; i < results.gl_pathc; i++) {
        printf("> %s\n", results.gl_pathv[i]);
        parse_file(results.gl_pathv[i]);
    }
    globfree(&results);
}


int main(int argc, char *argv[]) {

    iter_scripts("scripts/*.f");

    return EXIT_SUCCESS;
}
