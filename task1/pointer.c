#include <stdio.h>


void basic_pointer (int x)
{
	int* address_of_x;

	address_of_x = &x;

	printf("The value of x is %d\n", x);
	// man 3 printf - %p is defined to print void * in particular in hexadecimal
	printf("The address of x is %p\n", (void*)&x);
	printf("The address of x via address_of_x is %p\n", (void*)address_of_x);
	// deref
	printf("The value of x via address_of_x is %d\n", *address_of_x);
}

void basic_pointer2 (int x)
{
	// Take reference with &
	int* address_of_x = &x;
	// Another variable y gets the value of x
	// We can simply copy the value here directly
	int y = x;

	printf("The value of y is %d\n", y);

	// Assignment via address
	x = 10;
	y = *address_of_x;

	printf("The value of y is %d\n", y);
}

void basic_pointer_changeValue (int x)
{
	int* address_of_x = &x;

	// Change the value of x to 10
	// Passed by value so no problem
	x = 10;
	printf("x = %d\n", x);

	// Change the value of x via its address
	// Dereference to write on space behind pointer
	*address_of_x = 20;
	printf("x = %d\n", x);
}


void call_by_reference (int* x)
{
	// Change the value that is stored at the address stored in x
	// Dereference to write on space behind pointer
	*x = 200;
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
