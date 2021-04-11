#include <stdio.h>

// Delete after fixing call_by_reference
int TODO;

void basic_pointer (int x)
{
	int* address_of_x;

	address_of_x = &x;

	printf("The value of x is %d\n", 0 /* TODO */);
	printf("The address of x is %p\n", NULL /* TODO */);
	printf("The address of x via address_of_x is %p\n", NULL /* TODO */);
	printf("The value of x via address_of_x is %d\n", 0 /* TODO */);
}

void basic_pointer2 (int x)
{
	int* address_of_x = NULL /* TODO */;
	// Another variable y gets the value of x
	int y = 0 /* TODO */;

	printf("The value of y is %d\n", 0 /* TODO */);

	// Assignment via address
	x = 10;
	y = *address_of_x;

	printf("The value of y is %d\n", 0 /* TODO */);
}

void basic_pointer_changeValue (int x)
{
	int* address_of_x = NULL /* TODO */;

	// Change the value of x to 10
	TODO = 10;
	printf("x = %d\n", 0 /* TODO */);

	// Change the value of x via its address
	TODO = 20;
	printf("x = %d\n", x);
}


void call_by_reference (int* x)
{
	// Change the value that is stored at the address stored in x
	TODO = 200;
}

int main (void)
{
	int x = 5;

	basic_pointer(x);
	basic_pointer2(x);
	basic_pointer_changeValue(x);

	printf("The value of x before call_by_reference is %d\n",x);
	call_by_reference(&x);
	printf("The value of x after call_by_reference is %d\n",x);

	return 0;
}
