#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define ARGUMENTS 4

struct command{
    char** arg;
    struct command* next;
};

struct conveyor{
    struct command* com;
    struct conveyor* next;
    char* subshell;
    int in;
    int out;
    int status; //if the separating sign is "&&", status == 0, otherwise, if the sign is "||", status == 1//
};

struct condition_com{
    struct conveyor* conv;
    struct condition_com* next;
    int status; //if the separating sign is "&", status == 0, otherwise, if the sign is ";", status == 1//
};


int ending(char* line, char** p, int i, int *arg_num, int *is_com, struct command* ccom){
    if(*p != NULL){
                line[i] = '\0';
                ccom->arg[*arg_num] = *p;
                *p = NULL;
                *arg_num += 1;
    }
    if(*is_com){
        if(*arg_num % ARGUMENTS == 0){
            ccom->arg = realloc(ccom->arg, (sizeof(char*)*(*arg_num+1)));
            if(ccom->arg == NULL){
                perror("Memory allocation error");
                return 1;
            }
        }
        ccom->arg[*arg_num] = NULL;
        *arg_num = 0;
        *is_com = 0;
    }
    return 0;
}

int red_end(char* line, char* p, int i, int* temp_in, int* temp_out, int* red_type){
    line[i] = '\0';
    if(*red_type == 0){
        *temp_out = open(p, O_WRONLY | O_CREAT | O_TRUNC , 0666);
        if(*temp_out == -1){
            perror("Opening file error");
            return 1;
        }
    }else if(*red_type == 1){
        *temp_out = open(p, O_WRONLY | O_CREAT | O_APPEND , 0666);
        if(*temp_out == -1){
            perror("Opening file error");
            return 1;
        }
    }else if(*red_type == 2){
        *temp_in = open(p, O_RDONLY);
        if(*temp_in == -1){
            perror("Opening file error");
            return 1;
        }
    }
    
    return 0;
}
struct condition_com* parser(char* line, char* shell){
    struct condition_com* bcond;
    struct condition_com* cond = malloc(sizeof(struct condition_com));
    if(cond == NULL){
        perror("Memory allocation error");
        return NULL;
    }
    cond->next = NULL;
    cond->conv = NULL;
    cond->status = -1;
    bcond = cond;
    int i = 0;
    int arg_num = 0;
    char* p = NULL;
    int is_com = 0;
    int is_conv = 0;
    int is_cond = 0;
    int is_red = 0;
    int is_brackets = 0;
    struct command* ccom = NULL;
    struct conveyor* ccov = NULL;
    int temp_in = 0;
    int temp_out = 1;
    int red_type = -1;
    while(1){
        if(is_brackets) continue;
        if(line[i] == '('){
            is_brackets = 1;
            p = &line[i+1];
        }else if(line[i] == ')'){
            line[i] = '\0';
            is_brackets = 0;
            ccov->subshell = p;
            p = NULL;
        }    
        if(isspace(line[i])){
            if(p != NULL && is_com){
                line[i] = '\0';
                ccom->arg[arg_num] = p;
                p = NULL;
                arg_num++;
                if(arg_num % ARGUMENTS == 0){
                    ccom->arg = realloc(ccom->arg, (sizeof(char*)*(arg_num+ARGUMENTS)));
                    if(ccom->arg == NULL){
                        perror("Memory allocation error");
                        return NULL;
                    }
                }
            }
            if(is_red == 2){
                if(red_end(line, p, i, &temp_in, &temp_out, &red_type)) return NULL;
                is_red = 0;
                red_type = -1;
                p = NULL;
            }
        }else if(line[i] == '\0'){
            if(ending(line, &p, i, &arg_num, &is_com, ccom)) return NULL;
            if(is_red == 2 || is_red == 1){
                if(red_end(line, p, i, &temp_in, &temp_out, &red_type)) return NULL;
                is_red = 0;
                red_type = -1;
                p = NULL;
            }
            if(is_conv){
                    is_conv = 0;
                    ccov->in = temp_in; 
                    ccov->out = temp_out;
                    temp_in = 0;
                    temp_out = 1;
                }
            break;
        }else if(line[i] == '|'){
            if(is_red == 2){
                if(line[i+1] != '|') return NULL;
                if(red_end(line, p, i, &temp_in, &temp_out, &red_type)) return NULL;
                is_red = 0;
                red_type = -1;
                p = NULL;
            }
            if(ending(line, &p, i, &arg_num, &is_com, ccom)) return NULL;
            if(line[i+1] == '|'){
                i++;
                if(is_conv){
                    is_conv = 0;
                    ccov->status = 1; 
                    ccov->in = temp_in; 
                    ccov->out = temp_out;
                    temp_in = 0;
                    temp_out = 1;
                }
            }else{
                
            }
        }else if(line[i] == '<'){
            if(ending(line, &p, i, &arg_num, &is_com, ccom)) return NULL;
            is_red = 1;
            red_type = 2;
        }else if(line[i] == '>'){
            if(ending(line, &p, i, &arg_num, &is_com, ccom)) return NULL;
            is_red = 1;
            if(line[i+1] == '>'){
                i++;
                red_type = 1;
            }else{
                red_type = 0;
            }
        }else if(line[i] == '&'){
            if(is_red == 2){
                if(red_end(line, p, i, &temp_in, &temp_out, &red_type)) return NULL;
                is_red = 0;
                red_type = -1;
                p = NULL;
            }
            if(ending(line, &p, i, &arg_num, &is_com, ccom)) return NULL;
            if(line[i+1] == '&'){
                i++;
                if(is_conv){
                    is_conv = 0;
                    ccov->status = 0; 
                    ccov->in = temp_in; 
                    ccov->out = temp_out;
                    temp_in = 0;
                    temp_out = 1;
                }
            }else{
                if(is_conv){
                    is_conv = 0;
                    ccov->in = temp_in; 
                    ccov->out = temp_out;
                    temp_in = 0;
                    temp_out = 1;
                }
                if(is_cond) {is_cond = 0; cond->status = 0;}
            }
        }else if(line[i] == ';'){
            if(is_red == 2){
                if(red_end(line, p, i, &temp_in, &temp_out, &red_type)) return NULL;
                is_red = 0;
                red_type = -1;
                p = NULL;
            }
            if(ending(line, &p, i, &arg_num, &is_com, ccom)) return NULL;
            if(is_conv){
                    is_conv = 0;
                    ccov->in = temp_in; 
                    ccov->out = temp_out;
                    temp_in = 0;
                    temp_out = 1;
                }
            if(is_cond) {is_cond = 0; cond->status = 1;}
        }else{
            if(!is_cond && is_red == 0){
                p = &line[i]; 

                cond->next = malloc(sizeof(struct condition_com));
                if(cond->next == NULL){
                    perror("Memory allocation error");
                    return NULL;
                }
                cond = cond->next;

                cond->conv = malloc(sizeof(struct conveyor));
                if(cond->conv == NULL){
                    perror("Memory allocation error");
                    return NULL;
                }
                cond->status = -1;
                cond->next = NULL; 
                is_cond = 1;

                cond->conv->com = malloc(sizeof(struct command));
                if(cond->conv->com == NULL){
                    perror("Memory allocation error");
                    return NULL;
                }
                cond->conv->next = NULL;  
                cond->conv->subshell = NULL;  
                cond->conv->status = -1;
                is_conv = 1;
                ccov = cond->conv;

                cond->conv->com->arg = malloc(sizeof(char*)*ARGUMENTS);
                if(cond->conv->com->arg == NULL){
                    perror("Memory allocation error");
                    return NULL;
                }
                cond->conv->com->next = NULL;
                is_com = 1;
                ccom = cond->conv->com; 
                

            }else if(!is_conv && is_red == 0){
                p = &line[i]; 
                ccov->next = malloc(sizeof(struct conveyor));
                if(ccov->next == NULL){
                    perror("Memory allocation error");
                    return NULL;
                }
                ccov = ccov->next;

                ccov->com = malloc(sizeof(struct command));
                if(ccov->com == NULL){
                    perror("Memory allocation error");
                    return NULL;
                } 
                ccov->subshell = NULL;  
                ccov->next = NULL;
                ccov->status = -1;
                is_conv = 1;

                ccov->com->arg = malloc(sizeof(char*)*ARGUMENTS);
                if(ccov->com->arg == NULL){
                    perror("Memory allocation error");
                    return NULL;
                }
                ccov->com->next = NULL;
                is_com = 1;
                ccom = ccov->com;
            }else if(!is_com && is_red == 0){
                p = &line[i]; 
                ccom->next = malloc(sizeof(struct command));
                if(ccom->next == NULL){
                    perror("Memory allocation error");
                    return NULL;
                }
                ccom = ccom->next;

                ccom->arg = malloc(sizeof(char*)*ARGUMENTS);
                if(ccom->arg == NULL){
                    perror("Memory allocation error");
                    return NULL;
                }
                ccom->next = NULL;
                is_com = 1;

            }else if(is_red == 1){
                p = &line[i];
                //printf("%c", line[i]);
                is_red = 2;
            }else if(p == NULL && is_com && is_red == 0) p = &line[i];
        }
        i++;
    }
    return bcond;
}

int exe_conveyor(struct conveyor* conv, struct condition_com* cond, pid_t p){
    struct command* a;
    int* pipe_ar = (int*)malloc(sizeof(int)*2);
    int fd;
    a = conv->com;
    if(a != NULL){
        if(a->next != NULL){
            pipe(pipe_ar);
            if((p = fork()) == -1){
                perror("Fork faled");
                free(pipe_ar);
                return 0;
            }
            if(p == 0){
                close(pipe_ar[0]);
                dup2(pipe_ar[1], 1);
                close(pipe_ar[1]);
                execvp(a->arg[0], a->arg);
                exit(1);
            }
            close(pipe_ar[1]);
            fd = pipe_ar[0];
            a = a->next;
            while(a != NULL){
                if(a->next != NULL){
                    pipe(pipe_ar);
                    if((p = fork()) == -1){
                        perror("Fork faled");
                        free(pipe_ar);
                        return 0;
                    }
                    if(p == 0){
                        close(pipe_ar[0]);
                        dup2(pipe_ar[1], 1); 
                        close(pipe_ar[1]);
                        dup2(fd, 0);
                        close(fd);
                        execvp(a->arg[0], a->arg);
                        exit(1);
                    }
                    close(pipe_ar[1]);
                    close(fd);
                    fd = pipe_ar[0];
                }else{
                    if((p = fork()) == -1){
                        perror("Fork faled");
                        free(pipe_ar);
                        return 0;
                    }
                    if(p == 0){
                        dup2(fd, 0);
                        close(fd);
                        execvp(a->arg[0], a->arg);
                        exit(1);
                    }
                    close(fd);
                }
                a = a->next;
            } 
        }else{
            if((p = fork()) == -1){
                perror("Fork faled");
                free(pipe_ar);
                return 0;
            }
            if(p == 0){
                execvp(a->arg[0], a->arg);
                exit(1);
            }
        }
        int status;
        if(cond->status != 0){
            waitpid(p, &status, 0); 
        }
        free(pipe_ar);
        return  WEXITSTATUS(status);
        
    }
    return 0;
}

void delete_struct(struct condition_com* cond){
struct command* a;
struct conveyor* b;
struct condition_com* c;
    while(cond != NULL){
        while(cond->conv != NULL){
            while(cond->conv->com != NULL){
                free(cond->conv->com->arg);
                a = cond->conv->com;
                cond->conv->com = cond->conv->com->next;
                free(a);
            }
            b = cond->conv;
            cond->conv = cond->conv->next;
            free(b);
        }
        c = cond;
        cond = cond->next;
        free(c);
    }
}

int main(int argc, char* argv[]){
    if(argc > 2) return 1;
    int k = 1;
    struct condition_com* cond;
    struct condition_com* condb;
    struct conveyor* b;
    struct condition_com* c;
    char* line = NULL;
    pid_t p;
    size_t len = 0;
    int stat;
    while(k){
        if(argc > 1){
            printf("%s\n", argv[1]);
            strcpy(line, argv[1]);
            printf("%s\n", line);
            k = 0;
        }
        else getline(&line, &len, stdin);
        cond = parser(line, argv[0]);
        condb = cond;
        if(cond == NULL) return 0;
        c = cond;
        while(c->next != NULL){
            c = c->next;
            //printf("%d", cond->status);
            b = c->conv;
            if(c->status == 1 || c->status == -1){
                while(b != NULL){
                    if(b->subshell != NULL){
                        if((p = fork()) == -1){
                            perror("Fork failed");
                            return 0;
                        }
                        if(p == 0){
                            execl(argv[0], b->subshell, NULL);
                        }
                    }
                    if((p = fork()) == -1){
                        perror("Fork failed");
                        return 0;
                    }
                    if(p == 0){
                        dup2(b->out, 1);
                        dup2(b->in, 0);
                        stat = exe_conveyor(b, c, p);
                        close(b->out);
                        close(b->in);
                        exit(stat);
                    }
                    if(b->status == 1){
                        waitpid(p, &stat, 0);
                        stat = WEXITSTATUS(stat);
                        if(stat == 0) b = b->next;
                    }else if(b->status == 0){
                        waitpid(p, &stat, 0);
                        stat = WEXITSTATUS(stat);
                        if(stat != 0) b = b->next;
                    }else{
                        waitpid(p, &stat, 0);
                    }
                    if(b != NULL) b = b->next;
                }
            }else{
                while(b != NULL){
                    if((p = fork()) == -1){
                        perror("Fork failed");
                        return 0;
                    }
                    if(p == 0){
                        //if(b->out == 1) b->out = open("/dev/null", O_WRONLY);
                        if(b->in == 0) b->in = open("/dev/null", O_RDONLY);    
                        dup2(b->out, 1);
                        dup2(b->in, 0);
                        signal(SIGINT,SIG_IGN);
                        stat = exe_conveyor(b, c, p);
                        close(b->out);
                        close(b->in);
                        exit(stat);
                    }
                    if(b->status == 1){
                        waitpid(p, &stat, 0);
                        stat = WEXITSTATUS(stat);
                        if(stat == 0) b = b->next;
                    }else if(b->status == 0){
                        waitpid(p, &stat, 0);
                        stat = WEXITSTATUS(stat);
                        if(stat != 0) b = b->next;
                    }
                    if(b != NULL) b = b->next;
                }
            }
        }
        delete_struct(condb);
    }
    free(line);
    return 0;
}
