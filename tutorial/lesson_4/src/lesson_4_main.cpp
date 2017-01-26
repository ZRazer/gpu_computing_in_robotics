#include <GL/freeglut.h>

//PCL
#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
//#include <pcl/point_representation.h>
#include <pcl/io/pcd_io.h>
//#include <pcl/filters/voxel_grid.h>
//#include <pcl/filters/filter.h>
//#include <pcl/features/normal_3d.h>
//#include <pcl/registration/transforms.h>
//#include <pcl/registration/ndt.h>
#include <pcl/console/parse.h>
//#include <pcl/registration/icp.h>
//#include <pcl/common/time.h>
//#include <pcl/filters/voxel_grid.h>
//#include <pcl/filters/statistical_outlier_removal.h>
//#include <pcl/PCLPointCloud2.h>

#include "cudaWrapper.h"

const unsigned int window_width  = 512;
const unsigned int window_height = 512;
int mouse_old_x, mouse_old_y;
int mouse_buttons = 0;
float rotate_x = 0.0, rotate_y = 0.0;
float translate_z = -20.0;
float translate_x, translate_y = 0.0;

float search_radius = 0.5f;
int number_of_points_in_search_sphere_threshold = 3;
int max_number_considered_in_INNER_bucket = 1000;
int max_number_considered_in_OUTER_bucket = 1000;

pcl::PointCloud<pcl::PointXYZ> point_cloud;
CCudaWrapper cudaWrapper;

bool initGL(int *argc, char **argv);
void display();
void keyboard(unsigned char key, int x, int y);
void mouse(int button, int state, int x, int y);
void motion(int x, int y);
void printHelp();

int main(int argc, char **argv)
{
	std::cout << "Lesson 4 - noise removal" << std::endl;

		if(argc < 2)
		{
			std::cout << "Usage:\n";
			std::cout << argv[0] <<" point_cloud_file.pcd parameters\n";
			std::cout << "-sr search_radius: default " << search_radius << std::endl;
			std::cout << "-npt number_of_points_in_search_sphere_threshold: default " << number_of_points_in_search_sphere_threshold << std::endl;

			std::cout << "Default:  ../../data/scan_Velodyne_VLP16.pcd\n";

			if(pcl::io::loadPCDFile("../../data/scan_Velodyne_VLP16.pcd", point_cloud) == -1)
			{
				return -1;
			}
		}else
		{
			std::vector<int> ind_pcd;
			ind_pcd = pcl::console::parse_file_extension_argument (argc, argv, ".pcd");

			if(ind_pcd.size()!=1)
			{
				std::cout << "did you forget pcd file location? return" << std::endl;
				return -1;
			}

			if(pcl::io::loadPCDFile(argv[1], point_cloud) == -1)
			{
				return -1;
			}

			pcl::console::parse_argument (argc, argv, "-sr", search_radius);
			std::cout << "search_radius for noise removal: " << search_radius << std::endl;

			pcl::console::parse_argument (argc, argv, "-npt", number_of_points_in_search_sphere_threshold);
			std::cout << "number_of_points_in_search_sphere_threshold: " << number_of_points_in_search_sphere_threshold << std::endl;
		}


	if (false == initGL(&argc, argv))
	{
		return -1;
	}

	printHelp();
	cudaWrapper.warmUpGPU();

	for(int i = 0; i < 10000; i++)
	{
		pcl::PointXYZ p;
		p.x = (((float(rand()%100000)) / 100000.0f) - 0.5f) * 50.0f;
		p.y = (((float(rand()%100000)) / 100000.0f) - 0.5f) * 50.0f;
		p.z = (((float(rand()%100000)) / 100000.0f) - 0.5f) * 50.0f;
		point_cloud.push_back(p);
	}

	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutMouseFunc(mouse);
	glutMotionFunc(motion);
	glutMainLoop();
}

bool initGL(int *argc, char **argv)
{
    glutInit(argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Lesson 4 - noise removal");
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutMotionFunc(motion);

    glClearColor(0.0, 0.0, 0.0, 1.0);
    glDisable(GL_DEPTH_TEST);

    glViewport(0, 0, window_width, window_height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (GLfloat)window_width / (GLfloat) window_height, 0.01, 10000.0);

    return true;
}

void display()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(translate_x, translate_y, translate_z);
    glRotatef(rotate_x, 1.0, 0.0, 0.0);
    glRotatef(rotate_y, 0.0, 0.0, 1.0);

    glBegin(GL_LINES);
   	glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(1.0f, 0.0f, 0.0f);

    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 1.0f, 0.0f);

    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 1.0f);
    glEnd();

    glColor3f(1.0f, 1.0f, 1.0f);
	glBegin(GL_POINTS);
	for(size_t i = 0; i < point_cloud.size(); i++)
	{
		glVertex3f(point_cloud[i].x, point_cloud[i].y, point_cloud[i].z);
	}
	glEnd();



    glutSwapBuffers();
}

void keyboard(unsigned char key, int /*x*/, int /*y*/)
{
    switch (key)
    {
        case (27) :
            glutDestroyWindow(glutGetWindow());
            return;
        case 'r' :
        {
        	clock_t begin_time;
			double computation_time;
			begin_time = clock();

        	if(!cudaWrapper.removeNoise(
        			point_cloud,
        			search_radius,
        			number_of_points_in_search_sphere_threshold,
        			max_number_considered_in_INNER_bucket,
        			max_number_considered_in_OUTER_bucket))
			{
				cudaDeviceReset();
			}
        	computation_time=(double)( clock () - begin_time ) /  CLOCKS_PER_SEC;
        	std::cout << "cudaWrapper.removeNoise computation_time: " << computation_time << std::endl;

			break;
        }

    }
    glutPostRedisplay();
    printHelp();
}

void mouse(int button, int state, int x, int y)
{
    if (state == GLUT_DOWN)
    {
        mouse_buttons |= 1<<button;
    }
    else if (state == GLUT_UP)
    {
        mouse_buttons = 0;
    }

    mouse_old_x = x;
    mouse_old_y = y;
}

void motion(int x, int y)
{
    float dx, dy;
    dx = (float)(x - mouse_old_x);
    dy = (float)(y - mouse_old_y);

    if (mouse_buttons & 1)
    {
        rotate_x += dy * 0.2f;
        rotate_y += dx * 0.2f;

    }
    else if (mouse_buttons & 4)
    {
        translate_z += dy * 0.05f;
    }
    else if (mouse_buttons & 3)
    {
            translate_x += dx * 0.05f;
            translate_y -= dy * 0.05f;
    }

    mouse_old_x = x;
    mouse_old_y = y;

    glutPostRedisplay();
}

void printHelp()
{
	std::cout << "----------------------" << std::endl;
	std::cout << "press 'r': remove noise" << std::endl;
	std::cout << "press 'Esc': EXIT" << std::endl;
}
