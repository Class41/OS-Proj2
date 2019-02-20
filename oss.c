#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>


int main()
{
	int optionItem;
	int childMax;
	int childConcurMax;
	
	char* inputName = malloc(50 * sizeof(char) + 1); //store filenames
	char* outputName = malloc(50 * sizeof(char) + 1);

	strcpy(inputName, "input.dat"); //put initial strings into the malloc
	strcpy(outputName, "output.dat");
	while ((optionItem = getopt(argc, argv, "hi:o:n:s:")) != -1) //read option list
	{
			switch (optionItem)
			{
			case 'h': //show help menu
					printf("\t%s Help Menu\n\
		\t-h : show help dialog \n\
		\t-i [filename] : input filename. default: input.dat\n\
		\t-o [filename] : output filename. default: output.dat\n", argv[0]);
					return;
			case 'i': //change input
					strcpy(inputName, optarg);
					printf("\n%s: Info: set input file to: %s", argv[0], optarg);
					break;
			case 'o': //change output
					strcpy(outputName, optarg);
					printf("\n%s: Info: set output file to: %s", argv[0], optarg);
					break;
			case 's': //max child at once
					childConcurMax = optarg;
					printf("\n%s: Info: set max concurrent children to: %s", argv[0], optarg);
					break;
			case 'n': //total # of children
					childMax = optarg;
					printf("\n%s: Info: set max children to: %s", argv[0], optarg);
					break;
			case '?': //an error has occoured reading arguments
					printf("\n%s: Error: Invalid Argument or Arguments missing. Use -h to see usage.", argv[0]);
					return;
			}
	}

	printf("\n%s: Info: input file: %s, output file: %s", argv[0], inputName, outputName);

	FILE* input = fopen(inputName, "r"); //open input/output files specified
	FILE* output = fopen(outputName, "wr");

	if (input == NULL) //check if the input file exists
	{
			printf("\n%s: ", argv[0]);
			fflush(stdout);
			perror("Error: Failed to open input file");
			return;
	}
	
	return 0;
}