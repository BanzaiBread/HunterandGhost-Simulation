#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <stdint.h>
#include "helpers.h"
#include "defs.h"


/*
  Initialize the Room with all its variables and sets the semaphore inside it 

  in/out: ghost; gets the structure
  in: name; the name of the room
  in/out; is_exit; the simulation started so the flag is false

  Return:
*/
void room_init(struct Room* room, const char* name, bool is_exit){

     room->is_exit = is_exit;
    strncpy(room->name, name, MAX_ROOM_NAME-1);
    room->name[MAX_ROOM_NAME-1] = '\0'; // Ensure null termination
    
    room->hunterCount = 0;
    room->roomCount = 0;
    room->ghost = NULL;
    room->clue = 0;

    sem_init(&room->mutex, 0, 1); 

};



/*
  Connects the room to the other room and viceversa, 

  in/out: a; the room to connect 
  in/out; b: the other room to connect

  Return:
*/
void room_connect(struct Room* a, struct Room* b){

    a-> roomConnections[a->roomCount] = b;
    a->roomCount++;

    b-> roomConnections[b->roomCount] = a;
    b->roomCount++;

}; // Bidirectional connection
