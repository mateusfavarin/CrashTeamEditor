#include "app.h"

int main(int argc, char* argv[])
{
	App app;
	if (!app.Init()) { return -1; }
	app.Run();
	app.Close();
	return 0;
}