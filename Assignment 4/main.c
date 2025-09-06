#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_USERS 5
#define MAX_RESOURCES 5
#define MAX_NAME_LEN 20

typedef enum{ 
    READ = 1,
    WRITE = 2,
    EXECUTE = 4
}Permission;

//User and Resource Definitions
typedef struct{
    char name[MAX_NAME_LEN];
}User;

typedef struct{
    char name[MAX_NAME_LEN];
}Resource;

//ACL Entry
typedef struct{
    char username[MAX_NAME_LEN];
    int permissions;
}ACLEntry;

typedef struct{
    Resource resource;
    ACLEntry entries[MAX_USERS];
    int entryCount;
}ACLControlledResource;

//Capability Entry
typedef struct{
    char resourceName[MAX_NAME_LEN];
    int permissions;
}Capability;

typedef struct{
    User user;
    Capability capabilities[MAX_RESOURCES];
    int capabilityCount;
}CapabilityUser;

//Utility Functions
void printPermissions(int perm){
    if (perm & READ) printf("READ ");
    if (perm & WRITE) printf("WRITE ");
    if (perm & EXECUTE) printf("EXECUTE ");
}

int hasPermission(int userPerm, int requiredPerm){
    return (userPerm & requiredPerm) == requiredPerm;
}

//ACL System
void checkACLAccess(ACLControlledResource *res, const char *userName, int perm){
    for (int i = 0; i < res->entryCount; i++) {
        if (strcmp(res->entries[i].username, userName) == 0) {
            printf("ACL Check: User %s requests ", userName);
            printPermissions(perm);
            printf("on %s: ", res->resource.name);
            if (hasPermission(res->entries[i].permissions, perm)) {
                printf("Access GRANTED\n");
                return;
            } else {
                printf("Access DENIED\n");
                return;
            }
        }
    }
    printf("ACL Check: User %s has NO entry for resource %s: Access DENIED\n", userName, res->resource.name);
}

//Capability System
void checkCapabilityAccess(CapabilityUser *user, const char *resourceName, int perm){
    for (int i = 0; i < user->capabilityCount; i++) {
        if (strcmp(user->capabilities[i].resourceName, resourceName) == 0) {
            printf("Capability Check: User %s requests ", user->user.name);
            printPermissions(perm);
            printf("on %s: ", resourceName);
            if (hasPermission(user->capabilities[i].permissions, perm)) {
                printf("Access GRANTED\n");
                return;
            } else {
                printf("Access DENIED\n");
                return;
            }
        }
    }
    printf("Capability Check: User %s has NO capability for %s: Access DENIED\n", user->user.name, resourceName);
}

//Enhancement Functions
int addACLEntry(ACLControlledResource *aclResource, const char *username, int permissions) {
    if (aclResource->entryCount >= MAX_USERS) {
        return 0; // No space available
    }
    strcpy(aclResource->entries[aclResource->entryCount].username, username);
    aclResource->entries[aclResource->entryCount].permissions = permissions;
    aclResource->entryCount++;
    return 1; // Success
}

int addCapability(CapabilityUser *user, const char *resourceName, int permissions) {
    if (user->capabilityCount >= MAX_RESOURCES) {
        return 0; // No space available
    }
    strcpy(user->capabilities[user->capabilityCount].resourceName, resourceName);
    user->capabilities[user->capabilityCount].permissions = permissions;
    user->capabilityCount++;
    return 1; // Success
}

int main(){
    //Sample users and resources
    User users[MAX_USERS] = {{"Alice"}, {"Bob"}, {"Charlie"}, {"Arnab"}, {"ZTP"}};
    Resource resources[MAX_RESOURCES] = {{"File1"}, {"File2"}, {"File3"}, {"Secret"}, {"Secret2"}};

    //ACL Setup
    ACLControlledResource aclResources[MAX_RESOURCES];
    
    aclResources[0].resource = resources[0];
    aclResources[0].entryCount = 0;  // Initialize count
    addACLEntry(&aclResources[0], "Alice", READ | WRITE);
    addACLEntry(&aclResources[0], "Bob", READ);
    
    aclResources[1].resource = resources[1];
    strcpy(aclResources[1].entries[0].username, "Alice");
    aclResources[1].entries[0].permissions = READ | EXECUTE;
    strcpy(aclResources[1].entries[1].username, "Charlie");
    aclResources[1].entries[1].permissions = WRITE;
    aclResources[1].entryCount = 2;
    
    aclResources[2].resource = resources[2];
    strcpy(aclResources[2].entries[0].username, "Bob");
    aclResources[2].entries[0].permissions = READ | WRITE | EXECUTE;
    strcpy(aclResources[2].entries[1].username, "Arnab");
    aclResources[2].entries[1].permissions = READ;
    aclResources[2].entryCount = 2;
    
    aclResources[3].resource = resources[3];
    strcpy(aclResources[3].entries[0].username, "Arnab");
    aclResources[3].entries[0].permissions = READ | WRITE;
    strcpy(aclResources[3].entries[1].username, "ZTP");
    aclResources[3].entries[1].permissions = READ;
    aclResources[3].entryCount = 2;
    
    aclResources[4].resource = resources[4];
    strcpy(aclResources[4].entries[0].username, "ZTP");
    aclResources[4].entries[0].permissions = READ | WRITE | EXECUTE;
    strcpy(aclResources[4].entries[1].username, "Alice");
    aclResources[4].entries[1].permissions = EXECUTE;
    aclResources[4].entryCount = 2;

    //Capability Setup
    CapabilityUser capabilityUsers[MAX_USERS];
    
    capabilityUsers[0].user = users[0];
    capabilityUsers[0].capabilityCount = 0;  // Initialize count
    addCapability(&capabilityUsers[0], "File1", READ | WRITE);
    addCapability(&capabilityUsers[0], "File2", READ | EXECUTE);
    addCapability(&capabilityUsers[0], "Secret2", EXECUTE);
    
    capabilityUsers[1].user = users[1];
    strcpy(capabilityUsers[1].capabilities[0].resourceName, "File1");
    capabilityUsers[1].capabilities[0].permissions = READ;
    strcpy(capabilityUsers[1].capabilities[1].resourceName, "File3");
    capabilityUsers[1].capabilities[1].permissions = READ | WRITE | EXECUTE;
    capabilityUsers[1].capabilityCount = 2;
    
    capabilityUsers[2].user = users[2];
    strcpy(capabilityUsers[2].capabilities[0].resourceName, "File3");
    capabilityUsers[2].capabilities[0].permissions = WRITE;
    capabilityUsers[2].capabilityCount = 1;
    
    capabilityUsers[3].user = users[3];
    strcpy(capabilityUsers[3].capabilities[0].resourceName, "File3");
    capabilityUsers[3].capabilities[0].permissions = READ;
    strcpy(capabilityUsers[3].capabilities[1].resourceName, "Secret");
    capabilityUsers[3].capabilities[1].permissions = READ | WRITE;
    capabilityUsers[3].capabilityCount = 2;
    
    capabilityUsers[4].user = users[4];
    strcpy(capabilityUsers[4].capabilities[0].resourceName, "Secret");
    capabilityUsers[4].capabilities[0].permissions = READ;
    strcpy(capabilityUsers[4].capabilities[1].resourceName, "Secret2");
    capabilityUsers[4].capabilities[1].permissions = READ | WRITE | EXECUTE;
    capabilityUsers[4].capabilityCount = 2;

    //Test ACL
    checkACLAccess(&aclResources[0], "Alice", READ);
    checkACLAccess(&aclResources[0], "Bob", WRITE);
    checkACLAccess(&aclResources[0], "Charlie", READ);
    checkACLAccess(&aclResources[1], "Alice", EXECUTE);
    checkACLAccess(&aclResources[2], "Bob", READ | WRITE);
    checkACLAccess(&aclResources[3], "Arnab", WRITE);
    checkACLAccess(&aclResources[3], "ZTP", WRITE);
    checkACLAccess(&aclResources[4], "ZTP", READ | EXECUTE);
    checkACLAccess(&aclResources[4], "Alice", READ);

    //Test Capability
    checkCapabilityAccess(&capabilityUsers[0], "File1", WRITE);
    checkCapabilityAccess(&capabilityUsers[1], "File1", WRITE);
    checkCapabilityAccess(&capabilityUsers[2], "File2", READ);
    checkCapabilityAccess(&capabilityUsers[0], "File2", EXECUTE);
    checkCapabilityAccess(&capabilityUsers[3], "Secret", READ);
    checkCapabilityAccess(&capabilityUsers[4], "Secret2", WRITE);
    checkCapabilityAccess(&capabilityUsers[1], "Secret", READ);
    checkCapabilityAccess(&capabilityUsers[2], "Secret2", READ);
    checkCapabilityAccess(&capabilityUsers[4], "File1", READ);

    return 0;
}