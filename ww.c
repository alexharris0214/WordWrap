#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define BUFFER_SIZE 16

/*
* char *output: a string containing a filename which needs an output file to write to
*
* Returns a string with the prefix "wrap." in front of the string passed to the char *output parameter.
*/
char *getOutputName(char *output){
    char *outputFileName = (char *)malloc(sizeof(output) + 5); // 5 is hardcoded to be length of "wrap." (5 character)

    // manually assigning the prefix
    outputFileName [0]= 'w';
    outputFileName [1]= 'r';
    outputFileName [2]= 'a';
    outputFileName [3]= 'p';
    outputFileName [4]= '.';

    // assigning the rest of the string according the input file name
    for(int i = 0; i<sizeof(output); i++){
        outputFileName[5+i] = output[i];
    }

    return outputFileName;
}

/*
*   int inputFD: a file descriptor which points to source of input data,
*   int lineLength: describes number of characters in a line for output formatting
*   int outputFD: a file descriptor pointing to the file to write formatted input to
*      (if NULL, outputs to STDOUT)
*   
*   Uses an input buffer to read data from input file fd,
*   processes data from input buffer character-by-character,
*   composing non-whitespace characters into words via output buffer,
*   and then using whitespace characters as cues for writing words 
*   or starting new paragraphs in the output file.
*/
int normalize(int inputFD, int lineLength, int outputFD){
    
    int lineSpot = 0;           //keeps track of the spot we are at in the current output file line
    int seenTerminator = 0;     //keeps track of consecutive newlines; if the last character was a newline, seenTerminator = 1
    int newPG = 0;              //keeps track of when to start a new paragraph before writing a new word
    int i;

    char buffer[BUFFER_SIZE];                                           //char array buffer for data read from input
    char *currWord = (char *)malloc(sizeof(char)*(lineLength + 1));     //string buffer for words to write to output
    int currWordSize = sizeof(char)*(lineLength + 1);                   //keep track of output buffer size for calculations & error-checking
    memset(currWord, '\0', currWordSize);                               //initialize buffer with null chars (it is a string)
    
    int bytesRead; // variable keeping track of how many bytes were read in

    bytesRead = read(inputFD, buffer, BUFFER_SIZE);
    if(bytesRead == 0){         //if file is empty, we can continue no further
        puts("ERROR: Input appears to contain zero bytes.");
        free(currWord);
        return EXIT_FAILURE;
    }         
    while(bytesRead > 0){
        for(i = 0; i < bytesRead; i++){         //iterate thru bytes read, one char at a time
            if(isspace(buffer[i])){ 

                if(buffer[i] == '\n'){
                    if(seenTerminator){
                        newPG = 1;
                        lineSpot = 0;
                    } else {
                        seenTerminator = 1;
                    }
                } else {
                    seenTerminator = 0;
                }

                if(currWord[0] != '\0'){        // go on to write currWord to output on reaching whitespace if currWord is non-empty
                    if(newPG){   //start a new paragraph before the next word is written if needed
                        write(outputFD, "\n\n", 2);
                        newPG = 0;
                        lineSpot = 0;
                    }
                    if(lineSpot == 0){                                  //if at the beginning of the line, write just the current token regardless of length
                        write(outputFD, currWord, strlen(currWord));
                        lineSpot += strlen(currWord);
                    } else if(lineSpot + 1 + strlen(currWord) <= lineLength){ //if not at the beginning of a line and the current word plus a space fits, add it to the current line
                        write(outputFD, " ", 1);
                        write(outputFD, currWord, strlen(currWord));    //write currWord preceded by a space
                        lineSpot += (1 + strlen(currWord));
                    } else {                                            //currWord does not fit at the end of the current line. start a new line
                        write(outputFD, "\n", 1);                       
                        write(outputFD, currWord, strlen(currWord));
                        lineSpot = strlen(currWord);
                    }
                    memset(currWord, '\0', currWordSize);           //clear currWord buffer and start a new word
                }

            } else {                                                //if the current character is non-whitespace, then we can add it to the write buffer
                seenTerminator = 0;
                if(strlen(currWord) == (currWordSize - 1)){         //if adding a character will exceed string buffer (-1 for the final \0 character), 
                    currWord = (char *)realloc(currWord, (currWordSize*2));   
                    currWordSize *= 2;      //currWordSize will serve as an indicator for word > lineLength error
                    memset((currWord + currWordSize/2), '\0', (currWordSize/2));   
                }
                currWord[strlen(currWord)] = (char)buffer[i];
            }
        }
        // Resetting the buffer before reassigning to it
        memset(buffer, '\0', sizeof(buffer));
            
        bytesRead = read(inputFD, buffer, BUFFER_SIZE);
    }
    //Need to print last word after program is finished reading input
    if(currWord[0] != '\0'){
        if(newPG){   //start a new paragraph before the next word is written if needed
            write(outputFD, "\n\n", 2);
            lineSpot = 0;
        }
        if(lineSpot == 0){                                        //if we're at the beginning of the line, just write the word
            write(outputFD, currWord, strlen(currWord));
        } else if(lineSpot + 1 + strlen(currWord) <= lineLength){ 
            write(outputFD, " ", 1);
            write(outputFD, currWord, strlen(currWord));          //write currWord preceded by a space
        } else {                                                  //if the word plus a space didn't fit, we need to put it on a new line
            write(outputFD, "\n", 1);
            write(outputFD, currWord, strlen(currWord));
        }
    }

    //every file must end in a newline
    write(outputFD, "\n", 1);

    if(currWordSize > (sizeof(char)*(lineLength + 1))){       //we had a word larger than a line; need to error out
        puts("ERROR: Input contains a word longer than specified line length.");
        free(currWord);
        return EXIT_FAILURE;
    }

    free(currWord);
    return EXIT_SUCCESS;
}


int main(int argc, char const *argv[])
{

    int fd = -1; // File pointer
    DIR *dr = NULL; // Directory pointer
    struct stat statbuf; // Holds file information to determine its type
    struct dirent *dp;  // Temp variable to hold individual temp entries
    int err; // Variable to store error information
    int exitFlag = EXIT_SUCCESS; //determine what exit status we return

    /*
    * checking to see if arguments were passed in before accessing
    * returns exit failure if no arguments were passed in 
    */
    if(argc>1){
        for(int i = 0; i < strlen(argv[1]); i++){ //check if length argument is a number
            if(!isdigit(argv[1][i])){               // checking each digit individually
                puts("ERROR: Invalid lineLength argument; must be an integer.");
                return EXIT_FAILURE;
            }
        }
    } else {
        puts("ERROR: Not enough arguments given.");
        return EXIT_FAILURE;
    }

    int length = atoi(argv[1]);

    /*
    * Checks to see if valid arguments are passed in
    * if it was, check to see if file argument is a directory or normal file
    * if not, exit with failure
    */
    if(argc == 3){
        err = stat(argv[2], &statbuf);
        // Checking to see if fstat returned a valid stat struct
        if(err){
            perror(argv[2]); //printing out what went wrong when accessing the file
            return EXIT_FAILURE;
        }

        // file argument is a directory
        if(S_ISDIR(statbuf.st_mode)){
            dr = opendir(argv[2]);
            if(dr <= 0 ){ // error checking to see if directory was opened
                puts("ERROR: Could not open directory.\n");
            }
            chdir(argv[2]); // Changing the working directory to have access to the files we need

            int outputFD;
            while ((dp = readdir(dr)) != NULL){ // while there are files to be read

                //ignoring files which start with "." or "wrap."
                if(strstr(dp->d_name, ".") == dp->d_name || strstr(dp->d_name, "wrap.") == dp->d_name){
                    continue;
                }
        
                // open the current file that we are on and work on it
                fd = open(dp->d_name, O_RDONLY);
                if(fd > 0 && dp->d_name){ // checking to see if file opened successfully
                    char *outputFilename = getOutputName(dp->d_name);
                    outputFD = open(outputFilename, O_WRONLY|O_CREAT|O_TRUNC, 0700);    // creating file if it does not exist with user RWX permissions
                    free(outputFilename);                                        
                    if(outputFD <= 0){                                                  // checking to see if open returned successfully 
                        puts("ERROR: Could not open/create desired output file.\n");
                        perror("outputFD");
                        return EXIT_FAILURE;
                    }
                    if(normalize(fd, length, outputFD) == EXIT_FAILURE)     //save exit status to return later, as the program still needs to process the rest of the directory
                        exitFlag = EXIT_FAILURE;
                    close(outputFD);
                }
                // close file when we are done
                close(fd);
            }
            // freeing directory object
            free(dr);
        } else if(S_ISREG(statbuf.st_mode)){ // file argument is a regular file
            fd = open(argv[2], O_RDONLY);
            if(fd <= 0){ // checking to see if file is opened successfully
                puts("ERROR: Could not open file.\n");
                return EXIT_FAILURE;
            }
            // reformatting file and assigning the result to return flag
            exitFlag = normalize(fd, length, 1);
            close(fd);
        } else {
            puts("ERROR: Invalid file type passed as argument.\n");
                return EXIT_FAILURE;
        }
    } else if(argc==2){     // no file was passed in; reading from standard input
        normalize(0, length, 1);
        return EXIT_FAILURE;
    } else {                // fall through case when not enough arguments are passed
        puts("ERROR: Not enough arguments.\n");
        return EXIT_FAILURE;
    }
    return exitFlag;
}