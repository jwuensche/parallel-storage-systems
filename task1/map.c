#include <stdio.h>

// Define an enum cardd
enum cardd
{
	N = 1,
	S = 2,
	W = 4,
	E = 8
};

// Define a 3x3 array namend map containign values of type cardd
enum cardd map[9];

// The function set_dir should put the value dir at position x, y into the array map
// Check x, y and dir for validity

// i would like to note that i switched x and y here because i cant let x go downwards and y to the left, it just feels wrong
void set_dir (int y, int x, enum cardd dir)
{
	//create enum that should be here
	//x is the index modulo 3
	// 0 1 2
	// 0 1 2
	// 0 1 2
	//y is the index divided by 3
	// 0 0 0
	// 1 1 1
	// 2 2 2
	//create the value that should be in (x,y)
	//since the values of the enums are bitwise disjoint i use +, does the same as | in this case
	enum cardd now = (y==0)*N + (y==2)*S + (x==0)*W + (x==2)*E;
	//compare with what we have
	if(now == dir)
	{
		//set it
		map[y*3 + x] = dir;
	}	
}

// The function show_map should output the array as a 3x3 matrix
void show_map (void)
{
	for(int i=0;i<3;++i)
	{
		for(int j=0;j<3;++j)
		{
			//i am to lazy to make it fancy, have switch
			//i hope i dont have to error handle printf now ...
			switch((int)(map[i*3 + j]))
			{
				case N:
					printf(" N ");
					break;
				case S:
					printf(" S ");
					break;
				case W:
					printf("W  ");
					break;
				case E:
					printf("  E");
					break;
				case N|W:
					printf("NW ");
					break;
				case S|W:
					printf("SW ");
					break;
				case N|E:
					printf(" NE");
					break;
				case S|E:
					printf(" SE");
					break;
				default:
					switch(j)
					{
					case 0:
						printf("0  ");
						break;
					case 1:
						printf(" 0 ");
						break;
					case 2:
						printf("  0");
						break;
					}
					
			}
		}
		printf("\n");
	}
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
