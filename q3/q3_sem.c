# include <stdio.h>
# include <stdlib.h>
# include <unistd.h>
# include <pthread.h>
# include <semaphore.h>
# include <time.h>
# include <errno.h>
# define RED "\033[0;31m"
# define BLUE "\033[0;34m"
# define GREEN "\033[0;32m"
# define YELLOW "\033[0;33m"
# define CYAN "\033[0;36m"
# define MAGENTA "\e[0;35m"
# define RESET "\033[m"


// ------------------- MUTEXES, SEMAPHORES AND CONDITION VARIABLES -------------------
sem_t stage;
sem_t acoustic_stage;
sem_t electric_stage;
sem_t singer_not_performing;
sem_t musician_performing;
sem_t singer_joined;
sem_t coordinator_available;

pthread_mutex_t wait_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t singer_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t singer_left = PTHREAD_COND_INITIALIZER; // singer left a musician after completing performance


// ------------------- GLOBAL STRUCTURES -------------------
typedef struct performerInfo {
    int performer_num;
    int status; // -1 if not performing, <stage number> if musician/singer is performing solo, 0 for singer who has joined a musician
    char name[100];
    char instrument;
    char stage_type; // a -> acoustic, e -> electric, b -> both
    int arrival_time;
} performerInfo;

typedef struct timeInfo {
    int t1; // minimum performance time
    int t2; // maximum performance time
    int t; // maximum performer waiting time
} timeInfo;
timeInfo* ti;

typedef struct list
{
    int arr[10000];
    int add_ptr;
    int remove_ptr;
    int MAX_SIZE; // maximum list size
} list;


// ------------------- PERFORMER RELATED GLOBAL VARIABLES -------------------
performerInfo* all_performers[10000];


// ------------------- SINGER RELATED GLOBAL VARIABLES -------------------
list* singer_list; // queue of singers (numbers) ready to join a musician


// ------------------- STAGE RELATED GLOBAL VARIABLES -------------------
list* acoustic_stage_list; // list of available acoustic stages
list* electric_stage_list; // list of available electric stages


// ------------------- ERROR HANDLING WRAPPER FUNCTIONS -------------------
void pthreadCreate(pthread_t *tid, const pthread_attr_t *attr, void *function_ptr, void *arg)
{
    if(pthread_create(tid, attr, function_ptr, arg) != 0)
        perror("ERROR");
}

void pthreadJoin(pthread_t tid, void **retval)
{
    if(pthread_join(tid, retval) != 0)
        perror("ERROR");
}

void pthreadMutexLock(pthread_mutex_t *mutex)
{
    if(pthread_mutex_lock(mutex) != 0)
        perror("ERROR");
}

void pthreadMutexUnlock(pthread_mutex_t *mutex)
{
    if(pthread_mutex_unlock(mutex) != 0)
        perror("ERROR");
}

void pthreadCondWait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex)
{
    if(pthread_cond_wait(cond, mutex) != 0)
        perror("ERROR");
}

void pthreadCondBroadcast(pthread_cond_t *cond)
{
    if(pthread_cond_broadcast(cond) != 0)
        perror("ERROR");
}

void semInit(sem_t *sem, int proc_shared, unsigned int value)
{
    if(sem_init(sem, proc_shared, value) != 0)
        perror("ERROR");
}

void semWait(sem_t *sem, pthread_mutex_t *mutex)
{
    if(mutex != NULL)
        pthreadMutexUnlock(mutex);
    if(sem_wait(sem) != 0)
        perror("ERROR");
    if(mutex != NULL)
        pthreadMutexLock(mutex);
}

int semTryWait(sem_t *sem, pthread_mutex_t *mutex)
{
    if(mutex != NULL)
        pthreadMutexUnlock(mutex);
    int ret = sem_trywait(sem);
    if(ret != 0 && errno != EAGAIN)
        perror("ERROR");
    ret = (ret == 0) ? 0 : errno;
    if(mutex != NULL)
        pthreadMutexLock(mutex);
    return ret;
}

int semTimedWait(sem_t *sem, const struct timespec *ts, pthread_mutex_t *mutex)
{
    if(mutex != NULL)
        pthreadMutexUnlock(mutex);
    int ret = sem_timedwait(sem, ts);
    if(ret != 0 && errno != ETIMEDOUT)
        perror("ERROR");
    ret = (ret == 0) ? 0 : errno;
    if(mutex != NULL)
        pthreadMutexLock(mutex);
    return ret;
}

void semPost(sem_t *sem)
{
    if(sem_post(sem) != 0)
        perror("ERROR");
}


// ------------------- GLOBAL DATA INITIALIZATION -------------------
void initializeGlobalData(int a, int e, int c)
{
    semInit(&stage, 0, a + e);
    semInit(&acoustic_stage, 0, a);
    semInit(&electric_stage, 0, e);
    semInit(&singer_joined, 0, 0);
    semInit(&singer_not_performing, 0, a + e);
    semInit(&musician_performing, 0, 0);
    semInit(&coordinator_available, 0, c);
}

void initializeList(list **l, int max_size)
{
    *l = (list*)malloc(sizeof(list));
    (*l)->add_ptr = 0;
    (*l)->remove_ptr = 0;
    (*l)->MAX_SIZE = max_size;
}


// ------------------- FUNCTIONS UPDATING GLOBAL DATA -------------------
int removeFromList(list* l)
{
    int ele = l->arr[l->remove_ptr];
    l->remove_ptr = (l->remove_ptr+1) % l->MAX_SIZE;
    return ele;
}

void addToList(list* l, int ele)
{
    l->arr[l->add_ptr] = ele;
    l->add_ptr = (l->add_ptr+1) % l->MAX_SIZE;
}


// ------------------- HELPER FUNCTIONS -------------------
char chooseStage()
{
    int res;
    char stage_type;
    double choice = rand() / (double)RAND_MAX; // can wait for acoustic or electric stage with equal probability
    if(choice > 0.5)
    {
        res = semTryWait(&acoustic_stage, &wait_mutex); // wait for acoustic before electric
        if(res == EAGAIN)
        {
            semWait(&electric_stage, &wait_mutex); // will not block as electric stage is definitely available
            stage_type = 'e';
        }
        else
            stage_type = 'a';
    }
    else
    {
        res = semTryWait(&electric_stage, &wait_mutex); // wait for electric before acoustic
        if(res == EAGAIN)
        {
            semWait(&acoustic_stage, &wait_mutex); // will not block as acoustic stage is definitely available
            stage_type = 'a';
        }
        else
            stage_type = 'e';
    }
    return stage_type;
}

void collectTshirt(performerInfo *pi)
{
    semWait(&coordinator_available, NULL); // wait for coordinator
    printf(CYAN "%s (%s) collecting t-shirt\n" RESET, pi->name, (pi->instrument == 's') ? "singer" : "musician");
    sleep(2); // collect t-shirt
    printf(RED "%s (%s) collected t-shirt and left\n" RESET, pi->name, (pi->instrument == 's') ? "singer" : "musician");
    semPost(&coordinator_available); // signal that coordinator is available
}

void endPerformance(char stage_type, performerInfo* pi)
{
    if(stage_type == 'a')
    {
        addToList(acoustic_stage_list, all_performers[(pi->performer_num)-1]->status);
        semPost(&acoustic_stage); // release acoustic stage
    }
    else
    {
        addToList(electric_stage_list, all_performers[(pi->performer_num)-1]->status);
        semPost(&electric_stage); // release electric stage
    }
    semPost(&stage); // signal that stage is now available
    if(pi->instrument == 's')
        semPost(&singer_not_performing); // signal that singer is done performing
    all_performers[(pi->performer_num)-1]->status = -1; // update status of performer
}


// ------------------- SINGER THREAD HANDLER -------------------
void joinSingerWithMusician(performerInfo *pi)
{
    all_performers[(pi->performer_num)-1]->status = 0; // update status of singer
    pthreadMutexUnlock(&wait_mutex);

    pthreadMutexLock(&singer_mutex);
    addToList(singer_list, pi->performer_num);
    pthreadMutexUnlock(&singer_mutex);
    semPost(&singer_joined); // singer joins a musician

    // wait for singer to finish joint performance with musician
    pthreadMutexLock(&wait_mutex);
    while(all_performers[(pi->performer_num)-1]->status != -1)
        pthreadCondWait(&singer_left, &wait_mutex);
    pthreadMutexUnlock(&wait_mutex);
}


void* singerHandler(void* input)
{
    performerInfo* pi = (performerInfo*)input;
    sleep(pi->arrival_time); // wait until arrival
    printf(GREEN "%s (singer) has arrived\n" RESET, pi->name);

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += ti->t;
    int res;
    char stage_type;

    pthreadMutexLock(&wait_mutex); // make atomic
    res = semTimedWait(&singer_not_performing, &ts, &wait_mutex); // wait until there is no singer performing
    if(res == ETIMEDOUT)
    {
        printf(RED "%s (singer) left due to impatience\n" RESET, pi->name);
        pthreadMutexUnlock(&wait_mutex);
        return NULL; // singer becomes impatient and leaves
    }

    // can choose stage or musician with equal probability
    double choice = rand() / (double)RAND_MAX;
    if(choice > 0.5)
    {
        res = semTryWait(&stage, &wait_mutex); // perform solo if stage is available else join a musician
        if(res == EAGAIN)
        {
            semWait(&musician_performing, &wait_mutex); // will not block as musician is definitely performing
            joinSingerWithMusician(pi);
            collectTshirt(pi);
            return NULL;
        }
        else
            stage_type = chooseStage();
    }
    else
    {
        res = semTryWait(&musician_performing, &wait_mutex); // join a musician if he is performing else perform solo
        if(res == EAGAIN)
        {
            semWait(&stage, &wait_mutex);  // will not block as a stage is definitely free
            stage_type = chooseStage();
        }
        else
        {
            joinSingerWithMusician(pi);
            collectTshirt(pi);
            return NULL;
        }
    }
    if(stage_type == 'a')
        all_performers[(pi->performer_num)-1]->status = removeFromList(acoustic_stage_list); // update status
    else
        all_performers[(pi->performer_num)-1]->status = removeFromList(electric_stage_list); // update status
    pthreadMutexUnlock(&wait_mutex);

    // singer solo performance starts
    int stage_num = all_performers[(pi->performer_num)-1]->status;
    int performance_duration = rand() % (ti->t2 - ti->t1 + 1) + ti->t1;
    printf(BLUE "%s (singer) performing solo on %s stage (stage number %d) for %d seconds\n" RESET,
           pi->name, (stage_type == 'a') ? "acoustic" : "electric", stage_num, performance_duration);

    sleep(performance_duration);

    // performance ends
    pthreadMutexLock(&wait_mutex);
    endPerformance(stage_type, pi);
    printf(MAGENTA "%s (singer) has finished performing on %s stage (stage number %d)\n" RESET,
           pi->name, (stage_type == 'a') ? "acoustic" : "electric", stage_num);
    pthreadMutexUnlock(&wait_mutex);

    // collect t-shirt
    collectTshirt(pi);

    return NULL;
}


// ------------------- MUSICIAN THREAD HANDLER -------------------
void* musicianHandler(void* input)
{
    performerInfo* pi = (performerInfo*)input;
    sleep(pi->arrival_time); // wait until arrival
    printf(GREEN "%s (who plays instrument %c) has arrived\n" RESET, pi->name, pi->instrument);

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += ti->t;
    int res;
    char stage_type;

    pthreadMutexLock(&wait_mutex);
    if(pi->stage_type == 'a')
    {
        res = semTimedWait(&acoustic_stage, &ts, &wait_mutex); // wait for acoustic stage
        if(res == ETIMEDOUT)
        {
            printf(RED "%s (who plays instrument %c) left due to impatience\n" RESET, pi->name, pi->instrument);
            pthreadMutexUnlock(&wait_mutex);
            return NULL; // musician becomes impatient and leaves
        }
        stage_type = 'a';
    }
    else if(pi->stage_type == 'e')
    {
        res = semTimedWait(&electric_stage, &ts, &wait_mutex); // wait for electric stage
        if(res == ETIMEDOUT)
        {
            printf(RED "%s (who plays instrument %c) left due to impatience\n" RESET, pi->name, pi->instrument);
            pthreadMutexUnlock(&wait_mutex);
            return NULL; // musician becomes impatient and leaves
        }
        stage_type = 'e';
    }
    else
    {
        res = semTimedWait(&stage, &ts, &wait_mutex); // wait for a stage to become available for at most t seconds
        if(res == ETIMEDOUT)
        {
            printf(RED "%s (who plays instrument %c) left due to impatience\n" RESET, pi->name, pi->instrument);
            pthreadMutexUnlock(&wait_mutex);
            return NULL; // musician becomes impatient and leaves
        }
        stage_type = chooseStage();
    }
    if(stage_type == 'a')
        all_performers[(pi->performer_num)-1]->status = removeFromList(acoustic_stage_list); // update status
    else
        all_performers[(pi->performer_num)-1]->status = removeFromList(electric_stage_list); // update status
    semPost(&musician_performing); // signal that musician is performing
    pthreadMutexUnlock(&wait_mutex);

    // performance starts
    int stage_num = all_performers[(pi->performer_num)-1]->status;
    int performance_duration = rand() % (ti->t2 - ti->t1 + 1) + ti->t1;
    printf(BLUE "%s (who plays instrument %c) performing on %s stage (stage number %d) for %d seconds\n" RESET,
           pi->name, pi->instrument, (stage_type == 'a') ? "acoustic" : "electric", stage_num, performance_duration);

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += performance_duration;
    
    int singer_num = 0;
    res = semTimedWait(&singer_joined, &ts, NULL); // wait for singer to join performance
    if(res == ETIMEDOUT)
    {
        semWait(&musician_performing, NULL); // musician is done performing (will not block)
    }
    else
    {
        pthreadMutexLock(&singer_mutex);
        singer_num = removeFromList(singer_list);
        printf(YELLOW "%s joined %s's performance. Performance time extended by 2 seconds\n" RESET, all_performers[singer_num-1]->name, pi->name);
        pthreadMutexUnlock(&singer_mutex);

        ts.tv_sec += 2;
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts, NULL); // wait for performance to get over
    }

    // performance ends
    pthreadMutexLock(&wait_mutex);
    endPerformance(stage_type, pi);
    if(singer_num != 0)
    {
        semPost(&singer_not_performing); // signal that singer is done performing
        all_performers[singer_num-1]->status = -1; // update status of singer
        pthreadCondBroadcast(&singer_left); // signal that singer finished performance with musician
        printf(MAGENTA "%s (who plays instrument %c) and %s (singer) have finished performing on %s stage (stage number %d)\n" RESET,
               pi->name, pi->instrument, all_performers[singer_num-1]->name, (stage_type == 'a') ? "acoustic" : "electric", stage_num);
    }
    else if(res == ETIMEDOUT)
    {
        printf(MAGENTA "%s (who plays instrument %c) has finished performing on %s stage (stage number %d)\n" RESET,
               pi->name, pi->instrument, (stage_type == 'a') ? "acoustic" : "electric", stage_num);
    }
    pthreadMutexUnlock(&wait_mutex);

    // collect t-shit
    collectTshirt(pi);

    return NULL;
}


// ------------------- MAIN (THREAD) -------------------
int main()
{
    srand(time(0));
    int k, a, e, c;
    ti = (timeInfo*)malloc(sizeof(timeInfo));

    printf("Enter the details of the event: ");
    scanf("%d %d %d %d %d %d %d", &k, &a, &e, &c, &ti->t1, &ti->t2, &ti->t);

    initializeGlobalData(a, e, c);
    initializeList(&singer_list, k);
    initializeList(&acoustic_stage_list, a);
    initializeList(&electric_stage_list, e);

    for(int i=0; i<a; i++)
        acoustic_stage_list->arr[i] = i+1;
    for(int i=0; i<e; i++)
        electric_stage_list->arr[i] = a+i+1;

    pthread_t performers[k];
    printf("Enter the details of each performer: \n");
    for(int i=0; i<k; i++)
    {
        all_performers[i] = (performerInfo*)malloc(sizeof(performerInfo));
        scanf("%s %c %d", all_performers[i]->name, &all_performers[i]->instrument, &all_performers[i]->arrival_time);
        all_performers[i]->performer_num = i+1;
        all_performers[i]->status = -1;

        if(all_performers[i]->instrument == 'v')
            all_performers[i]->stage_type = 'a';
        else if(all_performers[i]->instrument == 'b')
            all_performers[i]->stage_type = 'e';
        else
            all_performers[i]->stage_type = 'b';
    }

    // create threads
    for(int i=0; i<k; i++)
    {
        if(all_performers[i]->instrument == 's')
            pthreadCreate(&performers[i], NULL, singerHandler, (void*)all_performers[i]);
        else
            pthreadCreate(&performers[i], NULL, musicianHandler, (void*)all_performers[i]);
    }

    // join threads
    for(int i=0; i<k; i++)
        pthreadJoin(performers[i], NULL);

    // free memory
    for(int i=0; i<k; i++)
        free(all_performers[i]);
    free(ti);
    free(singer_list);
    free(acoustic_stage_list);
    free(electric_stage_list);

    printf(GREEN "Event finished\n" RESET);
    return 0;
}
