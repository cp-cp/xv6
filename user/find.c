#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

char *fmtname(char *path)
{
    char *p;
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;
    return p;
}
void equal_print(char *path, char *findname)
{
    if (strcmp(fmtname(path), findname) == 0)
        printf("%s\n", path);
}
void find(char *dir_name, char *file_name)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(dir_name, O_RDONLY)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", dir_name);
        return;
    }

    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", dir_name);
        close(fd);
        return;
    }
    if(st.type==T_FILE)
    {
        equal_print(dir_name, file_name);
    }
    else if (st.type==T_DIR)
    {
        if (strlen(dir_name) + 1 + DIRSIZ + 1 > sizeof buf)
        {
            printf("find: path too long\n");
        }
        //将path复制到buf里
        strcpy(buf, dir_name);
        //p为一个指针，指向buf(path)的末尾
        p = buf + strlen(buf);
        //在末尾添加/ 比如 path为 a/b/c 经过这步后变为 a/b/c/<-p
        *p++ = '/';
        // 如果是文件夹，则循环读这个文件夹里面的文件
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if (de.inum == 0 || (strcmp(de.name, ".") == 0) || (strcmp(de.name, "..") == 0))
                continue;
            //拼接出形如 a/b/c/de.name 的新路径(buf)
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            find(buf, file_name);
        }
    }    
    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        find(".", argv[1]);
        exit(0);
    }
    if (argc > 3)
    {
        fprintf(2, "Usage: find <path> <file>\n");
        exit(-1);
    }
    find(argv[1], argv[2]);
    exit(0);
}