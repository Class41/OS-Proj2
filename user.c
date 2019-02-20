#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
	printf("%s: Value got: %i", argv[0], atoi(argv[1]));
	return 0;
}
