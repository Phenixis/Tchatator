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

Il y a 3 rôles dans la communication avec Tchatator
- Le <span class="role client">client</span>, pouvant envoyer des messages au professionnel
- Le <span class="role pro">professionnel</span>, pouvant communiquer avec ses potentiels client et les bloquer
- L'<span class="role admin">administrateur</span>, pouvant tout faire

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
Pour communiquer avec le service, utiliser n'importe quel outil de connexion en socket C. Voci une liste non exhaustive.
- **Telnet**
- **Netcat**
- **Socat**

## Liste des requêtes
Une fois connecté au service, vous pouvez profiter d'une multitude de requêtes dont vous obtiendez les détails avec la syntaxe ```/<commande> -h```. Certaines commandes sont spécifiques à chaque rôle

#### Identification
| Commande       | Pour          |
|----------------|---------------|
| /connexion     | Tous          |
| /deconnexion   | Tous |

#### Messagerie
| Commande                 | Pour          |
|--------------------------|---------------|
| /message {id_client} {message} | Tous        |
| /liste                   | <span class="role client">Client</span>, <span class="role pro">Pro</span>        |
| /conversation {id_client} {?page=0} | <span class="role pro">Pro</span>        |
| /info {id_message} {?page=0}  | <span class="role client">Client</span>, <span class="role pro">Pro</span> |

#### Bannissement
| Commande     | Pour          |
|--------------|---------------|
| /bloque {id_client} | <span class="role pro">Pro</span>, <span class="role admin">Admin</span> |
| /ban {id_client}    | <span class="role pro">Pro</span>, <span class="role admin">Admin</span> |
| /deban {id_client}  | <span class="role pro">Pro</span>, <span class="role admin">Admin</span> |

#### Paramétrage
| Commande | Pour          |
|----------|---------------|
| /sync    | <span class="role admin">Admin</span> |
| /logs    | <span class="role admin">Admin</span> |
