/*
 * Simple error demonstration to demonstrate power of Valgrind.
 * Julian M. Kunkel - 17.04.2008
 */

#include <stdio.h>
#include <stdlib.h>

int*
mistake1(void)
{
	int buf[] = { 1, 1, 2, 3, 4, 5 };

	return buf;
}

int*
mistake2(void)
{
	int* buf = malloc(sizeof(char) * 4);

	buf[2] = 2;

	return buf;
}

int*
mistake3(void)
{
	/* This function is not allowed to allocate memory directly. */
	int mistake2_ = 0;
	int* buf = (int*)&mistake2;

	buf[0] = 3;

	return buf;
}

int*
mistake4(void)
{
	int* buf = malloc(sizeof(char) * 4);

	buf[4] = 4;
	free(buf);

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
	free(p[1]); /* What was the correct pointer here? FIXME */

	return 0;
}
