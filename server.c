#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <pthread.h>
#include <fcntl.h>

#define MULTI_SERVER_IP "214.102.2.103"
#define SERVER_PORT 5432
#define BUFSIZE 4096
#define BUFSIZE_SMALL 1024
#define BUFSIZE_MAX 256
#define TOTAL_STATIONS 5

typedef struct ts_station_data
{
    uint8_t station_length;
    uint8_t station_number;
    uint16_t data_port;
    uint16_t info_port;
    uint32_t multicast_address;
    uint32_t bit_rate;
    char station_name[BUFSIZE_MAX];
} station_data;

typedef struct ts_song_data
{
    uint8_t type;
    uint8_t song_length;
    uint8_t next_song_length;
    char song[BUFSIZE_MAX];
    char next_song[BUFSIZE_MAX];
} song_data;

song_data initSongInfo(song_data *si)
{
    si->type = 12;
    return *si;
}

typedef struct ts_station
{
    int id;
    char path[BUFSIZE_SMALL];
    int port;
} station;

station_data stations[TOTAL_STATIONS];
station station_paths_id[TOTAL_STATIONS];
pthread_t station_threads[TOTAL_STATIONS];

long idealSleep;

int arg_c;
char **arg_v;

//set data for all stations
void setStationDetails()
{
    station_data station_data;
    station locationOfStation;

    locationOfStation.port = 8200;
    locationOfStation.id = 1;
    strcpy(locationOfStation.path, "./91.1 - Party English/");

    station_data.station_number = 1;
    station_data.station_length = htonl(strlen("91.1 - Party English"));
    strcpy(station_data.station_name, "91.1 - Party English");
    station_data.data_port = htons(8200);

    memcpy(&stations[0], &station_data, sizeof(station_data));
    memcpy(&station_paths_id[0], &locationOfStation, sizeof(station));

    bzero(&station_data, sizeof(station_data));
    bzero(&locationOfStation, sizeof(station));

    locationOfStation.port = 8201;
    locationOfStation.id = 2;
    strcpy(locationOfStation.path, "./93.5 - Romantic/");

    station_data.station_number = 2;
    station_data.station_length = htonl(strlen("93.5 - Romantic"));
    strcpy(station_data.station_name, "93.5 - Romantic");
    station_data.data_port = htons(8200);

    memcpy(&stations[1], &station_data, sizeof(station_data));
    memcpy(&station_paths_id[1], &locationOfStation, sizeof(station));

    bzero(&station_data, sizeof(station_data));
    bzero(&locationOfStation, sizeof(station));

    locationOfStation.port = 8202;
    locationOfStation.id = 3;
    strcpy(locationOfStation.path, "./95.0 - Retro/");

    station_data.station_number = 3;
    station_data.station_length = htonl(strlen("95.0 - Retro"));
    strcpy(station_data.station_name, "95.0 - Retro");
    station_data.data_port = htons(8202);

    memcpy(&stations[2], &station_data, sizeof(station_data));
    memcpy(&station_paths_id[2], &locationOfStation, sizeof(station));

    bzero(&station_data, sizeof(station_data));
    bzero(&locationOfStation, sizeof(station));

    locationOfStation.port = 8203;
    locationOfStation.id = 4;
    strcpy(locationOfStation.path, "./96.7 - Spiritual/");

    station_data.station_number = 4;
    station_data.station_length = htonl(strlen("96.7 - Spiritual"));
    strcpy(station_data.station_name, "96.7 - Spiritual");
    station_data.data_port = htons(8203);

    memcpy(&stations[3], &station_data, sizeof(station_data));
    memcpy(&station_paths_id[3], &locationOfStation, sizeof(station));

    bzero(&station_data, sizeof(station_data));
    bzero(&locationOfStation, sizeof(station));

    locationOfStation.port = 8204;
    locationOfStation.id = 5;
    strcpy(locationOfStation.path, "./98.3 - Party Hindi/");

    station_data.station_number = 5;
    station_data.station_length = htonl(strlen("98.3 - Party Hindi"));
    strcpy(station_data.station_name, "98.3 - Party Hindi");
    station_data.data_port = htons(8204);

    memcpy(&stations[4], &station_data, sizeof(station_data));
    memcpy(&station_paths_id[4], &locationOfStation, sizeof(station));

    bzero(&station_data, sizeof(station_data));
    bzero(&locationOfStation, sizeof(station));
}

//start server and establish connection
void *startServer(void *arg)
{

    struct sockaddr_in sin;
    int len;
    int socket_2, newSocket;
    char str[INET_ADDRSTRLEN];
    char *server_ip;
    if (arg_c > 1)
        server_ip = arg_v[1];
    bzero((char *)&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr(server_ip);
    sin.sin_port = htons(SERVER_PORT);

    if ((socket_2 = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation failed!!");
        exit(1);
    }

    inet_ntop(AF_INET, &(sin.sin_addr), str, INET_ADDRSTRLEN);
    printf("Server address %s and port %d.\n", str, SERVER_PORT);

    if ((bind(socket_2, (struct sockaddr *)&sin, sizeof(sin))) < 0)
    {
        perror("Failed to Bind");
        exit(1);
    }
    else
        printf("Bind Successfull\n");
    listen(socket_2, 5);

    while (1)
    {
        if ((newSocket = accept(socket_2, (struct sockaddr *)&sin, (unsigned int *)&len)) < 0)
        {
            perror("Failed to Accept");
            exit(1);
        }
        printf("Connection Established between client and server.\n");
        uint32_t numberOfStations = TOTAL_STATIONS;
        numberOfStations = htonl(numberOfStations);
        send(newSocket, &numberOfStations, sizeof(uint32_t), 0);

        for (int z = 0; z < TOTAL_STATIONS; z++)
        {
            send(newSocket, &stations[z], sizeof(station_data), 0);
        }
    }
}

//start station and manage songs
void *startStation(void *arg)
{
    station *locationOfStation = (station *)arg;
    DIR *directory;
    struct dirent *files;
    int cnt = 0;
    printf("Path: %s\n", locationOfStation->path);
    if ((directory = opendir(locationOfStation->path)) != NULL)
    {
        while ((files = readdir(directory)) != NULL)
        {
            if (strstr(files->d_name, ".mp3") != NULL)
                cnt++;
        }
        closedir(directory);
    }
    else
    {
        perror("Unable to read Directory");
        return 0;
    }

    char songs[cnt][BUFSIZE_SMALL];
    char song_name[cnt][BUFSIZE_SMALL];

    FILE *song_files[cnt];

    int n = 0;
    while (n < cnt)
    {
        memset(songs[n], '\0', BUFSIZE_SMALL);
        strcpy(songs[n], locationOfStation->path);
        n++;
    }

    int z = 0;
    if ((directory = opendir(locationOfStation->path)) != NULL)
    {
        while ((files = readdir(directory)) != NULL)
        {
            if (strstr(files->d_name, ".mp3") != NULL)
            {
                memcpy(&(songs[z][strlen(locationOfStation->path)]), files->d_name, strlen(files->d_name) + 1);
                strcpy((song_name[z]), files->d_name);

                song_files[z] = fopen(songs[z], "rb");
                if (song_files[z] == NULL)
                {
                    perror("Empty directory");
                    exit(1);
                }
                z++;
            }
        }
        closedir(directory);
    }

    song_data songInfo[cnt];

    for (int z = 0; z < cnt; z++)
        bzero(&songInfo[z], sizeof(song_data));
    for (int z = 0; z < cnt; z++)
    {
        initSongInfo(&songInfo[z]);
        printf("song info : %hu p = %p\n", (unsigned short)songInfo[z].type, &songInfo[z].type);
    }
    for (int z = 0; z < cnt; z++)
    {
        songInfo[z].song_length = (uint8_t)strlen(song_name[z]) + 1;
        printf("%d", songInfo[z].song_length);
        strcpy((songInfo[z].song), song_name[z]);

        songInfo[z].next_song_length = (uint8_t)strlen(song_name[(z + 1) % cnt]) + 1;
        strcpy((songInfo[z].next_song), song_name[(z + 1) % cnt]);
    }
    for (int z = 0; z < cnt; z++)
    {
        printf("%s\n", songs[z]);
        printf("Song info : %hu p = %p\n", (unsigned short)songInfo[z].type, &songInfo[z].type);
    }

    int socket_2;
    struct sockaddr_in sin;
    int len;
    char buf[BUFSIZE_SMALL];

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 20000000L;

    if ((socket_2 = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Failed to create");
        exit(1);
    }
    memset((char *)&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr(MULTI_SERVER_IP);
    sin.sin_port = htons(locationOfStation->port);
    printf("\nInitializing station ID : %d!\n\n", locationOfStation->id);
    memset(buf, 0, sizeof(buf));

    int curSong = -1;
    while (1)
    {
        curSong = (curSong + 1) % cnt;
        FILE *song = song_files[curSong];
        if (song == NULL)
            printf("Song not found!!\n");
        printf("Curent_Song_number = %d z Song_name= %p\n", curSong, song);
        rewind(song);

        int size = BUFSIZE_SMALL;
        int counter = 0;
        printf("Sending Structure : z Song_number= %d. Song_Info->type = %hu p = %p\n", curSong, (unsigned short)songInfo[curSong].type, &songInfo[curSong].type);

        if ((len = sendto(socket_2, &songInfo[curSong], sizeof(song_data), 0, (struct sockaddr *)&sin, sizeof(sin))) == -1)
        {
            perror("Failed server sending");
            exit(1);
        }
        float bitrate = 128;
        idealSleep = ((BUFSIZE_SMALL * 8) / bitrate) * 500000000;

        if (idealSleep < 0)
            idealSleep = ts.tv_nsec;

        if (ts.tv_nsec > idealSleep)
            ts.tv_nsec = idealSleep;

        while (!(size < BUFSIZE_SMALL))
        {
            size = fread(buf, 1, sizeof(buf), song);

            if ((len = sendto(socket_2, buf, size, 0, (struct sockaddr *)&sin, sizeof(sin))) == -1)
            {
                perror("Server: sendto");
                exit(1);
            }
            if (len != size)
            {
                printf("Error!");
                exit(0);
            }
            nanosleep(&ts, NULL);
            memset(buf, 0, sizeof(buf));
        }
    }
    close(socket_2);
}

int main(int argc, char *argv[])
{
    arg_c = argc;
    arg_v = argv;
    setStationDetails();
    pthread_t tcp;
    pthread_create(&tcp, NULL, startServer, NULL);
    for (int z = 0; z < TOTAL_STATIONS; z++)
    {
        pthread_create(&station_threads[z], NULL, startStation, &station_paths_id[z]);
    }
    pthread_join(tcp, NULL);
    return 0;
}