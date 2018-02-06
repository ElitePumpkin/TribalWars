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

struct GeneralMessage {
    long type;
    char text [50];
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

void sem_wait(int sem_id, struct Player * pl) {
    struct sembuf semdwn;
    semdwn.sem_num = 0;
    semdwn.sem_op = -1;
    semdwn.sem_flg = 0;
    if(semop(sem_id, &semdwn, 1) == -1) {
        printf("Player %ld ,", pl->type);
        perror("Error in semaphore wait!\n");
    }
    //printf("Semaphore wait! Player no: %ld\n", pl->type);
}

void sem_signal(int sem_id, struct Player * pl) {
    struct sembuf semup;
    semup.sem_num = 0;
    semup.sem_op = 1;
    semup.sem_flg = 0;
    if(semop(sem_id, &semup, 1) == -1) {
        printf("Player %ld ,", pl->type);
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

int units_available_check (struct Player * player, struct AttackOrder order) {
    if(order.li_count <= player->light_infantry &&
        order.hi_count <= player->heavy_infantry &&
        order.c_count <= player->cavalry) {
            return 1;
        }
    return -1;
    
}

void send_notification(long player_type, int communicate_no, int q_notifications) {
    int msg_size = sizeof(struct GeneralMessage) - sizeof(long);
    struct GeneralMessage no_cash_msg;
    no_cash_msg.type = player_type;
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
    sem_wait(sem_id, player);
    int player_size = sizeof(struct Player) - sizeof(long);
    //printf("SERVER mother player no. %ld : gold: %d income: %d\n", player->type, player->gold, player->income_per_second);
        if(msgsnd(q_player_info, player, player_size, 0) == -1) {
            perror("Error in sending player info : server :");
            printf("player to send: type: %ld gold: %d\n", player->type, player->gold);
        }
    sem_signal(sem_id, player);
}

void add_unit_to_player (struct Player * player, int player_id, int unit_id, int unit_production_time) {
    sleep(unit_production_time);

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
}

void update_gold(struct Player * player) {
    player->gold = player->gold + player->income_per_second;
}

int listen_term_msg (struct Player * player, int termination_count) {
    int q_terminate;
    int terminate_msg_length = sizeof(struct TerminateMessage) - sizeof(long);
    struct TerminateMessage term_msg;
    q_terminate = msgget(MSG_GAME_OVER, IPC_CREAT | 0640);
    if(msgrcv(q_terminate, &term_msg, terminate_msg_length, player->type, 0) == -1) {
        perror("Error in receiving terminate message : server");
    }
    termination_count += 1;
    return termination_count;
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

void process_attack(struct Player * attacker, struct Player * defender, struct AttackOrder order, int q_attack_message) {
    //P
    take_units_from_attacker(attacker, order);
    printf("Units taken from attacker - player %ld\n", attacker->type);
    //V
    sleep(5);
    //P
    if(resolve_fight(attacker, defender, order, q_attack_message) == 1) {
        //won, add one point
        attacker->wins += 1;
        
        if(attacker-> wins > 4)
            //send end game message
            printf("Player %ld wins the game!\n", attacker->type);
            //V
    }
    else {
        //lost attack
    }
    //V
}




void await_unit_order(struct Player * pl, int q_unit_production) {
    printf("Awaiting unit orders connected with player no %ld\n", pl->type);
    struct Unit unit;
    int unit_size = sizeof(struct Unit) - sizeof(long);
    while(1) {
        if(msgrcv(q_unit_production, &unit, unit_size, pl->type, 0) == -1) {
            perror("Error in receiving single unit order : server");
        }
        add_unit_to_player(pl, pl->type, unit.unit_id, unit.production_time);
        
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
            send_notification(pl->type, NO_CASH, q_notifications);
        }
        else {
            schedule_production(pl, prod_order, q_unit_production);
        }
        
    }
}

void await_attack_order(struct Player *player, struct Player *other1, struct Player *other2, int q_attack_order, int q_notifications, int q_attack_message) {
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
        if (units_available_check(player, atc_order) == -1) {
            struct AttackMessage atc_msg;
            atc_msg.type = player->type + 3;
            atc_msg.status = -1;
            printf("Player %ld has no units available to perform this attack, L: %d, H: %d, C: %d\n", player->type, atc_order.li_count, atc_order.hi_count, atc_order.c_count);
            send_attack_message(*player, atc_msg, q_attack_message); //dopracuj...
        }
        else {
            if(atc_order.to == opponent1)
                process_attack(player, other1, atc_order, q_attack_message);
            if(atc_order.to == opponent2)
                process_attack(player, other2, atc_order, q_attack_message);
        }
        
    }
}

void unit_listener(struct Player * pl1, struct Player * pl2, struct Player * pl3, int q_unit_production) {
    int a, b;
    if((a = fork()) < 0) {
        perror("Error in internal forking unit listner no 1");
    }
    if(a == 0) {
        //1st order listener and unit scheduler
        await_unit_order(pl1, q_unit_production);
    }
    else {
        if((b = fork()) < 0) {
            perror("Error in internal forking unit listener no 2");
        }
        if(b == 0) {
            //2nd order listener and unit scheduler
            await_unit_order(pl2, q_unit_production);
        }
        else {
            //3rd order listener and unit scheduler
            await_unit_order(pl3, q_unit_production);
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

void attack_listener(struct Player * pl1, struct Player * pl2, struct Player * pl3, int q_attack_order, int q_notifications, int q_attack_message) {
    int a, b;
    if((a = fork()) < 0) {
        perror("Error in internal forking attack listner no 1");
    }
    if(a == 0) {
        //1st attack listener
        
        await_attack_order(pl1, pl2, pl3, q_attack_order, q_notifications, q_attack_message);
        
    }
    else {
        if((b = fork()) < 0) {
            perror("Error in internal forking attack listener no 2");
        }
        if(b == 0) {
            //2nd attack listener
            await_attack_order(pl2, pl1, pl3, q_attack_order, q_notifications, q_attack_message);
        }
        else {
            //3rd attack listener
            await_attack_order(pl3, pl1, pl2, q_attack_order, q_notifications, q_attack_message);
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
    int pl1ma, pl2ma, pl3ma;
    int pl1sema, pl2sema, pl3sema;
    //może tu wskaźnikiem go
    
    
    
    pl1sema = semget(SEM_PLAYER1, 1, IPC_CREAT | 0640);
    pl2sema = semget(SEM_PLAYER2, 1, IPC_CREAT | 0640);
    pl3sema = semget(SEM_PLAYER3, 1, IPC_CREAT | 0640);
    semctl(pl1sema, 0, SETVAL, 1);
    semctl(pl2sema, 0, SETVAL, 1);
    semctl(pl3sema, 0, SETVAL, 1);
    pl1ma = shmget(MEM_PLAYER1, sizeof(struct Player), IPC_CREAT | 0640);
    pl2ma = shmget(MEM_PLAYER2, sizeof(struct Player), IPC_CREAT | 0640);
    pl3ma = shmget(MEM_PLAYER3, sizeof(struct Player), IPC_CREAT | 0640);
    struct Player * player1;
    struct Player * player2;
    struct Player * player3;
    /*
    struct Player * player1 = malloc(sizeof(*player1));
    struct Player * player2 = malloc(sizeof(*player2));
    struct Player * player3 = malloc(sizeof(*player3));*/
    player1 = shmat(pl1ma, 0, 0);
    player2 = shmat(pl2ma, 0, 0);
    player3 = shmat(pl3ma, 0, 0);
    
    initialize_players(player1, player2, player3);
    q_ready = msgget(MSG_READY, IPC_CREAT | 0640);
    
    await_ready_players(q_ready, player1, player2, player3);
    
    if((f = fork()) < 0) {
        perror("Error in first forking server line 100");
    }
    
    if (f == 0) {
        //server child
        printf("Server up and running : child[resources send]\n");
        q_player_info = msgget(MSG_PLAYER_INFO, IPC_CREAT | 0640);
        
        while(1) {
            sleep(1);
            update_gold(player1);
            send_current_info(player1, q_player_info, pl1sema);
            update_gold(player2);
            send_current_info(player2, q_player_info, pl2sema);
            update_gold(player3);
            send_current_info(player3, q_player_info, pl3sema);
            printf("Player 1 wins: %d\n", player1->wins);
            printf("Player 2 wins: %d\n", player2->wins);
            printf("Player 3 wins: %d\n", player3->wins);
        }
    }
    else {
        if((g = fork()) < 0) {
            perror("Error in second forking server line 100");
        }
        
        if(g == 0) {
            //server train listener and production scheduler
            printf("Server up and running : child[train and unit listener]\n");
            q_production_order = msgget(MSG_PRODUCTION_ORDER, IPC_CREAT | 0640);
            q_unit_production = msgget(MSG_UNIT_PRODUCTION, IPC_CREAT | 0640);
            q_notifications = msgget(MSG_NOTI, IPC_CREAT | 0640);
            /*int production_order_size = sizeof(struct ProductionOrder) - sizeof(long);
            int unit_size = sizeof(struct Unit) - sizeof(long);
            struct Unit unit;
            struct ProductionOrder prod_order;*/
            
            if((un = fork()) < 0) {
                perror("Order and Unit listener split unsuccesful");
            }
            
            if(un == 0) {
                printf("Server up and running : child of child [unit listener]\n");
                unit_listener(player1, player2, player3, q_unit_production);
            }
            else {
                printf("Server up and running : child of child [train listener]\n");
                order_listener(player1, player2, player3, q_production_order, q_unit_production, q_notifications);
            }
            
            /*if((t = fork()) < 0) {
                perror("Error in t forking train listener / unit listener");
            }
            
            if(t == 0) {
                //train listeners
                if((t1 = fork()) < 0) {
                    perror("Error in first t1 forking train listener");
                }
                
                if(t1 == 0) {
                    //train listener no 1
                    production_order_listener(player1, q_unit_production);
                    printf("Production order listener no 1 shut down\n");
                }
                else {
                    if((t2 = fork()) < 0) {
                        perror("Error in second t2 forking train listener");
                    }
                    
                    if(t2 == 0) {
                        //train listener no 2
                        production_order_listener(player2, q_unit_production);
                        printf("Production order listener no 1 shut down\n");
                    }
                    else {
                        //train listener no 3
                        production_order_listener(player3, q_unit_production);
                        printf("Production order listener no 1 shut down\n");
                    }
                }
                printf("All production order listeners were shut down!\n");
            }*/
            
            // while(1) {
                
            //     if(msgrcv(q_production_order, &prod_order, production_order_size, 0, 0) == -1) {
            //         perror("Error in receiving production order : server");
            //     }
                
            //     //critical section!
            //     //tu myslalem zeby zrobic sekwencyjnie z wyborem po player.type, ale
            //     //chyba bedzie to srednio dzialalo w przypadku unit listenera
            //     int has_money = gold_available_check(player, prod_order);
            //     if(has_money != -1) {
            //         schedule_production(player1, prod_order, q_unit_production);
            //     }
            //     else {
            //         send_notification(PLAYER_TYPE_1, NO_CASH, q_notifications);
            //     }
            //     //stop critical section!
                
            // }
            
        }
        
        else {
            
            if((h = fork()) < 0) {
                perror("Error in third forking");
            }
            
            if(h == 0) {
                //server, production scheduler
                // printf("Server up and running : child[production scheduler]\n");
                // q_unit_production = msgget(MSG_UNIT_PRODUCTION, IPC_CREAT | 0640);
                // int unit_size = sizeof(struct Unit) - sizeof(long);
                // struct Unit unit;
                
                //while(1) {
                    q_notifications = msgget(MSG_NOTI, IPC_CREAT | 0640);
                    q_attack_order = msgget(MSG_ATTACK_ORDER, IPC_CREAT | 0640);
                    q_attack_message = msgget(MSG_ATTACK_MSG, IPC_CREAT | 0640);
                    attack_listener(player1, player2, player3, q_attack_order, q_notifications, q_attack_message);
                //}
               
                
                // while(1) {
                //     // if(msgrcv(q_unit_production, &unit, unit_size, 1, 0) == -1) {
                //     //     perror("Error in receiving single unit from production scheduler : server");
                //     // }
                //     //add units to player
                //     // add_unit_to_player(player1, PLAYER_TYPE_1, unit.unit_id, unit.production_time);
                // }
            }
            
            else {
                //server mother, terminate listener
                printf("Server up and running : mother[terminate listener]\n");
                int term_count = 0;
                while(1) {
                    term_count = listen_term_msg(player1, term_count);
                }
                
                
            }
            
            
        }
        
        
        
        /*while(1) {
            struct ProductionOrder order;
            if(msgrcv(q_production_order, &order, sizeof(struct ProductionOrder) - sizeof(long), 0, 0) == -1) {
                perror("Error in receiving production order: server");
                break;
            }
            switch(order.unit_id) {
                case 3:
                    printf("Worker order has arrived! ");
                    printf("SERVER mother player1: gold: %d income: %d\n", player1->gold, player1->income_per_second);
                    player1->gold -= worker[0];
                    sleep(worker[3]);
                    player1->workers += 1;
                    player1->income_per_second += 5;
                    
                    break;
                default:
                    printf("wrong no inserted!\n");
            }
            
            
            
        }*/
        
    }
    wait(NULL);
    
    
    /*free(player1);
    free(player2);
    free(player3);*/
    semctl(pl1sema, 0, IPC_RMID, 1);
    semctl(pl2sema, 0, IPC_RMID, 1);
    semctl(pl3sema, 0, IPC_RMID, 1);
    return 0;
}
//jest niezle ale cos sie niewyswietla printf po forku na poczatku procesu servera