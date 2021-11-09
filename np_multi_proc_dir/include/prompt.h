#ifndef PROMPT_H
#define PROMPT_H

#define MAX_MSG_LEN 0xf00

typedef struct message_buf_tag message_buf;
struct message_buf_tag {
    // msg resides in shared memory
    char *msg;

    int shmid; // shared memory id

    // semaphore id
    // sem[0] - write_sem
    // sem[1] - read_sem
    int semid;
};

extern message_buf global_msg;

extern void global_msg_init(void);

extern void global_msg_release(void);

extern void msg_write_wait(void);

extern void msg_write_signal(void);

extern void msg_read_wait(void);

extern void msg_read_signal(void);

extern void msg_tell(int sock, char *msg);

// Outputing prompt
extern void msg_prompt(int sock);

extern void msg_motd(int sock);

#endif