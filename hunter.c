#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "helpers.h"
#include "defs.h"



/*
  Initialize the hunter with all its variables adds a random tracker

  in/out: hunter; hunter that the variables are directed to
  in: id; id of the hunter
  in: name; name of the hunter
  in/out: house; the house is a parameter so the hunter can start at the Van
  in/out: file; every hunter gets the shared casefile

  Return:
*/
void hunter_create(Hunter* hunter, int id, char name[], House* house, CaseFile* file) {
   
    const enum EvidenceType *list;
    int count = get_all_evidence_types(&list);
    hunter->id = id;
    strncpy(hunter->name, name, MAX_HUNTER_NAME - 1);
    hunter->fear = 0;
    hunter->boredom = 0;

    //Always start at the van 
    hunter->currentRoom = &(house->rooms[0]);

    hunter->tracker = list[rand_int_threadsafe(0, count)];
    hunter->flag = false;
    hunter->exit_van = false;
    hunter->roomStack = stackinit();
    hunter->caseFile = file;
}



/*
  Initialize the stacks allocating it in the heap
  
  Return the pointer to the initialized stack:
*/
RoomStack* stackinit() {
    RoomStack* stack = malloc(sizeof(RoomStack));
    stack->head = NULL;
    return stack;
}

/*
  Push the hunter into the house hunter array

  in/out: hunter; the hunter is the structure being pushed
  in/out: house; the house is a parameter to access its hunter array

  Return:
*/
void hunter_push(Hunter* hunter, House* house) {
    resize(house);
    house->hunters[house->currentHunters] = *hunter;
    house->currentHunters++;
}
/*
  Resizing the house hunter array

  in/out: house; the house is a parameter to access its hunter array

  Return:
*/
void resize(House* house) {
    if (house->currentHunters == house->huntersMax) {
        house->huntersMax = 2 * house->huntersMax;
        house->hunters = (Hunter*) realloc(house->hunters, sizeof(Hunter) * house->huntersMax);
    }
}

/*
  Frees out all hunters, roomnodes and the roomstack for each hunter

  in/out: house; the house is a parameter to  access its hunter array

  Return:
*/
void freePack(House* house) {
    for (int i = 0; i < house->currentHunters; i++) {
        while (house->hunters[i].roomStack->head != NULL) {
            removeFirst(house->hunters[i].roomStack);
        }
        free(house->hunters[i].roomStack);
    }
    free(house->hunters);
}

/*
  Starts the hunter thread the hunter always moves and checks if the room has evidence unless the hunter 
  has to go back to the van for some reason

  in/out: arg; A generic pointer to anything in this case the function received the ghost structure

  Return:
*/
void* hunter_thread(void* arg) {
    Hunter* hunter = (Hunter*)arg;

    //Ends the simulation when the flag is true
    while (hunter->flag == false) {

        //Moves and checks evidence
        if (hunter->exit_van == false) {
            if (strcmp(hunter->currentRoom->name, "Van") != 0) {
                checkEvidence(hunter);
            }

            if (hunter->exit_van == false) {
                hunterMove(hunter);
            }
        // Goes back to the van when the flag is true
        } else {
            traversalHome(hunter);
        }

        //Always check status and update it 
        checkRoomStatus(hunter);
        exitStatus(hunter);

        //Checks if the case is solved
        if (strcmp(hunter->currentRoom->name, "Van") == 0 && hunter->flag == false) {
            sem_wait(&hunter->caseFile->mutex);
            bool solved = hunter->caseFile->solved;
            sem_post(&hunter->caseFile->mutex);

            //Exits simulation if solved
            if (solved) {
                log_exit(hunter->id, hunter->boredom, hunter->fear, hunter->currentRoom->name, hunter->tracker, LR_EVIDENCE);
                hunter->flag = true;
            //Otherwise just swap tracker and clear stack
            } else {
                swapTracker(hunter);
                while (hunter->roomStack->head != NULL) {
                    removeFirst(hunter->roomStack);
                }
                hunter->exit_van = false;
            }
        }
    }
    return NULL;
}
/*
  Pops the stack and moves the hunter to that room 

  in/out: hunter; that specific hunter stack is being manipulated

  Return:
*/
void traversalHome(Hunter* hunter) {
    if (hunter->roomStack->head != NULL) {
        
        //Sets which room to move to and current room so a fixed order can be used for the semaphores
        Room* moveRoom = hunter->roomStack->head->room;
        Room* cur = hunter->currentRoom;
        
        /*A fixed order to locking the rooms so the hunters don't enter a deadlock one hunter just locks the rooms
          moving that hunter releases then the other hunter can lock the room and do his movement
        */
        if (cur < moveRoom) {
            sem_wait(&cur->mutex);
            sem_wait(&moveRoom->mutex);
        } else {
            sem_wait(&moveRoom->mutex);
            sem_wait(&cur->mutex);
        }

        //Only moves hunter if there is space in the room
        if (moveRoom->hunterCount < MAX_ROOM_OCCUPANCY) {
            //Pops stack
            removeFirst(hunter->roomStack);

            //Moves hunter
            hunterBacktrack(hunter, moveRoom);
        } 
        
        sem_post(&cur->mutex);
        sem_post(&moveRoom->mutex);
    }
    

    if (strcmp(hunter->currentRoom->name, "Van") == 0 && hunter->exit_van == true) {
        log_return_to_van(hunter->id, hunter->boredom, hunter->fear, hunter->currentRoom->name, hunter->tracker, false);
    }
}
/*
  Adds something to the hunter 

  in/out: hunter; that specific hunter stack is being manipulated

  Return:
*/
void addStack(Hunter* hunter) {
    RoomNode* newNode = malloc(sizeof(RoomNode));
    newNode->room = hunter->currentRoom;
    newNode->next = hunter->roomStack->head;
    hunter->roomStack->head = newNode;
}
/*
  Pops the stack and free the old node

  in/out: hunter; that specific hunter stack is being manipulated

  Return: the pointer to the removed room
*/
Room* removeFirst(RoomStack* stack) {
    if (stack->head == NULL) {
        return NULL;
    } else {
        RoomNode* oldNode = stack->head;
        Room* room = oldNode->room;
        stack->head = stack->head->next;
        free(oldNode);
        return room;
    }
}

/*
  Checks evidence gather up the evidence if its there and see if it matches with the tracker adding it to the file
  leaves a 30% chance to swap the tracker

  in/out: hunter; that specific hunter casefile and tracker is being manipulated

  Return:
*/
void checkEvidence(Hunter* hunter){
    bool found = false;

    sem_wait(&hunter->currentRoom->mutex);

    if (hunter->currentRoom->ghost != NULL) {
        hunter->boredom = 0; 
    }

    //If there is a clue in the room gather it up
    if(hunter->tracker & hunter->currentRoom->clue){
        hunter->currentRoom->clue &= ~(hunter->tracker); 
        found = true;
    }
    sem_post(&hunter->currentRoom->mutex); 

    // The hunter has sucessfully gathered the file and added it to the casefile if found is true
    if(found){
        sem_wait(&hunter->caseFile->mutex); 
        hunter->caseFile->collected |= hunter->tracker;
        if(evidence_has_three_unique(hunter->caseFile->collected)){
             hunter->caseFile->solved = true;
        }
        sem_post(&hunter->caseFile->mutex);

        log_evidence(hunter->id, hunter->boredom, hunter->fear, hunter->currentRoom->name, hunter->tracker);
        hunter->exit_van = true;
    }

    //Random chance of swapping tracker if the evidence does not match with
    else{
 
        if(rand_int_threadsafe(0, 100) < 30){ 
            log_return_to_van(hunter->id, hunter->boredom, hunter->fear, hunter->currentRoom->name, hunter->tracker, true);
            hunter->exit_van = true;
        }
    }
}

/*
  Swaps the tracker with a random one

  in/out: hunter; that specific hunter tracker is being manipulated

  Return:
*/
void swapTracker(Hunter* hunter) {

    enum EvidenceType newTracker = (enum EvidenceType) 1 << rand_int_threadsafe(0, 7);
    log_swap(hunter->id, hunter->boredom, hunter->fear, hunter->tracker, newTracker);
    hunter->tracker = newTracker;
}

/*
  Moves the hunter to a random room connected to its current room adjusting the hunter count

  in/out: hunter; that specific hunter room location is being manipulated

  Return:
*/
void hunterMove(Hunter* hunter) {
    
    Room* cur = hunter->currentRoom;
    Room* next = cur->roomConnections[rand_int_threadsafe(0, cur->roomCount)];


    /*A fixed order to locking the rooms so the hunters don't enter a deadlock one hunter just locks the rooms
        moving that hunter releases then the other hunter can lock the room and do his movement
    */
    if (cur < next) {
        sem_wait(&cur->mutex);
        sem_wait(&next->mutex);
    } else {
        sem_wait(&next->mutex);
        sem_wait(&cur->mutex);
    }

    if (hunter->currentRoom->ghost != NULL) {
        hunter->boredom = 0; 
    }

    //Only moves if the next room has room to enter
    if (next->hunterCount < MAX_ROOM_OCCUPANCY) {
        
        adjustOccupy(hunter);
        addStack(hunter);

        log_move(hunter->id, hunter->boredom, hunter->fear, cur->name, next->name, hunter->tracker);

        hunter->currentRoom = next;
        hunter->currentRoom->hunterOccupy[hunter->currentRoom->hunterCount] = hunter;
        hunter->currentRoom->hunterCount++;
    }

    sem_post(&cur->mutex);
    sem_post(&next->mutex);
}

/* Determining if the room has a ghost and updates its fear and boredom

in/out: hunter; that specific hunter fear and boredom is being manipulated

return;
*/
void checkRoomStatus(Hunter* hunter) {
    sem_wait(&hunter->currentRoom->mutex);
    bool ghostHere = (hunter->currentRoom->ghost != NULL);
    sem_post(&hunter->currentRoom->mutex);

    if (ghostHere) {
        hunter->fear++;
        hunter->boredom = 0;
    } else {
        hunter->boredom++;
    }
}


/* Moves the room without adding it to the stack

in/out: hunter; that specific hunter room location is being manipulated

return;
*/
void hunterBacktrack(Hunter* hunter, Room* oldRoom) {
    adjustOccupy(hunter);
    log_move(hunter->id, hunter->boredom, hunter->fear, hunter->currentRoom->name, oldRoom->name, hunter->tracker);
    hunter->currentRoom = oldRoom;
    hunter->currentRoom->hunterOccupy[hunter->currentRoom->hunterCount] = hunter;
    hunter->currentRoom->hunterCount++;
}

/* Removes the hunter from the occupy of the room it is currently in 

in/out: hunter; that specific hunter room hunter count is being manipulated

return;
*/
void adjustOccupy(Hunter* hunter) {

    for (int i = 0; i < hunter->currentRoom->hunterCount; i++) {
        if (hunter->currentRoom->hunterOccupy[i] == hunter) {
            // Swap last to here
            hunter->currentRoom->hunterOccupy[i] = hunter->currentRoom->hunterOccupy[hunter->currentRoom->hunterCount - 1];
            // Remove pointer at the end just in case
            hunter->currentRoom->hunterOccupy[hunter->currentRoom->hunterCount - 1] = NULL;
            hunter->currentRoom->hunterCount--;
            return;
        }
    }

}

/* Determines if the hunter should exit the simulation and for what reason 

in/out: hunter; that specific hunter reason is being manipulated

return;
*/
void exitStatus(Hunter* hunter) {
    sem_wait(&hunter->caseFile->mutex);
    bool solved = hunter->caseFile->solved;
    sem_post(&hunter->caseFile->mutex);

    //Checks boredom reason and exits the simulation
    if (hunter->boredom >= ENTITY_BOREDOM_MAX) {
        sem_wait(&hunter->currentRoom->mutex);
        adjustOccupy(hunter);
        sem_post(&hunter->currentRoom->mutex);

        hunter->reason = LR_BORED;
        hunter->flag = true;
        log_exit(hunter->id, hunter->boredom, hunter->fear, hunter->currentRoom->name, hunter->tracker, hunter->reason);

    //Checks fear reason and exits the simulation
    } else if (hunter->fear >= HUNTER_FEAR_MAX) {
        sem_wait(&hunter->currentRoom->mutex);
        adjustOccupy(hunter);
        sem_post(&hunter->currentRoom->mutex);

        hunter->reason = LR_AFRAID;
        hunter->flag = true;
        log_exit(hunter->id, hunter->boredom, hunter->fear, hunter->currentRoom->name, hunter->tracker, hunter->reason);
    
    //Checks evidence reason and goes to the van
    } else if (solved) {
        hunter->reason = LR_EVIDENCE;
        hunter->exit_van = true;
    }
}