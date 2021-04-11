#include <stdio.h>

// Define an enum cardd


// Define a 3x3 array namend map containign values of type cardd


// The function set_dir should put the value dir at position x, y into the array map
// Check x, y and dir for validity
void set_dir (int x, int y, enum cardd dir)
{

}

// The function show_map should output the array as a 3x3 matrix
void show_map (void)
{

}

int main (void)
{
	// You are not allowed to change anything in this function!
	set_dir(0, 1, N);
	set_dir(1, 0, W);
	set_dir(1, 4, W);
	set_dir(1, 2, E);
	set_dir(2, 1, S);

	show_map();

	set_dir(0, 0, N|W);
	set_dir(0, 2, N|E);
	set_dir(0, 2, N|S);
	set_dir(2, 0, S|W);
	set_dir(2, 2, S|E);
	set_dir(2, 2, E|W);
	set_dir(1, 3, N|S|E);
	set_dir(1, 1, N|S|E|W);

	show_map();

	return 0;
}
