/*
 * New_Alarm_Cond.c
 *
 * Group members:
 * Jared Nuguid 216 316 143
 * Graeme Miller, 216 415 366
 * Vivek Pereira, 216 421 588
 * Joshua Dors, 216 583 916
 * Sukhraj Boall, 216 320 806
 *
 */
#include <pthread.h>
#include <time.h>
#include "errors.h"
#include <semaphore.h>

typedef struct alarm_tag {
    struct alarm_tag    *link;
    int                 seconds;
    time_t              time;   /* seconds from EPOCH */
    char                message[64];
    int                 alarmid;
    int                 groupid;
} alarm_t;

pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for the alarm list
pthread_mutex_t request_mutex = PTHREAD_MUTEX_INITIALIZER; // This should be a semaphore but semaphore initialization is not working.
pthread_cond_t alarm_cond = PTHREAD_COND_INITIALIZER;
int avail_displays = 0; // Number of available display alarm threads. Could not get it to work as a semaphore because of compile errors so made it an int.
//sem_t avail_displays;
int real_avail_displays = 0; // for the newest created alarm, the number of display alarm threads that match its group id
//sem_init(&avail_displays, 0, 0); Compile error.

alarm_t *alarm_list = NULL;
alarm_t *change_alarm_request_list = NULL;
time_t current_alarm = 0;
alarm_t *new_alarm;


/*
 * Insert alarm entry on list, in order of alarmid.
 */
void alarm_insert (alarm_t *alarm)
{
    int status;
    alarm_t **last, *next;

    /*
     * LOCKING PROTOCOL:
     *
     * This routine requires that the caller have locked the
     * alarm_mutex!
     */
    last = &alarm_list;
    next = *last;
    while (next != NULL) {
        if (next->alarmid >= alarm->alarmid) {
            alarm->link = next;
            *last = alarm;
            break;
        }
        last = &next->link;
        next = next->link;
    }
    /*
     * If we reached the end of the list, insert the new alarm
     * there.  ("next" is NULL, and "last" points to the link
     * field of the last item, or to the list header.)
     */
    if (next == NULL) {
        *last = alarm;
        alarm->link = NULL;
    }
#ifdef DEBUG
    printf ("[list: ");
    for (next = alarm_list; next != NULL; next = next->link)
        printf ("%d(%d)[\"%s\"] ", next->time,
            next->time - time (NULL), next->message);
    printf ("]\n");
#endif
    /*
     * Wake the alarm thread if it is not busy (that is, if
     * current_alarm is 0, signifying that it's waiting for
     * work), or if the new alarm comes before the one on
     * which the alarm thread is waiting.
     */
    
    if (current_alarm == 0) {
        current_alarm = alarm->time;
        status = pthread_cond_signal (&alarm_cond);
        if (status != 0)
            err_abort (status, "Signal cond");
    }
    
}

// The display alarm thread start routine.
void *display_thread (void *arg)
{
    int groupid;
    alarm_t *alarm1;
    alarm_t *alarm2; // Was not able to implement a display_thread that could handle two alarms.
    time_t now;
    int status;
    struct timespec cond_time;

    now = time (NULL);
    alarm1 = new_alarm;   
    groupid = alarm1->groupid; // Give this thread the group id of the assigned alarm.
   
   while (alarm1->time > now) { // Loop until the current time is greater than or equal to the expiration time of the alarm.
    cond_time.tv_sec = time (NULL) + 5; // The 5 second wait time before printing the alarm's message.
    cond_time.tv_nsec = 0; // Nanoseconds are not used so it is initialized to 0.
    /* The thread waits on the condition variable. Since it waits on a condition variable, the monitor thread can signal it
       if the alarm already expired while it was waiting.
    */
    status = pthread_cond_timedwait ( 
        &alarm_cond, &alarm_mutex, &cond_time);
    if (status == ETIMEDOUT){
        printf("Alarm(%d) Printed by Alarm Display Thread %d at %d: Group(%d) %d %s\n", alarm1->alarmid, pthread_self(), time (NULL),alarm1->groupid, alarm1->time, alarm1->message); 
    }   
    now = time (NULL); 
   }
   
    printf("Display Thread %d Has Stopped Printing Message of Alarm(%d) at %d: Group(%d) %d %s\n", pthread_self(), alarm1->alarmid, time (NULL), alarm1->groupid, alarm1->time, alarm1->message);
    printf("No More Alarms in Group(%d): Display Thread %d exiting at %d\n", groupid, pthread_self(), time (NULL));
}

// The monitor thread start routine.
void *alarm_monitor_thread (void *arg) 
{
    alarm_t *alarm;
    struct timespec cond_time;
    time_t now;
    int status;
    status = pthread_mutex_lock (&alarm_mutex);
    if (status != 0)
        err_abort (status, "Lock mutex");
    while (1) {
         /*
         * If the alarm list is empty, wait until an alarm is
         * added. Setting current_alarm to 0 informs the insert
         * routine that the thread is not busy.
         */
        current_alarm = 0;
        while (alarm_list == NULL) {
            status = pthread_cond_wait (&alarm_cond, &alarm_mutex); // Use condition variable to wait until alarm list is not empty.
            if (status != 0)
                err_abort (status, "Wait on cond");
            }
        alarm = alarm_list; // Monitor will 'monitor' the first alarm in the alarm list.
        now = time (NULL);
        if (alarm->time > now) { // If the alarm that is being monitored has not yet expired.
#ifdef DEBUG
            printf ("[waiting: %d(%d)\"%s\"]\n", alarm->time,
                alarm->time - time (NULL), alarm->message);
#endif
            cond_time.tv_sec = alarm->time; // Set the wait time to the expiration time of the monitored alarm.
            cond_time.tv_nsec = 0;
            current_alarm = alarm->time;
            status = pthread_cond_timedwait (
                &alarm_cond, &alarm_mutex, &cond_time); // Do a timed wait using the condition variable.
            if (status == ETIMEDOUT) {
                status = pthread_cond_signal (&alarm_cond);
                if (status != 0)
                    err_abort (status, "Signal cond");
                printf("Alarm Monitor Thread %d Has Removed Alarm (%d) at %d: Group(%d) %d %s\n",
                pthread_self(), alarm->alarmid, time (NULL),
                alarm->groupid, alarm->time, alarm->message);
                alarm_list = alarm->link; // Remove the monitored alarm from the alarm list.
                free(alarm); // Deallocate the alarm.
            }
        } else { // If the alarm that is being monitored has expired.
            printf("Alarm Monitor Thread %d Has Removed Alarm (%d) at %d: Group(%d) %d %s",
                pthread_self(), alarm->alarmid, time (NULL),
                alarm->groupid, alarm->time, alarm->message);
            alarm_list = alarm->link; // Remove the monitored alarm from the alarm list.
            free(alarm); // Deallocate the alarm.
        }
    }
}

//The Main thread
int main (int argc, char *argv[])
{
    int status;
    char line[128];
    pthread_t monitor_thread;

    
    status = pthread_create (
        &monitor_thread, NULL, alarm_monitor_thread, NULL);
    if (status != 0)
        err_abort (status, "Create alarm thread");
    
    while (1) {
        alarm_t *alarm;
        int alarmid; // Alarm id of the alarm
        int groupid; // Group id of the alarm
        char message[64]; // Message that the alarm will print.
        char request[12]; // The alarm request a user makes. Can be either Start_Alarm or Change_Alarm.
        char group_keyword[5]; // Part of an alarm request a user makes. Must be 'Group' or else returns an error.
        int seconds; // The number of seconds before an alarm expires.

        printf ("Alarm> ");
        if (fgets (line, sizeof (line), stdin) == NULL) exit (0);
        if (strlen (line) <= 1) continue;

        /*
         * Parse input line.
         * The input is tokenized and inserted into fields that will be checked for errors.
         */
        if (sscanf (line, "%s (%d): %s (%d) %d %64[^\n]",
            request, &alarmid, group_keyword, &groupid,
            &seconds, message) < 6) {
            fprintf (stderr, "Bad command\n");
            free (alarm);
        } else {        
            if (alarmid <= 0 || groupid <= 0 || seconds < 0 || strcmp(group_keyword, "Group") != 0) { // checks for invalid input
                fprintf (stderr, "Invalid input\n");
            } else {
                if (strcmp(request, "Start_Alarm") == 0) { // If the request is a Start_Alarm request
                    alarm = (alarm_t*)malloc (sizeof (alarm_t)); // Dynamically allocating space for the alarm.
                    if (alarm == NULL)
                        errno_abort ("Allocate alarm");
                    pthread_t display_alarm_t; // A display alarm thread instance in case a display alarm thread needs to be created.

                    //Initializing the fields of the alarm struct.
                    alarm->seconds = seconds;
                    strcpy(alarm->message, message);
                    alarm->alarmid = alarmid;
                    alarm->groupid = groupid;
                    alarm->time = time (NULL) + alarm->seconds;
                    /*
                    * Insert the new alarm into the list of alarms,
                    * sorted by alarm id.
                    */
                   status = pthread_mutex_lock (&alarm_mutex); // lock access to the alarm list
                    if (status != 0)
                        err_abort (status, "Lock mutex");    
                    alarm_insert (alarm);
                    printf("Alarm(%d) Inserted by Main Thread %d Into Alarm List at %d: Group(%d) %d %s\n",
                     alarmid, main, time (NULL), groupid, alarm->time, message);
                    new_alarm = alarm; // Field used to assign the new display alarm thread the newly created alarm.
                    if (avail_displays == 0) { // If there are no available display alarm threads, create a new one.
                        status = pthread_create (
                            &display_alarm_t, NULL, display_thread, NULL); // Begins its start routine in display_thread.
                        if (status != 0)
                            err_abort (status, "Create alarm thread");
                        printf("Main Thread Created New Display Alarm Thread %d For Alarm(%d) at %d: Group(%d)) %d %s\n", display_alarm_t, alarmid, time (NULL), groupid, alarm->time, message);
                    } else { // If there is an existing display alarm thread available. Not implemented.

                    }


                    status = pthread_mutex_unlock (&alarm_mutex);
                    if (status != 0)
                        err_abort (status, "Unlock mutex");
                } else if (strcmp(request, "Change_Alarm") == 0) {
                    alarm = (alarm_t*)malloc (sizeof (alarm_t)); // Dynamically allocating space for the alarm. It will act as the alarm request.
                    if (alarm == NULL)
                        errno_abort ("Allocate alarm");

                    //Initializing the fields of the request.
                    alarm->seconds = seconds;
                    strcpy(alarm->message, message);
                    alarm->alarmid = alarmid;
                    alarm->groupid = groupid;
                    alarm->time = time (NULL) + alarm->seconds;

                    status = pthread_mutex_lock (&request_mutex); // lock access to the request list
                    if (status != 0)
                        err_abort (status, "Lock mutex");    
                    
                    // Insert the request into the request list ordered by alarm id.
                    alarm_t **last, *next;
                    last = &change_alarm_request_list;
                    next = *last;
                    while (next != NULL) {
                        if (next->alarmid >= alarm->alarmid) {
                            alarm->link = next;
                            *last = alarm;
                            break;
                        }
                        last = &next->link;
                        next = next->link;
                    }
                    /*
                    * If we reached the end of the list, insert the new alarm
                    * there.  ("next" is NULL, and "last" points to the link
                    * field of the last item, or to the list header.)
                    */
                    if (next == NULL) {
                        *last = alarm;
                        alarm->link = NULL;
                    }
                    printf("Change Alarm Request (%d) Inserted by Main Thread %d into Alarm List at %d: Group(%d) %d %s\n", alarmid, main, time (NULL), groupid, alarm->time, message);
                    status = pthread_mutex_unlock (&request_mutex); // Done writing to the request list so unlock mutex.
#ifdef DEBUG
                    printf ("[list: ");
                    for (next = change_alarm_request_list; next != NULL; next = next->link)
                        printf ("%d(%d)[\"%s\"] ", next->time,
                            next->time - time (NULL), next->message);
                    printf ("]\n");
#endif                   
                    if (status != 0)
                        err_abort (status, "Unlock mutex");
                } else {
                    fprintf(stderr, "Invalid Alarm Request\n");  
                }
            }
        }
    }
}
