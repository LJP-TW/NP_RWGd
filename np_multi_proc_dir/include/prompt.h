#ifndef PROMPT_H
#define PROMPT_H

#define MAX_MSG_LEN 0xf00

// Message type
#define MSG_NONE   0
#define MSG_LOGOUT 1

#define MSG_STR_TAG 0xcc

typedef struct msg_str_tag msg_str;
struct msg_str_tag {
    char tag;
    char content[1];
};

typedef struct message_tag message;
struct message_tag {
    int type;
    int uid;
    int write_offset;
    char mem[MAX_MSG_LEN]; // Ring Buffer
};

typedef struct message_buf_tag message_buf;
struct message_buf_tag {
    // msg resides in shared memory
    message *msg;
    
    int read_offset;

    int shmid; // shared memory id

    // semaphore id
    // sem[0] - write_sem
    // sem[1] - read_sem
    int semid;
};

extern message_buf global_msg;

extern void global_msg_init(void);

extern void global_msg_release(void);

extern void msg_set_msg(char *msg, int msg_type);

extern void msg_read_msg(void);

extern void msg_reset_read_offset(void);

extern void msg_tell(int sock, char *msg);

// Outputing prompt
extern void msg_prompt(int sock);

extern void msg_motd(int sock);

#endif