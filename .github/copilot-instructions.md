Hey Copilot !
Tu vas m'aider à écrire un service de chat entre un professionnel et un client via une API par ligne de commande. Ce chat sera asynchrone.
Ci-dessous, tu trouveras le protocole de communication à respecter pour que le service fonctionne correctement. 

Le chat sera écrit en langage C et doit mettre en oeuvre une programmation de sockets.

Un fichier de paramétrage '.tchatator' contiendra les informations suivantes :
- Port d'écoute
- Nombre maxi de messages
- Taille maxi d'un message
- Le nombre de requête maxi par minute
- Le nombre de requête maxi par heure
- La clé d'API d'administration
- Le nombre de connexion en file d'attente
- Les codes de réponses

# Pactocole

## Préambule
À toute commande, le SERVICE peut renvoyer les codes suivants :
- `429/TOO MANY REQUESTS` : Limite de requêtes atteinte.
- `401/UNAUTHORIZED` : Connexion non identifiée ou clé d'API invalide.
- `404/NOT FOUND` : La commande n'est pas reconnue.
- `500/SERVER ERROR` : Le serveur a rencontré une erreur imprévue.
- `501/NOT IMPLEMENTED` : La commande est reconnue mais n'est pas encore fonctionnelle.

À toute commande, le CLIENT peut ajouter l'option `-n` ou `--help` pour avoir plus d'informations sur comment utiliser la commande.

## Identification
### Connexion
- **Commande** : `/connexion {api_key}`
- **Réponses possibles** :
  - `200/OK` : Identification acceptée, token fourni.
  - `401/UNAUTHORIZED` : Clé d'API invalide.

### Déconnexion
- **Commande** : `/deconnexion`
- **Réponses possibles** :
  - `200/OK` : Déconnexion acceptée, token réinitialisé.

## Envoi d'un message
- **Commande** : `/message {id_client} {message}`
- **Réponses possibles** :
  - `200/OK` : Message envoyé.
  - `413/PAYLOAD TOO LARGE` : Message trop long.
  - `404/NOT FOUND` : Destinataire inexistant.

## Réception des messages non lus
- **Commande** : `/liste {page=0}`
- **Réponses possibles** :
  - `200/OK` : Conversations reçues.
  - `204/NO CONTENT` : Liste vide.
  - `416/RANGE NOT SATISFIABLE` : Bloc inexistant.

## Réception d'un historique de messages
- **Commande** : `/conversation {id_client} {?page=0}`
- **Réponses possibles** :
  - `200/OK` : Messages reçus.
  - `204/NO CONTENT` : Aucun message.
  - `404/NOT FOUND` : `id_client` inexistant.
  - `416/RANGE NOT SATISFIABLE` : Bloc inexistant.

## Réception des informations d'un message
- **Commande** : `/info {id_message}`
- **Réponses possibles** :
  - `200/OK` : Informations reçues.
  - `404/NOT FOUND` : Message inexistant.
  - `403/FORBIDDEN` : Message inaccessible.

## Modification ou suppression d'un message
### Modification
- **Commande** : `/modifie {id_message} {nouveau_message}`
- **Réponses possibles** :
  - `200/OK` : Message modifié.
  - `404/NOT FOUND` : Message inexistant.
  - `403/FORBIDDEN` : Message non émis par le CLIENT.

### Suppression
- **Commande** : `/supprime {id_message}`
- **Réponses possibles** :
  - `200/OK` : Message supprimé.
  - `404/NOT FOUND` : Message inexistant.
  - `403/FORBIDDEN` : Message non émis par le CLIENT.

## Blocage d'un CLIENT
- **Commande** : `/bloque {id_client}`
- **Réponses possibles** :
  - `200/OK` : CLIENT bloqué.
  - `409/CONFLICT` : CLIENT déjà bloqué.
  - `404/NOT FOUND` : `id_client` inexistant.

## Bannissement d'un CLIENT
### Bannissement
- **Commande** : `/ban {id_client}`
- **Réponses possibles** :
  - `200/OK` : CLIENT banni.
  - `409/CONFLICT` : CLIENT déjà banni.
  - `404/NOT FOUND` : `id_client` inexistant.

### Levage d'un bannissement
- **Commande** : `/deban {id_client}`
- **Réponses possibles** :
  - `200/OK` : Bannissement levé.
  - `404/NOT FOUND` : `id_client` inexistant.

## Paramétrage
- **Commande** : `/sync`
- **Réponses possibles** :
  - `200/OK` : Paramétrage relu.
  - `404/NOT FOUND` : Fichier de paramétrage introuvable.

## Logs
- **Commande** : `/logs {?nb_logs=50}`
- **Réponses possibles** :
  - `200/OK` : Logs reçus.
  - `204/NO CONTENT` : Aucun log disponible.