/*
 * Simple error demonstration to demonstrate power of Valgrind.
 * Julian M. Kunkel - 17.04.2008
 */

#include <stdio.h>
#include <stdlib.h>

int*
mistake1(void)
{
	int* buf = malloc(sizeof(int) * 6);
	buf[0] = 1;
	buf[1] = 1;
	buf[2] = 2;
	buf[3] = 3;
	buf[4] = 4;
	buf[5] = 5;

	return buf;
}

int*
mistake2(void)
{
	int* buf = malloc(sizeof(int) * 4);

	buf[1] = 2;

	return buf;
}

int*
mistake3(void)
{
	/* This function is not allowed to allocate memory directly. */
	// int mistake2_ = 0;
	int* buf = (int*)mistake2();

	buf[0] = 3;

	return buf;
}

int*
mistake4(void)
{
	int* buf = malloc(sizeof(int));

	buf[0] = 4;

	return buf;
}

int
main(void)
{
	/* Do NOT modify the following line. */
	int* p[4] = { &mistake1()[1], &mistake2()[1], mistake3(), mistake4() };

	printf("1: %d\n", *p[0]);
	printf("2: %d\n", *p[1]);
	printf("3: %d\n", *p[2]);
	printf("4: %d\n", *p[3]);

	/* Add the correct calls to free() here. */
	free(&p[1][-1]); /* What was the correct pointer here? FIXME */
	free(&p[0][-1]);
	free(p[2]);
	free(p[3]);

	return 0;
}
