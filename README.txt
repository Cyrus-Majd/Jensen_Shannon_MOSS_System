CS 214
PROJECT II

----=|JENSON SHANNON MEASURE OF SOFTWARE SIMILARITY SYSTEM|=----

AUTHORS: Cyrus Majd [cm1355] and Kayla Kam [kak424]


Program Description:
	A measure of software similarity (MOSS) system that employs the Jenson-Shannon Distance of two files as the basis for comparing similarity
	between those files. This program compares the similarity of .txt files, but can be easily modified to accomodate other kinds of files.

How to use the program:

	- COMPILATION: Compile "compare.c" with the following command: "gcc -g -fsanitize=address compare.c -lpthread -lm -o compare"
		       Alternatively, you could run "make" with the included makefile.
	- EXECUTION: To use the MOSS system, call the executable "./compare" and pass in at least one arguement.

Arguements:

	- acceptable arguements into this program are:
		1) Directories
		2) Files
		3) Thread-specific parameters (-dN, -fN, -aN, -sS)
	- UNACCEPTABLE arguements for this program are:
		1) a total of less than two files (for the compare program to work, we need at least two files to compare with eachother)
		2) non-text files (the compare program will NOT execute on files ending in extentions other than '.txt')

Program structure:

	The program first reads in all arguements from the command line, and carefully traverses any found directories to populate a mutex-protected
	synchronous file queue structure. The contents from this file queue are then read from and a WFD structure is produced for each file, containing 
	a list of all words in each file and their respective frequencies in said files. These WFD structures are then compiled into one big mutex-protected 
	WFD repository, which contains the WFD calculations for each file. Next, the Jenson Shannon Distance is calculated, drawing information from the 
	WFD repository, and calculating the JSD for every combination of pairs of files synchronously. It is important to reiterate that our program 
	generates every possible COMBINATION, not permutation, of pairs of files. After the JSD for every combination is calculated, the results are 
	stored in a mutex-protected masterlist, containing both all possible combinations and their respective total word count. The contents of this 
	JSD masterlist is then sorted via a custom quicksort implementation, and the results of this sorted masterlist is then printed out to the screen. 
	Extra care has been taken to close all opened files and ensure no memory leaks occur.

Test cases:

	- Handles unexpected arguements
	- Handles duplicate files
	- Handles duplicate directories
	- Handles large numbers of files and directories
	- Handles large text files
	- Handles hidden files (skips them)


:)
