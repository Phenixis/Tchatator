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
#include <libpq-fe.h>
#include <time.h>

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

void read_param_file(struct param *params, char *filename)
{
    FILE *file = fopen(filename, "r");

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

    for (int i = index; i < 100; i++)
    {
        params[i].name = NULL;
        params[i].value = NULL;
    }

    fclose(file);
}

char *get_param(struct param *params, const char *name)
{
    for (int i = 0; i < 100; i++)
    {
        if (params[i].name == NULL || params[i].value == NULL)
        {
            break;
        }
        else if (strcmp(params[i].name, name) == 0)
        {
            return params[i].value;
        }
    }
    printf("Paramètre [%s] introuvable dans le fichier de paramétrage\n", name);
    exit(1);
}

int logs(char *message, char *clientID, char *clientIP, int verbose)
{
    time_t now;
    time(&now);
    struct tm *local = localtime(&now);
    char time_str[100];
    strftime(time_str, sizeof(time_str), "%d/%m/%Y - %H:%M:%S", local);
    if (verbose == 1)
    {
        printf("[date+heure:%s] [id_client:%s] [ip_client:%s] : %s\n", time_str, clientID, clientIP, message);
    }
    FILE *file = fopen("tchatator.log", "a");
    if (!file)
    {
        perror("fopen failed");
        return 1;
    }
    fprintf(file, "[date+heure:%s] [id_client:%s] [ip_client:%s] : %s\n", time_str, clientID, clientIP, message);
    fclose(file);
    return 0;
}

char send_answer(int cnx, struct param *params, char *code, char *clientID, char *clientIP, int verbose)
{
    char *value = get_param(params, code);
    char message[1024] = "";
    if (value)
    {
        strcat(strcat(strcat(message, code), "/"), value);

        // On log le message avant d'y ajouter le caractère de fin de ligne
        char *to_log = malloc(strlen(message) + 100);
        sprintf(to_log, "Réponse envoyée : %s", message, verbose);
        logs(to_log, clientID, clientIP, verbose);

        strcat(message, "\n");

        write(cnx, message, strlen(message));
        return 1;
    }
    else
    {
        return send_answer(cnx, params, "500", clientID, clientIP, verbose);
    }
    return 0;
}

void exit_on_error(PGconn *conn)
{
    fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(conn));
    PQfinish(conn);
    exit(EXIT_FAILURE);
}

PGconn *get_connection(struct param *params, int verbose)
{
    char *host = get_param(params, "db_host");
    char *port = get_param(params, "db_port");
    char *dbname = get_param(params, "db_name");
    char *user = get_param(params, "db_user");
    char *password = get_param(params, "db_password");
    char conninfo[1024];
    sprintf(conninfo, "host=%s port=%s dbname=%s user=%s password=%s", host, port, dbname, user, password);

    // Connect to the database
    PGconn *conn = PQconnectdb(conninfo);

    // Check if the connection was successful
    if (PQstatus(conn) != CONNECTION_OK)
    {
        exit_on_error(conn);
    }

    logs("Successfully connected to the database!", "", "", verbose);

    return conn;
}

PGresult *execute(PGconn *conn, char *query)
{
    PGresult *res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK && PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "Query failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        exit(EXIT_FAILURE);
    }
    return res;
}

int client_existe(PGconn *conn, char *id_client)
{
    char query[256];
    snprintf(query, sizeof(query), "SELECT * FROM sae_db._compte WHERE id_compte = '%s';", id_client);
    PGresult *res = execute(conn, query);
    int result = PQntuples(res) > 0;
    PQclear(res);
    return result;
}

int client_est_banni(PGconn *conn, char *id_client)
{
    char query[256];
    snprintf(query, sizeof(query), "SELECT * FROM sae_db._bannissement WHERE id_banni = '%s' AND date_debannissement IS NULL;", id_client);
    PGresult *res = execute(conn, query);
    int result = PQntuples(res) > 0;
    PQclear(res);
    return result;
}

int main(int argc, char *argv[])
{
    char *param_file = ".tchatator"; // Fichier de paramètres par défaut
    int verbose = 0;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--fileparam") == 0)
        {
            if (i + 1 < argc)
            {
                param_file = argv[i + 1];
                i++; // Skip the next argument as it is the file name
            }
            else
            {
                printf("Missing argument for option %s\n", argv[i]);
                return 1;
            }
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
        {
            verbose = 1;
        }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            printf("Usage: %s [-f|--fileparam FILE] [-v|--verbose]\n", argv[0]);
            printf("Options:\n");
            printf("  -f, --fileparam FILE  Use FILE as the parameter file (default: .tchatator)\n");
            printf("  -v, --verbose         Print verbose output\n");
            return 0;
        }
        else
        {
            printf("Unknown option: %s\n", argv[i]);
            return 1;
        }
    }
    struct param params[100];
    read_param_file(params, param_file);

    char *to_log = malloc(200);
    sprintf(to_log, "Service lancé avec le fichier de paramétrage '%s' et le mode verbose '%d'", param_file, verbose);
    logs(to_log, "", "", verbose);

    int sock;
    struct sockaddr_in addr;
    int ret;
    char *portName = get_param(params, "socket_port");
    int port = atoi(portName);
    int port_max = port + 10;
    int size;
    int cnx;
    struct sockaddr_in conn_addr;

    // Création de la socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("Création du socket échouée : ");
        return 1;
    }
    sprintf(to_log, "Socket: %d", sock);
    logs(to_log, "", "", verbose);

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

                sprintf(to_log, "Le port %d est déjà utilisé, essaie pour le port %d", port, port + 1);
                logs(to_log, "", "", 1);
                if (port > port_max)
                {
                    perror("Aucun port disponible");
                    close(sock);
                    return 1;
                }
                port++;
                continue;
            }
            else
            {
                perror("Erreur lors du bind");
                close(sock);
                return 1;
            }
        }
        break;
    }
    sprintf(to_log, "Socket bind sur le port %d", port);
    logs(to_log, "", "", 1);

    // Mise en écoute de la socket
    char *max_pending_connections = get_param(params, "max_pending_connections");
    ret = listen(sock, atoi(max_pending_connections));
    if (ret == -1)
    {
        perror("Listen failed");
        close(sock);
        return 1;
    }
    sprintf(to_log, "Listen: %d", ret);
    logs(to_log, "", "", verbose);

    size = sizeof(conn_addr);
    cnx = accept(sock, (struct sockaddr *)&conn_addr, (socklen_t *)&size);
    if (cnx == -1)
    {
        perror("Erreur lors de l'acceptation de la connexion");
        close(sock);
        return 1;
    }
    sprintf(to_log, "Accept: %d", cnx);
    logs(to_log, "", "", verbose);

    char *client_ip = inet_ntoa(conn_addr.sin_addr);
    PGconn *conn = get_connection(params, verbose);
    char buffer[2048];
    char id_compte_client[1024];
    int len = read(cnx, buffer, sizeof(buffer) - 1);
    buffer[len] = '\0';

    while (len > 0)
    {
        char *trimmed_buffer = trim_newline(buffer);

        // Log the message
        char *to_log = malloc(strlen(trimmed_buffer) + 100);
        sprintf(to_log, "Message reçu : %s", trimmed_buffer);
        logs(to_log, id_compte_client, client_ip, verbose);

        if (strncmp(trimmed_buffer, "/deconnexion", 12) == 0)
        {
            if (strcmp(trimmed_buffer, "/deconnexion -h") == 0 || strcmp(trimmed_buffer, "/deconnexion --help") == 0)
            {
                logs("Commande d'aide /deconnexion", id_compte_client, client_ip, verbose);
                write(cnx, "Usage: /deconnexion\nDéconnecte le client et ferme la connexion\n", 45);
                send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
            }
            else
            {
                logs("Commande /deconnexion", id_compte_client, client_ip, verbose);
                strcpy(id_compte_client, "");
                send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                break;
            }
        }
        else if (strncmp(trimmed_buffer, "/connexion ", 10) == 0)
        {
            if (strcmp(trimmed_buffer, "/connexion -h") == 0 || strcmp(trimmed_buffer, "/connexion --help") == 0)
            {
                write(cnx, "Usage: /connexion {API_KEY}\nConnecte au compte du client avec la clé d'API {API_KEY}.\n", 88);
                send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
            }
            else if (strncmp(trimmed_buffer, "/connexion tchatator_", 21) == 0)
            {
                char *api_key = trimmed_buffer + 11;
                char query[256];
                snprintf(query, sizeof(query), "SELECT * FROM sae_db._compte WHERE api_key = '%s';", api_key);

                PGresult *res = execute(conn, query);

                if (PQntuples(res) > 0)
                {
                    strcpy(id_compte_client, PQgetvalue(res, 0, 0));
                    char update_query[256];
                    snprintf(update_query, sizeof(update_query), "UPDATE sae_db._compte SET derniere_connexion = NOW() WHERE id = '%s';", id_compte_client);
                    execute(conn, update_query);
                    send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                }
                else
                {
                    char *admin_api_key = get_param(params, "admin_api_key");
                    if (strcmp(api_key, admin_api_key) == 0)
                    {
                        strcpy(id_compte_client, "admin");
                        send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                    }
                    else
                    {
                        send_answer(cnx, params, "401", id_compte_client, client_ip, verbose);
                    }
                }

                PQclear(res);
            }
            else
            {
                send_answer(cnx, params, "404", id_compte_client, client_ip, verbose);
            }
        }
        else if (strncmp(trimmed_buffer, "/message ", 9) == 0)
        {
            if (strcmp(trimmed_buffer, "/message -h") == 0 || strcmp(trimmed_buffer, "/message --help") == 0)
            {
                write(cnx, "Usage: /message {id_client} {message}\nEnvoie un message au client spécifié.\n", 79);
                send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
            }
            else
            {
                send_answer(cnx, params, "501", id_compte_client, client_ip, verbose);
            }
        }
        else if (strncmp(trimmed_buffer, "/liste", 6) == 0)
        {
            if (strcmp(trimmed_buffer, "/liste -h") == 0 || strcmp(trimmed_buffer, "/liste --help") == 0)
            {
                write(cnx, "Usage: /liste {page=0}\nAffiche la liste des messages non lus.\n", 63);
                send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
            }
            else
            {
                send_answer(cnx, params, "501", id_compte_client, client_ip, verbose);
            }
        }
        else if (strncmp(trimmed_buffer, "/conversation ", 14) == 0)
        {
            if (strcmp(trimmed_buffer, "/conversation -h") == 0 || strcmp(trimmed_buffer, "/conversation --help") == 0)
            {
                write(cnx, "Usage: /conversation {id_client} {?page=0}\nAffiche l'historique des messages avec le client spécifié.\n", 105);
                send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
            }
            else
            {
                send_answer(cnx, params, "501", id_compte_client, client_ip, verbose);
            }
        }
        else if (strncmp(trimmed_buffer, "/info ", 6) == 0)
        {
            if (strcmp(trimmed_buffer, "/info -h") == 0 || strcmp(trimmed_buffer, "/info --help") == 0)
            {
                write(cnx, "Usage: /info {id_message}\nAffiche les informations du message spécifié.\n", 75);
                send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
            }
            else
            {
                send_answer(cnx, params, "501", id_compte_client, client_ip, verbose);
            }
        }
        else if (strncmp(trimmed_buffer, "/modifie ", 9) == 0)
        {
            if (strcmp(trimmed_buffer, "/modifie -h") == 0 || strcmp(trimmed_buffer, "/modifie --help") == 0)
            {
                write(cnx, "Usage: /modifie {id_message} {nouveau_message}\nModifie le message spécifié.\n", 79);
                send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
            }
            else
            {
                send_answer(cnx, params, "501", id_compte_client, client_ip, verbose);
            }
        }
        else if (strncmp(trimmed_buffer, "/supprime ", 10) == 0)
        {
            if (strcmp(trimmed_buffer, "/supprime -h") == 0 || strcmp(trimmed_buffer, "/supprime --help") == 0)
            {
                write(cnx, "Usage: /supprime {id_message}\nSupprime le message spécifié.\n", 63);
                send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
            }
            else
            {
                send_answer(cnx, params, "501", id_compte_client, client_ip, verbose);
            }
        }
        else if (strncmp(trimmed_buffer, "/bloque ", 8) == 0)
        {
            if (strcmp(trimmed_buffer, "/bloque -h") == 0 || strcmp(trimmed_buffer, "/bloque --help") == 0)
            {
                write(cnx, "Usage: /bloque {id_client}\nBloque le client spécifié.\n", 57);
                send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
            }
            else
            {
                send_answer(cnx, params, "501", id_compte_client, client_ip, verbose);
            }
        }
        else if (strncmp(trimmed_buffer, "/ban ", 5) == 0)
        {
            if (strcmp(trimmed_buffer, "/ban -h") == 0 || strcmp(trimmed_buffer, "/ban --help") == 0)
            {
                write(cnx, "Usage: /ban {id_client}\nBannit le client spécifié.\n", 54);
                send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
            }
            else if (strcmp(id_compte_client, "admin") == 0)
            {
                char *id_client = trimmed_buffer + 5;
                if (client_existe(conn, id_client) == 0)
                {
                    send_answer(cnx, params, "404", id_compte_client, client_ip, verbose);
                    continue;
                }

                if (client_est_banni(conn, id_client) == 1)
                {
                    send_answer(cnx, params, "409", id_compte_client, client_ip, verbose);
                    continue;
                }
                
                char query[256];
                snprintf(query, sizeof(query), "INSERT INTO sae_db._bannissement (id_banni) VALUES ('%s');", id_client);
                execute(conn, query);
                
                send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
            }
            else
            {
                send_answer(cnx, params, "401", id_compte_client, client_ip, verbose);
            }
        }
        else if (strncmp(trimmed_buffer, "/deban ", 7) == 0)
        {
            if (strcmp(trimmed_buffer, "/deban -h") == 0 || strcmp(trimmed_buffer, "/deban --help") == 0)
            {
                write(cnx, "Usage: /deban {id_client}\nLève le bannissement du client spécifié.\n", 71);
                send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
            }
            else if (strcmp(id_compte_client, "admin") == 0)
            {
                char *id_client = trimmed_buffer + 7;
                if (client_existe(conn, id_client) == 0)
                {
                    send_answer(cnx, params, "404", id_compte_client, client_ip, verbose);
                    continue;
                }

                if (client_est_banni(conn, id_client) == 0)
                {
                    send_answer(cnx, params, "409", id_compte_client, client_ip, verbose);
                    continue;
                }

                char query[256];
                snprintf(query, sizeof(query), "UPDATE sae_db._bannissement SET date_debannissement = NOW() WHERE id_banni = '%s' AND date_debannissement IS NULL;", id_client);
                execute(conn, query);

                send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
            }
            else
            {
                send_answer(cnx, params, "401", id_compte_client, client_ip, verbose);
            }
        }
        else if (strncmp(trimmed_buffer, "/sync", 5) == 0)
        {
            if (strcmp(trimmed_buffer, "/sync -h") == 0 || strcmp(trimmed_buffer, "/sync --help") == 0)
            {
                write(cnx, "Usage: /sync\nRecharge le fichier de paramétrage.\n", 51);
                send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
            }
            else
            {
                read_param_file(params, param_file);
                send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
            }
        }
        else if (strncmp(trimmed_buffer, "/logs", 5) == 0)
        {
            if (strcmp(trimmed_buffer, "/logs -h") == 0 || strcmp(trimmed_buffer, "/logs --help") == 0)
            {
                write(cnx, "Usage: /logs {?nb_logs=50}\nAffiche les logs.\n", 46);
                send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
            }
            else if (strcmp(id_compte_client, "admin") == 0)
            {
                char *nb_logs = strtok(trimmed_buffer + 6, " ");
                if (nb_logs == NULL)
                {
                    nb_logs = "50";
                }

                char *log_file = get_param(params, "fichier_logs");
                FILE *file = fopen(log_file, "r");
                if (!file)
                {
                    perror("fopen failed");
                    send_answer(cnx, params, "500", id_compte_client, client_ip, verbose);
                }
                else
                {
                    fseek(file, 0, SEEK_END);
                    long file_size = ftell(file);
                    fseek(file, 0, SEEK_SET);

                    char *file_content = malloc(file_size + 1);
                    fread(file_content, 1, file_size, file);
                    file_content[file_size] = '\0';

                    fclose(file);

                    char *line = strtok(file_content, "\n");
                    int line_count = 0;
                    while (line)
                    {
                        line_count++;
                        line = strtok(NULL, "\n");
                    }

                    free(file_content);

                    file = fopen(log_file, "r");
                    if (!file)
                    {
                        perror("fopen failed");
                        send_answer(cnx, params, "500", id_compte_client, client_ip, verbose);
                    }
                    else
                    {
                        file_content = malloc(file_size + 1);
                        fread(file_content, 1, file_size, file);
                        file_content[file_size] = '\0';

                        fclose(file);

                        line = strtok(file_content, "\n");
                        int start_line = line_count - atoi(nb_logs);
                        if (start_line < 0)
                        {
                            start_line = 0;
                        }

                        line_count = 0;
                        while (line)
                        {
                            if (line_count >= start_line)
                            {
                                write(cnx, line, strlen(line));
                                write(cnx, "\n", 1);
                            }
                            line_count++;
                            line = strtok(NULL, "\n");
                        }

                        free(file_content);
                        send_answer(cnx, params, "200", id_compte_client, client_ip, verbose);
                    }
                }
            }
            else
            {
                send_answer(cnx, params, "401", id_compte_client, client_ip, verbose);
            }
        }
        else
        {
            send_answer(cnx, params, "404", id_compte_client, client_ip, verbose);
        }

        free(trimmed_buffer);
        len = read(cnx, buffer, sizeof(buffer) - 1);
        buffer[len] = '\0';
    }

    close(cnx);
    close(sock);

    return 0;
}