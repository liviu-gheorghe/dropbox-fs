#define FUSE_USE_VERSION 31

// Constanta ce defineste numarul maxim de fisiere din filesystem

#define MAX_FILES_COUNT 1000

#define MAX_POPEN_RESP_SIZE 5000

const char rootPath[2] = "/";
const char defaultFileContent[] = "Text content";

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

/*
 * Structura care modeleaza un fisier care va fi montant in filesystem. Informatiile de care avem nevoie pt un fisier sunt, in primul rand, denumirea acestuia, dar si alte
   lucruri precum path-ul fisierului in filesystem si continutul acestuia.
 */

struct File
{
        char *filename;
        char *path;
        char *contents;
};

// Variabila in care vom retine lista de fisiere care vor fi montate in filesystem

struct File fileList[MAX_FILES_COUNT];

// Numarul de fisiere din filesystem
int fileCount = 0;

char **getFileNamesFromResponse(char *response)
{
        char **resultedArray = (char **)malloc(sizeof(char *) * (MAX_FILES_COUNT + 1));
        int dim = 0;
        char delim[3] = " \n";
        char *token = strtok(response, delim);
        while (token != NULL)
        {

                if (strchr(token, '.'))
                {

                        resultedArray[dim] = (char *)malloc(sizeof(char) * (strlen(token) + 1));
                        strcpy(resultedArray[dim], token + 1);
                        dim++;
                }

                token = strtok(NULL, delim);
        }
        resultedArray[dim] = NULL;
        // for (int i = 0; i < dim; i++)
        //         printf("%s\n", resultedArray[i]);

        return resultedArray;
}

static void *filesystem_init(struct fuse_conn_info *conn, struct fuse_config *cfg)
{
        (void)conn;
        cfg->kernel_cache = 1;
        return NULL;
}

static int filesystem_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{

        // Structura ce retine informatiile despre fisier
        (void)fi;

        bool fileFound = false;
        int foundfileIndex = -1;

        for (int fileIndex = 0; fileIndex < fileCount; ++fileIndex)
        {
                if (strcmp(path + 1, fileList[fileIndex].filename) == 0)
                {
                        fileFound = true;
                        foundfileIndex = fileIndex;
                        break;
                }
        }

        // Daca nu am gasit fisierul returnam eroare

        if (!fileFound && foundfileIndex >= 0)
        {
                return -ENOENT;
        }

        // Alocam memorie pentru structura ce va retine statisticile fisierului curent

        memset(stbuf, 0, sizeof(struct stat));

        if (strcmp(path, "/") == 0)
        {
                stbuf->st_mode = S_IFDIR | 0755;
                stbuf->st_nlink = 2;
        }
        else if (strcmp(path + 1, fileList[foundfileIndex].filename) == 0)
        {
                stbuf->st_mode = S_IFREG | 0444;
                stbuf->st_nlink = 1;
                stbuf->st_size = strlen(fileList[foundfileIndex].contents);
        }
        else
        {
                return -ENOENT;
        }

        return 0;
}

static int filesystem_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{

        (void)offset;
        (void)fi;
        (void)flags;

        // Daca incercam sa accesam altceva decat root-ul fs-ului atunci aruncam o eroare (ENOENT)
        //
        if (strcmp(path, "/") != 0)
        {
                return -ENOENT;
        }

        // Adaugam cele doua fisiere speciale care pointeaza catre folderul curent si folderul parinte

        filler(buf, ".", NULL, 0, 0);
        filler(buf, "..", NULL, 0, 0);

        // Populam filler-ul pt fiecare fisier din fs in parte

        for (int fileIndex = 0; fileIndex < fileCount; ++fileIndex)
        {
                filler(buf, fileList[fileIndex].filename, NULL, 0, 0);
        }

        return 0;
}

static int filesystem_open(const char *path, struct fuse_file_info *fi)
{

        bool fileFound = false;

        for (int fileIndex = 0; fileIndex < fileCount; ++fileIndex)
        {
                if (strcmp(path + 1, fileList[fileIndex].filename) == 0)
                {
                        fileFound = true;
                        break;
                }
        }

        // Daca nu am gasit fisierul returnam eroare

        if (!fileFound)
        {
                return -ENOENT;
        }

        // Daca nu avemdreptul de a citi returnam o eroare
        if ((fi->flags & O_ACCMODE) != O_RDONLY)
                return -EACCES;

        return 0;
}

static int filesystem_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
        size_t len;
        (void)fi;

        bool fileFound = false;
        int foundfileIndex = -1;

        for (int fileIndex = 0; fileIndex < fileCount; ++fileIndex)
        {
                if (strcmp(path + 1, fileList[fileIndex].filename) == 0)
                {
                        fileFound = true;
                        foundfileIndex = fileIndex;
                        break;
                }
        }

        // Daca nu am gasit fisierul returnam eroare

        if (!fileFound && foundfileIndex >= 0)
        {
                return -ENOENT;
        }

        len = strlen(fileList[foundfileIndex].contents);
        if (offset < len)
        {
                if (offset + size > len)
                        size = len - offset;
                memcpy(buf, fileList[foundfileIndex].contents + offset, size);
        }
        else
                size = 0;

        return size;
}

// Structura ce defineste operatiile care se pot realiza pe filesystem si functiile asociate cu acestea

static const struct fuse_operations filesystem_operations = {
    .init = filesystem_init,
    .getattr = filesystem_getattr,
    .readdir = filesystem_readdir,
    .open = filesystem_open,
    .read = filesystem_read,
};

int main(int argc, char *argv[])
{
        printf("Pulling Dropbox file data....\n");

        FILE *dbxCLiResponseFile = popen("/usr/local/bin/dbxcli ls", "r");
        char *dbxCLiResponse = NULL;

        if (dbxCLiResponseFile == NULL)
        {
                perror("Something bad happened when reading the dbxcli response");
        }

        int responseSize = MAX_POPEN_RESP_SIZE;

        dbxCLiResponse = malloc(responseSize);

        fread(dbxCLiResponse, 1, responseSize, dbxCLiResponseFile);

        int actualBufferSize = strlen(dbxCLiResponse);

        pclose(dbxCLiResponseFile);

        char **resultedFiles = getFileNamesFromResponse(dbxCLiResponse);

        printf("The files are the following\n");

        for (int i = 0; resultedFiles[i] != NULL; i++)
        {
                struct File currentFile;
                currentFile.path = (char *)malloc(sizeof(char) * (strlen(rootPath) + 1));
                strcpy(currentFile.path, rootPath);
                currentFile.contents = (char *)malloc(sizeof(char) * (strlen(defaultFileContent) + 1));
                strcpy(currentFile.contents, defaultFileContent);
                currentFile.filename = (char *)malloc(sizeof(char) * (strlen(resultedFiles[i]) + 1));
                strcpy(currentFile.filename, resultedFiles[i]);

                printf("FileList[%d] = {%s, %s, %s}\n", fileCount, currentFile.filename, currentFile.path, currentFile.contents);
                fileList[fileCount++] = currentFile;
        }

        int exitStatusCode;
        struct fuse_args fuseArgs = FUSE_ARGS_INIT(argc, argv);

        if (fuse_opt_parse(&fuseArgs, &fileList, NULL, NULL) == -1)
        {

                printf("Filesystem mount error");
                exitStatusCode = 1;
                return 1;
        }

        exitStatusCode = fuse_main(fuseArgs.argc, fuseArgs.argv, &filesystem_operations, NULL);

        fuse_opt_free_args(&fuseArgs);

        return 0;
}
