typedef struct {
    int locked; //locked if 1, unlocked if 0
    void* _chan; //channel value of lock
    int old_nice; //old nice value
    int proc; //pointer to process struct of holder (cast as int)
    int lowest_nice;
} mutex;