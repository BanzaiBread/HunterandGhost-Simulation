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
  Initialize the CaseFile that contains the shared file that all hunters can access

  in/out: collected; contains the shared evideence the hunters got 
  in/out: solved; determines if the case was solved
  in/out: mutex; the semphore for the casefile

  Return:
*/
void caseFile_init(CaseFile* file ){

    file->collected = 0;
    file->solved = false;
    sem_init(&file->mutex, 0, 1);
}


/*
  Checks if the evidence is collected and solved 

  in/out: mask; the total amount of evidecen collected from the hunters

  Return:
*/
bool evidence_has_three_unique(EvidenceByte mask) {
    int count = 0;
    // Iterate through the 7 possible evidence bits
    for (int i = 0; i < 7; i++) {
        if ((mask >> i) & 1) {
            count++;
        }
    }
    return count >= 3;
}


