#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <softTone.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <pthread.h>
#include <zconf.h>

#define RED 27
#define YELLOW 28
#define GREEN 3
#define READ_PIR 25
#define READ_USO 7
#define TRIGGER_USO 8
#define SNDOUT 24
#define WLOCK -2
#define WUNLOCK 2
#define KEY 1336
#define LOCK       -1
#define UNLOCK      1
#define PERM 0666      /* Zugriffsrechte */

static struct sembuf semaphore;
static int semid;

static int init_semaphore (short initval, int semkey) {
    /* Testen, ob das Semaphor bereits existiert */
    semid = semget (semkey, 0, IPC_PRIVATE);
    if (semid < 0) {
        /* ... existiert noch nicht, also anlegen        */
        /* Alle Zugriffsrechte der Dateikreierungsmaske */
        /* erlauben                                     */
        umask(0);
        semid = semget (semkey, 1, IPC_CREAT | IPC_EXCL | PERM);
        if (semid < 0) {
            printf ("Fehler beim Anlegen des Semaphors ...\n");
            return -1;
        }
        printf ("(angelegt) Semaphor-ID : %d\n", semid);
        /* Semaphor mit 1 initialisieren */
        if (semctl (semid, 0, SETVAL, initval) == -1)
            return -1;
    }
    return 1;
}
static int semaphore_operation (short op) {
    semaphore.sem_op = op;
    //semaphore.sem_flg = SEM_UNDO;
    if( semop (semid, &semaphore, 1) == -1) {
        perror(" semop ");
        exit (EXIT_FAILURE);
    }
    return 1;
}


struct argsForpthread
{
    double distance;
    short evaluationDone;
    short detectedMove;
    short alive;
};

void p3_thread1(struct argsForpthread *demArgs)
{
    while(demArgs->alive)
    {
        semaphore_operation(WLOCK);
        printf("THREAD1 LOCK\n");
        if (digitalRead(READ_PIR) == 1)
        {
        demArgs->detectedMove=1;
        printf("MOVEMENT DETECTED\n");
        semaphore_operation(WUNLOCK);
        }
        else
        {
            demArgs->detectedMove=0;
        }
    }
}

void p3_thread2(struct argsForpthread * demArgs)
{
    struct timeval start, ende;
    long sec, usec;
    while(demArgs->alive)
    {
        semaphore_operation(WLOCK);
        printf("THREAD2 LOCK\n");
        digitalWrite(TRIGGER_USO,1);
        delay(10);
        digitalWrite(TRIGGER_USO,0);
        if (gettimeofday(&start,(struct timezone*)0))
        {
            printf("error\n");
            exit(1);
        }
        while (digitalRead(READ_USO)==0)
        {
            if (gettimeofday(&start,(struct timezone*)0))
            {
                printf("error\n");
                exit(1);
            }
        }
        while (digitalRead(READ_USO)==1)
        {
            if (gettimeofday(&ende,(struct timezone*)0))
            {
                printf("error\n");
                exit(1);
            }
        }
        sec = ende.tv_sec - start.tv_sec;
        usec = ende.tv_usec - start.tv_usec;
        double totaldiff = (double)sec + (double)usec/1000;
        demArgs->distance = ((double)sec + (double)usec/1000)*34300/2;
        printf("DISTANCE MEASURED!\n");
        semaphore_operation(WUNLOCK);
    }
}

void p3_thread3(struct argsForpthread *demArgs)
{
    while(demArgs->alive) {
        semaphore_operation(LOCK);
        printf("THREAD3 LOCK\n");
        digitalWrite(GREEN, 0);
        digitalWrite(YELLOW, 0);
        digitalWrite(RED, 0);
        if(demArgs->distance<10)
        {
            digitalWrite(RED,1);
        }
        else if (demArgs->distance >= 10 && demArgs->distance <= 20)
        {
            digitalWrite(YELLOW,1);
        }
        else if (demArgs->distance > 20)
        {
            digitalWrite(GREEN,1);
        }
        printf("SET LEDS\nSEMID %d\n",semid);
        semaphore_operation(UNLOCK);
    }
}

void p3_thread4(struct argsForpthread *demArgs)
{
    while(demArgs->alive)
    {
        semaphore_operation(LOCK);
        printf("THREAD4 LOCK\n");
        if(demArgs->distance < 10.0)
        {
            softToneWrite(SNDOUT,100);
            delay(300);
            printf("Distance: %lf\nSEMID %d\n",demArgs->distance,semid);
        }
        semaphore_operation(UNLOCK);
    }
}

pthread_t readPIR, calcDist, doLED, doSound;

int main() {
    struct argsForpthread demArgs;
    demArgs.distance=0.0;
    demArgs.detectedMove=0;
    demArgs.evaluationDone=0;
    demArgs.alive=1;
    int res;
    res = init_semaphore (2,KEY);
    if (res < 0) {
        printf("ERROR CREATING SEMAPHORE");
        return EXIT_FAILURE;
    }
    wiringPiSetup();
    pinMode(READ_PIR,INPUT);
    pinMode(READ_USO,INPUT);
    pinMode(RED,OUTPUT);
    pinMode(YELLOW,OUTPUT);
    pinMode(GREEN,OUTPUT);
    pinMode(TRIGGER_USO,OUTPUT);
    pinMode(SNDOUT,SOFT_TONE_OUTPUT);
    if (pthread_create(&readPIR,NULL,&p3_thread1,&demArgs)!=0)
    {
        printf("ERROR CREATING THREAD");
        return EXIT_FAILURE;
    }
    if (pthread_create(&calcDist,NULL,&p3_thread2,&demArgs)!=0)
    {
        printf("ERROR CREATING THREAD");
        return EXIT_FAILURE;
    }
    if (pthread_create(&doLED,NULL,&p3_thread3,&demArgs)!=0)
    {
        printf("ERROR CREATING THREAD");
        return EXIT_FAILURE;
    }
    if (pthread_create(&doSound,NULL,&p3_thread4,&demArgs)!=0)
    {
        printf("ERROR CREATING THREAD");
        return EXIT_FAILURE;
    }

    sleep(30);
    demArgs.alive=0;
    pthread_join(readPIR,NULL);
    pthread_join(calcDist,NULL);
    pthread_join(doSound,NULL);
    pthread_join(doLED,NULL);
    return 0;
}