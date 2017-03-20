/* gpstime.cpp copyright (C) Erwin Bejsta VK3ERW 2017
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/* required a GPS with a serial output set for 9600 Baud connected to serial0 
 * designed to work on Raspberry Pi
 */

#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     //for UART
#include <fcntl.h>     //for UART
#include <termios.h>     //for UART

#include <sys/types.h>
#include <sys/wait.h>

#include <iostream>
//#include <iomanip>
#include <cstdlib>

#include "gps.h"

#define PORT "/dev/serial0"
#define BAUDRATE B9600
#define WAIT_FOR_COMPLETION

static int receivedSignal = 0;

/** Handle process signals
 * call by OS when our process receives a signal
 * @param signum the signal
 * @return
 */
void sigHandler(int signum)
{
    char signame[10];
    switch (signum) {
        case SIGTERM:
            strcpy(signame, "SIGTERM");
            break;
        case SIGHUP:
            strcpy(signame, "SIGHUP");
            break;
        case SIGINT:
            strcpy(signame, "SIGINT");
            break;
            
        default:
            break;
    }
    
    //syslog(LOG_INFO, "Received %s", signame);
    receivedSignal = signum;
}

static int exec_prog(const char **argv)
{
    pid_t   my_pid;
    int     status, timeout /* unused ifdef WAIT_FOR_COMPLETION */;
    
    if (0 == (my_pid = fork())) {
        if (-1 == execve(argv[0], (char **)argv , NULL)) {
            perror("child process execve failed [%m]");
            return -1;
        }
    }
    
#ifdef WAIT_FOR_COMPLETION
    timeout = 1000;
    
    while (0 == waitpid(my_pid , &status , WNOHANG)) {
        if ( --timeout < 0 ) {
            perror("timeout");
            return -1;
        }
        sleep(1);
    }
    
    //printf("%s WEXITSTATUS %d WIFEXITED %d [status %d]\n", argv[0], WEXITSTATUS(status), WIFEXITED(status), status);
    
    if (1 != WIFEXITED(status) || 0 != WEXITSTATUS(status)) {
        perror("%s failed, halt system");
        return -1;
    }
    
#endif
    return 0;
}


time_t mk_gps_time (char *datestr, char *timestr)
{
    struct tm gps_tm;
    time_t ret;
    
    // overwrite elemts with time form GPS
    gps_tm.tm_hour = (timestr[0]-'0')*10 + (timestr[1]-'0');
    gps_tm.tm_min = (timestr[2]-'0')*10 + (timestr[3]-'0');
    gps_tm.tm_sec = (timestr[4]-'0')*10 + (timestr[5]-'0');
    gps_tm.tm_year = (datestr[4]-'0')*1000 + (datestr[5]-'0')*100 + (datestr[6]-'0')*10 + (datestr[7]-'0');
    gps_tm.tm_year -= 1900;
    gps_tm.tm_mon = ((datestr[2]-'0')*10 + (datestr[3]-'0')) - 1;
    gps_tm.tm_mday = (datestr[0]-'0')*10 + (datestr[1]-'0');
    gps_tm.tm_isdst = -1;      // use DST setting from TZ information

    char *tz;
    
    // Change time zone to UTC
    tz = getenv("TZ");
    if (tz)
        tz = strdup(tz);
    setenv("TZ", "", 1);
    tzset();
    
    // Make time from GPS (UTC)
    ret = mktime(&gps_tm);
    
    // Chnage timezone back
    if (tz) {
        setenv("TZ", tz, 1);
        free(tz);
    } else
        unsetenv("TZ");
    tzset();
    
    return ret;
}

int main (int argc, char **argv)
{
    int uart0_fd = -1;
    char datetimestr[24];

    struct tm *gm_tm;
    time_t  gps_rawtime;

    const char * exec_argv[64] = {"/usr/bin/timedatectl" , "set-time" , NULL , NULL};;
    
    signal (SIGINT, sigHandler);
    signal (SIGHUP, sigHandler);
    signal (SIGTERM, sigHandler);
    
    uart0_fd = open(PORT, O_RDONLY | O_NOCTTY | O_NDELAY);		//Open in non blocking read/write mode
    if (uart0_fd == -1)
    {
        //ERROR - CAN'T OPEN SERIAL PORT
        fprintf (stderr, "Unable to open UART %s\n", PORT);
        //std::cerr << __PRETTY_FUNCTION__ << "Unable to open UART " << PORT << endl;
        exit(1);
    }
    
    struct termios options;
    tcgetattr(uart0_fd, &options);
    options.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;		//<Set baud rate
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;
    tcflush(uart0_fd, TCIFLUSH);
    tcsetattr(uart0_fd, TCSANOW, &options);

    int rx_length;
    unsigned char c;
    
    gps_setup();
    
    while ( (!gps_time_received) || (!gps_date_received) ) {
        // Read up to 255 characters from the port if they are there
        rx_length = read(uart0_fd, &c, 1);
        if (rx_length == 1)
        {
            gps_decode(c);
        }
        if (receivedSignal != 0) {
            break;
        }
    }
    
    //----- CLOSE THE UART -----
    close(uart0_fd);
    
    // get GPS time in epoch
    gps_rawtime = mk_gps_time(gps_date, gps_time);
    // populate time structure in local time format
    gm_tm = localtime(&gps_rawtime);
    //printf("localtime: %s", asctime(gm_tm) );
 
    // make date time string
    sprintf(datetimestr, "%04d-%02d-%02d %02d:%02d:%02d", gm_tm->tm_year+1900, gm_tm->tm_mon+1, gm_tm->tm_mday, gm_tm->tm_hour, gm_tm->tm_min, gm_tm->tm_sec );
    
    //printf("setting datetime%s\n",datetimestr);
    
    exec_argv[2] = datetimestr;
    
    int rc = exec_prog(exec_argv);
    
    //printf ("result: %d\n", rc);
    
    return rc;
}
