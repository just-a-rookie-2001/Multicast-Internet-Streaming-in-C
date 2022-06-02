// import required libraries
#include <net/if.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>

// declare static variables
#define BUF_SIZE 4096
#define STRUCT_CHAR_MAX_SIZE 256
#define MULTI_SERVER_IP "214.102.2.103"
#define SERVER_PORT 5432

// required data structures
typedef struct station_info_t
{
    uint8_t station_number;
    uint8_t station_name_size;
    char station_name[STRUCT_CHAR_MAX_SIZE];
    uint32_t multicast_address;
    uint16_t data_port;
    uint16_t info_port;
    uint32_t bit_rate;
} station_info;

typedef struct site_info_t
{
    uint8_t type;
    uint8_t site_name_size;
    char site_name[STRUCT_CHAR_MAX_SIZE];
    uint8_t site_desc_size;
    char site_desc[STRUCT_CHAR_MAX_SIZE];
    uint8_t station_count;
    station_info station_list[STRUCT_CHAR_MAX_SIZE];
} site_info;

typedef struct song_info_t
{
    uint8_t type;
    uint8_t song_name_size;
    char song_name[STRUCT_CHAR_MAX_SIZE];
    uint16_t remaining_time_in_sec;
    uint8_t next_song_name_size;
    char next_song_name[STRUCT_CHAR_MAX_SIZE];
} song_info;

// variables for 5 stations
station_info stations[5];
pthread_t recvSongsPID;

// global variables
int TotalNStations, curVLCPid = 0, stationNow = 0, count = 0, argC, forceClose = 0;
char cur_status = 'r';

// recieve the initial metadata of the radio stations i.e., port, IP, name, etc using TCP
void StationListReceive(char *argv[])
{
    struct sockaddr_in s_addr;
    int tcp_socket;

    if ((tcp_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("receiver: socket");
        exit(1);
    }

    bzero((char *)&s_addr, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = inet_addr(argv[1]);
    s_addr.sin_port = htons(SERVER_PORT);

    while (connect(tcp_socket, (struct sockaddr *)&s_addr, sizeof(s_addr)) < 0)
        ;

    uint32_t NStations = 0;
    read(tcp_socket, &NStations, sizeof(uint32_t));
    TotalNStations = ntohl(NStations);

    printf("No. of Stations : %d\n", TotalNStations);

    station_info *si = malloc(sizeof(station_info));

    for (int i = 0; i < TotalNStations; i++)
    {
        read(tcp_socket, si, sizeof(station_info));
        memcpy(&stations[i], si, sizeof(station_info));

        printf("\n---- STATION INFO %d ------\n", i + 1);
        printf("Station No. %hu\n", si->station_number);
        printf("Station multicast Port : %d\n", ntohs(si->data_port));
        printf("Station Name : %s\n", si->station_name);
    }

    close(tcp_socket);
}

// recieve songs from the server using UDP
void ReceiveSongs(void *args)
{
    printf("\nReceiving Songs \n");
    char **argv = (char **)args, *if_name = "wlan0", buf[BUF_SIZE];
    int udp_socket, len, nulticast_port;

    socklen_t multicast_saddr_len;
    struct ifreq ifr;
    struct sockaddr_in sin;
    struct ip_mreq multicast_req;
    struct sockaddr_in multicast_saddr;

    if ((udp_socket = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("receiver: socket");
        exit(1);
    }

    if (stationNow > TotalNStations)
    {
        printf("Such station does not exist\n");
        nulticast_port = 8200;
        stationNow = 0;
    }
    else
        nulticast_port = ntohs(stations[stationNow].data_port);

    memset((char *)&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(nulticast_port);

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, sizeof(if_name) - 1);

    if ((setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, (void *)&ifr, sizeof(ifr))) < 0)
    {
        perror("receiver: setsockopt() error");
        close(udp_socket);
        exit(1);
    }

    if ((bind(udp_socket, (struct sockaddr *)&sin, sizeof(sin))) < 0)
    {
        perror("receiver: bind()");
        exit(1);
    }

    multicast_req.imr_multiaddr.s_addr = inet_addr(MULTI_SERVER_IP);
    multicast_req.imr_interface.s_addr = htonl(INADDR_ANY);

    if ((setsockopt(udp_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&multicast_req, sizeof(multicast_req))) < 0)
    {
        perror("mcast join receive: setsockopt()");
        exit(1);
    }

    FILE *fp;
    int counter = 0;
    count = 0;
    song_info *songInfo = (song_info *)malloc(sizeof(song_info));
    char *tempSong = "tempSong.mp3";

    printf("\n Now streaming the song!\n\n");

    while (1)
    {
        memset(&multicast_saddr, 0, sizeof(multicast_saddr));
        multicast_saddr_len = sizeof(multicast_saddr);

        memset(buf, 0, sizeof(buf));
        if ((len = recvfrom(udp_socket, buf, BUF_SIZE, 0, (struct sockaddr *)&multicast_saddr, &multicast_saddr_len)) < 0)
        {
            perror("receiver: recvfrom()");
            exit(1);
        }

        uint8_t tempType;
        if (len == sizeof(song_info))
        {
            printf("Len = %d. Checking for metadata\n", len);
            memcpy(songInfo, buf, len);

            tempType = (uint8_t)songInfo->type;

            if (tempType == 12)
            {
                printf("Next Song : %s\n", songInfo->next_song_name);
                printf("Current Song : %s\n", songInfo->song_name);
                continue;
            }
            else
                printf("Not here!\n");
        }

        if (counter++ == 10)
        {
            curVLCPid = fork();
            if (curVLCPid == 0)
                execlp("/usr/bin/cvlc", "cvlc", tempSong, (char *)NULL);
        }

        fp = fopen(tempSong, "ab+");
        fwrite(buf, len, 1, fp);
        fclose(fp);

        if (forceClose == 1)
            break;
    }

    close(udp_socket);
    forceClose = 0;
    fp = fopen(tempSong, "wb");
    fclose(fp);
}

// start the radio in a different thread resulting in a non-blocking flow
void runRadio(char *argv[])
{
    pthread_create(&recvSongsPID, NULL, &ReceiveSongs, argv);
}

// function to remove the temporary files
void cleanFiles()
{
    system("rm tempSong*");
}

// function to kill the vlc process running in the background
void *closeFunction(void *args)
{
    char cmd[256];
    sprintf(cmd, "kill %d", curVLCPid);
    system(cmd);
    return NULL;
}

// if user clicks the play button
void clicked_play_button(GtkWidget *widget, gpointer data, char *argv[])
{
    printf("Running\n");
    runRadio(argv);
    cur_status = 'r';
}

// if user clicks the pasue button
void clicked_pause_button(GtkWidget *widget, gpointer data)
{
    printf("\n\nPausing\n");
    closeFunction(NULL);
    forceClose = 1;
    cur_status = 'p';
}

// if user clicks the stop button
void clicked_stop_button(GtkWidget *widget, gpointer data)
{
    g_print("\n\nExiting! \n\n");
    closeFunction(NULL);
    forceClose = 1;
    cleanFiles();
    exit(0);
}

// if user chooses the station #1
void clicked_station_1_station(GtkWidget *widget, gpointer data, char *argv[])
{
    g_print("Station 1 tuning!!\n");
    closeFunction(NULL);
    forceClose = 1;
    cleanFiles();
    stationNow = 0;
    cur_status = 'c';
}

// if user chooses the station #2
void clicked_station_2_button(GtkWidget *widget, gpointer data, char *argv[])
{
    g_print("Station 2 tuning!!\n");
    closeFunction(NULL);
    forceClose = 1;
    cleanFiles();
    stationNow = 1;
    cur_status = 'c';
}

// if user chooses the station #3
void clicked_station_3_button(GtkWidget *widget, gpointer data, char *argv[])
{
    g_print("Station 3 tuning!!\n");
    closeFunction(NULL);
    forceClose = 1;
    cleanFiles();
    stationNow = 2;
    cur_status = 'c';
}

// if user chooses the station #4
void clicked_station_4_button(GtkWidget *widget, gpointer data, char *argv[])
{
    g_print("Station 4 tuning!!\n");
    closeFunction(NULL);
    forceClose = 1;
    cleanFiles();
    stationNow = 3;
    cur_status = 'c';
}

// if user chooses the station #5
void clicked_station_5_button(GtkWidget *widget, gpointer data, char *argv[])
{
    g_print("Station 5 tuning!!\n");
    closeFunction(NULL);
    forceClose = 1;
    cleanFiles();
    stationNow = 4;
    cur_status = 'c';
}

// a new window open with list of all stations a user can choose
void clicked_change_station_button(char *argv[])
{
    GtkWidget *window;
    GtkWidget *halign;
    GtkWidget *vbox;
    GtkWidget *btn;
    GtkWidget *btn1;
    GtkWidget *btn2;
    GtkWidget *btn3;
    GtkWidget *btn4;
    GtkWidget *frame;
    GtkWidget *label;

    int argc = 1;
    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Select a Station");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);
    gtk_container_set_border_width(GTK_CONTAINER(window), 15);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    vbox = gtk_vbox_new(TRUE, 25);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    btn = gtk_button_new_with_label("91.1 - Party English");
    btn1 = gtk_button_new_with_label("93.5 - Romantic");
    btn2 = gtk_button_new_with_label("95.0 - Retro");
    btn3 = gtk_button_new_with_label("96.7 - Spiritual");
    btn4 = gtk_button_new_with_label("98.3 - Party Hindi");

    gtk_widget_set_size_request(btn, 70, 30);
    gtk_widget_set_size_request(btn1, 70, 30);
    gtk_widget_set_size_request(btn2, 70, 30);
    gtk_widget_set_size_request(btn3, 70, 30);
    gtk_widget_set_size_request(btn4, 70, 30);

    gtk_box_pack_start(GTK_BOX(vbox), btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn1, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn2, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn3, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn4, TRUE, TRUE, 0);

    g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(clicked_station_1_station), argv);
    g_signal_connect(G_OBJECT(btn1), "clicked", G_CALLBACK(clicked_station_2_button), argv);
    g_signal_connect(G_OBJECT(btn2), "clicked", G_CALLBACK(clicked_station_3_button), argv);
    g_signal_connect(G_OBJECT(btn3), "clicked", G_CALLBACK(clicked_station_4_button), argv);
    g_signal_connect(G_OBJECT(btn4), "clicked", G_CALLBACK(clicked_station_5_button), argv);

    g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), G_OBJECT(window));

    gtk_widget_show_all(window);
    gtk_main();
}

// MAIN FUNCTION
int main(int argc, char *argv[])
{
    argC = argc;

    StationListReceive(argv);

    GtkWidget *window;
    GtkWidget *halign;
    GtkWidget *vbox;
    GtkWidget *btn;
    GtkWidget *btn1;
    GtkWidget *btn2;
    GtkWidget *btn3;

    gtk_init(&argc, &argv);

    printf("\nIn Main Function\n");

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "INTERNET RADIO");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);
    gtk_container_set_border_width(GTK_CONTAINER(window), 15);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    vbox = gtk_vbox_new(TRUE, 25);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    btn = gtk_button_new_with_label("Play radio");
    btn1 = gtk_button_new_with_label("Pause");
    btn2 = gtk_button_new_with_label("Change station");
    btn3 = gtk_button_new_with_label("Stop radio");

    gtk_widget_set_size_request(btn, 70, 30);
    gtk_widget_set_size_request(btn1, 70, 30);
    gtk_widget_set_size_request(btn2, 70, 30);
    gtk_widget_set_size_request(btn3, 70, 30);

    gtk_box_pack_start(GTK_BOX(vbox), btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn1, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn2, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn3, TRUE, TRUE, 0);

    g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(clicked_play_button), argv);
    g_signal_connect(G_OBJECT(btn1), "clicked", G_CALLBACK(clicked_pause_button), NULL);
    g_signal_connect(G_OBJECT(btn2), "clicked", G_CALLBACK(clicked_change_station_button), argv);
    g_signal_connect(G_OBJECT(btn3), "clicked", G_CALLBACK(clicked_stop_button), NULL);

    g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), G_OBJECT(window));

    gtk_widget_show_all(window);

    gtk_main();

    return 0;
}