/*
 * New_Alarm_Cond.c
 *
 * Group members:
 * Jared Nuguid 216 316 143
 *
 *
 *
 *
 *
 *  [CHANGE IN FUTURE]
 * This is an enhancement to the alarm_mutex.c program, which
 * used only a mutex to synchronize access to the shared alarm
 * list. This version adds a condition variable. The alarm
 * thread waits on this condition variable, with a timeout that
 * corresponds to the earliest timer request. If the main thread
 * enters an earlier timeout, it signals the condition variable
 * so that the alarm thread will wake up and process the earlier
 * timeout first, requeueing the later request.
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

pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t alarm_cond = PTHREAD_COND_INITIALIZER;
sem_t alarm_list_sem;
sem_t change_alarm_sem;
//sem_init(&alarm_list_sem, 0, 1);

alarm_t *alarm_list = NULL;
alarm_t *change_alarm_request_list = NULL;
time_t current_alarm = 0;
alarm_t *new_alarm;
int avail_displays = 0; // the number of display alarm threads that can claim an alarm. Could make this a semaphore.
int real_avail_displays = 0; // for the newest created alarm, the number of display alarm threads that match its group id

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
    // Might not need this
    
    if (current_alarm == 0 || alarm->time < current_alarm) {
        current_alarm = alarm->time;
        status = pthread_cond_signal (&alarm_cond);
        if (status != 0)
            err_abort (status, "Signal cond");
    }
    
}

/*
 * The alarm thread's start routine.
 */
void *alarm_thread (void *arg)
{
    alarm_t *alarm;
    struct timespec cond_time;
    time_t now;
    int status, expired;

    /*
     * Loop forever, processing commands. The alarm thread will
     * be disintegrated when the process exits. Lock the mutex
     * at the start -- it will be unlocked during condition
     * waits, so the main thread can insert alarms.
     */
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
            status = pthread_cond_wait (&alarm_cond, &alarm_mutex);
            if (status != 0)
                err_abort (status, "Wait on cond");
            }
        alarm = alarm_list;
        alarm_list = alarm->link;
        now = time (NULL);
        expired = 0;
        if (alarm->time > now) {
#ifdef DEBUG
            printf ("[waiting: %d(%d)\"%s\"]\n", alarm->time,
                alarm->time - time (NULL), alarm->message);
#endif
            cond_time.tv_sec = alarm->time;
            cond_time.tv_nsec = 0;
            current_alarm = alarm->time;
            while (current_alarm == alarm->time) {
                status = pthread_cond_timedwait (
                    &alarm_cond, &alarm_mutex, &cond_time);
                if (status == ETIMEDOUT) {
                    expired = 1;
                    break;
                }
                if (status != 0)
                    err_abort (status, "Cond timedwait");
            }
            if (!expired)
                alarm_insert (alarm);
        } else
            expired = 1;
        if (expired) {
            printf ("(%d) %s\n", alarm->seconds, alarm->message);
            free (alarm);
        }
    }
}

void *display_thread (void *arg)
{
    int groupid;
    alarm_t *alarm1;
    alarm_t *alarm2;
    int alarm_num = 0; // the number of alarms currently assigned to this dispay alarm thread
    time_t now;
    int status;
    int expired;
    struct timespec cond_time;
    /*
        Between alarm1 and alarm2, the time of the alarm that 
        will expire first. 
        WIP -- When a new alarm is assigned to this display alarm thread
        need to add functionallity that updates curr_alarm.
        Need to use curr_alarm similar to how current_alarm is used
        in alarm_cond.c where the routine does a conditional timed wait
        using the current_alarm time.
    */
    time_t curr_alarm;

    now = time (NULL);
    expired = 0;
    alarm1 = new_alarm;  
    curr_alarm = alarm1->time;  
    alarm_num++;
    groupid = alarm1->groupid;
   
   while (alarm1->time > now) {
    cond_time.tv_sec = time (NULL) + 5;
    cond_time.tv_nsec = 0;
    status = pthread_cond_timedwait (
        &alarm_cond, &alarm_mutex, &cond_time);
    if (status == ETIMEDOUT){
        printf("Alarm(%d) Printed by Alarm Display Thread %d at %d: Group(%d) %d %s\n",
            alarm1->alarmid, pthread_self(), time (NULL), 
            alarm1->groupid, alarm1->time, alarm1->message); 
    }   
    now = time (NULL); 
   }
   
    printf("Display Thread %d Has Stopped Printing Message of Alarm(%d) at %d: Group(%d) %d %s\n", pthread_self(), alarm1->alarmid, time (NULL), alarm1->groupid, alarm1->time, alarm1->message);

    printf("No More Alarms in Group(%d): Display Thread %d exiting at %d\n", groupid, pthread_self(), time (NULL));
}

void *alarm_monitor_thread (void *arg) 
{
    alarm_t *alarm;
    struct timespec cond_time;
    time_t now;
    int status;
    printf("here\n");
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
            status = pthread_cond_wait (&alarm_cond, &alarm_mutex);
            if (status != 0)
                err_abort (status, "Wait on cond");
            }
        alarm = alarm_list;
        now = time (NULL);
        if (alarm->time > now) {
#ifdef DEBUG
            printf ("[waiting: %d(%d)\"%s\"]\n", alarm->time,
                alarm->time - time (NULL), alarm->message);
#endif
            cond_time.tv_sec = alarm->time;
            cond_time.tv_nsec = 0;
            current_alarm = alarm->time;
            status = pthread_cond_timedwait (
                &alarm_cond, &alarm_mutex, &cond_time);
            if (status == ETIMEDOUT) {
                status = pthread_cond_signal (&alarm_cond);
                if (status != 0)
                    err_abort (status, "Signal cond");
                printf("Alarm Monitor Thread %d Has Removed Alarm (%d) at %d: Group(%d) %d %s\n",
                pthread_self(), alarm->alarmid, time (NULL),
                alarm->groupid, alarm->time, alarm->message);
                alarm_list = alarm->link;
                free(alarm);
            }
        } else {
            printf("Alarm Monitor Thread %d Has Removed Alarm (%d) at %d: Group(%d) %d %s",
                pthread_self(), alarm->alarmid, time (NULL),
                alarm->groupid, alarm->time, alarm->message);
            alarm_list = alarm->link;
            free(alarm);
        }
    }
}

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
        int alarmid;
        int groupid;
        char message[64];
        char request[12];
        char group_keyword[5];
        int seconds;

        printf ("Alarm> ");
        if (fgets (line, sizeof (line), stdin) == NULL) exit (0);
        if (strlen (line) <= 1) continue;

        /*
         * Parse input line.
         */
        if (sscanf (line, "%s (%d): %s (%d) %d %64[^\n]",
            request, &alarmid, group_keyword, &groupid,
            &seconds, message) < 6) {
            fprintf (stderr, "Bad command\n");
            free (alarm);
        } else {
            status = pthread_mutex_lock (&alarm_mutex); // lock access to the alarm list
            if (status != 0)
                err_abort (status, "Lock mutex");            
            if (alarmid <= 0 || groupid <= 0 || seconds < 0 || strcmp(group_keyword, "Group") != 0) { // checks for invalid input
                fprintf (stderr, "Invalid input\n");
            } else {
                if (strcmp(request, "Start_Alarm") == 0) {
                    alarm = (alarm_t*)malloc (sizeof (alarm_t));
                    if (alarm == NULL)
                        errno_abort ("Allocate alarm");
                    pthread_t display_alarm_t;
                    alarm->seconds = seconds;
                    strcpy(alarm->message, message);
                    alarm->alarmid = alarmid;
                    alarm->groupid = groupid;
                    alarm->time = time (NULL) + alarm->seconds;
                    /*
                    * Insert the new alarm into the list of alarms,
                    * sorted by alarm id.
                    */
                    alarm_insert (alarm);
                    printf("Alarm(%d) Inserted by Main Thread %d Into Alarm List at %d: Group(%d) %d %s\n",
                     alarmid, main, time (NULL), groupid, alarm->time, message);
                    new_alarm = alarm;
                    if (avail_displays == 0) {
                        status = pthread_create (
                            &display_alarm_t, NULL, display_thread, NULL);
                        if (status != 0)
                            err_abort (status, "Create alarm thread");
                        printf("Main Thread Created New Display Alarm Thread %d For Alarm(%d) at %d: Group(%d)) %d %s\n",
                         display_alarm_t, alarmid, time (NULL), groupid, alarm->time, message);
                    } else {

                    }


                    status = pthread_mutex_unlock (&alarm_mutex);
                    if (status != 0)
                        err_abort (status, "Unlock mutex");
                } else if (strcmp(request, "Change_Alarm") == 0) {
                    //[WIP]
                } else {
                    fprintf(stderr, "Invalid Alarm Request\n");  
                }
            }
        }
    }
}
