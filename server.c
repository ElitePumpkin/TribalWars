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

#define SEM_PLAYER1 101
#define SEM_PLAYER2 102
#define SEM_PLAYER3 103

#define PLAYER_TYPE_1 1
#define PLAYER_TYPE_2 2
#define PLAYER_TYPE_3 3


#define NO_CASH 1
#define CASH_OK 2

struct GeneralMessage {
    long type;
    char text [50];
    int unit_type;
    int unit_count;
};

struct ReadyMessage {
    long type;
    int player_id;
    int status;
};


struct TerminateMessage {
    long type;
    int player_id;
    int kill_status;
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
    long type; //0 = client->server
    int to;
    int li_count;
    int hi_count;
    int c_count;
};

struct Unit {
    long type;
    int unit_id;
    int cost;
    double attack;
    double defence;
    int production_time;
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

void sem_wait(int sem_id, struct Player pl) {
    struct sembuf semdwn;
    semdwn.sem_num = 0;
    semdwn.sem_op = -1;
    semdwn.sem_flg = 0;
    if(semop(sem_id, &semdwn, 1) == -1) {
        printf("Player %ld ,", pl.type);
        perror("Error in semaphore wait!\n");
    }
    //printf("Semaphore wait! Player no: %ld\n", pl->type);
}

void sem_signal(int sem_id, struct Player pl) {
    struct sembuf semup;
    semup.sem_num = 0;
    semup.sem_op = 1;
    semup.sem_flg = 0;
    if(semop(sem_id, &semup, 1) == -1) {
        printf("Player %ld ,", pl.type);
        perror("Error in semaphore signal!\n");
    }
    //printf("Semaphore signal! Player no: %ld\n", pl->type);
}

void wait_for_player(int q_ready, struct Player * pl, int type) {
    int ready_message_size = sizeof(struct ReadyMessage) - sizeof(long);
    struct ReadyMessage ready_msg;
    if(msgrcv(q_ready, &ready_msg, ready_message_size, 1, 0) == -1) {
        perror("Error in receiving ready message player1 : server");
    }
    ready_msg.type = 2;
    ready_msg.player_id = type;
    ready_msg.status = 1;
    if(msgsnd(q_ready, &ready_msg, ready_message_size, 0) == -1) {
        perror("Error in sending unique ID for player 1 : server");
    }
    pl->status = 1;
}

void await_ready_players (int q_ready, struct Player * pl1, struct Player * pl2, struct Player * pl3) {
    printf("Waiting for first player to join\n");
    wait_for_player(q_ready, pl1, 1);
    printf("Waiting for second player to join game\n");
    wait_for_player(q_ready, pl2, 2);
    printf("Waiting for third player to join game\n");
    wait_for_player(q_ready, pl3, 3);
    printf("All players have joined the game!\n");
    
}

int gold_available_check (struct Player * player, struct ProductionOrder order) {
    //to be continued
    int cost_table [] = {150, 100, 250, 550};
    if(player->gold < order.unit_count * cost_table[order.unit_id]) {
        return -1;
    }
    return 1;
}

int units_available_check (struct Player * player, struct AttackOrder order, int semaphores[]) {
    sem_wait(semaphores[player->type - 1], *player);
    if(order.li_count <= player->light_infantry &&
        order.hi_count <= player->heavy_infantry &&
        order.c_count <= player->cavalry) {
            sem_signal(semaphores[player->type - 1], *player);
            return 1;
        }
    sem_signal(semaphores[player->type - 1], *player);
    return -1;
    
}

void send_notification(long player_type, int communicate_no, int q_notifications, int unit_type, int unit_count) {
    int msg_size = sizeof(struct GeneralMessage) - sizeof(long);
    struct GeneralMessage no_cash_msg;
    no_cash_msg.type = player_type;
    no_cash_msg.unit_type = unit_type;
    no_cash_msg.unit_count = unit_count;
    strcpy(no_cash_msg.text, "Not enough gold !!!");
    if(msgsnd(q_notifications, &no_cash_msg, msg_size, 0) == -1) {
        perror("Error in sending notification, code NO_CASH");
    }
    printf("Message to player %ld was sent. He has no cash\n", player_type);
    
}

void send_attack_message(struct Player player, struct AttackMessage attack_message, int q_attack_message) {
    int attack_message_size = sizeof(struct AttackMessage) - sizeof(long);
    if(msgsnd(q_attack_message, &attack_message, attack_message_size, 0) == -1) {
        perror("Error in sending attack message");
    }
    printf("Attack message to player %ld was sent\n", player.type);
}

void schedule_production(struct Player * player, struct ProductionOrder order, int q_unit_production) {
    int cost_table [] = {150, 100, 250, 550};
    double attack_table [] = {0.0, 1.0, 1.5, 3.5};
    double defence_table [] = {0.0, 1.2, 3.0, 1.2};
    int production_time_table [] = {2, 2, 3, 5};
    int unit_size = sizeof(struct Unit) - sizeof(long);
    
    struct Unit new_unit;
    new_unit.type = player->type;
    new_unit.unit_id = order.unit_id;
    //add code here
    //case switch
    switch(new_unit.unit_id) {
        case 0:
            //worker
            new_unit.cost = cost_table[0];
            new_unit.attack = attack_table[0];
            new_unit.defence = defence_table[0];
            new_unit.production_time = production_time_table[0];
            break;
        case 1:
            //light infantry
            new_unit.cost = cost_table[1];
            new_unit.attack = attack_table[1];
            new_unit.defence = defence_table[1];
            new_unit.production_time = production_time_table[1];
            break;
        case 2:
            //heavy infantry
            new_unit.cost = cost_table[2];
            new_unit.attack = attack_table[2];
            new_unit.defence = defence_table[2];
            new_unit.production_time = production_time_table[2];
            break;
        case 3:
            //cavalry
            new_unit.cost = cost_table[3];
            new_unit.attack = attack_table[3];
            new_unit.defence = defence_table[3];
            new_unit.production_time = production_time_table[3];
            break;
        default:
            break;
    }
    player->gold -= order.unit_count * new_unit.cost;
    
    int k;
    for(k = 0; k < order.unit_count; k++) {
        if(msgsnd(q_unit_production, &new_unit, unit_size, 0) == -1) {
            perror("Error in scheduling production : server");
        }
        printf("Scheduled production for player %ld, unit no %d, gold now: %d\n", order.type, order.unit_id, player->gold);
    }
}

void send_current_info(struct Player * player, int q_player_info, int sem_id) {
    sem_wait(sem_id, *player);
    int player_size = sizeof(struct Player) - sizeof(long);
    printf("SERVER mother player no. %ld : gold: %d income: %d\n", player->type, player->gold, player->income_per_second);
        if(msgsnd(q_player_info, player, player_size, 0) == -1) {
            perror("Error in sending player info : server :");
            printf("player to send: type: %ld gold: %d\n", player->type, player->gold);
        }
    sem_signal(sem_id, *player);
}

void add_unit_to_player (struct Player * player, int player_id, int unit_id, int unit_production_time, int semaphores[]) {
    sleep(unit_production_time);
    sem_wait(semaphores[player->type - 1], *player);
    switch(unit_id) {
        case 0:
            player->workers += 1;
            player->income_per_second += 5;
            break;
        case 1:
            player->light_infantry += 1;
            break;
        case 2:
            player->heavy_infantry += 1;
            break;
        case 3:
            player->cavalry += 1;
            break;
        default:
            break;
    }
    sem_signal(semaphores[player->type - 1], *player);
}

void update_gold(struct Player * player) {
    player->gold = player->gold + player->income_per_second;
}

/*int listen_term_msg (struct Player player) {
    int q_terminate;
    int terminate_msg_length = sizeof(struct TerminateMessage) - sizeof(long);
    struct TerminateMessage term_msg;
    q_terminate = msgget(MSG_GAME_OVER, IPC_CREAT | 0640);
    if(msgrcv(q_terminate, &term_msg, terminate_msg_length, player->type, 0) == -1) {
        perror("Error in receiving terminate message : server");
    }
    termination_count += 1;
    return termination_count;
}*/

int listen_game_over_msg(int q_terminate) {
    int terminate_msg_length = sizeof(struct TerminateMessage) - sizeof(long);
    struct TerminateMessage term_msg;
    if(msgrcv(q_terminate, &term_msg, terminate_msg_length, 0, 0) == -1) {
        perror("Error in receiving terminate message : server");
    }
    
    if(term_msg.kill_status == 1) { //ondemand quit game
        struct TerminateMessage temp;
        int i;
        for(i = 0; i < 3; i++) {
            temp.type = 21 + i;
            temp.kill_status = term_msg.type;
            if(msgsnd(q_terminate, &temp, terminate_msg_length, 0) == -1) {
                perror("Error in sending  1st terminate message to client : server");
            }
            printf("Terminate message was sent with id: %ld\n", temp.type);
        }
        return term_msg.type;
    }
    if(term_msg.kill_status == 0) { //from server
        struct TerminateMessage temp;
        int i;
        for(i = 0; i < 3; i++) {
            temp.type = 31 + i;
            temp.kill_status = term_msg.type;
            if(msgsnd(q_terminate, &temp, terminate_msg_length, 0) == -1) {
                perror("Error in sending  1st terminate message to client : server");
            }
            printf("Terminate message was sent with id: %ld\n", temp.type);
        }
        return term_msg.type + 10; //11 player1 wins, 12 player2 wins etc
    }
    else {
        perror("Received wrong kill status : server");
        return -1;
    }
}

void initialize_players(struct Player * pl1, struct Player * pl2, struct Player * pl3 ) {
    pl1->type = 1;
    pl1->wins = 0;
    strcpy(pl1->name, "Pl1");
    pl1->income_per_second = 50;
    pl1->workers = 0;
    pl1->light_infantry = 0;
    pl1->heavy_infantry = 0;
    pl1->cavalry = 0;
    pl1->gold = 300;
    pl1->status = 0;
    //pl1->won_games = 0;
    
    pl2->type = 2;
    pl2->wins = 0;
    strcpy(pl2->name, "Pl2");
    pl2->income_per_second = 50;
    pl2->workers = 0;
    pl2->light_infantry = 0;
    pl2->heavy_infantry = 0;
    pl2->cavalry = 0;
    pl2->gold = 300;
    pl2->status = 0;
    //pl2->won_games = 0;
    
    pl3->type = 3;
    pl3->wins = 0;
    strcpy(pl3->name, "Pl3");
    pl3->income_per_second = 50;
    pl3->workers = 0;
    pl3->light_infantry = 0;
    pl3->heavy_infantry = 0;
    pl3->cavalry = 0;
    pl3->gold = 300;
    pl3->status = 0;
    //pl3->won_games = 0;
}

double calculate_current_defence_force(struct Player defender) {
    double force = 0;
    force += defender.light_infantry * 1.2;
    force += defender.heavy_infantry * 3;
    force += defender.cavalry * 1.2;
    return force;
}

double calculate_order_force(struct AttackOrder order) {
    double force = 0;
    force += order.li_count * 1;
    force += order.hi_count * 1.5;
    force += order.c_count * 3.5;
    return force;
}

void initialize_report(struct AttackMessage * report, struct Player attacker, struct Player defender, struct AttackOrder order, double attack_force, double defence_force) {
    report->attacker = attacker.type;
    report->defender = defender.type;
    report->attack_force = attack_force;
    report->defence_force = defence_force;
    report->li_attack = order.li_count;
    report->hi_attack = order.hi_count;
    report->c_attack = order.c_count;
    report->li_defence = defender.light_infantry;
    report->hi_defence = defender.heavy_infantry;
    report->c_defence = defender.cavalry;
}

int resolve_fight(struct Player * attacker, struct Player * defender, struct AttackOrder order, int q_attack_message) {
    double attack_force;
    double defence_force;
    double hi_lost, li_lost, c_lost, hi_alive, li_alive, c_alive;
    struct AttackMessage * attacker_report = malloc(sizeof(struct AttackMessage));
    struct AttackMessage * defender_report = malloc(sizeof(struct AttackMessage));
    attack_force = calculate_order_force(order);
    defence_force = calculate_current_defence_force(*defender);
    defender_report->type = defender->type;
    attacker_report->type = attacker->type + 3;
    initialize_report(defender_report, *attacker, *defender, order, attack_force, defence_force);
    initialize_report(attacker_report, *attacker, *defender, order, attack_force, defence_force);
    printf("Attack force player %ld : %f\n", attacker->type, attack_force);
    printf("Defence force player %ld : %f\n", defender->type, defence_force);
    if(attack_force == 0) {
        //wyslalem 0 jednostek bo czemu nie
        printf("Player %ld - > Player %ld. Attacker lost due to 0 units sent\n", attacker->type, defender->type);
        attacker_report->status = 0;
        defender_report->status = 1;
        send_attack_message(*attacker, *attacker_report, q_attack_message);
        send_attack_message(*defender, *defender_report, q_attack_message);
        free(attacker_report);
        free(defender_report);
        return -1;
    }
    if(defence_force == 0) {
        //nie bronil sie
        printf("Player %ld - > Player %ld. Defender lost due to 0 units in HQ\n", attacker->type, defender->type);
        attacker->light_infantry += order.li_count;
        attacker->heavy_infantry += order.hi_count;
        attacker->cavalry += order.c_count;
        attacker_report->status = 1;
        defender_report->status = 0;
        send_attack_message(*attacker, *attacker_report, q_attack_message);
        send_attack_message(*defender, *defender_report, q_attack_message);
        free(attacker_report);
        free(defender_report);
        return 1;
    }
    if(attack_force > defence_force) {
        //atakujacy wygrywa
        defender->light_infantry = 0;
        defender->heavy_infantry = 0;
        defender->cavalry = 0;
        li_alive = (double) order.li_count - ((double) order.li_count * ((double) defence_force / (double) attack_force));
        hi_alive = (double) order.hi_count - ((double) order.hi_count * ((double) defence_force / (double) attack_force));
        c_alive = (double) order.c_count - ((double) order.c_count * ((double) defence_force / (double) attack_force));
        attacker->light_infantry += li_alive;
        attacker->heavy_infantry += hi_alive;
        attacker->cavalry += c_alive;
        printf("Player %ld - > Player %ld. Attacker won! Units alive: L: %f, H: %f, C:%f\n", attacker->type, defender->type, li_alive, hi_alive, c_alive);
        attacker_report->status = 1;
        defender_report->status = 0;
        send_attack_message(*attacker, *attacker_report, q_attack_message);
        send_attack_message(*defender, *defender_report, q_attack_message);
        free(attacker_report);
        free(defender_report);
        //wins
        return 1;
    }
    else {
        //defender wins
        
        li_lost = (double) order.li_count * ((double) attack_force / (double) defence_force);
        hi_lost = (double) order.hi_count * ((double) attack_force / (double) defence_force);
        c_lost = (double) order.c_count * ((double) attack_force / (double) defence_force);
        if(defender->light_infantry - li_lost > 0)
            defender->light_infantry -= li_lost;
        else 
            defender->light_infantry = 0;
        if(defender->heavy_infantry - hi_lost > 0)
            defender->heavy_infantry -= hi_lost;
        else
            defender->heavy_infantry = 0;
        if(defender->cavalry - c_lost > 0)
            defender->cavalry -= c_lost;
        else
            defender->cavalry = 0;
        printf("Player %ld - > Player %ld. Defender won! Units alive: L: %d, H: %d, C:%d\n", attacker->type, defender->type, defender->light_infantry, defender->heavy_infantry, defender->cavalry);
        attacker_report->status = 0;
        defender_report->status = 1;
        send_attack_message(*attacker, *attacker_report, q_attack_message);
        send_attack_message(*defender, *defender_report, q_attack_message);
        free(attacker_report);
        free(defender_report);
        //loses
        return -1;
    }
}

void take_units_from_attacker(struct Player * attacker, struct AttackOrder order) {
    attacker->light_infantry -= order.li_count;
    attacker->heavy_infantry -= order.hi_count;
    attacker->cavalry -= order.c_count;
}

void send_win_message(long winner) {
    int q_terminate;
    q_terminate = msgget(MSG_GAME_OVER, IPC_CREAT | 0640);
    struct TerminateMessage term_msg;
    int terminate_msg_length = sizeof(struct TerminateMessage) - sizeof(long);
    term_msg.type = winner;
    term_msg.kill_status = 0;
    if(msgsnd(q_terminate, &term_msg, terminate_msg_length, 0) == -1) {
        perror("Error in sending winning notification from server to server : server");
    }
    printf("Winning message was sent, player %ld\n", winner);
}

void win_check(struct Player pl1, struct Player pl2, struct Player pl3) {
    if(pl1.wins == 5) {
        send_win_message(pl1.type);
    }
    if(pl2.wins == 5) {
        send_win_message(pl2.type);
    }
    if(pl3.wins == 5) {
        send_win_message(pl3.type);
    }
}

void process_attack(struct Player * attacker, struct Player * defender, struct AttackOrder order, int q_attack_message, int semaphores[]) {
    sem_wait(semaphores[attacker->type - 1], *attacker);
    take_units_from_attacker(attacker, order);
    printf("Units taken from attacker - player %ld\n", attacker->type);
    sem_signal(semaphores[attacker->type - 1], *attacker);
    sleep(5);
    sem_wait(semaphores[defender->type - 1], *defender);
    sem_wait(semaphores[attacker->type - 1], *attacker);
    if(resolve_fight(attacker, defender, order, q_attack_message) == 1) {
        //won, add one point
        attacker->wins += 1;
        printf("Player no. %ld has now %d wins\n", attacker->type, attacker->wins);
        if(attacker->wins > 4)
            //send end game message
            printf("Player %ld wins the game!\n", attacker->type);
            //send_win_message(attacker->type);
            //V
    }
    else {
        //lost attack
    }
    sem_signal(semaphores[attacker->type - 1], *attacker);
    sem_signal(semaphores[defender->type - 1], *defender);
    //V
}




void await_unit_order(struct Player * pl, int q_unit_production, int semaphores[]) {
    printf("Awaiting unit orders connected with player no %ld\n", pl->type);
    struct Unit unit;
    int unit_size = sizeof(struct Unit) - sizeof(long);
    while(1) {
        if(msgrcv(q_unit_production, &unit, unit_size, pl->type, 0) == -1) {
            perror("Error in receiving single unit order : server");
        }
        add_unit_to_player(pl, pl->type, unit.unit_id, unit.production_time, semaphores);
        
    }
}

void await_production_order(struct Player * pl, int q_production_order, int q_unit_production, int q_notifications) {
    printf("Awaiting production orders from player no %ld\n", pl->type);
    struct ProductionOrder prod_order;
    int production_order_size = sizeof(struct ProductionOrder) - sizeof(long);
    while(1) {
        if(msgrcv(q_production_order, &prod_order, production_order_size, pl->type, 0) == -1) {
            perror("Error in receiving production order : server");
        }
        //if has money bla bla
        if(gold_available_check(pl, prod_order) == -1) {
            send_notification(pl->type, NO_CASH, q_notifications, prod_order.unit_id, prod_order.unit_count);
        }
        else {
            send_notification(pl->type + 10, CASH_OK, q_notifications, prod_order.unit_id, prod_order.unit_count);
            schedule_production(pl, prod_order, q_unit_production);
        }
        
    }
}

void await_attack_order(struct Player *player, struct Player *other1, struct Player *other2, int q_attack_order, int q_notifications, int q_attack_message, int semaphores[]) {
    printf("Awaiting attack orders from player no: %ld", player->type);
    int opponent1, opponent2;
    if(player->type == 1) {
        opponent1 = 2;
        opponent2 = 3;
    }
    if(player->type == 2){
        opponent1 = 1;
        opponent2 = 3;
    }
    if(player->type == 3) {
        opponent1 = 1;
        opponent2 = 2;
    }
    struct AttackOrder atc_order;
    int attack_order_size = sizeof(struct AttackOrder) - sizeof(long);
    while(1) {
        if(msgrcv(q_attack_order, &atc_order, attack_order_size, player->type, 0) == -1) {
            perror("Error in receiving attack order");
        }
        printf("New attack order from player %ld -> player %d\n", player->type, atc_order.to);
        
        if (units_available_check(player, atc_order, semaphores) == -1) {
            struct AttackMessage atc_msg;
            atc_msg.type = player->type + 3;
            atc_msg.status = -1;
            printf("Player %ld has no units available to perform this attack, L: %d, H: %d, C: %d\n", player->type, atc_order.li_count, atc_order.hi_count, atc_order.c_count);
            send_attack_message(*player, atc_msg, q_attack_message); //dopracuj...
        }
        
        else {
            if(atc_order.to == opponent1) {
                
                
                process_attack(player, other1, atc_order, q_attack_message, semaphores);
                
                
            }
            if(atc_order.to == opponent2) {
                
                process_attack(player, other2, atc_order, q_attack_message, semaphores);
                
            }
        }
        
        
    }
}

void unit_listener(struct Player * pl1, struct Player * pl2, struct Player * pl3, int q_unit_production, int semaphores[]) {
    int a, b;
    if((a = fork()) < 0) {
        perror("Error in internal forking unit listner no 1");
    }
    if(a == 0) {
        //1st order listener and unit scheduler
        await_unit_order(pl1, q_unit_production, semaphores);
    }
    else {
        if((b = fork()) < 0) {
            perror("Error in internal forking unit listener no 2");
        }
        if(b == 0) {
            //2nd order listener and unit scheduler
            await_unit_order(pl2, q_unit_production, semaphores);
        }
        else {
            //3rd order listener and unit scheduler
            await_unit_order(pl3, q_unit_production, semaphores);
        }
    }
    
    printf("End of unit listner reached\n");
}

void order_listener(struct Player * pl1, struct Player * pl2, struct Player * pl3, int q_production_order, int q_unit_production, int q_notifications) {
    int a, b;
    if((a = fork()) < 0) {
        perror("Error in internal forking order listner no 1");
    }
    if(a == 0) {
        //1st order listener and unit scheduler
        await_production_order(pl1, q_production_order, q_unit_production, q_notifications);
    }
    else {
        if((b = fork()) < 0) {
            perror("Error in internal forking order listener no 2");
        }
        if(b == 0) {
            //2nd order listener and unit scheduler
            await_production_order(pl2, q_production_order, q_unit_production, q_notifications);
        }
        else {
            //3rd order listener and unit scheduler
            await_production_order(pl3, q_production_order, q_unit_production, q_notifications);
        }
    }
    
    printf("End of order listner reached\n");
}

void attack_listener(struct Player * pl1, struct Player * pl2, struct Player * pl3, int q_attack_order, int q_notifications, int q_attack_message, int semaphores[]) {
    int a, b;
    if((a = fork()) < 0) {
        perror("Error in internal forking attack listner no 1");
    }
    if(a == 0) {
        //1st attack listener
        
        await_attack_order(pl1, pl2, pl3, q_attack_order, q_notifications, q_attack_message, semaphores);
        
    }
    else {
        if((b = fork()) < 0) {
            perror("Error in internal forking attack listener no 2");
        }
        if(b == 0) {
            //2nd attack listener
            await_attack_order(pl2, pl1, pl3, q_attack_order, q_notifications, q_attack_message, semaphores);
        }
        else {
            //3rd attack listener
            await_attack_order(pl3, pl1, pl2, q_attack_order, q_notifications, q_attack_message, semaphores);
        }
    }
    
    printf("End of order listner reached\n");
}

int main() {
    int f, g, h, un;
    int q_player_info;
    int q_production_order;
    int q_unit_production;
    int q_ready;
    int q_notifications;
    int q_attack_order;
    int q_attack_message;
    int q_terminate;
    int pl1ma, pl2ma, pl3ma;
    int pl1sema, pl2sema, pl3sema;
    int semaphores[3];
    signal(SIGINT, SIG_IGN);
    
    q_production_order = msgget(MSG_PRODUCTION_ORDER, IPC_CREAT | 0640);
    q_unit_production = msgget(MSG_UNIT_PRODUCTION, IPC_CREAT | 0640);
    q_notifications = msgget(MSG_NOTI, IPC_CREAT | 0640);
    q_attack_order = msgget(MSG_ATTACK_ORDER, IPC_CREAT | 0640);
    q_attack_message = msgget(MSG_ATTACK_MSG, IPC_CREAT | 0640);
    q_player_info = msgget(MSG_PLAYER_INFO, IPC_CREAT | 0640);
    q_ready = msgget(MSG_READY, IPC_CREAT | 0640);
    q_terminate = msgget(MSG_GAME_OVER, IPC_CREAT | 0640);
    msgctl(q_player_info, IPC_RMID, 0);
    msgctl(q_production_order, IPC_RMID, 0);
    msgctl(q_notifications, IPC_RMID, 0);
    msgctl(q_attack_order, IPC_RMID, 0);
    msgctl(q_attack_message, IPC_RMID, 0);
    msgctl(q_ready, IPC_RMID, 0);
    msgctl(q_unit_production, IPC_RMID, 0);
    msgctl(q_terminate, IPC_RMID, 0);
    q_production_order = msgget(MSG_PRODUCTION_ORDER, IPC_CREAT | 0640);
    q_unit_production = msgget(MSG_UNIT_PRODUCTION, IPC_CREAT | 0640);
    q_notifications = msgget(MSG_NOTI, IPC_CREAT | 0640);
    q_attack_order = msgget(MSG_ATTACK_ORDER, IPC_CREAT | 0640);
    q_attack_message = msgget(MSG_ATTACK_MSG, IPC_CREAT | 0640);
    q_player_info = msgget(MSG_PLAYER_INFO, IPC_CREAT | 0640);
    q_ready = msgget(MSG_READY, IPC_CREAT | 0640);
    q_terminate = msgget(MSG_GAME_OVER, IPC_CREAT | 0640);

    pl1sema = semget(SEM_PLAYER1, 1, IPC_CREAT | 0640);
    pl2sema = semget(SEM_PLAYER2, 1, IPC_CREAT | 0640);
    pl3sema = semget(SEM_PLAYER3, 1, IPC_CREAT | 0640);
    semaphores[0] = pl1sema;
    semaphores[1] = pl2sema;
    semaphores[2] = pl3sema;
    semctl(pl1sema, 0, SETVAL, 1);
    semctl(pl2sema, 0, SETVAL, 1);
    semctl(pl3sema, 0, SETVAL, 1);
    pl1ma = shmget(MEM_PLAYER1, sizeof(struct Player), IPC_CREAT | 0640);
    pl2ma = shmget(MEM_PLAYER2, sizeof(struct Player), IPC_CREAT | 0640);
    pl3ma = shmget(MEM_PLAYER3, sizeof(struct Player), IPC_CREAT | 0640);
    struct Player * player1;
    struct Player * player2;
    struct Player * player3;
    player1 = shmat(pl1ma, 0, 0);
    player2 = shmat(pl2ma, 0, 0);
    player3 = shmat(pl3ma, 0, 0);
    int left = 0;
    
    initialize_players(player1, player2, player3);
    
    await_ready_players(q_ready, player1, player2, player3);
    
    if((f = fork()) < 0) {
        perror("Error in first forking server line 100");
    }
    
    if (f == 0) {
        //server child
        printf("Server up and running : child[resources send]\n");
        /*q_player_info = msgget(MSG_PLAYER_INFO, IPC_CREAT | 0640);*/
        
        while(1) {
            sleep(1);
            win_check(*player1, *player2, *player3);
            update_gold(player1);
            send_current_info(player1, q_player_info, semaphores[player1->type - 1]);
            update_gold(player2);
            send_current_info(player2, q_player_info, semaphores[player2->type - 1]);
            update_gold(player3);
            send_current_info(player3, q_player_info, semaphores[player3->type - 1]);
            
        }
    }
    else {
        if((g = fork()) < 0) {
            perror("Error in second forking server line 100");
        }
        
        if(g == 0) {
            printf("Server up and running : child[train and unit listener]\n");
            
            
            if((un = fork()) < 0) {
                perror("Order and Unit listener split unsuccesful");
            }
            
            if(un == 0) {
                printf("Server up and running : child of child [unit listener]\n");
                unit_listener(player1, player2, player3, q_unit_production, semaphores);
            }
            else {
                printf("Server up and running : child of child [train listener]\n");
                order_listener(player1, player2, player3, q_production_order, q_unit_production, q_notifications);
            }
            
            
        }
        
        else {
            
            if((h = fork()) < 0) {
                perror("Error in third forking");
            }
            
            if(h == 0) {
                attack_listener(player1, player2, player3, q_attack_order, q_notifications, q_attack_message, semaphores);
            }
            
            else {
                //server mother, terminate listener
                printf("Server up and running : mother[terminate listener]\n");
                int endstatus = 0;
                endstatus = listen_game_over_msg(q_terminate);
                printf("Received end game message : status %d\n", endstatus);
            }
        }
        
    }
    //wait(NULL);
    int my_pid = getpid();
    signal(SIGQUIT, SIG_IGN);
    kill(-my_pid, SIGQUIT);
    printf("Game ended!");
    sleep(5);
    msgctl(q_player_info, IPC_RMID, 0);
    msgctl(q_production_order, IPC_RMID, 0);
    msgctl(q_notifications, IPC_RMID, 0);
    msgctl(q_attack_order, IPC_RMID, 0);
    msgctl(q_attack_message, IPC_RMID, 0);
    msgctl(q_ready, IPC_RMID, 0);
    msgctl(q_unit_production, IPC_RMID, 0);
    msgctl(q_terminate, IPC_RMID, 0);
    
    
    shmctl(pl1ma, IPC_RMID, 0);
    shmctl(pl2ma, IPC_RMID, 0);
    shmctl(pl3ma, IPC_RMID, 0);
    semctl(pl1sema, 0, IPC_RMID, 1);
    semctl(pl2sema, 0, IPC_RMID, 1);
    semctl(pl3sema, 0, IPC_RMID, 1);
    
    return 0;
}
//jest niezle ale cos sie niewyswietla printf po forku na poczatku procesu servera
//dodaj semafory tam gdzie trzeba // atak zrobiony teraz jeszcze unity?
//dodaj wiadomosci z iloscia wojsk do klienta

