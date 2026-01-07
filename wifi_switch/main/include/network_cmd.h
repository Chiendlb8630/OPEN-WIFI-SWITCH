#pragma once

#define ROOT_TOPIC     "swDevice/"
#define ROOT_TOPIC_LEN  9

char* cmd_macToTopic(void);
void cmd_parse_ctrlData(int data_len, const char *data);
void cmd_pub_staData(int i, int status);
