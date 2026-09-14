#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <stdint.h>
#include "defs.h"
#include "helpers.h"

/*
  Initialize the Ghost structure sets a random ghost type and put the evidences 
  from that ghost type into an array

  in/out: ghost; gets the structure
  in/out; flag; the simulation started so the flag is false

  Return:
*/
void ghost_init(Ghost* ghost ,bool flag ){
    const enum GhostType *list;
    int count = get_all_ghost_types(&list);
    
    ghost->flag = false;
    ghost->id = DEFAULT_GHOST_ID;
    ghost->boredom = 0;
    ghost->type = list[rand_int_threadsafe(0, count)];
    determineEvidence(ghost);
}

/*
  Takes the ghost types and determines what evidences that type produces and puts it in a varaible

  in/out: ghost; gets the structure

  Return:
*/

void determineEvidence(Ghost* ghost ){
    char byte = ghost->type;
    int counter = 0;
    char bitmask = 1;

    for(int j = 0; j < 7;j++){
        if(bitmask & byte){
            ghost->evidences[counter] = (enum EvidenceType) bitmask;
            counter++;
        }  
        bitmask = bitmask << 1;
    }
}

/*
  Takes a random piece of evidence and adds it to the room

  in/out: ghost; gets the structure

  Return:
*/
void haunting(Ghost* ghost){    
    enum EvidenceType evidence = ghost->evidences[rand_int_threadsafe(0,3)];

    //Locks the room
    sem_wait(&ghost->currentRoom->mutex);

    //Stops all movement if hunter is in the room
    if (ghost->currentRoom->hunterCount > 0) {
        ghost->boredom = 0;
        sem_post(&ghost->currentRoom->mutex);
        return;
    }

    log_ghost_evidence(ghost->id, ghost->boredom, ghost->currentRoom->name, evidence);
    ghost->currentRoom->clue |= evidence;
    printf("%d", ghost->currentRoom->hunterCount);

    //The room unlocks
    sem_post(&ghost->currentRoom->mutex);

    ghost->boredom++;
}

void moving(Ghost* ghost){
    Room* cur = ghost->currentRoom;
    Room* next = cur->roomConnections[rand_int_threadsafe(0, cur->roomCount)];

    /*A fixed order to locking the rooms so the hunters don't enter a deadlock one hunter just locks the rooms
      moving that hunter releases then the other hunter can lock the room and do his movement
    */
    if(cur < next){
        sem_wait(&cur->mutex);
        sem_wait(&next->mutex);
    } else {
        sem_wait(&next->mutex);
        sem_wait(&cur->mutex);
    }
    
    //Checks if the hunter is there immediately stop all movement
    if (cur->hunterCount > 0) {
        ghost->boredom = 0;
        sem_post(&cur->mutex);
        sem_post(&next->mutex);
        return;
    }

    log_ghost_move(ghost->id, ghost->boredom, cur->name, next->name);
    cur->ghost = NULL;
    ghost->currentRoom = next;
    next->ghost = ghost;
     printf("%d", ghost->currentRoom->hunterCount);
    sem_post(&cur->mutex);
    sem_post(&next->mutex);

    ghost->boredom++;
}

/*
  Checks if the ghost has to leave the simulation

  in/out: ghost; gets the structure

  Return:
*/
void ghostLeave(Ghost* ghost){

    //Checks boredom value
    if(ghost->boredom >= ENTITY_BOREDOM_MAX){
        log_ghost_exit(ghost->id, ghost->boredom, ghost->currentRoom->name);
        ghost->flag = true;

        sem_wait(&ghost->currentRoom->mutex);
        ghost->currentRoom->ghost = NULL;
        sem_post(&ghost->currentRoom->mutex);
    }
}

void* ghost_thread(void* arg) {
    Ghost* ghost = (Ghost*)arg;

    while (ghost->flag == false) {
        int behaviourCheck = rand_int_threadsafe(1, 4);
    
        //Idle
        if (behaviourCheck == 1) { 

            //Checks if hunter is in the room;
            sem_wait(&ghost->currentRoom->mutex);
            if (ghost->currentRoom->hunterCount > 0) {
                ghost->boredom = 0;
            } else {
                log_ghost_idle(ghost->id, ghost->boredom, ghost->currentRoom->name);
                ghost->boredom++;
            }
            sem_post(&ghost->currentRoom->mutex);
        }

        //Haunting
        else if (behaviourCheck == 2) { 
            haunting(ghost);
        }
        else if (behaviourCheck == 3) { 
            moving(ghost);
        }

        ghostLeave(ghost);
    }
    return NULL;
}