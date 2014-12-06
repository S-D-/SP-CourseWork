#include "utils.h"
#include <stdlib.h>
#include <string.h>

void makeLine(char* remstr, 
              int remsize, 
              char* str, 
              int strsize, 
              char** line, //out, free
              int* linesize, //out
              char** newrem, //out, free
              int* newremsize) // out
{
    int resLineSizeVal;
    int newRemSizeVal;
    char* resLine;
    char* resRem;
    
    int remNewLineIdx = -1;
    for (int i = 0; i < remsize; ++i) {
        if (remstr[i] == '\n') {
            remNewLineIdx = i;
            break;
        }
    }
    if (remNewLineIdx != -1) {
        resLineSizeVal = remNewLineIdx+1;
        resLine = malloc(resLineSizeVal + 1);
        memcpy(resLine, remstr,resLineSizeVal);
        resLine[resLineSizeVal] = '\0';
        
        newRemSizeVal = remsize - resLineSizeVal + strsize;
        resRem = malloc(newRemSizeVal);
        int s1 = remsize - resLineSizeVal;
        memcpy(resRem, &remstr[resLineSizeVal], s1);
        memcpy(&resRem[s1], str, strsize);
    } else {
        int strNewLineIdx = -1;
        for (int i = 0; i < strsize; ++i) {
            if (str[i] == '\n') {
                strNewLineIdx = i;
                break;
            }
        }
        if (strNewLineIdx != -1) {
            resLineSizeVal = remsize + strNewLineIdx + 1;
            resLine = malloc(resLineSizeVal + 1);
            memcpy(resLine, remstr,remsize);
            memcpy(&resLine[remsize], str, strNewLineIdx + 1);
            resLine[resLineSizeVal] = '\0';
            
            newRemSizeVal = strsize - strNewLineIdx - 1;
            resRem = malloc(newRemSizeVal);
            memcpy(resRem, &str[strNewLineIdx + 1], newRemSizeVal);
        } else {
            resLineSizeVal = 0;
            resLine = NULL;
            
            newRemSizeVal = remsize + strsize; 
            resRem = malloc(newRemSizeVal);
            memcpy(resRem, remstr, remsize);
            memcpy(resRem, str, strsize);
        }
    }
    *line = resLine;
    *linesize = resLineSizeVal;
    *newrem = resRem;
    *newremsize = newRemSizeVal;
}

void nextLine(char* newChars, 
              int size, 
              char** resLine, //out, free
              int* resLineSize) //out
{
    static char* remainder = NULL; // should be not static
    static int remSize = 0;
    char* newRem;
    int newRemSize;
    makeLine(remainder,
             remSize,
             newChars,
             size,
             resLine,
             resLineSize,
             &newRem,
             &newRemSize);
    free(remainder);
    remainder = newRem;
    remSize = newRemSize;
}
