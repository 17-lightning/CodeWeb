#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#define PRINT(info, ...) {printf(info, ##__VA_ARGS__); fflush(0);}
#define DEBUG if(1)
#define DDEBUG if(0)

#define CONSOLE "./console.html"
#define SWAP_FILE "./swap.temp"
#define LINE_LENGTH 500
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
char *g_relate_list[] = {
    "child",
    "parent",
    "struct",
    "global",
    "macro",
    "else",
    NULL,
};
struct param_t g_superlink[50]; // 用于存储超链接，最大50个，程序结束时需要释放
int g_superlink_number = 0;

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

// 移除行首的空格/tab
int remove_front_space(char *line)
{
    char *index = line;
    char temp[LINE_LENGTH];
    while (*index == ' ' || *index == '\t') index++;
    memmove(line, index, strlen(index) + 1);
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

// 替换字符串中指定的子字符串
int str_replace_str(char *line, char *target, char *replace)
{
    char temp[LINE_LENGTH * 2];
    char *ptr = strstr(line, target);
    if (ptr == NULL) return 1;
    *ptr = 0;
    sprintf(temp, "%s%s%s", line, replace, ptr + strlen(target));
    strcpy(line, temp);
    return 0;
}

int copy_file(char *src, char *dst)
{
    char line[LINE_LENGTH];
    sprintf(line, "copy \"%s\" \"%s\"\n", src, dst);
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

// 判断目标行中是否包含该元素，为避免识别到子字符串，规定其前后字符必须是 space * ( ) { } ; , \0
int is_element(char *line, char *target)
{
    char *front;
    char *rear;
    if (!strstr(line, target)) return 0;
    front = strstr(line, target);
    rear = front + strlen(target);
    if (line != front) {
        front--;
        if (*front != '(' && *front != ' ' && *front != '*' && *front != ' ' && *front != '\t') {
            return 0;
        }
    }
    if (*rear != '\0' && *rear != '\n' && *rear != '\r' && *rear != '(' && *rear != ')' && *rear != ';' && *rear != ',' && *rear != ' ' && *rear != '\t' && *rear != '}') return 0;
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
    if (file == NULL) return 0;
    if (strlen(file) <= 1) return 0;
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

// ↑ 通用
// ↓ 专用

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

int set_append(char *append)
{
    int i;
    char *list[] = {
        "add_child",
        "add_parent",
        "add_struct",
        "add_global",
        "add_macro",
        "add_else",
        NULL,
    };

    for (i = 0; list[i]; i++) {
        if (get_value(list[i])) {
            sprintf(append, "&%s=%s", list[i], get_value(list[i]));
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
    char append[LINE_LENGTH];
    char *function;

    record_element(value);
    // 如果没有指定搜索目标
    if (get_value("target") == NULL) {
        return quit_with_alert("please choose target.");
    }
    strcpy(target, get_value("target"));
    str_replace(target, '/',  '-');
    str_replace(target, '\\',  '+');
    set_append(append);

    copy_file("./book/document.html", CONSOLE);
    console = fopen(CONSOLE, "ab");
    // 优先在库中搜索有无目标并显示(子串匹配即可)
    while (fgets(line, LINE_LENGTH, book)) {
        if (strstr(line, target)) {
            if (strchr(line, '+')) {
                sprintf(output, "<a href=\"http:/document-show?target=%s%s\" traget=\"_blank\">", line, append);
                str_replace(line, '-', '/');
                strcat(output, line);
                strcat(output, "</a><br>\n");
            } else {
                sprintf(output, "<a href=\"http:/document-show?target=%s%s\" traget=\"_blank\">%s</a><br>\n", line, append, line);
            }
            fprintf(console, "%s", output);
        }
    }
    fclose(book);
    // 接下来搜索其他可能的函数
    fprintf(console, "<p>其他可能的函数</p>\n");
    fprintf(console, "<a href=\"http:/document-register?target=%s\" target=\"_blank\">%s(非函数注册)</a><br>\n", target, target);
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
        fprintf(console, "<a href=\"http:/document-register?target=%s%s\" target=\"_blank\">%s+%s</a><br>\n", line, append, output, target);
    }
    fclose(console);
    fclose(cmd);
    return 0;
}

int str_to_html(char *line)
{
    char output[LINE_LENGTH * 2] = {0};
    int i;
    for (i = 0; line[i]; i++) {
        if (line[i] == '&') strcat(output, "&amp;");
        else if (line[i] == '<') strcat(output, "&lt;");
        else if (line[i] == '>') strcat(output, "&gt;");
        else if (line[i] == '\t') strcat(output, "&emsp;");
        else if (line[i] == '\"') strcat(output, "&quot;");
        else if (line[i] == '\'') strcat(output, "&#39;");
        else output[strlen(output)] = line[i];
    }
    strcpy(line, output);
    return 0;
}

// 获取页面的描述信息
int get_abstract(char *target, char *abstract)
{
    char line[LINE_LENGTH];
    FILE *fp;
    if (target == NULL || abstract == NULL) {
        DEBUG PRINT("[get_abstract] invalid input.\n");
        return -21;
    }
    if (!is_documented(target)) {
        DEBUG PRINT("target[%s] is not instance.\n", target);
        return -15;
    }
    sprintf(line, "./book/%s.html", target);
    fp = fopen(line, "rb");
    if (fp == NULL) {
        DEBUG PRINT("open target document[%s] failed.", line);
        return -2;
    }
    while (fgets(line, LINE_LENGTH, fp)) {
        if (strstr(line, "<code id=\"description\">")) { // 取descrpition的第一行
            fgets(line, LINE_LENGTH, fp);
            if (!strstr(line, "</code>")) {
                strcpy(abstract, line);
                remove_enter(abstract);
            }
            fclose(fp);
            return 0;
        }
    }
    fclose(fp);
    DEBUG PRINT("theres no description in document[%s]\n", target);
    return -1;
}

int record_superlink(char *file)
{
    char *filepath = file ? file : CONSOLE;
    FILE *fp = fopen(filepath, "rb");
    char line[LINE_LENGTH];
    char description[LINE_LENGTH * 2];
    int i;
    int in_area = 0;

    for (i = 0; i < g_superlink_number; i++) {
        free(g_superlink[i].key);
        free(g_superlink[i].value);
    }
    g_superlink_number = 0;

    if (fp == NULL) {
        DEBUG PRINT("Cannot open file [%s]\n", file);
        return -2;
    }

    while (fgets(line, LINE_LENGTH, fp)) {
        if (strstr(line, "<tbody id=\"relate_table\">")) in_area = 1;
        else if (strstr(line, "</tbody>")) in_area = 0;
        else {
            remove_enter(line);
            remove_front_space(line);
            if (get_abstract(line, description) == 0) {
                g_superlink[g_superlink_number].key = malloc(LINE_LENGTH);
                g_superlink[g_superlink_number].value = malloc(LINE_LENGTH * 2);
                strcpy(g_superlink[g_superlink_number].key, line);
                sprintf(g_superlink[g_superlink_number].value, "<a href=\"http://localhost:8000/document-show?target=%s\" title=\"%s\" target=\"_blank\">%s</a>", line, description, strchr(line, '+') + 1);
                DEBUG PRINT("load superlink[%d] : [%s] >> [%s]\n", g_superlink_number, line, g_superlink[g_superlink_number].value);
                g_superlink_number++;
            }
        }
    }
    return 0;
}

// 在页面中寻找相关元素，并超链接化
int apply_superlink(void)
{
    FILE *console;
    FILE *fp;
    char line[LINE_LENGTH * 2];
    char description[LINE_LENGTH];
    char *output;
    int i;

    console = fopen(CONSOLE, "rb");
    fp = fopen(SWAP_FILE, "wb");
    while (fgets(line, LINE_LENGTH, console)) {
        WRITE("%s", line);
        if (strstr(line, "<tbody id=\"relate_table\">")) {
            while (fgets(line, LINE_LENGTH, console)) {
                if (strstr(line, "</table>")) {
                    WRITE("%s", line);
                    break;
                } else if (strchr(line, '<')) {
                    WRITE("%s", line);
                } else if (strlen(line) > 1) {
                    remove_enter(line);
                    remove_front_space(line);
                    if (get_abstract(line, description) == 0) {
                        g_superlink[g_superlink_number].key = malloc(LINE_LENGTH);
                        g_superlink[g_superlink_number].value = malloc(LINE_LENGTH * 2);
                        strcpy(g_superlink[g_superlink_number].key, line);
                        sprintf(g_superlink[g_superlink_number].value, "<a href=\"http://localhost:8000/document-show?target=%s\" title=\"%s\" target=\"_blank\">%s</a>", line, description, strchr(line, '+') + 1);
                        WRITE("%s<br>\n", g_superlink[g_superlink_number].value);
                        DEBUG PRINT("load superlink [%s] : [%s]\n", line, g_superlink[g_superlink_number].value);
                        g_superlink_number++;
                    } else {
                        WRITE("%s\n", line);
                    }
                } else {
                    WRITE("%s\n", line);
                }
            }
        }
    }
    fclose(fp);
    fclose(console);
    copy_file(SWAP_FILE, CONSOLE);
    // 以上是录入relate区间的superlink，以下是把superlink应用到description和coding中
    console = fopen(CONSOLE, "rb");
    fp = fopen(SWAP_FILE, "wb");
    while (fgets(line, LINE_LENGTH, console)) {
        WRITE("%s", line);
        if (strstr(line, "<code id=\"description\">") || strstr(line, "<code id=\"content\">")) {
            while (fgets(line, LINE_LENGTH, console)) {
                if (strstr(line, "<textarea id=\"")) {
                    WRITE("%s", line);
                    break;
                } else if (strstr(line, "</code>")) {
                    WRITE("%s", line);
                    break;
                } else {
                    DDEBUG PRINT("is parsing content[%s]", line);
                    for (i = 0; i < g_superlink_number; i++) {
                        if (is_element(line, strchr(g_superlink[i].key, '+') + 1)) {
                            DDEBUG PRINT("apply superlink [%s] -> [%s]\n", g_superlink[i].key, g_superlink[i].value);
                            str_replace_str(line, strchr(g_superlink[i].key, '+') + 1, g_superlink[i].value);
                        }
                    }
                    WRITE("%s", line);
                }
            }
        }
    }
    fclose(fp);
    fclose(console);
    copy_file(SWAP_FILE, CONSOLE);
    return 0;
}

int turn_to_html_mode(void)
{
    FILE *console = fopen(CONSOLE, "rb");
    FILE *fp = fopen(SWAP_FILE, "wb");
    char line[LINE_LENGTH];
    while (fgets(line, LINE_LENGTH, console)) {
        WRITE("%s", line);
        if (strstr(line, "<code id=\"description\">") || strstr(line, "<code id=\"content\">")) {
            while (fgets(line, LINE_LENGTH, console) && !strstr(line, "</code>")) {
                if (strstr(line, "<textarea id=\"")) {
                    break;
                }
                str_to_html(line);
                WRITE("%s", line);
            }
            WRITE("%s", line);
        }
    }
    fclose(fp);
    fclose(console);
    copy_file(SWAP_FILE, CONSOLE);
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
                while (fgets(line, LINE_LENGTH, src) && line[0] != '}') ;
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

int paraing_document(void)
{
    int i;
    char line[LINE_LENGTH];
    char temp[LINE_LENGTH];
    char *value;
    FILE *book;
    FILE *fp;
    for (i = 0; i < 10; i++) {
        sprintf(temp, "para_%d", i);
        DDEBUG PRINT("para_%d is [%s]\n", i, get_value(temp));
        if (value = get_value(temp)) {
            DEBUG PRINT("updating param_%d 's description: %s", i, value);
            sprintf(temp, "<td id=\"para_%d\">", i);
            sprintf(line, "./book/%s.html", g_document);
            book = fopen(line, "rb");
            fp = fopen(SWAP_FILE, "wb");
            while (fgets(line, LINE_LENGTH, book)) {
                fprintf(fp, "%s", line);
                if (strstr(line, temp)) {
                    remove_enter(value);
                    fprintf(fp, "%s\n", value);
                    while (fgets(line, LINE_LENGTH, book) && !strstr(line, "</td>")) ;
                    fprintf(fp, "%s", line);
                }
            }
            fclose(book);
            fclose(fp);
            sprintf(line, "./book/%s.html", g_document);
            copy_file(SWAP_FILE, line);
        }
    }
    return 0;
}

int relating_document(void)
{
    char line[LINE_LENGTH];
    char temp[LINE_LENGTH];
    char *value;
    int i;
    FILE *book;
    FILE *swap;

    for (i = 0; g_relate_list[i]; i++) {
        sprintf(temp, "add_%s", g_relate_list[i]);
        if ((value = get_value(temp)) == NULL) continue;
        if (!is_documented(value)) {
            PRINT("target document [%s] not exist (database)\n");
            return -21;
        }
        sprintf(temp, "./book/%s.html", value);
        book = fopen(temp, "rb");
        if (book == NULL) {
            PRINT("target document [%s] not exist (fopen)");
            return -2;
        }
        swap = fopen(SWAP_FILE, "wb");
        sprintf(temp, "<td id=\"%s\">", g_relate_list[i]);
        DEBUG PRINT("is now relating [%s]'[%s] = [%s]\n", value, g_relate_list[i], g_document);
        while (fgets(line, LINE_LENGTH, book)) {
            fprintf(swap, "%s", line);
            if (strstr(line, temp)) {
                while (fgets(line, LINE_LENGTH, book)) {
                    if (is_element(line, g_document)) {
                        fprintf(swap, "%s", line);
                        break;
                    } else if (strstr(line, "</td>")) {
                        fprintf(swap, "%s\n", g_document);
                        fprintf(swap, "%s", line);
                        break;
                    }
                    fprintf(swap, "%s", line);
                }
            }
        }
        fclose(swap);
        fclose(book);
        sprintf(line, "./book/%s.html", value);
        copy_file(SWAP_FILE, line);

        // 如果连接的是父子函数，将会双向连接
        if (strchr(g_document, '+') && (i == 0 || i == 1)) {
            sprintf(line, "./book/%s.html", g_document);
            book = fopen(line, "rb");
            swap = fopen(SWAP_FILE, "wb");
            IF_ERR_RETURN(book == NULL, "open current file[%s] failed", g_document); 
            sprintf(temp, "<td id=\"%s\">", g_relate_list[!i]);
            DEBUG PRINT("is now re-relating [%s]'[%s] = [%s]\n", g_document, g_relate_list[i], value);
            while (fgets(line, LINE_LENGTH, book)) {
                fprintf(swap, "%s", line);
                if (strstr(line, temp)) {
                    while (fgets(line, LINE_LENGTH, book)) {
                        if (is_element(line, value)) {
                            fprintf(swap, "%s", line);
                            break;
                        } else if (strstr(line, "</td>")) {
                            fprintf(swap, "%s\n", value);
                            fprintf(swap, "%s", line);
                            break;
                        }
                        fprintf(swap, "%s", line);
                    }
                }
            }
            fclose(book);
            fclose(swap);
            sprintf(line, "./book/%s.html", g_document);
            copy_file(SWAP_FILE, line);
        }
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
    } else {
        // 把参数修改放这里了，因为……好像没什么好的方法has_key去判断para_X
        paraing_document();
        // 增加联系也放这里了
        relating_document();
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
    int para_number = 1;
    char *src_file; // 文件名
    char *function; // 函数名

    // 如果函数已经被注册过了，再次注册 = 更新，会保留用户写入的注释等信息，但是这部分代码暂时没实现
    if (is_documented(get_value("target"))) {
        sprintf(line, "./book/%s.html", get_value("target"));
        copy_file(line, CONSOLE);
        strcpy(g_document, get_value("target"));
        return 0;
    }

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
    sprintf(g_document, "%s+%s", src_file, function);
    DEBUG PRINT("is now registering [%s] : [%s]\n", get_value("target"), function);
    fp = fopen(filepath, "wb");
    IF_ERR_RETURN(fp == NULL, "register [%s+%s] failed. Cannot open document file [%s]", src_file, function, filepath);
    ref = fopen("./book/function_template.html", "rb");

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
                WRITE("<td id=\"para_%d\">\n", para_number);
                WRITE("</td>\n");
                // WRITE("<button type=\"button\">更新</button>\n");
                WRITE("<td><button type=\"button\" onclick=javascript:edit_para(%d)>更新</button></td>\n", para_number);
                WRITE("</tr>\n");
                index = para;
                para_number++;
            }
        } else if (strstr(line, "id=\"script\"")) {
            WRITE("var url_me = \"%s+%s\";\n", src_file, function);
        } else if (strstr(line, "id=\"content\"")) {
            WRITE("%s\n", definition);
            WRITE("{\n", definition);
            while (fgets(line, LINE_LENGTH, src) && *line != '}') {
                WRITE("%s", line);
            }
            WRITE("}\n");
        }
    }
    fclose(src);
    fclose(ref);
    fclose(fp);
    copy_file(filepath, CONSOLE);
    sprintf(line, "%s\n", g_document);
    if (!is_documented(g_document)) append_file("./book/database", line);
    return 0;
}

int document_register_other(void)
{
    char line[LINE_LENGTH];
    FILE *ref;
    FILE *fp;
    // 如果目标已经被注册过了，再次注册 = 更新，会保留用户写入的注释等信息，但是这部分代码暂时没实现
    sprintf(line, "./book/%s.html", get_value("target"));
    if (is_documented(get_value("target"))) {
        copy_file(line, CONSOLE);
        strcpy(g_document, get_value("target"));
        return 0;
    }
    ref = fopen("./book/blank_template.html", "rb");
    fp = fopen(line, "wb");
    while (fgets(line, LINE_LENGTH, ref)) {
        WRITE("%s", line);
        if (!strstr(line, "id=\"")) {
            continue;
        } else if (strstr(line, "id=\"title\"")) {
            WRITE("%s\n", get_value("target"));
        } else if (strstr(line, "id=\"description\"")) {
            WRITE("请输入描述信息\n");
        } else if (strstr(line, "id=\"script\"")) {
            WRITE("var url_me = \"%s\";\n", get_value("target"));
        }
    }
    fclose(ref);
    fclose(fp);
    sprintf(line, "./book/%s.html", get_value("target"));
    copy_file(line, CONSOLE);
    // 增加联系也放这里了
    relating_document();
    sprintf(line, "%s\n", get_value("target"));
    if (!is_documented(g_document)) append_file("./book/database", line);
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
        return document_register_other();
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
    goto_sub_command(url, sub_command_list);
    turn_to_html_mode();
    apply_superlink();
    return 0;
}
