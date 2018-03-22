/*
	This file is part of procinfo-NG

	procinfo-NG is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; version 2.

	procinfo-NG is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with procinfo-NG; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <iostream>
#include <string>
#include <vector>
#include <fstream>

//#include "eventxx"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <termios.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <unistd.h>

#ifdef __CYGWIN__
#include <sys/select.h>
#endif

#include <ncurses.h>

using namespace std;

#include "lib/routines.cpp"
#include "lib/timeRoutines.cpp"
#include "lib/prettyPrint.cpp"

#define DEFAULT_INTERVAL 5
#define USER_HZ sysconf(_SC_CLK_TCK)
// this might be wrong for optical, but it might not!

#define VERSION "2.0"
#define REVISION "$Rev: 304 $"

// This really should use linkable objects, not includes. -.-

#include "interrupts.cpp"

#ifdef __CYGWIN__
#include "cygwin_procstat.cpp"
#else
#include "linux26_procstat.cpp"
#endif

#include "getmeminfo.cpp"
#include "rendercpupagestat.cpp"

#ifdef __CYGWIN__
#include "cygwin_rendercpupagestat.cpp"
#else
#include "linux26_rendercpupagestat.cpp"
#endif

#ifdef __linux__
#include "linux26_netstat.cpp"
#endif

inline double getUptime() {
	getUptime_label:
	vector <string> lines = readFile(string("/proc/uptime"));
	if(lines.size() == 0) { goto getUptime_label; };
	vector <string> tokens = splitString(" ", lines[0]);
	return string2double(tokens[0]);
}

inline string getLoadAvg() {
	vector <string> lines = readFile(string("/proc/loadavg"));
	return lines[0];
}

inline vector <string> renderBootandLoadAvg(const time_t &bootTime, const string &loadAvg) {
	vector <string> row;
	
	string bootTimeStr = string(ctime(&bootTime));
	// remove the "\n". don't ask me why ctime does that...
	bootTimeStr.erase(bootTimeStr.end()-1);
	row.push_back(string(string("Bootup: ") + bootTimeStr));
	row.push_back(string("Load average: " + loadAvg));
	return row;
}

inline uint32_t getCPUcount() __attribute__((always_inline));
inline uint32_t getCPUcount() { // has only one call-site.
	vector <string> lines = readFile(string("/proc/cpuinfo"));
	uint32_t CPUcount = 0;
	for(uint32_t i = 0; i < lines.size(); i++) {
		vector <string> tokens = splitString(" ", lines[i]);
		//printf("getCPUcount token0: %s\n", tokens[0].c_str());
		if (tokens.size() && tokens[0] == "processor") { // x86/x86_64 Cygwin
			CPUcount++;
		} else if (tokens.size() && tokens[0] == "processor\t:") { // x86/x86_64 Linux
			CPUcount++;
		} else if(tokens.size() && tokens[0] == "ncpus") { // SPARC
			CPUcount = string2uint32(tokens[2]); // untested, I don't have a SPARC yet
			break;
		} else if(tokens.size() && tokens[0] == "cpus" && tokens[1] == "detected\t:") { // Alpha
			CPUcount = string2uint32(tokens[2]); // untested, I don't have an Alpha yet
			break;
		} else {
			// do nothing
		}
	}
	return CPUcount;
}

#include "diskStats.cpp"

int mainLoop(bool perSecond, bool showTotals, bool showTotalsMem, bool fullScreen,
	bool showRealMemFree, bool showSectors, bool humanizeNums, bool partitionStats,
	bool skipIfaces,
	const uint32_t CPUcount, const vector <struct IRQ> &IRQs)
{
	static double oldUptime = 0;

	vector<vector <string> > rows;

	double uptime = getUptime();
	double elapsed = ( oldUptime != 0 ? uptime - oldUptime : 0 );
	if(fullScreen) // returns to home-position on screen.
		printf("\e[H");
	rows = getMeminfo(perSecond, showTotalsMem, showRealMemFree, humanizeNums, elapsed);

	vector <uint32_t> rowWidth(5, 10);
	rowWidth[0] = 6;
/*
	rowWidth.push_back(6);
	rowWidth.push_back(10);
	rowWidth.push_back(10);
	rowWidth.push_back(10);
	rowWidth.push_back(10);
*/
	prettyPrint(rows, rowWidth, false);
	rows.clear();
	print("\n");

/*
	// This isn't code, but rather documenting the contents of stats[][]
	// Please don't delete it.
	vector <uint64_t> cpuDiff = stats[0];
	vector <uint64_t> intrDiff = stats[1];
	uint64_t ctxtDiff = stats[2][0];
	uint64_t bootTime = stats[2][1];
*/
	vector <vector <uint64_t> > stats = getProcStat(showTotals, CPUcount, elapsed);

	//uint64_t pageInDiff, pageOutDiff, swapInDiff, swapOutDiff;
	vector <uint64_t> vmStat;
//#ifndef __CYGWIN__
		vmStat = getVMstat(showTotals);
//#endif

	string loadAvg = getLoadAvg();
	rows.push_back( renderBootandLoadAvg((time_t) stats[2][1], loadAvg) );
	prettyPrint(rows, false);
	rows.clear();
	//cout << endl;
	print("\n");

	rows = renderCPUandPageStats(perSecond, showTotals, elapsed, CPUcount, (uint64_t)(uptime * USER_HZ),
		 stats[0], stats[2][0], vmStat);
	prettyPrint(rows, false);
	rows.clear();
	//cout << endl;
	print("\n");


	rows = renderIRQs(perSecond, showTotals, elapsed, IRQs, stats[1]);
	prettyPrint(rows, false);
	//cout << endl;
	print("\n");
	rows.clear();

#ifndef __CYGWIN__
		vector <struct diskStat_t> diskStats = getDiskStats(showTotals, partitionStats);
		rows = renderDiskStats(perSecond, showTotals, showSectors, elapsed, diskStats);
		prettyPrint(rows, false);
		rows.clear();
#endif
#ifdef __linux__
	rowWidth.clear();

	rowWidth.resize(6, 15);
	rowWidth[0] = rowWidth[3] = 10;
/*
	rowWidth.push_back(10);
	rowWidth.push_back(15);
	rowWidth.push_back(15);
	rowWidth.push_back(10);
	rowWidth.push_back(15);
	rowWidth.push_back(15);
*/
	try {
		rows = getNetStats(perSecond, showTotals, skipIfaces, elapsed);
	} catch (string exceptionMessage) {
		print(exceptionMessage.c_str());
	}
	print("\n");
	prettyPrint(rows, rowWidth, true);
#endif
	rows.clear();
	refresh();
	//clear();
	erase();
	
	oldUptime = uptime;
	return 0;
}

int main(int argc, char *argv[]) {
	double interval = DEFAULT_INTERVAL;
	bool perSecond = false, showTotals = true, showTotalsMem = true, fullScreen = false;
	bool showRealMemFree = false, showSectors = false;
	bool humanizeNums = false, partitionStats = false;
	bool repeat = false;
	bool skipIfaces = true;
	extern char *optarg;
	int c;
	if(argc > 1) {
		perSecond = false; showTotals = true; showTotalsMem = true;
		while((c = getopt(argc, argv, "n:N:SDdrbhHvps")) != -1) {
		
			switch(c) {
				case 'n':
				case 'N':
					interval = string2double(optarg);
					// in case of a bum param. Can't allow interval <= 0
					interval = (interval > 0 ? interval : DEFAULT_INTERVAL);
					repeat = fullScreen = true;
					break;
				/*
				case 'f':
					// FIXME: 'f' has been removed from the options
					// as it always is in fullScreen mode now (ncurses)
					fullScreen = true;
					break;
				*/
				case 'S':
					perSecond = true;
					repeat = fullScreen = true;
					break;
				case 'D':
					showTotals = false;
					showTotalsMem = true;
					break;
				case 'd':
					showTotals = showTotalsMem = false;
					break;
				case 'r':
					showRealMemFree = true;
					break;
				case 'b':
					showSectors = true;
					break;
				case 'v':
					printf("procinfo version %s\n", VERSION);
					exit(0);
					break;
				case 'H':
					humanizeNums = true;
					break;
				case 'p':
					partitionStats = true;
					break;
				case 's':
					skipIfaces = false;
					break;
				case 'h':
				default:
					printf ("procinfo version %s %s\n"
						"usage: %s [-sidDSbhHv] [-nN]\n"
						"\n"
						"\t-nN\tpause N second between updates (implies -f)\n"
						"\t-d\tshow differences rather than totals (implies -f)\n"
						"\t-D\tshow current memory/swap usage, differences on rest\n"
						"\t-S\twith -nN and -d/-D, always show values per second\n"
						"\t-b\tshow number of bytes instead of requests for disk statistics\n"
						"\t-H\tshow memory stats in KiB/MiB/GiB\n"
						"\t-r\tshow memory usage -/+ buffers/cache\n"
						"\t-s\tDon't skip netdevs in /etc/procinfo/skipIfaces\n"
						"\t-h\tprint this help\n"
						"\t-v\tprint version info\n",
						VERSION, REVISION, argv[0]);
					exit (c == 'h' ? 0 : 1);
			}
		}
	} else {
		perSecond = true;
		interval = 0;
		fullScreen = false;
	}

	if(fullScreen) {
		printf("\e[2J");
		initConsole();
	}

	uint32_t CPUcount = getCPUcount();
	const struct timeval sleepInterval = { (int)interval, getFrac(interval, 1000000) };
	
#ifdef __CYGWIN__
	const vector <struct IRQ> IRQs;
#else
	const vector <struct IRQ> IRQs = getIRQs();
#endif
#ifdef __linux__
	loadNetdevSkipList();
#endif
	while(1) {
		fd_set fdSet;
		FD_ZERO(&fdSet);
		FD_SET(0, &fdSet);
		struct timeval sleepTime = sleepInterval; // select can modify sleepTime
		mainLoop(perSecond, showTotals, showTotalsMem, fullScreen,
			showRealMemFree, showSectors, humanizeNums, partitionStats,
			skipIfaces,
			CPUcount, IRQs);
		if(interval == 0 || repeat == false) {
			break;
		}
		int ret = select(1, &fdSet, NULL, NULL, &sleepTime);
		if(ret > 0) {
			char key = getchar();
			switch(key) {
				case 'f':
					fullScreen = !fullScreen;
					break;
				case 'S':
					perSecond = !perSecond;
				case 'D':
					showTotals = !showTotals;
					break;
				case 'd':
					showTotalsMem = !showTotalsMem;
					break;
				case 'r':
					showRealMemFree = !showRealMemFree;
					break;
				case 'b':
					showSectors = !showSectors;
					break;
				case 'h':
				case 'H':
					humanizeNums = !humanizeNums;
					break;
				case 'p':
				case 'P':
					partitionStats = !partitionStats;
					break;
				case 's':
					skipIfaces = !skipIfaces;
			}
			if(key == 'q' || key == 'Q') {
				break;
			}
			printf("\e[2J\n");
			clear();
		}
	};
	if(fullScreen)
		resetConsole();
	return 0;	
}
