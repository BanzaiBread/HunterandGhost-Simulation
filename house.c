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
  Initialize the House structure allocated memory to the heap for creating hunters

  in/out: house; initalizes this particular structure
  in/out; file; add the casefile to the house structure

  Return:
*/
void house_init(House* house, CaseFile* file){

    house->room_count = 0;    
    house->huntersMax = 4;
    house->currentHunters = 0;
    house->hunters = calloc( house->huntersMax , sizeof(Hunter));
    house->file = file;
    return;
};

