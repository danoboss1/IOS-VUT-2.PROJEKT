/**************************/
/* *  Daniel Sehnoutek  * */
/* *        IOS2        * */
/**************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>   
#include <sys/shm.h>
#include <semaphore.h>
#include <time.h>
#include <sys/wait.h>

typedef enum {
    MAIN,
    ZAKAZNIK,
    URADNIK
} ProcessType;

/*
 *  Structure: ProcessInfo
 *  ----------------------
 *  Stores data about individual processes
 */
typedef struct {
    ProcessType type;
    int id;
} ProcessInfo;

/*
 *  Structure: Shared_memory
 *  ------------------------
 *  Store data about the shared memory 
 */
typedef struct{
    bool open;
    int cislo_vypisu;
    int num_letters;
    int num_packages;
    int num_money;
    sem_t sem_writing;
    sem_t sem_post_office;
    sem_t sem_letters;
    sem_t sem_packages;
    sem_t sem_money;
    sem_t sem_uradnik;
    sem_t sem_calling_before_done; 
}Shared_memory;

// Identification key for allocation of the shared memory
#define shared_memory_key 1337

/*
 *  Funtion: check_time_range_included
 *  ----------------------------------
 *  Checks a required range in input times  
 */
void check_time_range_included(int n, int min, int max)
{
    if (n < min || max < n){
        fprintf(stderr, "Argument value is out of the range\n");
        exit(1);
    }
}

/*
 *  Funtion: not_number_input
 *  -------------------------
 *  Checks if inputs are numbers
 */
void not_number_input(char *str)
{
    if(strlen(str)){
    fprintf(stderr, "Invalid argument, only numbers are expected \n");
    exit(1);
    }
}

/*
 *  Funtion: destroy_shared_mem
 *  ---------------------------
 *  Detachment and deletion of the shared memory 
 */
void destroy_shared_mem(int shmid, Shared_memory *shm) {
    // The function detaches the shared memory segment
    if (shmdt(shm) == -1) {
        fprintf(stderr, "Error: Failed to detach shared memory.\n");
        exit(EXIT_FAILURE);
    }

    // The function removes the shared memory segment from the system
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        fprintf(stderr, "Error: Failed to remove shared memory.\n");
        exit(EXIT_FAILURE);
    }
}


/*
 *  Funtion: create_processes
 *  -------------------------
 *  Creates all child processes 
 *  Takes the total number of processes
 *  and firstly creates officers, then customers
 */
void create_processes(int num_zakaznik, int num_uradnik, ProcessInfo* process_info) {
    for (int i = 1; i <= num_zakaznik + num_uradnik; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            fprintf(stderr, "Failed to create child process %d\n", i);
            exit(1);
        } else if (pid == 0) {
            // This is the child process
            if (i <= num_uradnik) {
                process_info->type = URADNIK;
                process_info->id = i;
            } else {
                process_info->type = ZAKAZNIK;
                process_info->id = i - num_uradnik;
            }
            return;
        }
    }
}

/*
 *  Function: sem_inicialization
 *  ----------------------------
 *  Provides inicialization of semaphores
 *  Returns: 0 (if the initialization took place correctly)
 *           else (not) 
 */
int sem_inicialization(sem_t *sem, int init_value)
{
	if (sem_init(sem, 1, init_value) == -1)
	{
		fprintf(stderr, "%s\n", "Error: Sem_init failed.");
		return 1;
	}
	return 0;
}

/*
 *  Function: write_log
 *  -------------------
 *  Writes processes log messages
 */
void write_log(Shared_memory *shm, char* name, ProcessInfo* process_info, char* state, FILE* f)
{   
    sem_wait(&shm->sem_writing);
    fprintf(f,"%d: %s %d: %s\n", shm->cislo_vypisu, name, process_info->id, state);
    fflush(f);
    shm->cislo_vypisu++;
    sem_post(&shm->sem_writing);
}

/*
 *  Funtion: exit_closed_entrance
 *  -----------------------------
 *  Customer checks if post office is open
 *  while entering it
 *  Going home if it is closed 
 */
void exit_closed_entrance(Shared_memory *shm, ProcessInfo* process_info, FILE* f)
{
    if(shm->open == false)
    {   
        fprintf(f,"%d: Z %d: going home\n", shm->cislo_vypisu, process_info->id);
        fflush(f);
        shm->cislo_vypisu++;
        sem_post(&shm->sem_writing);
        sem_post(&shm->sem_post_office);
        exit(0);
    }
}

/*
 *  Funtion: officer_wait_before_task_done()
 *  -----------------------------
 *  Officer doing task, unsleep <0,10>
 */
void officer_wait_before_task_done()
{
    srand(time(NULL) *getpid());
    int officer_wait = rand() % 11;
    usleep(officer_wait);
}

/*
 *  Function: customer
 *  ------------------
 *  Life cycle of a custumer
 *  Provides synchronization between processes
 */
void customer(ProcessInfo* process_info,int TZ, Shared_memory *shm, FILE* f)
{   
    // Random time before entering the Post office
    srand(time(NULL) *getpid());
    int time_zakaznik = rand() % (TZ + 1);
    usleep(time_zakaznik * 1000);

    write_log(shm, "Z", process_info, "started", f);

    if(shm->open == false)
    {   
        write_log(shm, "Z", process_info, "going home", f);
        exit(0);
    }

    sem_wait(&shm->sem_post_office);

    // Chooses random servise at post office
    int type_service = (random() % 3) + 1;
    switch(type_service)
	{
		case 1:  
            sem_wait(&shm->sem_writing);

            exit_closed_entrance(shm, process_info, f);
            
            // Enter post office if not closed
            shm->num_letters++;
            fprintf(f, "%d: Z %d: entering office for a service %d\n", shm->cislo_vypisu, process_info->id, type_service);
            fflush(f);
            shm->cislo_vypisu++;

            sem_post(&shm->sem_writing);

            sem_post(&shm->sem_post_office);

            // Give signal that someone is waiting in the queue with letters
            sem_wait(&shm->sem_letters);

            write_log(shm, "Z", process_info, "called by office worker", f);
            // Synchronization called by office worker and service finished          
            sem_post(&shm->sem_calling_before_done);
			break;
		case 2:  
            sem_wait(&shm->sem_writing);

            exit_closed_entrance(shm, process_info, f);

            // Enter post office if not closed
            shm->num_packages++;
            fprintf(f, "%d: Z %d: entering office for a service %d\n", shm->cislo_vypisu, process_info->id, type_service);
            fflush(f);
            shm->cislo_vypisu++;

            sem_post(&shm->sem_writing);

            sem_post(&shm->sem_post_office);

            // Give signal that someone is waiting in the queue with packages
            sem_wait(&shm->sem_packages);

            write_log(shm, "Z", process_info, "called by office worker", f);
            // Synchronization called by office worker and service finished
            sem_post(&shm->sem_calling_before_done);
			break;
        case 3:  
            sem_wait(&shm->sem_writing);

            exit_closed_entrance(shm, process_info, f);

            // Enter post office if not closed
            shm->num_money++;
            fprintf(f, "%d: Z %d: entering office for a service %d\n", shm->cislo_vypisu, process_info->id, type_service);
            fflush(f);
            shm->cislo_vypisu++;

            sem_post(&shm->sem_writing);

            sem_post(&shm->sem_post_office);

            // Give signal that someone is waiting in the queue with packages
            sem_wait(&shm->sem_money);

            write_log(shm, "Z", process_info, "called by office worker", f);
            // Synchronization called by office worker and service finished
            sem_post(&shm->sem_calling_before_done);
		    break;
		default:
			break;
	}

    srand(time(NULL) *getpid());
    int customer_wait = rand() % 11;
    usleep(customer_wait);
    write_log(shm, "Z", process_info, "going home", f);
}

/*
 *  Function: urad
 *  ------------------
 *  Life cycle of a office worker
 *  Provides synchronization between processes
 */
void urad(ProcessInfo* process_info, int TU, Shared_memory *shm, FILE* f)
{
    srand(time(NULL) *getpid());

    write_log(shm, "U", process_info, "started", f);
    while(true)
    {
        sem_wait(&shm->sem_post_office);
        int order_of_lanes = (random() % 3) + 1;
        int type_service = 1;

        // Provides REAL RANDOMIZATION of post officer's choice of queue order
        switch(order_of_lanes)
        {
            case 1:  
                if(shm->num_letters > 0)
                {
                    sem_wait(&shm->sem_writing);

                    // Taking one requirement from letters queue
                    type_service = 1;
                    fprintf(f, "%d: U %d: serving a service of type %d\n", shm->cislo_vypisu, process_info->id, type_service);
                    fflush(f);
                    shm->cislo_vypisu++;

                    sem_post(&shm->sem_writing);
                    shm->num_letters--;

                    sem_post(&shm->sem_letters);

                    // Synchronization called by office worker and service finished
                    sem_wait(&shm->sem_calling_before_done);

                    officer_wait_before_task_done();
                    write_log(shm, "U", process_info, "service finished", f);
                }
                else if(shm->num_packages > 0)
                {
                    sem_wait(&shm->sem_writing);

                    // Taking one requirement from packages queue
                    type_service = 2;
                    fprintf(f, "%d: U %d: serving a service of type %d\n", shm->cislo_vypisu, process_info->id, type_service);
                    fflush(f);
                    shm->cislo_vypisu++;

                    sem_post(&shm->sem_writing);
                    shm->num_packages--;

                    sem_post(&shm->sem_packages);

                    // Synchronization called by office worker and service finished
                    sem_wait(&shm->sem_calling_before_done);

                    officer_wait_before_task_done();
                    write_log(shm, "U", process_info, "service finished", f);
                }
                else if(shm->num_money > 0)
                {
                    sem_wait(&shm->sem_writing);

                    // Taking one requirement from money queue
                    type_service = 3;
                    fprintf(f, "%d: U %d: serving a service of type %d\n", shm->cislo_vypisu, process_info->id, type_service);
                    fflush(f);
                    shm->cislo_vypisu++;

                    sem_post(&shm->sem_writing);
                    shm->num_money--;

                    sem_post(&shm->sem_money);

                    // Synchronization called by office worker and service finished
                    sem_wait(&shm->sem_calling_before_done);

                    officer_wait_before_task_done();
                    write_log(shm, "U", process_info, "service finished", f);
                }
                break;
            case 2:  
                if(shm->num_packages > 0)
                {   
                    sem_wait(&shm->sem_writing);

                    // Taking one requirement from packages queue
                    type_service = 2;
                    fprintf(f, "%d: U %d: serving a service of type %d\n", shm->cislo_vypisu, process_info->id, type_service);
                    fflush(f);
                    shm->cislo_vypisu++;

                    sem_post(&shm->sem_writing);
                    shm->num_packages--;

                    sem_post(&shm->sem_packages);

                    // Synchronization called by office worker and service finished
                    sem_wait(&shm->sem_calling_before_done);

                    officer_wait_before_task_done();
                    write_log(shm, "U", process_info, "service finished", f);
                }
                else if(shm->num_money > 0)
                {   
                    sem_wait(&shm->sem_writing);

                    // Taking one requirement from money queue
                    type_service = 3;
                    fprintf(f, "%d: U %d: serving a service of type %d\n", shm->cislo_vypisu, process_info->id, type_service);
                    fflush(f);
                    shm->cislo_vypisu++;

                    sem_post(&shm->sem_writing);
                    shm->num_money--;

                    sem_post(&shm->sem_money);

                    // Synchronization called by office worker and service finished
                    sem_wait(&shm->sem_calling_before_done);

                    officer_wait_before_task_done();
                    write_log(shm, "U", process_info, "service finished", f);
                }
                else if(shm->num_letters > 0)
                {
                    sem_wait(&shm->sem_writing);

                    // Taking one requirement from letters queue
                    type_service = 1;
                    fprintf(f, "%d: U %d: serving a service of type %d\n", shm->cislo_vypisu, process_info->id, type_service);
                    fflush(f);
                    shm->cislo_vypisu++;

                    sem_post(&shm->sem_writing);
                    shm->num_letters--;

                    sem_post(&shm->sem_letters);

                    // Synchronization called by office worker and service finished
                    sem_wait(&shm->sem_calling_before_done);

                    officer_wait_before_task_done();
                    write_log(shm, "U", process_info, "service finished", f);
                }
                break;
            case 3:  
                if(shm->num_money > 0)
                {   
                    sem_wait(&shm->sem_writing);

                    // Taking one requirement from money queue
                    type_service = 3;
                    fprintf(f, "%d: U %d: serving a service of type %d\n", shm->cislo_vypisu, process_info->id, type_service);
                    fflush(f);
                    shm->cislo_vypisu++;

                    sem_post(&shm->sem_writing);
                    shm->num_money--;

                    sem_post(&shm->sem_money);

                    // Synchronization called by office worker and service finished
                    sem_wait(&shm->sem_calling_before_done);

                    officer_wait_before_task_done();
                    write_log(shm, "U", process_info, "service finished", f);
                }
                else if(shm->num_letters > 0)
                {   
                    sem_wait(&shm->sem_writing);

                    // Taking one requirmenent from letters queue
                    type_service = 1;
                    fprintf(f, "%d: U %d: serving a service of type %d\n", shm->cislo_vypisu, process_info->id, type_service);
                    fflush(f);
                    shm->cislo_vypisu++;

                    sem_post(&shm->sem_writing);
                    shm->num_letters--;

                    sem_post(&shm->sem_letters);

                    // Synchronization called by office worker and service finished 
                    sem_wait(&shm->sem_calling_before_done);

                    officer_wait_before_task_done();
                    write_log(shm, "U", process_info, "service finished", f);
                }
                else if(shm->num_packages > 0)
                {   
                    sem_wait(&shm->sem_writing);

                    //Taking one requirement from packages queue
                    type_service = 2;
                    fprintf(f, "%d: U %d: serving a service of type %d\n", shm->cislo_vypisu, process_info->id, type_service);
                    fflush(f);
                    shm->cislo_vypisu++;

                    sem_post(&shm->sem_writing);
                    shm->num_packages--;

                    sem_post(&shm->sem_packages);

                    // Synchronization called by office worker and service finished
                    sem_wait(&shm->sem_calling_before_done);

                    officer_wait_before_task_done();
                    write_log(shm, "U", process_info, "service finished", f);
                }
                break;
            default:
                break;
        }

        sem_post(&shm->sem_post_office);
        
        // Office worker is going home when the post is closed and all requirement are done
        if(shm->num_letters == 0 && shm->num_packages == 0 && shm->num_money == 0 && shm -> open == false)
        {
            break;
        }

        // Office worker is taking break when nobody is waiting in queue and the post is opened
        if(shm->num_letters == 0 && shm->num_packages == 0 && shm->num_money == 0)
        {
            write_log(shm, "U", process_info, "taking break", f);

            int time_uradnik = random() % (TU + 1);
            usleep(time_uradnik * 1000);

            write_log(shm, "U", process_info, "break finished", f);
        }
    }

    write_log(shm, "U", process_info, "going home", f);
}

/***    MAIN    ***/
int main(int argc,char *argv[])
{
    // Seed the random number generator with the current time and a process ID
    srand(time(NULL) *getpid());

    if (argc != 6){
        fprintf(stderr, "Invalid number of arguments\n");
        exit(-1);
    }

    int NZ; // Number of customers
    int NU; // Number of office workers
    int TZ, TU, F; // Times

    char* str;

    // Checking numbers of child processes, if they are numbers
    NZ = (int)strtol(argv[1], &str, 0);
    not_number_input(str);
    NU = (int)strtol(argv[2], &str, 0);
    not_number_input(str);

    // Checking range of time input arguments and if they are numbers
    check_time_range_included(TZ = (int)strtol(argv[3], &str, 0), 0, 10000);
    not_number_input(str);
    check_time_range_included(TU = (int)strtol(argv[4], &str, 0), 0 , 100);
    not_number_input(str);
    check_time_range_included(F = (int)strtol(argv[5], &str, 0), 0, 10000);
    not_number_input(str);

    // File handling
    FILE* f;
    f = fopen("proj2.out", "w");
    if (f == NULL) {
        fprintf(stderr, "Error: Nepodarilo sa otvorit subor\n");
        exit(1);
    }

    // _______SHARED MEMERY INICIALIZATION__________

    int shmid = shmget(shared_memory_key, sizeof(Shared_memory), IPC_CREAT | 0666);
    if(shmid < 0)
    {
        fprintf(stderr, "A error occured during the shared memory allocation - pointer allocation\n");
        return 1;
    }

    //attachment of the shared memory to the address space
    Shared_memory *shm = shmat(shmid, NULL, 0);
    if(shm == (Shared_memory*) -1)
    {
        fprintf(stderr, "A error occured during the shared memory allocation - space allocation\n");
        return 1;
    }

    // Inicialization of variables in the shared memory
    shm -> open = true;
    shm -> cislo_vypisu = 1;
    shm -> num_letters = 0;
    shm -> num_packages = 0;
    shm -> num_money = 0;

    // _______SEMAPHORES INICIALIZATION__________
    if (sem_inicialization(&shm->sem_writing, 1) == 1) return 1;
    if (sem_inicialization(&shm->sem_post_office, 1) == 1) return 1;
    if (sem_inicialization(&shm->sem_letters, 0) == 1) return 1;
    if (sem_inicialization(&shm->sem_packages, 0) == 1) return 1;
    if (sem_inicialization(&shm->sem_money, 0) == 1) return 1;
    if (sem_inicialization(&shm->sem_uradnik, 1) == 1) return 1;
    if (sem_inicialization(&shm->sem_calling_before_done, 0) == 1) return 1;

    // Fork
    ProcessInfo process_info;
    process_info.type = MAIN;

    
    if (process_info.type == MAIN) 
    {
        create_processes(NZ, NU, &process_info);
    } 


    if (process_info.type == MAIN)
    {
        int time = (random() % ((F/2) + 1)) + (F/2); 
        usleep(time * 1000);

        sem_wait(&shm->sem_writing);

        shm->open = false;

        fprintf(f, "%d: closing\n", shm->cislo_vypisu);
        shm->cislo_vypisu++;
        fflush(f);

        sem_post(&shm->sem_writing);
    }


    // Sorting child processes
    switch(process_info.type)
	{
		case ZAKAZNIK:  
            customer(&process_info,TZ, shm, f);
            exit(0);
			break;
		case URADNIK:   
            urad(&process_info, TU, shm, f);
            exit(0);
			break;
		default:
			break;
	}

    // Main process waiting for all child processes
    while (wait(NULL) != -1);

    setbuf(f, NULL);

    // Destruction of semaphores
    sem_destroy(&shm->sem_writing);
    sem_destroy(&shm->sem_post_office);
    sem_destroy(&shm->sem_letters);
    sem_destroy(&shm->sem_packages);
    sem_destroy(&shm->sem_money);
    sem_destroy(&shm->sem_uradnik);
    sem_destroy(&shm->sem_calling_before_done);


    //Cleanup of shared memory
    if (fclose(f) == EOF) {
        fprintf(stderr, "Closing of the file failed\n");
    }
    destroy_shared_mem(shmid, shm);

    return 0;
}