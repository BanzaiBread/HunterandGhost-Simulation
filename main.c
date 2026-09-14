#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h> 
#include "defs.h"
#include "helpers.h"

int main() {

    //Initializes all the sturctures except hunters and fill them with variables 
    House house;
    Ghost ghost;
    CaseFile file;

    caseFile_init(&file);
    house_init(&house, &file);
    house_populate_rooms(&house);
    
    ghost_init(&ghost, false);
    ghost.currentRoom = &(house.rooms[rand_int_threadsafe(1,13)]);
    house.ghost = &ghost; 
    log_ghost_init(ghost.id, ghost.currentRoom->name, ghost.type);

    //The loop continuously askes for the user to input new hunters into the program, exits the loop when the user types done
    while(true){
        char name[MAX_HUNTER_NAME];
        int id;
        printf("What is the hunters name: ");
        scanf("%63s", name); 
        if(strcmp(name, "done") == 0){
            break;
        }
        printf("What is the hunters id: ");
        scanf("%d", &id);


        //Creates a hunter allocating it to the heap and all its variables 
        Hunter* newHunter = malloc(sizeof(Hunter));
        hunter_create(newHunter, id, name, &house, &file);
        hunter_push(newHunter, &house);    
        
        log_hunter_init(newHunter->id, newHunter->currentRoom->name, newHunter->name, newHunter->tracker);
        
        free(newHunter); 
    }

    pthread_t ghost_tid;
    pthread_t* hunter_tids = malloc(sizeof(pthread_t) * house.currentHunters);

    // Start Threads Concurrently
    pthread_create(&ghost_tid, NULL, ghost_thread, &ghost);

    //Adds the number of threads each the number of hunters
    for(int i = 0; i < house.currentHunters; i++){
        pthread_create(&hunter_tids[i], NULL, hunter_thread, &house.hunters[i]);
    }

    // Wait for Hunters to end
    for(int i = 0; i < house.currentHunters; i++){
        pthread_join(hunter_tids[i], NULL);
    }
    
    pthread_join(ghost_tid, NULL);

    //Goes through every hunter and prints the reason for their exit
    for(int i=0; i<house.currentHunters; i++){
        Hunter* h = &house.hunters[i];
        printf("Hunter %s (ID: %d) - Reason: ", h->name, h->id);
        if(h->reason == LR_EVIDENCE){
            printf("Evidence Collected\n");
        }
        else if(h->reason == LR_BORED) {
            printf("Boredom\n");
        }
        else if(h->reason == LR_AFRAID) {
            printf("Fear\n");
        }

    }

    if(file.solved){
        printf("Ghost identified!\n");
    }
    else {
        printf("Ghost was not identified.\n");
    }

    //Free up all semaphores
    sem_destroy(&file.mutex);
    for(int i=0; i < house.room_count; i++) {
        sem_destroy(&house.rooms[i].mutex);
    }

    //Free out all structures in the heap
    free(hunter_tids);
    freePack(&house); 

    return 0;
    

}