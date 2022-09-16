/*
    Autore: Fabrizio Cau
    Matricola: 508700
    Corso A Informatica

    Sorgente libreria mylib
*/

#include <mylib.h>
#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>


void SetDefConf() {
    strncpy(LOGFILE, def_LOGFILE, sizeof(def_LOGFILE)+1);
    FORK_FROM_MNG = def_FORK_FROM_MNG;
    K = def_K;
    T = def_T;
    P = def_P;
    C = def_C;
    E = def_E;
    TIME_PER_PROD = def_TIME_PER_PROD;
    MAX_BASE_ST = def_MAX_BASE_ST;
    INCODA_NOTIFY_TIMEOUT = def_INCODA_NOTIFY_TIMEOUT;
    CUST_MOVES_TIMEOUT = def_CUST_MOVES_TIMEOUT;
    INITIAL_CHECKOUTS = def_INITIAL_CHECKOUTS;
    MAX_EMPTY_CKOUTS = def_MAX_EMPTY_CKOUTS;
    MAX_CUST_PER_CKOUT = def_MAX_CUST_PER_CKOUT;
}

void setConfValue(char *name, char *val) {
    int t;
    if( strcmp(name, "LOGFILE") == 0 ) 
        strcpy(LOGFILE, val);
    else if( strcmp(name, "FORK_FROM_MNG") == 0 ) {
        t = (int)strtol(val, (char**)NULL, 10);
        if(t==1 || t==0)
            FORK_FROM_MNG = t;
    } else if( strcmp(name, "K") == 0 ) {
        t = (int)strtol(val, (char**)NULL, 10);
        if(t>0)
            K = t;
    } else if( strcmp(name, "T") == 0 ) {
        t = (int)strtol(val, (char**)NULL, 10);
        if(t>10)
            T = t;
    } else if( strcmp(name, "P") == 0 ) {
        t = (int)strtol(val, (char**)NULL, 10);
        if(t>0)
            P = t;
    } else if( strcmp(name, "C") == 0 ) {
        t = (int)strtol(val, (char**)NULL, 10);
        if(t>0)
            C = t;
    } else if( strcmp(name, "E") == 0 ) {
        t = (int)strtol(val, (char**)NULL, 10);
        if(t>0 && t<C)
            E = t;
    } else if( strcmp(name, "TIME_PER_PROD") == 0 ) {
        t = (int)strtol(val, (char**)NULL, 10);
        if(t>0)
            TIME_PER_PROD = t;
    } else if( strcmp(name, "MAX_BASE_ST") == 0 ) {
        t = (int)strtol(val, (char**)NULL, 10);
        if(t>20)    
            MAX_BASE_ST = t;
    } else if( strcmp(name, "INCODA_NOTIFY_TIMEOUT") == 0 ) {
        t = (int)strtol(val, (char**)NULL, 10);
        if(t>0)
            INCODA_NOTIFY_TIMEOUT = t;
    } else if( strcmp(name, "CUST_MOVES_TIMEOUT") == 0 ) {
        t = (int)strtol(val, (char**)NULL, 10);
        if(t>0)    
            CUST_MOVES_TIMEOUT = t;
    } else if( strcmp(name, "INITIAL_CHECKOUTS") == 0 ) {
        t = (int)strtol(val, (char**)NULL, 10);
        if(t>0)
            INITIAL_CHECKOUTS = t;
    } else if( strcmp(name, "MAX_EMPTY_CKOUTS") == 0 ) {
        t = (int)strtol(val, (char**)NULL, 10);
        if(t>0)    
            MAX_EMPTY_CKOUTS = t;
    } else if( strcmp(name, "MAX_CUST_PER_CKOUT") == 0 ) {
        t = (int)strtol(val, (char**)NULL, 10);
        if(t>0)
            MAX_CUST_PER_CKOUT = t;
    }
    if(errno==ERANGE) { fprintf(stderr, "strtol error \n"); exit(errno); }
}

void ParseConfig() {
    FILE *conf;
    char name[128];
    char val[128];
    FE( (conf = fopen(CONFIGPATH, "r")) )
    while( fscanf(conf, "%127[^=]=%127[^\n]%*c", name, val) != EOF)
        setConfValue(name, val);        
    fclose(conf);
}

void OverwriteConfig(int n, char *el[]) {
    for(int i=1;i<n-1;i+=2)
        setConfValue(el[i],el[i+1]);
}

inline void sleepms(unsigned int ms) {
    if( ms<0 || ms>MAXSLEEP ) ARG_ERR("sleepms(out of range)")
    if(ms == 0) return;
    ts_t ts = { .tv_sec = (time_t)(ms/1000), .tv_nsec = (long)((ms%1000)*1000000) };
    if(nanosleep(&ts, NULL) == -1) fprintf(stderr, "nanosleep error: %d", errno);
}
 
int Exists(int *list, int n) {
    if(list==NULL) ARG_ERR("Exists(NULL)")
    short found = 0;
    for(int i=1;i<=list[0];i++)
        if(n==list[i]) found=1;
    if(!found) return 0;
    return 1;
}

int getRandom(int *list) {
    if(list==NULL) ARG_ERR("Exists(NULL)")
    unsigned int seed = time(NULL)+pthread_self();
    return list[1+(rand_r(&seed)%(list[0]))];
}

unsigned long GetTimeDiff(ts_t start, ts_t stop) {
    if(start.tv_sec<0 || start.tv_nsec<0 || stop.tv_sec<0 || stop.tv_nsec<0) ARG_ERR("SetCkTime(negative)")
    return ((stop.tv_sec - start.tv_sec)*1000)+((stop.tv_nsec - start.tv_nsec)/1000000);
}

void CleanSFD() {
    unlink(SOCKETPATH);
}

