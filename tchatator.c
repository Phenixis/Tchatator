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

char send_answer(int cnx, struct param *params, char *code)
{
    char *value = get_param(params, code);
    char message[1024] = "";
    if (value)
    {
        strcat(strcat(strcat(strcat(message, code), "/"), value), "\n\0");
        write(cnx, message, strlen(message));
        printf("> : %s", message);
        return 1;
    } else {
        return send_answer(cnx, params, "500");
    }
    return 0;
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
                printf("Port %d is already in use, trying port %d\n\n", port, port + 1);
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
    ret = listen(sock, atoi(get_param(params, "max_pending_connections")));
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

    while (len > 0)
    {
        char *trimmed_buffer = trim_newline(buffer);
        printf("< : %s\n", trimmed_buffer);
        if (strncmp(trimmed_buffer, "/deconnexion", 12) == 0)
        {
            if (strcmp(trimmed_buffer, "/deconnexion -h") == 0 || strcmp(trimmed_buffer, "/deconnexion --help") == 0)
            {
                write(cnx, "Usage: /deconnexion\nDéconnecte le client et ferme la connexion\n", 45);
                send_answer(cnx, params, "200");
            }
            else
            {
                send_answer(cnx, params, "200");
                break;
            }
        }
        else if (strncmp(trimmed_buffer, "/connexion ", 10) == 0)
        {
            if (strcmp(trimmed_buffer, "/connexion -h") == 0 || strcmp(trimmed_buffer, "/connexion --help") == 0)
            {
                write(cnx, "Usage: /connexion {API_KEY}\nConnecte au compte du client avec la clé d'API {API_KEY}.\n", 88);
                send_answer(cnx, params, "200");
            }
            else
            {
                send_answer(cnx, params, "501");
            }
        }
        else if (strncmp(trimmed_buffer, "/message ", 9) == 0)
        {
            if (strcmp(trimmed_buffer, "/message -h") == 0 || strcmp(trimmed_buffer, "/message --help") == 0)
            {
            write(cnx, "Usage: /message {id_client} {message}\nEnvoie un message au client spécifié.\n", 79);
            send_answer(cnx, params, "200");
            }
            else
            {
            send_answer(cnx, params, "501");
            }
        }
        else if (strncmp(trimmed_buffer, "/liste", 6) == 0)
        {
            if (strcmp(trimmed_buffer, "/liste -h") == 0 || strcmp(trimmed_buffer, "/liste --help") == 0)
            {
            write(cnx, "Usage: /liste {page=0}\nAffiche la liste des messages non lus.\n", 63);
            send_answer(cnx, params, "200");
            }
            else
            {
            send_answer(cnx, params, "501");
            }
        }
        else if (strncmp(trimmed_buffer, "/conversation ", 14) == 0)
        {
            if (strcmp(trimmed_buffer, "/conversation -h") == 0 || strcmp(trimmed_buffer, "/conversation --help") == 0)
            {
            write(cnx, "Usage: /conversation {id_client} {?page=0}\nAffiche l'historique des messages avec le client spécifié.\n", 105);
            send_answer(cnx, params, "200");
            }
            else
            {
            send_answer(cnx, params, "501");
            }
        }
        else if (strncmp(trimmed_buffer, "/info ", 6) == 0)
        {
            if (strcmp(trimmed_buffer, "/info -h") == 0 || strcmp(trimmed_buffer, "/info --help") == 0)
            {
            write(cnx, "Usage: /info {id_message}\nAffiche les informations du message spécifié.\n", 75);
            send_answer(cnx, params, "200");
            }
            else
            {
            send_answer(cnx, params, "501");
            }
        }
        else if (strncmp(trimmed_buffer, "/modifie ", 9) == 0)
        {
            if (strcmp(trimmed_buffer, "/modifie -h") == 0 || strcmp(trimmed_buffer, "/modifie --help") == 0)
            {
            write(cnx, "Usage: /modifie {id_message} {nouveau_message}\nModifie le message spécifié.\n", 79);
            send_answer(cnx, params, "200");
            }
            else
            {
            send_answer(cnx, params, "501");
            }
        }
        else if (strncmp(trimmed_buffer, "/supprime ", 10) == 0)
        {
            if (strcmp(trimmed_buffer, "/supprime -h") == 0 || strcmp(trimmed_buffer, "/supprime --help") == 0)
            {
            write(cnx, "Usage: /supprime {id_message}\nSupprime le message spécifié.\n", 63);
            send_answer(cnx, params, "200");
            }
            else
            {
            send_answer(cnx, params, "501");
            }
        }
        else if (strncmp(trimmed_buffer, "/bloque ", 8) == 0)
        {
            if (strcmp(trimmed_buffer, "/bloque -h") == 0 || strcmp(trimmed_buffer, "/bloque --help") == 0)
            {
            write(cnx, "Usage: /bloque {id_client}\nBloque le client spécifié.\n", 57);
            send_answer(cnx, params, "200");
            }
            else
            {
            send_answer(cnx, params, "501");
            }
        }
        else if (strncmp(trimmed_buffer, "/ban ", 5) == 0)
        {
            if (strcmp(trimmed_buffer, "/ban -h") == 0 || strcmp(trimmed_buffer, "/ban --help") == 0)
            {
            write(cnx, "Usage: /ban {id_client}\nBannit le client spécifié.\n", 54);
            send_answer(cnx, params, "200");
            }
            else
            {
            send_answer(cnx, params, "501");
            }
        }
        else if (strncmp(trimmed_buffer, "/deban ", 7) == 0)
        {
            if (strcmp(trimmed_buffer, "/deban -h") == 0 || strcmp(trimmed_buffer, "/deban --help") == 0)
            {
            write(cnx, "Usage: /deban {id_client}\nLève le bannissement du client spécifié.\n", 71);
            send_answer(cnx, params, "200");
            }
            else
            {
            send_answer(cnx, params, "501");
            }
        }
        else if (strncmp(trimmed_buffer, "/sync", 5) == 0)
        {
            if (strcmp(trimmed_buffer, "/sync -h") == 0 || strcmp(trimmed_buffer, "/sync --help") == 0)
            {
            write(cnx, "Usage: /sync\nRecharge le fichier de paramétrage.\n", 51);
            send_answer(cnx, params, "200");
            }
            else
            {
            send_answer(cnx, params, "501");
            }
        }
        else if (strncmp(trimmed_buffer, "/logs", 5) == 0)
        {
            if (strcmp(trimmed_buffer, "/logs -h") == 0 || strcmp(trimmed_buffer, "/logs --help") == 0)
            {
            write(cnx, "Usage: /logs {?nb_logs=50}\nAffiche les logs.\n", 46);
            send_answer(cnx, params, "200");
            }
            else
            {
            send_answer(cnx, params, "501");
            }
        }
        else {
            send_answer(cnx, params, "404");
        }
        
        free(trimmed_buffer);
        len = read(cnx, buffer, sizeof(buffer) - 1);
        buffer[len] = '\0';
    }

    close(cnx);
    close(sock);

    return 0;
}