# TCHATATOR

## Présentation
**Tchatator** est un outil de messagerie travaillant avec une base de données **PostgreSQL**.
Il est ici utilisé dans le cadre du site TripEnArmor et travaille avec le modèle de sa base de données.

## Rôles
<style>
    .role {
        font-weight: bold;
    }
    .client {
        color: #83c744;
    }
    .pro {
        color: #3ba8a6;
    }
    .admin {
        color: #d6665e;
    }
</style>

Il y a 3 rôles dans la communication avec Tchatator :
- Le <span class="role client">client</span> *(connecté ou non)*, pouvant envoyer des messages au professionnel
- Le <span class="role pro">professionnel</span>, pouvant communiquer avec ses potentiels clients et les bloquer / bannir
- L'<span class="role admin">administrateur</span>, pouvant bloquer, banir, ou lever n'importe quelle de ces contraintes

## Utilisation (en local)
1. Télécharger le repo git ```git clone https://github.com/Phenixis/Tchatator```
2. Se déplacer dans le repo ```cd Tchatator```
3. Créer le fichier de configuration à partir de l'exemplaire : ```cp .tchatator.example .tchatator``` 
4. Renseigner les identifiants de votre base de données Postgres dans le fichier ```.tchatator```
4. Lancer le SERVICE ```./run tchatator.c```

## Liste des paramètres au lancement ```./run tchatator.c```
- **-v** *(ou --verbose)* pour afficher les message de log aussi sur le terminal
- **-f** *(ou --fileparam FILE)* pour utiliser le fichier de configuration FILE (par défaut ```.tchatator```)
- **-h** *(ou --help)* pour afficher de l'aide

## Se connecter avec le service Tchatator
Pour communiquer avec le service, vous pouvez exécuter notre fichier-logiciel avec la commande ```./run client.c <port>``` ou utiliser n'importe quel outil de connexion en socket C. En voici quelques exemples :
- **Telnet**
- **Netcat**
- **Socat**

## Liste des requêtes
Si vous n'utilisez pas directement ```client.c```, vous pouvez profiter d'une multitude de requêtes dont vous obtiendrez les détails en tapant ```/<requete> -h```. Certaines commandes sont spécifiques à certains rôles.

#### Identification
| Commande       | Commentaire                               | Qui ?          |
|----------------|-------------------------------------------|---------------|
| /connexion {API_KEY}     | Permet de se connecter à son compte       | Tous          |
| /deconnexion   | Permet de se déconnecter et de quitter le service | Tous          |

#### Messagerie
| Commande                          | Commentaire                               | Qui ?          |
|-----------------------------------|-------------------------------------------|---------------|
| /message {id_compte} {message}    | Envoie un message au compte renseigné | Tous          |
| /liste                             | Affiche la liste de vos messages non llus | <span class="role client">Client</span>, <span class="role pro">Pro</span> |
| /info {id_message}       | Affiche les informations du message spécifié | <span class="role client">Client</span>, <span class="role pro">Pro</span> |
| /modifie {id_message} {nouveau_message}       | Remplace le contenu du message spécifié | <span class="role client">Client</span>, <span class="role pro">Pro</span> |
| /supprime {id_message} | Supprime un de vos messages | <span class="role client">Client</span>, <span class="role pro">Pro</span> |
| /conversation {id_client} {?page=0} | Affiche l'historique des messages avec le client spécifié | <span class="role pro">Pro</span> |

#### Bannissement
| Commande     | Commentaire                                 | Qui ?          |
|--------------|---------------------------------------------|---------------|
| /bloque {id_client} | Bloque un client pendant 24h                       | <span class="role pro">Pro</span>, <span class="role admin">Admin</span> |
| /ban {id_client}    | Bannit un client définitivement                          | <span class="role pro">Pro</span>, <span class="role admin">Admin</span> |
| /deban {id_client}  | Lève le bannissement d'un client                         | <span class="role pro">Pro</span>, <span class="role admin">Admin</span> |

#### Paramétrage
| Commande | Commentaire                                | Qui ?          |
|----------|--------------------------------------------|---------------|
| /sync    | Recharge le fichier de synchronisation | <span class="role admin">Admin</span> |
| /logs    | Affiche les logs dans le fichier logs               | <span class="role admin">Admin</span> |
