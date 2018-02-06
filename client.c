#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include <ncurses.h>

#define CHOICES 7
#define REFRESH_PER_SECOND 100
#define REFRESH_RATE 10000

#define MSG_ATTACK_MSG 4
#define MSG_ATTACK_ORDER 5
#define MSG_NOTI 6
#define MSG_GAME_OVER 7
#define MSG_READY 8
#define MSG_UNIT_PRODUCTION 9
#define MSG_PRODUCTION_ORDER 10
#define MSG_PLAYER_INFO 11
#define MSG_RESOURCES 12
#define MSG_CONNECT 13
#define MSG_ALERTS 14

#define MEM_PLAYER1 101
#define MEM_PLAYER2 102
#define MEM_PLAYER3 103

#define SEM_1 99

#define NO_CASH 1


#define PLAYER_TYPE_1 1
#define PLAYER_TYPE_2 2
#define PLAYER_TYPE_3 3
#define POS 25
#define SZE 50
#define DELAY 30000

struct GeneralMessage {
    long type;
    char text [50];
};

struct PaymentMessage {
    long type;
    int unit_count;
    int unit_id;
};

struct TerminateMessage {
    long type;
    int player_id;
    int kill_status;
};


struct ReadyMessage {
    long type;
    int player_id;
    int status;
};

struct AttackMessage {
    long type; //defence 1,2,3 attack 4,5,6
    int status; // -1 no units, 0 lost, 1 won
    int attacker;
    int defender;
    double attack_force;
    double defence_force;
    int li_attack;
    int li_defence;
    int hi_attack;
    int hi_defence;
    int c_attack;
    int c_defence;
};

struct ProductionOrder {
    long type;
    int unit_id;
    int unit_count;
};

struct AttackOrder {
    long type;
    int to;
    int li_count;
    int hi_count;
    int c_count;
};

struct Player {
    long type;
    int wins;
    char name[30];
    int gold;
    int workers;
    int light_infantry;
    int heavy_infantry;
    int cavalry;
    int income_per_second;
    int status;
    //int won_games;
};

void sem_wait(int sem_id) {
    struct sembuf semdwn;
    semdwn.sem_num = 0;
    semdwn.sem_op = -1;
    semdwn.sem_flg = 0;
    if(semop(sem_id, &semdwn, 1) == -1) {
        perror("Error in semaphore wait!\n");
    }
}

void sem_signal(int sem_id) {
    struct sembuf semup;
    semup.sem_num = 0;
    semup.sem_op = 1;
    semup.sem_flg = 0;
    if(semop(sem_id, &semup, 1) == -1) {
        perror("Error in semaphore signal!\n");
    }
    
}

void fetch_data_from_server(int q_player_info, struct Player * player) {
    //player->gold = player->gold + (player->income_per_second / REFRESH_PER_SECOND);
    struct Player received_player;
    if(msgrcv(q_player_info, &received_player, sizeof(struct Player) - sizeof(long), player->type, 0) == -1) {
        perror("Error in receiving player info: client");
        exit(0);
    }
    else {
        player->gold = received_player.gold;
        player->workers = received_player.workers;
        player->income_per_second = received_player.income_per_second;
        player->light_infantry = received_player.light_infantry;
        player->heavy_infantry = received_player.heavy_infantry;
        player->cavalry = received_player.cavalry;
    }
    
}

void update_actions(WINDOW * actions, WINDOW * notifications) {
        
}

void update_notifications(WINDOW * notifications, struct GeneralMessage message) {
    char text_buffer[50];
    wclear(notifications);
    box(notifications, 0, 0);
    strcpy(text_buffer, message.text);
    mvwprintw(notifications, 1, 1, text_buffer);
    wrefresh(notifications);
    
}

void update_defences_box(WINDOW * defences, struct AttackMessage report) {
    char defence_report_text[50];
    char text_buffer[50];
    switch (report.status) {
        case 0:
            sprintf(defence_report_text, "WE WERE ATTACKED BY PLAYER %d. OUR UNITS WERE CRUSHED!", report.attacker);
            break;
        case 1:
            sprintf(defence_report_text, "WE WERE ATTACKED BY PLAYER %d. OUR UNITS HAVE WON", report.attacker);
            break;
        default:
            sprintf(defence_report_text, "UNKNOWN REPORT STATUS ERR");
            break;
    }
    wclear(defences);
    box(defences, 0, 0);
    mvwprintw(defences, 1, 1, defence_report_text);
    wrefresh(defences);
}

void update_attack_box(WINDOW * attacks, struct AttackMessage report) {
    char attack_report_text[50];
    switch (report.status) {
        case -1:
            sprintf(attack_report_text, "ATTACK NOT SENT. NO UNITS AVAILABLE");
            break;
        case 0:
            sprintf(attack_report_text, "ATTACK LOST, PLAYER %d HAS WON THE BATTLE", report.defender);
            break;
        case 1:
            sprintf(attack_report_text, "ATTACK WON! PLAYER %d HAS LOST THE BATTLE", report.defender);
            break;
        default:
            sprintf(attack_report_text, "UNKNOWN REPORT STATUS ERR");
            break;
    }
    wclear(attacks);
    box(attacks, 0, 0);
    mvwprintw(attacks, 1, 1, attack_report_text);
    wrefresh(attacks);
}

void listen_notifications(int q_notifications, WINDOW * notifications, int player_type) {
    struct GeneralMessage msg;
    int msg_size = sizeof(struct GeneralMessage) - sizeof(long);
    
    if(msgrcv(q_notifications, &msg, msg_size, player_type, IPC_NOWAIT) > 0) {
        update_notifications(notifications, msg);
    }
    
}

void listen_defence_message(int q_attack_message, WINDOW * defences, int player_type) {
    struct AttackMessage report;
    int msg_size = sizeof(struct AttackMessage) - sizeof(long);
    if(msgrcv(q_attack_message, &report, msg_size, player_type, IPC_NOWAIT) > 0) {
        update_defences_box(defences, report);
    }
}

void listen_attack_message(int q_attack_message, WINDOW * attacks, int player_type) {
    struct AttackMessage report;
    int msg_size = sizeof(struct AttackMessage) - sizeof(long);
    if(msgrcv(q_attack_message, &report, msg_size, player_type + 3, IPC_NOWAIT) > 0) {
        update_attack_box(attacks, report);
    }
}


void update_info(WINDOW * info, struct Player display_player) {
    int x = 0, y = 0;
    char gold_buffer[8];
    char workers_buffer[4];
    char light_infantry_buffer[4];
    char heavy_infantry_buffer[4];
    char cavalry_buffer[4];
    char income_buffer[4];
    char id_buffer[3];
    char wins_buffer[3];
    wclear(info);
    box(info, 0, 0);
    sprintf(gold_buffer, "%d", display_player.gold);  
    sprintf(workers_buffer, "%d", display_player.workers);
    sprintf(light_infantry_buffer, "%d", display_player.light_infantry);
    sprintf(heavy_infantry_buffer, "%d", display_player.heavy_infantry);
    sprintf(cavalry_buffer, "%d", display_player.cavalry);
    sprintf(income_buffer, "%d", display_player.income_per_second);
    sprintf(id_buffer, "%ld", display_player.type);
    sprintf(wins_buffer, "%d", display_player.wins);
    mvwprintw(info, y + 1, x + 1, "Gold    W   L   H   C    Income   ID   MyWins");
    mvwprintw(info, y + 2, x + 1, gold_buffer); 
    mvwprintw(info, y + 2, x + 9, workers_buffer); 
    mvwprintw(info, y + 2, x + 13, light_infantry_buffer); 
    mvwprintw(info, y + 2, x + 17, heavy_infantry_buffer); 
    mvwprintw(info, y + 2, x + 21, cavalry_buffer); 
    mvwprintw(info, y + 2, x + 26, income_buffer); 
    mvwprintw(info, y + 2, x + 35, id_buffer); 
    mvwprintw(info, y + 2, x + 40, wins_buffer);
    wrefresh(info);
}

void send_production_order (int q_production, int unit_id, int unit_count, int player_type) {
    struct ProductionOrder order;
    order.type = player_type;
    order.unit_id = unit_id;
    order.unit_count = unit_count;
    int order_size = sizeof(struct ProductionOrder) - sizeof(long);
    if(msgsnd(q_production, &order, order_size, 0) == -1) {
        perror("Error in sending production order : client");
    }
    
}

void send_attack_order(int q_attack_order, int destination, int li_count, int hi_count, int c_count, int player_type) {
    struct AttackOrder order;
    int order_size = sizeof(struct AttackOrder) - sizeof(long);
    order.type = player_type;
    order.to = destination;
    order.li_count = li_count;
    order.hi_count = hi_count;
    order.c_count = c_count;
    if(msgsnd(q_attack_order, &order, order_size, 0) == -1) {
        perror("Error in sending attack order : client");
    }
    
}

int get_unit_amount(WINDOW * amounts, WINDOW * notifications) {
    //mvwprintw(amounts, 3, 1,  "How many units do you wish to train? [1-9]");
    //wrefresh(amounts);
    char c = mvwgetch(amounts, 1, 20);
    char buf [1];
    sprintf(buf, "%c", c);
    //mvwprintw(amounts, 3, 1, buf);
    //redrawwin(amounts);
    long unit_count = strtol(buf, NULL, 10);
    return unit_count;
    
}

void clear_queue (int queue) {
    struct Player dummy;
    printf("In clear queue: %d", queue);
    while(msgrcv(queue, &dummy, sizeof(struct Player) - sizeof(long), 0, 0) > 0) {
        
    }
}

void send_terminate_message (struct Player * player) {
    int q_terminate;
    int terminate_msg_length = sizeof(struct TerminateMessage) - sizeof(long);
    struct TerminateMessage term_msg;
    term_msg.type = player->type;
    term_msg.kill_status = 1;
    q_terminate = msgget(MSG_GAME_OVER, IPC_CREAT | 0640);
    if(msgsnd(q_terminate, &term_msg, terminate_msg_length, 0) == -1) {
        perror("Error in sending terminate signal : client");
    }
    usleep(REFRESH_RATE);
}

void ask_for_id(struct Player * player, int q_ready) {
    struct ReadyMessage ready_msg;
    //type 1 for send,  type 2 for receive
    ready_msg.type = 1;
    ready_msg.player_id = 0;
    ready_msg.status = 0;
    int ready_message_size = sizeof(struct ReadyMessage) - sizeof(long);
    if(msgsnd(q_ready, &ready_msg, ready_message_size, 0) == -1) {
        perror("Error in sending ready confirmation : client");
    }
    if(msgrcv(q_ready, &ready_msg, ready_message_size, 2, 0) == -1) {
        perror("Error in receiving ready confirmation with ID : client");
    }
    player->type = ready_msg.player_id;
}

int main (int argc, char *argv[]) {
    int q_player_info;
    int q_production;
    int q_notifications;
    int q_ready;
    int q_attack_order;
    int q_attack_message;
    int sem;
    int opponent1;
    int opponent2;
    int f, g = 1;
    
    //queues
    q_player_info = msgget(MSG_PLAYER_INFO, IPC_CREAT | 0640);
    q_production = msgget(MSG_PRODUCTION_ORDER, IPC_CREAT | 0640);\
    q_ready = msgget(MSG_READY, IPC_CREAT | 0640);
    q_notifications = msgget(MSG_NOTI, IPC_CREAT | 0640);
    q_attack_order = msgget(MSG_ATTACK_ORDER, IPC_CREAT | 0640);
    q_attack_message = msgget(MSG_ATTACK_MSG, IPC_CREAT | 0640);
    sem = semget(SEM_1, 1, IPC_CREAT | 0640);
    semctl(sem, 0, SETVAL, 1);
    //player
    struct Player * player = malloc(sizeof(*player));
    player->type = 1;
    strcpy(player->name, "Mike");
    player->income_per_second = 50;
    player->workers = 0;
    player->light_infantry = 0;
    player->heavy_infantry = 0;
    player->cavalry = 0;
    player->gold = 300;
    //player->won_games = 0;
    
    
    //init windows
    initscr();
    cbreak();
    curs_set(0);
    WINDOW * info = newwin(5,50,1,1);
    WINDOW * actions = newwin(9,50,7,1);
    WINDOW * notifications = newwin(4, 50, 1, 54);
    WINDOW * attacks = newwin(4, 50, 5, 54);
    WINDOW * defences = newwin(4, 50, 9, 54);
    WINDOW * amounts = newwin(3, 50, 13, 54);
    keypad(stdscr, FALSE);
    keypad(info, FALSE);
    keypad(notifications, FALSE);
    keypad(actions, TRUE);
    keypad(attacks, FALSE);
    keypad(amounts, TRUE);
    keypad(defences, FALSE);
    nodelay(stdscr, TRUE);
    nodelay(info, FALSE);
    nodelay(amounts, FALSE);
    nodelay(actions, TRUE);
    nodelay(attacks, FALSE);
    nodelay(defences, FALSE);
    leaveok(stdscr, TRUE);
    leaveok(info, TRUE);
    leaveok(notifications, TRUE);
    leaveok(actions, TRUE);
    leaveok(amounts, TRUE);
    leaveok(attacks, TRUE);
    leaveok(defences, TRUE);
    
    box(info, 0, 0);
    box(actions, 0, 0);
    box(notifications, 0, 0);
    box(amounts, 0, 0);
    box(attacks, 0, 0);
    box(defences, 0, 0);
    wrefresh(info);
    wrefresh(actions);
    wrefresh(notifications);
    wrefresh(amounts);
    wrefresh(attacks);
    wrefresh(defences);
    
    ask_for_id(player, q_ready);
    //send_ready_message(q_ready);
    char choices2[CHOICES][30] = {"Train Worker", "Train Light Infantry", "Train Heavy Infantry", "Train Cavalry", "A", "B", "Quit Game"};
    if(player->type == 1) {
        strcpy(choices2[4], "Attack Player 2");
        strcpy(choices2[5], "Attack Player 3");
        opponent1 = 2;
        opponent2 = 3;
    }
    if(player->type == 2){
        strcpy(choices2[4], "Attack Player 1");
        strcpy(choices2[5], "Attack Player 3");
        opponent1 = 1;
        opponent2 = 3;
    }
    if(player->type == 3) {
        strcpy(choices2[4], "Attack Player 1");
        strcpy(choices2[5], "Attack Player 2");
        opponent1 = 1;
        opponent2 = 2;
    }
    
    if((f = fork()) == -1) {
        perror("Error in initial forking : client");
    }
    
    if(f == 0) {
        //child client
        /*if((g == fork()) == -1) {
            perror("Error in second forking : client");
        }
        */
        //if(g == 0) {
            //LIVE INFO UPDATER
            while(1) {
                wclear(notifications);
                box(notifications, 0, 0);
                mvwprintw(notifications, 1, 1, "Press Train then units to be created [0-9]");
                mvwprintw(notifications, 2, 1, "Press Attack then sequence of units e.g 253");
                wrefresh(notifications);
                fetch_data_from_server(q_player_info, player);
                update_info(info, (*player));
                listen_notifications(q_notifications, amounts, player->type);
                listen_defence_message(q_attack_message, defences, player->type);
                listen_attack_message(q_attack_message, attacks, player->type);
                usleep(REFRESH_RATE);
            }
        //}
        /*else {
            //LIVE NOTIFICATIONS UPDATER
            while(1) {
                listen_notifications(q_notifications, attacks, player->type);
                usleep(REFRESH_RATE);
            }
        }*/
        
        
        
    }
    else {
        //mother client
        //USER INPUT LISTENER
        
        int choice;
        int highlight = 0;
        
        
        while(1) {
            usleep(7500);
            box(actions, 0, 0);
            redrawwin(actions);
            int i;
            for(i = 0; i < CHOICES; i++) {
                if(i == highlight)
                    wattron(actions, A_REVERSE);
                mvwprintw(actions, i + 1, 1, choices2[i]);
                wattroff(actions, A_REVERSE);
            }
            choice = wgetch(actions);
            switch(choice) {
                case KEY_UP:
                    highlight--;
                    if(highlight == -1)
                        highlight = 0;
                    break;
                case KEY_DOWN:
                    highlight++;
                    if(highlight == 7)
                        highlight = 6;
                    break;
                default:
                    break;
            }
            if(choice == 10) {
                //char buf[10];
                //sprintf(buf, "high:%d %s", highlight, choices2[highlight]);
                //mvwprintw(notifications, 5, 1, buf);
                //wrefresh(notifications);
                wclear(amounts);
                box(amounts, 0, 0);
                wrefresh(amounts);
                //TRAIN UNITS
                if(highlight >= 0 && highlight <= 3) {
                    int order_unit_count;
                    order_unit_count = get_unit_amount(amounts, notifications);
                    send_production_order(q_production, highlight, order_unit_count, player->type);
                }
                //ATTACK A,B
                if(highlight == 4 || highlight == 5) {
                    //opponent 1 or 2
                    int light_infantry_units = 0;
                    int heavy_infantry_units = 0;
                    int cavalry_units = 0;
                    light_infantry_units = get_unit_amount(amounts, notifications);
                    heavy_infantry_units = get_unit_amount(amounts, notifications);
                    cavalry_units = get_unit_amount(amounts, notifications);
                    if(highlight == 4)
                        send_attack_order(q_attack_order, opponent1, light_infantry_units, heavy_infantry_units, cavalry_units, player->type);
                    if(highlight == 5)
                        send_attack_order(q_attack_order, opponent2, light_infantry_units, heavy_infantry_units, cavalry_units, player->type);
                }
                
                //QUIT GAME
                if(highlight == 6) {
                    send_terminate_message(player);
                    free(player);
                    delwin(info);
                    delwin(actions);
                    delwin(notifications);
                    delwin(amounts);
                    endwin();
                    return 0;
                }
            
            }
        }
    }
    
    wait(NULL);
    getch();
    delwin(info);
    delwin(actions);
    delwin(notifications);
    endwin();
    free(player);
    return 0;
}

