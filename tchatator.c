#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

struct param
{
    char *name;
    char *value;
};

char *trim_newline(const char *str)
{
    int len = strlen(str);
    char *trimmed_str = (char *)malloc(len + 1);
    if (!trimmed_str)
    {
        perror("malloc failed");
        exit(1);
    }
    strcpy(trimmed_str, str);
    while (len > 0 && (trimmed_str[len - 1] == '\n' || trimmed_str[len - 1] == '\r' || isspace(trimmed_str[len - 1])))
    {
        trimmed_str[len - 1] = '\0';
        len--;
    }
    return trimmed_str;
}

int is_number(const char *str)
{
    while (*str)
    {
        if (!isdigit(*str))
            return 0;
        str++;
    }
    return 1;
}

void read_param_file(struct param *params)
{
    FILE *file = fopen(".tchatator", "r");

    if (!file)
    {
        perror("fopen failed");
        exit(1);
    }

    char line[1024];
    int index = 0;

    while (fgets(line, sizeof(line), file) && index < 100)
    {
        char *name = strtok(line, "=");
        char *value = strtok(NULL, "#");
        if (value)
        {
            char *comment = strchr(value, '#');
            if (comment)
            {
            *comment = '\0';
            }
        }

        if (name && value)
        {
            params[index].name = trim_newline(name);
            params[index].value = trim_newline(value);
            index++;
        }
    }

    fclose(file);
}

char *get_param(struct param *params, const char *name)
{
    for (int i = 0; i < 100; i++)
    {
        if (params[i].name && strcmp(params[i].name, name) == 0)
        {
            return params[i].value;
        }
    }
    return NULL;
}

int main()
{
    struct param params[100];
    read_param_file(params);
    int sock;
    int ret;
    struct sockaddr_in addr;
    int size;
    int cnx;
    struct sockaddr_in conn_addr;
    int port = atoi(get_param(params, "port"));

    // Création de la socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("Socket creation failed");
        return 1;
    }
    printf("Socket: %d\n", sock);

    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_family = AF_INET;

    // Cette boucle permet de trouver un port libre
    while (1)
    {
        addr.sin_port = htons(port);
        ret = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
        if (ret == -1)
        {
            if (errno == EADDRINUSE)
            {
                printf("Port %d is already in use, trying port %d\n", port, port + 1);
                if (port > 8089)
                {
                    perror("No available ports");
                    close(sock);
                    return 1;
                }
                port++;
                continue;
            }
            else
            {
                perror("Bind failed");
                close(sock);
                return 1;
            }
        }
        break;
    }
    printf("Bind: %d on port %d\n", ret, port);

    // Mise en écoute de la socket
    ret = listen(sock, 10);
    if (ret == -1)
    {
        perror("Listen failed");
        close(sock);
        return 1;
    }
    printf("Listen: %d\n", ret);

    size = sizeof(conn_addr);
    cnx = accept(sock, (struct sockaddr *)&conn_addr, (socklen_t *)&size);
    if (cnx == -1)
    {
        perror("Accept failed");
        close(sock);
        return 1;
    }
    printf("Accept: %d\n", cnx);

    char buffer[1024];
    int len = read(cnx, buffer, sizeof(buffer) - 1);
    buffer[len] = '\0';
    int pong_count = 0;

    while (len > 0)
    {
        char *trimmed_buffer = trim_newline(buffer);
        printf("Read: %s\n", trimmed_buffer);
        if (strcmp(trimmed_buffer, "PING") == 0)
        {
            pong_count++;
            snprintf(buffer, sizeof(buffer), "PONG n°%d\r\n", pong_count);
            len = strlen(buffer);
        }
        else if (strcmp(trimmed_buffer, "HELLO") == 0)
        {
            strcpy(buffer, "COUCOU LES GENS\r\n");
            len = strlen(buffer);
        }
        else if (strcmp(trimmed_buffer, "/deconnexion") == 0)
        {
            printf("Closing connection\n");
            free(trimmed_buffer);
            break;
        }
        else
        {
            char *star_pos = strchr(trimmed_buffer, '*');
            if (star_pos != NULL)
            {
                *star_pos = '\0';
                char *part1 = trimmed_buffer;
                char *part2 = star_pos + 1;

                if (is_number(part1) && is_number(part2))
                {
                    int num1 = atoi(part1);
                    int num2 = atoi(part2);
                    int result = num1 * num2;
                    snprintf(buffer, sizeof(buffer), "%s*%s=%d\r\n", part1, part2, result);
                    len = strlen(buffer);
                }
            }
        }
        write(cnx, buffer, len);
        printf("Write: %s\n", buffer);
        free(trimmed_buffer);
        len = read(cnx, buffer, sizeof(buffer) - 1);
        buffer[len] = '\0';
    }

    close(cnx);
    close(sock);

    return 0;
}