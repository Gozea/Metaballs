#include <iostream>
#include <vector>
#include <algorithm>
#include <GL/glut.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <eigen3/Eigen/Dense>

using namespace std;
using namespace Eigen;

//window dimensions
const int winX = 1024;
const int winY =  640;
const int GRID = 16;

//we want the coordinate to live in [0,4]**2 (for the "fun" implicit function)
const float scX1 = (float)1/256;
const float scY1 = (float)1/160;

//if we want normal scale
const float scX2 = 1;
const float scY2 = 1;


//the size of the grid
const int grX = winX/GRID;
const int grY = winY/GRID;

//center (x0, y0) of our implicit function, don't forget it's scaled in the space [0,4]**2
//center is supposed to be on (2,2)
const float centerX = (float)winX/2*scX1;
const float centerY = (float)winY/2*scY1;

//The constant term of the implicit equation
const float c = 1;

//constant data on balls
const int minrad = 5;
const int maxrad = 30;
const int maxvel = 10;

const int nballs = 8;


struct grid {
	int x;
	int y;
	int value;

	bool operator==(grid g) { return x == g.x && y == g.y && value == g.value;}
};
grid gr[grX*grY];


class metaball {
	private:
		int x = rand()%winX;
		int y = rand()%winY;
		int r = rand()%maxrad +minrad;
		int vx = rand()%maxvel + 1;
		int vy = rand()%maxvel + 1;
	public:
		Vector2f getCoord() {
			Vector2f res; res.x() = this->x; res.y() = this->y; return res;
		}
		float getR() { return (float)this->r;}
		void move() {
			this->x += vx;
			this->y += vy;
			this->vx = ((this->vx < 0 && (this->x - this->r) < 0) || (this->vx > 0 && (this->x + this->r) >= winX)) ? -this->vx : this->vx;
			this->vy = ((this->vy < 0 && (this->y - this->r) < 0) || (this->vy > 0 && (this->y + this->r) >= winY)) ? -this->vy : this->vy;
		}
}; metaball* meta =  new metaball[nballs];



//implicit function of the metaballs
float metafun(float x, float y) {
	float sum = 0;
	for (int i = 0 ; i < nballs ; i++) {
		Vector2f coords = meta[i].getCoord();
		sum += meta[i].getR()/sqrt(pow(x-coords.x(), 2) + pow(y-coords.y(), 2));
	}
	return sum;
}

//implicit function to draw a reversed-mickey-like shape
float fun(float x, float y) {
	return pow(pow(x-centerX, 2) + pow(y-centerY, 2) - 1, 3) - pow(x-centerX, 2)*pow(y-centerY, 3);
}



//drawing and initializing the grid to perform the marching_square on
void drawGrid(float(*f)(float, float), float scaleX, float scaleY) {
	for (int i = 0 ; i < grX; i++) {
		for (int j = 0 ; j < grY ; j++) {
			gr[i*grY+j].x = (grX-i)*GRID; 	
			gr[i*grY+j].y = j*GRID; 	
			if (f((float)gr[i*grY+j].x*scaleX, (float)gr[i*grY+j].y*scaleY) < c) {
				gr[i*grY+j].value = 0; 	
				glPointSize(2); glColor3f(1,0, 0); glBegin(GL_POINTS); glVertex2i(gr[i*grY+j].x, gr[i*grY+j].y); glEnd();
			} else {
				gr[i*grY+j].value = 1; 	
				glPointSize(2); glColor3f(0,1, 0); glBegin(GL_POINTS); glVertex2i(gr[i*grY+j].x, gr[i*grY+j].y); glEnd();
			}
		}
	}
}


//find neighbors through iterator
//NB don't compare iterator, compare on values (override operator if needed)
vector<grid> neighbors(vector<grid> square, vector<grid>::iterator n) {
	vector<grid> nbors;
	auto before = n; 
	before = (*n == *(square.begin())) ? --(square.end()) : --before; grid bf = *before;
	auto after = n;
	after = (*n == *(--square.end())) ?  square.begin() : ++after ; grid at = *after;
	nbors.push_back(bf) ; nbors.push_back(at);
	return nbors;
}

//rescale x from space [a0, a1] to space [b0, b1]
float rescale(float x, float a0, float a1, float b0, float b1) {
	return (x-a0)/(a1-a0)*(b1-b0)+b0;
} 

//linear interpolation between the two elements of grid to get the appoximated value of the implicit function
Vector2f interpolate(float(*f)(float, float), grid gr1, grid gr2, float scaleX, float scaleY) {
	Vector2f res;
	//values from the function
	float v1 = f(gr1.x*scaleX, gr1.y*scaleY);
	float v2 = f(gr2.x*scaleX, gr2.y*scaleY);
	if (gr1.x == gr2.x) {
		//we keep the same x-axis
		res.x() = gr1.x;
		//interpolate the y-axis
		res.y() = gr1.y + (c-v1)/(v2-v1)*(gr2.y-gr1.y);
	} else {
		//we keep the same y-axis
		res.y() = gr1.y;
		//interpolate the x-axis
		res.x() = gr1.x + (c-v1)/(v2-v1)*(gr2.x-gr1.x);
	}	
	return res;
}


//check the square of a given index and draw if needed
void square(float(*f)(float, float), int x, int y, float scaleX, float scaleY) {
	//we pick 4 element of the grid to form a square
	vector<grid> square ={gr[x*grY+y], gr[x*grY+(y+1)], gr[(x+1)*grY+(y+1)],gr[(x+1)*grY+y]};
	//the number of "1 values" in the square will determine the way we'll draw
	int sum = 0;
	for_each(square.begin(), square.end(), [&sum](grid gr) { sum+=gr.value;});
	vector<grid> nb;
	Vector2f l1;
	Vector2f l2;
	switch(sum) {
		case 1: 
			{
				//we trace the line by interpolation with the neighbors of the "one value"
				auto find_one = find_if(square.begin(), square.end(), [](grid g) {return g.value == 1;});
				grid one = *find_one;
				nb = neighbors(square, find_one);
				l1 = interpolate(f, one, nb[0], scaleX, scaleY);
				l2 = interpolate(f, one, nb[1], scaleX, scaleY);
				glColor3f(1,1,0);glBegin(GL_LINES); glVertex2d(l1.x(), l1.y()); glVertex2d(l2.x(), l2.y()); glEnd(); glFlush();
				nb.clear(); 
			}
			break;
		case 2: 
			{
				//we have to search for our two "one values"
				auto fo1 = find_if(square.begin(), square.end(), [](grid g) {return g.value == 1;});
				auto m = fo1;
				auto fo2 = find_if(++m,  square.end(), [](grid g) {return g.value == 1;});
				//we search for the neighbors of the "one values", we check if a "one value" is in the neighbors of the other
				vector<grid> n1 = neighbors(square, fo1);
				vector<grid> n2 = neighbors(square, fo2);
				auto fi = find(n1.begin(), n1.end(), *fo2);
				if (fi != n1.end()) {
					//the "1 values" are found as direct neighbors
					//we search for the 0 value, the first neighbor is interpolated with first pointer, second neighbor with second pointer
					vector<grid> nbors;
					copy_if(n1.begin(), n1.end(), back_inserter(nbors), [](grid g) {return g.value == 0;});
					copy_if(n2.begin(), n2.end(), back_inserter(nbors), [](grid g) {return g.value == 0;});
					n1.clear(); n2.clear();
					//interpolation and drawing
					l1 = interpolate(f, *fo1, nbors[0], scaleX, scaleY);
					l2 = interpolate(f, *fo2, nbors[1], scaleX, scaleY);
					glColor3f(1,1,0);glBegin(GL_LINES); glVertex2d(l1.x(), l1.y()); glVertex2d(l2.x(), l2.y()); glEnd(); glFlush();
					nbors.clear();
				} else {
					//the "1 values" are on opposite sides of the square
					Vector2f l11 = interpolate(f, *fo1, n1[0], scaleX, scaleY);
					Vector2f l12 = interpolate(f, *fo1, n1[1], scaleX, scaleY);
					Vector2f l21 = interpolate(f, *fo2, n2[0], scaleX, scaleY);
					Vector2f l22 = interpolate(f, *fo2, n2[1], scaleX, scaleY);
					glColor3f(1,1,0);glBegin(GL_LINES); glVertex2d(l11.x(), l11.y()); glVertex2d(l12.x(), l12.y()); glEnd(); glFlush();
					glColor3f(1,1,0);glBegin(GL_LINES); glVertex2d(l21.x(), l21.y()); glVertex2d(l22.x(), l22.y()); glEnd(); glFlush();
					n1.clear(); n2.clear();
				}
			}
			break;
		case 3:
			{
				//we trace the line by interpolation with the neighbors of the "one value"
				auto find_zero = find_if(square.begin(), square.end(), [](grid g) {return g.value == 0;});
				grid zero = *find_zero;
				nb = neighbors(square, find_zero);
				l1 = interpolate(f, zero, nb[0], scaleX, scaleY);
				l2 = interpolate(f, zero, nb[1], scaleX, scaleY);
				glColor3f(1,1,0); glBegin(GL_LINES); glVertex2d(l1.x(), l1.y()); glVertex2d(l2.x(), l2.y()); glEnd(); glFlush();
				nb.clear(); 
			}
			break;
		default:
			//we don't draw if only "one values" or only "zero values" in the square
			break;
	}
}

//marching square algorithm
//we apply the square function on each element of the grid
//neighbors elements will be picked to draw in the selected square
void marching_square(float(*f)(float, float), float scaleX, float scaleY) {
	for(int i = 0 ; i < (grX-1) ; i++) {
		for (int j = 0 ; j < (grY-1); j++) {
			square(f, i, j, scaleX, scaleY);
		}
	}
}



//display function 
void display() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	drawGrid(metafun, scX2, scY2);
	marching_square(metafun, scX2, scY2);
	for (int i = 0 ; i < nballs ; i++) {
		meta[i].move();
	}
	glutSwapBuffers();
	glutPostRedisplay();
}

int main(int argc, char* argv[]) {
	 glutInit(&argc, argv);
	 glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	 glutInitWindowSize(winX,winY);
	 glutCreateWindow("Implicit function");
	 gluOrtho2D( 0.0, winX, winY, 0.0);
	 glutDisplayFunc(display);
	 glutMainLoop();
}
