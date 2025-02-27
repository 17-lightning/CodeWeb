#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#define PRINT(info, ...) {printf(info, ##__VA_ARGS__); fflush(0);}
#define DEBUG if(1)
#define DDEBUG if(0)

#define CONSOLE "./console.html"
#define LINE_LENGTH 300
#define CODE_PATH "D:/PP/PJ03"
#define WRITE(info, ...) fprintf(book, info, ##__VA_ARGS__);

struct sub_command_t {
    char *cmd;
    int (*execute)(char *value, char *next);
};

char g_document[LINE_LENGTH]; // 用于表示当前正在操作的文档是什么

int remove_enter(char *line)
{
    int i = strlen(line) - 1;
    while (i >= 0 && (line[i] == '\n' || line[i] == '\r')) {
        line[i] = 0;
        i--;
    }
    return 0;
}

int str_replace(char *line, char a, char b)
{
    for (int i = 0; line[i] != 0; i++) {
        if (line[i] == a) line[i] = b;
    }
    return 0;
}

int copy_file(char *file1, char *file2)
{
    char line[LINE_LENGTH];
    sprintf(line, "copy \"%s\" \"%s\"\n", file1, file2);
    // Windows的CP只吃\不吃/
    str_replace(line, '/', '\\');
    PRINT("executing cmd : [%s]\n", line);
    system(line);
    return 0;
}

int append_file(char *file, char *line)
{
    FILE *fp = fopen(file, "ab");
    fprintf(fp, "%s", line);
    fclose(fp);
    return 0;
}

int add_alert(char *alert)
{
    char line[LINE_LENGTH];
    sprintf(line, "<script>alert(\"%s\");</script>", alert);
    return append_file(CONSOLE, line);
}

int char_to_int(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    else if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    else return -1;
}

int recover_str(char *line)
{
    char result[LINE_LENGTH * 5];
    int i, j;
    j = 0;
    for (i = 0; line[i]; i++, j++) {
        if (line[i] == '%') {
            result[j] = char_to_int(line[i+1]) * 16 + char_to_int(line[i+2]);
            i+=2;
        } else {
            result[j] = line[i];
        }
        PRINT("%x ", result[j]);
    }
    result[j] = 0;
    strcpy(line, result);
    return 0;
}

// 判断函数三板斧： 1. 有没有= 2. 有没有; 3. 有没有顶格写
int is_function(char *line)
{
    if (strchr(line, '=')) return 0;
    if (strchr(line, ';')) return 0;
    if (line[0] == ' ' || line[0] == '\t') return 0;
    return 1;
}

// 查询目标文档是否已被注册
int is_documented(char *file)
{
    FILE *fp = fopen("./book/database", "rb");
    char line[LINE_LENGTH];
    while (fgets(line, LINE_LENGTH, fp)) {
        remove_enter(line);
        DDEBUG PRINT("comparing %s and %s (%d)\n", file, line, strcmp(file, line));
        if (strcmp(line, file) == 0) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

// 回到初始页面并显示错误信息
int quit_with_alert(char *info)
{
    copy_file("./book/welcome.html", CONSOLE);
    add_alert(info);
    return 0;
}

int show_welcome_page(char *value, char *next)
{
    return copy_file("./book/welcome.html", CONSOLE);
}

int document_welcome(char *value, char *next)
{
    FILE *book;
    FILE *console;
    char line[LINE_LENGTH];
    char result[LINE_LENGTH];

    DEBUG PRINT("will show document welcome page.\n");

    copy_file("./book/document.html", CONSOLE);
    book = fopen("./book/database", "rb");
    console = fopen(CONSOLE, "ab");

    while (fgets(line, LINE_LENGTH, book)) {
        remove_enter(line);
        sprintf(result, "<a href=\"http:/document^show=%s\" traget=\"_blank\">%s</a><br>\n", line, line);
        fprintf(console, "%s", result);
    }
    fclose(console);
    fclose(book);
    return 0;
}

int goto_sub_command(char *url, struct sub_command_t *list)
{
    int i = 0;
    char *next = NULL;
    char *value = NULL;
    char line[LINE_LENGTH];
    if (url && strchr(url, '^')) {
        next = strchr(url, '^');
        *next = 0;
        next++;
    }
    if (url && strchr(url, '=')) {
        value = strchr(url, '=');
        *value = 0;
        value++;
    }
    // 匹配=前的关键字
    while (list[i].cmd) {
        if (url && strcmp(url, list[i].cmd) == 0) {
            return list[i].execute(value, next);
        }
        i++;
    }
    if (list[i].execute) {
        return list[i].execute(value, next);
    }
    sprintf_s(line, LINE_LENGTH, "Cannot find module : [%s]", url);
    return quit_with_alert(line);
}

int document_search(char *value, char *next)
{
    char line[LINE_LENGTH];
    char output[LINE_LENGTH];
    FILE *cmd;
    FILE *book = fopen("./book/database", "rb");
    FILE *console;

    copy_file("./book/document.html", CONSOLE);
    console = fopen(CONSOLE, "ab");
    // 优先在库中搜索有无目标并显示
    while (fgets(line, LINE_LENGTH, book)) {
        if (strstr(line, value)) {
            sprintf(output, "<a href=\"http:/document^show=%s\" traget=\"_blank\">%s</a><br>\n", line, line);
            fprintf(console, "%s", output);
        }
    }
    fclose(book);
    fprintf(console, "<p>其他可能的函数</p>\n");
    sprintf(line, "chcp 65001 && cd %s && findstr /s /n /c:\"\\ %s\" *.c", CODE_PATH, value);
    DEBUG PRINT("cmd executing: [%s]\n", line);
    cmd = popen(line, "rb");
    fgets(line, LINE_LENGTH, cmd); // 吃掉chcp
    while (fgets(line, LINE_LENGTH, cmd)) {
        remove_enter(line);
        DEBUG PRINT("cmd executing: [%s]\n", line);
        if (!is_function(line)) continue;
        // ↑ 这里函数判断规则3：首字符不能是\t或空格，是无效的，因为这里line是"文件路径:行号:实际代码"，所以要再做判断
        if (*(strchr(strchr(line, ':') + 1, ':') + 1) == '\t' || *(strchr(strchr(line, ':') + 1, ':') + 1) == ' ') {
            continue;
        }
        strcpy(output, line);
        *strchr(output, ':') = 0;
        str_replace(output, '/', '-'); // 由于文件名中不能存在/，需要转为-
        // 检查，若该函数已被注册过，不再注册之
        strcat(output, "+");
        strcat(output, value);
        if (is_documented(output)) {
            continue;
        }
        *strchr(output, '+') = 0;
        fprintf(console, "<a href=\"http:/document^register=%s+%s\" target=\"_blank\">%s</a><br>\n", output, value, line);
    }
    fclose(console);
    fclose(cmd);
    return 0;
}

int editing_document(char *value, char *next)
{
    FILE *book;
    FILE *console;
    char line[LINE_LENGTH];

    DEBUG PRINT("editing document with value[%s]\n", value);

    sprintf(line, "./book/%s.html", g_document);
    book = fopen(line, "rb");
    console = fopen(CONSOLE, "wb");
    // value有值代表这是一个写入操作
    if (value) {
        while (fgets(line, LINE_LENGTH, book)) {
            fprintf(console, "%s", line);
            if (strstr(line, "<code id=\"description\">")) {
                remove_enter(value); // 手动换行，你别掺和
                fprintf(console, "%s\n", value);
                while (fgets(line, LINE_LENGTH, book)) {
                    if (strstr(line, "</code>")) {
                        fprintf(console, "%s", line);
                        break;
                    }
                }
            }
        }
        fclose(book);
        fclose(console);
        sprintf(line, "./book/%s.html", g_document);
        copy_file(CONSOLE, line);
    } else { // 否则，这是进入写入界面
        while (fgets(line, LINE_LENGTH, book)) {
            if (strstr(line, "<code id=\"description\">")) {
                fprintf(console, "%s", line);
                fprintf(console, "<textarea id=\"description_area\" rows=\"10\" cols=\"200\">\n");
                while (fgets(line, LINE_LENGTH, book)) {
                    if (strstr(line, "</code>")) {
                        fprintf(console, "</textarea>\n");
                        fprintf(console, "%s\n", line);
                        break;
                    }
                    fprintf(console, "%s", line);
                }
            } else if (strstr(line, "javascript:begin_edit()")) {
                DEBUG PRINT("is replacing edit button with finish.\n");
                fprintf(console, "<button type=\"button\" onclick=javascript:finish_edit()>编辑</button>");
            } else {
                fprintf(console, "%s", line);
            }
        }
        fclose(book);
        fclose(console);
    }
    return 0;
}

int document_normal_show(char *value, char *next)
{
    char line[LINE_LENGTH];
    sprintf(line, "./book/%s.html", value);
    copy_file(line, CONSOLE);
    return 0;
}

int document_show(char *value, char *next)
{
    char line[LINE_LENGTH];
    if (is_documented(value)) {
        strcpy(g_document, value);
        sprintf(line, "./book/%s.html", value);
        copy_file(line, CONSOLE);
        // 根据next中是否包含editing/coding标签，对console进行处理，并反哺原始文档
        struct sub_command_t sub_command_list[] = {
            {"editing", editing_document},
            {0, document_normal_show},
        };
        return goto_sub_command(next, sub_command_list);
    } else {
        sprintf(line, "Cannot find target document [%s]", value);
        quit_with_alert(line);
    }
    return 0;
}

int document_register_function(char *value, char *next)
{
    FILE *src;
    FILE *ref;
    FILE *book;
    char filepath[LINE_LENGTH];
    char function[LINE_LENGTH];
    char line[LINE_LENGTH];
    char definition[LINE_LENGTH];
    char storage[LINE_LENGTH];
    char *index;
    char *para;
    int para_number;
    index = strchr(value, '+');
    strcpy(filepath, value);
    *strchr(filepath, '+') = 0;
    strcpy(function, index + 1);
    str_replace(filepath, '-', '/');
    DEBUG PRINT("is now registering [%s] : [%s]\n", filepath, function);
    sprintf(line, "%s/%s", CODE_PATH, filepath);
    src = fopen(line, "rb");
    if (src == NULL) {
        sprintf(line, "register [%s] failed. Cannot open file [%s/%s]", function, CODE_PATH, filepath);
        return quit_with_alert(line);
    }

    // 打开工作台
    ref = fopen("./book/loading.html", "rb");
    sprintf(line, "./book/%s.html", value);
    book = fopen(line, "wb");
    // 找到函数定义
    sprintf(definition, " %s", function); // 前面加个空格避免子串匹配
    while (fgets(line, LINE_LENGTH, src)) {
        if (strstr(line, definition) && is_function(line)) {
            strcpy(definition, line);
            remove_enter(definition);
            if (!strchr(line, ')')) { // 防护函数定义过长中间有换行的情况
                fgets(line, LINE_LENGTH, src);
                if (definition[strlen(definition) - 1] != ' ') strcat(definition, " ");
                strcat(definition, line);
                remove_enter(definition);
            }
            break;
        }
    }

    if (strlen(definition) == 0) {
        sprintf(line, "Cannot find target function [%s] in [%s]", function, filepath);
        return quit_with_alert(line);
    }

    while (fgets(line, LINE_LENGTH, ref)) {
        DDEBUG PRINT("dealing : %s\n", line);
        if (!strstr(line, "<replace")) {
            WRITE("%s", line);
        } else {
            if (strstr(line, "<replace title")) {
                WRITE("%s\n", function);
            } else if (strstr(line, "<replace function/")) {
                WRITE("%s\n", definition);
            } else if (strstr(line, "<replace description")) {
                WRITE("请输入函数描述信息\n");
            } else if (strstr(line, "<replace para_list")) {
                strcpy(line, definition);
                index = strchr(line, '(');
                while (*index != ' ') index--;
                *index = 0;
                WRITE("<table border=\"2\" cellspacing=\"0\">\n");
                WRITE("\t<thead>\n");
                WRITE("\t\t<tr>\n");
                WRITE("\t\t\t<th align=\"center\">%s</th>\n", line);
                WRITE("\t\t\t<th>%s</th>\n", index + 1);
                WRITE("\t\t</tr>\n");
                WRITE("\t</thead>\n");
                WRITE("\t<tbody>\n");

                WRITE("\t\t<tr>\n");
                WRITE("\t\t\t<th>(返回值)%s</th>\n", line);
                WRITE("\t\t\t<th><input id=\"ret\" size=\"50\"/></th>\n");
                WRITE("\t\t</tr>\n");
                index = strchr(index + 1, '(');
                while (strchr(index + 1, ',') || strchr(index + 1, ')')) {
                    para = strchr(index + 1, ',') ? strchr(index + 1, ',') : strchr(index + 1, ')');
                    *para = 0;
                    WRITE("\t\t<tr>\n");
                    WRITE("\t\t\t<th>%s</th>\n", index + 1);
                    WRITE("\t\t\t<th><input id=\"para_%d\" size=\"50\"/></th>\n", para_number++);
                    WRITE("\t\t</tr>\n");
                    index = para;
                }
                WRITE("\t</tobdy>\n");
                WRITE("</table>\n");
            } else if (strstr(line, "<replace content")) {
                WRITE("%s\n", definition);
                while (fgets(line, LINE_LENGTH, src) && *line != '}') {
                    WRITE("%s", line);
                }
                WRITE("};\n");
            } else if (strstr(line, "<replace function_full")) {
                WRITE("%s", definition);
            } else if (strstr(line, "<replace address")) {
                WRITE("%s<br>\n", filepath);
            } else if (strstr(line, "<replace ME/>")) {
                sprintf(line, "var url_me = \"%s+%s\";\n", filepath, function);
                str_replace(line, '/', '-');
                WRITE("%s", line);
            }
        }
    }
    fclose(src);
    fclose(book);
    fclose(ref);
    sprintf(line, "./book/%s.html", value);
    copy_file(line, CONSOLE);
    append_file("./book/database", value);
    append_file("./book/database", "\n");
    return 0;

}

int document_register(char *value, char *next)
{
    if (strchr(value, '+')) {
        return document_register_function(value, next);
    } else {
        return 0; // 非函数的注册正在施工中
    }
}

int document_execute(char *value, char *next)
{
    struct sub_command_t sub_command_list[] = {
        {"search", document_search},
        {"show", document_show},
        {"register", document_register}, // 目前只支持注册函数哦
        {0, document_welcome},
    };
    return goto_sub_command(next, sub_command_list);
}

int main(int argc, char **argv)
{
    char url[LINE_LENGTH * 100]; // 最多100行每行300
    if (argc != 2) {
        PRINT("Invalid input, this program should only be executed by server.\n");
        return 0;
    }
    strcpy(url, argv[1]);
    recover_str(url);
    PRINT("Server Handler is executed : [%s]\n", url);
    struct sub_command_t sub_command_list[] = {
        {"/", show_welcome_page},
        {"/document", document_execute},
        {0},
    };
    return goto_sub_command(url, sub_command_list);
}