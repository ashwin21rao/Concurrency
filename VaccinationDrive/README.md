## PROGRAM STRUCTURE

There are 3 groups of child threads:
- Pharmaceutical Companies
- Vaccination Zones
- Students

There can be a maximum of 10,000 students and 10,000 pharmaceutical companies.  
Mutexes and condition variables have been used for thread synchronization.  

## SYNCHRONIZATION LOGIC

- Pharmaceutical Companies and Vaccination Zones have a Producer-Consumer relationship. 
    
    - A Vaccination Zone waits for a company to deliver a batch of vaccines to it.
      
      ```
      while(total_batches <= 0 && done == 0)
          pthreadCondWait(&created_batch, &batch_mutex);
      
    - A Pharmaceutical Company waits until all its vaccine batches have been used before resuming production.
      
      ```
      while(all_companies[(ci->company_num)-1]->batches_left > 0 && done == 0)
          pthreadCondWait(&used_batch, &batch_mutex); 
      
- Synchronization between Vaccination Zones and Students is as follows.
   
    - Each vaccination zone has one mutex and two condition variables associated with it.
      1. Using the condition variable ```filled_slot```, a student signals a particular vaccination zone that he/she 
         has been added to the waiting queue of that zone.
      2. Using the condition variable ```vaccinated```, the vaccination zone signals a student that his/her
         vaccination has been completed.
        
    - Students arrive at the gate at random times (which is implemented by making the student threads sleep for a 
      random amount of time before signalling that they are ready for vaccination) and join the waiting queue of a 
      vaccination zone. 
    
    - A Vaccination Zone waits for students to become ready for vaccination before making slots ready, that is, it 
      allots slots once its waiting queue has students in it.
      
      ```
      while(all_zones[zone_num-1]->total_students <= 0 && done == 0)
          pthreadCondWait(&all_zones[zone_num-1]->filled_slot, &all_zones[zone_num-1]->slot_mutex);
      ```  
    
    - The number of slots in any Vaccination Phase will be less than or equal to the total number of students in the 
      waiting queue of that zone at that time. The students who are not allocated a slot must wait for the vaccination phase to 
      finish. These students will be allocated a slot in the next phase of that vaccination zone.
            
    - A Student waits for his/her vaccination to be completed, which may be successful (```result = 1```) or 
      unsuccessful, in which case he may be sent for re-vaccination. A signal indicating that a vaccination phase has 
      been completed is sent from the vaccination zone to the students using ```pthreadCondBroadcast``` which is 
      handled in the corresponding student threads (those in which ```result``` has become ```1``` or 
      ```vaccination_round``` has increased indicating that the student needs to be re-vaccinated) and ignored 
      in others. The next round of vaccination can happen in any zone.
      
      ```
      while(all_students[(si->student_num)-1]->vaccination_round == round_number && all_students[(si->student_num)-1]->result == 0)
          pthreadCondWait(&all_zones[zone_num-1]->vaccinated, &all_zones[zone_num-1]->slot_mutex);
      ```    

- The global variable ```done``` is set to ```1``` when the Vaccination Drive is completed (all students have been 
  vaccinated successfully or completed 3 rounds of vaccination). At this point, the threads of Vaccination Zones and 
  Pharmaceutical Companies are terminated and the simulation ends.
