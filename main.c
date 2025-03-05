#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#define PRINT(info, ...) {printf(info, ##__VA_ARGS__); fflush(0);}
#define DEBUG if(1)
#define DDEBUG if(0)

#define CONSOLE "./console.html"
#define SWAP_FILE "./swap.temp"
#define LINE_LENGTH 300
#define CODE_PATH "."
#define WRITE(info, ...) fprintf(fp, info, ##__VA_ARGS__);
#define BREAKPOINT return quit_with_alert("breakpoint");
#define IF_ERR_RETURN(x, info, ...) do { if (x) {char _alert[LINE_LENGTH]; sprintf(_alert, info, ##__VA_ARGS__); return quit_with_alert(_alert);} } while (0);

struct sub_command_t {
    char *cmd;
    int (*execute)(char *value);
};

struct param_t {
    char *key;
    char *value;
};

char g_document[LINE_LENGTH]; // 用于表示当前正在操作的文档是什么
int g_param_count = 0; // 用于表示当前请求有几个参数
struct param_t g_param_list[10]; // 用于存储请求参数，最大10个

// 移除行尾的换行符
int remove_enter(char *line)
{
    int i = strlen(line) - 1;
    while (i >= 0 && (line[i] == '\n' || line[i] == '\r')) {
        line[i] = 0;
        i--;
    }
    return 0;
}

// 移除行尾的空格/tab
int remove_rear_space(char *line)
{
    int i = strlen(line) - 1;
    while (i >= 0 && (line[i] == ' ' || line[i] == '\t')) {
        line[i] = 0;
        i--;
    }
    return 0;
}

// 在字符串中进行指定字符的替换(只能一对一不能一对多多对一)
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
    }
    result[j] = 0;
    strcpy(line, result);
    return 0;
}

int recover_substr(char *line, char *end)
{
    char result[LINE_LENGTH * 5];
    int i, j;
    j = 0;
    for (i = 0; line[i] && (end ? (line + i < end) : 1); i++, j++) {
        if (line[i] == '%') {
            result[j] = char_to_int(line[i+1]) * 16 + char_to_int(line[i+2]);
            i+=2;
        } else {
            result[j] = line[i];
        }
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

int is_target_function(char *line, char *target)
{
    char temp[LINE_LENGTH];
    if (!is_function(line)) return 0;
    sprintf(temp, " %s(", target);
    if (strstr(line, temp)) return 1;
    sprintf(temp, " *%s(", target);
    if (strstr(line, temp)) return 1;
    return 0;
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

int show_welcome_page(char *value)
{
    return copy_file("./book/welcome.html", CONSOLE);
}

int document_welcome(char *value)
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
        sprintf(result, "<a href=\"http:/document-show?target=%s\" traget=\"_blank\">%s</a><br>\n", line, line);
        fprintf(console, "%s", result);
    }
    fclose(console);
    fclose(book);
    return 0;
}

int goto_sub_command(char *url, struct sub_command_t *list)
{
    int i = 0;
    char *value = NULL;
    char line[LINE_LENGTH];
    if (url && strchr(url, '?')) {
        value = strchr(url, '?');
        *value = 0;
        value++;
    }
    while (list[i].cmd) {
        if (url && strcmp(url, list[i].cmd) == 0) {
            return list[i].execute(value);
        }
        i++;
    }
    if (list[i].execute) {
        return list[i].execute(value);
    }
    sprintf_s(line, LINE_LENGTH, "Cannot find module : [%s]", url);
    return quit_with_alert(line);
}

// 展示所有的参数
int show_all_params(void)
{
    int i;
    for (i = 0; i < g_param_count; i++) {
        PRINT("PARAM[%d] : [%s] = [%s]\n", i, g_param_list[i].key, g_param_list[i].value);
    }
    return 0;
}

// 将参数放入g_param_list列表中，最大只能放10个参数，会清空原有的数据
int record_element(char *url)
{
    char *next = url;
    char *sep;
    DEBUG PRINT("recording input : [%s]\n", url);
    g_param_count = 0;
    memset(g_param_list, 0, sizeof(g_param_list));
    if (url == NULL) return 0;
    if (strlen(url) == 0) return 0;
    do {
        g_param_list[g_param_count].key = next;
        sep = strchr(next, '=');
        next = strchr(next, '&');
        if (sep && (next ? sep < next : 1)) {
            g_param_list[g_param_count].value = sep + 1;
            *sep = 0;
            recover_substr(g_param_list[g_param_count].value, next);
        }
        recover_substr(g_param_list[g_param_count].key, next);
        g_param_count++;
        if (next) {
            *next = 0;
            next++;
        }
    } while (next);
    return 0;
}

char *get_value(char *key)
{
    int i;
    for (i = 0; i < g_param_count; i++) {
        if (strcmp(key, g_param_list[i].key) == 0) {
            return g_param_list[i].value;
        }
    }
    return NULL;
}

int has_key(char *key)
{
    int i;
    for (i = 0; i < g_param_count; i++) {
        if (strcmp(key, g_param_list[i].key) == 0) {
            return 1;
        }
    }
    return 0;
}

int document_search(char *value)
{
    char line[LINE_LENGTH];
    char output[LINE_LENGTH];
    FILE *cmd;
    FILE *book = fopen("./book/database", "rb");
    FILE *console;
    char target[LINE_LENGTH];
    char *function;

    record_element(value);
    // 如果没有指定搜索目标
    if (get_value("target") == NULL) {
        return quit_with_alert("please choose target.");
    }
    strcpy(target, get_value("target"));
    str_replace(target, '/',  '-');
    str_replace(target, '\\',  '+');

    copy_file("./book/document.html", CONSOLE);
    console = fopen(CONSOLE, "ab");
    // 优先在库中搜索有无目标并显示(子串匹配即可)
    while (fgets(line, LINE_LENGTH, book)) {
        if (strstr(line, target)) {
            if (strchr(line, '+')) {
                sprintf(output, "<a href=\"http:/document-show?target=%s\" traget=\"_blank\">", line);
                str_replace(line, '-', '/');
                strcat(output, line);
                strcat(output, "</a><br>\n");
            } else {
                sprintf(output, "<a href=\"http:/document-show?target=%s\" traget=\"_blank\">%s</a><br>\n", line, line);
            }
            fprintf(console, "%s", output);
        }
    }
    fclose(book);
    // 接下来搜索其他可能的函数
    fprintf(console, "<p>其他可能的函数</p>\n");
    sprintf(line, "chcp 65001 && cd %s && findstr /s /c:\" %s(\" /c:\" *%s(\" *.c", CODE_PATH, target, target);
    DEBUG PRINT("cmd executing: [%s]\n", line);
    cmd = popen(line, "rb");
    fgets(line, LINE_LENGTH, cmd); // 吃掉chcp
    while (fgets(line, LINE_LENGTH, cmd)) {
        remove_enter(line);
        DEBUG PRINT("cmd executing: [%s]\n", line);
        function = strchr(line, ':');
        if (function == NULL) continue;
        function++; // 找到`文件:实际代码`的实际代码部分
        if (!is_function(function)) continue;
        DEBUG PRINT("function is : [%s]\n", function);
        // 展示函数
        *strchr(line, ':') = 0;
        strcpy(output, line); // 记录文件路径
        str_replace(line, '/', '-'); // 由于文档名中不能存在/，需要转为-
        str_replace(line, '\\', '-'); // 由于文档名中不能存在/，需要转为-
        // 检查，若该函数已被注册过，不再注册之
        strcat(line, "+");
        strcat(line, target);
        if (is_documented(line)) {
            continue;
        }
        fprintf(console, "<a href=\"http:/document-register?target=%s\" target=\"_blank\">%s+%s</a><br>\n", line, output, target);
    }
    fclose(console);
    fclose(cmd);
    return 0;
}

int editing_document(char *value)
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

int coding_document(char *value)
{
    FILE *book;
    FILE *console;
    FILE *src;
    FILE *swap;
    char line[LINE_LENGTH];
    int swap_already = 0;

    DEBUG PRINT("coding document with value[%s]\n", value);

    sprintf(line, "./book/%s.html", g_document);
    book = fopen(line, "rb");
    console = fopen(CONSOLE, "wb");
    // value有值代表这是一个写入操作
    if (value) {
        while (fgets(line, LINE_LENGTH, book)) {
            fprintf(console, "%s", line);
            if (strstr(line, "<code id=\"content\">")) {
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
        // 在编辑完文档之后，还需要对源代码进行修改
        DEBUG PRINT("is editing src file.\n");
        sprintf(line, "%s/%s", CODE_PATH, g_document);
        *strchr(line, '+') = 0;
        str_replace(line, '-', '/');
        src = fopen(line, "rb");
        swap = fopen(SWAP_FILE, "wb");
        while (fgets(line, LINE_LENGTH, src)) {
            if (swap_already == 0 && is_target_function(line, strchr(g_document, '+') + 1)) {
                remove_enter(value);
                fprintf(swap, "%s\n", value);
                while (fgets(line, LINE_LENGTH, src) && strncmp(line, "};", 2)) ;
                swap_already = 1;
            } else {
                fprintf(swap, "%s", line);
            }
        }
        fclose(src);
        fclose(swap);
        if (swap_already) {
            sprintf(line, "%s/%s", CODE_PATH, g_document);
            *strchr(line, '+') = 0;
            str_replace(line, '-', '/');
            copy_file(SWAP_FILE, line);
        } else {
            sprintf(line, "Cannot find target function[%s]", g_document);
            return quit_with_alert(line);
        }
    } else { // 否则，这是进入写入界面
        while (fgets(line, LINE_LENGTH, book)) {
            if (strstr(line, "<code id=\"content\">")) {
                fprintf(console, "%s", line);
                fprintf(console, "<textarea id=\"coding_area\" rows=\"10\" cols=\"200\">\n");
                while (fgets(line, LINE_LENGTH, book)) {
                    if (strstr(line, "</code>")) {
                        fprintf(console, "</textarea>\n");
                        fprintf(console, "%s\n", line);
                        break;
                    }
                    fprintf(console, "%s", line);
                }
            } else if (strstr(line, "javascript:begin_code()")) {
                DEBUG PRINT("is replacing edit button with finish.\n");
                fprintf(console, "<button type=\"button\" onclick=javascript:finish_code()>编辑</button>");
            } else {
                fprintf(console, "%s", line);
            }
        }
        fclose(book);
        fclose(console);
    }
    return 0;
}

// 只是很普通的展示文档 —— 但是在这里你可以对文档进行操作
int document_show(char *value)
{
    char line[LINE_LENGTH];
    char *target;

    // 1. 记录输入
    record_element(value);
    show_all_params();
    // 2. 判断目标文档
    target = get_value("target");
    if (target == NULL) {
        return quit_with_alert("please choose target.");
    }
    // 3. 把文档名称中的/和\全部转成-
    str_replace(target, '/', '-');
    str_replace(target, '\\', '-');
    if (!is_documented(target)) {
        sprintf(line, "Cannot find target document[%s].", target);
        return quit_with_alert(line);
    }
    strcpy(g_document, target);
    // 展示文件时，仅可能在以下附加选项中选一：
    // edit
    // edit= 
    // code 
    // code= 
    // para=
    // add_child=
    if (has_key("edit")) {
        editing_document(get_value("edit"));
    } else if (has_key("code")) {
        coding_document(get_value("code"));
    } else if (has_key("ret")) {
        // 
    } else if (has_key("add_child")) {
        // 
    } else {
        // 什么参数都没有，就只是普通的展示
        sprintf(line, "./book/%s.html", target);
        copy_file(line, CONSOLE);
    }
    return 0;
}

int document_register_function(void)
{
    FILE *src; // 源文件
    FILE *ref; // 函数文档模板
    FILE *fp;  // 目标
    char filepath[LINE_LENGTH]; // 文件路径
    char line[LINE_LENGTH];
    char definition[LINE_LENGTH] = {0}; // 函数定义
    char *index;
    char *para;
    int para_number = 0;
    char *src_file; // 文件名
    char *function; // 函数名

    // 调用到这里时已经检查过了，target值必定存在且包含+符号
    function = strchr(get_value("target"), '+');
    *function = 0;
    function++;
    src_file = get_value("target");
    str_replace(src_file, '-', '/');
    sprintf(filepath, "%s/%s", CODE_PATH, src_file);
    src = fopen(filepath, "rb");
    IF_ERR_RETURN(src == NULL, "register [%s+%s] failed. Cannot open src file [%s]", src_file, function, filepath);
    str_replace(src_file, '/', '-');
    str_replace(src_file, '\\', '-');
    sprintf(filepath, "./book/%s+%s.html", get_value("target"), function);
    DEBUG PRINT("is now registering [%s] : [%s]\n", get_value("target"), function);
    fp = fopen(filepath, "wb");
    IF_ERR_RETURN(fp == NULL, "register [%s+%s] failed. Cannot open document file [%s]", src_file, function, filepath);
    ref = fopen("./book/loading.html", "rb");

    // 寻找函数定义
    while (fgets(line, LINE_LENGTH, src)) {
        if (is_target_function(line, function)) {
            strcpy(definition, line);
            remove_enter(definition);
            // 有时函数的定义会分好几行
            while (!strchr(definition, '{') && fgets(line, LINE_LENGTH, src)) {
                strcat(definition, line);
                remove_enter(definition);
                remove_rear_space(definition);
            }
            if (strchr(definition, '{')) {
                *strchr(definition, '{') = 0;
            } else {
                return quit_with_alert("invalid function definition");
            }
            break;
        }
    }
    IF_ERR_RETURN(strlen(definition) == 0, "Cannot find [%s] in [%s]", function, src_file);

    // 根据模板开始创建函数文档
    while (fgets(line, LINE_LENGTH, ref)) {
        WRITE("%s", line);
        if (!strstr(line, "id=\"")) {
            continue;
        } else if (strstr(line, "id=\"title\"")) {
            WRITE("%s\n", function);
        } else if (strstr(line, "id=\"function\"")) {
            WRITE("%s\n", definition);
        } else if (strstr(line, "id=\"description\"")) {
            WRITE("请输入描述信息\n");
        } else if (strstr(line, "id=\"group\"")) {
            WRITE("%s\n", filepath);
        } else if (strstr(line, "id=\"ret_thead\"")) {
            strcpy(line, definition);
            index = strchr(line, '(');
            while (*index != ' ') index--;
            *index = 0;
            WRITE("%s\n", line);
        } else if (strstr(line, "id=\"function_thead\"")) {
            strcpy(line, definition); // 我是坏逼
            WRITE("%s\n", index + 1);
        } else if (strstr(line, "id=\"ret\"")) {
            strcpy(line, definition);
            *index = 0;
            WRITE("%s 返回值\n", line);
        } else if (strstr(line, "id=\"param_list\"")) {
            // 填入参数
            strcpy(line, definition);
            index = strchr(line, '(');
            DEBUG PRINT("is loading params : [%s]\n", index);
            while (strchr(index + 1, ',') || strchr(index + 1, ')')) {
                para = strchr(index + 1, ',') ? strchr(index + 1, ',') : strchr(index + 1, ')');
                *para = 0;
                WRITE("<tr>\n");
                WRITE("<td>\n");
                WRITE("%s\n", index + 1);
                WRITE("</td>\n");
                WRITE("<td id=\"para_%d\">\n", para_number++);
                WRITE("</td>");
                WRITE("<td>\n");
                WRITE("<button type=\"button\">更新</button>\n");
                WRITE("</td>\n");
                WRITE("</tr>\n");
                index = para;
            }
        } else if (strstr(line, "id=\"script\"")) {
            WRITE("var url_me = \"%s+%s\";\n", src_file, function);
        } else if (strstr(line, "id=\"content\"")) {
            WRITE("%s\n", definition);
            WRITE("{\n", definition);
            while (fgets(line, LINE_LENGTH, src) && *line != '}') {
                WRITE("%s", line);
            }
            WRITE("};\n");
        }
    }
    fclose(src);
    fclose(ref);
    fclose(fp);
    copy_file(filepath, CONSOLE);
    sprintf(line, "%s+%s\n", src_file, function);
    append_file("./book/database", line);
    //     if (!strstr(line, "<replace")) {
    //         WRITE("%s", line);
    //     } else {
    //         if (strstr(line, "<replace title")) {
    //             WRITE("%s\n", function);
    //         } else if (strstr(line, "<replace function/")) {
    //             WRITE("%s\n", definition);
    //         } else if (strstr(line, "<replace description")) {
    //             WRITE("请输入函数描述信息\n");
    //         } else if (strstr(line, "<replace para_list")) {
    //             strcpy(line, definition);
    //             index = strchr(line, '(');
    //             while (*index != ' ') index--;
    //             *index = 0;
    //             WRITE("<table border=\"2\" cellspacing=\"0\">\n");
    //             WRITE("\t<thead>\n");
    //             WRITE("\t\t<tr>\n");
    //             WRITE("\t\t\t<th align=\"center\">%s</th>\n", line);
    //             WRITE("\t\t\t<th>%s</th>\n", index + 1);
    //             WRITE("\t\t</tr>\n");
    //             WRITE("\t</thead>\n");
    //             WRITE("\t<tbody>\n");

    //             WRITE("\t\t<tr>\n");
    //             WRITE("\t\t\t<th>(返回值)%s</th>\n", line);
    //             WRITE("\t\t\t<th><input id=\"ret\" size=\"50\"/></th>\n");
    //             WRITE("\t\t</tr>\n");
    //             index = strchr(index + 1, '(');
    //             while (strchr(index + 1, ',') || strchr(index + 1, ')')) {
    //                 para = strchr(index + 1, ',') ? strchr(index + 1, ',') : strchr(index + 1, ')');
    //                 *para = 0;
    //                 WRITE("\t\t<tr>\n");
    //                 WRITE("\t\t\t<th>%s</th>\n", index + 1);
    //                 WRITE("\t\t\t<th><input id=\"para_%d\" size=\"50\"/></th>\n", para_number++);
    //                 WRITE("\t\t</tr>\n");
    //                 index = para;
    //             }
    //             WRITE("\t</tobdy>\n");
    //             WRITE("</table>\n");
    //         } else if (strstr(line, "<replace content")) {
    //             WRITE("%s\n", definition);
    //             while (fgets(line, LINE_LENGTH, src) && *line != '}') {
    //                 WRITE("%s", line);
    //             }
    //             WRITE("};\n");
    //         } else if (strstr(line, "<replace function_full")) {
    //             WRITE("%s", definition);
    //         } else if (strstr(line, "<replace address")) {
    //             WRITE("%s<br>\n", filepath);
    //         } else if (strstr(line, "<replace ME/>")) {
    //             sprintf(line, "var url_me = \"%s+%s\";\n", filepath, function);
    //             str_replace(line, '/', '-');
    //             WRITE("%s", line);
    //         }
    //     }
    // }
    // fclose(src);
    // fclose(book);
    // fclose(ref);
    // sprintf(line, "./book/%s.html", value);
    // copy_file(line, CONSOLE);
    // append_file("./book/database", value);
    // append_file("./book/database", "\n");
    return 0;

}

int document_register(char *value)
{
    char *target;
    record_element(value);
    if (!(target = get_value("target"))) {
        return quit_with_alert("please choose target");
    }
    if (strchr(target, '+')) { // 含有+时，认为它是一个函数注册
        return document_register_function();
    } else {
        return quit_with_alert("非函数的注册正在施工中");
    }
    // 其实除了注册一个更新函数，也正在施工中
}

int main(int argc, char **argv)
{
    char url[LINE_LENGTH * 100]; // 最多100行每行300
    if (argc != 2) {
        PRINT("Invalid input, this program should only be executed by server.\n");
        return 0;
    }
    strcpy(url, argv[1]);
    PRINT("Server Handler is executing : [%s]\n", url);
    struct sub_command_t sub_command_list[] = {
        {"/", show_welcome_page},
        {"/document", document_welcome},
        {"/document-search", document_search},
        {"/document-show", document_show},
        {"/document-register", document_register},
        {0},
    };
    return goto_sub_command(url, sub_command_list);
}