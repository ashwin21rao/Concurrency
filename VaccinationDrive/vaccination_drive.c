# include <stdio.h>
# include <stdlib.h>
# include <unistd.h>
# include <pthread.h>
# include <time.h>
# define RED "\033[0;31m"
# define BLUE "\033[0;34m"
# define GREEN "\033[0;32m"
# define YELLOW "\033[0;33m"
# define CYAN "\033[0;36m"
# define MAGENTA "\e[0;35m"
# define RESET "\033[m"

// ------------------- MUTEXES AND CONDITION VARIABLES -------------------
pthread_mutex_t batch_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t used_batch = PTHREAD_COND_INITIALIZER; // signal from zone to company
pthread_cond_t created_batch = PTHREAD_COND_INITIALIZER; // signal from company to zone


// ------------------- GLOBAL STRUCTURES -------------------
typedef struct studentInfo {
    int student_num;
    int vaccination_round;
    int result;
} studentInfo;

typedef struct companyInfo {
    int company_num;
    double success_prob;
    int batches_left;
} companyInfo;

typedef struct batch {
    int capacity;
    companyInfo* company;
} batch;

typedef struct zoneInfo {
    int zone_num;
    int available_students[10000]; // queue of students (numbers) available for vaccination at that zone
    int add_ptr;
    int remove_ptr;
    int total_students; // number of remaining students waiting to be vaccinated at that zone
    int MAX_STUDENT_NUM; // maximum possible number of available students at any point in time
    pthread_mutex_t slot_mutex;
    pthread_cond_t filled_slot; // signal that student is available to fill slot
    pthread_cond_t vaccinated; // signal that student has been vaccinated
} zoneInfo;


// ------------------- BATCH RELATED GLOBAL VARIABLES -------------------
companyInfo* all_companies[1000];
batch available_batches[20000]; // queue of available batches to be used
int fill_ptr = 0;
int use_ptr = 0;
int total_batches = 0; // number of available batches
int MAX_BATCH_NUM; // maximum possible number of available batches at any point in time


// ------------------- STUDENT RELATED GLOBAL VARIABLES -------------------
studentInfo* all_students[10000];


// ------------------- ZONE RELATED GLOBAL VARIABLES -------------------
int total_zones;
zoneInfo* all_zones[10000];


// ------------------- SIMULATION RELATED GLOBAL VARIABLES -------------------
int done = 0;


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

void pthreadCondSignal(pthread_cond_t *cond)
{
    if(pthread_cond_signal(cond) != 0)
        perror("ERROR");
}

void pthreadCondBroadcast(pthread_cond_t *cond)
{
    if(pthread_cond_broadcast(cond) != 0)
        perror("ERROR");
}


// ------------------- GLOBAL DATA INITIALIZATION -------------------
void initializeZoneData(zoneInfo **z, int zone_num, int o)
{
    *z = (zoneInfo*)malloc(sizeof(zoneInfo));
    (*z)->zone_num = zone_num;
    (*z)->add_ptr = 0;
    (*z)->remove_ptr = 0;
    (*z)->total_students = 0;
    (*z)->MAX_STUDENT_NUM = o;
    pthread_mutex_init(&(*z)->slot_mutex, NULL);
    pthread_cond_init(&(*z)->filled_slot, NULL);
    pthread_cond_init(&(*z)->vaccinated, NULL);
}


// ------------------- FUNCTIONS UPDATING GLOBAL DATA -------------------
batch useBatch()
{
    batch b = available_batches[use_ptr];
    (b.company)->batches_left--;
    use_ptr = (use_ptr + 1) % MAX_BATCH_NUM;
    total_batches--;
    return b;
}

void createBatch(int capacity, companyInfo* company)
{
    batch b = {capacity, company};
    (b.company)->batches_left++;
    available_batches[fill_ptr] = b;
    fill_ptr = (fill_ptr + 1) % MAX_BATCH_NUM;
    total_batches++;
}

int removeStudent(zoneInfo* zone)
{
    int student_num = zone->available_students[zone->remove_ptr];
    zone->remove_ptr = (zone->remove_ptr + 1) % zone->MAX_STUDENT_NUM;
    zone->total_students--;
    return student_num;
}

void addStudent(zoneInfo* zone, int student_num)
{
    zone->available_students[zone->add_ptr] = student_num;
    zone->add_ptr = (zone->add_ptr + 1) % zone->MAX_STUDENT_NUM;
    zone->total_students++;
}


// ------------------- COMPANY THREAD HANDLER -------------------
void* companyHandler(void* input)
{
    companyInfo* ci = (companyInfo*)input;
    int r, w, p;

    int count = 0;
    while(1)
    {
        pthreadMutexLock(&batch_mutex);
        while(all_companies[(ci->company_num)-1]->batches_left > 0 && done == 0)
            pthreadCondWait(&used_batch, &batch_mutex); // wait until no batches are left
        if(done == 1)
        {
            pthreadMutexUnlock(&batch_mutex);
            break; // vaccination drive is done
        }

        if(count > 0)
            printf(BLUE "All the vaccines prepared by Company %d are used. Resuming production now\n" RESET, ci->company_num);
        pthreadMutexUnlock(&batch_mutex);
        sleep(1); // time taken to resume production

        // create r batches at once
        w = rand() % 4 + 2;
        r = rand() % 5 + 1;
        p = rand() % 11 + 10;
        printf(BLUE "Pharmaceutical Company %d is preparing %d batch(es) of vaccines with success probability %0.2lf\n" RESET, ci->company_num, r, 100 * ci->success_prob);
        sleep(w); // time taken to create batches

        pthreadMutexLock(&batch_mutex);
        for(int i=0; i<r; i++)
        {
            createBatch(p, all_companies[(ci->company_num)-1]);
            pthreadCondSignal(&created_batch); // signal that batch has been created
        }
        printf(BLUE "Pharmaceutical Company %d has prepared %d batch(es) of vaccines with success probability %0.2lf\n" RESET, ci->company_num, r, 100 * ci->success_prob);
        pthreadMutexUnlock(&batch_mutex);
        count++;
    }
    return NULL;
}


// ------------------- ZONE THREAD HANDLER -------------------
void* zoneHandler(void* input)
{
    zoneInfo* zi = (zoneInfo*)input;
    while(1)
    {
        pthreadMutexLock(&batch_mutex);
        while(total_batches <= 0 && done == 0)
            pthreadCondWait(&created_batch, &batch_mutex); // wait for batch to be created
        if(done == 1)
        {
            pthreadMutexUnlock(&batch_mutex);
            break; // vaccination drive is done
        }

        batch b = useBatch();

        printf(BLUE "Pharmaceutical Company %d delivering a batch (success probability %0.2lf) to Vaccination Zone %d\n" RESET, b.company->company_num, 100 * b.company->success_prob, zi->zone_num);
        sleep(1); // time taken to deliver batch from company to vaccination zone
        printf(BLUE "Vaccination Zone %d has received a batch from Pharmaceutical Company %d, resuming vaccinations now\n" RESET, zi->zone_num, b.company->company_num);

        pthreadCondBroadcast(&used_batch); // signal to all companies that batch has been used
        pthreadMutexUnlock(&batch_mutex);
        sleep(1); // time taken to resume vaccination

        int vaccines_left = b.capacity;
        int zone_num = zi->zone_num;
        while(vaccines_left > 0)
        {
            // wait for students to become available
            pthreadMutexLock(&all_zones[zone_num-1]->slot_mutex);
            while(all_zones[zone_num-1]->total_students <= 0 && done == 0)
            {
                printf(YELLOW "Vaccination Zone %d waiting for students to become available\n" RESET, zone_num);
                pthreadCondWait(&all_zones[zone_num-1]->filled_slot, &all_zones[zone_num-1]->slot_mutex);
            }
            if(done == 1)
            {
                pthreadMutexUnlock(&all_zones[zone_num-1]->slot_mutex);
                return NULL; // vaccination drive completed
            }

            // slots ready for vaccination phase
            int k = rand() % 8 + 1;
            k = (k < vaccines_left) ? ((k < all_zones[zone_num-1]->total_students) ? k : all_zones[zone_num-1]->total_students) :
                                      ((vaccines_left < all_zones[zone_num-1]->total_students) ? vaccines_left : all_zones[zone_num-1]->total_students);

            printf(MAGENTA "Vaccination Zone %d is ready to vaccinate with %d slot(s)\n" RESET, zone_num, k);

            // wait for slots to be filled
            int students[k];
            for(int i=0; i<k; i++)
            {
                if(i > 0) // lock is already held first time the loop is entered (guaranteeing that at least 1 student will be assigned a slot)
                    pthreadMutexLock(&all_zones[zone_num-1]->slot_mutex);

                students[i] = removeStudent(all_zones[zone_num-1]);

                printf(GREEN "Student %d assigned a slot in Vaccination Zone %d and is waiting to be vaccinated\n" RESET, students[i], zone_num);
                pthreadMutexUnlock(&all_zones[zone_num-1]->slot_mutex);
            }

            printf(MAGENTA "Vaccination Zone %d entering vaccination phase\n" RESET, zone_num);
            sleep(1); // time taken to enter vaccination phase

            pthreadMutexLock(&all_zones[zone_num-1]->slot_mutex);
            pthreadMutexUnlock(&all_zones[zone_num-1]->slot_mutex);

            // vaccination phase starts
            double result;
            for(int i=0; i<k; i++)
            {
                sleep(1); // time taken to vaccinate a student
                printf(RED "Student %d in Vaccination Zone %d has been vaccinated (success probability %0.2lf)\n" RESET, students[i], zi->zone_num, 100 * b.company->success_prob);

                // antibody test
                sleep(1); // time taken to perform antibody test
                result = rand() / (double)RAND_MAX;
                if(result < b.company->success_prob)
                {
                    printf(RED "Student %d has tested POSITIVE for antibodies! :)\n" RESET, students[i]);
                    pthreadMutexLock(&all_zones[zone_num-1]->slot_mutex);
                    all_students[students[i]-1]->result = 1;
                    pthreadCondBroadcast(&all_zones[zone_num-1]->vaccinated); // signal that student has been vaccinated
                    pthreadMutexUnlock(&all_zones[zone_num-1]->slot_mutex);
                }
                else
                {
                    printf(RED "Student %d has tested NEGATIVE for antibodies! :(\n" RESET, students[i]);
                    pthreadMutexLock(&all_zones[zone_num-1]->slot_mutex);
                    all_students[students[i]-1]->vaccination_round++;
                    pthreadCondBroadcast(&all_zones[zone_num-1]->vaccinated); // signal that student has been vaccinated
                    pthreadMutexUnlock(&all_zones[zone_num-1]->slot_mutex);
                }
            }
            vaccines_left -= k;
        }
        printf(MAGENTA "Vaccination Zone %d has run out of vaccines\n" RESET, zone_num);
    }
    return NULL;
}


// ------------------- STUDENT THREAD HANDLER -------------------
void* studentHandler(void* input)
{
    studentInfo* si = (studentInfo*)input;
    int round_number;

    while(1)
    {
        round_number = si->vaccination_round;
        if(all_students[(si->student_num)-1]->result == 1)
        {
            printf(CYAN "Student %d has been successfully vaccinated, can now attend college!\n" RESET, si->student_num);
            break;
        }
        if(round_number > 3)
        {
            printf(CYAN "Student %d could not be vaccinated, cannot attend college\n" RESET, si->student_num);
            break;
        }

        // randomise initial student arrival
        if(round_number == 1)
            sleep(rand() % 20 + 1); // students become available for vaccination at different times

        printf(GREEN "Student %d has arrived for vaccination (round %d)\n" RESET, si->student_num, round_number);
        printf(GREEN "Student %d waiting to be allocated a slot in a Vaccination Zone\n" RESET, si->student_num);

        int zone_num = rand() % total_zones + 1;
        pthreadMutexLock(&all_zones[zone_num-1]->slot_mutex);
        addStudent(all_zones[zone_num-1], si->student_num);
        pthreadCondSignal(&all_zones[zone_num-1]->filled_slot); // signal that student is ready for vaccination
        pthreadMutexUnlock(&all_zones[zone_num-1]->slot_mutex);

        // wait until student has completed current round of vaccination
        pthreadMutexLock(&all_zones[zone_num-1]->slot_mutex);
        while(all_students[(si->student_num)-1]->vaccination_round == round_number && all_students[(si->student_num)-1]->result == 0)
            pthreadCondWait(&all_zones[zone_num-1]->vaccinated, &all_zones[zone_num-1]->slot_mutex);
        pthreadMutexUnlock(&all_zones[zone_num-1]->slot_mutex);
    }
    return NULL;
}


// ------------------- MAIN (THREAD) -------------------
int main()
{
    srand(time(0));
    int n, m, o;
    printf("Enter the number of companies, vaccination zones and students: ");
    scanf("%d %d %d", &n, &m, &o);
    MAX_BATCH_NUM = n * 20;

    // handle the case when n, m, o = 0
    if(n == 0 || m == 0 || o == 0)
    {
        if(n == 0)
            printf(CYAN "No Pharmaceutical Companies are available to prepare vaccines\n" RESET);
        if(m == 0)
            printf(CYAN "No Vaccination Zones available for vaccination\n" RESET);
        if(o == 0)
            printf(CYAN "No students available for vaccination\n" RESET);
        printf(CYAN "Vaccination drive unsuccessful\n" RESET);
        return 0;
    }

    total_zones = m;
    for(int i=0; i<m; i++)
        initializeZoneData(&all_zones[i], i+1, o); // initialize zone data

    pthread_t companies[n], zones[m], students[o];
    printf("Enter the success probabilities of each company (between 0 and 1): ");
    for(int i=0; i<n; i++)
    {
        all_companies[i] = (companyInfo*)malloc(sizeof(companyInfo));
        all_companies[i]->company_num = i+1;
        all_companies[i]->batches_left = 0;
        scanf("%lf", &all_companies[i]->success_prob);
        pthreadCreate(&companies[i], NULL, companyHandler, (void*)all_companies[i]);
        sleep(1);
    }

    for(int i=0; i<o; i++)
    {
        all_students[i] = (studentInfo*)malloc(sizeof(studentInfo));
        all_students[i]->student_num = i+1;
        all_students[i]->vaccination_round = 1;
        all_students[i]->result = 0;
        pthreadCreate(&students[i], NULL, studentHandler, (void*)all_students[i]);
    }

    for(int i=0; i<m; i++)
    {
        pthreadCreate(&zones[i], NULL, zoneHandler, (void*)all_zones[i]);
        sleep(1);
    }

    // join threads
    for(int i=0; i<o; i++)
        pthreadJoin(students[i], NULL);

    // signal all waiting companies and zones that simulation is done
    done = 1;
    for(int i=0; i<m; i++)
        pthreadCondSignal(&all_zones[i]->filled_slot);
    pthreadCondBroadcast(&created_batch);
    pthreadCondBroadcast(&used_batch);

    for(int i=0; i<n; i++)
        pthreadJoin(companies[i], NULL);
    for(int i=0; i<m; i++)
        pthreadJoin(zones[i], NULL);

    // free memory
    for(int i=0; i<n; i++)
        free(all_companies[i]);
    for(int i=0; i<m; i++)
        free(all_zones[i]);
    for(int i=0; i<o; i++)
        free(all_students[i]);

    printf(CYAN "All students are done with vaccination\n" RESET);
    printf(CYAN "Vaccination drive completed!\n" RESET);
    return 0;
}
