1. First copy the files "New_Alarm_Cond.c", "makefile.txt" (remove .txt) , and "errors.h" into your
   own directory.

   There are two ways to compile "New_Alarm_Cond.c"
   
   1. To compile the program "New_Alarm_Cond.c", use the following command:

      cc alarm_cond.c -D_POSIX_PTHREAD_SEMANTICS -lpthread
      
      Type "a.out" to run the executable code.
      
      OR
      
   2. Use the following command:
   
      make
      
      In the same directory open the Unix exucutable file "New_Alarm_Cond" to run the code.

2. Sample output:

   Alarm> Start_Alarm (3): Group (4) 10 test
   Alarm(3) Inserted by Main Thread 4200916 Into Alarm List at 1607547390: Group(4) 1607547400 test
   Main Thread Created New Display Alarm Thread 526640896 For Alarm(3) at 1607547390: Group(4)) 1607547400 test
  
   Alarm> Alarm(3) Printed by Alarm Display Thread 526640896 at 1607547394: Group(4) 1607547400 test
   Alarm(3) Printed by Alarm Display Thread 526640896 at 1607547399: Group(4) 1607547400 test
   Alarm Monitor Thread 535033600 Has Removed Alarm (3) at 1607547399: Group(4) 1607547400 test
   Display Thread 526640896 Has Stopped Printing Message of Alarm(3) at 1607547400: Group(4) 1607547400 test
   No More Alarms in Group(4): Display Thread 526640896 exiting at 1607547400

   Alarm> Start_Alarm (3): Group (4) 10 test
   Alarm(3) Inserted by Main Thread 4200320 Into Alarm List at 1607571355: Group(4) 1607571365 test
   Main Thread Created New Display Alarm Thread -963836160 For Alarm(3) at 1607571355: Group(4)) 1607571365 test
   Alarm> Change_Alarm (3): Group (4) 40 new input
   Change Alarm Request (1) Inserted by Main Thread4200320 into Alarm List at 1607570970: Group(1) 1607571010 new input
