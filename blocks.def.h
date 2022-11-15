//Modify this file to change what commands output to your statusbar, and recompile using the make command.
static const Block blocks[] = {
	/*Icon*/	/*Command*/		/*Update Interval*/	/*Update Signal*/
	//{"Mem:", "free -h | awk '/^Mem/ { print $3\"/\"$2 }' | sed s/i//g",	30,		0},

	//{"", "date '+%b %d (%a) %I:%M%p'",					5,		0},
	//{"",    "sb-tasks", 10, 26},
    //{"", "cat /tmp/recordingicon 2>/dev/null",  0,  9},

    //{"", "sb-music",         0,  11},
    //{"", "sb-pacpackages",   0,  8},
    //{"", "sb-music",         1,  11},
    //{"", "sb-pacpackages",   3600,  8},
    //{"", "sb-cpu",           10, 18},

    {"", "sb-volume",       0,  10},
    {"", "sb-forecast",     600,  5},
    {"", "sb-memory",       1, 14},
    {"", "sb-clock",        30, 1},
};

//sets delimeter between status commands. NULL character ('\0') means no delimeter.
static char delim[] = " | ";
static unsigned int delimLen = 5;
